# Guía de flasheo y restauración — iPod Classic 6G

Detalle técnico de qué hace el instalador de Aura Studio y cómo volver atrás. Para el paso a paso normal de instalación, ver la [guía de instalación](guia-instalacion.md) — este documento es para cuando algo no sale como se espera, o para quien quiera entender/hacerlo a mano por línea de comandos.

## Qué instala Aura, exactamente

Aura usa un **bootloader dual-boot** (`bootloader/ipod-s5l87xx.c` en el fork, compilado en `firmware/dist/bootloader-ipod6g.ipod`) que reemplaza el arranque de NOR del iPod, pero **no borra ni toca el firmware original de Apple** — decide a cuál arrancar según qué botones mantengas presionados en el momento del arranque:

| Combinación | Resultado |
|---|---|
| (nada) | Arranca Aura |
| MENU (mantenido) | Arranca el firmware original de Apple |
| SELECT + MENU (~5s, reset; +8s más → modo DFU) | Reinicia / entra a modo DFU |
| SELECT + LEFT | Diagnósticos de Apple |
| SELECT + PLAY | Modo disco de Apple |
| SELECT + RIGHT | Modo USB del propio bootloader |
| Hold activado al encender | Firmware original de Apple |

Esto es el comportamiento estándar de los bootloaders duales de Rockbox para iPod (ver el manual de Rockbox, sección "Dual boot") — Aura no lo reinventa, lo hereda del fork.

## Instalar/reinstalar por línea de comandos (sin Aura Studio)

Si preferís no usar la app, `mks5lboot` (compilado en `firmware/dist/mks5lboot`, o compilable con `cd firmware/rockbox/utils/mks5lboot && make`) es la misma herramienta que usa Aura Studio por dentro:

```bash
# 1. Verificar checksums antes de instalar nada
shasum -a 256 -c firmware/dist/checksums.txt

# 2. Poner el iPod en modo DFU: mantené SELECT+MENU ~12 segundos
#    hasta despues de que la pantalla se ponga negra, y soltalos.
#    Podés confirmar que entró en DFU con:
firmware/dist/mks5lboot --dfuscan --loop

# 3. Instalar el bootloader Aura (dual-boot, conserva Apple):
firmware/dist/mks5lboot --bl-inst firmware/dist/bootloader-ipod6g.ipod

# 4. Copiar el firmware al iPod (una vez montado normalmente, no en DFU):
cp firmware/dist/rockbox.ipod /Volumes/TU_IPOD/.rockbox_boot   # ver nota abajo
```

> **Nota sobre el paso 4**: el flujo estándar de Rockbox espera `rockbox.ipod` en la raíz del volumen del iPod junto a una carpeta `.rockbox/` con el resto del firmware (fuentes, temas, plugins) — eso es lo que genera `make install` dentro del árbol de build (`firmware/build-ipod6g/`), no solo el archivo `.ipod` suelto. Si copiás `firmware/dist/rockbox.ipod` a mano, copiá también la carpeta `.rockbox/` completa que `make install` deja en `firmware/build-ipod6g/`. Aura Studio hace esto por vos.

**Modo single-boot** (destruye el arranque original de Apple, no recomendado salvo que estés seguro): agregá `--single` al comando de `--bl-inst`. Aura Studio lo ofrece como casillero explícito, sin marcar por defecto.

## Restaurar el iPod original

Con Aura Studio: pestaña **Instalador** → **Restaurar iPod original**, mismo asistente guiado (detección, modo DFU) pero terminando en `mks5lboot --bl-uninst ipod6g` en vez de instalar.

Por línea de comandos:

```bash
firmware/dist/mks5lboot --bl-uninst ipod6g
```

Esto quita el bootloader de Aura — tu iPod vuelve a arrancar directo al firmware original de Apple, como si nunca hubieras instalado Aura. La música/fotos/videos que sincronizaste con Aura Studio **no se borran solas** del disco (quedan en las carpetas `Music/`, `Videos/`, `Photos/` del volumen) — si querés espacio de vuelta, borralas a mano desde Finder.

## Problemas comunes

- **"No se detecta el iPod en modo DFU"**: la combinación de botones es sensible al timing — soltar antes de tiempo (antes de los ~12 segundos y de que la pantalla se ponga negra) no llega a activar el DFU. Reintentá desde cero (desconectar, reconectar, repetir la combinación completa).
- **"El iPod no está formateado en FAT32"**: Aura (como todo Rockbox) necesita FAT32, no HFS+/APFS ni el formato de partición propietario de Apple viejo. Aura Studio te avisa en el paso de detección; convertirlo requiere reformatear el disco (que borra su contenido — hacé backup de lo que tengas ahí primero) desde Utilidad de Discos o `diskutil`.
- **"El instalador dice que un checksum no coincide"**: no continúes — probablemente los archivos de `firmware/dist/` se corrompieron al descargar/copiar. Recompilá desde cero siguiendo la [guía de desarrollo](guia-desarrollo.md#firmware--target-real-ipod6g).
- **Si algo sale mal a mitad del flasheo del bootloader**: el propio `mks5lboot`/bootloader tiene su propio manejo de errores por sonido (ver su README en `firmware/rockbox/utils/mks5lboot/README`) — un tono grave repetido indica que el NOR se corrompió y hace falta restaurar vía iTunes/Finder (modo de recuperación de Apple). Esto es infraestructura de Rockbox, no específica de Aura, y es la misma que usan miles de instalaciones de Rockbox en iPods S5L desde hace años.

## Qué está verificado y qué no (a la fecha de esta guía)

Ver la sección correspondiente en el resumen final del proyecto (README.md / mensaje de cierre de la sesión de desarrollo) para el detalle completo. En resumen: el firmware y el bootloader compilan limpio para hardware real y sus checksums están verificados; lo que **no** se pudo verificar en esta sesión, por no haber un iPod físico conectado, es el propio proceso de flasheo/arranque en un dispositivo real.
