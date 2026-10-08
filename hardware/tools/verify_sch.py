#!/usr/bin/env python3
"""Exporta a netlist do esquemático com o kicad-cli e compara com netlist.py (redes e pinos)."""
import pathlib
import re
import subprocess
import sys
import tempfile

import netlist

sch = pathlib.Path(__file__).resolve().parent.parent / "kicad" / "hushrig_carrier.kicad_sch"
with tempfile.TemporaryDirectory() as d:
    out = pathlib.Path(d) / "sch.net"
    subprocess.run(["kicad-cli", "sch", "export", "netlist", str(sch), "-o", str(out)], check=True, capture_output=True)
    text = out.read_text()

got = {}
nets_text = text[text.index("(nets"):]
for chunk in nets_text.split("(net (code")[1:]:
    name = re.search(r'\(name "([^"]+)"\)', chunk).group(1).lstrip("/")
    nodes = re.findall(r'\(node \(ref "([^"]+)"\) \(pin "([^"]+)"\)', chunk)
    got[name] = sorted((r, p) for r, p in nodes if not r.startswith("#"))
want = netlist.nets()

bad = 0
for n in sorted(set(got) | set(want)):
    g, w = got.get(n), want.get(n)
    if w is None and n.startswith(("unconnected-", "Net-")):
        continue
    if g != w:
        bad += 1
        print(f"DIVERGE {n}: esquemático={g}  netlist.py={w}")
parts = set(re.findall(r'\(comp \(ref "([^"]+)"\)', text))
missing = {p.ref for p in netlist.PARTS} - parts
if missing:
    bad += 1
    print("peças ausentes no esquemático:", sorted(missing))
print("OK: esquemático == netlist.py" if not bad else f"{bad} divergências")
sys.exit(1 if bad else 0)
