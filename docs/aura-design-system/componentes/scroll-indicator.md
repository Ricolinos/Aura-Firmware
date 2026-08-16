# ScrollIndicator

Barra de desplazamiento de las listas de Aura: dentro de `LeftPanel`
(`componentes/left-panel.md`) y, desde D-275, **el mismo componente** en las
listas de contenido a pantalla completa (ver "En listas a pantalla completa"
abajo), en el reverso de `CoverFlow` y en las listas de álbumes/playlists.

## Posición

Dentro del panel izquierdo, del lado derecho, ocupando el espacio del
padding derecho del panel (los 4px de margen) — no resta espacio del área
de contenido de 152px, usa el padding mismo como su track.

## Dimensiones

- **Grosor: 4px** — coincide exacto con el ancho del padding derecho que
  ocupa.
- **Puntas redondeadas** (tipo pill/cápsula) — **cápsula vertical real desde
  D-277**: semicírculos exactos de radio 2px (mitad del grosor) con
  antialias subpíxel, vía `a26_shell_capsule_ends_over_content()`. Antes,
  `fill_rounded_rect` con radio 2 solo quitaba un píxel por esquina y a 4px
  de grosor se leía cuadrado. Regla general de barras en
  `fundamentos/04-bordes.md`. En `LeftPanel` el casquete derecho quedaba
  además tapado por una línea de 1px del mismo gris en la última columna del
  panel — retirada en D-277 (la doc ya decía desde D-274 que ahí no hay
  línea, solo sombra).
- **Tamaño fijo** (no proporcional al contenido visible/total como un
  scrollbar estándar). Solo aparece si la lista tiene **más de 10 ítems**
  — si todo cabe en la pantalla, no se renderiza en absoluto.

- **Alto fijo: 24px** (`scroll_indicator.height` en `tokens.json` —
  propuesto bajo mandato autónomo en D-086 G8 y **ratificado por el dueño
  el 2026-08-16, D-274**; ya no es provisional).

## Color

**Gris neutro** — no usa `--color-accent` (a diferencia de `Selector`).
Reutiliza el gris de riel ya definido en la paleta, `SHELL_RAIL`
(`fundamentos/01-color.md`): **`#C6C6C8`** en tema claro, **`#3A3A3C`** en
tema oscuro (D-086, G8 — no se inventó un segundo gris; es el gris neutro
real del sistema).

## Comportamiento

**Aparece con cualquier movimiento de selección** dentro de `MenuList`,
aunque ese movimiento no cause un desplazamiento visual de la lista — no
está condicionado a que la lista realmente se desplace.

**Se desvanece tras 1.5 segundos de inactividad (idle)** — patrón
`Fade-on-Idle` (`transiciones/00-vocabulario.md`), timing ya confirmado
para este componente (`scroll_indicator.idle_before_fade_ms = 1500`).

**Se desliza/mueve indicando la posición actual dentro de la lista**, y lo
hace de forma **animada/suave, siguiendo el scroll en tiempo real** — a
diferencia de `Selector`, que salta directo sin animación.

**Cómo se mueve (D-275)** — dos reglas, ambas necesarias para que se lea
como el iPod Classic:

1. **La posición es proporcional al ítem seleccionado**, no a la ventana
   visible: `thumb_y = track_y + (track_h − 24) × selected ⁄ (count − 1)`.
   Antes seguía a `first` (el primer ítem visible), que con 7 filas
   visibles no cambia mientras la selección recorre las primeras 3 ni las
   últimas 3 — el pulgar se quedaba quieto arriba, luego saltaba, luego
   se quedaba quieto abajo (el "estático" que reportó el dueño).
2. **Entre una posición y la siguiente se desliza**, lineal, en
   **150ms** (`scroll_indicator.slide_ms` — contenido, no control, así que
   fundido lineal, sin resorte; misma cadencia que el fade-in). Patrón
   "redirigir sin salto": si la rueda vuelve a girar a mitad del recorrido,
   el pulgar arranca desde donde iba. El estado del deslizamiento vive
   dentro del componente; se planta sin animar cuando cambia el carril
   (otra lista/pantalla) o cuando el indicador acaba de reaparecer tras
   desvanecerse — nunca "vuela" desde una posición que el usuario no vio.

**Fundido asimétrico (D-275)** — como en el iPod Classic, la barra
**aparece rápido y se va lento**:

| Tramo | Duración | Token |
|---|---|---|
| Aparición (fade-in) | **150ms** — la cadencia estándar de fundido lineal del sistema | `scroll_indicator.fade_in_ms` |
| Persistencia | mientras haya movimiento, y 1.5s después del último | `scroll_indicator.idle_before_fade_ms` |
| Desvanecido (fade-out) | **500ms** — "lenta, ~500ms" | `scroll_indicator.fade_out_ms` |

Antes de D-275 un solo valor (`fade_duration_ms = 500`) gobernaba ambos
sentidos: la barra tardaba medio segundo en verse tras cada movimiento —
el "ligero delay" que reportó el dueño. La doc siempre pidió "lenta" solo
para el desvanecido; el código lo había aplicado también a la aparición.

## En listas a pantalla completa (LISTA-COMPLETA) — D-275

Las listas de contenido a pantalla completa (canciones, artistas, Ajustes
en `(full)`… — `aura_widgets_draw_list()`) usan **este mismo componente**,
con las mismas dimensiones, color, umbral y comportamiento de arriba. Lo
único que cambia es el carril:

| | `LeftPanel` `(split)` | LISTA-COMPLETA `(full)` |
|---|---|---|
| Carril | Padding derecho del panel (4px) | Columna derecha de la pantalla, con **2px de margen** al borde (`scroll_indicator.inset_full`) |
| Track vertical | Los 217px de las 7 filas del panel | Desde el tope de la lista (StatusBar + 4px) hasta el borde inferior |
| Umbral | > 10 ítems | > 10 ítems (7 filas visibles, igual que `LeftPanel`) |
| Convivencia con `IndexRail` | No aplica (el riel solo existe en `(full)`) | **Excluyentes** (D-276, Q2a): con ≥ 12 elementos se monta el riel y el `ScrollIndicator` no se dibuja; con 11 se ve el indicador; con ≤ 10 nada. Detalle en `componentes/index-rail.md` |

**Historia, para que nadie la reviva**: hasta D-275 estas listas dibujaban
un scrollbar **propio del sistema viejo** (D-073, `docs/design/Reglas de
diseño Apple2026 (v2).md` §5.3): 3px, cápsula r1.5, alto **proporcional**
con mínimo 24px, fundido 150ms/~800ms/330ms, y umbral `count > filas
visibles` (7) — por eso una lista de 8-10 ítems mostraba barra aunque
esta doc siempre dijo "más de 10" (el bug que reportó el dueño), y por
eso su timing y grosor no coincidían con esta spec. Esa versión **quedó
reemplazada** por este componente; sus valores ya no existen en el
código ni en `tokens.json`. Ante cualquier conflicto entre §5.3 de la
base histórica y este archivo, gana este archivo.

## Pendiente de definir

- [x] Alto exacto del indicador — resuelto: 24px (D-086 G8, ratificado por
      el dueño en D-274)
- [x] Valor hex del gris neutro — resuelto: `SHELL_RAIL` del tema,
      `#C6C6C8` / `#3A3A3C` (D-086 G8, ratificado en D-274)
