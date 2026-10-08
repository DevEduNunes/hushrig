#!/usr/bin/env python3
"""Confere que a tabela PARAMS da página (index.html) é idêntica à de Rig.h (id, min, max, na mesma ordem).

O celular e o pedal só trocam valores por posição; se as tabelas divergirem, os sliders ficam trocados.
"""
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent / "src"
rig = (root / "Rig.h").read_text(encoding="utf-8")
html = (root / "web" / "index.html").read_text(encoding="utf-8")

# { "inputGain",     -24.0f,  24.0f,   0.0f },
cpp = [(m[0], float(m[1]), float(m[2]))
       for m in re.findall(r'\{\s*"(\w+)"\s*,\s*(-?[\d.]+)f\s*,\s*(-?[\d.]+)f\s*,\s*-?[\d.]+f\s*\}', rig)]

block = re.search(r"const PARAMS = \[(.*?)\n\];", html, re.S).group(1)
js = [(m[0], float(m[1]), float(m[2]))
      for m in re.findall(r'\["(\w+)",\s*(-?[\d.]+),\s*(-?[\d.]+)\]', block)]

if not cpp or cpp != js:
    print("Tabelas divergem!")
    for i in range(max(len(cpp), len(js))):
        a = cpp[i] if i < len(cpp) else None
        b = js[i] if i < len(js) else None
        if a != b:
            print(f"  #{i}: Rig.h={a}  index.html={b}")
    sys.exit(1)
print(f"OK: {len(cpp)} parâmetros iguais em Rig.h e index.html")
