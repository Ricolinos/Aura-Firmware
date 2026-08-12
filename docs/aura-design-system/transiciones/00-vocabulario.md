# Vocabulario de Transiciones

Cada patrón de transición tiene nombre propio. La idea es poder decir
"este componente usa `Push-and-Drop`" y que eso baste como spec — sin
reexplicar los pasos cada vez.

Un patrón puede ser de un solo paso (una interpolación) o **coreografiado**
(varios pasos con dependencia temporal, donde el paso 2 no arranca hasta que
el paso 1 termina). Hay que distinguir siempre cuál es cuál al documentar un
componente.

---

## `Morph Directo`

**Tipo:** Un solo paso.

El componente interpola directamente de su estado A a su estado B (posición,
tamaño, opacidad) sin pasos intermedios ni componentes adicionales entrando o
saliendo. Es el patrón por defecto para cambios de estado simples.

**Cuándo usarlo:** cuando el componente en el estado A y el componente en el
estado B son "la misma silueta visual" solo que en otra geometría — no hay
necesidad de ocultar nada mientras tanto.

---

## `Push-and-Drop`

**Tipo:** Coreografiado, 3 fases, secuenciales (fase *n+1* espera a que
termine la fase *n*).

Patrón identificado para la transición `(split) → (full)` de la Status Bar
(ver `componentes/status-bar.md` para el spec completo aplicado). Descripción
general del patrón:

1. **Fase 1 — Push (empuje):** el nuevo contenido de pantalla completa entra
   desde el lado derecho, empujando fuera de pantalla (hacia la izquierda) al
   panel que antes ocupaba ese espacio — junto con cualquier componente de
   `--layer-chrome` asociado a ese estado anterior.
2. **Fase 2 — Hueco intencional:** mientras la Fase 1 no haya terminado, **no
   existe** todavía el componente correspondiente al nuevo estado en
   `--layer-chrome`. No se hace cross-fade ni morph — el hueco es a propósito.
3. **Fase 3 — Drop (caída):** una vez que la Fase 1 terminó por completo,
   el nuevo componente de `--layer-chrome` entra desde arriba de la pantalla
   y cae hacia su posición final.

**Por qué existe este patrón y no un `Morph Directo`:** porque el componente
de `--layer-chrome` en el estado A (mitad de pantalla) y en el estado B
(pantalla completa) no son la misma silueta — interpolarlos directamente se
vería como un stretch feo, no como una transición intencional. Separar la
salida de la entrada, con una pausa entre medio, es lo que lo hace sentir
diseñado y no un bug de layout.

**Estado abierto:** el sentido inverso `(full) → (split)` **no está
definido todavía**. No asumas que es el mismo patrón en reversa — puede serlo,
o puede que amerite su propia coreografía. Definir esto es un pendiente.

---

---

## `Fade-on-Idle`

**Tipo:** Un solo paso, pero bidireccional según input, no según navegación.

El componente aparece (fade-in) cuando hay interacción activa (ej. el
usuario se mueve dentro de una lista) y desaparece (fade-out) cuando el
input queda inactivo por un tiempo. No está atado a un cambio de estado de
pantalla — está atado a actividad/inactividad del usuario.

**Usado por:** `ScrollIndicator` dentro de `LeftPanel` (ver
`componentes/left-panel.md`) — primer caso confirmado: idle de 1.5s antes
del fade-out, duración del fade de ~500ms o más. Estos valores son
específicos de `ScrollIndicator`; si otro componente usa este patrón, no
asumir que hereda los mismos números sin confirmarlo.

**Pendiente:** duración del fade y tiempo de idle todavía sin valores para
usos futuros de este patrón fuera de `ScrollIndicator`.

---

## `Marquee Loop`

**Tipo:** Loop continuo, sin fin, dos fases que se alternan mientras el
texto siga en pantalla. **No es una transición de cambio de estado** — es
un comportamiento de contenido: se activa por una condición (el texto no
cabe en su contenedor), no por una navegación.

**Condición de activación:** solo aplica a texto que no cabe completo en su
espacio disponible. Texto que sí cabe no hace nada de esto — se muestra
estático.

1. **Fase de lectura (estática):** el texto aparece, visiblemente
   incompleto (se ve solo lo que cabe), y se queda quieto **2 segundos**
   para dar tiempo a empezar a leerlo.
2. **Fase de movimiento:** durante **5 segundos**, el texto se desplaza de
   derecha a izquierda a velocidad moderada (fluida, no brusca) hasta salir
   completamente de pantalla por la izquierda. **Al mismo tiempo**, el mismo
   texto vuelve a entrar desde la derecha — es un loop continuo, sin
   transición de entrada separada ni pausa entre una vuelta y la siguiente.
3. El ciclo se repite indefinidamente: 2s estático → 5s en movimiento → 2s
   estático → ...

**Sin transición de entrada:** a diferencia de otros patrones de este
documento, este no tiene una fase de "aparición" distinta — el texto entra
ya en su ciclo normal.

