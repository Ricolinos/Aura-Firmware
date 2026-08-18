# PLAN-scroll-fix — Fase 1: diagnóstico y plan (ScrollIndicator + barra A–Z)

> **ESTADO: EJECUTADO** — 2026-08-17. Histórico. No es trabajo pendiente.
> Decisiones en `DECISIONS.md` (D-275/D-276).

**Fecha: 2026-08-16. Estado: PENDIENTE DE APROBACIÓN DEL DUEÑO. Ningún código editado.**

Investigación de solo lectura cruzando `docs/aura-design-system/componentes/scroll-indicator.md`, `docs/design/Reglas de diseño Apple2026 (v2).md`, `design-system/tokens.json`, el código real (`aura_scroll_indicator.c/.h`, `aura_menu_list.c`, `aura_widgets.c`, `aura_patterns.c`, `aura_coverflow.c`, `aura_screens.c`, `aura_wheel.c/.h`) y `DECISIONS.md` por fecha.

---

## Paso 0 — Spec vigente del ScrollIndicator: RESUELTO con certeza

**No hay dos specs en disputa: hay DOS COMPONENTES distintos en el código**, y cada "versión" que se veía corresponde a uno real:

| | Versión A — `ScrollIndicator` (componente v2) | Versión B — `draw_scrollbar()` (sistema viejo) |
|---|---|---|
| Doc | `componentes/scroll-indicator.md` (viva): grosor 4px (l.13), alto fijo 24px (l.20), gris `SHELL_RAIL` (l.27-29), idle 1.5s (l.38), fade "lenta ~500ms" (l.46), "aparece con cualquier movimiento" (l.34), "animada/suave, en tiempo real" (l.42-44) | `docs/design/Reglas de diseño Apple2026 (v2).md` §5.3 (l.114-121, histórica): 3px, cápsula r1.5, inset 2px, `SHELL_RAIL`, alto **proporcional** mín. 24px, fade-in 150 / hold ~800 / fade-out 330ms |
| Tokens | `tokens.json:179-186` `scroll_indicator`: `thickness 4, min_items_to_show 10, idle_before_fade_ms 1500, fade_duration_ms 500, height 24` | **Ninguno** — `#define` locales `aura_widgets.c:445-451` (`SCROLLBAR_W 3, INSET 2, MIN_H 24, RADIUS 1, FADE_IN 150, HOLD 800, FADE_OUT 330`) — deuda contra la regla "nada hardcodeado" |
| Código | `aura_scroll_indicator.c` (T2.4, D-093). Consumidores: MenuList/LeftPanel (`aura_menu_list.c:263`), reverso de CoverFlow (`aura_coverflow.c:962`), listas de álbumes/playlists (`aura_screens.c:3546, 3701`) | `aura_widgets.c:522-552`. Único consumidor: `aura_widgets_draw_list()` (`:826`) = **todas las listas de contenido a pantalla completa** (canciones, artistas, Ajustes… 6 llamadores en `aura_screens.c`) |
| Origen | D-086 (G8) + D-093 (componente v2), valores ratificados D-274 (hoy) | D-073 (Fase 27, sistema viejo) — nunca migrado a v2 |

**Conclusión**: la spec vigente es la **A** (`scroll-indicator.md`): es la doc viva, la más reciente, ratificada hoy, y `docs/aura-design-system/` gana sobre `docs/design/` por regla del proyecto. La B **no compite** — es el scrollbar del sistema viejo que quedó vivo en LISTA-COMPLETA sin migrar. **Justo ahí (listas de canciones a pantalla completa) es donde el dueño ve los síntomas**, así que parte del arreglo es migrar ese consumidor al componente v2 (ver P1).

---

## Bug 1 — "aparece con delay y se mueve estático" — causa raíz

### (a) Delay = fade-in simétrico de 500ms (LeftPanel/MenuList)
`aura_scroll_indicator.c:20-23` llama `aura_pattern_fade_on_idle_alpha_256(idle, FADE_DURATION_MS, IDLE_BEFORE_FADE_MS, FADE_DURATION_MS)` — **el mismo token `fade_duration_ms = 500` como `fade_in_ms` y como `fade_out_ms`**. `aura_patterns.c:38-39`: `alpha = elapsed*256/fade_in_ms` → tarda medio segundo en llegar a opacidad plena tras cada movimiento; con la rueda girando la barra nunca pasa de un gris pálido. La doc dice "fade lenta ~500ms" para el *desvanecido* sin distinguir sentido; el código lo aplicó a ambos. **Hipótesis confirmada**: la aparición es lenta porque hereda la duración del desvanecido. El disparador en sí funciona bien (`s_activity_since` se reinicia por cada cambio de `selected`, `aura_menu_list.c:143-147`).
En LISTA-COMPLETA (versión B) el fade-in ya es de 150ms (`aura_widgets.c:449`) — ahí el "delay" no aplica.

