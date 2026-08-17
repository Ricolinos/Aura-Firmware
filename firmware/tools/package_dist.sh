#!/usr/bin/env bash
# Empaqueta los artefactos distribuibles del firmware en firmware/dist/.
# No se versionan (ver firmware/dist/README.md) -- este script los
# reproduce a partir del código fuente, para publicarlos como GitHub
# Release.
#
# Uso:
#   firmware/tools/package_dist.sh
#
# Requiere: firmware/tools/build_sim.sh ya corrido al menos una vez (para
# tener design-system/out/ con las fuentes e íconos), y el toolchain ARM
# instalado en firmware/toolchain/ (ver docs/guia-desarrollo.md,
# rockboxdev.sh).
#
# Produce:
#   firmware/dist/rockbox.ipod             -- binario del firmware
#   firmware/dist/rockbox.zip              -- árbol .rockbox/ completo
#   firmware/dist/mks5lboot                -- herramienta de flasheo DFU
#   firmware/dist/AuraPalette.swift        -- paleta para Aura Studio (design-system/generate.py --swift-out)
#   firmware/dist/MODIFICATIONS.md         -- listado GPL §2a, para el Release
#   firmware/dist/theme-format-v1.json     -- contrato del formato de tema, para Aura Studio
#   firmware/dist/aura-theme-default.zip   -- el default reempaquetado como tema instalable (id "aura", libre)
#   firmware/dist/checksums.txt            -- SHA-256 de los 3 binarios de arriba
#
# NO produce (paso manual, ver nota abajo):
#   firmware/dist/bootloader-ipod6g.ipod

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_DIR="$ROOT_DIR/firmware/rockbox"
BUILD_DIR="$ROOT_DIR/firmware/build-ipod6g"
DIST_DIR="$ROOT_DIR/firmware/dist"
TOOLCHAIN="$ROOT_DIR/firmware/toolchain/bin"

if [[ ! -d "$TOOLCHAIN" ]]; then
  echo "ERROR: no se encontró $TOOLCHAIN -- instala el toolchain ARM primero" >&2
  echo "  (ver docs/guia-desarrollo.md, rockboxdev.sh)" >&2
  exit 1
fi

if [[ ! -f "$ROOT_DIR/design-system/out/apple2026_tokens.h" ]]; then
  echo "==> design-system/out/ no existe todavía -- corriendo build_sim.sh primero"
  "$ROOT_DIR/firmware/tools/build_sim.sh"
fi

echo "==> Configurando firmware/build-ipod6g (si hace falta)"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
if [[ ! -f Makefile ]]; then
  PATH="$TOOLCHAIN:$PATH" "$SRC_DIR/tools/configure" --target=ipod6g --type=N
fi

echo "==> Compilando el firmware ARM (rockbox.ipod)"
PATH="$TOOLCHAIN:$PATH" make -j"$(sysctl -n hw.ncpu)"

echo "==> Compilando mks5lboot"
MKS5LBOOT_DIR="$SRC_DIR/utils/mks5lboot"
(cd "$MKS5LBOOT_DIR" && make)

mkdir -p "$DIST_DIR"

echo "==> Copiando rockbox.ipod"
cp "$BUILD_DIR/rockbox.ipod" "$DIST_DIR/rockbox.ipod"

echo "==> Copiando mks5lboot"
cp "$MKS5LBOOT_DIR/mks5lboot" "$DIST_DIR/mks5lboot"

