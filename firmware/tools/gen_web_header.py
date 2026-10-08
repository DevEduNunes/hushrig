#!/usr/bin/env python3
"""Gera src/web_index.h (a página de controle como string C++) a partir de src/web/index.html.

Evita depender do EMBED_TXTFILES do ESP-IDF/PlatformIO. `--check` só confere se o header está em dia (CI).
"""
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parent.parent / "src"
html = (root / "web" / "index.html").read_text(encoding="utf-8")
DELIM = "HUSHRIG_HTML"
assert f"){DELIM}\"" not in html
out = ("// GERADO por firmware/tools/gen_web_header.py a partir de web/index.html. Não edite à mão.\n"
       "#pragma once\n\n"
       f"static const char kIndexHtml[] = R\"{DELIM}({html}){DELIM}\";\n"
       "static constexpr unsigned kIndexHtmlLen = sizeof (kIndexHtml) - 1;\n")
dst = root / "web_index.h"
if "--check" in sys.argv:
    if not dst.exists() or dst.read_text(encoding="utf-8") != out:
        print("web_index.h desatualizado: rode firmware/tools/gen_web_header.py")
        sys.exit(1)
    print("OK: web_index.h em dia")
else:
    dst.write_text(out, encoding="utf-8")
    print("escrito", dst, f"({len(html)} bytes)")