### (b) "Estático" = thumb proporcional a `first` entero, y `first` no cambia mientras la selección viaja dentro de la ventana
`aura_scroll_indicator.c:30-33`: `thumb_y = track_y + (track_h - thumb_h) * first / max_first` — sin interpolación (el propio header lo declara "simplificación… diferida", `aura_scroll_indicator.h:22-30`). Y `first` (`aura_menu_list.c:134-142`) es `selected - visible/2` acotado a `[0, count-visible]`: con 7 filas visibles, mientras la selección va de la fila 0 a la 3, `first = 0` → **el thumb no se mueve nada**; después salta un paso por ítem; en los últimos 3 ítems vuelve a quedarse quieto. Resultado exacto de lo que reporta el dueño: aparece pegado arriba, inmóvil, luego a saltos, luego inmóvil abajo. **Hipótesis confirmada** (posición por ventana, no por ítem, y sin interpolar).
En LISTA-COMPLETA `draw_scrollbar()` también usa `first` entero (`aura_widgets.c:543-545`) aunque la pastilla de selección de esa lista sí se anima (`pill_animated_y`, resorte).

### (c) No hay animación de scroll a la que engancharse
MenuList salta filas sin animación por decisión del dueño (D-212 puesto / D-214 revertido, `aura_menu_list.c:56-70`); no existe un offset animado de ventana. El thumb necesita su propia interpolación.

---

## Bug 2 — aparece en listas que no lo necesitan — causa raíz

- **LeftPanel/MenuList**: `aura_scroll_indicator.c:17` `if (count <= MIN_ITEMS_TO_SHOW /*10*/) return;` — **correcto** (>10).
- **LISTA-COMPLETA (versión B)**: `aura_widgets.c:530` `if (count <= visible) return;` con `visible = (240-24)/29 = 7` (`aura_widgets.c:358-361`). **Aquí está el bug**: cualquier lista de **8, 9 o 10 ítems** a pantalla completa muestra la barra — este dibujo nunca leyó `min_items_to_show`. Es el caso que el dueño ve.
- Reverso de CoverFlow: doble condición (`> visible` y `> 10`), coherente. Álbumes/playlists (`aura_screens.c:3546, 3701`): componente v2 con 4 visibles → 5-10 álbumes desbordan sin indicador (spec-consistente, discutible; no lo toco salvo que se pida).
- ¿Tiene sentido "10" en pantalla completa? Caben **7 filas** en LISTA-COMPLETA, las mismas que en LeftPanel → el mismo umbral es defendible. El hueco es que nunca se especificó (ver "Doc").

---

## Cambio 3 — Barra de índice A–Z siempre visible — hechos

Vive íntegra en `aura_widgets.c`, dentro de `aura_widgets_draw_list()`; nombre interno **"riel A-Z"**, `draw_index_rail()`:

| Qué | Dónde / valor |
|---|---|
| Constantes | `aura_widgets.c:636-637`: `RAIL_W 10`, `RAIL_MIN_ITEMS 12` — **literales, sin token** |
| Clasificación de inicial | `:639-653` `rail_initial()`: a–z/A–Z → letra; dígitos, acentuadas (≥0x80) y símbolos → `'#'` |
| Conjunto | `:664-673`: recorre los ítems ordenados y agrega la inicial solo si difiere de la anterior → **solo las iniciales presentes**, en orden; `#` va primero (`label_cmp` ordena números antes) |
| Umbral | `:661` `count < 12 → return`; también `n < 2 → nada` |
| Selección | `:675-676` `sel_letter = rail_initial(items[selected].label)` — **la letra en acento sigue el scroll en tiempo real** |
| Dibujo | `:678-696`: x = 310, columna 10px, rango vertical 24..240 (216px), paso `216/n` mín. alto de glifo, recorte por abajo (`if (y+h > 240) break`); fuente `DS_REG_8` (glifo 9px de alto); normal `A26_TEXT_TERTIARY`, seleccionada `A26_ACCENT` (del tema); sin fondo |
| Solo en `(full)` | `:821-824`; en `(split)` ese espacio es el panel derecho |
| Recorte del texto de fila | `:787-800`: viewport acortado 12px (`RAIL_W + SPACING_XS`) |
| **Navegación por letra** | **No existe.** `aura_wheel_should_hop_letters()` (`aura_wheel.c:26`, D-077) tiene cero consumidores; el comentario de `aura_wheel.h:28-32` ("Aura no tiene lista indexada A-Z") es falso desde D-155 |
| Historia | D-073 lo difirió; **D-155** (2026-08-13) lo construyó como indicador pasivo. Spec vieja en `docs/design/…(v2).md:61,66,90` ("7 px SF Pro: riel A-Z", "Semibold → letra activa", "con lupa") — Semibold y lupa no implementados. En doc viva: solo 5 líneas en `sistema/02-navegacion-menus-contenido.md:87-91`, **sin archivo de componente** |

