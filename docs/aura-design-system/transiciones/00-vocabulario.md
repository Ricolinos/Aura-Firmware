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

**Sentido inverso:** resuelto 2026-08-13 — ver `Lift-and-Push` abajo.

**Timings (ratificados por el dueño 2026-08-16, D-274; tokens
`aura_ds.metrics.push_and_drop.*` en `tokens.json`):**

| Fase | Modo de animación completo | Modo reducido |
|---|---|---|
| Fase 1 — Push | 8 cuadros a 60Hz (≈133ms) | 4 cuadros a 45Hz (≈89ms) |
| Fase 3 — Drop | 5 cuadros (≈83ms) | 3 cuadros |

Se expresan en cuadros y no en milisegundos porque la coreografía avanza
por cuadro dibujado (`aura_transitions.c`), no por reloj — el equivalente
en ms es orientativo. Los mismos valores aplican a `Lift-and-Push` en
inversa (Lift = mismos cuadros que Drop, Push = mismos que Push).

---

## `Lift-and-Push`

**Tipo:** Coreografiado, 3 fases, secuenciales. Es la **inversa temporal
exacta** de `Push-and-Drop`, confirmada 2026-08-13 (antes marcada como
"no definida", lo que hacía que `(full) → (split)` cayera en el cambio
instantáneo y se viera como un salto).

1. **Fase 1 — Lift (levantada):** la `StatusBar (full)` sube y sale por
   el borde superior. Es el Drop al revés: la barra se va sola, antes de
   que el contenido se mueva.
2. **Fase 2 — Hueco intencional:** no hay barra en pantalla mientras el
   contenido se desplaza.
3. **Fase 3 — Push (empuje):** el contenido de pantalla completa sale
   hacia su borde y el nuevo layout `(split)` entra desde el opuesto,
   con su `StatusBar (split)` **entrando empujada junto a su panel** —
   exactamente como se va empujada en la ida.

**Por qué la simetría importa:** en `Push-and-Drop` la barra de destino
es la que "aterriza" sola porque no comparte silueta con la de origen; en
el regreso el que no comparte silueta es el origen, así que es el origen
el que se va solo. Interpolar barras de distinta silueta sigue estando
prohibido en ambos sentidos.

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

**Implementado (D-278, 2026-08-16)** — `aura_transition_shift_and_reveal()`
(`aura_transitions.c/.h`), utilidad reutilizable siguiendo la convención del
resto del archivo (sin callbacks de dibujo: el elemento persistente se
captura como bitmap de un framebuffer ya renderizado, no se le pide al
llamador que sepa dibujarlo). Primer consumidor real: `componentes/about.md`
(la pantalla "Acerca de", tile de `SelectionSummary` viajando a la
izquierda) — confirma que el patrón SÍ aplica a un `SelectionSummary`
estático, no solo a un ícono dinámico (ver pendiente cerrado abajo).
`componentes/date-editor.md` queda como segundo consumidor previsto, listo
para conectar sin reescribir la utilidad. Se diferencia de `Push-and-Drop`
en que el elemento existente **nunca sale de pantalla** — es el mismo
elemento que se reposiciona y se adapta a la nueva interfaz, no una salida
seguida de una entrada de un elemento distinto.

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

- [x] Timings de `Push-and-Drop` / `Lift-and-Push` — ratificados por el
      dueño (D-274, 2026-08-16): push 8 cuadros @60Hz / 4 @45Hz, drop/lift
      5 / 3 cuadros; tokenizados en `aura_ds.metrics.push_and_drop.*`. Ver
      tabla en la sección `Push-and-Drop`.
- [x] Timing/easing de `Shift-and-Reveal` — resuelto (D-278, 2026-08-16,
      ratificado por el dueño vía `PLAN-about-storage.md` Q6): mismos
      tokens de `Push-and-Drop` (`push_and_drop.*`) que ya gobiernan el
      empuje de `LeftPanel` que ocurre en paralelo — dos velocidades
      simultáneas se verían desacopladas. Sin token propio nuevo.
- [x] `(full) → (split)`: resuelto — es `Lift-and-Push` (sección arriba,
      2026-08-13; código `reveal_behind_panels_exit()`, D-267). Para
      `Shift-and-Reveal` la inversa exacta vive en la misma utilidad
      (`aura_transition_shift_and_reveal()`, `direction < 0`), simétrica.
