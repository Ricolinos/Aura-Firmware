#!/usr/bin/env bash
# Genera archivos de audio sinteticos cortos en cada formato nativo que
# Aura debe reproducir (FLAC, MP3, AAC/m4a, ALAC, WAV, AIFF), con tags y
# una letra .lrc de muestra, para probar el reproductor en el simulador
# sin depender de musica real del usuario.
#
# Salida: firmware/test-media/ (no se versiona -- generado on-demand).
#
# Uso: firmware/tools/gen_test_media.sh
#      firmware/tools/gen_test_media.sh --aspect-fixtures
#      firmware/tools/gen_test_media.sh --lang-fixtures
#
# --aspect-fixtures (D-350, contrato v18): ademas de lo de siempre, crea
# cuatro albumes en el simdisk cuyo cover.jpg NO es cuadrado (1:1, 4:3,
# 16:9 y 1:4). Existen para probar el recorte fill + center-crop de los
# caminos que no pasan por la cache maestra: si alguno vuelve a suponer
# que el bitmap decodificado es cuadrado, estas portadas lo delatan de
# inmediato (el patron de barras de color se ve sesgado o rasgado).
# Studio, desde el contrato v18, escribe siempre 320x320 -- estos
# fixtures son justamente lo que el firmware debe seguir tolerando.
#
# --lang-fixtures (D-357, ronda "ajustes 2" SS D.4): dos albumes con
# titulo/artista en cirilico y en aleman con ss/u dieresis -- para
# verificar a simple vista, en Musica/Cover Flow, que las etiquetas
# reales de archivo (no las cadenas de aura_lang.c, que ya tienen su
# propio host test) se leen y dibujan bien con las fuentes Inter del
# firmware, sin '?'/huecos.

set -euo pipefail

ASPECT_FIXTURES=0
LANG_FIXTURES=0
if [[ "${1:-}" == "--aspect-fixtures" ]]; then
  ASPECT_FIXTURES=1
  shift
elif [[ "${1:-}" == "--lang-fixtures" ]]; then
  LANG_FIXTURES=1
  shift
fi

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
  mkdir -p "$SIMDISK/Photos" "$SIMDISK/Videos"
  cp "$OUT_DIR"/Photos/* "$SIMDISK/Photos/"
  cp "$OUT_DIR"/Videos/*.mpg "$SIMDISK/Videos/"
  # Fixtures de MUSICA: solo bajo peticion explicita
  # (AURA_INSTALL_MUSIC_FIXTURES=1). Desde 2026-08-12 el simulador
  # trabaja contra la biblioteca REAL del dueno del diseno (symlink
  # simdisk/Musica -> "/Volumes/Ricolinos/Música/Exports CD") para ver
  # caratulas reales en Cover Flow -- instalar los tonos sinteticos por
  # defecto contaminaria esa biblioteca con albumes de prueba.
  if [[ "${AURA_INSTALL_MUSIC_FIXTURES:-0}" == "1" ]]; then
    mkdir -p "$SIMDISK/Music"
    cp "$OUT_DIR"/aura-test.* "$SIMDISK/Music/"
    cp "$OUT_DIR"/cover.jpg "$SIMDISK/Music/"
    # En la RAIZ del disco, no bajo Music/: find_albumart tambien busca
    # cover.jpg en el directorio padre, y Music/cover.jpg "vestiria" al
    # album que precisamente debe quedar sin arte.
    mkdir -p "$SIMDISK/SinArte"
    cp "$OUT_DIR"/SinArte/*.mp3 "$SIMDISK/SinArte/"
  fi
fi

# -- D-350: portadas NO cuadradas -----------------------------------------
if [[ "${ASPECT_FIXTURES:-0}" == "1" ]]; then
  SIMDISK="$ROOT_DIR/firmware/build-sim/simdisk"
  if [[ ! -d "$SIMDISK" ]]; then
    echo "ERROR: no existe $SIMDISK -- corre firmware/tools/build_sim.sh primero" >&2
    exit 1
  fi
  echo "==> Generando albumes de prueba con portadas NO cuadradas"
  # "<nombre>:<ancho>x<alto>". Barras de color SMPTE escaladas a la
  # proporcion: un recorte con el stride equivocado sale rasgado, no
  # solo mal encuadrado, asi que el fallo se ve a simple vista.
  for pair in "Aspecto 1x1:600x600" "Aspecto 4x3:800x600" "Aspecto 16x9:960x540" "Aspecto 1x4:300x1200"; do
    name="${pair%%:*}"
    dims="${pair##*:}"
    dir="$SIMDISK/Music/Aura QA/$name"
    mkdir -p "$dir"
    ffmpeg -y -loglevel error -f lavfi -i "smptebars=size=${dims}:rate=1" \
      -pix_fmt yuvj420p -frames:v 1 "$dir/cover.jpg"
    ffmpeg -y -loglevel error \
      -f lavfi -i "sine=frequency=${FREQ}:duration=${DURATION}" \
      -metadata title="Pista de $name" -metadata artist="$ARTIST" \
      -metadata album="$name" \
      -c:a libmp3lame -b:a 128k "$dir/pista.mp3"
    echo "   $name ($dims)"
  done
  echo "==> Recuerda: la base tagcache del simdisk tiene que reconstruirse"
  echo "    (Ajustes > Actualizar biblioteca) para que aparezcan."
fi

# -- D-357: albumes de prueba cirilico y aleman ---------------------------
if [[ "${LANG_FIXTURES:-0}" == "1" ]]; then
  SIMDISK="$ROOT_DIR/firmware/build-sim/simdisk"
  if [[ ! -d "$SIMDISK" ]]; then
    echo "ERROR: no existe $SIMDISK -- corre firmware/tools/build_sim.sh primero" >&2
    exit 1
  fi
  echo "==> Generando albumes de prueba cirilico y aleman"

  dir_ru="$SIMDISK/Music/Aura QA/Тестовый альбом"
  mkdir -p "$dir_ru"
  ffmpeg -y -loglevel error \
    -f lavfi -i "sine=frequency=${FREQ}:duration=${DURATION}" \
    -metadata title="Первая дорожка" -metadata artist="Тестовый исполнитель" \
    -metadata album="Тестовый альбом" \
    -c:a libmp3lame -b:a 128k "$dir_ru/pista.mp3"
  echo "   ru: Тестовый альбом / Тестовый исполнитель"

  dir_de="$SIMDISK/Music/Aura QA/Größe & Übermaß"
  mkdir -p "$dir_de"
  ffmpeg -y -loglevel error \
    -f lavfi -i "sine=frequency=${FREQ}:duration=${DURATION}" \
    -metadata title="Straße der Überraschung" -metadata artist="Königsgrößen" \
    -metadata album="Größe & Übermaß" \
    -c:a libmp3lame -b:a 128k "$dir_de/pista.mp3"
  echo "   de: Größe & Übermaß / Königsgrößen (ß/ü/ö)"

  echo "==> Recuerda: la base tagcache del simdisk tiene que reconstruirse"
  echo "    (Ajustes > Actualizar biblioteca) para que aparezcan."
fi

echo "==> Listo: $OUT_DIR"