**Letras sin contenido**: confirmado, no se dibujan — el filtrado es al construir el conjunto (`:666-671`). Una lista con A, C, M, S muestra exactamente `A C M S` estiradas en 216px (paso 54px): la barra cambia de forma con el contenido.

**⚠️ Problema geométrico para "todas visibles"**: 216px / 27 letras (`#` + A–Z) = **8.0px de paso**, pero el glifo de `DS_REG_8` mide **9px** → `step = max(8, 9) = 9` → la última letra cae en y = 24 + 26·9 = 258 > 240 → **el `break` recorta X, Y, Z**. Con 26 letras (sin `#`) tampoco cabe (8.3px). **No caben 27 con la fuente actual** — pregunta P3 abajo.

---

## Cambio 4 — Convivencia ScrollIndicator ↔ barra A–Z — hechos

- **Sí coexisten y se enciman, confirmado en código**: `aura_widgets_draw_list()` llama `draw_index_rail()` (`:824`) y después `draw_scrollbar()` (`:826`), ambas en `(full)`. Riel: x = 310–319 (letras ≈ 312–317). Scrollbar B: x = 320 − 2 − 3 = **315–317**, mismo rango vertical → **el thumb pasa por el centro de las letras**, y como `a26_shell_fill_rounded_rect` pinta `SHELL_BG` en las esquinas del thumb, **borra parcialmente la letra que cruza**.
- **La barra A–Z ya comunica posición**: la letra en acento es la inicial del ítem seleccionado y sigue el scroll. Con letras siempre visibles (Cambio 3), comunica posición aproximada de forma continua.
- Para la opción (b) reposicionar: no hay dónde sin robar ancho a las filas — el riel ya ocupa los 10px del borde y el texto ya se recorta 12px; poner el scrollbar a la izquierda del riel (x≈300) costaría ~5px más de texto y quedaría entre el texto y las letras.

---

## Cambios propuestos (uno por uno, con valores exactos)

Todos los valores nuevos van a `design-system/tokens.json`; ninguno hardcodeado en `.c`.

