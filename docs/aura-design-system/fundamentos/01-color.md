# Color

🟡 Pendiente — traer valores desde las guías "Apple 2026 design language"
que ya construimos en este chat, y formalizarlos aquí como tokens.

## Tokens definidos

| Token | Valor por default | Uso | Notas |
|---|---|---|---|
| `--color-accent` | `#FF2D52` | `Selector` (`componentes/selector.md`) y cualquier otro elemento de contraste/acento | **Configurable por el usuario desde Ajustes** — el hex es solo el valor por default, no un valor absoluto del sistema. Cualquier componente que use este token debe leerlo dinámicamente, no asumirlo fijo. |

## Estructura sugerida (a llenar)

| Token | Valor | Uso |
|---|---|---|
| `--color-bg-base` | — | |
| `--color-bg-panel-left` | — | |
| `--color-bg-panel-right` | — | |
| `--color-text-primary` | — | |
| `--color-text-secondary` | — | |
| `--color-accent` | — | |

Nota: la pantalla del iPod Classic 6G es a color (QVGA 320×240), no hay
restricción de escala de grises — pero sí hay restricción de paleta/gamut
real del panel LCD original que vale la pena documentar aquí si afecta cómo
se ven los colores elegidos.
