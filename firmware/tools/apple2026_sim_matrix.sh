#!/usr/bin/env bash
# Matriz de capturas: cada pantalla de Aura x 2 temas x 3 modos
# graficos, generada automaticamente con el mismo mecanismo de
# inyeccion de botones headless de apple2026_sim_shot.sh (ver D-017 en
# DECISIONS.md). Requiere que firmware/build-sim ya este compilado
# (firmware/tools/build_sim.sh) y los fixtures de prueba instalados
# (firmware/tools/gen_test_media.sh).
#
# Uso: firmware/tools/apple2026_sim_matrix.sh

set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/firmware/build-sim"
OUT_DIR="$ROOT_DIR/docs/screenshots/matrix"
SHOT="$ROOT_DIR/firmware/tools/apple2026_sim_shot.sh"
CFG="$BUILD_DIR/simdisk/.rockbox/aura/aura.cfg"

mkdir -p "$OUT_DIR"
mkdir -p "$(dirname "$CFG")"

# name;ticks;buttons (buttons usa '|' en vez de ',' para no chocar con IFS)
#
# ATENCION al tocar settings_entries[] en aura_screens.c: los offsets de
# abajo son cuentas de SCROLL_FWD contra el orden de esa tabla, asi que
# insertar una fila en Ajustes los desplaza a todos y las capturas pasan
# a retratar la pantalla equivocada EN SILENCIO (ya paso una vez, ver
# D-067). No los edites a mano: regeneralos derivandolos de la tabla.
# Orden real actual: Tema(0) Animaciones(1) Graficos(2) EQ(3) Brillo(4)
# Aleatorio(5) Repetir(6) Temp.luz(7) Temp.reposo(8) LimiteVol(9)
# Clicker(10) MenuPpal(11) Idioma(12) AcercaDe(13) Restablecer(14).
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
  "settings_animations;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SELECT"
  "settings_graphics;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_eq;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_brightness;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_shuffle;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_repeat;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_backlight;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_sleeptimer;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_volume_limit;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_clicker;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_mainmenu;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_language;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_about;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
  "settings_reset;150;SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SCROLL_FWD|SELECT"
)

THEMES=(0 1)      # 0=light 1=dark (ver aura_settings.h)
# Se barre solo Graficos: Animaciones es movimiento y no deja rastro en
# una captura estatica, asi que agregarlo como tercer eje triplicaria la
# matriz sin agregar una sola imagen distinta. Queda fijo en Minimas.
MODES=(0 1 2)     # 0=ninguno 1=minimos 2=todos
THEME_LABEL=(light dark)
MODE_LABEL=(none minimal all)

FAILED=()

for ti in "${!THEMES[@]}"; do
  theme="${THEMES[$ti]}"
  tlabel="${THEME_LABEL[$ti]}"
  for mi in "${!MODES[@]}"; do
    mode="${MODES[$mi]}"
    mlabel="${MODE_LABEL[$mi]}"

    cat > "$CFG" <<EOF
theme: $theme
animation_mode: 1
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