| # | Cambio | Archivo:línea | Valor exacto | Resuelve |
|---|---|---|---|---|
| P1 | **Migrar LISTA-COMPLETA al componente v2**: `aura_widgets_draw_list()` deja de llamar `draw_scrollbar()` y llama `aura_scroll_indicator_draw()`; retirar `draw_scrollbar()` y sus 7 `#define` (`aura_widgets.c:445-451, 522-552, 826`) | `aura_widgets.c` | Posición: columna derecha, `x = A26_SCREEN_WIDTH - thickness - inset` con el mismo inset que hoy (2px) — nuevo token `scroll_indicator.inset_full = 2` (LeftPanel sigue en su padding de 4px, sin cambio); rango vertical `LIST_TOP..240` | Paso 0 (una sola spec), Bug 2 (umbral 10 llega vía el componente), Bug 1 (los arreglos siguientes aplican a ambos), Cambio 4 (un solo elemento que gestionar) |
| P2 | **Fade asimétrico**: separar `fade_duration_ms` en dos tokens | `tokens.json:179-186`; `aura_scroll_indicator.c:20-23`; `aura_menu_list.c:277-297` (`pending()/animating()`) | `scroll_indicator.fade_in_ms = 150` (cadencia estándar del sistema, la misma que ya usaba la versión B y que docs/design §5.3 llama "fundido lineal de 150ms"), `scroll_indicator.fade_out_ms = 500` (conserva "lenta ~500ms" de la doc para el desvanecido); `idle_before_fade_ms = 1500` sin cambio. Retirar `fade_duration_ms`. Macros: `AURA_DS_METRICS_SCROLL_INDICATOR_FADE_IN_MS / _FADE_OUT_MS` | Bug 1(a) |
| P3 | **Posición por ítem seleccionado**, no por ventana | `aura_scroll_indicator.c:30-33` (firma: nuevo parámetro `selected` explícito, no abusar de `first`) y sus 4 llamadores | `thumb_y = track_y + (track_h - thumb_h) * selected / (count - 1)` — el iPod Classic mueve el pulgar con cada ítem | Bug 1(b), primera mitad |
| P4 | **Interpolación del thumb** ("redirigir sin salto", mismo patrón `aura_pattern_lerp()` de CoverDrift/CoverFlow/ClockIndicator D-108) | `aura_scroll_indicator.c` (estado `s_thumb_from/to/since`); `aura_menu_list.c:284` (`animating()` debe devolver true también durante el deslizamiento, si no el bucle no redibuja) | Nuevo token `scroll_indicator.slide_ms = 150` — lineal (es contenido, no control → lineal según la regla de movimiento del sistema; misma cadencia que el fade-in) | Bug 1(b), segunda mitad |
| P5 | **Umbral único** `min_items_to_show = 10` en todos los consumidores (llega solo con P1 para LISTA-COMPLETA) | vía P1 | sin valor nuevo | Bug 2 |
| P6 | **Riel A–Z: conjunto fijo de 27** (`#` + A–Z) con máscara de presencia; siempre dibujar los 27; tinta por estado; tokenizar `RAIL_W`/`RAIL_MIN_ITEMS` | `aura_widgets.c:636-637, 664-696` | Nuevos tokens `aura_ds.metrics.index_rail.width = 10`, `.min_items = 12`; colores: presente+seleccionada `A26_ACCENT` (como hoy), presente `A26_TEXT_TERTIARY` (como hoy), **ausente/deshabilitada `A26_SHELL_RAIL`** (ya es "el gris de rieles/separadores", `01-color.md`; en tema oscuro `#3A3A3C` también lee como apagado; cero tokens nuevos) — alternativa con `disabled_alpha_pct` solo si el dueño quiere afinar | Cambio 3 (visual) — **sujeto a P-alto, ver preguntas** |
| P7 | **"No seleccionables"**: hoy nada del riel es seleccionable (no hay salto por letra), así que la deshabilitación es puramente visual. Si el dueño quiere salto por letra, es feature aparte (gancho listo: `aura_wheel_should_hop_letters()`, debe saltar solo entre presentes) | — | — | Cambio 3, mitad "no seleccionable" — pregunta P4 |
| P8 | **Convivencia**: según respuesta del dueño (ver pregunta) | `aura_widgets.c` orden/condición de dibujo | — | Cambio 4 |
| P9 | Limpieza: comentario obsoleto `aura_wheel.h:28-32`; comentario en `aura_scroll_indicator.h:22-30` que declara la simplificación diferida | — | — | deuda |

Fuera de alcance salvo que se pida: `aura_menu_list.c` reinicia la actividad en la primera pintura (`s_last_selected = -1`), así que el indicador aparece al entrar a una pantalla sin movimiento — contradice literalmente "aparece con movimiento"; menor. Álbumes/playlists con 4 visibles y umbral 10 (5-10 álbumes desbordan sin indicador).

---

## Preguntas abiertas para el dueño

**Q1 — Migrar LISTA-COMPLETA al componente v2 (P1)** — recomendación: **sí**. Es la única forma de que Bug 1/2 queden arreglados donde el dueño los ve, de tener una sola spec (la viva) y de que Cambio 4 gestione un solo elemento. Costo: el thumb de listas completas pasa de alto proporcional (spec vieja) a alto fijo 24px (spec viva) y de 3px a 4px de grosor. Si prefieres conservar alto proporcional en pantalla completa, dilo — sería un token `height_mode` y una excepción documentada, no la recomendación.

