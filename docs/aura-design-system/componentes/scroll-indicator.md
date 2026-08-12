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

🔴 Pendiente: alto exacto en px del indicador (sabemos que es de tamaño
fijo, falta el valor).

## Color

**Gris neutro** — no usa `--color-accent` (a diferencia de `Selector`).
Valor hex exacto pendiente (ver `fundamentos/01-color.md`).

## Comportamiento

**Aparece con cualquier movimiento de selección** dentro de `MenuList`,
aunque ese movimiento no cause un desplazamiento visual de la lista — no
está condicionado a que la lista realmente se desplace.

**Se desvanece tras 1.5 segundos de inactividad (idle)** — patrón
`Fade-on-Idle` (`transiciones/00-vocabulario.md`), timing ya confirmado
para este componente.

**Se desliza/mueve indicando la posición actual dentro de la lista**, y lo
hace de forma **animada/suave, siguiendo el scroll en tiempo real** — a
diferencia de `Selector`, que salta directo sin animación.

**Duración del fade (aparecer/desaparecer): lenta, ~500ms o más.**

## Pendiente de definir

- [ ] Alto exacto del indicador (tamaño fijo, falta el valor en px)
- [ ] Valor hex del gris neutro
