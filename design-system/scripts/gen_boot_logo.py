#!/usr/bin/env python3
"""Genera los bitmaps de la marca de arranque de Aura.

Dos salidas, del MISMO dibujo:

1. `apps/bitmaps/native/rockboxlogo.320x98x16.bmp` -- el lienzo completo
   de 320x98 que dibuja el FIRMWARE en `show_logo_boot()` (apps/main.c).
   Sustituye directamente al bitmap del fork, mismo nombre y dimensiones,
   asi que la regla de `bitmaps.make` (bmp2rb) que lo compila a
   `bm_rockboxlogo` no necesita ningun cambio. El fondo va fijo en negro
   porque este bitmap se pinta ANTES de que el tema de Aura este cargado
   (D-016: `lcd_set_backdrop(NULL)` corre dentro de `aura_theme_init()`,
   mas tarde) -- ver D-051.

2. `apps/bitmaps/native/bootwordmark.<w>x<h>x16.bmp` (bandera
   `--bootloader-crop`, D-347) -- el RECORTE ajustado del wordmark, para
   que el BOOTLOADER pinte la misma marca sin cargar el lienzo entero.
   Solo lo compila el build del bootloader (`apps/bitmaps/native/SOURCES`,
   bajo `#if defined(BOOTLOADER) && defined(IPOD_6G)`).

## Por que el recorte cae exactamente donde el firmware pinta la marca

Es la propiedad que hace que la transicion bootloader -> firmware no
tenga salto, y no depende de ninguna constante copiada a mano:

- Este script centra la tinta del wordmark en los dos ejes del lienzo de
  320x98 (`x`/`y` se calculan restando el bbox, asi que lo centrado es la
  CAJA DE TINTA, no la caja de la fuente).
- `show_logo_boot()` centra ese lienzo en los dos ejes de la pantalla de
  320x240. Por lo tanto el centro de la tinta cae en el centro de la
  pantalla, (160, 120).
- El recorte es el bbox de la tinta con un margen de 4 px, ensanchado 1 px
  de un lado cuando hace falta: el bootloader centra con una division
  ENTERA, y esa es la unica forma de que caiga en el pixel exacto sin
  pasarle al C un offset que se pueda desincronizar. Lo que cambia es
  cuanto fondo negro lleva de un lado; la marca queda donde debe.

El script COMPRUEBA esa igualdad pixel a pixel antes de escribir nada, asi
que si alguien cambia el lienzo o el centrado, falla aqui en vez de
producir una marca que salta al arrancar.

3. `docs/screenshots/ronda-estabilidad/bootloader-maqueta.png` -- maqueta
   de 320x240 con la pantalla de arranque completa (marca + las dos
   leyendas), para aprobarla antes de flashear: el simulador NO ejecuta el
   bootloader. Las leyendas se dibujan con los glifos REALES de
   `FONT_SYSFIXED` (6x8), leidos del mismo BDF del que Rockbox genera su
   `sysfont.c` -- no una fuente monoespaciada parecida.

No es parte del pipeline normal de `generate.py` (ese produce `out/`, que
no se versiona; estos bitmaps SI se versionan, como el resto de
`apps/bitmaps/native/`) -- se ejecuta a mano, una vez, cuando el diseno
cambia:

  design-system/.venv/bin/python3 design-system/scripts/gen_boot_logo.py
  design-system/.venv/bin/python3 design-system/scripts/gen_boot_logo.py --bootloader-crop
"""
import argparse
import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parent
TOKENS_PATH = ROOT / "tokens.json"
FONT_PATH = ROOT / "vendor" / "inter-ttf" / "Inter-SemiBold.ttf"
BITMAPS_DIR = REPO / "firmware" / "rockbox" / "apps" / "bitmaps" / "native"
OUT_PATH = BITMAPS_DIR / "rockboxlogo.320x98x16.bmp"
SYSFONT_BDF = REPO / "firmware" / "rockbox" / "fonts" / "08-Schumacher-Clean.bdf"
MOCKUP_PATH = (REPO / "docs" / "screenshots" / "ronda-estabilidad"
               / "bootloader-maqueta.png")

