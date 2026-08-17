# PLAN — Personalización: sección propia, "Modo" con valor en la fila, fondos del panel por acento

> **ESTADO: EJECUTADO** — 2026-08-17. Histórico. No es trabajo pendiente.
> Decisiones en `DECISIONS.md` (D-292). Q0 (procedencia de `ColorsBack/`)
> se resolvió por respuesta directa del dueño: naranja/verde/morado son
> propias, rojo (Franck V.) y azul (Matthew McBrayer) son de Unsplash,
> atribuidas en `THIRD-PARTY-NOTICES.txt` y en Créditos. Los 5 commits de
> la Fase 2 (§8 abajo) están todos en `main`. Pendientes reales que
> quedaron fuera, con su porqué, en el cierre de D-292 (`DECISIONS.md`) —
> no en las preguntas abiertas de este documento, que describen el
> estado ANTES de ejecutar.

**Estado (texto original de la Fase 1, conservado tal cual):** Fase 0 (inventario, solo lectura) y Fase 1 (plan) terminadas — 2026-08-17. **BARRERA**: la Fase 2 no arranca hasta aprobación explícita del dueño. Hay además **una pregunta bloqueante (Q0, procedencia de `ColorsBack/`)** que hay que contestar antes de mover un solo archivo al repo.
**Alcance**: firmware (`Aura-Firmware`). Toques laterales de Studio se listan al final, sin ejecutarlos aquí.
**Numeración**: `DECISIONS.md` va en **D-292+** (el encargo decía D-287+; D-286…D-291 ya existen).
**Contexto del árbol**: hay otra sesión con trabajo sin commitear en este repo (los `PLAN-*.md` de la raíz movidos a `docs/plans/archivo/`, aún sin `git add`). Este documento se escribe en `docs/plans/` siguiendo esa convención nueva y **no toca** ese estado.

---

## 0. Resumen ejecutivo — tres premisas del encargo que hay que corregir antes de planear

1. **El fondo del `SelectionSummary` NO es hoy un degradado calculado desde el acento.** Eso fue D-097 y se retiró en **D-267**: desde entonces el panel derecho completo (160×240) es una **imagen BMP por preset de acento** cargada de disco (`.rockbox/icons/aura/backgrounds/<preset>.bmp`) — pero solo existe `pink.bmp`, y **cualquier acento cae a rosa** ("interino explícito", `tokens.json:263-267`). Si el archivo falta, cae a un relleno plano `SHELL_BG`. Lo único que sigue siendo degradado calculado es el **tile de 90×90** que va encima, y ese sigue el **color de categoría** (Música/Video/Fotos/Ajustes/Extras), no el acento. Las 5 imágenes de `ColorsBack/` son, casi con certeza, **"las otras 5" que D-267 dejó pendientes** ("el dueño pidió probar SOLO con rosa primero… antes de mandar las otras 5").
2. **El toggle "Mostrar sombras" ya está implementado** (`AURA_SCREEN_SETTINGS_LEFT_PANEL_SHADOW`, `aura_settings.left_panel_shadow`, D-154 — `aura_screens.c:226,679,701`; `efectos/01-sombras.md:10-24`). No nace aquí: se **muda** a Personalización.
3. **La geometría de fila del encargo (152×22, ícono 14, SF Pro 10px) está desactualizada**: desde **D-195** las filas del `LeftPanel` miden **31px**, ícono **20px**, texto `ds_reg_12` (`tokens.json:183-191`, `menu_list.row_height/icon_max`). El patrón nuevo se diseña sobre la geometría vigente, y ya existe un elemento derecho inline con el que convivir: el **switch de 28×14** de las filas booleanas (`aura_menu_list.c:111-112,250-256`) y la **fila "Repetir" que cicla con SELECT sin flecha** (D-264, `aura_screens.c:520-530, 2064-2078`).

---

## 1. Fase 0 — Hallazgos

### 1.1 — Inventario de Ajustes (`aura_screens.c:212-263`, `settings_entries[]`)

Orden actual = orden del firmware original con los ajustes de Aura intercalados por sección, sin separadores ("el orden ES la agrupación", `03-arbol-de-menus.md` §Ajustes).

