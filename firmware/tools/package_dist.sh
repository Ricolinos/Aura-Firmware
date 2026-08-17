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
#   firmware/dist/checksums.txt            -- SHA-256 de los anteriores
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
[[ -d "$ROOT_DIR/design-system/out/icons/backgrounds" ]] && \
  cp -R "$ROOT_DIR/design-system/out/icons/backgrounds" "$STAGE/.rockbox/icons/aura/backgrounds"
[[ -d "$ROOT_DIR/design-system/out/icons/tile-icons" ]] && \
  cp -R "$ROOT_DIR/design-system/out/icons/tile-icons" "$STAGE/.rockbox/icons/aura/tile-icons"
# rockbox.ipod suelto en la raíz del árbol (el bootloader lo arranca así)
cp "$BUILD_DIR/rockbox.ipod" "$STAGE/.rockbox/rockbox.ipod"
(cd "$STAGE" && zip -qr "$DIST_DIR/rockbox.zip" .rockbox)

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
