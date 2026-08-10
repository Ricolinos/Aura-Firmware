#!/usr/bin/env bash
# Matriz de capturas de la Fase 7: cada pantalla de Aura x 2 temas x 3
# modos graficos, generada automaticamente con el mismo mecanismo de
# inyeccion de botones headless de sim_screenshot.sh (ver D-017 en
# DECISIONS.md). Requiere que firmware/build-sim ya este compilado
# (firmware/tools/build_sim.sh) y los fixtures de prueba instalados
# (firmware/tools/gen_test_media.sh).
#
# Uso: firmware/tools/capture_matrix.sh

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/firmware/build-sim"
OUT_DIR="$ROOT_DIR/docs/screenshots/matrix"
SHOT="$ROOT_DIR/firmware/tools/sim_screenshot.sh"
CFG="$BUILD_DIR/simdisk/.rockbox/aura/aura.cfg"

mkdir -p "$OUT_DIR"
mkdir -p "$(dirname "$CFG")"

# name;ticks;buttons (buttons usa '|' en vez de ',' para no chocar con IFS)
SCREENS=(
  "root;150;"
  "music;150;SELECT"
  "music_artists;300;SELECT|SELECT"
  "music_albums;300;SELECT|SCROLL_FWD|SELECT"
  "music_albums_by_artist;300;SELECT|SELECT|SELECT"
  "music_songs;300;SELECT|SCROLL_FWD|SCROLL_FWD|SELECT"
  "music_songs_by_album;300;SELECT|SCROLL_FWD|SELECT|SELECT"
  "music_songs_by_genre;300;SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SELECT"
  "music_genres;300;SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "music_playlists;300;SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "videos;150;SCROLL_FWD|SELECT"
  "photos;150;SCROLL_FWD|SCROLL_FWD|SELECT"
  "photo_viewer;200;SCROLL_FWD|SCROLL_FWD|SELECT|SELECT"
  "nowplaying;350;SELECT|SCROLL_FWD|SCROLL_FWD|SELECT|SELECT"
  "settings;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_theme;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SELECT"
  "settings_graphics;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SELECT"
  "settings_eq;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_brightness;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_language;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_about;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
)

THEMES=(0 1)      # 0=light 1=dark (ver aura_settings.h)
MODES=(0 1 2)     # 0=ultra 1=minimal 2=full
THEME_LABEL=(light dark)
MODE_LABEL=(ultra minimal full)

FAILED=()

for ti in "${!THEMES[@]}"; do
  theme="${THEMES[$ti]}"
  tlabel="${THEME_LABEL[$ti]}"
  for mi in "${!MODES[@]}"; do
    mode="${MODES[$mi]}"
    mlabel="${MODE_LABEL[$mi]}"

    cat > "$CFG" <<EOF
theme: $theme
graphics_mode: $mode
eq_preset: 0
language: 0
EOF

    for entry in "${SCREENS[@]}"; do
      name="${entry%%;*}"
      rest="${entry#*;}"
      ticks="${rest%%;*}"
      buttons="${rest#*;}"
      buttons="${buttons//|/,}"

      out="$OUT_DIR/${name}-${tlabel}-${mlabel}.png"
      echo "==> $name ($tlabel/$mlabel)"
      if ! "$SHOT" "$out" "$ticks" "$buttons" >/tmp/capture_matrix_last.log 2>&1; then
        echo "    FALLO: $name-$tlabel-$mlabel"
        FAILED+=("$name-$tlabel-$mlabel")
      fi
    done
  done
done

rm -f "$CFG"

echo ""
echo "==> Listo. ${#FAILED[@]} fallo(s)."
if [[ ${#FAILED[@]} -gt 0 ]]; then
  printf '  - %s\n' "${FAILED[@]}"
fi
