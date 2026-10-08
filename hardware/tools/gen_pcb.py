#!/usr/bin/env python3
"""Gera hardware/kicad/hushrig_carrier.kicad_pcb: posiciona as peças, roteia (A* em 2 camadas) e preenche o GND.

Usa o pcbnew do KiCad 7 (python3 -c "import pcbnew") e os footprints padrão em /usr/share/kicad/footprints.
"""
import heapq
import math
import pathlib
import sys

import numpy as np
import pcbnew
from pcbnew import FromMM, ToMM, VECTOR2I
from shapely.geometry import LineString, Point, Polygon, box
from shapely import contains_xy
from shapely.affinity import rotate as sh_rotate

import netlist

FP_DIR = pathlib.Path("/usr/share/kicad/footprints")
OUT = pathlib.Path(__file__).resolve().parent.parent / "kicad" / "hushrig_carrier.kicad_pcb"

BOARD_W, BOARD_H = 100.0, 76.0
CLEARANCE = 0.2       # mm, cobre a cobre
EDGE_CLEAR = 0.5      # mm, cobre à borda
VIA_D, VIA_DRILL = 0.8, 0.4
GRID = 0.25
SNAP_MARGIN = 0.1     # folga extra no roteador: as trilhas terminam no centro exato do pad, fora da grade
TRACK_W = {"default": 0.25, "+5V": 0.5, "+3V3": 0.5, "5VA": 0.4, "GND": 0.3}

# (x, y, rotação em graus) do pino 1 / origem do footprint
# Tudo em coordenadas absolutas (mm). Sem "c": (x, y) é o pino 1; com "c": centro dos pads (peças giradas).
# O ESP32 fica à direita; os módulos (J3/J4) e os resistores série do I2S ficam colados na coluna J1 do DevKit,
# para as trilhas de clock serem curtas. O resto (analógico, reforço de alimentação, controles) fica à esquerda.
PLACE = {
    "J1": (60.6, 8.0, 0), "J2": (86.0, 8.0, 0),
    # módulos
    "J3": (44.0, 10.0, 0), "J4": (44.0, 36.0, 0),
    # resistores série do I2S (pino 1 = lado do ESP32)
    "R8": (53.4, 32.5, 180, "c"), "R9": (53.4, 27.0, 180, "c"), "R7": (89.5, 41.0, 0), "R10": (89.5, 46.0, 0),
    # entrada e buffer
    "J5": (10.0, 6.0, 0), "C1": (16.0, 6.0, 0), "R4": (24.0, 6.0, 0), "C2": (36.0, 6.0, 0), "R1": (8.0, 12.0, 0),
    "U1": (22.0, 18.0, 180, "c"), "R5": (30.0, 17.0, 0), "C3": (31.0, 24.0, 0),
    # referência e 5V analógico
    "R2": (8.0, 23.0, 90, "c"), "R3": (13.0, 23.0, 90, "c"), "C4": (6.0, 31.0, 0), "C7": (12.0, 31.0, 0),
    "C6": (20.0, 27.0, 0), "R6": (6.0, 38.0, 0), "C5": (18.0, 38.0, 0),
    # barramentos
    "C8": (6.0, 46.0, 0), "C11": (14.0, 46.0, 0), "C9": (20.0, 46.0, 0), "C10": (30.0, 46.0, 0), "C12": (30.0, 38.0, 0),
    # controles, bateria e saída
    "C13": (6.0, 55.0, 0), "R11": (15.0, 55.0, 0), "R12": (26.0, 55.0, 0), "C14": (36.0, 58.0, 0), "R13": (24.0, 63.0, 0),
    # conectores para fora (borda inferior)
    "J11": (11.27, 71.0, 90, "c"), "J10": (19.27, 71.0, 90, "c"), "J9": (27.27, 71.0, 90, "c"), "J8": (35.27, 71.0, 90, "c"),
    "J7": (44.54, 71.0, 90, "c"), "J6": (54.27, 71.0, 90, "c"),
}
HOLES = [(4.0, 4.0), (96.0, 4.0), (4.0, 72.0), (96.0, 72.0)]

