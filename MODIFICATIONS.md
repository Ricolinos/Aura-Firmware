# MODIFICATIONS.md — Aviso de modificaciones (GPL v2 §2a)

Este archivo cumple el requisito de la GPL v2 §2(a) de hacer constar,
de forma destacada, que este software fue modificado y la fecha de la
modificación.

## Origen

Este repositorio es un fork de [Rockbox](https://www.rockbox.org/)
([rockbox.org](https://www.rockbox.org/), espejo
[github.com/Rockbox/rockbox](https://github.com/Rockbox/rockbox)),
software libre bajo GPL v2. Rockbox se importó a `firmware/rockbox/`
en el commit base
**`0726ec93517a61f602679ab052b083217ec9c96d`** (2026-08-09), clonado
con `--depth 1` y sin el `.git` interno de Rockbox (se versiona dentro
del `.git` de este repositorio; ver `DECISIONS.md`, D-002).

Las modificaciones sobre ese código base ocurrieron entre 2026-08-09 y
2026-08-16 (fecha de esta nota). El detalle decisión por decisión de
todos los cambios está en `DECISIONS.md`.

## `apps/aura/` — código nuevo, no una modificación

Todo el árbol `firmware/rockbox/apps/aura/` (83 archivos `.c`/`.h`) es
código **nuevo**, escrito para este proyecto — no es una modificación
de un archivo preexistente de Rockbox. Cada archivo lleva su propia
cabecera de copyright GPL v2 (ver más abajo). Es el reemplazo de la UI
de Rockbox (menús, WPS) por la capa "Aura UI", conectado al resto del
árbol mediante los 20 archivos listados abajo.

## Los 20 archivos de Rockbox modificados fuera de `apps/aura/`

Todos conservan su cabecera de copyright original de Rockbox intacta.
Los cambios de Aura están marcados inline en el propio código con
comentarios `Aura` / `D-xxx` (referencia a la entrada correspondiente
de `DECISIONS.md`, donde está el detalle completo de cada cambio):

- `apps/SOURCES`
- `apps/bitmaps/native/rockboxlogo.320x98x16.bmp`
- `apps/bitmaps/native/usblogo.176x48x16.bmp`
- `apps/gui/splash.c`
- `apps/gui/usb_screen.c`
- `apps/main.c`
- `apps/misc.c`
- `apps/misc.h`
- `apps/plugins/mpegplayer/mpeg_settings.c`
- `apps/plugins/mpegplayer/mpegplayer.c`
- `apps/plugins/solitaire.c`
- `apps/settings.h`
- `apps/tagcache.c`
- `bootloader/ipod-s5l87xx.c`
- `firmware/export/config/ipod6g.h`
- `firmware/export/font.h`
- `firmware/target/hosted/filesystem-unix.c`
- `lib/rbcodec/codecs/aiff.c`
- `uisimulator/common/sim_tasks.c`
- `utils/mks5lboot/Makefile`

(Rutas relativas a `firmware/rockbox/`.)

De estos 20, dos no tenían ningún comentario que mencionara a Aura:

- **`utils/mks5lboot/Makefile`**: sí tiene una modificación real vigente
  (backend libusb opcional en macOS, D-050, 2026-08-10) — recibió un
  aviso de modificación explícito en esta misma revisión (ver
  `DECISIONS.md`, D-286).
- **`firmware/target/hosted/filesystem-unix.c`**: el único cambio de
  Aura sobre este archivo (D-284, perfil de disco simulado) fue
  **revertido íntegramente por D-285 el mismo día** ("vuelve
  exactamente al estado previo a D-284", ver `DECISIONS.md`). Verificado
  al escribir esta nota (2026-08-16): el archivo es hoy **byte-idéntico**
  al import original de Rockbox — no tiene ninguna modificación de Aura
  vigente, así que no se agregó ningún aviso en su cabecera (agregar
  uno habría sido un aviso de un cambio que ya no existe en el código
  distribuido). El historial completo del intento y su reversión queda
  igualmente documentado en `DECISIONS.md` D-284/D-285 y en `git log`.

## Texto completo de la licencia

`firmware/rockbox/docs/COPYING` (GPL v2 íntegra) y `LICENSE` en la
raíz de este repositorio (copia literal de esa misma licencia, con una
nota de contexto breve antepuesta).
