#!/usr/bin/env bash
# Compila el simulador SDL de Aura (target ipod6g) en macOS.
#
# Requisitos (Homebrew): sdl2, un gcc real (no solo el clang de Xcode),
# freetype, librsvg — `brew install sdl2 gcc freetype librsvg`. Ademas un
# venv en design-system/.venv con Pillow (design-system/scripts/fetch_assets.sh
# no lo crea; `python3 -m venv design-system/.venv && design-system/.venv/bin/pip
# install pillow` una vez). tools/configure detecta automaticamente el gcc
# de Homebrew mas nuevo disponible (16, 15, 14 o 13); ver D-007 en
# DECISIONS.md sobre por que clang no sirve aqui.
#
# Uso:
#   firmware/tools/build_sim.sh            # configura (si hace falta) y compila
#   firmware/tools/build_sim.sh --reconfigure   # fuerza reconfigurar desde cero
#   firmware/tools/build_sim.sh --run           # compila y lanza el simulador

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_DIR="$ROOT_DIR/firmware/rockbox"
BUILD_DIR="$ROOT_DIR/firmware/build-sim"

if [[ "${1:-}" == "--reconfigure" ]]; then
  rm -rf "$BUILD_DIR"
  shift
fi

echo "==> Regenerando el design system (design-system/generate.py)"
GENERATE_PY="$ROOT_DIR/design-system/.venv/bin/python3"
if [[ ! -x "$GENERATE_PY" ]]; then
  GENERATE_PY="python3"
fi
"$GENERATE_PY" "$ROOT_DIR/design-system/generate.py"

echo "==> Instalando apple2026_tokens.h en apps/aura/"
cp "$ROOT_DIR/design-system/out/apple2026_tokens.h" "$SRC_DIR/apps/aura/apple2026_tokens.h"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

if [[ ! -f Makefile ]]; then
  echo "==> Configurando simulador (target ipod6g)"
  "$SRC_DIR/tools/configure" --target=ipod6g --type=s
fi

echo "==> Compilando (make -j)"
make -j"$(sysctl -n hw.ncpu)"

echo "==> Instalando assets de Rockbox en el disco simulado"
make install

echo "==> Instalando fuentes e iconos de Aura en el disco simulado"
mkdir -p simdisk/.rockbox/fonts
cp "$ROOT_DIR"/design-system/out/fonts/*.fnt simdisk/.rockbox/fonts/
for theme in light dark; do
  mkdir -p "simdisk/.rockbox/icons/aura/$theme"
  cp "$ROOT_DIR"/design-system/out/icons/$theme/*.bmp "simdisk/.rockbox/icons/aura/$theme/"
done
# Mascaras de cobertura (composicion runtime en aura_widgets.c) -- un
# solo set para ambos temas, la tinta se decide al dibujar.
mkdir -p simdisk/.rockbox/icons/aura/masks
cp "$ROOT_DIR"/design-system/out/icons/masks/*.bmp simdisk/.rockbox/icons/aura/masks/
# Fondos del panel derecho de SelectionSummary por acento (D-267).
mkdir -p simdisk/.rockbox/icons/aura/backgrounds
cp "$ROOT_DIR"/design-system/out/icons/aura/backgrounds/*.bmp simdisk/.rockbox/icons/aura/backgrounds/
# Iconos de tile a color completo, un solo consumidor cada uno (D-269).
mkdir -p simdisk/.rockbox/icons/aura/tile-icons
cp "$ROOT_DIR"/design-system/out/icons/aura/tile-icons/*.bmp simdisk/.rockbox/icons/aura/tile-icons/

echo "==> Listo: $BUILD_DIR/rockboxui"

if [[ "${1:-}" == "--run" ]]; then
  exec ./rockboxui
fi