SILK_PINS = {  # rótulos de função ao lado de cada pino dos cabeçalhos
    "J3": ["5V", "3V3", "GND", "SCK", "BCK", "LRCK", "OUT", "L-IN", "GND"],
    "J4": ["VIN", "3V3", "GND", "BCK", "LRCK", "DIN", "L-OUT", "GND"],
    "J5": ["TIP", "GND"], "J6": ["TIP", "GND"], "J7": ["IN", "WPR", "GND"], "J8": ["SW", "GND"],
    "J9": ["LED+", "LED-"], "J10": ["5V", "GND"], "J11": ["BAT+", "GND"],
}
SILK_TITLE = {"J3": "ADC PCM1808", "J4": "DAC PCM5102A", "J5": "IN", "J6": "OUT", "J7": "VOL", "J8": "SW", "J9": "LED", "J10": "5V", "J11": "BAT"}


def mm(v):
    return FromMM(v)


def load_fp(fpid):
    lib, name = fpid.split(":")
    fp = pcbnew.FootprintLoad(str(FP_DIR / f"{lib}.pretty"), name)
    assert fp is not None, fpid
    return fp


# --------------------------------------------------------------------------- geometria dos pads
def pad_polygon(pad, grow=0.0):
    """Polígono shapely (mm) do pad de furo passante (retângulo/oval/círculo), com margem `grow`."""
    c = pad.GetPosition()
    cx, cy = ToMM(c.x), ToMM(c.y)
    sz = pad.GetSize()
    w, h = ToMM(sz.x), ToMM(sz.y)
    shape = pad.GetShape()
    ang = pad.GetOrientationDegrees()
    if shape == pcbnew.PAD_SHAPE_RECT:
        g = box(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)
    elif shape == pcbnew.PAD_SHAPE_OVAL and abs(w - h) > 1e-6:
        r = min(w, h) / 2
        if w > h:
            g = LineString([(cx - (w / 2 - r), cy), (cx + (w / 2 - r), cy)]).buffer(r, 16)
        else:
            g = LineString([(cx, cy - (h / 2 - r)), (cx, cy + (h / 2 - r))]).buffer(r, 16)
    else:
        g = Point(cx, cy).buffer(max(w, h) / 2, 16)
    if ang:
        g = sh_rotate(g, -ang, origin=(cx, cy))  # y para baixo
    return g.buffer(grow) if grow else g


