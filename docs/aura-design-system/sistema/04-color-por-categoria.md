# Color por categoría del Menú principal

🟢 Definido — encargo del dueño 2026-08-14 ("fixed per-menu-category
colors cascading to submenu icons"). Ver también
`fundamentos/01-color.md` (tabla de colores) y
`componentes/selection-summary.md` (color del tile).

## Qué resuelve

Antes de este encargo, el ícono de `SelectionSummary` (tile + glifo) y
el glifo de cualquier ícono "activo"/seleccionado en una lista
(`MenuList`, listas de contenido) usaban **siempre** el mismo color: el
acento configurable del usuario, sin importar en qué sección del Menú
principal estuviera el usuario. Video se veía igual que Música, Ajustes
se veía igual que Fotos.

La regla nueva: cada una de las 5 secciones de nivel superior del Menú
principal (Música/Video/Fotos/Extras/Ajustes) tiene su propio color, y
**toda pantalla descendiente de esa sección lo hereda**, sin importar la
profundidad de navegación. Ver la tabla completa de colores en
`fundamentos/01-color.md`.

## Mecanismo

### 1. Mapeo pantalla → categoría (`aura_category.h`/`.c`)

Módulo puro en C99 (mismo criterio que `aura_nav.c`/`aura_color.c`: sin
dependencias de Rockbox, compila igual en `apps/aura/test/`). Expone:

- `aura_category_t` — `NONE`/`MUSIC`/`SETTINGS`/`VIDEO`/`PHOTOS`/`EXTRAS`.
- `aura_category_for_screen(aura_screen_id_t)` — switch exhaustivo sobre
  **todo** `aura_screen_id_t` (`aura_nav.h`), agrupado por prefijo de
  nombre (`AURA_SCREEN_MUSIC_*` → `MUSIC`, `AURA_SCREEN_SETTINGS_*` →
  `SETTINGS`, etc.). Una pantalla nueva que no se agregue aquí cae en
  `AURA_CATEGORY_NONE` (fallback seguro, nunca crash ni color al azar) —
  agregar una pantalla al árbol de menús sin agregarla a este switch es
  un bug de omisión, no un caso cubierto por el diseño.
- `aura_category_set_current()`/`aura_category_current()` — el estado
  "categoría de la pantalla activa", recalculado **una vez por cuadro**
  desde `aura_screens_draw()` (único punto de entrada de dibujo,
  `aura_main.c`) antes de despachar a la pantalla concreta. Mismo patrón
  de estado global "una sola pantalla visible a la vez" que ya usaba
  `aura_widgets.c` (`s_panel_pending_icon`, `aura_widgets_split_active()`)
  — evita tener que agregar un parámetro de categoría a cada una de las
  ~15 funciones `draw_*` de `aura_screens.c` y a cada capa intermedia
  (`aura_menu_list_draw`, `aura_widgets_draw_list`,
  `aura_selection_summary_draw`) que dibuja un ícono.

**Caso especial: la raíz del Menú principal.** `AURA_SCREEN_ROOT` no
tiene sección propia — la categoría real es la del ítem RESALTADO en
ese momento (`root_entries[seleccion].target`), no la pantalla en sí.
`aura_screens_draw()` resuelve este caso aparte antes de llamar al
mapeo genérico.

### 2. Resolución de color (`aura_category_gradient()`, `apple2026_shell.h`/`.c`)

```c
void aura_category_gradient(aura_category_t cat,
                             unsigned *color_a, unsigned *color_center,
                             unsigned *color_b);
```

Devuelve los mismos 3 puntos que ya usaba el degradado diagonal del
tile de `SelectionSummary` (esquina clara / centro / esquina oscura):

- `MUSIC`/`NONE` (fallback): `aura_accent_light()` / `aura_accent()` /
  `aura_accent_dark()` — **exactamente el mismo cálculo de siempre**,
  cero cambio de comportamiento para Música.
- `SETTINGS`/`VIDEO`/`PHOTOS`: el color fijo del token
  (`aura_ds.color.category.*_hex`) mezclado hacia blanco/negro con el
  MISMO porcentaje que ya usaba el acento
  (`AURA_DS_COLOR_ACCENT_DERIVED_LIGHTEN_PCT`/`_DARKEN_PCT`, 25%) —
  reusado tal cual, ninguna razón documentada para que estas categorías
  usen un porcentaje distinto.
- `EXTRAS`: caso especial de DOS TONOS, no luz/sombra de un solo color —
  `color_a` = amarillo (`aura_ds.color.category.extras_yellow_hex`),
  `color_b` = `aura_accent()` (el acento vigente, no un hex fijo),
  `color_center` = mezcla 50/50 de ambos.

Consumidores que solo necesitan 2 tonos (no 3) usan `color_a`/`color_b`
y descartan `color_center`.

### 3. Los dos consumidores reales

**Tile de fondo de `SelectionSummary`** (`aura_selection_summary.c`,
`draw_summary()`): el degradado diagonal de 3 puntos que antes SIEMPRE
usaba `aura_accent_light()/aura_accent()/aura_accent_dark()` ahora pide
los 3 puntos a `aura_category_gradient(aura_category_current(), ...)`.
El glifo/símbolo ENCIMA del tile sigue siendo blanco constante
(variante `-selector`, sin cambios) — la regla de "símbolo claro sobre
tile de color pleno" de `componentes/selection-summary.md` no se toca,
es lo que mantiene el glifo legible sobre CUALQUIER color de tile.

**Glifo "activo"/seleccionado de cualquier ícono** (`aura_widgets.c`,
`draw_icon_variant()`, sufijo `-on` — el que usan
`aura_widgets_draw_icon_selected()`, consumido por `aura_menu_list.c`
y `aura_widgets_draw_list()` para la fila resaltada de cualquier lista):
antes resolvía un color PLANO (`aura_accent()`, vía `variant_ink()`).
Ahora resuelve un degradado verdadero de DOS tonos
(`aura_category_gradient()`, usando `color_a`/`color_b` y descartando
`color_center`) a través de una extensión nueva del compositor de
máscaras de cobertura, `draw_icon_mask_2()`: la MISMA mezcla por píxel
que ya hacía `draw_icon_mask()` contra el framebuffer real (antialias
correcto sobre cualquier fondo), pero interpolando el tono de tinta por
posición diagonal (`(fila+columna)/ancho+alto`, misma convención que el
degradado del tile: claro arriba-izquierda, oscuro abajo-derecha) en
vez de una tinta fija. `draw_icon_mask()` (tinta plana, todavía usada
por las variantes `""`/`-tertiary`/`-rail`/`-selector`) pasó a ser un
envoltorio de una línea sobre `draw_icon_mask_2()` con `ink_a == ink_b`
— mismo resultado exacto que antes, sin rama especial.

Esto es un degradado real por píxel, no una aproximación — confirmado
visualmente incluso a 20px (tamaño de ícono de fila de `MenuList`), no
solo en el tile de 90px de `SelectionSummary`.

**Fallback (sin máscaras en disco):** el camino de respaldo de
`draw_icon_variant()` (bmp pre-compuesto por tema, D-010) no puede
llevar un degradado en tiempo de ejecución — en el caso raro de que
falten las máscaras de cobertura, el ícono `-on` pierde el degradado y
vuelve a un acento plano sin categoría. Degradación aceptable, nunca un
crash; el camino primario (con máscaras, el caso real en disco) siempre
tiene el degradado completo.

## Alcance de este encargo — qué quedó explícitamente afuera

- **Qué íconos de Ajustes se excluyen del gris**: el dueño mencionó que
  "algunos, no todos" los ítems de Ajustes deberían quedar afuera de la
  regla, mecanismo a definir más adelante. Por ahora TODO el subárbol de
  Ajustes es gris, sin excepción — no se adivinó ninguna exclusión.
- **Color del texto de las filas de lista**: sigue siendo
  `aura_accent()` para la fila seleccionada (`aura_menu_list.c`), sin
  cambios — el encargo fue específicamente sobre íconos, no sobre el
  color del texto.
