#!/usr/bin/env python3
"""DRC e conectividade da PCB (o kicad-cli 7 não tem DRC). Confere:
 - folga cobre a cobre entre redes diferentes, por camada (>= CLEARANCE)
 - largura mínima de trilha, anel das vias, folga entre furos, distância à borda
 - cada rede do netlist.py forma UM só componente conexo (pads + trilhas + vias + preenchimento do GND)
 - os pinos de cada pad batem com netlist.py
"""
import pathlib
import sys

import pcbnew
from pcbnew import ToMM
from shapely import STRtree
from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import netlist
from gen_pcb import pad_polygon, BOARD_W, BOARD_H

PCB = pathlib.Path(__file__).resolve().parent.parent / "kicad" / "hushrig_carrier.kicad_pcb"
CLEARANCE, MIN_TRACK, MIN_ANNULAR, MIN_HOLE_GAP, EDGE = 0.2, 0.2, 0.15, 0.25, 0.3

board = pcbnew.LoadBoard(str(PCB))
LAYERS = {pcbnew.F_Cu: "F", pcbnew.B_Cu: "B"}
errors = []


def zone_polys(layer):
    out = []
    for z in board.Zones():
        if z.GetLayer() != layer or not z.IsFilled():
            continue
        ps = z.GetFilledPolysList(layer)
        for i in range(ps.OutlineCount()):
            o = ps.Outline(i)
            pts = [(ToMM(o.CPoint(k).x), ToMM(o.CPoint(k).y)) for k in range(o.PointCount())]
            holes = []
            for h in range(ps.HoleCount(i)):
                ho = ps.Hole(i, h)
                holes.append([(ToMM(ho.CPoint(k).x), ToMM(ho.CPoint(k).y)) for k in range(ho.PointCount())])
            if len(pts) >= 3:
                out.append((z.GetNetname(), Polygon(pts, holes).buffer(0)))
    return out


# itens: (netname|None, camada, geometria, descrição)
items = []
pad_nodes = {}
for fp in board.GetFootprints():
    for pad in fp.Pads():
        if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
            continue
        net = pad.GetNetname() or None
        desc = f"{fp.GetReference()}.{pad.GetNumber()}"
        g = pad_polygon(pad)
        for layer in LAYERS:
            items.append((net or f"~{desc}", layer, g, desc))
        pad_nodes[desc] = net
for t in board.GetTracks():
    if t.GetClass() == "PCB_VIA":
        p = t.GetPosition()
        g = Point(ToMM(p.x), ToMM(p.y)).buffer(ToMM(t.GetWidth()) / 2, 16)
        for layer in LAYERS:
            items.append((t.GetNetname(), layer, g, f"via@({ToMM(p.x):.2f},{ToMM(p.y):.2f})"))
        if (ToMM(t.GetWidth()) - ToMM(t.GetDrillValue())) / 2 < MIN_ANNULAR - 1e-6:
            errors.append(f"anel da via pequeno em ({ToMM(p.x):.2f},{ToMM(p.y):.2f})")
    else:
        a, b = t.GetStart(), t.GetEnd()
        w = ToMM(t.GetWidth())
        if w < MIN_TRACK - 1e-6:
            errors.append(f"trilha fina ({w} mm) na rede {t.GetNetname()}")
        g = LineString([(ToMM(a.x), ToMM(a.y)), (ToMM(b.x), ToMM(b.y))]).buffer(w / 2, 8)
        items.append((t.GetNetname(), t.GetLayer(), g, f"trilha {t.GetNetname()}"))
for layer in LAYERS:
    for net, poly in zone_polys(layer):
        items.append((net, layer, poly, f"zona {net}/{LAYERS[layer]}"))

# --- folga entre redes diferentes
for layer, name in LAYERS.items():
    its = [i for i in items if i[1] == layer]
    tree = STRtree([i[2] for i in its])
    for a_idx, a in enumerate(its):
        for b_idx in tree.query(a[2].buffer(CLEARANCE + 0.5)):
            if b_idx <= a_idx:
                continue
            b = its[b_idx]
            if a[0] == b[0]:
                continue
            d = a[2].distance(b[2])
            if d < CLEARANCE - 1e-3:
                errors.append(f"[{name}] folga {d:.3f} mm < {CLEARANCE}: {a[0]} ({a[3]}) x {b[0]} ({b[3]})")

# --- distância à borda e furos
outline = box(0, 0, BOARD_W, BOARD_H)
for net, layer, g, desc in items:
    if layer != pcbnew.F_Cu or desc.startswith("zona"):
        continue
    if outline.exterior.distance(g) < EDGE - 1e-3 or not outline.contains(g):
        errors.append(f"perto da borda: {desc} ({net})")
holes = []
for fp in board.GetFootprints():
    for pad in fp.Pads():
        ds = pad.GetDrillSize()
        if ds.x > 0:
            p = pad.GetPosition()
            holes.append((ToMM(p.x), ToMM(p.y), ToMM(ds.x) / 2, f"{fp.GetReference()}.{pad.GetNumber()}"))
for t in board.GetTracks():
    if t.GetClass() == "PCB_VIA":
        p = t.GetPosition()
        holes.append((ToMM(p.x), ToMM(p.y), ToMM(t.GetDrillValue()) / 2, "via"))
for i, a in enumerate(holes):
    for b in holes[i + 1:]:
        gap = ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5 - a[2] - b[2]
        if gap < MIN_HOLE_GAP and not (a[3].split(".")[0] == b[3].split(".")[0] and gap > 0.5):
            errors.append(f"furos muito próximos ({gap:.2f} mm): {a[3]} e {b[3]}")

# --- conectividade
parent = {}


def find(x):
    while parent.setdefault(x, x) != x:
        parent[x] = parent[parent[x]]
        x = parent[x]
    return x


def union(a, b):
    parent[find(a)] = find(b)


for layer in LAYERS:
    its = [(k, i) for k, i in enumerate(items) if i[1] == layer]
    tree = STRtree([i[2] for _, i in its])
    for ai, (k, a) in enumerate(its):
        for bi in tree.query(a[2]):
            kb, b = its[bi]
            if kb > k and a[0] == b[0] and a[2].intersects(b[2]):
                union(k, kb)
# pads e vias furados ligam as duas camadas: mesma descrição (ref.pino ou posição da via)
by_desc = {}
for k, (net, layer, g, desc) in enumerate(items):
    if not desc.startswith(("trilha", "zona")):
        by_desc.setdefault(desc, []).append(k)
for ks in by_desc.values():
    for k in ks[1:]:
        union(ks[0], k)

want = netlist.nets()
for net, pins in want.items():
    comps = {}
    for ref, pin in pins:
        ks = by_desc.get(f"{ref}.{pin}")
        if not ks:
            errors.append(f"pad {ref}.{pin} não existe na PCB")
            continue
        comps.setdefault(find(ks[0]), []).append(f"{ref}.{pin}")
    if len(comps) > 1:
        errors.append(f"rede {net} aberta: {list(comps.values())}")
# o net de cada pad deve bater com netlist.py
for p in netlist.PARTS:
    for pin, net in p.pins.items():
        if net and pad_nodes.get(f"{p.ref}.{pin}") != net:
            errors.append(f"pad {p.ref}.{pin}: rede {pad_nodes.get(f'{p.ref}.{pin}')} != {net}")

if errors:
    print(f"{len(errors)} problemas:")
    for e in errors[:60]:
        print("  -", e)
    sys.exit(1)
print(f"OK: DRC sem violações e {len(want)} redes conectadas ({len(items)} itens de cobre)")
