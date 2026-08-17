# Color

🟢 Definido (resync 2026-08-16) — la paleta base de 9 tokens × 2 temas
(D-072, esquema de las guías "Apple 2026 design language" adoptado tal
cual), el acento configurable, el color por categoría y el fondo del panel
derecho por acento (D-267) viven en `design-system/tokens.json` y se
generan a `apple2026_tokens.h` / `a26_palette`. Queda abierto, marcado
🔴 abajo: la interfaz de selección del acento (el dueño lo definirá más
adelante). La relación entre el acento del tema y el del usuario quedó
resuelta en D-274.

**D-289 (2026-08-17, sistema de temas):** de los 9 tokens de la tabla de
abajo, 8 (todos salvo `accent`, que sigue siendo 100% el ajuste del
usuario — ver "Rosa de fábrica y acento del tema") son **sustituibles por
un estilo instalado** (`sistema/05-temas.md`): la tabla de abajo describe
los valores del tema COMPILADO por defecto ("Aura"); un tema alternativo
puede traer sus propios 8 × 2 valores, heredando del default lo que no
declare. Los 4 colores fijos de categoría (Ajustes/Video/Fotos/el
amarillo de Extras, ver `sistema/04-color-por-categoria.md`) también son
sustituibles por tema. Formato exacto de las claves en
`CONTRATO-formato-tema.md`.

## Paleta base (`color.light` / `color.dark`, D-072)

Estos son los tokens del sistema Apple2026 que consume `a26_palette`
(`A26_SHELL_BG`, `A26_TEXT_PRIMARY`, …); varían con el tema claro/oscuro.

| Token (`tokens.json`) | Claro | Oscuro | Uso |
|---|---|---|---|
| `shell_bg` | `#FFFFFF` | `#1C1C1E` | Fondo base de pantalla y de `LeftPanel` |
| `text_primary` | `#000000` | `#FFFFFF` | Texto principal |
| `text_secondary` | `#6E6E73` | `#98989D` | Texto secundario |
| `text_tertiary` | `#3C3C43` | `#C7C7CC` | Texto terciario |
| `accent` | `#FF2D55` | `#FF456C` | Acento **del tema** (`A26_ACCENT`). El valor claro es el mismo rosa de fábrica del usuario; el oscuro es su adaptación al tema oscuro (D-274, ver "Rosa de fábrica y acento del tema" abajo) |
| `shell_rail` | `#C6C6C8` | `#3A3A3C` | Rieles, separadores, gris del `ScrollIndicator` (D-086) |
| `progress_fill` | `#3C3C43` | `#E5E5EA` | Relleno de barras de progreso |
| `progress_track` | `#E5E5EA` | `#48484A` | Pista de barras de progreso |
| `selection_fill` | `#E5E5EA` | `#2C2C2E` | Pastilla del `Selector` (D-112) |
| `white_constant` | `#FFFFFF` | `#FFFFFF` | Blanco que no cambia con el tema |

## Tokens definidos

| Token | Valor por default | Uso | Notas |
|---|---|---|---|
| `--color-accent` | `#FF2D55` (`aura_ds.color.accent_default_hex`, D-274) | Texto/ícono/flecha del ítem seleccionado en el `Selector` (D-112 — la pastilla es gris `selection_fill`, no de acento), color de la categoría Música, y cualquier otro elemento de contraste/acento | **Configurable por el usuario desde Ajustes** — el hex es solo el valor por default, no un valor absoluto del sistema. Cualquier componente que use este token debe leerlo dinámicamente vía `aura_accent()`/`aura_accent_light()`/`aura_accent_dark()` (derivados ±25%, `accent_derived_lighten_pct`/`_darken_pct`, D-086), no asumirlo fijo. 🔴 **Interfaz de selección sin cerrar (D-087, provisional):** hoy son 6 presets con nombre (Rosa `#FF2D55`, Rojo `#FF3B30`, Naranja `#FF9500`, Verde `#34C759`, Azul `#007AFF`, Morado `#AF52DE`; `accent_presets_hex`) en una lista de elección — pendiente que el dueño confirme si es el diseño final o si sigue previsto un selector visual de swatches (el dueño lo definirá más adelante — 2026-08-16, tiene varias cosas por definir). |

### Rosa de fábrica y acento del tema (resuelto, D-274)

El rosa de fábrica es **`#FF2D55`** (D-274, decisión del dueño 2026-08-16;
antes `#FF2D52`) — mismo valor que `A26_ACCENT` del tema claro. El
`#FF456C` de `A26_ACCENT` en tema oscuro es la **adaptación del mismo rosa
al tema oscuro** (el mismo hex no se ve bien en ambos temas — explicación
del dueño), no un segundo acento. `aura_accent()` (configurable) y
`A26_ACCENT` (del tema) siguen siendo tokens distintos **por consumidor**:
el primero es el que elige el usuario y lo consumen `Selector`, la
categoría Música y todo el sistema nuevo; el segundo lo consumen todavía
algunas pantallas heredadas (páginas de "Acerca de", el riel A-Z, diálogos
de confirmación del sistema viejo). Al elegir "Rosa" en Ajustes, ambos
coinciden en tema claro.

**Pendiente (D-292, no resuelto en esta pasada):** el acento **libre**
(cualquiera de los 6 presets, o un color fuera de la lista) no tiene hoy
una adaptación por Modo como la que D-274 le dio al rosa de fábrica
(`#FF2D55` claro → `#FF456C` oscuro) — `aura_accent()` devuelve el mismo
hex sin importar el Modo activo. Si se decide extender esa adaptación al
acento configurable, la fórmula propuesta es la misma idea de D-274
(+10% hacia blanco en Modo oscuro, a falta de una hecha a mano por color
como la del rosa), calculada en `aura_accent()`, no hardcodeada por
preset.

## Fondo del panel derecho por acento (D-267, D-292)

En `split` de menú, el fondo del panel derecho (detrás de
`SelectionSummary`) no es `shell_bg` plano: es una **imagen horneada de
160×240 por preset de acento**
(`aura_ds.metrics.right_panel_background.presets`, fuente versionada en
`design-system/assets/panel-backgrounds/<preset>-source.png` →
`out/icons/aura/backgrounds/<preset>.bmp`) cuando el acento activo
coincide con uno de los 6 presets con imagen, o un **degradado calculado
desde el acento** (D-292: `aura_accent_dark()` → `aura_accent()` →
`aura_accent_light()`) cuando no. Sigue el **acento** del usuario, no la
categoría de la fila (confirmado por mockup del dueño: fila Fotos con
tile naranja de categoría sobre fondo rosa de acento). Detalle completo
(correspondencia, procedencia de las 6 fotos, por qué el fallback nunca
es un color plano) en `componentes/selection-summary.md`.

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
| `--color-accent` | `aura_ds.color.accent_default_hex` vía `aura_accent()` (usuario); `A26_ACCENT` es el del tema — relación resuelta en D-274, ver "Rosa de fábrica y acento del tema" | Acento |

Nota: la pantalla del iPod Classic 6G es a color (QVGA 320×240), no hay
restricción de escala de grises — pero sí hay restricción de paleta/gamut
real del panel LCD original que vale la pena documentar aquí si afecta cómo
se ven los colores elegidos.
