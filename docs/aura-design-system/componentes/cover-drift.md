# CoverDrift

Reemplazo condicional de `SelectionSummary` en el panel derecho de
pantallas de **menú de navegación** (Menú raíz y submenús como Música) —
NUNCA en listas de contenido (Canciones, Artistas, Álbumes como archivos,
etc.), que son pantalla completa y no tienen panel derecho. Ver
`sistema/02-navegacion-menus-contenido.md` para la distinción entre las dos
rutas de dibujo del sistema.

**Historia**: un primer intento (2026-08-15) conectó CoverDrift reemplazando
`SelectionSummary` en listas de CONTENIDO, lo que obligó a convertirlas de
pantalla completa a panel dividido (perdiendo su riel A-Z). El dueño del
producto lo rechazó de plano y pidió revertirlo por completo (DECISIONS.md
D-253) — la clasificación de qué pantallas son pantalla-completa vs.
panel-dividido es una decisión ya cerrada que CoverDrift nunca debe tocar.
El diseño de abajo (D-254) es la versión correcta, confirmada por el dueño
del producto tras varias iteraciones de prueba en el simulador.

## Capa

`--layer-content`. **Opcional y condicional por fila** — solo reemplaza a
`SelectionSummary` cuando (a) la fila resaltada del menú es una de las que
califica (ver "Filas que califican" abajo), (b) hay imágenes suficientes
disponibles, y (c) ya pasó el retardo de activación de 3s. Fuera de esas
condiciones, `SelectionSummary` en `--layer-base` sigue siendo quien se
muestra (`componentes/selection-summary.md`). Siempre por debajo de
`LeftPanel` (`--layer-panel`) cuando este está montado.

## Filas que califican (D-254, solo Música por ahora)

- **Menú raíz**: solo cuando la fila resaltada es "Música". Videos y Fotos
  quedan fuera de esta pasada a propósito — el dueño del producto pidió
  validar el comportamiento en un solo lugar antes de replicarlo.
- **Submenú de Música**: Cover Flow, Listas de reproducción, Artistas,
  Álbumes, Recopilaciones, Canciones, Géneros, Autores, Búsqueda —
  **excepto Audiolibros** (fila inerte, sin contenido real detrás).
- Todas las filas que califican, sin importar cuál exactamente, muestran la
  MISMA colección: un pool general de todas las carátulas de álbum de la
  biblioteca (no hay forma de resolver el álbum exacto de una fila
  específica sin API pública, ver DECISIONS.md D-242) — se hereda la
  colección del padre, no se re-filtra por hijo.

## Comportamiento

Animación fluida donde las imágenes (carátulas de álbum) se mueven en 8
direcciones posibles: **0°, 60°, 90°, 120°, 180°, 240°, 270° y 300°** —
elegida al azar en cada movimiento, sin repetir la dirección inmediatamente
anterior.

**Mecánica del movimiento:**

- Cada movimiento **termina en el centro** — no empieza ahí. La imagen
  arranca desde una posición descentrada (en alguna de las 8 direcciones) y
  se desplaza *hacia* el centro.
- **Recorre el margen disponible completo, de borde a borde** (D-256 —
  antes, D-098/D-254, la distancia de arranque variaba entre 40% y 100%
  del margen; el dueño del producto pidió un movimiento mucho más
  pronunciado, así que ahora siempre usa el margen completo).
- **Duración: 7 segundos** por desplazamiento.
- **Curva: velocidad constante** (D-254 probó una desaceleración real hacia
  el cambio de imagen, remapeando el tiempo con `aura_motion_ease_out()`
  antes de pasarlo al motor de posición -- el dueño del producto la probó
  en el simulador y pidió quitarla, D-255).
