#!/usr/bin/env python3
"""Gera hardware/kicad/hushrig_carrier.kicad_sch a partir de netlist.py (usa os símbolos padrão do KiCad)."""

import copy
import math
import pathlib
import sys
import uuid

from kiutils.items.common import Effects, Font, Justify, Position, Property
from kiutils.items.schitems import (Connection, LocalLabel, SchematicSymbol, SymbolProjectInstance,
                                    SymbolProjectPath, Text, HierarchicalSheetInstance)
from kiutils.items.common import Stroke, TitleBlock
from kiutils.schematic import Schematic
from kiutils.symbol import SymbolLib

import netlist

SYM_DIR = pathlib.Path("/usr/share/kicad/symbols")
OUT = pathlib.Path(__file__).resolve().parent.parent / "kicad" / "hushrig_carrier.kicad_sch"
PROJECT = "hushrig_carrier"
STUB = 5.08
RAILS = {"GND": "power:GND", "+5V": "power:+5V", "+3V3": "power:+3V3"}

# Posição (mm) de cada símbolo, agrupados por bloco. Folha A2 (594 x 420).
POS = {
    # 1. Entrada de guitarra -> buffer -> ADC
    "J5": (45, 65), "C1": (95, 65), "R1": (135, 65), "R4": (175, 65), "C2": (215, 65),
    "U1A": (275, 65), "R5": (330, 65), "C3": (370, 65),
    "U1B": (275, 115), "U1C": (330, 115),
    # 2. Referência e 5V analógico
    "R2": (60, 180), "R3": (100, 180), "C4": (140, 180), "C7": (180, 180),
    "R6": (230, 180), "C5": (270, 180), "C6": (310, 180),
    # 3. Barramentos, clocks I2S e saída
    "C8": (60, 245), "C9": (100, 245), "C10": (140, 245), "C11": (180, 245), "C12": (220, 245),
    "R7": (270, 245), "R8": (310, 245), "R9": (350, 245), "R13": (390, 245),
    # 4. Controles e bateria
    "R10": (60, 315), "C13": (100, 315), "R11": (150, 315), "R12": (190, 315), "C14": (230, 315),
    # 5. Cabeçalhos
    "J1": (60, 372), "J2": (130, 372), "J3": (205, 372), "J4": (270, 372),
    "J6": (330, 350), "J8": (400, 350), "J10": (470, 350),
    "J7": (330, 375), "J9": (400, 375), "J11": (470, 375),
}
TITLES = [
    (30, 35, "1. ENTRADA DA GUITARRA: buffer de alta impedancia (1 MOhm) -> saida para o ADC"),
    (30, 150, "2. REFERENCIA 2,5 V E 5 V ANALOGICO FILTRADO"),
    (30, 215, "3. DESACOPLAMENTO DOS BARRAMENTOS 5 V / 3,3 V  |  CLOCKS I2S COM RESISTOR EM SERIE  |  SAIDA"),
    (30, 285, "4. CONTROLES (footswitch, LED) E LEITURA DA BATERIA"),
    (30, 332, "5. ESP32-DevKitC (38 pinos), MODULOS PCM1808 / PCM5102A (por cabinhos) E CONECTORES PARA FORA DA PLACA"),
]
NOTES = {"U1": (262, 90), "J1": (22, 404), "J2": (80, 408), "J3": (170, 392), "J4": (250, 398)}

_libs = {}


def load_symbol(lib_id):
    nick, name = lib_id.split(":")
    if lib_id not in _libs:
        lib = SymbolLib.from_file(str(SYM_DIR / f"{nick}.kicad_sym"))
        sym = next(s for s in lib.symbols if s.entryName == name)
        if sym.extends is not None:  # achata a herança: usa o símbolo-pai com o nome e os campos do filho
            parent = next(s for s in lib.symbols if s.entryName == sym.extends)
            flat = copy.deepcopy(parent)
            flat.entryName = name
            for u in flat.units:
                u.entryName = name
            flat.properties = copy.deepcopy(sym.properties)
            sym = flat
        sym = copy.deepcopy(sym)
        sym.libraryNickname = nick
        _libs[lib_id] = sym
    return _libs[lib_id]


def pins_of(sym, unit):
    out = []
    for u in sym.units:
        if u.unitId in (0, unit) and u.styleId in (0, 1):
            out.extend(u.pins)
    return out


def eff(h=1.27, justify=None, hide=False):
    return Effects(font=Font(height=h, width=h), justify=Justify(horizontally=justify) if justify else Justify(), hide=hide)


_uid_counter = [0]


def uid():
    """UUIDs determinísticos: regenerar o esquemático não muda o arquivo à toa."""
    _uid_counter[0] += 1
    return str(uuid.uuid5(uuid.NAMESPACE_URL, f"hushrig-carrier/{_uid_counter[0]}"))