# --------------------------------------------------------------------------- roteador
class Router:
    def __init__(self, board, nets_by_name):
        self.board = board
        self.nx = int(BOARD_W / GRID) + 1
        self.ny = int(BOARD_H / GRID) + 1
        self.classes = sorted(set(TRACK_W.values()) | {VIA_D})  # VIA_D: margem para posicionar vias
        # owner[classe][camada] -> int16: -1 livre, -2 bloqueado, >=0 id da rede dona
        self.owner = {w: np.full((2, self.ny, self.nx), -1, dtype=np.int16) for w in self.classes}
        self.xs = np.arange(self.nx) * GRID
        self.ys = np.arange(self.ny) * GRID
        self.netid = {n: i for i, n in enumerate(sorted(nets_by_name))}
        self.tracks = []   # (net, layer, [(x,y)...], width)
        self.vias = []     # (net, x, y)

    # -- marcação de obstáculos --
    def _mark_poly(self, poly, layer, net, extra_by_class=None):
        for w in self.classes:
            g = poly.buffer(CLEARANCE + SNAP_MARGIN + w / 2, 8)
            minx, miny, maxx, maxy = g.bounds
            i0, i1 = max(int(minx / GRID) - 1, 0), min(int(maxx / GRID) + 2, self.nx)
            j0, j1 = max(int(miny / GRID) - 1, 0), min(int(maxy / GRID) + 2, self.ny)
            if i0 >= i1 or j0 >= j1:
                continue
            gx, gy = np.meshgrid(self.xs[i0:i1], self.ys[j0:j1])
            inside = contains_xy(g, gx, gy)
            arr = self.owner[w][layer, j0:j1, i0:i1]
            nid = -2 if net is None else self.netid[net]
            free = arr == -1
            same = arr == nid
            arr[inside & free] = nid
            arr[inside & ~free & ~same] = -2

    def mark_pad(self, pad, net):
        poly = pad_polygon(pad)
        for layer in (0, 1):
            self._mark_poly(poly, layer, net)

    def mark_via_keepout(self, x, y, drill_r):
        """Vias não podem ficar sobre/perto de furos de pads (folga entre furos 0,25 mm)."""
        poly = Point(x, y).buffer(drill_r + VIA_DRILL / 2 + 0.3, 16)
        for layer in (0, 1):
            cls = self.owner[VIA_D][layer]
            minx, miny, maxx, maxy = poly.bounds
            i0, i1 = max(int(minx / GRID) - 1, 0), min(int(maxx / GRID) + 2, self.nx)
            j0, j1 = max(int(miny / GRID) - 1, 0), min(int(maxy / GRID) + 2, self.ny)
            gx, gy = np.meshgrid(self.xs[i0:i1], self.ys[j0:j1])
            cls[j0:j1, i0:i1][contains_xy(poly, gx, gy)] = -2

    def mark_hole(self, x, y, r):
        for layer in (0, 1):
            self._mark_poly(Point(x, y).buffer(r, 16), layer, None)

    def mark_edge(self):
        for w in self.classes:
            m = EDGE_CLEAR + w / 2
            for layer in (0, 1):
                a = self.owner[w][layer]
                a[self.ys < m, :] = -2
                a[self.ys > BOARD_H - m, :] = -2
                a[:, self.xs < m] = -2
                a[:, self.xs > BOARD_W - m] = -2

    def mark_track(self, layer, pts, width, net):
        for a, b in zip(pts, pts[1:]):
            self._mark_poly(LineString([a, b]).buffer(width / 2, 8), layer, net)

    def mark_via(self, x, y, net):
        for layer in (0, 1):
            self._mark_poly(Point(x, y).buffer(VIA_D / 2, 16), layer, net)

    # -- A* --
    def cell(self, x, y):
        return int(round(x / GRID)), int(round(y / GRID))

    def route(self, net, sources, targets, width):
        """sources/targets: conjuntos {(layer,i,j)}. Devolve lista de (layer,i,j) ou None."""
        own = self.owner[width]
        viaown = self.owner[VIA_D]
        nid = self.netid[net]
        tset = set(targets)
        tx = np.array([t[1] for t in tset]); ty = np.array([t[2] for t in tset])

        def heur(i, j):
            return GRID * float(np.min(np.abs(tx - i) + np.abs(ty - j)))

        pq = []
        g = {}
        parent = {}
        for s in sources:
            g[s] = 0.0
            heapq.heappush(pq, (heur(s[1], s[2]), 0.0, s))
        via_cost = 6.0
        while pq:
            _, gc, cur = heapq.heappop(pq)
            if gc > g.get(cur, 1e18):
                continue
            if cur in tset:
                path = [cur]
                while cur in parent:
                    cur = parent[cur]
                    path.append(cur)
                return path[::-1]
            l, i, j = cur
            for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ni, nj = i + di, j + dj
                if not (0 <= ni < self.nx and 0 <= nj < self.ny):
                    continue
                o = own[l, nj, ni]
                if o != -1 and o != nid:
                    continue
                horiz = dj == 0
                step = GRID * (1.0 if (horiz == (l == 0)) else 1.35)  # topo prefere horizontal, fundo vertical
                nxt = (l, ni, nj)
                ng = gc + step
                if ng < g.get(nxt, 1e18):
                    g[nxt] = ng
                    parent[nxt] = cur
                    heapq.heappush(pq, (ng + heur(ni, nj), ng, nxt))
            # via
            nl = 1 - l
            o0, o1 = viaown[l, j, i], viaown[nl, j, i]
            if (o0 in (-1, nid)) and (o1 in (-1, nid)):
                nxt = (nl, i, j)
                ng = gc + via_cost
                if ng < g.get(nxt, 1e18):
                    g[nxt] = ng
                    parent[nxt] = cur
                    heapq.heappush(pq, (ng + heur(i, j), ng, nxt))
        return None