| # | Fila (es-MX) | Pantalla | Tipo hoy | ¿Personalización? |
|---|---|---|---|---|
| 1 | Acerca de | `SETTINGS_ABOUT` | FULL-CARRY (D-279) | no |
| 2 | Aleatorio | `SETTINGS_SHUFFLE` | toggle inline (switch) | no — reproducción |
| 3 | Repetir | `SETTINGS_REPEAT` | **fila inline que cicla 3 estados** (D-264), sin flecha, valor solo en el panel derecho | no — reproducción |
| 4 | Menú principal | `SETTINGS_MAINMENU` | pantalla propia (jerarquía) | **límite**: configura el menú de inicio (es personalización *del menú*), pero es fila del original en su posición original — se queda (ver §2.2) |
| 5 | **Tema** (Claro/Oscuro) | `SETTINGS_THEME` | pantalla de elección SPLIT (`is_choice_screen`), `theme_choice_labels`, íconos por opción `theme-light`/`theme-dark` (`:2278`) | **sí → pasa a "Modo"** |
| 6 | **Estilo** | `SETTINGS_STYLE` | lista dinámica de temas instalados (D-289, `draw_style_list`) | **sí → pasa a "Temas"** |
| 7 | **Color de acento** | `SETTINGS_ACCENT` | elección SPLIT, 6 presets (`accent_choice_labels`, `:511-518`) | sí |
| 8 | **Animaciones** | `SETTINGS_ANIMATIONS` | elección SPLIT (Ninguna/Mínimas/Todas) | sí |
| 9 | **Gráficos** | `SETTINGS_GRAPHICS` | elección SPLIT (Ninguno/Mínimos/Todos) | sí |
| 10 | **Mostrar sombras** | `SETTINGS_LEFT_PANEL_SHADOW` | toggle inline | sí (ya existe) |
| 11 | **Mostrar iconos** | `SETTINGS_SHOW_ICONS` | toggle inline | sí |
| 12–13 | Brillo · Temporiz. luz | pantalla | — | no — pantalla (hardware) |
| 14–18 | Ecualizador · Límite volumen · Ajuste volumen · Audiolibros · Sonido de clic | — | — | no — sonido |
| 19–26 | Temporiz. reposo · Apagado · Bloqueo pantalla · Fecha y hora · Ordenar por · Idioma · Avisos legales · Restablecer | — | — | no — sistema |

Las 7 filas de personalización (5–11) **ya están contiguas** (bloque "apariencia (propios de Aura)", `:222-234`) — no están dispersas por la lista; el trabajo es darles una **entrada propia** ("Personalización") en vez de convivir sueltas entre "Menú principal" y "Brillo".

### 1.2 — Persistencia: **sin riesgo de reseteo, sin migración necesaria**

`aura.cfg` es **clave: valor por nombre** (`aura_settings.c:227-274` lee, `:311-338` escribe: `theme`, `animation_mode`, `graphics_mode`, `accent_rgb24`, `left_panel_shadow`, `show_icons`, `theme_id`…), no por índice de menú ni por ruta. Los ajustes del núcleo (brillo, EQ, etc.) van en `global_settings` de Rockbox, también por clave. El enum `aura_screen_id_t` es **append-only** (una pantalla nueva `SETTINGS_PERSONALIZATION` va antes de `AURA_SCREEN_COUNT`, `aura_nav.h:154`); las selecciones de `aura_nav` son solo runtime. Único acoplamiento índice→valor: `apply_choice(index)` para las pantallas de elección — no cambia (Claro=0/Oscuro=1 se conserva).

**Decisión que sí hay que tomar**: al renombrar "Tema"→"Modo", **la clave `theme:` de `aura.cfg` NO se renombra** (renombrarla a `mode:` resetearía el modo de todos los dispositivos instalados). Cambian solo el texto de UI (`AURA_STR_SETTINGS_THEME` → "Modo"/"Mode") y, si se quiere, un alias interno; el nombre en disco es contrato de facto.

### 1.3 — Temas en runtime: **veredicto SÍ, ya en producción (D-289)**

Las 14 fuentes se cargan con `font_load()` desde `.rockbox/aura/themes/<id>/fonts/`; íconos (máscaras), fondos (`backgrounds/<preset>.bmp`, `CONTRATO-formato-tema.md` §E) y **paleta clara + oscura** (`palette_light_*` y `palette_dark_*`, §B, `:39-54`) se leen de disco; `aura_style_scan()` lista los paquetes instalados; "Estilo" ya los muestra con fila inerte para los inválidos; verificado en simulador con temas de prueba (D-289). Además `firmware/dist/aura-theme-default.zip` es un tema instalable canónico. **"Temas" tendrá qué listar** — al menos "Aura" (default) y lo que Studio instale. La entrada se crea, no se pospone.

### 1.4 — Fondo del `SelectionSummary` hoy (`aura_selection_summary.c`)

