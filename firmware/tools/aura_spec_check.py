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

    python3 aura_spec_check.py statusbar <img.png> [--height N] [--tolerance N]
        D-352 (SS H del plan maestro): comprueba que TODO lo que vive en
        la barra de estado esta centrado verticalmente por su CAJA DE
        TINTA, con +-1 px. Agrupa la barra en columnas contiguas de tinta
        (cada glifo/icono/bateria queda en su propio grupo), calcula el
        centro vertical de cada uno y falla si alguno se aparta del
        centro de la barra mas de la tolerancia.

        Tres criterios que hacen que la medicion sea honesta, los tres
        aprendidos a golpes (moonlit D-068):

        1. "Tinta" NO es "pixel claro": es "pixel que se aparta del
           FONDO". El fondo se deduce de la propia captura (el color mas
           repetido dentro de la banda), asi que la misma herramienta
           sirve en tema claro y en tema oscuro. Un umbral absoluto de
           luminancia se invierte al cambiar de tema.
        2. Cada elemento se mide por SU caja de tinta, no por su celda:
           un simbolo de 16 px puede dibujar entre 8 y 15 px de tinta
           segun cual sea, y centrar la celda no centra lo que se ve.
        3. La tolerancia de 1 px NO es holgura para errores: el firmware
           centra el TITULO por la altura de MAYUSCULAS (constantes
           A26_FONT_CAP_*, medidas sobre el .fnt) mientras esta
           herramienta mide la tinta REAL de la cadena. Un titulo con
           acentos o con letras que bajan (p, g, j) tiene tinta por
           encima o por debajo de las mayusculas, asi que su centro medido
           cae medio pixel arriba o abajo del centro de las mayusculas.
           Eso es correcto y esperado.
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




# -- D-352: alineacion vertical de la barra de estado ---------------------

def _dominant_color(px, w, h):
    """Color mas repetido de la region: el fondo de la barra."""
    from collections import Counter
    c = Counter(px[x, y] for y in range(h) for x in range(w))
    return c.most_common(1)[0][0]


def _ink_columns(px, w, h, bg, tolerance):
    """-> por columna, (fila_min, fila_max) de tinta, o None."""
    out = []
    for x in range(w):
        rows = [y for y in range(h) if not close(px[x, y], bg, tolerance)]
        out.append((rows[0], rows[-1]) if rows else None)
    return out


def _group_columns(cols, gap=2):
    """Agrupa columnas con tinta separadas por menos de `gap` vacias.

    `gap` = 2 porque dentro de una palabra las letras se tocan o casi, y
    entre elementos de la barra (titulo | reloj | iconos | bateria) hay
    separaciones de varios pixeles. Un grupo = un elemento visual.
    """
    groups, cur = [], None
    empty = 0
    for x, c in enumerate(cols):
        if c is None:
            empty += 1
            if cur is not None and empty >= gap:
                groups.append(cur)
                cur = None
            continue
        empty = 0
        if cur is None:
            cur = [x, x, c[0], c[1]]
        else:
            cur[1] = x
            cur[2] = min(cur[2], c[0])
            cur[3] = max(cur[3], c[1])
    if cur is not None:
        groups.append(cur)
    return groups


def statusbar_check(img, bar_h=20, tolerance=24, max_delta=1.0, verbose=True):
    """True si cada elemento de la barra esta centrado +-max_delta px."""
    rgb = img.convert("RGB")
    w = rgb.size[0]
    px = rgb.load()
    bg = _dominant_color(px, w, bar_h)
    groups = _group_columns(_ink_columns(px, w, bar_h, bg, tolerance))
    center = (bar_h - 1) / 2.0
    worst, failed, measured = 0.0, [], 0

    if verbose:
        print(f"fondo de la barra: {bg}; centro: {center:.1f} px; "
              f"{len(groups)} elemento(s)")
    for x0, x1, y0, y1 in groups:
        if (y1 - y0 + 1) >= bar_h:
            # Ocupa la banda entera de arriba abajo: no es un elemento de
            # la barra sino el CONTENIDO que hay detras (en (split), la
            # imagen del panel derecho llega hasta el borde superior).
            # Medir su "centro" no dice nada -- por construccion cae en el
            # centro de la banda -- y contarlo como elemento centrado
            # falsearia el resultado.
            if verbose:
                print(f"  x {x0:3d}..{x1:3d}  (banda completa: contenido "
                      f"de fondo, no un elemento de la barra)")
            continue
        measured += 1
        c = (y0 + y1) / 2.0
        d = abs(c - center)
        worst = max(worst, d)
        mark = "" if d <= max_delta else "   <-- FUERA"
        if verbose:
            print(f"  x {x0:3d}..{x1:3d}  tinta y {y0:2d}..{y1:2d}  "
                  f"centro {c:4.1f}  delta {d:+.1f}{mark}")
        if d > max_delta:
            failed.append((x0, x1, d))

    if measured == 0:
        print("ERROR: no se encontro ningun elemento medible en la barra",
              file=sys.stderr)
        return False
    if failed:
        print(f"FALLA: {len(failed)} elemento(s) fuera de +-{max_delta} px "
              f"(peor {worst:.1f})")
        return False
    print(f"statusbar: OK -- {measured} elementos centrados "
          f"(peor desviacion {worst:.1f} px, tope {max_delta})")
    return True


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
    elif cmd == "statusbar":
        path = sys.argv[2]
        bar_h = 20
        tolerance = 24
        if "--height" in sys.argv:
            bar_h = int(sys.argv[sys.argv.index("--height") + 1])
        if "--tolerance" in sys.argv:
            tolerance = int(sys.argv[sys.argv.index("--tolerance") + 1])
        img = Image.open(path)
        sys.exit(0 if statusbar_check(img, bar_h, tolerance) else 1)
    elif cmd == "pixel":
        path, x, y = sys.argv[2:5]
        img = Image.open(path).convert("RGB")
        print(img.getpixel((int(x), int(y))))
    else:
        print(f"comando desconocido: {cmd}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
