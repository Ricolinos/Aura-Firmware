# Breakpoints / Estados de Pantalla

🟡 Pendiente formalizar como tabla, pero ya sabemos los dos estados
centrales desde `sistema/01-capas-y-jerarquia.md`:

| Estado | Descripción |
|---|---|
| `split` | Panel izquierdo (menú) + panel derecho visibles simultáneamente |
| `full` | Un solo panel ocupa el ancho completo de los 320px |

A diferencia de un breakpoint responsivo web (que reacciona al tamaño de
viewport), aquí ambos estados corren en la misma pantalla fija de 320×240 —
el "breakpoint" es un estado de navegación/UI, no un tamaño de dispositivo.
Vale la pena aclarar esto en la doc para que no se confunda con RWD clásico.
