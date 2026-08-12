#!/usr/bin/env python3
"""Verificador de medidas por pixel sobre capturas del simulador
(PLAN.md Seccion 4, punto 4). Convierte "medidas exactas en px segun
spec" de una inspeccion a ojo en un check ejecutable.

Uso como libreria (desde otro script) o linea de comandos para checks
puntuales:

    python3 aura_spec_check.py hline <img.png> <y> <x_start> <color_hex> [--tolerance N]
        Cuenta la corrida CONTIGUA de pixeles ~= color_hex en la fila y,
        empezando en x_start hacia la derecha. Imprime la longitud.

    python3 aura_spec_check.py pixel <img.png> <x> <y>
        Imprime el color RGB del pixel (x,y).
"""
import sys
from pathlib import Path


def hex_to_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def close(a, b, tol):
    return all(abs(a[i] - b[i]) <= tol for i in range(3))


def run_length(img, x_start, y, color, tolerance=10, max_x=None):
    """Cuenta cuantos pixeles CONTIGUOS desde (x_start, y) hacia la
    derecha son ~= color (dentro de `tolerance` por canal). Se detiene
    en el primer pixel que no matchea, o en max_x/borde de imagen."""
    w, h = img.size
    if max_x is None:
        max_x = w
    px = img.convert("RGB").load()
    n = 0
    x = x_start
    while x < min(max_x, w) and close(px[x, y], color, tolerance):
        n += 1
        x += 1
    return n


def find_run_start(img, y, color, tolerance=10, x_from=0, x_to=None):
    """Primer x (desde x_from) donde empieza una corrida de `color`."""
    w, h = img.size
    if x_to is None:
        x_to = w
    px = img.convert("RGB").load()
    for x in range(x_from, min(x_to, w)):
        if close(px[x, y], color, tolerance):
            return x
    return None


def vertical_run_length(img, x, y_start, color, tolerance=10, max_y=None):
    w, h = img.size
    if max_y is None:
        max_y = h
    px = img.convert("RGB").load()
    n = 0
    y = y_start
    while y < min(max_y, h) and close(px[x, y], color, tolerance):
        n += 1
        y += 1
    return n


def main():
    from PIL import Image

    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1]
    if cmd == "hline":
        path, y, x_start, color_hex = sys.argv[2:6]
        tolerance = 10
        if "--tolerance" in sys.argv:
            tolerance = int(sys.argv[sys.argv.index("--tolerance") + 1])
        img = Image.open(path)
        n = run_length(img, int(x_start), int(y), hex_to_rgb(color_hex), tolerance)
        print(n)
    elif cmd == "pixel":
        path, x, y = sys.argv[2:5]
        img = Image.open(path).convert("RGB")
        print(img.getpixel((int(x), int(y))))
    else:
        print(f"comando desconocido: {cmd}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
