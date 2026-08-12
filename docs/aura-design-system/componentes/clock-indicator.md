# ClockIndicator

Nombre provisional. El elemento de hora dentro de `StatusBar`.

## Visibilidad (configurable por el usuario)

Tres modos:

- **Persistente** — siempre visible.
- **Auto-oculta / por atajo** — **confirmado: se oculta después de 10
  segundos** de estar visible, sin importar cómo se haya mostrado
  (por atajo o por el modo "auto-oculta" por defecto).
- **Por atajo** — se revela manteniendo presionado el botón Select unos
  segundos. ⚠️ Falta confirmar que este atajo no choque con otras funciones
  que ya usen "mantener Select" presionado.

## Dimensiones y formato

- Alto: 10px
- Ancho máximo: 40px
- Formato: `HH:MM`, nunca segundos. 24 horas o AM/PM — configurable por el
  usuario.

## Posición y comportamiento por estado

### `(split)`

Centrada en la barra de estado, tanto horizontal como verticalmente.

Entrada/salida: patrón `Drop-and-Lift`
(`transiciones/00-vocabulario.md`) — entra cayendo desde arriba de la
pantalla, sale subiendo hacia arriba. Se oculta automáticamente a los 10
segundos de estar visible (ver arriba).

### `(full)`

A la izquierda de la barra de estado. Interactúa directamente con
`DynamicTitle` (`componentes/dynamic-title.md`):

- Si `ClockIndicator` es **persistente** → `DynamicTitle` queda siempre
  centrado en la barra.
- Si `ClockIndicator` **no se renderiza por defecto** → `DynamicTitle`
  ocupa su lugar, alineado a la izquierda.
- Si el usuario la **revela por atajo** → entra de izquierda a derecha
  (patrón `Push-and-Pull`, ver vocabulario), empujando a `DynamicTitle`
  hacia el centro durante los 10 segundos que permanece visible. Al salir
  (hacia la izquierda), jala a `DynamicTitle` de regreso a su posición
  alineada a la izquierda.

**Nota:** el comportamiento de entrada/salida en `(full)` (horizontal,
empuja/jala) es distinto al de `(split)` (vertical, cae/sube) — parece
intencional dado que en `(full)` interactúa con `DynamicTitle` y en
`(split)` no, pero vale la pena confirmarlo explícitamente.

## Pendiente de definir

- [ ] ¿Qué dispara la revelación inicial en modo "auto-oculta" si no es el
      atajo manual? (el timing de ocultado — 10s — ya está confirmado para
      ambos modos)
- [ ] Confirmar que mantener Select no choca con otras funciones existentes
- [ ] Confirmar que la diferencia vertical (`split`) vs. horizontal (`full`)
      es intencional
