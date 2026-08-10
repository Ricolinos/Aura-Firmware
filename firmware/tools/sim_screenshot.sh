#!/usr/bin/env bash
# Lanza el simulador Aura headless, toma un screendump automatico y lo
# convierte a PNG. No requiere permisos de Grabacion de Pantalla de macOS:
# usa el mecanismo screen_dump() propio de Rockbox (ver D-008 en
# DECISIONS.md), disparado por las variables de entorno
# AURA_SIM_AUTODUMP_TICKS / AURA_SIM_AUTODUMP_QUIT (parche en
# uisimulator/common/sim_tasks.c).
#
# Uso:
#   firmware/tools/sim_screenshot.sh <salida.png> [ticks]
#
# `ticks` son ticks del kernel de Rockbox tras el arranque (HZ ticks = 1s
# aprox.); por defecto 150 (~1.5 s), suficiente para que la primera
# pantalla ya este dibujada.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/firmware/build-sim"
OUT_PNG="${1:?Uso: sim_screenshot.sh <salida.png> [ticks]}"
TICKS="${2:-150}"

cd "$BUILD_DIR"
rm -f simdisk/dump*.bmp

AURA_SIM_AUTODUMP_TICKS="$TICKS" AURA_SIM_AUTODUMP_QUIT=1 ./rockboxui \
  > /dev/null 2>&1 || true

DUMP_FILE="$(ls -t simdisk/dump*.bmp 2>/dev/null | head -1)"
if [[ -z "$DUMP_FILE" ]]; then
  echo "ERROR: no se genero ningun dump.bmp" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUT_PNG")"
sips -s format png "$DUMP_FILE" --out "$OUT_PNG" > /dev/null
rm -f "$DUMP_FILE"
echo "==> Screenshot guardado en $OUT_PNG"
