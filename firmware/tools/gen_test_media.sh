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

echo "==> Generando fixture SIN caratula (test-media/SinArte)"
# Carpeta propia sin cover.jpg: find_albumart no encuentra arte ni
# embebido ni de carpeta -> ejercita la imagen "Default" (nota gris
# sobre tile gris, D-112) en Cover Flow, el reproductor y Flip-and-Flow.
mkdir -p "$OUT_DIR/SinArte"
ffmpeg -y -loglevel error \
  -f lavfi -i "sine=frequency=330:duration=${DURATION}" \
  -metadata title="Pista sin arte" -metadata artist="Aura QA" \
  -metadata album="Album sin portada" \
  -c:a libmp3lame -b:a 128k "$OUT_DIR/SinArte/aura-test-noart.mp3"

echo "==> Generando $OUT_DIR/cover.jpg"
# -pix_fmt yuvj420p fuerza submuestreo de croma 4:2:0 estandar con tablas
# de cuantizacion separadas por componente. Sin este flag, el encoder
# mjpeg de ffmpeg a veces emite un muestreo no estandar (mismo factor de
# muestreo en los 3 componentes + una sola tabla de cuantizacion
# compartida) que el decoder JPEG de Rockbox no interpreta bien: decodifica
# "con exito" (ret>0, dimensiones correctas) pero el resultado sale
# corrupto/desordenado. Ver D-030 en DECISIONS.md.
ffmpeg -y -loglevel error -f lavfi -i "color=c=0x3366CC:s=200x200" \
  -pix_fmt yuvj420p -frames:v 1 "$OUT_DIR/cover.jpg"

echo "==> Generando fixtures de Fotos (test-media/Photos)"
mkdir -p "$OUT_DIR/Photos"
ffmpeg -y -loglevel error -f lavfi -i "testsrc=size=320x240:rate=1" \
  -pix_fmt yuvj420p -frames:v 1 "$OUT_DIR/Photos/photo1.jpg"
ffmpeg -y -loglevel error -f lavfi -i "color=c=0x2244AA:s=320x240" \
  -pix_fmt yuvj420p -frames:v 1 "$OUT_DIR/Photos/photo2.jpg"
ffmpeg -y -loglevel error -f lavfi -i "smptebars=size=160x120:rate=1" \
  -frames:v 1 "$OUT_DIR/Photos/photo3.bmp"
ffmpeg -y -loglevel error -f lavfi -i "color=c=0xAA6622:s=100x100" \
  -frames:v 1 "$OUT_DIR/Photos/photo4_unsupported.png"

echo "==> Generando fixture de Video (test-media/Videos)"
mkdir -p "$OUT_DIR/Videos"
ffmpeg -y -loglevel error -f lavfi -i "testsrc=size=320x240:rate=15:duration=2" \
  -f lavfi -i "sine=frequency=440:duration=2" \
  -c:v mpeg2video -q:v 5 -c:a mp2 "$OUT_DIR/Videos/test.mpg"

SIMDISK="$ROOT_DIR/firmware/build-sim/simdisk"
if [[ -d "$SIMDISK" ]]; then
  echo "==> Instalando fixtures en $SIMDISK"
  mkdir -p "$SIMDISK/Music" "$SIMDISK/Photos" "$SIMDISK/Videos"
  cp "$OUT_DIR"/aura-test.* "$SIMDISK/Music/"
  cp "$OUT_DIR"/cover.jpg "$SIMDISK/Music/"
  # En la RAIZ del disco, no bajo Music/: find_albumart tambien busca
  # cover.jpg en el directorio padre, y Music/cover.jpg "vestiria" al
  # album que precisamente debe quedar sin arte.
  mkdir -p "$SIMDISK/SinArte"
  cp "$OUT_DIR"/SinArte/*.mp3 "$SIMDISK/SinArte/"
  cp "$OUT_DIR"/Photos/* "$SIMDISK/Photos/"
  cp "$OUT_DIR"/Videos/*.mpg "$SIMDISK/Videos/"
fi

echo "==> Listo: $OUT_DIR"