WORDMARK = "aura"

LOGO_W, LOGO_H = 320, 98
SCREEN_W, SCREEN_H = 320, 240

# Fondo del arranque. Negro literal, no un token: esta marca se pinta
# antes de que exista tema (D-051), y el bootloader ya limpia la pantalla
# con LCD_BLACK (`bootloader/ipod-s5l87xx.c`). tokens.json no tiene un rol
# para "el negro de antes del tema" -- `dark.shell_bg` es #1C1C1E, que no
# es esto.
BG = (0, 0, 0)

# Margen del recorte del bootloader alrededor de la caja de tinta (B.2).
CROP_PAD = 4

# Gris de las leyendas del bootloader. Excepcion documentada (SS B.2 del
# plan maestro): el bootloader no puede incluir la paleta de Aura, asi
# que este valor viaja como literal RGB en el C. Se toma de
# `aura_ds.color.category.settings_gray_hex` para que la maqueta y el
# firmware no puedan divergir.
LEGEND_TOKEN = ("aura_ds", "color", "category", "settings_gray_hex")

LEGEND_BOTTOM_MARGIN = 14   # SS B.2: ultima linea a 14 px del borde
LEGEND_LINE_SPACING = 12    # SS B.2: interlineado 12


def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def hex_to_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def token(tokens, path):
    node = tokens
    for key in path:
        node = node[key]
    return node


# -- El lienzo de 320x98 --------------------------------------------------

