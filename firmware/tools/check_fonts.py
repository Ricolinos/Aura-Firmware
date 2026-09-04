#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Verificacion mecanica de cobertura de glifos de las fuentes Inter (D-357).

Portado de check_fonts.py de moonlit-aura (solo lectura, ver PLAN-ronda-
ajustes-2-maestro.md SS D.3) y adaptado a Aura: sin transliteracion (Aura
no tiene equivalente de moonlit_translit.c -- si un glifo falta, falta),
con un chequeo explicito del rango cirilico U+0400-U+045F ademas de las
cadenas reales de aura_lang.c.

  check_fonts.py <archivo.fnt>
      Lee la cabecera RB12 (firmware/rockbox/firmware/font.c:378-411, 36
      bytes, little-endian) e imprime firstchar/defaultchar/size/height/
      maxwidth. No falla por si solo -- es un reporte.

  check_fonts.py --coverage [--fonts DIR] [--lang ARCHIVO]
      Lee la tabla de glifos de cada .fnt de out/fonts/ (las 14 de Inter,
      D-357) y la compara con: (a) todas las cadenas literales de las
      seis columnas de aura_lang.c (es/en/fr/de/ru/it), (b) el rango
      cirilico completo U+0400-U+045F, y (c) una lista curada de
      puntuacion tipografica frecuente en metadatos de musica. Falla si
      falta algo de (a) o (b) -- la UI propia y el cirilico declarado en
      el contrato no pueden tener huecos; lo de (c) se reporta sin
      romper el build.
