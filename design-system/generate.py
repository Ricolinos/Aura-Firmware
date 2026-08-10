#!/usr/bin/env python3
"""Pipeline determinista del sistema de diseno Aura.

Lee tokens.json (unica fuente de verdad) y produce, a partir de los
assets de origen en vendor/ (Inter TTF, iconos Lucide SVG):

  - out/aura_tokens.h   Header C con colores, espaciados y tipografia,
                         para incluir desde firmware/rockbox/apps/aura/.
  - out/fonts/*.fnt      Fuentes bitmap Inter en el formato nativo de
                         Rockbox, rasterizadas a los tamanos exactos de
                         type_scale, via la herramienta convttf del fork.
  - out/icons/<theme>/*.bmp  Iconos Lucide rasterizados a los tamanos
                         exactos de uso, uno por tema (color de trazo
                         resuelto en tiempo de generacion, no en runtime;
                         ver DECISIONS.md).

No descarga nada de la red: eso es responsabilidad de scripts/fetch_assets.sh.
Es seguro volver a ejecutar este script cuantas veces haga falta; out/ se
regenera por completo cada vez.
"""
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TOKENS_PATH = ROOT / "tokens.json"
VENDOR = ROOT / "vendor"
OUT = ROOT / "out"
ROCKBOX_TOOLS = ROOT.parent / "firmware" / "rockbox" / "tools"
CONVTTF = ROCKBOX_TOOLS / "convttf"


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def ensure_convttf():
    if CONVTTF.exists():
        return
    print("==> Compilando convttf (herramienta de fuentes de Rockbox)...")
    result = subprocess.run(["make", "-C", str(ROCKBOX_TOOLS), "convttf"])
    if result.returncode != 0 or not CONVTTF.exists():
        die("no se pudo compilar tools/convttf (falta freetype2? -> brew install freetype)")


def hex_to_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i : i + 2], 16) for i in (0, 2, 4))


def generate_header(tokens):
    print("==> Generando out/aura_tokens.h")
    lines = []
    lines.append("/* Generado por design-system/generate.py a partir de tokens.json. */")
    lines.append("/* NO editar a mano: los cambios se perderian al regenerar. */")
    lines.append("#ifndef AURA_TOKENS_H")
    lines.append("#define AURA_TOKENS_H")
    lines.append("")
    lines.append('#include "lcd.h"')
    lines.append("")

    screen = tokens["screen"]
    lines.append(f"#define AURA_SCREEN_WIDTH  {screen['width']}")
    lines.append(f"#define AURA_SCREEN_HEIGHT {screen['height']}")
    lines.append("")

    lines.append("/* Espaciados (px) */")
    for name, value in tokens["spacing"].items():
        lines.append(f"#define AURA_SPACING_{name.upper()} {value}")
    lines.append("")

    lines.append("/* Escala tipografica (px) */")
    for name, value in tokens["type_scale"].items():
        lines.append(f"#define AURA_TYPE_{name.upper()} {value}")
    lines.append("")

    lines.append("/* Rutas de las fuentes bitmap generadas (relativas a FONT_DIR) */")
    for style_name, weight in tokens["font"]["styles_by_size"].items():
        size = tokens["type_scale"][style_name]
        fname = font_filename(style_name, size)
        lines.append(f'#define AURA_FONT_{style_name.upper()} "{fname}"')
    lines.append("")

    lines.append("/* Colores por tema, empaquetados con LCD_RGBPACK al formato nativo del LCD */")
    for theme_name, colors in tokens["color"].items():
        lines.append(f"/* Tema {theme_name} */")
        for token_name, hexval in colors.items():
            r, g, b = hex_to_rgb(hexval)
            define = f"AURA_COLOR_{theme_name.upper()}_{token_name.upper()}"
            lines.append(f"#define {define} LCD_RGBPACK({r}, {g}, {b})")
        lines.append("")

    lines.append("/* Tamanos de icono (px). Los bitmaps viven en ICON_DIR \"/aura/<tema>/\". */")
    for size_name, size_px in tokens["icon"]["sizes"].items():
        lines.append(f"#define AURA_ICON_SIZE_{size_name.upper()} {size_px}")
    lines.append("")
    lines.append("/* El enum de temas vive en aura_settings.h (es logica de app, no un")
    lines.append(" * token de diseno); este header solo expone los colores de cada uno. */")
    lines.append("")
    lines.append("#endif /* AURA_TOKENS_H */")
    lines.append("")

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "aura_tokens.h").write_text("\n".join(lines))


def font_filename(style_name, size_px):
    return f"aura-{style_name}-{size_px}.fnt"


