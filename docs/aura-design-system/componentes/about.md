# About

🟢 Definido (D-279, actualizado D-280…D-283, 2026-08-16). Pantalla "Ajustes →
Acerca de" — variante dinámica de `SelectionSummary` en `(split)`, con una
barra de almacenamiento por categoría sobre un fondo neutro propio de esta
fila; al pulsar SELECT se expande a una secuencia de **3 páginas**
`FULL-CARRY` con `Shift-and-Reveal` (D-278) para entrar/salir y `Fade-Slide`
de región (D-283) para pasar de una página a otra. Nombre interno en código:
`draw_about*`/`about_storage_collect()` (`aura_screens.c`).

## Dónde vive

- **`(split)`**: variante dinámica de `SelectionSummary` (mismo mecanismo de
  B-04/D-108 que la fila "Fecha y hora") — `icon_renderer` dibuja el badge
  real de Aura (D-269), `panel_top` = "Mi iPod", `bottom_renderer` dibuja la
  barra de almacenamiento, y `background = AURA_SS_BG_NEUTRAL_FADE` (D-281)
  en vez del fondo de imagen por acento que usan las demás filas.
- **`(full)`**: pantalla propia, `FULL-CARRY` (`docs/design/…(2008).md`: "un
  elemento del panel derecho sobrevive a la transición y se estira a
  pantalla completa") — **3 páginas** navegables con SELECT/RIGHT/LEFT:
  **1. Almacenamiento** (barra expandida + 5 filas de categoría),
  **2. Conteos** (canciones/artistas/listas, película/serie/videoclip,
  imágenes/fotografías/IA), **3. Créditos** (atribución GPL v2 + URL del
  código fuente). El tile de Aura **persiste en las 3** (D-283) — es el
  mismo elemento que viajó con `Shift-and-Reveal`, no un dibujo nuevo por
  página.

## Cuándo aparece

Siempre — es una fila fija de Ajustes, no depende de umbral ni de contenido
sincronizado (sin sync, la página 1 y 2 muestran el aviso existente en vez
de datos; la página 3, Créditos, no depende del manifiesto).

## Anatomía

### En `(split)`

| Elemento | Posición | Origen |
|---|---|---|
| Fondo del panel: degradado horizontal gris→`shell_bg` | Toda la columna del panel derecho, calculado en runtime (`draw_neutral_fade_background()`) | D-281 |
| Tile (90×90, degradado por categoría, sombra) | Centro exacto del panel derecho (`aura_selection_summary_tile_rect_split()`) | D-270 |
| Badge de Aura (ícono a color) | Centrado sobre el tile | D-269 |
| "Mi iPod" | Texto superior, Bold 18pt, tinta `A26_TEXT_PRIMARY` (no blanco fijo — D-281, ver "Fondo variante" abajo) | D-264 / D-281 |
| Barra de almacenamiento (**6 segmentos**) | Centrada en el margen inferior del tile (`[tile_y+90, 240)`), misma fórmula que D-272 usa para el texto | D-279 |

### En `(full)`, página 1 — Almacenamiento (estado expandido)

| Elemento | Posición | Token |
|---|---|---|
| Tile (mismo tile, misma función de dibujo) | `(about.expanded_tile_x, tile_y_split)` — mismo eje Y que en split, solo cambia X | `about.expanded_tile_x` = 16 |
| 5 filas de categoría (punto de color + etiqueta + cifra + %) | Apiladas entre `about.expanded_text_top_y` (30) y `about.expanded_text_bottom_y` (190), columna `[about.expanded_bar_x, about.expanded_bar_right)` | — |
| Barra expandida (6 segmentos) | Mismo eje vertical que en split; ancho `about.expanded_bar_x`…`about.expanded_bar_right` (182px) | `about.bar_h` = 12 |

### En `(full)`, página 2 — Conteos

Mismo tile persistente a la izquierda; a la derecha (misma columna
`about.expanded_bar_x`…`bar_right`), dos bloques de 3 líneas cada uno:
Música (canciones, artistas, listas) y, si el manifiesto los trae, Video
(películas, series, videoclips) y Fotos (imágenes, fotografías, IA). Sin
esos campos, cada bloque muestra el aviso "Sincroniza con Aura Studio para
ver el detalle" en vez de un cero engañoso.

### En `(full)`, página 3 — Créditos

Mismo tile persistente a la izquierda; a la derecha, texto envuelto con
`aura_widgets_wrap_text()` en la misma columna de 182px, con scroll por
rueda (`BUTTON_SCROLL_FWD`/`BACK`) porque el texto completo (~18 líneas) no
cabe en las ~13 líneas visibles a ese ancho — mismo patrón que ya usa
Ajustes → Avisos legales (`draw_long_text()`/`handle_legal_text()`), no un
mecanismo nuevo.

Puntos de paginación (3 puntos, sin cambio de posición) al pie de las 3
páginas.

## Estados

- **Colapsado** (`split`): fila de Ajustes con su `SelectionSummary`.
- **Expandido** (`full`): 3 páginas, `SELECT`/`RIGHT` avanza (sin ciclar —
  en la página 3 no hace nada más), `LEFT` retrocede, `MENU` colapsa de
  vuelta a `split` desde cualquier página (nunca un segundo `SELECT`
  colapsa).

## Comportamiento

- **Entrar** (`SELECT` en la fila): `Shift-and-Reveal` (D-278) hacia
  adelante. El tile viaja de `(195, 75)` a `(16, 75)`; `LeftPanel` sale
  empujado en paralelo; el resto de la página 1 aparece con fundido en el
  hueco liberado; la `StatusBar (full)` cae al final.
- **Salir** (`MENU`, en cualquiera de las 3 páginas): inversa exacta de
  `Shift-and-Reveal` — `StatusBar (full)` sube primero, el tile viaja de
  vuelta a `(195, 75)`, `LeftPanel` entra empujado, el contenido de la
  derecha se desvanece.
- **Paginar** (`SELECT`/`RIGHT`/`LEFT` dentro de `(full)`, D-283): **con
  transición** desde D-283 — `Fade-Slide` de una región (no de toda la
  pantalla: `StatusBar`, tile y puntos de paginación no cambian entre
  páginas y no se animan). El contenido saliente se desliza hacia la
  izquierda con fundido de salida mientras el entrante llega desde la
  derecha con fundido de entrada (`SELECT`/`RIGHT`); al revés con `LEFT`.
  Antes de D-283 este cambio era instantáneo.

## Fondo variante del `SelectionSummary` (D-281)

**Por qué**: encargo del dueño — "reemplazar la imagen de fondo rosa por un
degradado de grises que se diluya a blanco, para que la transición al
estado expandido sea más fluida" (el estado expandido siempre fue
`SHELL_BG` plano; la imagen de acento del resto de las filas no se parece
en nada a eso).

