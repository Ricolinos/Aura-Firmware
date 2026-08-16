# RESYNC-GAPS — Huecos de implementación (tipo B)

**Fecha: 2026-08-16.** Salida de la Fase 2 del resync doc↔código (ver `RESYNC-PLAN.md`). Todo lo de aquí es **código que no alcanzó lo que la doc describe** — la doc tiene razón y NO se tocó en estas secciones. Nada de esto se arregló en esta pasada (regla: cero cambios de código). Es la lista de trabajo para una pasada de código posterior.

Priorizado por impacto visible al usuario.

## Huecos funcionales

| # | Hueco | Dónde | Evidencia en doc | Nota |
|---|---|---|---|---|
| B1 | **El título de la StatusBar nunca hace marquee** — solo se recorta | `aura_status_bar_v2.c:265-266` pasa `marquee_elapsed_ms = 0` siempre → `aura_pattern_marquee_offset(0, …)` = fase estática perpetua | `status-bar.md:198`, `dynamic-title.md:27-31` prometen MarqueeText cuando el título no cabe | Arreglo: cablear un reloj de marquee real al título (mismo patrón `s_top_since` que ya usa `aura_selection_summary.c:242-247`) |
| B2 | **DynamicTitle nunca dispara Fade-Slide ni Scroll-Slide** — el motor está completo y correcto, pero su único llamador pasa `prev_text = NULL` → siempre `AURA_TITLE_TRANSITION_NONE`. El concepto "sección alfabética" tampoco alimenta el título | `aura_status_bar_v2.c:262-266`, `aura_dynamic_title.c:35` | `dynamic-title.md:33-49` | Trabajo de conexión: pasar `prev_text` + dirección desde la navegación real y las secciones A-Z de las listas. Anotado como "follow-up, no bloqueado" en el propio código |
| B3 | **La sombra del tile de SelectionSummary ignora "Mostrar sombras"** — es la única sombra del sistema que no consulta `aura_shadows_enabled()` (LeftPanel, CoverDrift y NP Modo 4 sí lo hacen) | `aura_selection_summary.c:462`, `draw_tile_shadow()` | D-154: el toggle gobierna "todas las sombras" | Arreglo de una línea: `if (!aura_shadows_enabled()) return;` al inicio de `draw_tile_shadow()` |
| B4 | **Difuminado de 4px en los extremos de MarqueeText no implementado** — el token `MARQUEE_EDGE_BLUR = 4` existe con **cero consumidores**; `aura_marquee.c` hace recorte duro de viewport | `apple2026_tokens.h:177`, `aura_marquee.c` | `marquee-text.md:22-26`, `dynamic-title.md:27-31` | Blend de las 4 columnas extremas hacia el fondo con `a26_shell_blend()` |
| B5 | **Patrón `Shift-and-Reveal` sin implementar** — cero referencias en código; `DateEditor` entra por la transición estándar de pantalla completa | — | `transiciones/00-vocabulario.md`, `date-editor.md:11-14` | Depende de que se decida construir la coreografía de entrada de DateEditor |
| B6 | **Ícono dinámico de hoja de calendario** para la fila Fecha — solo existe la variante reloj analógico (D-108); Fecha usa el ícono estático "calendar" | `aura_screens.c:242` | `date-editor.md:9-10`, `selection-summary.md` sub-filas Hora/Fecha | Mismo mecanismo `icon_renderer` que el reloj analógico, renderer nuevo con mes/día reales |
| B7 | **Ícono de carga (spinner) del Selector** — solo `CHEVRON`/`NONE`, sin consumidor async real | `aura_selector.h:11-19` (`TODO(pendiente-doc)` sin D-XXX) | `selector.md:33-36, 55` | Explícitamente diferido: "construirlo sin un consumidor real sería adivinar su apariencia" |
| B8 | **ScrollIndicator se mueve a saltos** — `thumb_y` es proporcional a `first` (entero), sin interpolación entre posiciones | `aura_scroll_indicator.c:29-33` | `scroll-indicator.md:36-38` "se mueve animada/suave, siguiendo el scroll en tiempo real" | Menor: lerp/spring del thumb entre pasos, mismo `aura_pattern_lerp()` que el resto |
| B9 | **Editores de Fecha/Hora sobre la StatusBar vieja** — `draw_date_edit()`/`draw_time_edit()` usan `aura_widgets_draw_status_bar()`, no StatusBar v2 | `aura_screens.c` (~2822 y el equivalente de hora) | Deuda de código, no de doc | Migrar a `aura_status_bar_v2_draw()` como el resto de pantallas |
| B10 | **Escala de espaciado base** (`fundamentos/03-espaciado.md`) — hueco de DISEÑO, no de código: el código nunca la decidió, la evitó (métricas puntuales `list_inset=16`, `left_panel.padding=4`, `icon_x_from_panel_edge=14`, sin `--space-*`) | `tokens.json` | `03-espaciado.md` 🟡 | Requiere primero definir la escala en la doc, después tokenizar |

## Marcadores `TODO(pendiente-doc)` obsoletos en código

La doc ya resolvió estos, pero el marcador sigue en el código. Se limpian en una pasada de código posterior (no en esta, por la regla de cero cambios de código):

| Archivo:línea | Marcador | Por qué ya es obsoleto |
|---|---|---|
| `aura_coverflow.c:163` | `CF_SCROLL_ANIM_MS 220` "el documento no da un timing de snap" (D-103) | `cover-flow.md:106-109` ya cita 220ms por nombre; D-245/D-249 lo reutilizan como ancla del zoom |
| `aura_coverflow.c:1194` | PLAY alterna pausa/reanudar "el encargo solo define reproducir" (D-115) | `cover-flow.md:86-90` ya lo ratifica |
| `aura_menu_list.h:19` | Switch 22×12 derivado del Selector (D-111) | Superado por la versión real 28×14 / perilla 15×10 de D-165/D-167 — el marcador describe una versión que ya no existe |

## Comentarios de código e inconsistencias diario↔código

No son bugs funcionales; son texto en el código que ya no describe la realidad:

| Dónde | Qué dice | Realidad |
|---|---|---|
| `aura_selector.h:1-6` (cabecera) | "la pastilla misma es del color de acento" | Falso desde D-112: la pastilla es `SELECTION_FILL` gris; el acento va en texto/ícono/chevron |
| `tokens.json` → `selector.comment_tint` | Describe el blanco como "texto sobre Selector de acento" | Ese uso no existe desde D-112; hoy ese blanco lo consumen SelectionSummary y el switch |
| `DECISIONS.md` D-116 vs `aura_coverflow.c:963` | D-116 dice que el ScrollIndicator del reverso usa tinta `TEXT_SECONDARY` | El código pasa `SHELL_RAIL` — inconsistencia diario↔código (no involucra doc de diseño) |
| `apple2026_shell.h:59` (comentario) | "MAXUSERFONTS (12 exacto…)" | `MAXUSERFONTS` es 14 desde D-267 (`font.h:64`) |

## Pendiente de verificación (no huecos, pero sin confirmar)

- Gesto de mantener SELECT (D-108): implementado, pero el arnés de capturas no puede sostener botones — solo verificable en el simulador interactivo en vivo. Sin evidencia registrada de esa verificación.
- `now-playing.md:359-360`: validar medidas de carátula (x=10, y=43, 135px, 7°) contra el dispositivo físico. Pendiente de hardware, no de doc ni código.