def generate_fonts(tokens):
    print("==> Generando fuentes bitmap (out/fonts/*.fnt)")
    ensure_convttf()
    fonts_out = OUT / "fonts"
    fonts_out.mkdir(parents=True, exist_ok=True)

    weights = tokens["font"]["weights"]
    for style_name, weight in tokens["font"]["styles_by_size"].items():
        size = tokens["type_scale"][style_name]
        ttf_path = VENDOR / "inter-ttf" / weights[weight]
        if not ttf_path.exists():
            die(f"falta {ttf_path} -- ejecuta design-system/scripts/fetch_assets.sh")
        out_fnt = fonts_out / font_filename(style_name, size)
        cmd = [str(CONVTTF), "-p", str(size), "-o", str(out_fnt), str(ttf_path)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0 or not out_fnt.exists():
            die(f"convttf fallo para {style_name}@{size}px:\n{result.stdout}\n{result.stderr}")
        print(f"   {style_name} ({weight} @ {size}px) -> {out_fnt.name}")


def check_tool(name):
    if shutil.which(name) is None:
        die(f"falta la herramienta '{name}' en PATH (brew install librsvg)")


def generate_icons(tokens):
    print("==> Generando iconos (out/icons/<tema>/*.bmp)")
    check_tool("rsvg-convert")
    try:
        from PIL import Image
    except ImportError:
        die(
            "falta el modulo Pillow -- crea el venv del design system:\n"
            "  python3 -m venv design-system/.venv && "
            "design-system/.venv/bin/pip install pillow\n"
            "y ejecuta este script con design-system/.venv/bin/python3"
        )

    icon_cfg = tokens["icon"]
    svg_dir = VENDOR / "lucide-svg"

    # Magenta = TRANSPARENT_COLOR de Rockbox (firmware/export/lcd.h). Los
    # iconos se componen sobre este color marcador y se dibujan con
    # lcd_bitmap_transparent(), asi funcionan igual sobre fondo normal o
    # sobre la barra de seleccion con color de acento.
    #
    # rsvg-convert antialiasea los bordes del trazo: si se compone
    # directamente sobre el marcador magenta, esos pixeles de borde
    # quedan en un magenta "casi puro" pero no exacto, y
    # lcd_bitmap_transparent() (que compara color exacto) no los trata
    # como transparentes -- se ve un halo magenta alrededor de cada
    # icono. Por eso se renderiza con canal alfa real (fondo
    # transparente) y se umbraliza con Pillow: alfa >= 128 -> color de
    # trazo solido, si no -> magenta puro. Sin colores intermedios, sin
    # halo, independiente del algoritmo de antialiasing de rsvg-convert.
    ALPHA_THRESHOLD = 128
    TRANSPARENT_RGB = (255, 0, 255)

    for theme_name, colors in tokens["color"].items():
        theme_out = OUT / "icons" / theme_name
        theme_out.mkdir(parents=True, exist_ok=True)
        fg = colors["text_primary"]
        fg_rgb = hex_to_rgb(fg)

        for icon_name in icon_cfg["names"]:
            svg_path = svg_dir / f"{icon_name}.svg"
            if not svg_path.exists():
                die(f"falta {svg_path} -- ejecuta design-system/scripts/fetch_assets.sh")

            svg_colored = svg_path.read_text().replace(
                "<svg\n", f'<svg\n  color="{fg}"\n', 1
            )

            for size_name, size_px in icon_cfg["sizes"].items():
                tmp_svg = theme_out / f"_{icon_name}-{size_px}.tmp.svg"
                tmp_png = theme_out / f"_{icon_name}-{size_px}.tmp.png"
                out_bmp = theme_out / f"{icon_name}-{size_px}.bmp"

                tmp_svg.write_text(svg_colored)
                subprocess.run(
                    [
                        "rsvg-convert",
                        "-w", str(size_px),
                        "-h", str(size_px),
                        "-o", str(tmp_png),
                        str(tmp_svg),
                    ],
                    check=True,
                    capture_output=True,
                )

                src = Image.open(tmp_png).convert("RGBA")
                out_img = Image.new("RGB", src.size, TRANSPARENT_RGB)
                pixels_in = src.load()
                pixels_out = out_img.load()
                for y in range(src.height):
                    for x in range(src.width):
                        if pixels_in[x, y][3] >= ALPHA_THRESHOLD:
                            pixels_out[x, y] = fg_rgb
                out_img.save(out_bmp, format="BMP")

                tmp_svg.unlink()
                tmp_png.unlink()

        print(f"   tema {theme_name}: {len(icon_cfg['names'])} iconos x {len(icon_cfg['sizes'])} tamanos")


def main():
    tokens = json.loads(TOKENS_PATH.read_text())

    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    generate_header(tokens)
    generate_fonts(tokens)
    generate_icons(tokens)

    print("==> Pipeline completo. Salida en design-system/out/")


if __name__ == "__main__":
    main()