def simplify(path):
    """(layer,i,j) -> segmentos por camada e vias."""
    segs, vias = [], []
    cur = [path[0]]
    for p in path[1:]:
        if p[0] != cur[-1][0]:
            segs.append(cur)
            vias.append((p[1], p[2]))
            cur = [p]
        else:
            cur.append(p)
    segs.append(cur)
    out = []
    for seg in segs:
        pts = [(seg[0][1], seg[0][2])]
        for a, b, c in zip(seg, seg[1:], seg[2:]):
            if (b[1] - a[1], b[2] - a[2]) != (c[1] - b[1], c[2] - b[2]):
                pts.append((b[1], b[2]))
        pts.append((seg[-1][1], seg[-1][2]))
        if len(pts) >= 2 and pts[0] != pts[-1]:
            out.append((seg[0][0], pts))
    return out, vias


# --------------------------------------------------------------------------- montagem
def build():
    board = pcbnew.CreateEmptyBoard()
    board.SetCopperLayerCount(2)
    ds = board.GetDesignSettings()
    nc = ds.m_NetSettings.m_DefaultNetClass
    nc.SetClearance(mm(CLEARANCE)); nc.SetTrackWidth(mm(TRACK_W["default"]))
    nc.SetViaDiameter(mm(VIA_D)); nc.SetViaDrill(mm(VIA_DRILL))
    ds.m_MinClearance = mm(CLEARANCE)
    ds.m_TrackMinWidth = mm(0.2)
    ds.m_ViasMinSize = mm(0.6)
    ds.m_MinThroughDrill = mm(0.3)

    # redes
    net_items = {}
    for name in sorted(netlist.nets()):
        ni = pcbnew.NETINFO_ITEM(board, name)
        board.Add(ni)
        net_items[name] = ni

    # footprints
    fps = {}
    for p in netlist.PARTS:
        fp = load_fp(p.footprint)
        fp.SetReference(p.ref)
        fp.SetValue(p.value)
        x, y, rot, *mode = PLACE[p.ref]
        fp.SetPosition(VECTOR2I(mm(x), mm(y)))
        if rot:
            fp.SetOrientationDegrees(rot)
        if mode and mode[0] == "c":  # (x, y) = centro dos pads
            xs = [ToMM(q.GetPosition().x) for q in fp.Pads()]
            ys = [ToMM(q.GetPosition().y) for q in fp.Pads()]
            fp.Move(VECTOR2I(mm(x - (min(xs) + max(xs)) / 2), mm(y - (min(ys) + max(ys)) / 2)))
        board.Add(fp)
        fps[p.ref] = fp
        for pad in fp.Pads():
            net = p.pins.get(int(pad.GetNumber())) if pad.GetNumber().isdigit() else None
            if net:
                pad.SetNet(net_items[net])

    # contorno e furos de fixação
    def edge(a, b):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetStart(VECTOR2I(mm(a[0]), mm(a[1])))
        s.SetEnd(VECTOR2I(mm(b[0]), mm(b[1])))
        s.SetWidth(mm(0.1))
        board.Add(s)
    corners = [(0, 0), (BOARD_W, 0), (BOARD_W, BOARD_H), (0, BOARD_H)]
    for a, b in zip(corners, corners[1:] + corners[:1]):
        edge(a, b)
    for k, (hx, hy) in enumerate(HOLES):
        mh = load_fp(netlist.FP_MH)
        mh.SetReference(f"H{k + 1}")
        mh.SetValue("M3")
        mh.SetPosition(VECTOR2I(mm(hx), mm(hy)))
        board.Add(mh)

    # pads de peças diferentes precisam ficar >= 1 mm um do outro (folga + espaço para trilha)
    pads = [(ref, pad_polygon(pad)) for ref, fp in fps.items() for pad in fp.Pads()]
    clash = []
    for i, (ra, ga) in enumerate(pads):
        for rb, gb in pads[i + 1:]:
            if ra != rb and ga.distance(gb) < 1.0:
                clash.append((ra, rb, round(ga.distance(gb), 2)))
    if clash:
        print("COLISÃO DE PEÇAS (pads < 1 mm):", sorted(set(clash)))
        sys.exit(2)
    return board, fps, net_items