- `AURA_SS_BG_ACCENT_IMAGE` (`:62`, D-267): `ensure_panel_background("pink")` (`:190-210`, **nombre fijo**) → `aura_style_read_icon_bmp("backgrounds/pink.bmp")` (pasa por el sistema de temas: un tema puede traer su propio `backgrounds/<preset>.bmp`) → buffer estático `s_bg_pixels[160*240]` (**76,800 B**, `:185`), cacheado por nombre + generación de estilo; se blitea con un `lcd_bitmap` (`:542`). Sin archivo → `SHELL_BG` plano (`:545-547`).
- `AURA_SS_BG_NEUTRAL_FADE` (`:63`, D-281): degradado horizontal gris→`shell_bg` calculado, solo para la fila "Acerca de".
- El **tile de 90×90** encima: `draw_diagonal_gradient()` (`:150`, D-097) con `aura_category_gradient()` (color de **categoría**), esquinas por `A26_LAYOUT_CORNER_RADIUS_CARD`, sombra SDF (D-270).
- Texto del panel en **blanco fijo** sobre la imagen (`tokens.json:149`, D-271: "el fondo nuevo es siempre oscuro/saturado sin importar el tema") — dato clave para 1.4 del encargo (¿variar por Modo?).
- Pipeline de assets ya existente: `design-system/assets/panel-backgrounds/<preset>-source.png` → `generate.py::generate_panel_backgrounds()` (`:637-662`, recorte centrado a 160×240, BMP 24-bit) → `out/icons/aura/backgrounds/<preset>.bmp` → `rockbox.zip` (`package_dist.sh`, D-288). Presets declarados en `tokens.json → right_panel_background.presets` (hoy `["pink"]`) y publicados a Studio en `theme-format-v1.json → background_presets` (Studio no lo consume hoy: cero referencias en su código).

### 1.5 — `ColorsBack/` (raíz del repo del firmware, **sin trackear**)

| Archivo | Tamaño | Formato | Alpha | Contenido |
|---|---|---|---|---|
| `Rojo.png` | 160×240 | PNG RGB | canal presente pero **100 % opaco** | textura roja (pliegue/plástico) |
| `Naranja.png` | 160×240 | idem | idem | pintura fluida naranja/amarilla |
| `Verde.png` | 160×240 | idem | idem | textura verde translúcida |
| `Azul.png` | 160×240 | idem | idem | cielo con nubes |
| `Morado.png` | 160×240 | idem | idem | geoda/amatista |

- **Dimensiones exactas de `BG_W×BG_H`** — no necesitan el recorte de `generate.py`; se convierten tal cual.
- **Convención de nombres**: en español, con mayúscula, sin sufijo `-source` — no coincide con la existente (`pink-source.png` → `pink.bmp`, id en inglés). Faltan `pink` (ya existe) y no hay más: 5 + 1 = **los 6 presets de acento** (`accent_presets_hex`: Rosa, Rojo, Naranja, Verde, Azul, Morado). Mapeo 1:1 con los presets.
- **Procedencia: no verificable desde los archivos.** Sin chunks `tEXt`/`iTXt`/`eXIf`, sin `kMDItemWhereFroms` (no fueron descargadas con Safari/Chrome — o se re-exportaron), creadas hoy 2026-08-17 21:46 UTC (los cinco en el mismo minuto: exportación en lote). Son fotografías/texturas (nubes reales, amatista real), no vectores propios: **no son derivadas de UI de Apple** (no se parecen a ningún asset del iPod original), pero **tampoco puedo afirmar que sean propias o de licencia compatible**. El único precedente escrito es D-267: la rosa fue "imagen propia compartida" por el dueño. **→ Q0 bloqueante** (§7).

---

## 2. Sección "Personalización" (1.1)

### 2.1 — Estructura propuesta

Fila nueva **"Personalización"** en Ajustes, **en el lugar exacto del bloque actual** (después de "Menú principal", antes de "Brillo" — respeta el orden por secciones del original y no mueve nada más). Ícono: `paintpalette` (hoy en Color de acento; ese pasa a `sliders-horizontal`… ver tabla — o **`theme`**, que queda libre al retirar "Tema"; recomendación: `theme` para la fila padre, porque `paintpalette` es más específico de "color"). Es un **submenú SPLIT** (nivel 2, misma mecánica que "Fecha y hora" → `datetime_entries[]`, `aura_screens.c:265-270`: tabla propia + `screen_uses_split_layout()` + `draw_nav_list()`; sin código nuevo de infraestructura).

| Orden | Fila | Pantalla | Tipo | Ícono | Nota |
|---|---|---|---|---|---|
| 1 | **Modo** — `Claro` / `Oscuro` en la fila | `SETTINGS_THEME` (id se conserva; texto "Modo") | **fila con valor inline** (patrón nuevo, §4) | `theme-light`/`theme-dark` **según valor** | reemplaza la pantalla de elección |
| 2 | **Temas** | `SETTINGS_STYLE` (texto "Temas") | lista dinámica (sin cambio, D-289) | `sync` → mejor `theme` si el padre usa otro; ver Q3 | sustituye a "Estilo" |
| 3 | Color de acento | `SETTINGS_ACCENT` | elección (sin cambio) | `paintpalette` | ver Q4 (variante inline) |
| 4 | Animaciones | `SETTINGS_ANIMATIONS` | elección (sin cambio) | `motion` | |
| 5 | Gráficos | `SETTINGS_GRAPHICS` | elección (sin cambio) | `graphics` | |
| 6 | Mostrar sombras | `SETTINGS_LEFT_PANEL_SHADOW` | toggle (sin cambio) | `square-on-square` | ya implementado |
| 7 | Mostrar iconos | `SETTINGS_SHOW_ICONS` | toggle (sin cambio) | `sliders-horizontal` | |