**Mecanismo, parametrizado, no un caso especial**: `draw_summary()`
(`aura_selection_summary.c`) gana un parámetro `aura_ss_background_t
background` (`AURA_SS_BG_ACCENT_IMAGE` default / `AURA_SS_BG_NEUTRAL_FADE`),
propagado vía `panel_identity_t.background` — **incluido en
`panel_identity_equal()`**, para que cambiar de variante cuente como cambio
de identidad del panel derecho y dispare el debounce normal (D-262/D-266).
Se evaluó (y se descartó) un `if` especial dentro del componente
comprobando qué `icon_renderer` llegó: más corto en líneas, pero rompe la
dirección de dependencia del componente (B-04/D-108, que recibe todo por
parámetro) y no participa del debounce sin código extra.

**Degradado horizontal, en runtime, sin BMP ni token de color nuevo**:
`draw_neutral_fade_background()` pinta columna por columna con
`a26_shell_blend()` entre dos colores **ya existentes en la paleta**, según
tema: claro `progress_track (#E5E5EA) → shell_bg`; oscuro `shell_rail
(#3A3A3C) → shell_bg`. Horizontal porque la franja derecha del panel ya
termina en el mismo color que tiene el estado expandido (`SHELL_BG` plano)
— durante `Shift-and-Reveal` el fondo casi no cambia en la zona visible, a
diferencia de un degradado vertical o diagonal que hubiera tenido que
replicarse en el expandido o aceptar un salto de color.

**Conflicto encontrado y resuelto antes de shippear**: el texto superior
"Mi iPod" era blanco fijo (D-267, "el fondo siempre es saturado") —
ilegible sobre el degradado nuevo. La tinta ahora depende de la variante:
`ACCENT_IMAGE` conserva blanco fijo; `NEUTRAL_FADE` usa `A26_TEXT_PRIMARY`.

**Extensible**: cualquier otra fila de Ajustes podría pedir este fondo
asignando el campo en su `panel_identity_t` — no se extendió a ninguna otra
fila en esta pasada (fuera de alcance del encargo).

## Sistema separado de Otros, y contorno de contraste (D-282)

