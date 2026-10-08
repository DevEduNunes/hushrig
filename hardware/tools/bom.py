#!/usr/bin/env python3
"""Lista de materiais agrupada (CSV) a partir de netlist.py."""
import csv
import sys
from collections import defaultdict

import netlist

BUY = {  # descrição de compra por (símbolo, valor)
    ("Device:R", None): "Resistor 1/4 W 5% (ou 1%), furo passante",
    ("Device:C", "100n"): "Capacitor cerâmico 100 nF, passo 5 mm",
    ("Device:C", "100p"): "Capacitor cerâmico 100 pF, passo 5 mm",
    ("Device:C", "1u filme"): "Capacitor de filme 1 uF (poliéster/MKT), passo 5 mm",
    ("Device:C_Polarized", None): "Capacitor eletrolítico radial (≥ 16 V)",
    ("Amplifier_Operational:LM358", None): "MCP6022-I/P (DIP-8) ou TLC2272; use soquete DIP-8",
}
g = defaultdict(list)
for p in netlist.PARTS:
    if p.ref.startswith("J"):
        key = (p.value, p.footprint.split(":")[1])
    else:
        key = (p.value, p.footprint.split(":")[1])
    g[(p.lib, key)].append(p.ref)
w = csv.writer(sys.stdout, lineterminator="\n")
w.writerow(["Qtd", "Referências", "Valor", "Footprint", "Observação"])
for (lib, (value, fp)), refs in sorted(g.items(), key=lambda kv: (kv[1][0] if False else kv[0][1][0], kv[0][1][1])):
    obs = BUY.get((lib, value)) or BUY.get((lib, None)) or ""
    if lib.startswith("Connector"):
        obs = "Barra de pinos 2,54 mm" + (" fêmea (soquete do DevKit)" if "Socket" in fp else " macho")
    refs.sort(key=lambda r: (r.rstrip("0123456789"), int("".join(c for c in r if c.isdigit()))))
    w.writerow([len(refs), " ".join(refs), value, fp, obs])
