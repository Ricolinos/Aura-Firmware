#!/usr/bin/env python3
"""Pipeline determinista del sistema de diseno Apple2026.

Lee tokens.json (unica fuente de verdad) y produce:

  - out/apple2026_tokens.h  Header C con colores, espaciados y tipografia
                         (macros A26_*), para incluir desde
                         firmware/rockbox/apps/aura/apple2026_shell.h.
  - out/fonts/*.fnt      Fuentes bitmap SF Compact/SF Pro en el formato
                         nativo de Rockbox, rasterizadas a los tamanos
                         exactos de type_scale, via convttf del fork.
  - out/icons/<theme>/*.bmp  SF Symbols en variante lineal, rasterizados
                         a los tamanos exactos de uso, por tema y por
                         variante (color resuelto en tiempo de
                         generacion, no en runtime; ver D-010).

Origen de los assets de Apple (SF Compact/SF Pro y SF Symbols): NO se
versionan en este repo. La fuente se lee de la instalacion local del
sistema y los simbolos se renderizan pidiendoselos a macOS via AppKit
(scripts/apple2026_sf_render.swift). Es decir: este pipeline solo corre
en un Mac con esas fuentes instaladas -- una restriccion aceptable
porque Aura Studio (el otro entregable del proyecto) ya es una app de
macOS.

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
    print("==> Generando out/apple2026_tokens.h")
    lines = []
    lines.append("/* Generado por design-system/generate.py a partir de tokens.json. */")
    lines.append("/* NO editar a mano: los cambios se perderian al regenerar. */")
    lines.append("#ifndef APPLE2026_TOKENS_H")
    lines.append("#define APPLE2026_TOKENS_H")
    lines.append("")
    lines.append('#include "lcd.h"')
    lines.append("")

    screen = tokens["screen"]
    lines.append(f"#define A26_SCREEN_WIDTH  {screen['width']}")
    lines.append(f"#define A26_SCREEN_HEIGHT {screen['height']}")
    lines.append("")

    layout = tokens["layout"]
    lines.append("/* Retroalimentacion de layout global (px) */")
    for name, value in layout.items():
        lines.append(f"#define A26_LAYOUT_{name.upper()} {value}")
    lines.append("")

    lines.append("/* Espaciados (px) */")
    for name, value in tokens["spacing"].items():
        lines.append(f"#define A26_SPACING_{name.upper()} {value}")
    lines.append("")

    lines.append("/* Escala tipografica (px) */")
    for name, value in tokens["type_scale"].items():
        lines.append(f"#define A26_TYPE_{name.upper()} {value}")
    lines.append("")

    lines.append("/* Rutas de las fuentes bitmap generadas (relativas a FONT_DIR) */")
    for style_name, weight in tokens["font"]["styles_by_size"].items():
        size = tokens["type_scale"][style_name]
        fname = font_filename(style_name, size)
        lines.append(f'#define A26_FONT_{style_name.upper()} "{fname}"')
    lines.append("")

    lines.append("/* Mismo nombre sin extension: lo que espera global_settings.font_file")
    lines.append(" * (Fase 14, PLAN-UX.md) -- settings.c le agrega \".fnt\" el solo. */")
    for style_name, weight in tokens["font"]["styles_by_size"].items():
        size = tokens["type_scale"][style_name]
        base = font_filename(style_name, size).removesuffix(".fnt")
        lines.append(f'#define A26_FONT_BASENAME_{style_name.upper()} "{base}"')
    lines.append("")

    lines.append("/* Colores por tema, empaquetados con LCD_RGBPACK al formato nativo del LCD */")
    for theme_name, colors in tokens["color"].items():
        lines.append(f"/* Tema {theme_name} */")
        for token_name, hexval in colors.items():
            r, g, b = hex_to_rgb(hexval)
            define = f"A26_COLOR_{theme_name.upper()}_{token_name.upper()}"
            lines.append(f"#define {define} LCD_RGBPACK({r}, {g}, {b})")
        lines.append("")

    lines.append("/* Tamanos de icono (px). Los bitmaps viven en ICON_DIR \"/apple2026/<tema>/\". */")
    for size_name, size_px in tokens["icon"]["sizes"].items():
        lines.append(f"#define A26_ICON_SIZE_{size_name.upper()} {size_px}")
    lines.append("")
    lines.append("/* El enum de temas vive en aura_settings.h (es logica de app, no un")
    lines.append(" * token de diseno); este header solo expone los colores de cada uno. */")
    lines.append("")

    lines.extend(generate_aura_ds_defines(tokens))

    lines.append("#endif /* APPLE2026_TOKENS_H */")
    lines.append("")

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "apple2026_tokens.h").write_text("\n".join(lines))


def generate_aura_ds_defines(tokens):
    """Aplana tokens['aura_ds'] (PLAN.md T0.1) a defines AURA_DS_*.

    Recorre el arbol recursivamente: dict -> prefijo compuesto, numero ->
    #define directo, string "#RRGGBB" -> LCD_RGBPACK (con sufijo _HEX
    quitado del nombre del define, ya no es un string), lista -> par
    _COUNT/_VALUES (para arrays como cover_flow angulos). Claves que
    empiezan con "comment" se saltan (son notas para humanos, no tokens).
    Strings que no son color tambien se saltan -- 'type_scale_roles' vive
    solo como mapa rol->estilo de fuente, consumido directo desde Python
    mas abajo (font_filename ya genera A26_FONT_DS_* para cada estilo),
    no hace falta duplicarlo como define C.
    """
    ds = tokens.get("aura_ds")
    if not ds:
        return []

    lines = ["/* Tokens del sistema de diseno nuevo (docs/aura-design-system/,",
             " * PLAN.md T0.1) -- en paralelo a A26_* mientras dura la migracion. */"]

    def walk(prefix, node):
        if isinstance(node, dict):
            for key, value in node.items():
                if key.startswith("comment"):
                    continue
                walk(f"{prefix}_{key.upper()}", value)
        elif isinstance(node, bool):
            return
        elif isinstance(node, (int, float)):
            lines.append(f"#define {prefix} {node}")
        elif isinstance(node, str):
            if node.startswith("#") and len(node) == 7:
                r, g, b = hex_to_rgb(node)
                name = prefix.removesuffix("_HEX")
                lines.append(f"#define {name} LCD_RGBPACK({r}, {g}, {b})")
                # Ademas del empaquetado nativo del LCD (para dibujar),
                # el mismo valor en 0xRRGGBB de 24 bits -- necesario para
                # persistir/comparar como entero plano (p. ej. el default
                # de aura_settings.accent_rgb24, T0.3) sin depender del
                # formato de pixel del target.
                rgb24 = (r << 16) | (g << 8) | b
                lines.append(f"#define {name}_RGB24 0x{rgb24:06X}")
            # otros strings (nombres de estilo de fuente, etc.) no generan
            # define C -- se resuelven en Python o se hardcodean en C.
        elif isinstance(node, list):
            lines.append(f"#define {prefix}_COUNT {len(node)}")
            is_hex_array = bool(node) and all(
                isinstance(v, str) and v.startswith("#") and len(v) == 7 for v in node
            )
            if is_hex_array:
                # Arreglo de colores (p. ej. presets de acento, T4.1) --
                # mismo tratamiento que un color hex suelto (arriba),
                # elemento por elemento: empaquetado nativo del LCD para
                # dibujar + el 0xRRGGBB de 24 bits para persistir/comparar.
                rgbs = [hex_to_rgb(v) for v in node]
                packed = ", ".join(f"LCD_RGBPACK({r}, {g}, {b})" for r, g, b in rgbs)
                lines.append(f"#define {prefix}_VALUES {{ {packed} }}")
                rgb24s = ", ".join(f"0x{((r << 16) | (g << 8) | b):06X}" for r, g, b in rgbs)
                lines.append(f"#define {prefix}_RGB24_VALUES {{ {rgb24s} }}")
            else:
                values = ", ".join(str(v) for v in node)
                lines.append(f"#define {prefix}_VALUES {{ {values} }}")

    walk("AURA_DS", ds)
    lines.append("")
    return lines


STUDIO_PROJECT = ROOT.parent / "studio" / "AuraStudio"
STUDIO_GENERATED = STUDIO_PROJECT / "Sources" / "AuraStudio" / "Generated"


def generate_swift_palette(tokens):
    """Emite la misma paleta para Aura Studio.

    El firmware y la app de escritorio comparten identidad visual, asi que
    tienen que compartir tambien la fuente de los colores: el header C y
    este archivo Swift salen del mismo tokens.json. Se escribe directo en
    el arbol de fuentes de Studio (no en out/) porque Studio se compila
    con SwiftPM/Xcode, que no tienen un paso de instalacion de assets
    como build_sim.sh.

    D-286 (separacion de repositorios, 2026-08-16): Aura Studio vive
    ahora en un repositorio aparte, ya no como `studio/` hermano de
    este. Si ese arbol no esta presente (el caso normal en un checkout
    de este repo, que es solo firmware), no hay a donde escribir esto
    -- se omite en vez de crear un `studio/` huerfano sin dueno. Si
    alguna vez se necesita este archivo desde el otro repositorio, hay
    que copiarlo/publicarlo explicitamente, no asumir un arbol hermano.
    """
    if not STUDIO_PROJECT.exists():
        print("==> Aura Studio no esta presente como hermano de este repo -- se omite AuraPalette.swift")
        return
    print("==> Generando la paleta de Aura Studio (Generated/AuraPalette.swift)")
    lines = [
        "// Generado por design-system/generate.py a partir de tokens.json.",
        "// NO editar a mano: los cambios se perderian al regenerar.",
        "",
        "import SwiftUI",
        "",
        "/// Paleta compartida con el firmware -- misma fuente de verdad",
        "/// (design-system/tokens.json) que apple2026_tokens.h. Los nombres de",
        "/// los campos son los mismos tokens del sistema de diseno Apple2026",
        "/// (docs/design/Reglas de diseno Apple2026 (v2).md); \"Aura\" en el",
        "/// nombre del tipo es el producto, no el sistema de diseno.",
        "struct AuraColors {",
    ]

    token_names = list(next(iter(tokens["color"].values())).keys())
    for name in token_names:
        lines.append(f"    let {to_camel(name)}: Color")
    lines.append("}")
    lines.append("")
    lines.append("extension AuraColors {")
    for theme_name, colors in tokens["color"].items():
        lines.append(f"    static let {theme_name} = AuraColors(")
        parts = []
        for name in token_names:
            r, g, b = hex_to_rgb(colors[name])
            parts.append(
                f"        {to_camel(name)}: Color(red: {r / 255:.4f}, "
                f"green: {g / 255:.4f}, blue: {b / 255:.4f})"
            )
        lines.append(",\n".join(parts))
        lines.append("    )")
    lines.append("}")
    lines.append("")

    STUDIO_GENERATED.mkdir(parents=True, exist_ok=True)
    (STUDIO_GENERATED / "AuraPalette.swift").write_text("\n".join(lines))


def to_camel(snake):
    head, *rest = snake.split("_")
    return head + "".join(part.capitalize() for part in rest)


def font_filename(style_name, size_px):
    return f"a26-{style_name}-{size_px}.fnt"


def resolve_font_file(tokens, filename):
    """Busca una cara de SF Pro en la instalacion local del sistema.

    No se versiona en el repo (licencia de Apple), asi que la ausencia se
    reporta con la instruccion exacta para resolverla en vez de un
    'archivo no encontrado' pelado.
    """
    for base in tokens["font"]["search_paths"]:
        base_path = Path(base).expanduser()
        # D-286: rutas relativas (ej. "vendor/inter-ttf") se resuelven contra
        # ROOT (design-system/), no contra el cwd del proceso que invoque
        # generate.py -- las absolutas (/Library/Fonts...) y "~" no cambian.
        if not base_path.is_absolute():
            base_path = ROOT / base_path
        candidate = base_path / filename
        if candidate.exists():
            return candidate
    die(
        f"no se encontro {filename} en {tokens['font']['search_paths']}.\n"
        "Si es una cara de Inter, verifica design-system/vendor/inter-ttf/.\n"
        "Si es una cara de Apple (tema opcional, no el default de este repo):\n"
        "descargala de https://developer.apple.com/fonts/ e instalala."
    )


def generate_fonts(tokens):
    print("==> Generando fuentes bitmap (out/fonts/*.fnt)")
    ensure_convttf()
    fonts_out = OUT / "fonts"
    fonts_out.mkdir(parents=True, exist_ok=True)

    faces = tokens["font"]["faces"]
    for style_name, face in tokens["font"]["styles_by_size"].items():
        size = tokens["type_scale"][style_name]
        ttf_path = resolve_font_file(tokens, faces[face])
        out_fnt = fonts_out / font_filename(style_name, size)
        cmd = [str(CONVTTF), "-p", str(size), "-o", str(out_fnt), str(ttf_path)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0 or not out_fnt.exists():
            die(f"convttf fallo para {style_name}@{size}px:\n{result.stdout}\n{result.stderr}")
        print(f"   {style_name} ({face} @ {size}px) -> {out_fnt.name}")


"""Supersampleo + composicion por cobertura (AUDITORIA-01 A-01, corrige
D-075: el supersampleo de 8x mejoraba DONDE caia el borde, pero el paso
siguiente seguia umbralizando el alfa a un solo bit -- 234 iconos por
tema, cada uno con exactamente 1 tono de tinta, verificado por conteo
real de colores, no a ojo). `lcd_bitmap_transparent()` solo admite
transparencia binaria por clave de color exacta (D-010): un pixel es
opaco o pura clave magenta, sin mezcla parcial en tiempo de DIBUJO. Eso
no obliga a que el ASSET mismo sea binario -- se puede pre-componer la
tinta contra el fondo conocido de cada tema en tiempo de GENERACION, y
dejar la clave magenta solo para los pixeles de cobertura exactamente
cero. El resultado sigue siendo un bitmap opaco de un solo plano (ningun
cambio en el codigo C ni en lcd_bitmap_transparent), pero el borde ya no
es una escalera: es una rampa real de tonos intermedios entre la tinta y
el fondo, igual que produciria un antialias autentico.