- [ ] Duración de fade y tiempo de idle para `Fade-on-Idle`
- [x] ¿`Shift-and-Reveal` aplica solo a un ícono dinámico, o también a un
      `SelectionSummary` estático? — resuelto (D-278/D-279): SÍ aplica a
      un `SelectionSummary` estático — "Acerca de" es la prueba real, el
      elemento que viaja es el tile completo con su badge, no un ícono
      dinámico como el reloj de Fecha y Hora.

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

**Variante de región (D-283, `aura_transition_fade_slide_region()`,
`aura_transitions.c`):** primer caso de cambio **full→full** que el
vocabulario cubre — antes solo describía pantalla completa fija con un
texto cambiando (`DynamicTitle`). Se usa entre las 3 páginas de "Acerca de"
(`componentes/about.md`): en vez de animar toda la pantalla, solo se anima
un rect — lo que queda fuera (`StatusBar`, un elemento persistente, puntos
de paginación) no cambia entre estados y se toma directo del destino ya
prerrenderizado. Mismo idioma de fade+deslizamiento, mismos tokens de
timing que `Push-and-Drop`/`Shift-and-Reveal` (`push_and_drop.*`, D-274) —
sin timing propio documentado hasta esta variante, igual que
`Shift-and-Reveal` no lo tenía.

**Segundo consumidor (D-291):** `PhotoViewer` (`componentes/photo-viewer.md`)
entre fotos dentro del visor — región = pantalla completa (sin `StatusBar`
que excluir, esa pantalla no tiene). A diferencia de "Acerca de" no hay
elemento persistente ni puntos de paginación: toda la región es la foto.

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

**Duración: 300ms** (`ClockIndicator`, D-274 — decisión del dueño
2026-08-16; antes 220ms provisional de D-108).

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

**Duración: 300ms** (`ClockIndicator`, D-274 — decisión del dueño
2026-08-16; antes 220ms provisional de D-108). Misma que `Drop-and-Lift`:
son el mismo gesto de reloj en dos ejes.

---

## `Flip-and-Flow`

**Tipo:** Coreografiado, 3 fases secuenciales, conecta dos pantallas sin
corte.

Observado/definido para la salida de Music Flow hacia el reproductor
(`componentes/music-flow.md`). Al confirmar una selección dentro de un
elemento "volteado" (flip):

1. **Unflip:** el elemento gira de regreso a su cara frontal (la carátula
   vuelve a verse).
2. **Asentamiento:** el elemento se posiciona en su lugar dentro de su
   layout de origen (el carrusel).
3. **Flujo:** desde esa posición, transición fluida y continua hacia la
   pantalla destino (Now Playing), llevándose consigo sus atributos
   visuales (incluido el reflejo) durante el trayecto.

**Parentesco con `Shift-and-Reveal`:** ambos usan un elemento persistente
que nunca desaparece mientras el layout cambia a su alrededor. La
diferencia: `Shift-and-Reveal` reacomoda dentro de la misma pantalla;
`Flip-and-Flow` cruza de una pantalla a otra usando el elemento como
puente visual.

**Requisito de fidelidad:** el flip en sí debe verse como el del iPod
original — es una réplica intencional, no una aproximación.

**Duración del vuelo (fase 3): 500ms** — decisión del dueño (antes 350ms
provisional), ver `aura_transition_flip_and_flow()` / `FLOW_MS` en
`aura_transitions.c:678`. Vive como `#define` local, sin token en
`design-system/tokens.json` todavía. Corre a 60 o 30 fps según el ajuste
`animation_mode`.

**Niveles de reducción (PLAN-niveles-fx.md, 2026-08-18):** este patrón,
junto con su regreso `Flow-Return`, es exclusivo de **Animaciones =
Todas** — con Mínimas o Ninguna se sustituye por el `Push-and-Drop`
genérico full↔full (arriba en esta página), en ambos sentidos, sin
ningún patrón nuevo: Music Flow y el reproductor son ambas pantallas FULL,
así que el push ya cubre el caso. Detalle en
`componentes/music-flow.md` § Niveles de reducción.
