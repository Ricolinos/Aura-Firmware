#!/usr/bin/env bash
# Lanza el simulador Aura headless, opcionalmente inyecta una secuencia
# de botones, toma un screendump automatico y lo convierte a PNG. No
# requiere permisos de Grabacion de Pantalla ni de Accesibilidad de
# macOS: usa el mecanismo screen_dump() propio de Rockbox mas el parche
# de automatizacion en uisimulator/common/sim_tasks.c (ver D-008 y D-017
# en DECISIONS.md).
#
# Uso:
#   firmware/tools/apple2026_sim_shot.sh <salida.png> [ticks] [botones]
#
# `ticks`   ticks del kernel de Rockbox (HZ ticks = 1s aprox.) a esperar
#           antes del dump -- desde el arranque si no hay `botones`, o
#           desde que termina de inyectarlos si los hay. Por defecto 150.
# `botones` lista separada por comas de: SELECT, MENU, SCROLL_FWD,
#           SCROLL_BACK, PLAY, LEFT, RIGHT -- se inyectan en orden antes
#           de tomar el dump. Ejemplo: "SCROLL_FWD,SCROLL_FWD,SELECT"
#           (baja dos veces en una lista y entra al item seleccionado).

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/firmware/build-sim"
OUT_PNG="${1:?Uso: apple2026_sim_shot.sh <salida.png> [ticks] [botones]}"
TICKS="${2:-150}"
BUTTONS="${3:-}"

cd "$BUILD_DIR"
rm -f simdisk/dump*.bmp

export AURA_SIM_AUTODUMP_TICKS="$TICKS"
export AURA_SIM_AUTODUMP_QUIT=1
if [[ -n "$BUTTONS" ]]; then
  export AURA_SIM_BUTTONS="$BUTTONS"
else
  unset AURA_SIM_BUTTONS 2>/dev/null || true
fi

./rockboxui > /dev/null 2>&1 || true

DUMP_FILE="$(ls -t simdisk/dump*.bmp 2>/dev/null | head -1)"
if [[ -z "$DUMP_FILE" ]]; then
  echo "ERROR: no se genero ningun dump.bmp" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUT_PNG")"
sips -s format png "$DUMP_FILE" --out "$OUT_PNG" > /dev/null
rm -f "$DUMP_FILE"
echo "==> Screenshot guardado en $OUT_PNG"
