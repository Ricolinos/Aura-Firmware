#!/usr/bin/env bash
# Descarga las fuentes de origen (Inter, OFL) y los iconos de origen
# (Lucide, ISC) usados por el pipeline del sistema de diseno Aura.
#
# Los archivos descargados se versionan en design-system/vendor/ (son
# pequenos y su version exacta importa para reproducibilidad), asi que
# normalmente NO hace falta volver a ejecutar este script. Se documenta
# para dejar constancia exacta de la procedencia y permitir actualizar
# a una version mas nueva de Inter o Lucide en el futuro.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VENDOR_DIR="$ROOT_DIR/vendor"

INTER_TAG="v4.1"
INTER_ZIP_URL="https://github.com/rsms/inter/releases/download/${INTER_TAG}/Inter-${INTER_TAG#v}.zip"
LUCIDE_REF="main"
LUCIDE_RAW="https://raw.githubusercontent.com/lucide-icons/lucide/${LUCIDE_REF}"

echo "==> Inter ${INTER_TAG} (SIL OFL) -> vendor/inter-ttf/"
mkdir -p "$VENDOR_DIR/inter-ttf"
TMP_ZIP="$(mktemp -t inter-XXXXXX.zip)"
curl -sL --max-time 120 -o "$TMP_ZIP" "$INTER_ZIP_URL"
unzip -o -j "$TMP_ZIP" \
  "extras/ttf/Inter-Regular.ttf" \
  "extras/ttf/Inter-Medium.ttf" \
  "extras/ttf/Inter-SemiBold.ttf" \
  "LICENSE.txt" \
  -d "$VENDOR_DIR/inter-ttf/"
rm -f "$TMP_ZIP"

echo "==> Iconos Lucide (ISC) -> vendor/lucide-svg/"
mkdir -p "$VENDOR_DIR/lucide-svg"
ICON_NAMES=$(python3 -c "
import json
t = json.load(open('$ROOT_DIR/tokens.json'))
print(' '.join(t['icon']['names']))
")
for n in $ICON_NAMES; do
  curl -s --max-time 30 -o "$VENDOR_DIR/lucide-svg/${n}.svg" "$LUCIDE_RAW/icons/${n}.svg"
done
curl -s --max-time 30 -o "$VENDOR_DIR/lucide-svg/LICENSE" "$LUCIDE_RAW/LICENSE"

echo "==> Listo. Ejecuta design-system/generate.py para producir los assets del firmware."
