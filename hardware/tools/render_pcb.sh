#!/bin/sh
# uso: render_pcb.sh <saida.png> [camadas]   (camadas padrão: cobre + serigrafia + contorno)
set -e
OUT="$1"; LAYERS="${2:-F.Cu,B.Cu,F.Silkscreen,Edge.Cuts}"
DIR="$(dirname "$0")/../kicad"
TMP="$(mktemp -d)"
kicad-cli pcb export svg "$DIR/hushrig_carrier.kicad_pcb" -o "$TMP/pcb.svg" --layers "$LAYERS" --page-size-mode 2 --exclude-drawing-sheet >/dev/null
python3 -c "import cairosvg;cairosvg.svg2png(url='$TMP/pcb.svg',write_to='$OUT',output_width=2000,background_color='white')"
rm -rf "$TMP"