**Difuminado en los bordes (confirmado):** el área visible tiene un
difuminado de 4px en cada extremo (izquierdo y derecho) para suavizar la
entrada/salida del texto en vez de un corte abrupto.

**Usado por:** `MarqueeText` (ver `componentes/marquee-text.md`), usado
dentro de `SelectionSummary` y `DynamicTitle`, y candidato a reutilizarse en
cualquier otro texto que pueda desbordar su contenedor (ej. títulos largos
en Now Playing).

---

## `Shift-and-Reveal`

**Tipo:** Coreografiado, 2 fases simultáneas.

**Confirmado.** Observado en el flujo Ajustes → Fecha y Hora → Fecha
(`componentes/date-editor.md`). Se diferencia de `Push-and-Drop` en que el
elemento existente **nunca sale de pantalla** — es el mismo elemento que se
reposiciona y se adapta a la nueva interfaz, no una salida seguida de una
entrada de un elemento distinto.

1. El elemento existente del lado derecho (ej. el ícono de calendario) se
   reacomoda hacia la izquierda dentro del nuevo layout — persiste como el
   mismo elemento, solo cambia de posición/tamaño.
2. **Simultáneamente**, `LeftPanel` sale hacia la izquierda siguiendo su
   comportamiento estándar (como en `Push-and-Drop`), y el nuevo componente
   (ej. `DateEditor`) aparece en el espacio que se libera a la derecha.

**Diferencia clave con `Push-and-Drop`:** `Push-and-Drop` es para
`--layer-chrome` (Status Bar) y siempre implica salida completa + hueco +
entrada. `Shift-and-Reveal` es para contenido de `--layer-base`/
`--layer-content` que tiene sentido mantener visible como referencia mientras
se profundiza en un flujo (ej. no tiene sentido que el ícono de calendario
desaparezca si sigues viendo/editando algo relacionado a fecha).

## Pendiente de definir

- [ ] Timing/easing tokens (duración de cada fase, curva de animación) — aún
      no se han fijado valores para `Push-and-Drop` ni `Shift-and-Reveal`.
- [ ] `(full) → (split)`: ¿reversa simétrica de `Push-and-Drop` o patrón nuevo?
- [ ] Duración de fade y tiempo de idle para `Fade-on-Idle`
- [ ] ¿`Shift-and-Reveal` aplica solo cuando el elemento existente es un
      "ícono dinámico", o también cuando es un `SelectionSummary` estático?

---

## `Fade-Slide`

**Tipo:** Transición de valor (no loop), un solo paso, horizontal, con
dirección variable según contexto.

Combina fade + desplazamiento horizontal al cambiar el valor de un texto de
un estado a otro. **Confirmado:** al entrar a un menú (profundizar en la
navegación), desliza de derecha a izquierda. Al salir (regresar), desliza
de izquierda a derecha.

**Usado por:** `DynamicTitle` (`componentes/dynamic-title.md`) cuando
cambia de un nombre de menú a otro.

---

## `Scroll-Slide`

**Tipo:** Transición de valor (no loop), un solo paso, vertical, con
dirección variable según contexto.

Igual que `Fade-Slide` pero en el eje vertical, atada a la dirección del
scroll del usuario en vez de a la navegación entre menús. **Confirmado:**
scroll hacia abajo → el texto nuevo entra desde abajo, el viejo sale hacia
arriba. Scroll hacia arriba → el texto nuevo entra desde arriba, el viejo
sale hacia abajo.

**Usado por:** `DynamicTitle` cuando cambia de un nombre de sección a otro.

---

## `Drop-and-Lift`

**Tipo:** Bidireccional, entrada y salida simétricas, vertical.

Entra cayendo desde arriba de la pantalla; sale subiendo de regreso hacia
arriba. Se distingue de la fase "Drop" de `Push-and-Drop` en que no depende
de una transición de pantalla completa — es el comportamiento propio de un
elemento que aparece/desaparece de forma independiente (ej. por un atajo de
usuario), sin que el resto del layout cambie de estado.

**Usado por:** `ClockIndicator` (`componentes/clock-indicator.md`) en
`(split)`.

---

## `Push-and-Pull`

**Tipo:** Bidireccional, un elemento desplaza a otro al aparecer y lo
libera al desaparecer.

Un elemento entra horizontalmente y empuja a un elemento vecino fuera de su
posición normal — sin sacarlo de pantalla, solo reacomodándolo (típicamente
centrándolo). Al salir, revierte el empuje y el vecino regresa a su
posición original.

**Diferencia con `Shift-and-Reveal`:** `Shift-and-Reveal` es para
transiciones de pantalla completa donde un elemento persiste mientras
cambia el layout general alrededor. `Push-and-Pull` es un comportamiento
interno entre dos elementos dentro del mismo componente, reversible en
ambas direcciones simétricamente, sin que haya un cambio de estado de
pantalla de por medio.

**Usado por:** `ClockIndicator` empujando a `DynamicTitle` en `(full)`.