Pedirle a AppKit el simbolo a 16x el tamano final y reducirlo con un
filtro de caja (Pillow BOX, promedio simple de cobertura de subpixeles
-- sin el ringing que LANCZOS puede introducir en curvas muy pequenas)
da un canal alfa de cobertura fiel a la forma real del glifo."""
SUPERSAMPLE = 16


def icon_canvas_dims(icon_cfg, icon_key, size_name, size_px):
    """Ancho/alto final (sin supersampleo) del lienzo de un icono.
    Cuadrado (size_px, size_px) por defecto -- 'battery_icon' (tokens.json)
    es la unica excepcion hoy: alto fijo = size_px (igual al resto de los
    iconos de esa categoria), ancho mayor, para que un simbolo de aspecto
    natural muy distinto de 1:1 (bateria, ~2:1 ancho:alto) no se aplaste
    verticalmente al forzarlo en un cuadrado -- 'contain' (render_symbol_shapes)
    queda limitado por el ANCHO en un cuadrado y el alto real encoge con el
    (ver comment de 'battery_icon' en tokens.json)."""
    bat = icon_cfg.get("battery_icon")
    if bat and icon_key in bat["icons"] and size_name == bat["size"]:
        return bat["width"], size_px
    return size_px, size_px


def render_symbol_shapes(tokens, shapes_dir):
    """Renderiza cada (simbolo, tamano) una sola vez a SUPERSAMPLE x el
    tamano final, negro sobre alfa 0 -- generate_icons() reduce con
    filtro de calidad antes de umbralizar (ver nota de modulo).

    La forma no depende del tema ni de la variante -- solo el color, que
    se aplica despues con Pillow. Un unico proceso de Swift para todo el
    lote: `swift archivo.swift` recompila el script en cada invocacion
    (~2s), asi que 57 invocaciones costarian minutos.
    """
    icon_cfg = tokens["icon"]
    shapes_dir.mkdir(parents=True, exist_ok=True)

    # Bocina dinamica (encargo 2026-08-12): sus 5 estados se renderizan
    # a pointSize FIJO (el de un icono de body_ref px) en el lienzo de
    # su tamano dedicado, sin contain -- el cuerpo de la bocina queda
    # identico entre variantes (ver comment en tokens.json).
    dyn = icon_cfg.get("dynamic_speaker")

    # D-263: icon_key con SVG propio (rutas relativas a design-system/,
    # resueltas aqui a absolutas -- el renderizador Swift recibe rutas
    # ya resueltas, no le corresponde a el buscar la raiz del repo).
    svg_overrides = {
        k: str((ROOT / v).resolve())
        for k, v in icon_cfg.get("svg_overrides", {}).items()
    }

    jobs = []
    for icon_key, symbol_name in icon_cfg["names"].items():
        svg_path = svg_overrides.get(icon_key)
        for size_name, size_px in icon_cfg["sizes"].items():
            w_px, h_px = icon_canvas_dims(icon_cfg, icon_key, size_name, size_px)
            job = {
                "px": w_px * SUPERSAMPLE,
                "py": h_px * SUPERSAMPLE,
                "out": str(shapes_dir / f"{icon_key}-{size_px}.png"),
            }
            if svg_path:
                job["svgPath"] = svg_path
            else:
                job["symbol"] = symbol_name
                job["weight"] = icon_cfg["weight_by_size"][size_name]
            if (dyn and icon_key in dyn["icons"]
                    and size_name == dyn["size"]):
                ref_px = icon_cfg["sizes"][dyn["body_ref"]]
                # mismo factor 0.82 que usa el renderizador en su via
                # normal para relacionar lienzo con pointSize
                job["pt"] = ref_px * SUPERSAMPLE * 0.82
            jobs.append(job)

    result = subprocess.run(
        ["swift", str(ROOT / "scripts" / "apple2026_sf_render.swift")],
        input=json.dumps(jobs),
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        die(f"el renderizador de SF Symbols fallo:\n{result.stdout}\n{result.stderr}")
    return len(jobs)


# Umbral de la verificacion mecanica (AUDITORIA-01 A-01, punto 5): una
# tira sana tiene decenas de tonos intermedios entre la tinta y el fondo;
# 3 o menos significa que la composicion volvio a binarizarse en algun
# paso. No es un numero estetico -- es el mismo chequeo que se corrio a
# mano sobre la tira vieja y encontro 234/234 archivos con 1 solo tono.
MIN_INK_TONES = 4


def generate_icons(tokens):
    print("==> Generando iconos (out/icons/<tema>/*.bmp)")
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
    shapes_dir = OUT / "_shapes"
    n_shapes = render_symbol_shapes(tokens, shapes_dir)
    print(f"   {n_shapes} formas renderizadas desde SF Symbols")

    # Magenta = TRANSPARENT_COLOR de Rockbox (firmware/export/lcd.h),
    # clave de transparencia binaria para lcd_bitmap_transparent()
    # (D-010) -- se reserva EXCLUSIVAMENTE para pixeles de cobertura
    # exactamente cero. Todo pixel con cobertura > 0 (incluido el borde
    # mas tenue) se pre-compone contra el fondo real del tema/variante,
    # nunca contra el marcador: asi el borde queda como una rampa de
    # tonos reales en vez de un salto binario tinta/magenta.
    TRANSPARENT_RGB = (255, 0, 255)

    # Fondo de composicion por variante: la normal ("") vive siempre
    # sobre SHELL_BG (fila no seleccionada, o cualquier icono suelto
    # fuera de listas); la "-on" existe especificamente "para la fila
    # activa" (ver comment_variants en tokens.json) -- su unico uso
    # nombrado es sobre la pastilla de seleccion, SELECTION_FILL. Es la
    # lectura mas fiel al proposito documentado de cada variante, no una
    # eleccion arbitraria -- ver AUDITORIA-01, seccion 0 (limitacion
    # honesta: donde "-on" se reutiliza sobre SHELL_BG directo, como en
    # Ahora suena, el borde queda pre-compuesto contra el fondo
    # equivocado; la rampa sigue siendo muchisimo mas fiel que la
    # escalera binaria de antes, y el caso primario -- la fila
    # resaltada -- queda exacto).
    # "-tertiary"/"-rail" (AUDITORIA-01 A-16): mismo criterio que la
    # variante normal -- se usan sobre contenido en reposo (icono de
    # modo inactivo, estrella vacia), nunca sobre la pastilla de
    # seleccion, asi que su fondo de composicion tambien es SHELL_BG.
    # "-selector" (PLAN.md T2.2/G5): tinta blanca constante para
    # contenido sobre el Selector nuevo (pastilla de acento, no gris) --
    # compuesta contra SHELL_BG como aproximacion documentada, porque el
    # fondo REAL (el acento, configurable en runtime) no se conoce en
    # tiempo de generacion -- mismo limite ya aceptado para "-on"
    # reutilizada fuera de una pastilla de seleccion (D-086).
    VARIANT_BG_TOKEN = {
        "": "shell_bg",
        "-on": "selection_fill",
        "-tertiary": "shell_bg",
        "-rail": "shell_bg",
        "-selector": "shell_bg",
    }

    tone_report = []  # (path, n_tonos) de cada bmp generado
    fail_files = []

    for theme_name, colors in tokens["color"].items():
        theme_out = OUT / "icons" / theme_name
        theme_out.mkdir(parents=True, exist_ok=True)

        for suffix, color_token in icon_cfg["variants"].items():
            fg_rgb = hex_to_rgb(colors[color_token])
            bg_token = VARIANT_BG_TOKEN.get(suffix, "shell_bg")
            bg_rgb = hex_to_rgb(colors[bg_token])

            for icon_key in icon_cfg["names"]:
                for size_name, size_px in icon_cfg["sizes"].items():
                    w_px, h_px = icon_canvas_dims(icon_cfg, icon_key, size_name, size_px)
                    src = Image.open(shapes_dir / f"{icon_key}-{size_px}.png").convert("RGBA")
                    # Reducir DESPUES de renderizar a SUPERSAMPLE x el
                    # tamano final (ver nota de modulo mas arriba): el
                    # filtro de caja promedia la cobertura real de los
                    # subpixeles del render en alta resolucion, asi el
                    # canal alfa describe la curva real del glifo.
                    src = src.resize((w_px, h_px), Image.BOX)
                    coverage = src.getchannel("A")

                    # Composicion por cobertura: donde coverage=255,
                    # resultado=fg puro; donde coverage=0, resultado=bg
                    # puro; en el medio, mezcla lineal -- exactamente lo
                    # que Image.composite hace con una mascara en modo L.
                    fg_img = Image.new("RGB", src.size, fg_rgb)
                    bg_img = Image.new("RGB", src.size, bg_rgb)
                    out_img = Image.composite(fg_img, bg_img, coverage)

                    # Magenta SOLO en cobertura exactamente cero -- nunca
                    # en un pixel de borde parcial (ese es justo el bug
                    # que D-075 tuvo que evitar con el marcador exacto).
                    zero_mask = coverage.point(lambda a: 255 if a == 0 else 0, mode="1")
                    out_img.paste(Image.new("RGB", src.size, TRANSPARENT_RGB), (0, 0), zero_mask)

                    out_path = theme_out / f"{icon_key}-{size_px}{suffix}.bmp"
                    out_img.save(out_path, format="BMP")

                    tones = {px for px in out_img.getdata() if px != TRANSPARENT_RGB}
                    tone_report.append((str(out_path), len(tones)))
                    # Tinta == fondo de composicion (unico caso real:
                    # "-selector" blanco sobre shell_bg blanco del tema
                    # claro): la rampa de 1 tono es matematicamente
                    # inevitable, no una regresion del pipeline -- el
                    # camino primario de ese icono son las mascaras.
                    if fg_rgb != bg_rgb and len(tones) < MIN_INK_TONES:
                        fail_files.append((str(out_path), len(tones)))

        total = len(icon_cfg["names"]) * len(icon_cfg["sizes"]) * len(icon_cfg["variants"])
        print(f"   tema {theme_name}: {total} bmp "
              f"({len(icon_cfg['names'])} iconos x {len(icon_cfg['sizes'])} tamanos "
              f"x {len(icon_cfg['variants'])} variantes)")

    # Mascaras de cobertura (una por icono x tamano, independientes de
    # tema y variante): BMP 24-bit donde R=G=B=cobertura del glifo
    # (0=fondo, 255=tinta plena, intermedios=borde antialiasado). El
    # firmware las compone en TIEMPO DE DIBUJO contra el framebuffer
    # real (aura_widgets.c) con la tinta del token vigente -- eso
    # elimina el limite estructural de los bmp pre-compuestos de arriba:
    # el fondo real de un icono no siempre es conocible en generacion
    # (Selector con acento configurable en runtime, tile con degradado
    # de SelectionSummary), y componer contra el fondo equivocado
    # produce el halo claro en los bordes que motivo este cambio. Los
    # bmp pre-compuestos se conservan como fallback (mismo formato de
    # siempre) para cualquier consumidor que no haya migrado.
    masks_out = OUT / "icons" / "masks"
    masks_out.mkdir(parents=True, exist_ok=True)
    n_masks = 0
    for icon_key in icon_cfg["names"]:
        for size_name, size_px in icon_cfg["sizes"].items():
            w_px, h_px = icon_canvas_dims(icon_cfg, icon_key, size_name, size_px)
            src = Image.open(shapes_dir / f"{icon_key}-{size_px}.png").convert("RGBA")
            src = src.resize((w_px, h_px), Image.BOX)
            coverage = src.getchannel("A")
            Image.merge("RGB", (coverage, coverage, coverage)).save(
                masks_out / f"{icon_key}-{size_px}.bmp", format="BMP")
            n_masks += 1
    print(f"   {n_masks} mascaras de cobertura (icons/masks/, un set unico para ambos temas)")

    # Verificacion mecanica de rampa (AUDITORIA-01 A-01, punto 5): no
    # basta con "compila y corre" -- si el resultado sigue binarizado,
    # el pipeline debe fallar ruidosamente, no dar por buena una tira
    # que se ve identica a la version rota anterior.
    tones_per_file = [n for _, n in tone_report]
    avg_tones = sum(tones_per_file) / len(tones_per_file) if tones_per_file else 0
    max_tones = max(tones_per_file) if tones_per_file else 0
    print(f"   verificacion de rampa: promedio {avg_tones:.1f} tonos/archivo, "
          f"maximo {max_tones}, minimo aceptado {MIN_INK_TONES}")

    if fail_files:
        detail = "\n".join(f"  {p}: {n} tono(s)" for p, n in fail_files[:20])
        more = f"\n  ... y {len(fail_files) - 20} mas" if len(fail_files) > 20 else ""
        die(
            f"verificacion de rampa fallo -- {len(fail_files)} icono(s) siguen "
            f"binarizados (< {MIN_INK_TONES} tonos de tinta):\n{detail}{more}"
        )

    shutil.rmtree(shapes_dir)


def generate_panel_backgrounds(tokens):
    """Fondos completos del panel derecho de SelectionSummary, uno por
    preset de acento (D-267, encargo del dueno de producto: 'el
    background del selection summary va a cambiar dependiendo del color
    de acento seleccionado'). Fuente: design-system/assets/panel-backgrounds/
    <nombre>-source.png (foto/gradiente propio del dueno, cualquier
    tamano) -> recorte centrado + reduccion exacta a las dimensiones
    reales del panel (aura_ds.metrics.left_panel, 160x240 hoy) -> BMP de
    24 bits sin transparencia (se dibuja opaco, cubre TODO el panel, no
    hay lcd_bitmap_transparent involucrado).
    """
    from PIL import Image

    print("==> Generando fondos del panel derecho (out/icons/aura/backgrounds/*.bmp)")
    cfg = tokens["aura_ds"]["metrics"]["right_panel_background"]
    panel = tokens["aura_ds"]["metrics"]["left_panel"]
    panel_w, panel_h = panel["width"], panel["height"]
    src_dir = ROOT / "assets" / "panel-backgrounds"
    out_dir = OUT / "icons" / "aura" / "backgrounds"
    out_dir.mkdir(parents=True, exist_ok=True)

    for name in cfg["presets"]:
        src_path = src_dir / f"{name}-source.png"
        if not src_path.exists():
            die(f"falta {src_path} (fondo de panel '{name}', declarado en "
                f"aura_ds.metrics.right_panel_background.presets)")
        img = Image.open(src_path).convert("RGB")

        # Recorte centrado a la proporcion del panel antes de reducir --
        # evita deformar el gradiente (un resize directo sin recortar
        # estiraria la imagen si la proporcion fuente no coincide).
        target_ratio = panel_w / panel_h
        src_ratio = img.width / img.height
        if src_ratio > target_ratio:
            new_w = int(img.height * target_ratio)
            x0 = (img.width - new_w) // 2
            img = img.crop((x0, 0, x0 + new_w, img.height))
        elif src_ratio < target_ratio:
            new_h = int(img.width / target_ratio)
            y0 = (img.height - new_h) // 2
            img = img.crop((0, y0, img.width, y0 + new_h))

        img = img.resize((panel_w, panel_h), Image.LANCZOS)
        img.save(out_dir / f"{name}.bmp", format="BMP")
        print(f"   {name} ({panel_w}x{panel_h}) -> {name}.bmp")


def generate_tile_icons(tokens):
    """Iconos de un solo consumidor, a color completo (D-269, encargo del
    dueno de producto: el badge real de Aura -- un bundle .icon de Icon
    Composer, aplanado a mano por mi via los SVG de capa individuales,
    ver DECISIONS.md) -- distinto del pipeline de SF Symbols de arriba:
    no son un glifo monocromo para tenir por tema/variante, son una
    imagen ya compuesta con su propio color. Mismo mecanismo de clave de
    transparencia magenta que generate_icons() (D-010) pero la
    composicion del BORDE se hace contra un solo color conocido (el
    centro del degradado de la seccion donde vive ESTE icono especifico
    -- hoy solo Ajustes/gris fijo, el unico consumidor), no contra un
    tema completo.
    """
    from PIL import Image

    print("==> Generando iconos de tile a color completo (out/icons/aura/tile-icons/*.bmp)")
    cfg = tokens["aura_ds"]["metrics"]["tile_icons"]
    src_dir = ROOT / "assets" / "tile-icons"
    out_dir = OUT / "icons" / "aura" / "tile-icons"
    out_dir.mkdir(parents=True, exist_ok=True)
    TRANSPARENT_RGB = (255, 0, 255)

    for name, spec in cfg["items"].items():
        # D-285: 'themed' -> una fuente y una salida por tema
        # (<name>-light / <name>-dark); si no, una sola (<name>).
        variants = ["light", "dark"] if spec.get("themed") else [None]
        for theme in variants:
            stem = f"{name}-{theme}" if theme else name
            src_path = src_dir / f"{stem}-source.png"
            if not src_path.exists():
                die(f"falta {src_path} (icono de tile '{name}', declarado en "
                    f"aura_ds.metrics.tile_icons.items)")
            size = spec["size"]
            bg_rgb = hex_to_rgb(spec["compose_bg_hex"])

            img = Image.open(src_path).convert("RGBA")
            img = img.resize((size, size), Image.LANCZOS)

            out_img = Image.new("RGB", (size, size), TRANSPARENT_RGB)
            bg_img = Image.new("RGB", (size, size), bg_rgb)
            alpha = img.split()[3]
            composed = Image.composite(img.convert("RGB"), bg_img, alpha)
            # Solo cobertura EXACTAMENTE cero se queda como clave de
            # transparencia -- cualquier otra cosa (incluido el borde
            # antialiasado) ya quedo pre-compuesta contra bg_rgb arriba.
            zero_mask = alpha.point(lambda a: 255 if a == 0 else 0)
            out_img.paste(composed, (0, 0))
            out_img.paste(Image.new("RGB", (size, size), TRANSPARENT_RGB), (0, 0), zero_mask)

            out_img.save(out_dir / f"{stem}.bmp", format="BMP")
            print(f"   {stem} ({size}x{size}) -> {stem}.bmp")


def main():
    tokens = json.loads(TOKENS_PATH.read_text())

    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    generate_header(tokens)
    generate_swift_palette(tokens)
    generate_fonts(tokens)
    generate_icons(tokens)
    generate_panel_backgrounds(tokens)
    generate_tile_icons(tokens)

    print("==> Pipeline completo. Salida en design-system/out/")


if __name__ == "__main__":
    main()
