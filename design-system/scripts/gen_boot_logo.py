#!/usr/bin/env python3
"""Genera el bitmap del logo de arranque de Aura (Fase 12, PLAN-UX.md).

Sustituye directamente apps/bitmaps/native/rockboxlogo.320x98x16.bmp del
fork -- mismo nombre y dimensiones que el original, asi que la regla de
bitmaps.make (bmp2rb) que lo compila a bm_rockboxlogo no necesita ningun
cambio. show_logo_boot() (apps/main.c) dibuja este bitmap centrado antes
de que el tema de Aura este cargado (D-016: lcd_set_backdrop(NULL) solo
corre dentro de aura_theme_init(), mas tarde), por eso el fondo va fijo
en negro (color "background" del tema Oscuro en tokens.json) en vez de
resolverse en tiempo de arranque -- ver D-051 en DECISIONS.md.

No es parte del pipeline normal de generate.py (ese produce out/, que no
se versiona; este bitmap SI se versiona, como el resto de
apps/bitmaps/native/) -- se ejecuta a mano, una vez, cuando el diseno
cambia:

  design-system/.venv/bin/python3 design-system/scripts/gen_boot_logo.py
"""
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
TOKENS_PATH = ROOT / "tokens.json"
FONT_PATH = ROOT / "vendor" / "inter-ttf" / "Inter-SemiBold.ttf"
OUT_PATH = (
    ROOT.parent
    / "firmware"
    / "rockbox"
    / "apps"
    / "bitmaps"
    / "native"
    / "rockboxlogo.320x98x16.bmp"
)

WORDMARK = "aura"


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def hex_to_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i : i + 2], 16) for i in (0, 2, 4))


def main():
    if not FONT_PATH.exists():
        die(f"falta {FONT_PATH} -- ejecuta design-system/scripts/fetch_assets.sh")

    tokens = json.loads(TOKENS_PATH.read_text())
    width, height = 320, 98
    bg = hex_to_rgb(tokens["color"]["dark"]["background"])
    fg = hex_to_rgb(tokens["color"]["dark"]["text_primary"])

    img = Image.new("RGB", (width, height), bg)
    draw = ImageDraw.Draw(img)

    # Busca el tamano de fuente mas grande que entre en el ancho
    # disponible con margen, partiendo de un tamano generoso.
    size = 64
    font = ImageFont.truetype(str(FONT_PATH), size)
    margin = 32
    while size > 8:
        font = ImageFont.truetype(str(FONT_PATH), size)
        bbox = draw.textbbox((0, 0), WORDMARK, font=font)
        text_w = bbox[2] - bbox[0]
        if text_w <= width - margin:
            break
        size -= 1

    bbox = draw.textbbox((0, 0), WORDMARK, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    x = (width - text_w) // 2 - bbox[0]
    y = (height - text_h) // 2 - bbox[1]

    draw.text((x, y), WORDMARK, font=font, fill=fg)

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT_PATH, format="BMP")
    print(f"==> Logo de arranque escrito en {OUT_PATH} ({size}px, {WORDMARK!r})")


if __name__ == "__main__":
    main()
