# Guía de flasheo y restauración — iPod Classic 6G

Detalle técnico de qué hace el instalador de Aura Studio y cómo volver atrás. Para el paso a paso normal, ver la [guía de instalación](guia-instalacion.md) — este documento es para cuando algo no sale como se espera, o para entender qué pasa por dentro. El flujo real de instalación/restauración es **100% dentro de Aura Studio**; este documento no incluye comandos para correr a mano.

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

## Qué hace Aura Studio por vos, paso a paso

Todo esto pasa dentro de la app (ver también la [guía de instalación](guia-instalacion.md)):

1. **Verifica checksums** (SHA-256) de los tres archivos embebidos (`rockbox.ipod`, `bootloader-ipod6g.ipod`, `mks5lboot`) antes de escribir nada.
2. **Pausa temporalmente** dos servicios de macOS (`AMPDevicesAgent`, `AMPDeviceDiscoveryAgent`) que en algunas Mac interfieren con la detección DFU — pidiendo tu contraseña de administrador, con una pantalla propia explicando por qué antes de pedirla. Se reactivan solos al terminar, o después de 10 minutos aunque algo falle a mitad de camino.
3. **Detecta el modo DFU** automáticamente y envía el bootloader (`mks5lboot --bl-inst`, sin necesitar contraseña de administrador — hablar con el iPod por USB en modo DFU no requiere privilegios elevados en macOS).
4. **Detecta el modo Bootloader USB** (tras pedirte reconectar con SELECT+RIGHT) y, si el disco ya está en FAT32 (el caso más común — cualquier iPod que ya tuvo Rockbox, o que vino así de fábrica), copia el firmware directo.
5. Si el disco necesita prepararse desde cero, **te explica el paso y pide tu contraseña** antes de formatear — nunca sin explicar antes, y solo después de reverificar la identidad del disco (fabricante, tamaño, que sea removible) en el mismo momento de ejecutar el comando, no confiando en una consulta anterior.
6. **Copia el firmware** y verifica que haya quedado bien escrito.

**Modo single-boot** (destruye el arranque original de Apple, no recomendado salvo que estés seguro): Aura Studio lo ofrece como casillero explícito en el asistente, sin marcar por defecto.

## Restaurar el iPod original

Pestaña **Instalador** → **Restaurar iPod original** — mismo asistente guiado (detección, modo DFU), pero termina desinstalando el bootloader en vez de instalarlo. Tu iPod vuelve a arrancar directo al firmware original de Apple, como si nunca hubieras instalado Aura. La música/fotos/videos que sincronizaste con Aura Studio **no se borran solas** del disco (quedan en las carpetas `Music/`, `Videos/`, `Photos/` del volumen) — si querés espacio de vuelta, borralas a mano desde Finder.

## Problemas comunes

- **"No se detecta el iPod en modo DFU"**: la combinación de botones es sensible al timing — soltar antes de tiempo (antes de los ~12 segundos y de que la pantalla se ponga negra) no llega a activar el DFU. Reintentá desde cero (desconectar, reconectar, repetir la combinación completa).
- **"El iPod no está formateado en FAT32"**: Aura (como todo Rockbox) necesita FAT32, no HFS+/APFS ni el formato de partición propietario de Apple viejo. Aura Studio te avisa en el paso de detección y lo ofrece reformatear ella misma, con tu confirmación explícita (esto borra el contenido del iPod — nada más, la app verifica su identidad antes de tocar nada).
- **"Aura Studio dice que hay más de un disco candidato"**: por seguridad, la app nunca elige "el disco más probable" cuando hay ambigüedad — desconectá los otros discos/dispositivos externos y volvé a intentar.
- **"El instalador dice que un checksum no coincide"**: no continúes — probablemente los archivos de `firmware/dist/` se corrompieron al descargar/copiar. Recompilá desde cero siguiendo la [guía de desarrollo](guia-desarrollo.md#firmware--target-real-ipod6g).
- **Si algo sale mal a mitad del flasheo del bootloader**: el propio `mks5lboot`/bootloader tiene su propio manejo de errores por sonido — un tono grave repetido indica que el NOR se corrompió y hace falta restaurar vía iTunes/Finder (modo de recuperación de Apple). Esto es infraestructura de Rockbox, no específica de Aura, y es la misma que usan miles de instalaciones de Rockbox en iPods S5L desde hace años.
- **Cancelaste un diálogo de contraseña por error**: no pasa nada — Aura Studio deja todo en el mismo estado en que estaba (servicios reactivados si se habían pausado, nada escrito en el disco si estabas en el paso de formateo) y te ofrece reintentar.

## Qué está verificado y qué no (a la fecha de esta guía)

El firmware y el bootloader compilan limpio para hardware real y sus checksums están verificados. El flujo de instalador nativo (autorización de administrador dentro de la app, identificación de disco, formateo) fue implementado y probado con tests unitarios, pero el flujo de **formateo de disco de punta a punta contra un iPod real** queda pendiente de una sesión de prueba guiada — ver el resumen de la sesión de desarrollo para el detalle completo de qué se verificó ya contra hardware real (bootloader, arranque de Aura, detección de modo USB) y qué queda para la próxima prueba.
