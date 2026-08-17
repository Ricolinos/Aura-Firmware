# Aura Firmware

Firmware moderno y minimalista para el iPod Classic de 6ª generación
(2008), con una interfaz propia ("Aura UI") construida sobre
[Rockbox](https://www.rockbox.org/).

Este repositorio es solo el firmware. La app de escritorio para
instalarlo y gestionar la biblioteca (Aura Studio, macOS) vive en un
repositorio aparte.

## Origen y licencia

Aura es un **fork de Rockbox**, software libre bajo la
[GNU General Public License v2](LICENSE). El árbol completo de
Rockbox se importó en `firmware/rockbox/` desde el espejo oficial
[github.com/Rockbox/rockbox](https://github.com/Rockbox/rockbox) en el
commit `0726ec93517a61f602679ab052b083217ec9c96d` (2026-08-09).

El kernel, los drivers (ATA, LCD, clickwheel, audio, USB, batería), el
sistema de archivos, los códecs y el motor de reproducción son obra
del equipo de Rockbox y sus colaboradores — todo el crédito por ese
trabajo es suyo. Aura reemplaza la UI de Rockbox (menús, WPS, temas,
plugins de usuario) por una capa propia, `apps/aura/`, y toca otros 20
archivos del árbol original para conectarla — ver
[`MODIFICATIONS.md`](MODIFICATIONS.md) para el listado exacto y
[`DECISIONS.md`](DECISIONS.md) para el porqué de cada cambio, entrada
por entrada (más de 280, en orden cronológico, incluidas las que se
revirtieron después).

Al ser un derivado de software GPL v2, **este repositorio completo se
distribuye bajo esa misma licencia** — texto íntegro en
[`LICENSE`](LICENSE) (copia literal de
`firmware/rockbox/docs/COPYING`).

## Sobre el hardware

Aura corre en el iPod Classic 6G. iPod, iPod Classic y Apple son
marcas registradas de Apple Inc. — esto es una mención factual del
hardware objetivo, no una afirmación de afiliación: **este proyecto no
está afiliado con, patrocinado por, ni respaldado por Apple Inc.**, y
no incluye ni redistribuye ningún asset, fuente o software propiedad
de Apple (ver "Tipografía e iconografía" abajo).

## Estado del proyecto

Firmware funcional, verificado en el simulador SDL (banco de pruebas
principal durante el desarrollo — ver `DECISIONS.md`, D-003) y
compilado para el target real `ipod6g`. Sin verificación exhaustiva en
un lote amplio de hardware físico — probado por el autor en su propio
dispositivo. Consulta `DECISIONS.md` para el historial completo,
incluidos los bugs reales encontrados y cómo se resolvieron.

## Compilar

Requisitos (macOS, Apple Silicon):

```bash
brew install sdl2 gcc freetype librsvg coreutils gnu-sed make texinfo automake autoconf ffmpeg
python3 -m venv design-system/.venv
design-system/.venv/bin/pip install pillow
```

**Simulador SDL** (día a día, no requiere el iPod físico):

```bash
firmware/tools/build_sim.sh --run
make -C firmware/rockbox/apps/aura/test test   # tests de lógica pura
```

**Target real `ipod6g`** (requiere el toolchain ARM — compílalo una
sola vez con el propio script de Rockbox):

```bash
export RBDEV_PREFIX="$PWD/firmware/toolchain"
export RBDEV_TARGET="a"
export RBDEV_DOWNLOAD="$PWD/firmware/toolchain/dl"
export RBDEV_BUILD="$PWD/firmware/toolchain/build-tmp"
bash firmware/rockbox/tools/rockboxdev.sh
```

Con el toolchain listo:

```bash
firmware/tools/package_dist.sh
```

Compila el firmware ARM y `mks5lboot` (herramienta de flasheo DFU), y
arma `firmware/dist/{rockbox.ipod, rockbox.zip, mks5lboot,
checksums.txt}` — ver [`firmware/dist/README.md`](firmware/dist/README.md).
El bootloader (`bootloader-ipod6g.ipod`) requiere un segundo toolchain
(`--type=B`) que el script no configura automáticamente; imprime las
instrucciones exactas si falta. Los artefactos empaquetados no se
versionan en git (se regeneran con este script y se publican como
GitHub Release) — ver el detalle de por qué en `.gitignore`.

Guía completa, con el resto de comandos día a día (capturas headless,
matriz de pantallas, generación del design system): 
[`docs/guia-desarrollo.md`](docs/guia-desarrollo.md).

## Tipografía e iconografía

El tema compilado por defecto usa **[Inter](https://rsms.me/inter/)**
(SIL Open Font License) para tipografía y **[Lucide](https://lucide.dev/)**
(licencia ISC) para iconografía, con
**[Phosphor](https://phosphoricons.com/)** (MIT) como set secundario
para los pocos íconos sin equivalente directo en Lucide — ver
`DECISIONS.md`, D-286. Ninguna fuente ni ícono de Apple se compila ni
se redistribuye en este repositorio.

Existe además un mecanismo, documentado pero **no incluido aquí**,
para construir localmente un tema opcional "Apple (uso personal)" que
usa SF Pro/SF Symbols ya instalados en la Mac de quien lo construye —
nunca redistribuidos, por respeto a la licencia de Apple. Ese
constructor (`design-system/scripts/apple2026_sf_render.swift`) se
conserva en el árbol como código, sin ejecutarse en ningún build de
este repositorio; el mecanismo completo para invocarlo vive en Aura
Studio (repositorio aparte).

## Estructura

| Directorio | Qué es |
|---|---|
| `firmware/` | El firmware: `firmware/rockbox/` (fork de Rockbox), `apps/aura/` dentro de él (la UI propia), `firmware/tools/` (scripts de build/empaquetado), `firmware/dist/` (artefactos compilados, no versionados). |
| `design-system/` | Fuente única de verdad de tokens visuales (`tokens.json`) y el pipeline que genera fuentes bitmap e íconos a partir de ella. |
| `docs/` | Sistema de diseño vivo (`docs/aura-design-system/`), guías de instalación/desarrollo/flasheo, capturas de evidencia. |

## Documentos clave

- [`DECISIONS.md`](DECISIONS.md) — registro cronológico de decisiones técnicas, con el problema real encontrado y la alternativa implementada en cada una.
- [`MODIFICATIONS.md`](MODIFICATIONS.md) — qué modificó Aura sobre el Rockbox original, archivo por archivo (aviso GPL v2 §2a).
- [`docs/guia-instalacion.md`](docs/guia-instalacion.md) — instalar Aura y sincronizar tu biblioteca.
- [`docs/guia-flasheo-restauracion.md`](docs/guia-flasheo-restauracion.md) — detalle técnico del flasheo, dual-boot y restauración.
- [`docs/guia-desarrollo.md`](docs/guia-desarrollo.md) — cómo compilar cada parte del proyecto.
- [`docs/aura-design-system/00-INDICE.md`](docs/aura-design-system/00-INDICE.md) — sistema de diseño vigente (fuente de verdad viva).