Orden: primero lo que cambia toda la pantalla (Modo, Temas), luego color, luego movimiento/detalle, luego los dos booleanos — de mayor a menor impacto visual, y los toggles al final como en "Fecha y hora" (Reloj 24 h / Hora en el título cierran esa lista).

### 2.2 — Lo que NO se arrastra

- **"Menú principal"** se queda en Ajustes (fila del original en su posición original, sección Reproducción del árbol; configura *qué* aparece, no *cómo se ve*). Anotado como decisión, no por inercia.
- **Brillo / Temporiz. luz**: hardware, sección Pantalla — se quedan.
- **Idioma, Ordenar por**: sistema — se quedan.
- Nada más se mueve: los 19 renglones restantes de Ajustes no cambian de sitio ni de índice relativo.

### 2.3 — Profundidad y layout (regla cerrada de FULL/SPLIT)

`SETTINGS_PERSONALIZATION` es SPLIT (nivel 2, como "Fecha y hora"). Las pantallas hijas **conservan exactamente su clasificación actual**: las de elección siguen SPLIT (`is_choice_screen()` las clasifica por tipo, no por profundidad — `aura_screens.c:4484-4491`), "Temas" sigue SPLIT (`SETTINGS_STYLE`), los toggles no navegan. **No se convierte ninguna pantalla FULL↔SPLIT** (memoria del proyecto, D-253). Lo que sí ocurre es que las pantallas de elección pasan de nivel 2 a nivel 3 siendo SPLIT — la regla por defecto del original dice "nivel 3+ → LISTA-COMPLETA salvo excepción marcada"; aquí la excepción es exactamente la que ya existe para "Fecha y hora → Reloj 24 h" (toggle en SPLIT a nivel 2) y "Zona horaria" (FULL-CARRY a nivel 3): **se documenta como excepción explícita en `03-arbol-de-menus.md`** (Q5 la deja abierta con recomendación).

---

## 3. "Modo" ↔ "Temas" (1.2) — resolución explícita

El sistema ya lo resolvió en D-289 y CLAUDE.md lo fija como regla ("conceptos distintos y ortogonales"); este plan lo **hace visible al usuario** y lo registra:

| Pregunta | Resolución | Justificación (verificada en código/contrato) |
|---|---|---|
| ¿Un tema define sus propios colores o respeta el Modo? | **Respeta el Modo.** Un tema **trae obligatoriamente ambas paletas** (`palette_light_*` y `palette_dark_*`, `CONTRATO-formato-tema.md` §B, `:39-54`; `s_palette[2][9]` en `aura_style.c`) y el Modo elige cuál se usa | El formato v1 no admite tema de una sola variante — no hay caso "impone una paleta", así que **no hay nada que deshabilitar ni ocultar** |
| ¿Puede traer variantes clara y oscura? | **Debe.** No es opcional | idem |
| Si impusiera una paleta, ¿Modo se deshabilita/oculta/ignora? | **N/A en v1.** Si un `theme_format: 2` futuro admitiera "una sola paleta", la regla sería: **la fila "Modo" se atenúa (inerte, `dimmed`) con su valor sustituido por "Fijado por el tema"** — nunca ocultarla ni ignorarla en silencio. Se anota como regla futura en `sistema/05-temas.md` | Coherente con la fila inerte que ya usa "Temas" para paquetes inválidos |
| ¿El Modo afecta al `--color-accent` del usuario? | **No.** `aura_accent()` lee `accent_rgb24` directo (`apple2026_shell.c:82-86`), sin pasar por la paleta ni por el modo. Solo el token heredado `A26_ACCENT` (rosa de fábrica) tiene adaptación oscura `#FF456C` (D-274, `01-color.md:44-59`), consumido por pantallas viejas | Se conserva. Mejora posible (Q6): aplicar al acento libre en Modo oscuro la misma adaptación que D-274 hace con el rosa |
| ¿El tema afecta al acento? | **No** (D-289: slot `A26_ACCENT` sin usar en la tabla del tema; `accent_default`/`accent_presets` del manifiesto aceptados pero no leídos, §H) | Sin cambio |
| ¿Cambiar de tema cambia el Modo? | **No.** `aura_style_activate()` no toca `aura_settings.theme` | Sin cambio |

Texto de cara al usuario que lo explica sin jerga: en el `SelectionSummary` de la fila "Temas", `panel_desc` = "Se ve en modo claro y oscuro" (nuevo string) — una línea, sin cambiar la lista.

---

## 4. Patrón nuevo: **`InlineValue`** — fila con valor visible, sin submenú (1.3)

Nombre para el vocabulario del design system: **`InlineValue`** (estado de fila del `LeftPanel`/`MenuList`, hermano de `toggle` y `checked`). Se documenta en `componentes/left-panel.md` (anatomía de fila) y en `componentes/selector.md` (regla del borde derecho), no como caso especial de Modo.

### 4.1 — Anatomía (geometría vigente, D-195)

