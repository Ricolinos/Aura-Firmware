# Color

🟢 Definido (resync 2026-08-16) — la paleta base de 9 tokens × 2 temas
(D-072, esquema de las guías "Apple 2026 design language" adoptado tal
cual), el acento configurable, el color por categoría y el fondo del panel
derecho por acento (D-267) viven en `design-system/tokens.json` y se
generan a `apple2026_tokens.h` / `a26_palette`. Quedan abiertos, marcados
🔴 abajo: la interfaz de selección del acento y la relación entre el acento
del tema y el acento del usuario.

## Paleta base (`color.light` / `color.dark`, D-072)

Estos son los tokens del sistema Apple2026 que consume `a26_palette`
(`A26_SHELL_BG`, `A26_TEXT_PRIMARY`, …); varían con el tema claro/oscuro.

| Token (`tokens.json`) | Claro | Oscuro | Uso |
|---|---|---|---|
| `shell_bg` | `#FFFFFF` | `#1C1C1E` | Fondo base de pantalla y de `LeftPanel` |
| `text_primary` | `#000000` | `#FFFFFF` | Texto principal |
| `text_secondary` | `#6E6E73` | `#98989D` | Texto secundario |
| `text_tertiary` | `#3C3C43` | `#C7C7CC` | Texto terciario |
| `accent` | `#FF2D55` | `#FF456C` | Acento **del tema** (`A26_ACCENT`) — ver 🔴 abajo |
| `shell_rail` | `#C6C6C8` | `#3A3A3C` | Rieles, separadores, gris del `ScrollIndicator` (D-086) |
| `progress_fill` | `#3C3C43` | `#E5E5EA` | Relleno de barras de progreso |
| `progress_track` | `#E5E5EA` | `#48484A` | Pista de barras de progreso |
| `selection_fill` | `#E5E5EA` | `#2C2C2E` | Pastilla del `Selector` (D-112) |
| `white_constant` | `#FFFFFF` | `#FFFFFF` | Blanco que no cambia con el tema |

## Tokens definidos

| Token | Valor por default | Uso | Notas |
|---|---|---|---|
| `--color-accent` | `#FF2D52` (`aura_ds.color.accent_default_hex`) | Texto/ícono/flecha del ítem seleccionado en el `Selector` (D-112 — la pastilla es gris `selection_fill`, no de acento), color de la categoría Música, y cualquier otro elemento de contraste/acento | **Configurable por el usuario desde Ajustes** — el hex es solo el valor por default, no un valor absoluto del sistema. Cualquier componente que use este token debe leerlo dinámicamente vía `aura_accent()`/`aura_accent_light()`/`aura_accent_dark()` (derivados ±25%, `accent_derived_lighten_pct`/`_darken_pct`, D-086), no asumirlo fijo. 🔴 **Interfaz de selección sin cerrar (D-087, provisional):** hoy son 6 presets con nombre (Rosa `#FF2D52`, Rojo `#FF3B30`, Naranja `#FF9500`, Verde `#34C759`, Azul `#007AFF`, Morado `#AF52DE`; `accent_presets_hex`) en una lista de elección — pendiente que el dueño confirme si es el diseño final o si sigue previsto un selector visual de swatches. 🔴 **Doble acento (D-086):** `A26_ACCENT` del tema (`#FF2D55`/`#FF456C`, tabla de arriba) y este acento del usuario coexisten como tokens distintos — pendiente documentar quién gana dónde o si se fusionan. |

## Fondo del panel derecho por acento (D-267)

En `split` de menú, el fondo del panel derecho (detrás de
`SelectionSummary`) ya **no** es `shell_bg` plano ni un degradado calculado:
es una **imagen horneada de 160×240 por preset de acento**
(`aura_ds.metrics.right_panel_background.presets`, fuente versionada en
`design-system/assets/panel-backgrounds/<preset>-source.png` →
`out/icons/aura/backgrounds/<preset>.bmp`). Sigue el **acento** del
usuario, no la categoría de la fila (confirmado por mockup del dueño: fila
Fotos con tile naranja de categoría sobre fondo rosa de acento). Estado
interino: solo existe `pink`; cualquier acento sin imagen propia cae a
`pink`, y si el archivo falta se rellena `shell_bg`. Los otros cinco
presets quedan pendientes de que el dueño comparta sus imágenes.

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

## Roles semánticos → tokens reales (D-072, resync 2026-08-16)

Los roles CSS-style de esta tabla no tienen macro 1:1 propia — mapean a los
tokens de la paleta base de arriba y a `a26_palette`:

| Rol | Token real | Uso |
|---|---|---|
| `--color-bg-base` | `shell_bg` → `A26_SHELL_BG` | Fondo de pantalla |
| `--color-bg-panel-left` | `shell_bg` → `A26_SHELL_BG` | `LeftPanel` |
| `--color-bg-panel-right` | imagen `right_panel_background` (split de menú, D-267) / `A26_SHELL_BG` (resto) | Panel derecho |
| `--color-text-primary` | `text_primary` → `A26_TEXT_PRIMARY` | Texto principal |
| `--color-text-secondary` | `text_secondary` → `A26_TEXT_SECONDARY` | Texto secundario |
| `--color-accent` | `aura_ds.color.accent_default_hex` vía `aura_accent()` (usuario); `A26_ACCENT` es el del tema — ver 🔴 arriba | Acento |

Nota: la pantalla del iPod Classic 6G es a color (QVGA 320×240), no hay
restricción de escala de grises — pero sí hay restricción de paleta/gamut
real del panel LCD original que vale la pena documentar aquí si afecta cómo
se ven los colores elegidos.
