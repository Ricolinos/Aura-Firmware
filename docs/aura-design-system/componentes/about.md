# About

🟢 Definido (D-279, 2026-08-16). Pantalla "Ajustes → Acerca de" — variante
dinámica de `SelectionSummary` en `(split)`, con una barra de almacenamiento
por categoría; al pulsar SELECT se expande a una pantalla `FULL-CARRY`
propia con `Shift-and-Reveal` (D-278). Nombre interno en código: `draw_about*`
(`aura_screens.c`).

## Dónde vive

- **`(split)`**: variante dinámica de `SelectionSummary` (mismo mecanismo de
  B-04/D-108 que la fila "Fecha y hora") — `icon_renderer` dibuja el badge
  real de Aura (D-269) en vez de un ícono estático, `panel_top` = "Mi iPod",
  `bottom_renderer` dibuja la barra de almacenamiento.
- **`(full)`**: pantalla propia, `FULL-CARRY` (`docs/design/…(2008).md`: "un
  elemento del panel derecho sobrevive a la transición y se estira a
  pantalla completa") — 3 páginas navegables con SELECT/RIGHT/LEFT
  (almacenamiento, contadores, dispositivo), igual que antes de D-279; solo
  la página 1 cambió de contenido.

## Cuándo aparece

Siempre — es una fila fija de Ajustes, no depende de umbral ni de contenido
sincronizado (sin sync, muestra el aviso existente en vez de datos).

## Anatomía

### En `(split)`

| Elemento | Posición | Origen |
|---|---|---|
| Tile (90×90, degradado por categoría, sombra) | Centro exacto del panel derecho (`aura_selection_summary_tile_rect_split()`) | D-270 |
| Badge de Aura (ícono a color, no el glifo monocromo "info" de la fila) | Centrado sobre el tile | D-269 |
| "Mi iPod" | Texto superior, Bold 18pt | D-264 |
| Barra de almacenamiento (5 segmentos) | **Centrada en el margen inferior del tile** (`[tile_y+90, 240)`), misma fórmula que D-272 usa para el texto — antes pegada al borde del tile, ~27px más arriba de donde queda ahora | D-279 |

### En `(full)`, estado expandido

| Elemento | Posición | Token |
|---|---|---|
| Tile (mismo tile, misma función de dibujo) | `(about.expanded_tile_x, tile_y_split)` — **mismo eje Y que en split**, solo cambia X | `about.expanded_tile_x` = 16 |
| 4 filas de categoría (punto de color + etiqueta + cifra) | Apiladas entre `about.expanded_text_top_y` (30) y `about.expanded_text_bottom_y` (190), columna `[about.expanded_bar_x, about.expanded_bar_right)` | — |
| Barra expandida (5 segmentos) | Mismo eje vertical que en split (centrada en el margen inferior del tile); ancho `about.expanded_bar_x`…`about.expanded_bar_right` (182px) | `about.bar_h` = 12 (mismo alto que en split) |
| Puntos de paginación | Sin cambio (3 páginas: almacenamiento/contadores/dispositivo) | — |

El tile es el **mismo elemento** en los dos estados — literalmente el mismo
bitmap capturado que viaja con `Shift-and-Reveal` — no dos dibujos
coincidentes por casualidad: `aura_selection_summary_draw_tile()` es la
única función que lo dibuja en ambos sitios.

## Estados

- **Colapsado** (`split`): fila de Ajustes con su `SelectionSummary`.
- **Expandido** (`full`): 3 páginas, `SELECT`/`RIGHT` avanza, `LEFT`
  retrocede, `MENU` colapsa de vuelta a split (nunca un segundo `SELECT`
  colapsa — `SELECT` solo pagina).

## Comportamiento

- **Entrar** (`SELECT` en la fila, con la variante ya comprometida —
  `aura_screens_about_reveal_active()`): `Shift-and-Reveal` hacia adelante.
  El tile viaja de `(195, 75)` a `(16, 75)`; `LeftPanel` sale empujado en
  paralelo; el resto de la pantalla (filas de categoría + barra expandida)
  aparece con fundido en el hueco liberado; la `StatusBar (full)` cae al
  final.
- **Salir** (`MENU`, en cualquiera de las 3 páginas): inversa exacta —
  `StatusBar (full)` sube primero, el tile viaja de vuelta a `(195, 75)`,
  `LeftPanel` entra empujado, el contenido de la derecha se desvanece.
- **Paginar** (`SELECT`/`RIGHT`/`LEFT` dentro de `(full)`): sin transición
  propia, cambio instantáneo (como antes de D-279) — solo la página 1
  cambió de contenido, las páginas 2 y 3 no se tocaron.

## Fuente del dato y sus límites

`about_storage_collect()` (`aura_screens.c`, D-279) combina dos orígenes,
sin escaneo del sistema de archivos:

| Categoría | Fuente | Actualidad |
|---|---|---|
| Música | `sync_summary.cfg` (manifiesto que escribe Aura Studio en cada sync) | Solo tan reciente como el último sync desde Aura Studio — copiar música por Finder no se refleja hasta la siguiente sincronización |
| Video, Fotos | Suma real de `/Videos` y `/Photos` (`dir_get_info().size`, sin `stat()` extra — mismo costo que ya paga la lista de esas carpetas) | En vivo, cada vez que se dibuja — refleja copias por Finder |
| Otros | Residual: `total − libre − música − video − fotos` (firmware, listas de reproducción, caché de carátulas, sueltos) | En vivo |
| Libre / Total | `volume_size()` (contador de clusters libres de FAT) | En vivo |

**Por qué no un escaneo real de `/Music`**: recorrer recursivamente miles de
archivos en el disco duro de 2008 tiene un costo real (del orden del
escaneo de tagcache al primer arranque) y Aura no usa hilos de fondo desde
los freezes D-204/D-206/D-214 — un escaneo síncrono en el hilo de UI cada
vez que se abre esta pantalla no es aceptable. Limitación documentada, no
un descuido.

**Recarga**: el `bottom_renderer` de `(split)` usa el caché de manifiesto de
una vez por sesión (`cached_manifest()`, igual que el resto del panel
derecho); el estado expandido relee el manifiesto fresco cada vez que se
dibuja (`about_storage_collect(force_reload=true, …)`) — mismo
comportamiento que ya tenía esta pantalla antes de D-279.

## Relación con SelectionSummary

`About` es una variante dinámica de `SelectionSummary` (B-04/D-108) en
`(split)` — comparte tile, sombra, degradado y el slot superior de texto;
solo el slot inferior cambia (barra en vez de texto, vía `bottom_renderer`).
El estado expandido es exclusivo de esta fila (ninguna otra fila de Ajustes
tiene un estado `FULL-CARRY` propio hoy).

## Tokens

- `aura_ds.metrics.about.bar_h` = 12 (alto de la barra, split y expandida)
- `aura_ds.metrics.about.segment_min_px` = 2 (ancho mínimo visible de un
  segmento con bytes > 0)
- `aura_ds.metrics.about.expanded_tile_x` = 16
- `aura_ds.metrics.about.expanded_bar_x` = 122
- `aura_ds.metrics.about.expanded_bar_right` = 304
- `aura_ds.metrics.about.expanded_text_top_y` = 30
- `aura_ds.metrics.about.expanded_text_bottom_y` = 190
- Colores de segmento: Música = `aura_accent()` (acento configurable, D-274
  C10); Video/Fotos = color fijo de categoría vigente (`aura_ds.color.category.*`,
  D-250 — **sin verde nuevo**, ver Origen); Otros = `extras_yellow_hex`
  (amarillo fijo, no el degradado hacia el acento que usa el resto del
  sistema para Extras); Libre = `A26_PROGRESS_TRACK`.

## Origen

- D-081 (Fase 32): pantalla `FULL-COLD` de 3 páginas, barra de 3 colores
  uniformes sin "Otros" (limitación explícita: sin `volume_size()` todavía).
- D-264 (2026-08-15): variante dinámica en `(split)` con badge de Aura y
  barra de 5 segmentos (Música/Video/Fotos/Otros/Libre) vía `volume_size()`.
- D-269: badge a color completo (antes ícono "ipod" monocromo).
- D-277: extremos de cápsula real en la barra de `(split)` (antes cuadrada,
  o con esquinas blancas sobre la imagen del panel).
- D-278: `aura_transition_shift_and_reveal()`, utilidad reutilizable.
- D-279 (encargo del dueño, 2026-08-16): barra de `(split)` reposicionada a
  la altura del texto inferior; 4 colores por categoría vigente (no verde
  nuevo — hubiera roto D-250); SELECT expande a `FULL-CARRY` con
  `Shift-and-Reveal`; filas de texto con cifra y porcentaje del disco.

## Pendiente de definir

- [ ] Presets de acento (`fundamentos/01-color.md`, D-274/C9): el dueño
      dejó pendiente la interfaz de selección — cuando se resuelva, revisar
      si cambia algo del choque potencial acento-música vs. color-video en
      esta barra (hoy no hay choque real: el navy de Video se eligió
      justamente para distinguirse del preset azul, D-250).
- [ ] "Otros" solo se puede leer como el residual del disco — si algún día
      Aura mide bytes reales de Extras (juegos, notas, calendarios), este
      segmento pasaría de aproximado a exacto.