Fila 152×31 dentro del `LeftPanel` (padding 4). Ícono 20px a 14px del borde del panel; etiqueta `ds_reg_12` a 4px del ícono (`menu_list.text_gap_after_icon`). Borde derecho de todos los elementos derechos: **4px del borde del Selector** (`selector.indicator_gap_from_edge`, `aura_menu_list.c:145-150`, `right_edge`).

| Elemento | Spec |
|---|---|
| **Valor** | Texto `ds_reg_12` (misma fuente que la etiqueta), alineado a la derecha contra `right_edge`, verticalmente centrado como la etiqueta |
| **Tinta** | **`A26_TEXT_SECONDARY`** en fila no seleccionada; en la fila seleccionada, **el mismo `A26_TEXT_SECONDARY`** (no el acento) — la etiqueta seleccionada va en acento (Selector), el valor se queda atenuado: **dato ≠ acción**. Justificación visual: en la fila resaltada la etiqueta en acento ya marca "esto es lo que vas a cambiar"; si el valor también fuera acento, la fila leería como un solo texto largo. Es exactamente la relación etiqueta/valor de Ajustes de iOS/macOS (valor gris a la derecha), sin inventar tinta nueva: `TEXT_SECONDARY` ya existe en la paleta y varía por Modo/tema |
| **Presupuesto de ancho** | `value_w = min(ancho medido del valor, 60px)`. 60px = ~40 % de los 152 útiles y cabe "Oscuro"/"Claro"/"Todas"/"Ninguna"/"Mínimos" (medidos < 48px en `ds_reg_12`); tokenizado como `menu_list.inline_value_max_w` |
| **Regla de truncado** | La **etiqueta cede primero**: `max_text_w = right_edge − value_w − TEXT_GAP − text_x` (mismo mecanismo que hoy usa el switch: `text_right = right_edge - SWITCH_W - TEXT_GAP`, `aura_menu_list.c:255`) y se recorta con `puts_clipped` (elipsis del sistema). El **valor nunca se recorta**: si un valor midiera > 60px es un error de contenido (los valores son enumeraciones cortas y traducidas — se acota en el `.lang`, no en el render). Si aun así ocurriera, `value_w` se clava en 60 y el propio valor se clipea con elipsis — visible, no silencioso |
| **Flecha** | **Mutuamente excluyente con el valor** (y con switch/checkmark): `full_screen_target = 0` para filas `InlineValue`, igual que ya se hace para toggles y para "Repetir" (`aura_screens.c:2064-2078`; `aura_menu_list.c:209-212`). Regla escrita en `selector.md`: *"la flecha significa 'abre un submenú'; una fila con valor, switch o checkmark no navega, así que no puede llevarla"* |

### 4.2 — Semántica

- **SELECT** cambia el valor **en el sitio**: con 2 opciones alterna; con N > 2 **cicla** en el orden de la tabla (misma semántica que "Repetir" hoy: Off → Todo → Uno → Off). Se documenta el ciclo desde ahora para que "Animaciones"/"Gráficos" (3 opciones) puedan adoptar el patrón si el dueño quiere (Q4) sin cambiar el vocabulario. LEFT/RIGHT no hacen nada (no hay `[OPCIÓN]` del original con izquierda/derecha; evita ambigüedad con "volver").
- **Aplicación del cambio de Modo**: **instantánea**, sin transición. Razones: (a) es lo que hace hoy la pantalla de elección (`apply_choice` → `aura_settings_save()` → el siguiente cuadro repinta con la paleta nueva); (b) el vocabulario no tiene un patrón de "recoloreado de toda la pantalla" y la regla del sistema es no inventar; el más cercano, `Morph Directo`, es para geometría/opacidad de un componente, no un cambio de paleta global; (c) un fundido de pantalla completa entre dos paletas costaría un framebuffer extra (150 KB) para algo que el original tampoco anima. Si el dueño lo quiere animado, la única opción sin patrón nuevo es un crossfade lineal de ~330ms ("fundido lineal para contenido", skill) reusando `s_outgoing_fb`/`s_push_fb` de `aura_transitions.c` — se marca como Q7, recomendación **no**.
- **Persistencia**: `aura_settings_save()` inmediata (mismo criterio que los toggles).

### 4.3 — `SelectionSummary` de la fila

Ícono del panel = **el ícono del valor activo**, mapeo 1:1: `theme-light` para Claro, `theme-dark` para Oscuro (los dos existen ya como máscaras y son los que usa hoy la pantalla de elección por opción, `aura_screens.c:2278`). `panel_top` = "Modo", `panel_desc` = valor ("Claro"/"Oscuro") — igual que "Repetir" cambia `panel_icon` a `repeat-1`/`repeat` según valor (`:2019-2026`), la regla "el ícono siempre cambia" ya tiene precedente.

### 4.4 — Implementación (sin infraestructura nueva)

