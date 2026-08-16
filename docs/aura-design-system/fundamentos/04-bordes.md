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

Grosores: `scroll_indicator.thickness` = 4px. **Confirmado (D-274,
ratificado por el dueño 2026-08-16):** entre `LeftPanel` y el panel
derecho **no hay línea de borde**; la separación la da únicamente la sombra
(`efectos/01-sombras.md`). No hace falta separador. Sin pendientes en este
documento.
