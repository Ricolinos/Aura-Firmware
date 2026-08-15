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
- **No siempre recorre el trayecto completo de extremo a extremo** — la
  distancia de arranque varía entre 40% y 100% del margen disponible.
- **Duración: 7 segundos** por desplazamiento.
- **Curva: velocidad constante** (D-254 probó una desaceleración real hacia
  el cambio de imagen, remapeando el tiempo con `aura_motion_ease_out()`
  antes de pasarlo al motor de posición -- el dueño del producto la probó
  en el simulador y pidió quitarla, D-255. `aura_pattern_drift_pos()`
  (`aura_patterns.c`) ya es puramente lineal por su cuenta -- recibe el
  tiempo real directo, sin ningún remapeo). Verificado analíticamente
  (misma aritmética entera exacta del código) que la trayectoria resultante
  es una diagonal genuinamente intercalada entre los dos ejes, nunca un
  patrón de "escalera" (varios pasos en un eje seguidos de varios en el
  otro) -- ambos ejes se derivan siempre del mismo escalar de distancia
  compartido.
- **Transición entre imágenes: fundido REAL** (D-254, corrección tras un
  primer intento con corte directo) — mezcla píxel a píxel entre la imagen
  saliente (quieta, en el centro) y la entrante (moviéndose), durante los
  400ms de cross-fade, con una rampa de alpha lineal de 8 pasos a 20fps
  (misma cadencia de fundidos ya establecida en el sistema,
  `AURA_MOTION_FADE_FPS`).

## Tamaño de imagen y margen de deriva (D-254)

El panel derecho mide 160×240px exactos. Las imágenes de CoverDrift son
**290×290px** (cuadradas — las carátulas de álbum lo son por naturaleza) —
deliberadamente MÁS GRANDES que el panel en ambos ejes, para que el
movimiento de deriva nunca revele el fondo del panel detrás de la imagen.
El margen de sobrante es asimétrico: 65px horizontal ((290−160)/2), 25px
vertical ((290−240)/2) — la distancia máxima de deriva usa el MENOR de los
dos (25px), así que en cualquiera de las 8 direcciones la imagen sigue
cubriendo el panel completo en todo momento.

## Activación (D-254)

**Umbral de montaje: al menos 3 imágenes disponibles** (bajado de un valor
provisional de 10 a pedido directo del dueño del producto). Con menos,
sigue `SelectionSummary`.

Cuando la selección se posa sobre una fila que califica, CoverDrift NO
reemplaza el ícono de inmediato — espera **3000ms** (tiempo para que el
dispositivo decodifique la primera carátula) antes de montarse. Si la
selección cambia antes de cumplirse el plazo, el temporizador se reinicia;
si la fila deja de calificar o el pool cae por debajo del umbral, se vuelve
al ícono normal sin esperar.

## Memoria (D-254)

Decodificación bajo demanda: solo la imagen ACTIVA y la ANTERIOR (nunca el
pool completo — con hasta 300 álbumes posibles, decodificarlos todos de
antemano sería inviable). Presupuesto real, todo en buffers `static` fijos
(sin asignación dinámica repetida):

- Dos tiles finales decodificados (activa + anterior): 290×290×2 bytes cada
  uno ≈ 336KB en total.
- Buffers de trabajo de decodificación/transposición (`aura_albumart.c`,
  compartidos con Cover Flow, redimensionados para este tamaño mayor):
  ≈657KB.
- Pool de seeks + estructuras auxiliares: ≈23KB.

Total ≈1.2MB, un ~1.9% de los 64MB de RAM del target — trivial, no
justifica una estrategia más compleja para este alcance.

## Transición con `SelectionSummary`

Cross-fade al montarse/desmontarse — spec completo en
`componentes/selection-summary.md`, sección "Transición con `CoverDrift`".

## Sombra de `LeftPanel`

Debe renderizarse una sombra que simule que `LeftPanel` está por encima de
este componente — spec completo en `efectos/01-sombras.md` (regla
actualizada, compartida con `SelectionSummary`).

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