`aura_menu_item_v2_t` gana un campo `const char *value` (NULL = sin valor). `aura_menu_list_draw()` lo dibuja donde hoy dibuja el switch/checkmark (rama nueva, excluyente). `draw_nav_list()` lo llena para `SETTINGS_THEME` (`items[i].value = aura_str(theme_choice_labels[aura_settings.theme])`); `handle_nav_list()` trata `SETTINGS_THEME` como ya trata `SETTINGS_REPEAT` (SELECT cicla, no `aura_nav_push`). `SETTINGS_THEME` sale de `is_choice_screen()` (y de `get_choice_table()`/`apply_choice()`) — mismo retiro limpio que D-264 hizo con Repetir. Test host-side: ninguno nuevo necesario (render), salvo si se tokeniza el ancho (se lee de `apple2026_tokens.h`).

---

## 5. Fondos del `SelectionSummary` por acento (1.4)

### 5.1 — Arquitectura: se **confirma** la propuesta con dos precisiones

1. **Ya existe la mitad**: la búsqueda por imagen y el fallback plano están (D-267). Lo que falta es (a) **elegir el archivo por el acento activo** en vez de `"pink"` fijo, y (b) un fallback **calculado desde el acento** en vez de `SHELL_BG` plano — para poder sostener "el acento es libremente configurable" cuando un tema (o Studio, D-289 §H) traiga un acento sin imagen.
2. **Correspondencia acento → archivo**: `accent_rgb24` (valor libre, `aura_settings.c:246`) se compara con la tabla de presets `AURA_DS_COLOR_ACCENT_PRESETS_HEX_RGB24_VALUES` (`aura_screens.c:518`, orden Rosa/Rojo/Naranja/Verde/Azul/Morado); si coincide exactamente con el preset *i*, el nombre es `right_panel_background.presets[i]` de `tokens.json` (`pink/red/orange/green/blue/purple`, generados a `apple2026_tokens.h` como tabla de strings — hoy solo la lista `presets` existe, hay que emparejarla por índice con `accent_presets_hex`; se añade en `tokens.json` como `right_panel_background.preset_names_by_accent_index` o, más simple, se garantiza que ambas listas tengan el mismo orden y `generate.py` lo verifica). **"No hay imagen"** = el acento no es ninguno de los 6 presets, **o** `aura_style_read_icon_bmp()` falla / el BMP no mide 160×240 (ya se comprueba, `:209`). En ambos casos → fallback calculado. Se elimina el interino "cualquier acento cae a pink".
3. **Fallback calculado**: `draw_accent_gradient_background(x, width)`: degradado **vertical** en el panel de `aura_accent_dark()` (arriba) → `aura_accent()` (centro) → `aura_accent_light()` (abajo) — los tres derivados ya existen (`apple2026_shell.c:96-104`, `accent_derived_lighten/darken_pct` = 25 %, tokens G9 "para el degradado de SelectionSummary": nacieron justo para esto). Misma primitiva de línea que `draw_neutral_fade_background()` (D-281), 240 líneas horizontales — costo despreciable, sin buffer. Vertical y no diagonal porque el diagonal (D-097) era del tile de 90×90; en 160×240 un diagonal de 3 puntos genera bandas visibles en RGB565; el vertical con 3 puntos y 240 pasos es liso.

### 5.2 — ¿Horneado en el binario o cargado de disco? → **de disco (como hoy), y esto rebate el encargo con números**

| | Horneado en `rockbox.ipod` (encargo) | BMP en disco (arquitectura vigente, D-267/D-289) |
|---|---|---|
| Tamaño binario | +5 × 76,800 B = **+384,000 B** sobre 1,241,132 B (**+31 %**) | +0 |
| RAM | En este target el binario **vive en DRAM** (se carga entero): **+375 KB de RAM permanente** para 5 imágenes de las que solo una se muestra | **sin cambio**: un buffer estático `s_bg_pixels` de 76,800 B (ya existe), se rellena al cambiar el acento/tema |
| Costo al mostrar | cero | un `lcd_bitmap` de 76,800 B (ya es así hoy); la carga (BMP 24 bit → RGB565, `read_bmp_file` del core, sin decodificación) ocurre **una vez por cambio de acento**, no por cuadro |
| `rockbox.zip` | +0 | +5 × ~115 KB = **+576 KB** sobre 6.67 MB (**+8.6 %**), en `.rockbox/icons/aura/backgrounds/` |
| Temas (D-289) | **Rompe el contrato**: `CONTRATO-formato-tema.md` §E permite que un tema traiga `backgrounds/<preset>.bmp` — si el firmware los hornea, un tema no puede reemplazarlos | Compatible: la lectura pasa por `aura_style_read_icon_bmp()` con fallback al default por archivo |
| Dependencia del visor de imágenes | (la premisa "que aún no existe" quedó vieja: D-291) — irrelevante en ambos casos: BMP se lee con `read_bmp_file` del core, no con el visor | idem |

**Recomendación: no hornear.** El costo real de la propuesta del encargo es RAM (+375 KB) y romper la sobreescritura por tema; lo que ya hay es "costo cero al mostrar" de facto (la imagen está en un buffer estático desde el primer dibujo). Se registra como decisión (D-292) con esta tabla.

