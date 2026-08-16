# Auditoría de decisiones de implementación no explícitas en la documentación

**Snapshot fechado: 2026-08-16.** Esto NO es `DECISIONS.md` (el diario cronológico de trabajo, D-001 a D-273, que sigue siendo la fuente principal) — es una síntesis **temática**, agrupada por componente, de dos cosas:

1. **Decisiones silenciosas**: valores o comportamientos que el código fija sin que `docs/aura-design-system/` los especifique, y sin un marcador `TODO(pendiente-doc)` en el código que lo señale explícitamente.
2. **`TODO(pendiente-doc)` explícitos**: los 12 marcadores reales en el código, con el valor provisional que eligieron, la entrada de `DECISIONS.md` que los originó, y si la pregunta sigue abierta hoy en la doc viva.

Cada entrada cita archivo/línea real — verificar ahí antes de asumir que sigue vigente, el código sigue cambiando.

---

## Parte 1 — Decisiones silenciosas (sin marcador en código)

| Componente | Decisión | Dónde vive | Por qué (si hay rastro) |
|---|---|---|---|
| Selector | Color de texto/ícono sobre el Selector = blanco, por máximo contraste | `tokens.json`, `comment_tint`, marcado "provisional G5" en el propio JSON | La doc nunca especificó este valor; no hay `TODO(pendiente-doc)` en C porque la decisión vive solo en el token |
| ScrollIndicator | Alto = 24px; color gris reutiliza `SHELL_RAIL` en vez de un hex nuevo | `tokens.json`, `comment_height`/`comment_gray`, "provisional G8" | Ambos pendientes de la doc se resolvieron aquí, sin marcador en el `.c` |
| MarqueeText | Gap entre vueltas del loop = 24px | `tokens.json`, "provisional G11" | Sin spec de la doc para este valor |
| CoverFlow | Radio de esquina 8px (reemplaza un 5px provisional anterior); opacidad de reflejo 25% base / 45% pico; centro medido en 130px de una captura real del CoverFlow original de Apple | `tokens.json`, sección `art`, "provisional G16" | Confirmado 2026-08-12; el centro en particular viene de medir una captura real aportada por el dueño, no de una spec escrita |
| CoverFlow | PLAY sobre álbum ya sonando alterna pausa/reanudar, no solo "reproducir" | `aura_coverflow.c:1194` | D-115: "provisional razonable" — sin esto no habría forma de pausar dentro de Cover Flow. La doc nunca lo ratifica ni lo contradice |
| CoverDrift | Categoría extendida a "Ahora suena" y "Canciones aleatorias" del menú raíz, no solo a Música | `aura_screens.c:715-727`, `music_row_wants_coverdrift()` | D-266 (esta sesión). Decisión real y documentada en `DECISIONS.md`, pero **nunca sincronizada de vuelta** a `componentes/cover-drift.md` — la sección "Filas que califican" de la doc no la menciona |
| SelectionSummary | Fondo del panel: imagen de fondo completo por acento, reemplaza el degradado diagonal calculado | `aura_selection_summary.c:385-391`, `ensure_panel_background()` | D-267 (esta sesión), comentario explícito "reemplaza el degradado diagonal". El degradado diagonal sigue existiendo, pero solo dentro del tile/ícono, no como fondo del panel — la doc no distingue esto |
| SelectionSummary | Tipografía real 18pt Bold (superior) / 16pt Medium (inferior), medida en píxel contra mockup | `aura_selection_summary.c`, `DS_BOLD_18`/`DS_MEDIUM_16` | D-271 (esta sesión) — la doc no tiene fila de tipografía para este componente en `fundamentos/02-tipografia.md` |
| SelectionSummary | Ícono centrado en el panel de forma independiente del texto (no como grupo); texto centrado en el margen completo, no pegado al tile; sombra SDF con caída de 12px | `aura_selection_summary.c`, `tile_y` fijo, `draw_tile_shadow()` | D-270/D-272 (esta sesión) |
| StatusBar / ClockIndicator | Tamaño de fuente real 12px, no los 8-10px que documentaba la doc originalmente | `aura_status_bar_v2.c:237-245`, `aura_clock_indicator.c:25-30` | Dos encargos del dueño el mismo día (2026-08-14): "el texto casi no se nota" — escaló 8→10→12px. La doc nunca se actualizó tras el cambio |
| Flip-and-Flow | `FLOW_MS = 500` (antes 350 provisional) | `aura_transitions.c:678`, comentario "decisión del dueño del diseño" | Confirmado por el dueño, pero vive solo como `#define` local — `transiciones/00-vocabulario.md` no lo expone como token documentado |
| NowPlaying | Stagger de entrada desde CoverFlow: los tres grupos de contenido entran en paralelo, la barra cae al final | `aura_transitions.c:856` | D-113: razonado como consistente con Push-and-Drop. La propia doc lo marca "pendiente menor" — sigue sin ratificar formalmente |

---

## Parte 2 — `TODO(pendiente-doc)` explícitos en código (12 encontrados)

