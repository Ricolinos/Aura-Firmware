# Color

🟡 Pendiente — traer valores desde las guías "Apple 2026 design language"
que ya construimos en este chat, y formalizarlos aquí como tokens.

## Tokens definidos

| Token | Valor por default | Uso | Notas |
|---|---|---|---|
| `--color-accent` | `#FF2D52` | `Selector` (`componentes/selector.md`) y cualquier otro elemento de contraste/acento | **Configurable por el usuario desde Ajustes** — el hex es solo el valor por default, no un valor absoluto del sistema. Cualquier componente que use este token debe leerlo dinámicamente, no asumirlo fijo. |

## Color por categoría del Menú principal (confirmado 2026-08-14)

Encargo del dueño: los íconos del Menú principal (y todo lo que cuelga de
cada sección, sin importar la profundidad) ya no son todos del mismo
color genérico — cada sección tiene su propio color, y ese color **se
hereda en cascada** por todas las pantallas descendientes. Entrar a
Ajustes → Pantalla → Brillo sigue siendo "categoría Ajustes" tres
niveles abajo — el color no se resetea al navegar.

| Categoría | Color | ¿Sigue el acento/tema? |
|---|---|---|
| Música (y todo su árbol: Artistas, Álbumes, Playlists, Ahora suena, Canciones aleat., etc.) | `--color-accent` (el mismo acento configurable de siempre) | Sí — comportamiento sin cambios respecto a antes de este encargo |
| Ajustes | Gris fijo, `#8E8E93` (`aura_ds.color.category.settings_gray_hex`) | No — fijo, no varía con el tema claro/oscuro ni con el acento |
| Video | Azul marino fijo, `#1E3A5F` (`aura_ds.color.category.video_hex`) | No |
| Fotos | Naranja fijo, `#FF9500` (`aura_ds.color.category.photos_hex`) | No |
| Extras | Degradado entre DOS tonos: amarillo `#FFCC00` (`aura_ds.color.category.extras_yellow_hex`) → `--color-accent` | El extremo "oscuro" del degradado SÍ sigue el acento configurable |

**Regla del degradado — dos casos distintos, no confundirlos:**
- Música/Ajustes/Video/Fotos: degradado entre una sombra **más clara** y
  una **más oscura** del MISMO color (igual mecanismo que ya existía para
  el acento: `AURA_DS_COLOR_ACCENT_DERIVED_LIGHTEN_PCT`/`_DARKEN_PCT`,
  25%, reusado tal cual para las categorías fijas — ninguna razón para
  que usen un porcentaje distinto al que ya eligió el dueño).
- Extras: degradado entre DOS TONOS distintos (amarillo → acento), no
  luz/sombra de un solo color. Caso especial, documentado aparte en cada
  punto de uso (`aura_category_gradient()`, `apple2026_shell.h`).

**"Todos los íconos" incluye el vidrio (glifo), no solo el tile de fondo:**
esto NO es solo el color de fondo de `SelectionSummary` (`tile de
90×90px` — ver `componentes/selection-summary.md`) — también aplica al
glifo blanco de cualquier ícono en estado "activo"/seleccionado en
CUALQUIER lista (`MenuList`, listas de contenido), que antes siempre se
teñía de acento sin importar la sección. Mecanismo completo, alcance,
excepciones (Ajustes: por ahora TODO su subárbol es gris, sin excluir
ningún ítem — el dueño pidió refinar cuáles quedan afuera más adelante,
no es una pantalla vacía de este encargo) y el mapeo pantalla→categoría
completo: `sistema/04-color-por-categoria.md`.

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