def route_all(board, fps, net_items):
    nets = netlist.nets()
    r = Router(board, nets)
    r.mark_edge()
    for hx, hy in HOLES:
        r.mark_hole(hx, hy, 1.6 + 0.3)
    pads_by_net = {n: [] for n in nets}
    for ref, fp in fps.items():
        for pad in fp.Pads():
            n = pad.GetNetname()
            r.mark_pad(pad, n or None) if n else r.mark_pad(pad, None)
            ds = pad.GetDrillSize()
            if ds.x > 0:
                r.mark_via_keepout(ToMM(pad.GetPosition().x), ToMM(pad.GetPosition().y), ToMM(ds.x) / 2)
            if n:
                pads_by_net[n].append(pad)

    def width_of(net):
        return TRACK_W.get(net, TRACK_W["default"])

    failed = []
    # alimentações (trilhas largas) primeiro, GND por último
    order = sorted(nets, key=lambda n: (n == "GND", n not in TRACK_W, bbox_len(pads_by_net[n])))
    for net in order:
        pads = pads_by_net[net]
        w = width_of(net)
        # ancora cada pad em uma célula da grade (as duas camadas, pois o furo é passante)
        cells = []
        for pad in pads:
            p = pad.GetPosition()
            i, j = r.cell(ToMM(p.x), ToMM(p.y))
            cells.append((i, j, pad))
        connected = {(l, cells[0][0], cells[0][1]) for l in (0, 1)}
        remaining = cells[1:]
        # liga o pad mais próximo da árvore a cada passo
        while remaining:
            remaining.sort(key=lambda c: min(abs(c[0] - s[1]) + abs(c[1] - s[2]) for s in connected))
            i, j, pad = remaining.pop(0)
            targets = {(l, i, j) for l in (0, 1)}
            path = r.route(net, connected, targets, w)
            if path is None:
                inv = {v: k for k, v in r.netid.items()}
                own = r.owner[w]
                for l in (0, 1):
                    blk = own[l, j - 3:j + 4, i - 3:i + 4]
                    print(f"  alvo {net} pad {pad.GetNumber()} cel=({i},{j}) camada {l}:")
                    for row in blk:
                        print("   ", " ".join(("." if v == -1 else ("#" if v == -2 else inv[int(v)][:3])).rjust(3) for v in row))
                failed.append((net, pad.GetParentAsString() if hasattr(pad, "GetParentAsString") else "", pad.GetNumber()))
                continue
            segs, vias = simplify(path)
            for layer, pts in segs:
                xy = [(i2 * GRID, j2 * GRID) for i2, j2 in pts]
                r.tracks.append((net, layer, xy, w))
                r.mark_track(layer, xy, w, net)
            for vi, vj in vias:
                r.vias.append((net, vi * GRID, vj * GRID))
                r.mark_via(vi * GRID, vj * GRID, net)
            for p2 in path:
                connected.add(p2)
        # a árvore ocupa as células
    return r, failed, pads_by_net


def bbox_len(pads):
    xs = [ToMM(p.GetPosition().x) for p in pads]; ys = [ToMM(p.GetPosition().y) for p in pads]
    return (max(xs) - min(xs)) + (max(ys) - min(ys))


def commit_copper(board, r, net_items, pads_by_net):
    # trilhas: do centro real do pad até a célula âncora
    layer_id = {0: pcbnew.F_Cu, 1: pcbnew.B_Cu}
    pad_pos = {}
    for net, pads in pads_by_net.items():
        for pad in pads:
            p = pad.GetPosition()
            pad_pos[(net, r.cell(ToMM(p.x), ToMM(p.y)))] = (ToMM(p.x), ToMM(p.y))

    def add_track(net, layer, a, b, w):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(VECTOR2I(mm(a[0]), mm(a[1])))
        t.SetEnd(VECTOR2I(mm(b[0]), mm(b[1])))
        t.SetWidth(mm(w))
        t.SetLayer(layer_id[layer])
        t.SetNet(net_items[net])
        board.Add(t)

    for net, layer, pts, w in r.tracks:
        # se uma ponta cair numa célula âncora de pad, usa o centro exato do pad
        fixed = []
        for k, (x, y) in enumerate(pts):
            c = r.cell(x, y)
            fixed.append(pad_pos.get((net, c), (x, y)))
        for a, b in zip(fixed, fixed[1:]):
            if a != b:
                add_track(net, layer, a, b, w)
    for net, x, y in r.vias:
        v = pcbnew.PCB_VIA(board)
        v.SetPosition(VECTOR2I(mm(x), mm(y)))
        v.SetViaType(pcbnew.VIATYPE_THROUGH)
        v.SetWidth(mm(VIA_D))
        v.SetDrill(mm(VIA_DRILL))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNet(net_items[net])
        board.Add(v)