### 5.3 — Preguntas del encargo, resueltas contra la spec y los archivos

| Pregunta | Respuesta |
|---|---|
| ¿Cubre el tile 90×90 o el área mayor? | **El panel completo, 160×240** (`BG_W×BG_H`, `aura_selection_summary.c:54-55`; `tokens.json:267`; `CONTRATO-formato-tema.md:91`). Los 5 archivos de `ColorsBack/` miden exactamente eso. El tile de 90×90 va encima con su degradado por categoría y su sombra — no cambia |
| ¿Variar según el Modo? | **No, por diseño vigente** (D-271, `tokens.json:149`): "el fondo nuevo es siempre oscuro/saturado sin importar el tema"; el texto del panel va blanco fijo (`SELECTOR_CONTENT_TINT_HEX_ON_ACCENT`). Verificación por muestreo de los 5 PNG: todos saturados con luminancia media–baja (los más claros, Naranja y Verde, siguen dando contraste ≥ 4:1 con blanco en su zona de texto). Duplicar por modo costaría +576 KB más de zip y otro buffer si se quisiera precargar; se **decide no** y se anota; si un asset futuro fuera claro, la regla es reemplazarlo por uno saturado, no crear variante |
| ¿Esquinas de 8px horneadas o recortadas? | **N/A para el fondo**: cubre el panel de borde a borde; las esquinas de **pantalla** (arriba/abajo derecha) las estampa `a26_shell_stamp_corners_left_only()` (D-268) sobre lo dibujado, y las esquinas del **tile** las dibuja el tile mismo. Nada que hornear en el asset (y `generate.py` no lo hace hoy para `pink`) |

### 5.4 — Pipeline de assets (si Q0 se aprueba)

- Renombrar/mover: `ColorsBack/Rojo.png` → `design-system/assets/panel-backgrounds/red-source.png`, `Naranja`→`orange`, `Verde`→`green`, `Azul`→`blue`, `Morado`→`purple` (ids en inglés = mismo criterio que `pink` y que `icon_key`s). Borrar `ColorsBack/` (los originales quedan en git en su ruta nueva).
- `tokens.json → right_panel_background.presets: ["pink","red","orange","green","blue","purple"]` en el **mismo orden** que `accent_presets_hex`; `generate.py` genera los 6 BMP y `theme-format-v1.json → background_presets` los publica (Studio no lo consume, cero impacto).
- `package_dist.sh` ya copia `out/icons/aura/backgrounds/*` (D-288). `aura-theme-default.zip` incluirá los 6 (es el default reempaquetado).
- Licencia: si son propias del dueño, van bajo la GPL v2 del repo como el resto de `design-system/assets/`; `THIRD-PARTY-NOTICES.txt` **no cambia**. Si fueran de terceros con licencia compatible (CC0/CC-BY), sí: entrada nueva con autor y licencia, y **CC-BY exige atribución también en "Acerca de → Créditos"** (`AURA_STR_ABOUT_CREDITS_BODY`).

---

## 6. Textos nuevos (`aura_lang.c`, al final de ambas tablas)

| id | es-MX | en |
|---|---|---|
| `AURA_STR_SETTINGS_PERSONALIZATION` | Personalización | Personalization |
| `AURA_STR_SETTINGS_THEME` (existente, **cambia texto**) | Modo | Mode |
| `AURA_STR_SETTINGS_STYLE` (existente, **cambia texto**) | Temas | Themes |
| `AURA_STR_STYLE_MODE_NOTE` | Se ve en modo claro y oscuro | Works in light and dark mode |

`AURA_STR_THEME_LIGHT/DARK` ("Claro"/"Oscuro") se reusan como valores inline.

---

## 7. Preguntas abiertas (con recomendación)

