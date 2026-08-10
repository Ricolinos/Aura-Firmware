# Guía de desarrollo — Aura

Cómo compilar y trabajar en cada parte del proyecto. Todos los comandos asumen que estás parado en la raíz del repo (`Aura-Proyect/`) en un Mac con Apple Silicon.

## Requisitos (una sola vez)

```bash
brew install sdl2 gcc freetype librsvg coreutils gnu-sed make texinfo automake autoconf xcodegen ffmpeg
python3 -m venv design-system/.venv
design-system/.venv/bin/pip install pillow
```

`tools/configure` de Rockbox detecta solo el gcc real de Homebrew más nuevo disponible (no el `clang` que Xcode expone como `gcc`) — ver D-007 en DECISIONS.md.

## Firmware — simulador SDL (día a día)

El simulador es el banco de pruebas principal de toda la UI — no hace falta el iPod físico para desarrollar ni probar Aura UI.

```bash
firmware/tools/build_sim.sh              # configura (primera vez) y compila
firmware/tools/build_sim.sh --run        # compila y lo abre
firmware/tools/gen_test_media.sh         # genera música/fotos/video de prueba en el disco simulado
```

Capturas headless (sin permisos de Accesibilidad de macOS, ver D-008/D-017 en DECISIONS.md):

```bash
firmware/tools/sim_screenshot.sh salida.png [ticks] ["SELECT,SCROLL_FWD,..."]
firmware/tools/capture_matrix.sh         # matriz completa: cada pantalla x 2 temas x 3 modos
```

Tests de la lógica pura (máquina de navegación, parser `.lrc`):

```bash
make -C firmware/rockbox/apps/aura/test test
```

## Firmware — target real `ipod6g`

Requiere el toolchain ARM (`binutils` 2.38 + `gcc` 9.5.0 para `arm-elf-eabi`), compilado una sola vez con el propio script de Rockbox — ver D-032 en DECISIONS.md:

```bash
export RBDEV_PREFIX="$PWD/firmware/toolchain"
export RBDEV_TARGET="a"
export RBDEV_DOWNLOAD="$PWD/firmware/toolchain/dl"
export RBDEV_BUILD="$PWD/firmware/toolchain/build-tmp"
bash firmware/rockbox/tools/rockboxdev.sh
```

Con el toolchain listo, compilar firmware y bootloader (dos árboles de build separados):

```bash
mkdir -p firmware/build-ipod6g && cd firmware/build-ipod6g
PATH="$PWD/../toolchain/bin:$PATH" ../rockbox/tools/configure --target=ipod6g --type=N
PATH="$PWD/../toolchain/bin:$PATH" make -j"$(sysctl -n hw.ncpu)"
# → rockbox.ipod

cd .. && mkdir -p build-ipod6g-boot && cd build-ipod6g-boot
PATH="$PWD/../toolchain/bin:$PATH" ../rockbox/tools/configure --target=ipod6g --type=B
PATH="$PWD/../toolchain/bin:$PATH" make -j"$(sysctl -n hw.ncpu)"
# → bootloader-ipod6g.ipod
```

La herramienta de flasheo DFU (`mks5lboot`) es un binario de host, se compila aparte:

```bash
cd firmware/rockbox/utils/mks5lboot && make
```

`firmware/dist/` ya tiene los tres artefactos compilados y verificados en esta sesión (más `checksums.txt`), listos para usar sin recompilar nada.

## Design system

Una sola fuente de verdad (`design-system/tokens.json`) genera todo lo demás — nunca se edita a mano el header C ni los bitmaps generados.

```bash
design-system/.venv/bin/python3 design-system/generate.py
```

Produce `design-system/out/aura_tokens.h` (que `build_sim.sh` copia a `firmware/rockbox/apps/aura/`), fuentes bitmap y bitmaps de íconos para ambos temas.

## Aura Studio (macOS)

El proyecto real es `studio/AuraStudio/AuraStudio.xcodeproj`, generado desde `studio/AuraStudio/project.yml` con [XcodeGen](https://github.com/yonaskolb/XcodeGen) — nunca se edita el `.xcodeproj` a mano, se edita el `.yml` y se regenera:

```bash
cd studio/AuraStudio && xcodegen generate
open AuraStudio.xcodeproj
```

Build/test por línea de comandos con Xcode (requiere que Xcode tenga todos sus componentes de plataforma instalados — abrilo una vez con la interfaz gráfica si es la primera vez que lo usás en esta máquina):

```bash
xcodebuild -project AuraStudio.xcodeproj -scheme AuraStudio build
xcodebuild -project AuraStudio.xcodeproj -scheme AuraStudio test
```

Camino alternativo, más rápido, vía Swift Package Manager (compila el mismo código fuente pero no genera el `.app` con recursos embebidos — ver D-034 en DECISIONS.md):

```bash
cd studio/AuraStudio
swift build
swift test          # 30 tests, incluye 4 que pegan contra APIs reales (MusicBrainz/CAA/LRCLIB) y se saltean solos sin red
```

## Estructura del repo

```
firmware/rockbox/apps/aura/   → la UI Aura (toda la lógica nueva; el resto de firmware/rockbox/ es el fork sin modificar salvo los parches puntuales documentados en DECISIONS.md)
firmware/tools/               → scripts de build/test/capturas
firmware/dist/                → artefactos compilados del target real (versionado)
design-system/                → tokens + pipeline de generación
studio/AuraStudio/            → app macOS (Swift/SwiftUI)
docs/                         → esta guía + guía de usuario + guía de flasheo + capturas
```

Ver [DECISIONS.md](../DECISIONS.md) para el porqué de cada decisión no obvia (son más de 40 entradas D-NNN, cada una con el problema real encontrado y la alternativa implementada).
