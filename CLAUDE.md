## Sistema de diseño — Apple2026 / Aura

Antes de crear, modificar o revisar cualquier pantalla, animación o componente visual, consulta la skill `apple2026-design-system`. No asumas valores de color, tipografía, geometría o movimiento: la **fuente de verdad viva es `docs/aura-design-system/`** (mapa en su `00-INDICE.md`); `docs/design/` es la base histórica original — consúltala solo para lo que el sistema vivo aún no cubra, y ante cualquier conflicto **gana `docs/aura-design-system/`**.

Reglas que aplican siempre, sin excepción, trabajes en lo que trabajes:
- Ningún color RGB hardcodeado en C — todo sale de `a26_palette` (`apps/apple2026_shell.h`).
- Cero cromo de Rockbox visible en cualquier estado (logotipos, nombres crudos de directorio, jerga técnica).
- Todo texto de cara al usuario en español natural, añadido al final de ambos `.lang`.
- Toda animación respeta la puerta `lcd_active()`.
- El "vidrio" (translucidez) vive solo en la capa de controles (barra de estado, pastillas) — nunca en contenido.

Si encuentras algo que no respeta estas reglas o las de `docs/design/`, ajústalo sin preguntar — salvo que el propio documento marque la decisión como abierta.