def add_gnd_zones(board, net_items):
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(net_items["GND"])
        z.SetLocalClearance(mm(0.3))
        z.SetMinThickness(mm(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetThermalReliefGap(mm(0.3))
        z.SetThermalReliefSpokeWidth(mm(0.4))
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_NEVER if hasattr(pcbnew, "ISLAND_REMOVAL_MODE_NEVER") else 0)
        o = z.Outline()
        o.NewOutline()
        m = 0.3
        for x, y in ((m, m), (BOARD_W - m, m), (BOARD_W - m, BOARD_H - m), (m, BOARD_H - m)):
            o.Append(mm(x), mm(y))
        board.Add(z)


def add_silk(board, fps):
    def text(s, x, y, size=1.0, angle=0, just="c"):
        t = pcbnew.PCB_TEXT(board)
        t.SetText(s)
        t.SetPosition(VECTOR2I(mm(x), mm(y)))
        t.SetLayer(pcbnew.F_SilkS)
        t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
        t.SetTextThickness(mm(0.15 if size < 1.2 else 0.2))
        t.SetTextAngleDegrees(angle)
        if just == "l":
            t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
        elif just == "r":
            t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT)
        board.Add(t)

    for ref in SILK_PINS:  # os títulos abaixo substituem a designação dos conectores
        fps[ref].Reference().SetVisible(False)
    for ref, labels in SILK_PINS.items():
        pads = sorted(fps[ref].Pads(), key=lambda p: int(p.GetNumber()))
        vertical = abs(ToMM(pads[0].GetPosition().x) - ToMM(pads[-1].GetPosition().x)) < 0.1
        for lab, pad in zip(labels, pads):
            x, y = ToMM(pad.GetPosition().x), ToMM(pad.GetPosition().y)
            if vertical:
                text(lab, x + 1.6, y, 0.9, 0, "l")
            else:
                text(lab, x, y - 1.7, 0.9, 90, "l")
    for ref, title in SILK_TITLE.items():
        pads = sorted(fps[ref].Pads(), key=lambda p: int(p.GetNumber()))
        x0, y0 = ToMM(pads[0].GetPosition().x), ToMM(pads[0].GetPosition().y)
        x1, y1 = ToMM(pads[-1].GetPosition().x), ToMM(pads[-1].GetPosition().y)
        if abs(x0 - x1) < 0.1:  # vertical: título logo acima do pino 1
            text(title, x0 - 1.0, y0 - 2.4, 1.0, 0, "l")
        else:                   # horizontal: título logo abaixo
            text(title, (x0 + x1) / 2, y0 + 2.2, 1.0, 0, "c")
    text("ESP32-DevKitC 38p", 73.3, 2.6, 1.0, 0, "c")
    text("(USB para BAIXO)", 73.3, 61.2, 1.0, 0, "c")
    text("HushRig Carrier v1 - rev A", 80, 66.0, 1.2, 0, "c")
    text("github.com/DevEduNunes/hushrig", 80, 69.0, 0.8, 0, "c")
    text("MCLK", 89.5, 37.4, 0.8, 0, "l")
    text("LED", 89.5, 49.8, 0.8, 0, "l")


def main():
    board, fps, net_items = build()
    router, failed, pads_by_net = route_all(board, fps, net_items)
    if failed:
        print("NÃO ROTEADO:", failed)
    commit_copper(board, router, net_items, pads_by_net)
    add_gnd_zones(board, net_items)
    add_silk(board, fps)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    board.Save(str(OUT))
    # o preenchimento das zonas só é estável numa placa recarregada do arquivo
    board = pcbnew.LoadBoard(str(OUT))
    board.BuildConnectivity()
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(str(OUT))
    print("escrito", OUT, f"({len(router.tracks)} trilhas, {len(router.vias)} vias)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