| # | Pregunta | Recomendación |
|---|---|---|
| **Q0 — BLOQUEANTE** | **¿De dónde salen las 5 imágenes de `ColorsBack/`?** Sin metadatos ni registro. Este repo es público y GPL v2: si son fotografías/texturas de terceros (stock, web) sin licencia compatible, o derivadas de material de Apple, **no pueden commitearse** — irían a `~/Aura-local/` fuera de todo repo, como el tema Apple. | Si son **propias del dueño** (como la rosa de D-267): entran al repo por el pipeline de §5.4, sin aviso adicional. Si son de terceros con licencia libre: entran + `THIRD-PARTY-NOTICES.txt` + Créditos. Si no se sabe: **no entran** — el firmware queda con `pink` + fallback calculado (§5.1.3) hasta tener assets con procedencia clara. **Nada de la Fase 2 relativo a fondos se ejecuta sin esta respuesta.** |
| Q1 | ¿"Personalización" como submenú SPLIT (recomendado) o dejar las 7 filas sueltas en Ajustes con solo los renombres? | **Submenú**: es lo que pide el encargo y reduce Ajustes de 26 a 20 filas; misma mecánica que "Fecha y hora" |
| Q2 | Ícono de la fila "Personalización" | **`theme`** (queda libre al convertir "Tema" en `InlineValue`, cuyo panel usa `theme-light/dark`); alternativa `paintpalette` |
| Q3 | Ícono de "Temas" (hoy `sync`, reuso documentado en D-289) | Mantener `sync` salvo que el dueño prefiera `paintpalette` para Temas y `sliders-horizontal`… — sin SVG nuevo (D-286/D-004) |
| Q4 | ¿Convertir también **Animaciones**, **Gráficos** (3 valores) y **Color de acento** (6, con swatch pendiente D-087) a `InlineValue`? | **Animaciones y Gráficos sí** (3 valores cortos, ciclo con SELECT — igual que Repetir); **Color de acento no** (6 valores + el dueño tiene pendiente decidir un selector de swatches, D-087 — no cerrar esa decisión por la puerta de atrás). Si el dueño prefiere empezar solo con Modo, el patrón queda igual de definido |
| Q5 | Pantallas de elección de Personalización a nivel 3 siendo SPLIT — ¿se acepta como excepción documentada a la regla "nivel 3 → LISTA-COMPLETA"? | **Sí, documentada** en `03-arbol-de-menus.md` (precedente: Fecha y hora → filas hijas). No cambia ninguna clasificación existente |
| Q6 | ¿Adaptar el acento libre en Modo oscuro (como D-274 hace con el rosa `#FF2D55`→`#FF456C`)? | **No en esta pasada** — es un cambio de color global; anotar en `01-color.md` como pendiente con la fórmula propuesta (aclarar +10 % hacia blanco en oscuro) |
| Q7 | ¿Transición al cambiar Modo? | **No** (instantáneo, §4.2). Si el dueño la quiere: crossfade lineal 330ms con los framebuffers de `aura_transitions.c`, sin patrón nuevo |
| Q8 | La clave `theme:` en `aura.cfg` se conserva (sin migración) — ¿aceptable que el nombre en disco ya no coincida con el texto de UI "Modo"? | **Sí**; el nombre en disco es contrato de facto; comentario en `aura_settings.c` |
| Q9 | Studio: `ExtrasView.swift:52` dice "Ajustes > Estilo, en el iPod" | Actualizar a "Ajustes > Personalización > Temas" en la pasada de Studio (`ST-NNN`), no aquí |

---

## 8. Fase 2 (solo tras aprobación) — commits

| # | Commit | Contenido | Aceptación |
|---|---|---|---|
| 1 | D-292 (1/5): patrón `InlineValue` en `MenuList` | `aura_menu_item_v2_t.value`, render con tinta secundaria y presupuesto de 60px, exclusión de flecha; token `menu_list.inline_value_max_w` | Sim: fila con valor largo + etiqueta larga → etiqueta se recorta, valor entero |
| 2 | (2/5): "Modo" inline + `SelectionSummary` 1:1 | `SETTINGS_THEME` sale de `is_choice_screen`; SELECT alterna; panel `theme-light/dark` + "Claro/Oscuro"; textos Modo/Temas | Sim claro y oscuro; `aura.cfg` conserva `theme:`; ajustes previos sobreviven (probar con un `aura.cfg` existente) |
| 3 | (3/5): sección "Personalización" | `SETTINGS_PERSONALIZATION` (enum al final), `personalization_entries[]`, split, categoría, `03-arbol-de-menus.md` | Sim: Ajustes con 20 filas; submenú con 7; toggles y elecciones funcionan; Q4 si aplica |
| 4 | (4/5): fondos por acento con fallback calculado | `ensure_panel_background()` recibe el nombre por acento; `draw_accent_gradient_background()`; **si Q0 = sí**: 5 `-source.png`, `tokens.json`, `generate.py`, `theme-format-v1.json` | Sim: los 6 presets muestran su imagen; **acento sin imagen** (forzar `accent_rgb24` a un valor fuera de tabla vía `aura.cfg`) → degradado calculado, nunca `pink`; tema instalado con su propio `backgrounds/blue.bmp` lo sobreescribe |
| 5 | (5/5): documentación viva + bitácora | `left-panel.md`/`selector.md` (`InlineValue`), `selection-summary.md` (fondos: correspondencia + fallback + "no varía por Modo"), `sistema/05-temas.md` (Modo ↔ Temas, regla futura "Fijado por el tema"), `01-color.md`, `DECISIONS.md` D-292 (+D-293 si el contrato de temas suma presets → `CONTRATO-formato-tema.md` §E, copia idéntica en Studio, pendiente de esa pasada) | Diff de docs revisado |

Reglas de siempre: build ARM + sim limpios por commit, `make -C firmware/rockbox/apps/aura/test test` sin regresiones, sin RGB hardcodeado (todo por `a26_color()`/`aura_accent*()`/tokens), `lcd_active()` en cualquier animación (aquí no hay ninguna nueva), textos es-MX. Sin push.
