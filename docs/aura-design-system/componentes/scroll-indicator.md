# ScrollIndicator

Barra de desplazamiento dentro de `LeftPanel` (`componentes/left-panel.md`).

## Posición

Dentro del panel izquierdo, del lado derecho, ocupando el espacio del
padding derecho del panel (los 4px de margen) — no resta espacio del área
de contenido de 152px, usa el padding mismo como su track.

## Dimensiones

- **Grosor: 4px** — coincide exacto con el ancho del padding derecho que
  ocupa.
- **Puntas redondeadas** (tipo pill/cápsula).
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

## Pendiente de definir

- [x] Alto exacto del indicador — resuelto: 24px (D-086 G8, ratificado por
      el dueño en D-274)
- [x] Valor hex del gris neutro — resuelto: `SHELL_RAIL` del tema,
      `#C6C6C8` / `#3A3A3C` (D-086 G8, ratificado en D-274)
