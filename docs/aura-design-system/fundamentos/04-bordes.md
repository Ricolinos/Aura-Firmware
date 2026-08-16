# Bordes y Radios

🟢 Definido (resync 2026-08-16) — un solo algoritmo de esquina por corte de
distancia (`a26_shell_fill_rounded_rect()` / `a26_shell_stamp_corners()` /
`a26_shell_round_bitmap_corners()`, `apple2026_shell.c`), sin antialias
salvo donde se indica. Todos los radios viven en `design-system/tokens.json`.

| Token | Valor | Uso | Origen |
|---|---|---|---|
| `layout.corner_radius_screen` | 12px | Las 4 esquinas físicas de pantalla (en `split` solo las 2 izquierdas: D-268) | D-072 |
| `layout.corner_radius_card` | 8px | Tarjetas | D-072 |
| `layout.corner_radius_pill` | 8px | Pastillas genéricas | D-072 |
| `layout.corner_radius_capsule` | 6px | Cápsulas (flotante de espera, etc.) | D-072 |
| `aura_ds.metrics.selector.corner_radius` | 5px | Pastilla del `Selector` | D-097 |
| `aura_ds.metrics.selection_summary.tile_corner_radius` | 28px (~31% de 90px) | Tile de `SelectionSummary`. Arco circular, no squircle G2: al mismo % se percibe menos redondo, por eso 28 y no el 22% literal de Apple | D-211 (8→20), D-236 (20→28) |
| `aura_ds.metrics.cover_flow.corner_radius` | 8px | Carátula en carrusel, reverso, reproductor y vuelo | D-083, D-116 |
| `aura_ds.metrics.search.pill_radius` | 11px (campo interior concéntrico: 6px) | Caja de búsqueda de 34px | encargo 2026-08-13 (`tokens.json`, `search.comment_radius`) |

## Barras finas: siempre cápsula (D-277)

Toda barra del sistema —barra de progreso/volumen del reproductor, sliders
de Brillo y Límite de volumen, pulgar del `ScrollIndicator`, barras de
almacenamiento de "Acerca de" (página y `SelectionSummary`)— lleva **extremos
semicirculares exactos, radio = mitad del grosor**, antialiasados con
cobertura subpíxel (encargo del dueño 2026-08-16: "todas las barras con
esquinas redondeadas, estilo Apple actual"). No es un radio de la tabla de
arriba: se deriva del grosor de cada barra, así que no hay token nuevo.

Cómo se construye, según el caso (todas en `apple2026_shell.c`):

| Caso | Primitiva | Ejemplo |
|---|---|---|
| Barra horizontal de un solo color sobre fondo plano | `a26_shell_fill_capsule()` | Barra del reproductor (ya la usaba desde 2026-08-12) |
| Barra con **interior de varios colores** (segmentos), o **vertical** | Se pinta cuadrada y al final `a26_shell_capsule_ends_over_content()` recorta los dos extremos hacia el fondo (se auto-orienta al lado corto) | Pulgar del `ScrollIndicator` (vertical), barra de almacenamiento de la página Acerca de |
| Barra sobre fondo **no plano** (imagen del panel derecho) | Igual, con `a26_shell_capsule_ends_restore()` restaurando los píxeles reales capturados antes de pintar | Barra de almacenamiento del `SelectionSummary` de Acerca de |
| Relleno de progreso dentro de un carril | Carril y relleno cuadrados; `a26_shell_capsule_tail_over_content()` curva solo la punta del relleno contra el color del carril; después la máscara de la barra completa curva ambos extremos contra el fondo | Sliders de Brillo / Límite de volumen |

Por qué el orden importa en el último caso: el extremo inicial del relleno
coincide con el del carril, así que se redondea **una sola vez** contra lo
que de verdad hay afuera — redondearlo aparte contra el carril pintaba gris
fuera del semicírculo del carril; redondearlo contra el fondo (como hacía
el slider antes de D-277) dejaba muescas blancas dentro de la barra.

Grosores: `scroll_indicator.thickness` = 4px. **Confirmado (D-274,
ratificado por el dueño 2026-08-16):** entre `LeftPanel` y el panel
derecho **no hay línea de borde**; la separación la da únicamente la sombra
(`efectos/01-sombras.md`). No hace falta separador. **Nota de código
(D-277):** hasta esa fecha el código sí pintaba una línea de 1px
`SHELL_RAIL` en la última columna del `LeftPanel` (desde `SelectionSummary`,
`CoverDrift` y el panel limpio) — contradecía esta decisión y, además,
tapaba el casquete derecho del pulgar del `ScrollIndicator`, que vive en esa
misma columna y es del mismo gris. Retirada; ahora código y doc coinciden.
Sin pendientes en este documento.
