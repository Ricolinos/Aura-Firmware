# DECISIONS.md — Registro de decisiones técnicas

Formato: cada decisión con contexto, alternativas consideradas y justificación. Orden cronológico.

---

## D-001 — Rockbox como capa de hardware, no firmware desde cero

**Decisión de partida (no negociable, del brief del proyecto).** Fork de Rockbox aportando kernel, drivers (ATA, LCD, clickwheel, audio, USB, batería), sistema de archivos, códecs y motor de reproducción. La UI de Rockbox (menús, WPS, temas, plugins de usuario) se elimina/desactiva y se reemplaza por la capa propia "Aura UI". El firmware resultante es GPL (obligación heredada de Rockbox).

## D-002 — Fork de Rockbox in-repo, no como submódulo

El código de Rockbox se clona dentro de `/firmware/rockbox` y se versiona como parte de este mismo repositorio (historial propio de Rockbox eliminado, `.git` interno descartado). Motivo: los cambios de Aura tocan el árbol de Rockbox en profundidad (apps/, firmware/, tools/) y un submódulo obligaría a mantener un fork remoto separado; in-repo simplifica clone-and-build.

- **Origen**: espejo oficial `https://github.com/Rockbox/rockbox` (espejo de git.rockbox.org).
- **Commit base**: `0726ec93517a61f602679ab052b083217ec9c96d` (2026-08-09, "FS#13978 - rbutil: Improve GUI responsiveness during voice/talk file generation"). Clonado con `--depth 1`; el `.git` interno se eliminó antes del primer commit para versionar el árbol dentro de este repositorio.

## D-007 — Parche de `tools/configure`: detección de GCC en macOS

El `configure` de Rockbox fuerza `CC=gcc-16` en Darwin (con un comentario `FIXME` sobre `sigaltstack` fallando en macOS ≥ 26.4, motivo por el cual el simulador usa hilos SDL en vez de ucontext). En este equipo Homebrew solo tiene `gcc-15` instalado. Se parcheó `tools/configure` (`firmware/rockbox/tools/configure`, bloque `Darwin)`) para probar `gcc-16`, `gcc-15`, `gcc-14`, `gcc-13` en orden y usar el primero disponible, en vez de fallar con una versión fija. Sigue siendo GCC real de Homebrew (no clang) porque el código de Rockbox depende de extensiones/builtins de GCC no soportadas por el clang de Xcode en este contexto.

## D-008 — Auto-screendump del simulador para evidencia headless

macOS moderno requiere permiso de "Grabación de Pantalla" para `screencapture`/`osascript System Events`, y esta sesión (agente en background, sin interacción del usuario) no puede concederlo interactivamente. Rockbox ya trae `screen_dump()` (`firmware/screendump.c`), que vuelca el framebuffer a un `.bmp` en el disco simulado, normalmente disparado con la tecla F5 dentro de la ventana SDL — inviable de automatizar sin inyección de eventos de teclado a nivel de SO.

**Alternativa implementada**: se añadió a `uisimulator/common/sim_tasks.c` un disparador por variables de entorno — `AURA_SIM_AUTODUMP_TICKS=<n>` programa un `screen_dump()` automático n ticks después del arranque, y `AURA_SIM_AUTODUMP_QUIT=1` cierra el proceso justo después. Envuelto en `firmware/tools/sim_screenshot.sh`. Este mecanismo es además la base de la matriz de capturas (pantalla × tema × modo) requerida en la Fase 7.

## D-003 — Simulador SDL como banco de pruebas principal de la UI

Toda la capa Aura UI se desarrolla y verifica primero contra el simulador SDL de Rockbox en macOS (Fase A del brief), con capturas automatizadas como evidencia. El target ARM real (ipod6g) se compila en la Fase 8. Motivo: ciclo de iteración de segundos vs. minutos, y verificación visual sin hardware.

## D-004 — Tipografía e íconos

- **Inter** (SIL Open Font License) rasterizada a fuentes bitmap en los tamaños exactos de uso — en un LCD 320×240 el render bitmap es más nítido y órdenes de magnitud más barato que el vectorial.
- **Lucide** (licencia ISC) como set de íconos, trazo fino, exportados a bitmap a tamaños exactos.
- **Prohibido**: SF Pro y SF Symbols en el firmware (la licencia de Apple prohíbe su redistribución fuera de plataformas Apple). En Aura Studio (app que corre en macOS) sí se usan SF Symbols, lo cual está permitido.

## D-005 — Aura Studio nativa en Swift + SwiftUI

Sin Electron. Motivos: coherencia estética con macOS, bajo consumo, acceso directo a IOKit/DiskArbitration para el instalador. Target: Apple Silicon.

## D-006 — Formato de video interno: MPEG-1/2 a 320×240

El S5L8702 no puede decodificar H.264/VP9 por software a framerate útil. Rockbox ya trae un decodificador MPEG-1/2 (libmpeg2) probado en este hardware. Aura Studio transcodifica cualquier formato de entrada a este formato interno con ffmpeg; el firmware solo reproduce ese formato.

## D-009 — Fuentes: `convttf` del propio fork, TTF → `.fnt` directo

Rockbox trae `tools/convttf.c`, que rasteriza un TTF a su formato binario nativo `.fnt` en un único paso (`-p <tamaño-px>`), sin pasar por BDF intermedio. Se usa directamente sobre los tres pesos de Inter (Regular/Medium/SemiBold) a los tamaños exactos de `type_scale` (20/14/11/9 px). Requiere `freetype2` (`brew install freetype`, ya presente). El binario compilado (`firmware/rockbox/tools/convttf`) no se versiona — ya está cubierto por el `.gitignore` propio de Rockbox.

## D-010 — Iconos: color pre-compuesto en tiempo de generación, no en runtime

Los SVG de Lucide usan `stroke="currentColor"`; el generador fija `color="<text_primary del tema>"` en el propio SVG antes de rasterizar con `rsvg-convert`, y compone el resultado sobre un fondo sólido (`background` del tema) al convertir a BMP top-down de 24 bits con `sips`. Se comprobó que el loader de Rockbox (`apps/recorder/bmp.c`) soporta BMP top-down (altura negativa). Se generan dos sets completos de bitmaps — uno por tema — en vez de un único set monocromo tintado en runtime, porque el loader de bitmaps de Rockbox no compone alpha en tiempo de dibujo; es el mismo enfoque que ya usan los propios iconos "tango" de Rockbox. Si en la Fase 3 surge la necesidad de iconos con fondo transparente (p. ej. superpuestos a una carátula), se revisará esta decisión.

## D-011 — Salida generada (`design-system/out/`) no se versiona

`tokens.json`, `vendor/` (fuentes Inter y SVG de Lucide descargados, ~1.3 MB) y los scripts se versionan porque son la fuente de verdad. `out/` (headers, `.fnt`, `.bmp`) se excluye del repo vía `.gitignore`: es 100 % derivable de lo anterior con `python3 design-system/generate.py` (determinista, ~segundos), y `firmware/tools/build_sim.sh` lo regenera automáticamente antes de cada build del simulador.

---

*(Las siguientes decisiones se añaden conforme avanza la ejecución.)*
