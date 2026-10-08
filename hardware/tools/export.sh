#!/bin/sh
# Gera gerbers + furação (zip pronto para a fábrica), renders e BOM em hardware/.
set -e
cd "$(dirname "$0")/.."
PCB=kicad/hushrig_carrier.kicad_pcb
rm -rf gerbers && mkdir -p gerbers
kicad-cli pcb export gerbers "$PCB" -o gerbers/ --layers "F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts" --no-protel-ext >/dev/null
kicad-cli pcb export drill "$PCB" -o gerbers/ --format excellon --drill-origin absolute --excellon-units mm --generate-map --map-format gerberx2 >/dev/null
( cd gerbers && rm -f ../hushrig_carrier_gerbers.zip && zip -q ../hushrig_carrier_gerbers.zip ./* )
mv hushrig_carrier_gerbers.zip gerbers/
python3 tools/bom.py > bom.csv
ls -la gerbers
