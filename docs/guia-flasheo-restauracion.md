# Guía de flasheo y restauración — iPod Classic 6G

Detalle técnico de qué instala Aura en el iPod y cómo funciona el arranque dual — para entender qué pasa por dentro del dispositivo, no cómo usar la app. El paso a paso de la instalación (Aura Studio) vive en el repositorio aparte de Aura Studio — este documento no incluye comandos para correr a mano ni pasos de una interfaz que no está en este repositorio.

## Qué instala Aura, exactamente

Aura usa un **bootloader dual-boot** (`bootloader/ipod-s5l87xx.c` en el fork, compilado en `firmware/dist/bootloader-ipod6g.ipod`) que reemplaza el arranque de NOR del iPod, pero **no borra ni toca el firmware original de Apple** — decide a cuál arrancar según qué botones mantengas presionados en el momento del arranque:

| Combinación | Resultado |
|---|---|
| (nada) | Arranca Aura |
| MENU (mantenido) | Arranca el firmware original de Apple |
| SELECT + MENU (~5s, reset; +8s más → modo DFU) | Reinicia / entra a modo DFU |
| SELECT + LEFT | Diagnósticos de Apple |
| SELECT + PLAY | Modo disco de Apple |
| SELECT + RIGHT | Modo Bootloader USB |
| Hold activado al encender | Firmware original de Apple |

Esto es el comportamiento estándar de los bootloaders duales de Rockbox para iPod (ver el manual de Rockbox, sección "Dual boot") — Aura no lo reinventa, lo hereda del fork. La tabla de combinaciones de botones **no cambió** con la Fase 22 (PLAN-UX.md): lo único que cambió es que el arranque normal (primera fila, "Arranca Aura") ya no muestra el texto "Rockbox boot loader" / "Version: ..." en pantalla — queda en negro hasta que aparece el logo de Aura. Los mensajes de error real (falla de almacenamiento, batería crítica, `Hold MENU+SELECT to reboot`, etc.) **se siguen mostrando igual que antes**, sin ningún cambio — solo se silenció el camino feliz. Sin verificar todavía contra hardware real (ver la última sección de esta guía): si algo se ve distinto a lo esperado durante una sesión de flasheo, es la primera señal a reportar.

## Qué hace el instalador, en términos del protocolo del dispositivo

El instalador (Aura Studio, repositorio aparte) habla con el iPod por dos canales, en este orden: (1) modo DFU vía `mks5lboot --bl-inst`, que no requiere privilegios elevados en macOS, para escribir el bootloader; (2) modo Bootloader USB (disco montado en FAT32) para copiar `rockbox.ipod`/el árbol `.rockbox/`, verificando checksums (SHA-256) antes de escribir nada y reverificando la identidad del disco justo antes de cualquier operación destructiva. El detalle de la experiencia de usuario (pantallas, diálogos, textos) vive en el repositorio de Aura Studio, no aquí.

**Modo single-boot** (destruye el arranque original de Apple, no recomendado salvo estar seguro): lo ofrece el instalador como opción explícita, sin marcar por defecto — a nivel de protocolo es la misma escritura de bootloader, sin la rama de "MENU mantenido → Apple".

## Restaurar el iPod original

El instalador puede desinstalar el bootloader en vez de instalarlo (mismo flujo de detección/DFU, en reversa): el iPod vuelve a arrancar directo al firmware original de Apple, como si Aura nunca se hubiera instalado. La música/fotos/videos sincronizados **no se borran solas** del disco (quedan en `Music/`, `Videos/`, `Photos/` del volumen).

## Problemas comunes (nivel dispositivo/bootloader)

- **"No se detecta el iPod en modo DFU"**: la combinación de botones es sensible al timing — soltar antes de tiempo (antes de los ~12 segundos y de que la pantalla se ponga negra) no llega a activar el DFU. Reintenta desde cero (desconectar, reconectar, repetir la combinación completa).
- **"El iPod no está formateado en FAT32"**: Aura (como todo Rockbox) necesita FAT32, no HFS+/APFS ni el formato de partición propietario de Apple viejo.
- **"El instalador dice que un checksum no coincide"**: no continúes — probablemente los archivos descargados del Release se corrompieron. Vuelve a descargarlos, o recompila desde cero siguiendo la [guía de desarrollo](guia-desarrollo.md#firmware--target-real-ipod6g).
- **Si algo sale mal a mitad del flasheo del bootloader**: el propio `mks5lboot`/bootloader tiene su propio manejo de errores por sonido — un tono grave repetido indica que el NOR se corrompió y hace falta restaurar vía iTunes/Finder (modo de recuperación de Apple). Esto es infraestructura de Rockbox, no específica de Aura, y es la misma que usan miles de instalaciones de Rockbox en iPods S5L desde hace años.

Para problemas de la app en sí (diálogos, permisos, detección de disco), ver la documentación del repositorio de Aura Studio.

## Qué está verificado y qué no (a la fecha de esta guía)

El firmware y el bootloader compilan limpio para hardware real y sus checksums están verificados. El flujo de instalador nativo (autorización de administrador dentro de la app, identificación de disco, formateo) fue implementado y probado con tests unitarios, pero el flujo de **formateo de disco de punta a punta contra un iPod real** queda pendiente de una sesión de prueba guiada — ver el resumen de la sesión de desarrollo para el detalle completo de qué se verificó ya contra hardware real (bootloader, arranque de Aura, detección de modo USB) y qué queda para la próxima prueba.