**Q2 — Cambio 4, (a) ocultar vs (b) reposicionar** — recomendación: **(a) ocultar el ScrollIndicator cuando el riel A–Z está dibujado**, condicionado: cuando el riel NO se dibuja (lista corta < 12, o < 2 iniciales), el ScrollIndicator vuelve (con su umbral de 10). Razones: la letra en acento del riel ya comunica posición en tiempo real (y con las 27 letras fijas, de forma continua); no hay espacio para (b) sin robarle ancho al texto de fila; y el iPod Classic original tampoco muestra ambos a la vez. Contra: entre 10 y 11 ítems habría ScrollIndicator sin riel — coherente con los umbrales, aceptable.

**Q3 — Riel de 27 letras no cabe con la fuente actual** (216px / 27 = 8px de paso; glifo de `DS_REG_8` mide 9px → se recortan X, Y, Z). Opciones:
1. **Fuente nueva `ds_reg_7`** (la spec vieja pedía justo "7 px SF Pro" para el riel) — cabe limpio (27·8 = 216). Costo: **un slot de `MAXUSERFONTS`, hoy exacto en 14/14** → subir a 15 (o retirar otro estilo). Recomendación: **esta**, subiendo a 15 — es el único ajuste que respeta "todas visibles" sin encimar; el costo de un slot es de memoria mínima.
2. Solapamiento controlado: paso 8px con glifo de 9 (1px encimado). Legible pero sucio.
3. Quitar `#` (26 letras → 8.3px): sigue sin caber con 9px, y `#` sí tiene contenido real (canciones que empiezan con número).
4. Empezar el riel sin el `SPACING_SM` superior (220px): 8.15px, tampoco alcanza.

**Q4 — "No seleccionables"**: ¿basta con la deshabilitación visual (hoy el riel no es navegable, nada es seleccionable) o quieres además el **salto por letra con la rueda rápida** (`aura_wheel_should_hop_letters()` ya existe sin consumidor desde D-077), saltando solo entre letras presentes? Recomendación: **visual ahora**, salto por letra como encargo aparte — es una feature nueva con su propia coreografía (¿lupa? ¿la letra crece?) que la spec vieja mencionaba y la viva nunca definió.

**Q5 — Color de letra deshabilitada**: `A26_SHELL_RAIL` reutilizado (recomendado, cero tokens nuevos) ¿o un alpha ajustable (`disabled_alpha_pct`) sobre `TEXT_TERTIARY`?

---

## Documentación a crear/actualizar (Fase 2, como parte del trabajo)

1. **`componentes/scroll-indicator.md`** — actualizar: fade asimétrico (in 150 / out 500) con los tokens; posición por ítem seleccionado + interpolación 150ms; **sección nueva "En listas a pantalla completa (LISTA-COMPLETA)"**: mismo componente, columna derecha con inset 2px, mismo umbral 10, y la regla de convivencia con el riel (según Q2); origen D-XXX nuevo. Retirar la mención a `aura_widgets.c` viejo si la hay.
2. **`componentes/index-rail.md`** — **archivo nuevo** para el riel (nombre propuesto **`IndexRail`**, conserva "riel" que ya usan código y docs). Secciones: qué es / dónde vive (`(full)` únicamente, mismo nivel que ScrollIndicator) / cuándo aparece (≥12 ítems) / anatomía (columna 10px, 27 posiciones `#`+A–Z, fuente, paso) / estados por letra (presente · seleccionada · ausente, con tokens) / comportamiento (la letra en acento sigue la selección; sin salto por letra hoy, gancho listo) / relación con ScrollIndicator (Q2) / tokens / origen (D-073 → D-155 → D-XXX) / pendientes (salto por letra, lupa de la spec vieja).
3. `sistema/02-navegacion-menus-contenido.md:87-91` — enlazar al componente nuevo.
4. `fundamentos/01-color.md` — el riel como consumidor de `A26_ACCENT`/`SHELL_RAIL` (ya menciona el primero).
5. `fundamentos/02-tipografia.md` — si Q3 = opción 1: fila `--font-index-rail` 7px + presupuesto 15/15.
6. `docs/design/Reglas de diseño Apple2026 (v2).md` §5.3 — no se edita (histórica), pero `scroll-indicator.md` debe decir explícitamente que la versión de 3px/proporcional quedó reemplazada, para que nadie la reviva.
7. `DECISIONS.md` — una entrada D-275 por unidad de trabajo aprobada (o varias si se aprueban por partes).

---

**BARRERA: no se edita código hasta que el dueño apruebe este plan y responda Q1–Q5.**
