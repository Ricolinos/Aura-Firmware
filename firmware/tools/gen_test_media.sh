#!/usr/bin/env bash
# Genera archivos de audio sinteticos cortos en cada formato nativo que
# Aura debe reproducir (FLAC, MP3, AAC/m4a, ALAC, WAV, AIFF), con tags y
# una letra .lrc de muestra, para probar el reproductor en el simulador
# sin depender de musica real del usuario.
#
# Salida: firmware/test-media/ (no se versiona -- generado on-demand).
#
# Uso: firmware/tools/gen_test_media.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="$ROOT_DIR/firmware/test-media"

mkdir -p "$OUT_DIR"

TITLE="Aura Test Tone"
ARTIST="Aura QA"
ALBUM="Fase 4 Fixtures"
DURATION=3
FREQ=440

gen() {
  local ext="$1"; shift
  local out="$OUT_DIR/aura-test.$ext"
  echo "==> Generando $out"
  ffmpeg -y -loglevel error \
    -f lavfi -i "sine=frequency=${FREQ}:duration=${DURATION}" \
    -metadata title="$TITLE" -metadata artist="$ARTIST" -metadata album="$ALBUM" \
    "$@" "$out"
}

gen flac -c:a flac
gen mp3  -c:a libmp3lame -b:a 128k
gen m4a  -c:a aac -b:a 128k
gen alac.m4a -c:a alac
gen wav  -c:a pcm_s16le
gen aiff -c:a pcm_s16be

cat > "$OUT_DIR/aura-test.lrc" <<'EOF'
[ar:Aura QA]
[ti:Aura Test Tone]
[al:Fase 4 Fixtures]
[00:00.00]Instrumental
[00:01.00]Segundo uno
[00:02.00]Segundo dos
EOF

echo "==> Generando $OUT_DIR/cover.jpg"
ffmpeg -y -loglevel error -f lavfi -i "color=c=0x3366CC:s=200x200" -frames:v 1 "$OUT_DIR/cover.jpg"

echo "==> Listo: $OUT_DIR"