def main():
    sch = Schematic.create_new()
    sch.version = 20230121
    sch.generator = "hushrig_gen"
    sch.uuid = uid()
    sch.paper.paperSize = "A2"
    sch.titleBlock = TitleBlock(title="HushRig Carrier v1", date="2026-10-08", revision="A", company="DevEduNunes",
                                comments={1: "ESP32 + PCM1808 + PCM5102A: pedal de guitarra", 2: "Licenca AGPL-3.0. Gerado por hardware/tools/gen_sch.py"})
    sch.sheetInstances = [HierarchicalSheetInstance(instancePath="/", page="1")]

    parts = {p.ref: p for p in netlist.PARTS}
    used_libs = []
    pwr_count = [0]

    def add_lib(lib_id):
        if lib_id not in used_libs:
            used_libs.append(lib_id)
            sch.libSymbols.append(load_symbol(lib_id))

    def place(lib_id, ref, value, footprint, x, y, unit=1, pinnets=None, hide_fields=False):
        add_lib(lib_id)
        sym = load_symbol(lib_id)
        nick, name = lib_id.split(":")
        props = [
            Property(key="Reference", value=ref, id=0, position=Position(x + 2.54, y - 3.81, 0), effects=eff(hide=hide_fields, justify="left")),
            Property(key="Value", value=value, id=1, position=Position(x + 2.54, y + 3.81, 0), effects=eff(hide=hide_fields, justify="left")),
            Property(key="Footprint", value=footprint, id=2, position=Position(x, y, 0), effects=eff(hide=True)),
            Property(key="Datasheet", value="~", id=3, position=Position(x, y, 0), effects=eff(hide=True)),
        ]
        pins = pins_of(sym, unit)
        inst = SchematicSymbol(libraryNickname=nick, entryName=name, position=Position(x, y, 0), unit=unit, inBom=not ref.startswith("#"),
                               onBoard=not ref.startswith("#"), dnp=False, uuid=uid(), properties=props,
                               pins={p.number: uid() for p in pins},
                               instances=[SymbolProjectInstance(name=PROJECT, paths=[SymbolProjectPath(sheetInstancePath="/" + sch.uuid, reference=ref, unit=unit)])])
        sch.schematicSymbols.append(inst)
        for p in pins:
            net = (pinnets or {}).get(p.number)
            if not net:
                continue
            ax, ay = x + p.position.X, y - p.position.Y
            out = math.radians((p.position.angle + 180) % 360)
            ex, ey = round(ax + STUB * math.cos(out), 4), round(ay - STUB * math.sin(out), 4)
            sch.graphicalItems.append(Connection(type="wire", points=[Position(ax, ay), Position(ex, ey)], stroke=Stroke(width=0, type="default"), uuid=uid()))
            if net in RAILS:
                pwr_count[0] += 1
                rail_id = RAILS[net]
                add_lib(rail_id)
                nm = rail_id.split(":")[1]
                sch.schematicSymbols.append(SchematicSymbol(
                    libraryNickname="power", entryName=nm, position=Position(ex, ey, 0), unit=1, inBom=False, onBoard=False, dnp=False, uuid=uid(),
                    properties=[Property(key="Reference", value=f"#PWR{pwr_count[0]:02d}", id=0, position=Position(ex, ey, 0), effects=eff(hide=True)),
                                Property(key="Value", value=nm, id=1, position=Position(ex, ey + (3.5 if nm == "GND" else -3.5), 0), effects=eff())],
                    pins={"1": uid()},
                    instances=[SymbolProjectInstance(name=PROJECT, paths=[SymbolProjectPath(sheetInstancePath="/" + sch.uuid, reference=f"#PWR{pwr_count[0]:02d}", unit=1)])]))
            else:
                angle = int((p.position.angle + 180) % 360)
                sch.labels.append(LocalLabel(text=net, position=Position(ex, ey, angle), effects=eff(justify="right" if angle in (180, 270) else "left"), uuid=uid()))

    for ref, part in parts.items():
        if ref == "U1":
            # LM358 tem 3 unidades: A, B e alimentação
            for unit, key in ((1, "U1A"), (2, "U1B"), (3, "U1C")):
                x, y = POS[key]
                place(part.lib, ref, part.value, part.footprint, x, y, unit=unit, pinnets={str(k): v for k, v in part.pins.items()})
            continue
        x, y = POS[ref]
        place(part.lib, ref, part.value, part.footprint, x, y, pinnets={str(k): v for k, v in part.pins.items()})

    # PWR_FLAG nas três redes de alimentação (para o ERC saber que há fonte)
    add_lib("power:PWR_FLAG")
    for net, (x, y) in {"+5V": (540, 350), "+3V3": (540, 362), "GND": (540, 376)}.items():
        pwr_count[0] += 1
        nm = "PWR_FLAG"
        ref = f"#FLG{pwr_count[0]:02d}"
        # o flag é ligado à rede por uma etiqueta/símbolo de alimentação no mesmo ponto
        rail = RAILS[net]
        add_lib(rail)
        for lib_id, r in ((rail, f"#PWR{pwr_count[0]:02d}"), ("power:PWR_FLAG", ref)):
            n = lib_id.split(":")[1]
            sch.schematicSymbols.append(SchematicSymbol(
                libraryNickname="power", entryName=n, position=Position(x, y, 0), unit=1, inBom=False, onBoard=False, dnp=False, uuid=uid(),
                properties=[Property(key="Reference", value=r, id=0, position=Position(x, y, 0), effects=eff(hide=True)),
                            Property(key="Value", value=n, id=1, position=Position(x + 2, y - 2, 0), effects=eff(hide=(n == "PWR_FLAG")))],
                pins={"1": uid()},
                instances=[SymbolProjectInstance(name=PROJECT, paths=[SymbolProjectPath(sheetInstancePath="/" + sch.uuid, reference=r, unit=1)])]))

    for x, y, text in TITLES:
        sch.texts.append(Text(text=text, position=Position(x, y, 0), effects=eff(2.0, justify="left"), uuid=uid()))
    for ref, (nx, ny) in NOTES.items():
        sch.texts.append(Text(text=f"{ref}: {parts[ref].note}", position=Position(nx, ny, 0), effects=eff(1.0, justify="left"), uuid=uid()))
    sch.to_file(str(OUT))
    print("escrito", OUT)


if __name__ == "__main__":
    OUT.parent.mkdir(parents=True, exist_ok=True)
    main()