- **Precisión subpixel real** (D-257 — el dueño del producto reportó que el
  movimiento diagonal se veía "escalonado"): `aura_pattern_drift_pos()`
  (`aura_patterns.c`) encadenaba dos truncamientos enteros independientes
  (distancia→píxeles, píxeles→componente por eje) que, a las pocas
  fracciones de píxel por cuadro que cubre este movimiento, podían hacer
  que los dos ejes redondearan en cuadros distintos entre sí. Nueva
  `aura_pattern_drift_pos_hp()` devuelve `dx256`/`dy256` (píxeles×256, sin
  truncar) en punto fijo Q?.8 -- no en `float`/`double` (este target,
  ARM926EJ-S, no tiene FPU, sería emulación por software y rompería la
  convención de trigonometría en punto fijo ya establecida en todo el
  sistema). `aura_coverdrift.c` trunca a entero UNA sola vez, al final,
  con redondeo simétrico, desde la misma fuente de precisión para ambos
  ejes -- ya no pueden desincronizarse. `aura_pattern_drift_pos()` (la
  función original, 5 pruebas existentes) sigue siendo un envoltorio de
  una línea sobre la nueva, sin cambio de comportamiento en los puntos ya
  probados.
- **Transición entre imágenes: fundido REAL** (D-254, corrección tras un
  primer intento con corte directo) — mezcla píxel a píxel entre la imagen
  saliente (quieta, en el centro) y la entrante (moviéndose), durante los
  **600ms** de cross-fade (D-256, subido de 400ms), con una rampa de alpha
  lineal a 20fps (misma cadencia de fundidos ya establecida en el sistema,
  `AURA_MOTION_FADE_FPS`).
- **Bug real corregido (D-256): el "flash" de color entre imágenes.** El
  avance de ciclo (`advance_cycle()`) se disparaba dentro de la función de
  dibujo, en el mismo cuadro en que el llamador ya había decodificado la
  imagen activa -- el índice nuevo se conocía demasiado tarde para
  decodificarlo a tiempo, y ese cuadro caía al color de acento como
  respaldo. Arreglado separando "decidir si toca avanzar"
  (`aura_coverdrift_advance_if_due()`, nueva, el llamador la invoca ANTES
  de decodificar) de "dibujar".

## Tamaño de imagen y margen de deriva (D-254, ampliado D-256)

El panel derecho mide 160×240px exactos. Las imágenes de CoverDrift son
**320×320px** (cuadradas — las carátulas de álbum lo son por naturaleza;
subido de 290px en D-256, movimiento más pronunciado a pedido del dueño) —
deliberadamente MÁS GRANDES que el panel en ambos ejes, para que el
movimiento de deriva nunca revele el fondo del panel detrás de la imagen.
El margen de sobrante es asimétrico: 80px horizontal ((320−160)/2), 40px
vertical ((320−240)/2) — la distancia máxima de deriva usa el MENOR de los
dos (40px), así que en cualquiera de las 8 direcciones la imagen sigue
cubriendo el panel completo en todo momento.

## Activación (D-254, corregido D-260, unificado D-262)

**Umbral de montaje: al menos 3 imágenes disponibles** (bajado de un valor
provisional de 10 a pedido directo del dueño del producto). Con menos,
sigue `SelectionSummary`.

**D-262 retira el temporizador de 3000ms propio de CoverDrift** — ya no
existe como mecanismo separado. En su lugar, CoverDrift se rige por el
mismo debounce/fundido GENERAL de todo el panel derecho de la Ruta A (ver
`sistema/02-navegacion-menus-contenido.md` o el comentario grande junto a
`render_panel_debounced()` en `aura_screens.c`): el panel se congela
mientras se recorre el `LeftPanel`, y solo se actualiza (con un fundido
real, no un corte) tras **2000ms** de estabilidad sobre la misma
identidad — sea que esa identidad pase a ser CoverDrift, vuelva a
`SelectionSummary`, o cambie de un ícono normal a otro. Encargo textual
del dueño del producto al confirmar la relación entre ambos plazos: "El de
2s reemplaza al de 3s — CoverDrift ya no necesita su propia espera de 3s
aparte, se unifica en esta regla general más simple".

