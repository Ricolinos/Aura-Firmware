#!/usr/bin/env python3
"""Genera el bitmap de la pantalla de conexion USB (Fase 19, PLAN-UX.md).

Sustituye apps/bitmaps/native/usblogo.176x48x16.bmp del fork -- mismo
nombre y dimensiones que el original, asi que la regla de
bitmaps.make (bmp2rb) que lo compila a bm_usblogo no necesita ningun
cambio. Mismo criterio que gen_boot_logo.py: wordmark propio en vez
del icono generico de conector de Rockbox, fondo fijo en negro (el
tema de Aura todavia no esta activo en este punto del arranque de la
pantalla USB -- ver D-051/D-055).

  design-system/.venv/bin/python3 design-system/scripts/gen_usb_logo.py
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
    / "usblogo.176x48x16.bmp"
)

WORDMARK = "USB"


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
    width, height = 176, 48
    bg = hex_to_rgb(tokens["color"]["dark"]["background"])
    fg = hex_to_rgb(tokens["color"]["dark"]["accent"])

    img = Image.new("RGB", (width, height), bg)
    draw = ImageDraw.Draw(img)

    size = 32
    margin = 24
    font = ImageFont.truetype(str(FONT_PATH), size)
    while size > 8:
        font = ImageFont.truetype(str(FONT_PATH), size)
        bbox = draw.textbbox((0, 0), WORDMARK, font=font)
        if (bbox[2] - bbox[0]) <= width - margin:
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
    print(f"==> Logo USB escrito en {OUT_PATH} ({size}px, {WORDMARK!r})")


if __name__ == "__main__":
    main()
