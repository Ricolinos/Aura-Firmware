# RESYNC-PREGUNTAS — Decisiones pendientes del dueño (tipo C)

**Fecha: 2026-08-16.** Salida de la Fase 2 del resync doc↔código (ver `RESYNC-PLAN.md`). Cada punto es una divergencia o un valor provisional donde **no hay D-XXX ni encargo registrado** que diga si fue decisión o accidente — no se decidió por cuenta propia. La doc conserva el 🔴/pendiente correspondiente hasta que se responda.

Para cada respuesta: si ratificas el valor actual → en una pasada posterior se cierra el 🔴 en la doc y se quita el `TODO(pendiente-doc)` del código (si lo hay). Si lo cambias → se registra como D-XXX nuevo y se aplica al código.

## SelectionSummary / panel derecho

| # | Pregunta | Valor actual en código | Referencia |
|---|---|---|---|
| C1 | ¿Ratificas **claro arriba-izquierda → oscuro abajo-derecha** para el degradado del tile? (Ya no es el fondo del panel desde D-267, solo vive dentro del tile.) | Convención "luz desde arriba", `aura_selection_summary.c:103-106` | D-097 lo registra como "provisional documentada"; `selection-summary.md` 🔴 |
| C13 | ¿**24px** de gap entre vueltas de MarqueeText es final? | `MARQUEE_LOOP_GAP = 24`, `aura_marquee.c:33` | D-086 G11 "provisional"; la doc ya lo cita como "provisional — vetable" |
| C14 | MarqueeText al navegar fuera a media vuelta: hoy **se interrumpe al instante y reinicia desde los 2s estáticos al volver** — ¿definitivo? | El reloj vive en el llamador por puntero de texto | `marquee-text.md:41-42`; `aura_selection_summary.c:242-247` |

## StatusBar / ClockIndicator

| # | Pregunta | Valor actual | Referencia |
|---|---|---|---|
| C2 | ¿Aceptas `SHELL_BG` **sólido** como fondo definitivo de la StatusBar, o quieres alpha-blend real ahora que en `(full)` sí hay contenido detrás (NowPlaying, CoverFlow)? En `(split)` la barra vive sobre el LeftPanel blanco, donde el blend sería indistinguible. | Relleno sólido, `aura_status_bar_v2.c:127-136` | D-096 explícitamente provisional; `status-bar.md:115-127` |
| C3 | ¿Confirmas que Drop-and-Lift **vertical** en `(split)` y Push-and-Pull **horizontal** en `(full)` para el ClockIndicator es intencional? | Implementado así, `aura_status_bar_v2.c:180-224` | D-108 lo implementa, nunca lo ratifica; `clock-indicator.md:46-50` |
| C4 | ¿**220ms** es el valor final de Drop-and-Lift/Push-and-Pull del reloj? | `AURA_SB2_CLOCK_ANIM_MS = 220`, `aura_status_bar_v2.c:46` (`TODO(pendiente-doc)`) | D-108 explícitamente provisional |

## NowPlaying / LyricsPanel

| # | Pregunta | Valor actual | Referencia |
|---|---|---|---|
| C5 | ¿Ratificas el stagger de entrada a NowPlaying desde CoverFlow: **tres grupos de contenido en paralelo (fade de textos, modos desde la derecha, barra desde abajo) + StatusBar cae al final, 8 cuadros (4 en modo reducido)**? | `aura_transitions.c:842-900` | D-113; `now-playing.md:100-114` lo describe igual, ambos lados lo marcan provisional |
| C6 | ¿Ratificas **8s de hueco mínimo / 3s de lectura** para los silencios del LyricsPanel? | `LYR_SILENCE_MIN_MS 8000`, `LYR_LINE_READ_MS 3000`, `aura_nowplaying.c:114-115` "valores provisionales" | `lyrics-panel.md:75-77`, `now-playing.md:364-365` |

## CoverFlow