"""
import argparse
import struct
import sys

RB12_HEADER = struct.Struct("<4sHHHHiiiiii")


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def read_rb12_header(path):
    with open(path, "rb") as f:
        data = f.read(RB12_HEADER.size)
    if len(data) < RB12_HEADER.size:
        die(f"{path}: archivo mas chico que la cabecera RB12 ({len(data)} bytes)")
    (magic, maxwidth, height, ascent, depth, firstchar, defaultchar,
     size, bits_size, noffset, nwidth) = RB12_HEADER.unpack(data)
    if magic != b"RB12":
        die(f"{path}: cabecera invalida (magic={magic!r}, se esperaba RB12)")
    return {
        "maxwidth": maxwidth, "height": height, "ascent": ascent, "depth": depth,
        "firstchar": firstchar, "defaultchar": defaultchar, "size": size,
        "bits_size": bits_size, "noffset": noffset, "nwidth": nwidth,
    }


def cmd_header(path):
    h = read_rb12_header(path)
    print(f"{path}: firstchar={h['firstchar']} defaultchar={h['defaultchar']} "
          f"size={h['size']} height={h['height']} maxwidth={h['maxwidth']}")


# --- coverage (D-357, portado de moonlit D-066) ---------------------------

# font.c:50. Por debajo de este bits_size la tabla de offsets es de 16
# bits; por encima, de 32. Hace falta para saltar hasta la tabla de
# anchos, que es la que dice si un codigo tiene glifo de verdad.
MAX_FONTSIZE_FOR_16_BIT_OFFSETS = 0xFFDB

# D-357: rango cirilico minimo exigido por el maestro (SS D.3), aunque
# ninguna cadena de aura_lang.c lo use todavia hoy -- ru ya esta activo
# en el selector (D-357), asi que el firmware ya puede mostrar texto
# cirilico ingresado por el usuario (nombres de pista/album/artista).
CYRILLIC_RANGE = range(0x0400, 0x0460)  # U+0400-U+045F inclusive

# Codepoints frecuentes en metadatos de musica reales que NO estan en el
# rango ASCII basico. Lista curada (misma que moonlit D-066, sin las
# entradas que dependen de su transliteracion propia).
METADATA_CODEPOINTS = (
    list(range(0x2010, 0x2028)) +   # guiones, comillas tipograficas, puntos suspensivos
    list(range(0x2030, 0x203B)) +   # por mil, primas, angulares, dagas, vinetas
    [0x2122,                        # (TM)
     0x266A, 0x266B,                # corcheas
     0x2605, 0x2606,                # estrellas
     0x2665,                        # corazon
     0x2022,                        # vineta
     0x00A0]                        # espacio duro
)


def read_glyph_table(path):
    """-> (firstchar, size, set de codepoints con glifo real).

    Un codigo dentro de [firstchar, firstchar+size) puede seguir sin
    glifo: convttf emite una entrada de ancho 0 para el hueco. Por eso
    no basta con el rango de la cabecera -- hay que leer la tabla de
    anchos."""
    h = read_rb12_header(path)
    with open(path, "rb") as f:
        data = f.read()

    # font.c:224-238: tras el bitmap se ALINEA (16 o 32 bits segun
    # bits_size) antes de la tabla de offsets.
    off = RB12_HEADER.size + h["bits_size"]
    if h["bits_size"] < MAX_FONTSIZE_FOR_16_BIT_OFFSETS:
        off = (off + 1) & ~1
        off += h["noffset"] * 2
    else:
        off = (off + 3) & ~3
        off += h["noffset"] * 4

    if off + h["nwidth"] != len(data):
        die(f"{path}: la tabla de anchos termina en {off + h['nwidth']} "
            f"pero el archivo mide {len(data)} -- el calculo de "
            f"desplazamiento no coincide con font.c")

    widths = data[off:off + h["nwidth"]]
    covered = set()
    if h["nwidth"] == 0:
        covered = set(range(h["firstchar"], h["firstchar"] + h["size"]))
    else:
        for i, w in enumerate(widths):
            if w > 0:
                covered.add(h["firstchar"] + i)
    return h, covered


def lang_codepoints(lang_path):
    """Codepoints de todas las cadenas literales de aura_lang.c (las seis
    columnas es/en/fr/de/ru/it)."""
    import re
    text = open(lang_path, encoding="utf-8").read()
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    cps = set()
    for lit in re.findall(r'"((?:[^"\\]|\\.)*)"', text):
        lit = lit.replace("\\n", "\n").replace("\\t", "\t").replace('\\"', '"')
        for ch in lit:
            cps.add(ord(ch))
    return cps


def fmt_cp(cp):
    ch = chr(cp)
    shown = ch if ch.isprintable() and not ch.isspace() else " "
    return f"U+{cp:04X} '{shown}'"


def cmd_coverage(fonts_dir, lang_path):
    import glob
    import os

    fonts = sorted(glob.glob(os.path.join(fonts_dir, "*.fnt")))
    if not fonts:
        die(f"no hay .fnt en {fonts_dir}")

    ui = lang_codepoints(lang_path)
    cyrillic = set(CYRILLIC_RANGE)
    meta = set(METADATA_CODEPOINTS)

    print(f"== cobertura de glifos ==  UI {len(ui)} codepoints, "
          f"cirilico {len(cyrillic)}, metadatos {len(meta)} (D-357)")
    hard_failures = 0

    for path in fonts:
        h, covered = read_glyph_table(path)
        name = os.path.basename(path)
        miss_ui = sorted(c for c in ui if c not in covered and c >= 32)
        miss_cyr = sorted(c for c in cyrillic if c not in covered)
        miss_meta = sorted(c for c in meta if c not in covered)

        print(f"\n{name}: firstchar={h['firstchar']} size={h['size']} "
              f"defaultchar={h['defaultchar']} glifos_reales={len(covered)}")
        if miss_ui:
            hard_failures += 1
            print(f"   FALTA de la UI ({len(miss_ui)}): "
                  + ", ".join(fmt_cp(c) for c in miss_ui[:12])
                  + (" ..." if len(miss_ui) > 12 else ""))
        else:
            print("   UI (es/en/fr/de/ru/it): completa")
        if miss_cyr:
            hard_failures += 1
            print(f"   FALTA cirilico U+0400-U+045F ({len(miss_cyr)}/{len(cyrillic)}): "
                  + ", ".join(fmt_cp(c) for c in miss_cyr[:12])
                  + (" ..." if len(miss_cyr) > 12 else ""))
        else:
            print("   cirilico U+0400-U+045F: completo")
        print(f"   metadatos: faltan {len(miss_meta)}/{len(meta)}"
              + (("  " + ", ".join(fmt_cp(c) for c in miss_meta[:8])
                  + (" ..." if len(miss_meta) > 8 else "")) if miss_meta else ""))

    if hard_failures:
        die(f"{hard_failures} carencia(s) de UI/cirilico entre las fuentes -- "
            f"corrige design-system/generate.py (rango de convttf o fuente base) "
            f"y regenera out/fonts/.")
    print("\ncheck_fonts: la UI y el cirilico estan cubiertos en todos los roles.")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs="?", help=".fnt a inspeccionar")
    parser.add_argument("--coverage", action="store_true",
                         help="compara la tabla de glifos de cada .fnt con la UI y el cirilico")
    parser.add_argument("--fonts", default="design-system/out/fonts",
                         help="directorio de .fnt para --coverage")
    parser.add_argument("--lang", default="firmware/rockbox/apps/aura/aura_lang.c",
                         help="fuente de las cadenas de UI para --coverage")
    args = parser.parse_args()

    if args.coverage:
        cmd_coverage(args.fonts, args.lang)
    elif args.path:
        cmd_header(args.path)
    else:
        parser.error("pasa una ruta .fnt o --coverage")


if __name__ == "__main__":
    main()