echo "==> Armando rockbox.zip (.rockbox/ completo)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/.rockbox"
# Fuentes e íconos generados por design-system/generate.py (via build_sim.sh)
cp -R "$ROOT_DIR/design-system/out/fonts" "$STAGE/.rockbox/fonts"
mkdir -p "$STAGE/.rockbox/icons/aura"
cp -R "$ROOT_DIR/design-system/out/icons/light" "$STAGE/.rockbox/icons/aura/light"
cp -R "$ROOT_DIR/design-system/out/icons/dark" "$STAGE/.rockbox/icons/aura/dark"
cp -R "$ROOT_DIR/design-system/out/icons/masks" "$STAGE/.rockbox/icons/aura/masks"
# D-288: el pipeline escribe estas dos carpetas bajo out/icons/aura/,
# no out/icons/ a secas -- esta linea buscaba la ruta vieja (de antes
# de que existiera el subdirectorio "aura") y nunca encontraba nada,
# asi que rockbox.zip salia sin fondos de panel ni tile-icons en TODO
# release hecho con este script. build_sim.sh no tiene este bug (usa
# la ruta con /aura/ correcta), por eso no se habia notado -- el
# simulador siempre los tuvo.
[[ -d "$ROOT_DIR/design-system/out/icons/aura/backgrounds" ]] && \
  cp -R "$ROOT_DIR/design-system/out/icons/aura/backgrounds" "$STAGE/.rockbox/icons/aura/backgrounds"
[[ -d "$ROOT_DIR/design-system/out/icons/aura/tile-icons" ]] && \
  cp -R "$ROOT_DIR/design-system/out/icons/aura/tile-icons" "$STAGE/.rockbox/icons/aura/tile-icons"
# D-289 (sistema de temas): el default compilado tambien vive bajo
# .rockbox/aura/ -- el directorio de estilos instalados (vacio de
# fabrica) viaja en el zip para que exista desde el primer arranque,
# sin depender de que Aura Studio lo cree.
mkdir -p "$STAGE/.rockbox/aura/themes"
# rockbox.ipod suelto en la raíz del árbol (el bootloader lo arranca así)
cp "$BUILD_DIR/rockbox.ipod" "$STAGE/.rockbox/rockbox.ipod"
(cd "$STAGE" && zip -qr "$DIST_DIR/rockbox.zip" .rockbox)

echo "==> Generando AuraPalette.swift (asset del Release para Aura Studio, ver CONTRATO-firmware-studio.md)"
"$ROOT_DIR/design-system/.venv/bin/python3" "$ROOT_DIR/design-system/generate.py" --swift-out "$DIST_DIR/AuraPalette.swift"

echo "==> Copiando MODIFICATIONS.md (asset del Release, cumplimiento GPL §2a)"
cp "$ROOT_DIR/MODIFICATIONS.md" "$DIST_DIR/MODIFICATIONS.md"

echo "==> Copiando theme-format-v1.json (asset del Release, contrato de formato de tema)"
cp "$ROOT_DIR/design-system/out/theme-format-v1.json" "$DIST_DIR/theme-format-v1.json"

# D-289 (sistema de temas, Q5 de PLAN-themes-impl.md): el default
# compilado (Inter + Lucide/Phosphor, licencia libre) reempaquetado en
# formato de tema instalable -- ejemplo canonico del formato para
# Aura Studio, y un tema libre real para probar el instalador sin
# tocar material con licencia restringida. id "aura" (no "default":
# ese id esta reservado para el tema compilado, aura_style_id_is_valid()
# lo rechaza como paquete en disco).
echo "==> Armando aura-theme-default.zip (tema por defecto reempaquetado, formato v1)"
THEME_STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE" "$THEME_STAGE"' EXIT
mkdir -p "$THEME_STAGE/aura/fonts" "$THEME_STAGE/aura/icons/masks"
# Pares "rol:archivo", NO un array asociativo -- el bash 3.2 que trae
# macOS de fabrica (sin associative arrays, bash 4+) es el que corre
# este script via #!/usr/bin/env bash en esta maquina, verificado con
# `bash --version`. Mismos 14 roles y nombres que role_font_names[] en
# aura_style.c.
FONT_ROLE_PAIRS="
title:a26-title-20.fnt
body:a26-body-13.fnt
caption:a26-caption-13.fnt
header:a26-header-13.fnt
micro:a26-micro-7.fnt
ds_reg_8:a26-ds_reg_8-8.fnt
ds_semibold_15:a26-ds_semibold_15-15.fnt
ds_reg_10:a26-ds_reg_10-10.fnt
ds_bold_10:a26-ds_bold_10-10.fnt
ds_reg_12:a26-ds_reg_12-12.fnt
ds_bold_12:a26-ds_bold_12-12.fnt
ds_bold_14:a26-ds_bold_14-14.fnt
ds_bold_18:a26-ds_bold_18-18.fnt
ds_medium_16:a26-ds_medium_16-16.fnt
"
for pair in $FONT_ROLE_PAIRS; do
  role="${pair%%:*}"
  file="${pair##*:}"
  cp "$ROOT_DIR/design-system/out/fonts/$file" "$THEME_STAGE/aura/fonts/$role.fnt"