**La identidad se compara por CATEGORÍA para CoverDrift, no por fila exacta
(D-260, preservado por D-262)** — moverse entre filas que califican DENTRO
de la misma categoría (p. ej. de "Música" en el menú raíz a "Cover Flow" en
el submenú de Música, o entre cualquier par de filas del submenú) NO cuenta
como un cambio de identidad y por lo tanto NO dispara ningún fundido — es
la misma sesión continua, siempre en vivo. Solo cuenta como cambio real
dejar de calificar (p. ej. llegar a "Audiolibros") o cambiar de categoría —
ahí sí aplican los 2000ms y el fundido de 600ms antes de reemplazar el
contenido.

## Memoria (D-254, actualizado D-256)

Decodificación bajo demanda: solo la imagen ACTIVA y la ANTERIOR (nunca el
pool completo — con hasta 300 álbumes posibles, decodificarlos todos de
antemano sería inviable). Presupuesto real, todo en buffers `static` fijos
(sin asignación dinámica repetida):

- Dos tiles finales decodificados (activa + anterior): 320×320×2 bytes cada
  uno ≈ 400KB en total.
- Buffers de trabajo de decodificación/transposición (`aura_albumart.c`,
  compartidos con Cover Flow, redimensionados para este tamaño mayor):
  ≈800KB.
- Pool de seeks + estructuras auxiliares: ≈23KB.

Total ≈1.2MB, un ~1.9% de los 64MB de RAM del target — trivial, no
justifica una estrategia más compleja para este alcance.

## Transición con `SelectionSummary`

Cross-fade al montarse/desmontarse — spec completo en
`componentes/selection-summary.md`, sección "Transición con `CoverDrift`".

## Sombra de `LeftPanel` (D-258: compositing real, no aproximación)

Se renderiza una sombra que simula que `LeftPanel` está por encima de este
componente — spec completo en `efectos/01-sombras.md` (regla actualizada,
compartida con `SelectionSummary`).

**Bug real corregido en D-258**: la sombra ya se dibujaba desde D-254, pero
ANTES de la carátula -- con el tile chico centrado (90px, diseño original)
se veía bien porque quedaba sobre el fondo plano del panel; al pasar la
carátula a llenar el panel completo, la sombra quedaba tapada por completo.
Además, `aura_shell_draw_left_panel_shadow()` (la función original) mezcla
contra un color de fondo FIJO, no compositing real -- pintarla sobre la
carátula habría dejado una franja de color plano en vez de oscurecer la
foto de verdad. Arreglado con una variante nueva,
`aura_shell_draw_left_panel_shadow_over_content()`
(`apple2026_shell.c`/`.h`), que lee y oscurece el píxel YA dibujado
(compositing real, mismo patrón que ya usa el compositor de máscaras de
íconos) -- llamada DESPUÉS de dibujar la carátula, no antes.

## Reutilización

El mismo componente (`aura_coverdrift.c`/`.h`) es agnóstico de la fuente de
imágenes — Fotos y Video quedaron fuera de esta pasada a propósito (el
dueño del producto los pidió después, uno a la vez, para validar el
comportamiento en Música primero). Cuando se conecten, debería ser
cableado nuevo en `aura_screens.c`/`aura_photos.c`/`aura_video.c` sin tocar
el componente en sí.

## Preguntas ya resueltas por la implementación (D-254)

- **¿Ciclos independientes o sincronizados?** Una sola imagen visible a la
  vez, con un ciclo de 7s COMPARTIDO — `s_index` rota secuencialmente por
  el pool, no hay ciclos independientes desfasados por imagen.
- **¿Cómo se elige el ángulo?** Aleatorio entre los 8, sin repetir el
  inmediatamente anterior.
- **¿Qué pasa al llegar al centro?** El siguiente movimiento arranca de
  inmediato, en el mismo cuadro — no hay pausa entre un movimiento y el
  siguiente.
