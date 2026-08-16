# ClockIndicator

Nombre provisional. El elemento de hora dentro de `StatusBar`.

## Visibilidad (configurable por el usuario — D-108)

Dos modos (un solo ajuste booleano, `clock_visible`):

- **Persistente** — siempre visible.
- **Oculto, revelable por atajo** — no se muestra por defecto; se revela
  **manteniendo presionado Select** (~300ms, el mismo umbral de repetición
  del driver de botones) y **se oculta solo a los 10 segundos** de estar
  visible.

Resuelto en D-108 (cerrando B-01/B-02): el disparador de la revelación
es el hold de Select — no hay un tercer modo "auto-oculta" con disparador
distinto. La infraestructura de "mantener presionado" es general
(`AURA_BUTTON_HOLD`), pero hoy `StatusBar` es la única dueña de ese gesto
para Select. **No choca con las demás funciones de Select por
construcción:** Aura despacha la pulsación corta al presionar (no al
soltar), así que la acción normal ya ocurrió antes de que un hold pueda
detectarse — no hay posibilidad de doble disparo. Limitación honesta
(D-108): el gesto de hold no es verificable con el arnés de capturas
automatizado; solo se confirma sosteniendo Select en el simulador
interactivo o en el dispositivo.

## Dimensiones y formato

- Alto: 12px (D-207; antes 10px)
- Ancho máximo: 46px (D-207; antes 40px — ajustado al glifo de 12px para
  que "HH:MM AM" quepa completo)
- Formato: `HH:MM`, nunca segundos. 24 horas o AM/PM — configurable por el
  usuario.

## Posición y comportamiento por estado

### `(split)`

Centrada en la barra de estado, tanto horizontal como verticalmente.

Entrada/salida: patrón `Drop-and-Lift`
(`transiciones/00-vocabulario.md`) — entra cayendo desde arriba de la
pantalla, sale subiendo hacia arriba, **300ms** por movimiento (D-274,
decisión del dueño 2026-08-16; antes 220ms provisional de D-108). Se
oculta automáticamente a los 10 segundos de estar visible (ver arriba).

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
  alineada a la izquierda. Misma duración que en `(split)`: **300ms**
  (D-274).

**Confirmado (D-274, ratificado por el dueño 2026-08-16):** el
comportamiento de entrada/salida en `(full)` (horizontal, empuja/jala) es
distinto al de `(split)` (vertical, cae/sube) **a propósito** — en `(full)`
interactúa con `DynamicTitle` y en `(split)` no.

## Pendiente de definir

- [x] ¿Qué dispara la revelación inicial en modo "auto-oculta"? — resuelto
      en D-108 (B-01): es el mismo hold de Select; no existe un tercer modo
- [x] Confirmar que mantener Select no choca con otras funciones — resuelto
      en D-108 (B-02): sin choque por construcción (despacho en press,
      StatusBar única dueña del gesto)
- [x] Diferencia vertical (`split`) vs. horizontal (`full`) — ratificada
      como intencional por el dueño (D-274, 2026-08-16)
- [x] Timing de `Drop-and-Lift`/`Push-and-Pull`: **300ms** (D-274, decisión
      del dueño 2026-08-16; sustituye los 220ms provisionales de D-108)