done
cp -R "$ROOT_DIR/design-system/out/icons/masks/." "$THEME_STAGE/aura/icons/masks/"
cp -R "$ROOT_DIR/design-system/out/icons/light" "$THEME_STAGE/aura/icons/light"
cp -R "$ROOT_DIR/design-system/out/icons/dark" "$THEME_STAGE/aura/icons/dark"
[[ -d "$ROOT_DIR/design-system/out/icons/aura/backgrounds" ]] && \
  cp -R "$ROOT_DIR/design-system/out/icons/aura/backgrounds" "$THEME_STAGE/aura/backgrounds"
[[ -d "$ROOT_DIR/design-system/out/icons/aura/tile-icons" ]] && \
  cp -R "$ROOT_DIR/design-system/out/icons/aura/tile-icons" "$THEME_STAGE/aura/tile-icons"
"$ROOT_DIR/design-system/.venv/bin/python3" - "$ROOT_DIR/design-system/tokens.json" "$THEME_STAGE/aura/theme.cfg" <<'PYEOF'
import json, sys
tokens = json.load(open(sys.argv[1]))
color = tokens["color"]
cat = tokens["aura_ds"]["color"]["category"]
roles = ["shell_bg", "text_primary", "text_secondary", "text_tertiary",
         "shell_rail", "progress_fill", "progress_track", "selection_fill"]
lines = [
    "theme_format: 1",
    "theme_id: aura",
    "theme_name: Aura",
    "theme_author: Ricardo Gomez",
    "theme_license: open",
    "theme_redistributable: yes",
]
for mode in ("light", "dark"):
    for role in roles:
        lines.append(f"palette_{mode}_{role}: {color[mode][role]}")
lines.append(f"category_settings_gray: {cat['settings_gray_hex']}")
lines.append(f"category_video: {cat['video_hex']}")
lines.append(f"category_photos: {cat['photos_hex']}")
lines.append(f"category_extras_yellow: {cat['extras_yellow_hex']}")
lines.append(f"accent_default: {tokens['aura_ds']['color']['accent_default_hex']}")
lines.append("accent_presets: " + ",".join(tokens["aura_ds"]["color"]["accent_presets_hex"]))
open(sys.argv[2], "w").write("\n".join(lines) + "\n")
PYEOF
(cd "$THEME_STAGE" && zip -qr "$DIST_DIR/aura-theme-default.zip" aura)
rm -rf "$THEME_STAGE"

echo "==> Generando checksums.txt"
(
  cd "$DIST_DIR"
  files=(rockbox.zip rockbox.ipod mks5lboot)
  [[ -f bootloader-ipod6g.ipod ]] && files+=(bootloader-ipod6g.ipod)
  shasum -a 256 "${files[@]}" > checksums.txt
)

echo "==> Listo: $DIST_DIR"
ls -la "$DIST_DIR"

if [[ ! -f "$DIST_DIR/bootloader-ipod6g.ipod" ]]; then
  cat <<'NOTE'

NOTA: bootloader-ipod6g.ipod NO se generó -- requiere el toolchain del
bootloader (tipo B), que este script no configura automáticamente:

  mkdir -p firmware/build-ipod6g-boot && cd firmware/build-ipod6g-boot
  PATH="$PWD/../toolchain/bin:$PATH" ../rockbox/tools/configure --target=ipod6g --type=B
  PATH="$PWD/../toolchain/bin:$PATH" make -j$(sysctl -n hw.ncpu)
  cp bootloader.ipod ../dist/bootloader-ipod6g.ipod

Después, vuelve a correr este script para incluirlo en checksums.txt.
NOTE
fi