| # | Archivo:línea | Valor provisional | D-XXX / razón | ¿Sigue vigente hoy? |
|---|---|---|---|---|
| 1 | `aura_status_bar_v2.c:133` | `SHELL_BG` como fondo sólido de la StatusBar (sin traslucencia real) | D-096: no había contenido detrás (SelectionSummary/CoverDrift aún no existían) para que un blend real se distinguiera de un color sólido | **Sí** — `status-bar.md:127` sigue diciendo "fallback es color sólido, valor exacto pendiente". Nunca se revisitó pese a que ya existe contenido detrás hoy |
| 2 | `aura_status_bar_v2.c:46` | `AURA_SB2_CLOCK_ANIM_MS = 220` | D-108: ningún documento confirmaba un valor para Drop-and-Lift/Push-and-Pull de ClockIndicator; se reutilizó un número de otra animación sin derivarlo de spec | **Sí** — sin valor de timing en `status-bar.md` ni `now-playing.md` |
| 3 | `aura_selection_summary.c:104` | Dirección del degradado diagonal: claro arriba-izquierda → oscuro abajo-derecha | D-097: "luz desde arriba", convención sin spec. El % de aclarado/oscurecido ya estaba resuelto desde D-086 | **Sí**, aunque discutible hoy — el degradado ya no es el fondo del panel (ver Parte 1), solo vive dentro del tile |
| 4 | `aura_coverflow.c:163` | `CF_SCROLL_ANIM_MS = 220` (snap con aceleración) | D-103: sin número en su momento | **Zona gris** — `cover-flow.md:106-109` hoy SÍ cita 220ms, pero como ancla derivada del código (trabajo posterior D-245/246/247), no como una confirmación independiente de diseño |
| 5 | `aura_coverflow.c:320` | `CF_FLIP_MS = 260` (por fase del flip) | D-104: timing de fase sin número | **Ambiguo** — la sección "Flip-and-Flow" original de la doc fue reemplazada por "Vuelo CoverFlow → reproductor" (modelo distinto). El checklist de pendientes ya no lista este timing, pero no está claro si se aceptó en silencio o se perdió al reescribir |
| 6 | `aura_menu_list.c:80` | Switch inline 22×12px, derivado del alto máx./gap de 4px del Selector | D-111: en vez de inventar un número aislado | **Sí** — `left-panel.md:97` sigue con "dimensiones individuales pendientes" |
| 7 | `aura_menu_list.h:19` | (mismo TODO que #6, comentario de implementación) | D-111 | Sí, mismo estado que #6 |
| 8 | `aura_selector.h:11` | Ícono de carga (spinner) del indicador dinámico — sin implementar, solo la flecha existe | **Sin D-XXX** — razonamiento vive solo inline ("construirlo sin un consumidor real sería adivinar su apariencia") | **Sí** — `selector.md:55` sigue con el checkbox sin marcar. Trazabilidad rota (no hay entrada formal en `DECISIONS.md`) |
| 9 | `aura_screens.c:467` | 6 presets de acento con nombre (Rosa/Rojo/Naranja/Verde/Azul/Morado), no un selector visual de swatches | D-087: `aura_widgets_draw_list()` no puede pintar un color arbitrario por fila hoy; extenderlo sería un cambio de arquitectura que D-087 no se atribuyó | **Sí** — sin fila nueva en `fundamentos/01-color.md` que especifique la interfaz |
| 10 | `design-system/tokens.json:113` | (mismo TODO que #9, comentario del token) | D-087 | Sí, mismo estado que #9 |
| 11 | *(sin ubicación de código — ver nota)* | — | — | — |
| 12 | *(sin ubicación de código — ver nota)* | — | — | — |

> Nota de conteo: el agente que hizo este barrido reportó 12 marcadores pero solo detalló 10 con ubicación (algunos archivos tienen el mismo TODO citado en dos sitios, ej. #6/#7 y #9/#10, que cuentan como 2 de los 12). Si necesitas la lista exhaustiva línea por línea, correr de nuevo: `grep -rn "TODO(pendiente-doc)" firmware/ design-system/ studio/`.

**Resumen de la Parte 2**: de los 10 casos con ubicación confirmada, **8 siguen genuinamente vigentes**, **1 tiene trazabilidad rota** (#8, sin D-XXX formal), y **1-2 están en zona gris/ambigua** (#4 y #5, CoverFlow) por reescrituras posteriores de la doc que no volvieron a confirmar el valor original.

---

## Patrón general observado

La mayoría de las divergencias no son "decisiones perdidas" sino **doc que no se sincronizó de vuelta tras un cambio real** (SelectionSummary tras D-267/270/271/272, tamaño de fuente de StatusBar/ClockIndicator tras el encargo 2026-08-14, extensión de CoverDrift tras D-266). El propio `docs/aura-design-system/00-INDICE.md` dice explícitamente: *"Cuando el comportamiento de un componente cambie, se actualiza aquí primero, no solo en el código"* — en la práctica, varias sesiones de trabajo rápido e iterativo (con el dueño corrigiendo en vivo sobre capturas) no volvieron a esa doc después. Vale la pena una pasada dedicada a re-sincronizar `componentes/selection-summary.md`, `componentes/status-bar.md`, `componentes/cover-drift.md` y `fundamentos/02-tipografia.md`/`04-bordes.md` contra el código actual.