**"Sistema"** es un segmento nuevo (el quinto de contenido, sexto contando
Libre): bytes de `/.rockbox/` en la partición de datos, medidos con el mismo
recorrido recursivo con `dircache_ready()` que usa Música (ver abajo), pero
con **caché de sesión completa** en vez de por-entrada — `/.rockbox/` casi
no cambia mientras el dispositivo está encendido. La partición de firmware
de Apple **no se cuenta**: ya no existe en el hardware del dueño (D-185/
D-186, formateada) y de todos modos no sería espacio de Aura. Color: gris
de Ajustes (`AURA_DS_COLOR_CATEGORY_SETTINGS_GRAY`, ya fijado por D-250 —
"Sistema" es semánticamente Ajustes, sin hex nuevo). "Otros" conserva el
amarillo de Extras y ahora es el residual real (playlists, caché de
carátulas, sueltos, holgura de asignación de clusters) sin la parte que ya
se distingue como Sistema.

**Contorno de contraste**: verificado con la fórmula WCAG (luminancia
relativa) antes de tocar nada — amarillo (1.5:1) y naranja (2.2:1) fallan
el umbral 3:1 para elementos gráficos sobre el fondo casi-blanco que D-281
acababa de introducir, y ningún tono de amarillo más oscuro pasa 3:1 sin
dejar de leerse como amarillo. Arreglo: contorno de 1px
(`about.segment_outline_pct = 30`) en los 6 segmentos de la barra y en los
puntos de color de las filas expandidas — `blend` hacia negro (tema claro)
o blanco (tema oscuro), una sola regla para los 6 colores en vez de tocar
los tokens de categoría (globales, ya fijados por encargos anteriores).

**Límite del simulador, no bug**: sin `HAVE_DIRCACHE` (excluido en
`SIMULATOR`), Sistema siempre muestra 0 B ahí — a diferencia de Música,
nunca tuvo un fallback al manifiesto porque nunca vivió en él.

## Fuente del dato y sus límites (D-280…D-283)

`about_storage_collect()` (`aura_screens.c`) combina tres orígenes:

| Categoría | Fuente | Actualidad |
|---|---|---|
| Música | En vivo: recorrido recursivo de `/Music` (`sum_dir_bytes_recursive()`) cuando `dircache_ready()` confirma que `readdir` se sirve de RAM; si no, cae al manifiesto de Aura Studio (`sync_summary.cfg`) | En vivo con dircache listo; si no, solo tan reciente como el último sync (copiar música por Finder no se refleja hasta entonces) |
| Video, Fotos (bytes) | Suma real de `/Videos` y `/Photos` (`dir_get_info().size`) | En vivo, refleja copias por Finder |
| Video, Fotos (conteo por subcategoría: película/serie/videoclip, imagen/fotografía/IA) | `sync_summary.cfg` — Aura Studio ya clasifica cada ítem al importar (`MediaCategory`, `MediaCategoryHeuristics.classifyPhoto`); Rockbox no tiene base de datos de video ni parser EXIF, así que **no puede** clasificar por sí solo | Solo tan reciente como el último sync — sin esos campos (manifiesto de un sync anterior a esta sesión), se muestra el aviso de sincronizar en vez de "0" |
| Música (canciones, listas) | Manifiesto (`music_count`/`playlist_count`) | Como el resto del manifiesto |
| Música (artistas) | En vivo, tagcache (`aura_music_count_artists()`, mismo patrón `tagcache_search`+`set_uniqbuf` que ya usa el navegador de Artistas) | En vivo, más fresco que cualquier sync |
| Sistema | En vivo: recorrido recursivo de `/.rockbox/`, cacheado por sesión | Casi constante mientras el dispositivo está encendido |
| Otros | Residual: `total − libre − música − video − fotos − sistema` | En vivo |
| Libre / Total | `volume_size()` — **corregido en D-280**: multiplicaba por `SECTOR_SIZE` (512) un valor que en realidad está en KiB, duplicando todos los porcentajes de contenido | En vivo |

**Por qué no un escaneo real de `/Music` sin dircache**: recorrer
recursivamente miles de archivos en el disco duro de 2008 sin caché en RAM
tiene un costo real (del orden del escaneo de tagcache al primer arranque)
y Aura no usa hilos de fondo desde los freezes D-204/D-206/D-214 — de ahí
el fallback al manifiesto cuando `dircache_ready()` es falso (incluido
siempre en el simulador).

**Recarga**: el `bottom_renderer` de `(split)` usa `about_storage_collect(false, …)`
(música desde su propio caché por-entrada, no releído en cada cuadro); el
estado expandido relee con `force_reload=true` **solo al entrar** a la
página 1 (`s_about_needs_reload`, D-280 — antes se releía y recorría en
cada uno de los ~20 cuadros/seg que la pantalla se redibuja, bug real
encontrado y corregido de paso).

## Créditos y GPL v2 (Estado 3, D-283)