def render_logo(tokens):
    """-> (imagen 320x98, bbox de tinta, tamano de fuente usado)."""
    fg = hex_to_rgb(tokens["color"]["dark"]["text_primary"])
    img = Image.new("RGB", (LOGO_W, LOGO_H), BG)
    draw = ImageDraw.Draw(img)

    # Busca el tamano de fuente mas grande que entre en el ancho
    # disponible con margen, partiendo de un tamano generoso.
    size = 64
    font = ImageFont.truetype(str(FONT_PATH), size)
    margin = 32
    while size > 8:
        font = ImageFont.truetype(str(FONT_PATH), size)
        bbox = draw.textbbox((0, 0), WORDMARK, font=font)
        if bbox[2] - bbox[0] <= LOGO_W - margin:
            break
        size -= 1

    bbox = draw.textbbox((0, 0), WORDMARK, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    x = (LOGO_W - text_w) // 2 - bbox[0]
    y = (LOGO_H - text_h) // 2 - bbox[1]
    draw.text((x, y), WORDMARK, font=font, fill=fg)

    return img, ink_bbox(img), size


def ink_bbox(img):
    """Caja de la tinta real (lo que no es fondo), no la caja de la fuente."""
    bg_img = Image.new("RGB", img.size, BG)
    from PIL import ImageChops
    box = ImageChops.difference(img, bg_img).getbbox()
    if box is None:
        die("el lienzo salio vacio: no hay tinta que recortar")
    return box


# -- El recorte del bootloader -------------------------------------------

def crop_for_bootloader(img, box):
    """Recorte con margen simetrico, corregido para caer donde el firmware.

    Devuelve (imagen recortada, (x, y) donde la pinta el bootloader).
    """
    # Donde cae la tinta en la pantalla cuando la pinta el FIRMWARE:
    # show_logo_boot() centra el lienzo de 320x98 en 320x240.
    logo_x = (SCREEN_W - LOGO_W) // 2
    logo_y = (SCREEN_H - LOGO_H) // 2
    ink_screen = (logo_x + box[0], logo_y + box[1],
                  logo_x + box[2], logo_y + box[3])

    left, top, right, bottom = box
    pad_l = pad_t = CROP_PAD

    def solve_pad(ink_len, target, screen_len, pad_near, canvas_room):
        """Margen lejano que hace que centrar el recorte de exactamente.

        El bootloader pinta en (screen_len - crop_len) // 2, una division
        ENTERA: con el margen simetrico de 4 px la marca puede quedar
        corrida un pixel respecto de donde la pinta el firmware. En vez de
        pasarle un offset al C (una constante mas que se puede desincronizar),
        se ensancha el margen LEJANO en 0 o 1 px hasta que el centrado
        entero da el pixel exacto. La marca sigue centrada; lo que cambia
        es cuanto fondo negro lleva de un lado.
        """
        want = target - pad_near          # (screen_len - crop_len) // 2 buscado
        for crop_len in (screen_len - 2 * want - 1, screen_len - 2 * want):
            pad_far = crop_len - ink_len - pad_near
            if pad_far >= CROP_PAD and pad_far <= canvas_room:
                if (screen_len - crop_len) // 2 + pad_near == target:
                    return pad_far
        die("no hay margen que haga coincidir el recorte con la marca del "
            f"firmware (tinta {ink_len}, objetivo {target})")

    pad_r = solve_pad(right - left, ink_screen[0], SCREEN_W, pad_l,
                      LOGO_W - right)
    pad_b = solve_pad(bottom - top, ink_screen[1], SCREEN_H, pad_t,
                      LOGO_H - bottom)

    crop_box = (max(0, left - pad_l), max(0, top - pad_t),
                min(LOGO_W, right + pad_r), min(LOGO_H, bottom + pad_b))
    crop = img.crop(crop_box)
    cw, ch = crop.size

    # Donde cae la tinta cuando la pinta el BOOTLOADER, centrando el recorte.
    draw_x = (SCREEN_W - cw) // 2
    draw_y = (SCREEN_H - ch) // 2
    ink_boot = (draw_x + (left - crop_box[0]), draw_y + (top - crop_box[1]))

    if ink_boot != (ink_screen[0], ink_screen[1]):
        die("el recorte no cae donde el firmware pinta la marca: "
            f"bootloader {ink_boot} vs firmware {ink_screen[:2]}. "
            "Cambio el centrado de show_logo_boot() o el lienzo; "
            "revisa la explicacion al principio de este archivo.")

    return crop, (draw_x, draw_y)


# -- sysfont (6x8) para la maqueta ---------------------------------------

def load_bdf(path):
    """-> {codepoint: [filas de bits]} del BDF de FONT_SYSFIXED."""
    glyphs = {}
    code = None
    bbx = None
    rows = None
    for raw in path.read_text(encoding="latin-1").splitlines():
        line = raw.strip()
        if line.startswith("ENCODING "):
            code = int(line.split()[1])
        elif line.startswith("BBX "):
            bbx = [int(v) for v in line.split()[1:]]
        elif line == "BITMAP":
            rows = []
        elif line == "ENDCHAR":
            if code is not None and code >= 0 and bbx and rows is not None:
                glyphs[code] = (bbx, rows)
            code, bbx, rows = None, None, None
        elif rows is not None and line:
            rows.append(line)
    if not glyphs:
        die(f"no se pudo leer ningun glifo de {path}")
    return glyphs


def draw_sysfont_text(img, glyphs, x, y, text, color):
    """Dibuja `text` con los glifos reales de sysfont. y = borde superior."""
    ascent = 7  # sysfont.c: ascent 7, height 8, FONTBOUNDINGBOX 6 8 0 -1
    px = img.load()
    cx = x
    for ch in text:
        code = ord(ch)
        entry = glyphs.get(code) or glyphs.get(ord("?"))
        (gw, gh, gx, gy), rows = entry
        for ry, bits in enumerate(rows):
            value = int(bits, 16)
            width_bits = len(bits) * 4
            for rx in range(gw):
                if value & (1 << (width_bits - 1 - rx)):
                    ax = cx + gx + rx
                    ay = y + ascent - (gy + gh) + ry
                    if 0 <= ax < img.width and 0 <= ay < img.height:
                        px[ax, ay] = color
        cx += 6  # monoespaciada: 6 px de avance, como FONT_SYSFIXED
    return cx


def sysfont_width(text):
    return 6 * len(text)


def render_mockup(crop, crop_pos, legends, legend_rgb):
    img = Image.new("RGB", (SCREEN_W, SCREEN_H), BG)
    img.paste(crop, crop_pos)
    glyphs = load_bdf(SYSFONT_BDF)

    # SS B.2: la ULTIMA linea a 14 px del borde inferior, interlineado 12.
    n = len(legends)
    first_top = SCREEN_H - LEGEND_BOTTOM_MARGIN - 8 - (n - 1) * LEGEND_LINE_SPACING
    for i, text in enumerate(legends):
        x = (SCREEN_W - sysfont_width(text)) // 2
        draw_sysfont_text(img, glyphs, x, first_top + i * LEGEND_LINE_SPACING,
                          text, legend_rgb)
    return img


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bootloader-crop", action="store_true",
                    help="ademas del lienzo, escribe el recorte del "
                         "bootloader y la maqueta de aprobacion")
    ap.add_argument("--version-string", default="fdf5be4e8f-260903",
                    help="rbversion que muestra la maqueta (solo cosmetico: "
                         "el bootloader real usa el suyo)")
    ap.add_argument("--check", action="store_true",
                    help="no escribe nada; compara con lo que ya esta en el "
                         "arbol y falla si difiere")
    args = ap.parse_args()

    if not FONT_PATH.exists():
        die(f"falta {FONT_PATH} -- ejecuta design-system/scripts/fetch_assets.sh")

    tokens = json.loads(TOKENS_PATH.read_text())
    img, box, size = render_logo(tokens)
    print(f"==> Wordmark {WORDMARK!r} a {size}px; caja de tinta {box} "
          f"en el lienzo de {LOGO_W}x{LOGO_H}")

    if args.check:
        if not OUT_PATH.exists():
            die(f"no existe {OUT_PATH}")
        if Image.open(OUT_PATH).convert("RGB").tobytes() != img.tobytes():
            die(f"{OUT_PATH.name} en el arbol NO coincide con lo que genera "
                "este script (distinta version de Pillow o de la fuente)")
        print(f"==> {OUT_PATH.name}: identico al del arbol")

    if not args.check:
        OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
        img.save(OUT_PATH, format="BMP")
        print(f"==> Lienzo del firmware escrito en {OUT_PATH}")

    if not args.bootloader_crop:
        return

    crop, pos = crop_for_bootloader(img, box)
    cw, ch = crop.size
    crop_path = BITMAPS_DIR / f"bootwordmark.{cw}x{ch}x16.bmp"
    print(f"==> Recorte {cw}x{ch}; el bootloader lo centra en {pos}, "
          f"la tinta cae donde show_logo_boot() la pinta")

    legend_rgb = hex_to_rgb(token(tokens, LEGEND_TOKEN))
    legends = [
        f"aura · arranque {args.version_string}",
        "Basado en Rockbox · GPL v2 · rockbox.org",
    ]
    mockup = render_mockup(crop, pos, legends, legend_rgb)

    if args.check:
        if not crop_path.exists():
            die(f"no existe {crop_path}")
        if Image.open(crop_path).convert("RGB").tobytes() != crop.tobytes():
            die(f"{crop_path.name} en el arbol NO coincide con lo generado")
        print(f"==> {crop_path.name}: identico al del arbol")
        return

    # Un recorte viejo con otras dimensiones dejaria dos entradas en
    # SOURCES apuntando a bitmaps distintos: se limpia.
    for old in BITMAPS_DIR.glob("bootwordmark.*x*x16.bmp"):
        if old != crop_path:
            old.unlink()
            print(f"==> Recorte anterior borrado: {old.name}")
    crop.save(crop_path, format="BMP")
    print(f"==> Recorte del bootloader escrito en {crop_path}")

    MOCKUP_PATH.parent.mkdir(parents=True, exist_ok=True)
    mockup.save(MOCKUP_PATH, format="PNG")
    print(f"==> Maqueta de aprobacion escrita en {MOCKUP_PATH}")
    print(f"    leyendas en {token(tokens, LEGEND_TOKEN)} "
          f"({'.'.join(LEGEND_TOKEN)})")


if __name__ == "__main__":
    main()
