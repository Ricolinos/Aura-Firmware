# Breakpoints / Estados de Pantalla

🟢 Definido (resync 2026-08-16). Los dos estados centrales vienen de
`sistema/01-capas-y-jerarquia.md`; las medidas reales viven en
`design-system/tokens.json` (`aura_ds.metrics.left_panel`, `statusbar`) y
el código decide el estado con `aura_widgets_split_active()`:

| Estado | Descripción | Geometría (D-072/D-086) |
|---|---|---|
| `split` | Panel izquierdo (menú) + panel derecho visibles simultáneamente | `LeftPanel` 160px + panel derecho 160px; `StatusBar` de 20px de alto sobre el panel izquierdo (`width_split` = 160) |
| `full` | Un solo panel ocupa el ancho completo | 320px; `StatusBar` de 20px a ancho completo (`width_full` = 320) |

Qué pantalla usa qué estado (la regla de asignación menú-vs-contenido) se
define en `sistema/02-navegacion-menus-contenido.md`, no aquí.

A diferencia de un breakpoint responsivo web (que reacciona al tamaño de
viewport), aquí ambos estados corren en la misma pantalla fija de 320×240 —
el "breakpoint" es un estado de navegación/UI, no un tamaño de dispositivo.
Vale la pena aclarar esto en la doc para que no se confunda con RWD clásico.
