## Sistema de diseño — Apple2026 / Aura

Antes de crear, modificar o revisar cualquier pantalla, animación o componente visual, consulta la skill `apple2026-design-system`. No asumas valores de color, tipografía, geometría o movimiento: la **fuente de verdad viva es `docs/aura-design-system/`** (mapa en su `00-INDICE.md`); `docs/design/` es la base histórica original — consúltala solo para lo que el sistema vivo aún no cubra, y ante cualquier conflicto **gana `docs/aura-design-system/`**.

Reglas que aplican siempre, sin excepción, trabajes en lo que trabajes:
- Ningún color RGB hardcodeado en C — todo sale de `a26_palette` (`firmware/rockbox/apps/aura/apple2026_shell.h`).
- Cero cromo de Rockbox visible en cualquier estado (logotipos, nombres crudos de directorio, jerga técnica).
- Todo texto de cara al usuario en español natural, añadido al final de ambos `.lang`.
- Toda animación respeta la puerta `lcd_active()`.
- El "vidrio" (translucidez) vive solo en la capa de controles (barra de estado, pastillas) — nunca en contenido.
- Todo texto de cara al usuario y toda documentación en español de México, natural, sin voseo.

Si encuentras algo que no respeta estas reglas o las de `docs/design/`, ajústalo sin preguntar — salvo que el propio documento marque la decisión como abierta.

## Este repositorio es solo el firmware

Aura Studio (app macOS) vive en un repositorio aparte. El contrato entre ambos está en `CONTRATO-firmware-studio.md` — léelo antes de tocar cualquier cosa que Studio consuma.

- Ningún script, `Makefile` ni generador de este repo asume la existencia de un checkout hermano de Aura Studio (`../studio` o similar) ni escribe fuera de este árbol. Lo que Studio necesita (binarios, `AuraPalette.swift`, `MODIFICATIONS.md`) sale por **GitHub Release**, nunca por ruta relativa — es la clase de acoplamiento que ya causó un bug real (`generate.py` creaba un `studio/` huérfano en checkouts sin Studio al lado).
- GPL v2: todo archivo `.c`/`.h` nuevo en `apps/aura/` lleva la cabecera de licencia; todo cambio a un archivo de Rockbox fuera de `apps/aura/` se registra en `MODIFICATIONS.md` en la misma pasada.
- Sin material de Apple en el árbol ni en `firmware/dist/`: nada de SF Pro, SF Symbols, ni sus derivados (`.fnt`, BMP horneados). El tema Apple es un constructor local fuera de este repo (`PLAN-theme-system.md`), nunca parte del build público.
- `DECISIONS-ARCHIVE.md` es histórico, de solo lectura (D-001…D-285). Las decisiones nuevas van a `DECISIONS.md` con numeración `D-286+`; una referencia a una decisión de Aura Studio se escribe `ST-NNN`.