Aura hereda la GPL v2 de Rockbox (§6: no se pueden imponer restricciones
adicionales a los derechos que la licencia otorga; §3: debe estar
disponible el código fuente). La página 3 de "Acerca de" ahora incluye la
atribución completa (Rockbox, Ricardo Gómez, la URL del repositorio) y la
nota de marca de Apple sin afiliación — reemplaza el resumen de una sola
línea ("Basado en Rockbox") que tenía antes de D-283. De paso se corrigió
`AURA_STR_COPYRIGHT_BODY` (Ajustes → Avisos legales), que tenía una
cláusula "PROHIBIDA su distribución… ESTRICTAMENTE PROHIBIDA su venta"
incompatible con la GPL v2 heredada — retirada, con la URL del código
fuente añadida en su lugar. `LICENSE` (copia literal de
`firmware/rockbox/docs/COPYING`) vive ahora en la raíz del repositorio.

## Relación con SelectionSummary

`About` es una variante dinámica de `SelectionSummary` (B-04/D-108) en
`(split)` — comparte tile, sombra, degradado del tile y el slot superior de
texto; el fondo del panel y la tinta del texto son la única variante propia
de esta fila (D-281); el slot inferior cambia (barra en vez de texto, vía
`bottom_renderer`). El estado expandido es exclusivo de esta fila (ninguna
otra fila de Ajustes tiene un estado `FULL-CARRY` propio hoy).

## Tokens

- `aura_ds.metrics.about.bar_h` = 12 (alto de la barra, split y expandida)
- `aura_ds.metrics.about.segment_min_px` = 2 (ancho mínimo visible de un
  segmento con bytes > 0)
- `aura_ds.metrics.about.segment_outline_pct` = 30 (D-282, contorno de
  contraste)
- `aura_ds.metrics.about.expanded_tile_x` = 16
- `aura_ds.metrics.about.expanded_bar_x` = 122
- `aura_ds.metrics.about.expanded_bar_right` = 304
- `aura_ds.metrics.about.expanded_text_top_y` = 30
- `aura_ds.metrics.about.expanded_text_bottom_y` = 190
- Colores de segmento (6): Música = `aura_accent()`; Video = color fijo de
  categoría (navy, D-250); Fotos = color fijo de categoría (naranja,
  D-250 — **sin verde nuevo**); Sistema = `AURA_DS_COLOR_CATEGORY_SETTINGS_GRAY`
  (D-282); Otros = `AURA_DS_COLOR_CATEGORY_EXTRAS_YELLOW` (amarillo fijo,
  no el degradado hacia el acento que usa el resto del sistema para
  Extras); Libre = `A26_PROGRESS_TRACK`.
- Fondo variante (D-281): **sin token de color nuevo** — reutiliza
  `progress_track`/`shell_rail`/`shell_bg` de la paleta existente, resueltos
  por tema en runtime.

## Origen

- D-081 (Fase 32): pantalla `FULL-COLD` de 3 páginas, barra de 3 colores
  uniformes sin "Otros".
- D-264 (2026-08-15): variante dinámica en `(split)` con badge de Aura y
  barra de 5 segmentos vía `volume_size()`.
- D-269: badge a color completo.
- D-277: extremos de cápsula real en la barra de `(split)`.
- D-278: `aura_transition_shift_and_reveal()`, utilidad reutilizable.
- D-279 (2026-08-16): barra de `(split)` reposicionada; 4 colores por
  categoría; SELECT expande a `FULL-CARRY` con `Shift-and-Reveal`; filas de
  texto con cifra y porcentaje.
- D-280 (2026-08-16): fix del bug ×2 en `volume_size()`; porcentaje con un
  decimal bajo 10%; música medida en vivo con `dircache`.
- D-281 (2026-08-16): fondo variante gris→blanco, parametrizado.
- D-282 (2026-08-16): categoría Sistema separada de Otros (6 segmentos);
  contorno de contraste.
- D-283 (2026-08-16): Estado 2 (conteos, con Aura Studio clasificando
  video/fotos), Estado 3 (créditos + corrección GPL v2), tile persistente
  en las 3 páginas, `Fade-Slide` de región entre páginas.

## Pendiente de definir

- [ ] Presets de acento (`fundamentos/01-color.md`, D-274/C9): el dueño
      dejó pendiente la interfaz de selección — sin choque real hoy (el
      navy de Video se eligió justamente para distinguirse del preset
      azul, D-250).
- [ ] "Fondos de pantalla" como categoría de foto se descartó del encargo
      del Estado 2 (no existe en Aura Studio) — las categorías reales son
      Imágenes/Fotografías/IA.
- [ ] Extender el fondo variante (D-281) a otras filas de Ajustes: el
      mecanismo lo permite, no se hizo en esta pasada.