| # | Pregunta | Valor actual | Referencia |
|---|---|---|---|
| C7 | ¿Se aceptan **260ms por fase** para el giro SELECT→reverso y reverso→carrusel (`cover_in`/`cover_out`)? Es distinto del vuelo CoverFlow→reproductor de 500ms que ya confirmaste. | `CF_FLIP_MS 260`, `aura_coverflow.c:320` (`TODO(pendiente-doc)`) | D-104 provisional; la sección original de la doc fue reemplazada por "Vuelo CoverFlow → reproductor" y este pendiente se perdió en la reescritura |
| C8 | El vuelo CoverFlow→reproductor gira **en el mismo sentido que la apertura del flip** — ¿lo confirmas como correcto (se cierra el pendiente) o quieres invertirlo? | `aura_transitions.c:658, 773` | `cover-flow.md:275-276` "confirmar en vivo si se lee como antihorario", sin verificación registrada |

## Color

| # | Pregunta | Valor actual | Referencia |
|---|---|---|---|
| C9 | ¿La lista de **6 presets de acento con nombre** (Rosa/Rojo/Naranja/Verde/Azul/Morado, misma lista de elección que Tema/Idioma) es el diseño final, o sigue pendiente un selector visual de swatches? | `accent_presets_hex` en tokens.json, `aura_screens.c:467` (`TODO(pendiente-doc)`) | D-087: se eligió porque `aura_widgets_draw_list()` no puede pintar un color por fila; extenderlo es cambio de arquitectura. `01-color.md` no especifica la interfaz |
| C10 | `A26_ACCENT` del tema (`#FF2D55` claro / `#FF456C` oscuro, paleta A26 vieja) y `aura_accent()` del usuario (`#FF2D52` default) coexisten como **dos "acentos"** distintos. ¿Documentar la separación y quién gana dónde, o está previsto fusionarlos? | `tokens.json` `color.light/dark.accent` vs `aura_ds.color.accent_default_hex` | D-086 los mantiene aparte deliberadamente (namespaces distintos) |

## Bordes / transiciones / capas

| # | Pregunta | Valor actual | Referencia |
|---|---|---|---|
| C11 | Entre LeftPanel y el panel derecho **no hay línea de borde**: la separación la da solo la sombra (8px, 20%). ¿Es diseño (se documenta así y se cierra) o falta un separador de 1px `SHELL_RAIL`? | Sin token `border_width`; solo `left_panel_shadow_width = 8` | `04-bordes.md` pide "grosores de borde/separadores" |
| C12 | Push-and-Drop: ¿los **≈133ms de push** (8 cuadros@60Hz, 4@45Hz) y **≈83ms de drop/lift** (5 cuadros, 3 en modo reducido) actuales son el diseño (→ tokenizar y documentar) o provisionales? | Hardcodeados como conteos de cuadros en `aura_transitions.c:281-287, 359, 467`, sin token ni D-XXX | `transiciones/00-vocabulario.md` "Pendiente: timing" |
| C15 | `sistema/01-capas-y-jerarquia.md:44-47`, dos preguntas abiertas: **(1)** ¿`--layer-content` puede tener más de un elemento apilado, o es estrictamente 0 o 1? **(2)** ¿Existen casos donde `--layer-base` cambia de contenido sin pasar por una transición de capa? — Nota: el debounce del panel derecho (D-262/D-266: 2s con crossfade a CoverDrift, 1s con corte a SelectionSummary) es evidencia de que **sí** existe cambio de contenido en `--layer-base` sin transición de capa. ¿Eso responde la (2)? | — | Sin resolución en código |

---

**Cómo responder**: basta con el número (C1…C15) y "sí"/"no"/el valor nuevo. Con las respuestas se hace una pasada corta que cierra los 🔴 correspondientes en la doc y, en una pasada de código aparte, quita los `TODO(pendiente-doc)` ratificados o aplica los valores nuevos con su D-XXX.
