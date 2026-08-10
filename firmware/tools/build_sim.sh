#!/usr/bin/env bash
# Compila el simulador SDL de Aura (target ipod6g) en macOS.
#
# Requisitos (Homebrew): sdl2, un gcc real (no solo el clang de Xcode) —
# `brew install sdl2 gcc`. tools/configure detecta automaticamente el gcc
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
python3 "$ROOT_DIR/design-system/generate.py"

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

echo "==> Listo: $BUILD_DIR/rockboxui"

if [[ "${1:-}" == "--run" ]]; then
  exec ./rockboxui
fi
