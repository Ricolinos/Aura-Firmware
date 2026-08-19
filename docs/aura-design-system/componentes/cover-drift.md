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
disponibles, y (c) se cumplió el debounce general del panel derecho (ver
"Activación" abajo — D-262 retiró el retardo propio de 3s). Fuera de esas
condiciones, `SelectionSummary` en `--layer-base` sigue siendo quien se
muestra (`componentes/selection-summary.md`). Siempre por debajo de
`LeftPanel` (`--layer-panel`) cuando este está montado.

## Filas que califican (D-254, ampliado D-266/D-316)

**Música:**
- **Menú raíz**: cuando la fila resaltada es **"Música"**, **"Canciones
  aleatorias"** o **"Ahora suena"** (D-266 — las tres resuelven a la
  categoría Música, y el dueño pidió explícitamente que las dos filas de
  reproducción también muestren la deriva de carátulas sin entrar a
  ellas).
- **Submenú de Música**: Cover Flow, Listas de reproducción, Artistas,
  Álbumes, Recopilaciones, Canciones, Géneros, Autores, Búsqueda —
  **excepto Audiolibros** (fila inerte, sin contenido real detrás).
- Todas las filas que califican, sin importar cuál exactamente, muestran la
  MISMA colección: un pool general de todas las carátulas de álbum de la
  biblioteca (no hay forma de resolver el álbum exacto de una fila
  específica sin API pública, ver DECISIONS.md D-242) — se hereda la
  colección del padre, no se re-filtra por hijo.

**Video (D-316):**
- **Menú raíz**: fila **"Videos"** — pool combinado Películas + Series.
- **Submenú de Video**: "Todos los videos" (mismo pool combinado),
  "Películas" (solo esa categoría), "Series"/"Programas de TV" (solo esa
  categoría) — **"Videoclips" NUNCA califica**: restricción textual del
  dueño del producto (2026-08-18, registrada en DECISIONS.md D-316 — no
  existía como decisión previa en ningún documento): CoverDrift de Video
  solo muestra carteles de películas y series.
- A diferencia de Música, aquí SÍ hay filtro real por fila (Películas y
  Series NUNCA comparten pool) — posible porque el índice de categoría
  por archivo (`CONTRATO-firmware-studio.md` §D.2) sí distingue cada
  video individual, algo que Música no necesita (ya tiene tagcache).
- Fuente de imagen: el póster opcional `<video>.jpg` hermano del archivo
  en `/Videos/` (`library-layout-v1.md` §1) — sin póster, ese video
  simplemente no aparece en el pool (no hay placeholder por-video, cae al
  resto del pool normalmente).

**Fotos (D-316):**
- **Menú raíz**: fila **"Fotos"** — pool de TODAS las fotos (sin filtrar).
- **Submenú de Fotos**: "Todas las fotos" (ídem), "Fotos" (solo categoría
  Foto), "Imágenes" (solo Imagen), "IA*" (solo IA) — cada una con su
  propio pool, nunca mezclados.
- Fuente de imagen: el archivo mismo en `/Photos/` (miniaturas de 320×320
  decodificadas para CoverDrift, independientes del caché de 48×48 de la
  lista).

Ambas fuentes dependen del índice OPCIONAL de categoría por archivo
(`aura_media_categories.h`) — sin él, todas las filas de categoría (no
"Todos"/"Todas") se ven vacías y caen a `SelectionSummary`, degradación
honesta, nunca un error.

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

## Activación (D-254, corregido D-260, unificado D-262, asimétrico D-266)

**Umbral de montaje: al menos 3 imágenes disponibles** (bajado de un valor
provisional de 10 a pedido directo del dueño del producto). Con menos,
sigue `SelectionSummary`.

**D-262 retira el temporizador de 3000ms propio de CoverDrift** — ya no
existe como mecanismo separado. En su lugar, CoverDrift se rige por el
mismo debounce GENERAL de todo el panel derecho de la Ruta A (ver
`sistema/02-navegacion-menus-contenido.md` o el comentario grande junto a
`render_panel_debounced()` en `aura_screens.c`): el panel se congela
mientras se recorre el `LeftPanel`, y solo se actualiza tras un tiempo de
estabilidad sobre la misma identidad. Encargo textual del dueño del
producto al confirmar la relación entre ambos plazos: "El de 2s reemplaza
al de 3s — CoverDrift ya no necesita su propia espera de 3s aparte, se
unifica en esta regla general más simple".

**La espera y la forma del cambio dependen del DESTINO (D-266):**

| Destino del cambio | Espera | Cómo cambia |
|---|---|---|
| Hacia `CoverDrift` (la fila empieza a calificar) | **2000ms** | **Fundido real de 600ms** |
| Hacia `SelectionSummary` o de un ícono normal a otro (incluye dejar de calificar, p. ej. llegar a "Audiolibros") | **1000ms** | **Corte instantáneo**, sin fundido |

Los 2s + fundido se reservan para la aparición de CoverDrift, que es lo
que vale la pena "anunciar" con una transición; el resto del panel cambia
más rápido y sin ceremonia, para que recorrer el menú no se sienta lento.

**La identidad se compara por CATEGORÍA para CoverDrift, no por fila exacta
(D-260, preservado por D-262)** — moverse entre filas que califican DENTRO
de la misma categoría (p. ej. de "Música" en el menú raíz a "Cover Flow" en
el submenú de Música, o entre cualquier par de filas del submenú) NO cuenta
como un cambio de identidad y por lo tanto NO dispara ningún fundido — es
la misma sesión continua, siempre en vivo. Solo cuenta como cambio real
dejar de calificar o cambiar de categoría — ahí aplica la cadencia de la
tabla anterior según el destino.

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

Cross-fade de 600ms **al montarse** (tras 2000ms de estabilidad); **al
desmontarse, corte instantáneo tras 1000ms**, sin fundido (D-266) — spec
completo en `componentes/selection-summary.md`, sección "Transición con
`CoverDrift`".

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

## Niveles de reducción (PLAN-niveles-fx.md, matriz del dueño 2026-08-18)

El canon de esta página sigue siendo **Animaciones = Todas / Gráficos =
Todos** (principio de máxima fidelidad, `00-INDICE.md`). Los niveles de
abajo son sustracciones sobre ese canon — un solo código con puntos de
sustracción, no implementaciones paralelas. Regla de precedencia (D-a):
Gráficos decide QUÉ existe, Animaciones decide CÓMO se mueve lo que
existe. Ver el índice transversal en `sistema/06-niveles-de-fx.md`.

| Gráficos ↓ / Animaciones → | Ninguna | Mínimas | Todas |
|---|---|---|---|
| **Ninguno** | Panel de **acento con degradado** (vertical, `aura_selection_summary_draw_accent_gradient_background()`, reutilizado tal cual), estático. Aparición por **corte** (gate de Animaciones apaga el fundido) | Igual, estático; la aparición **conserva el fundido** de 600ms | Igual; aparición con fundido. Sin imágenes, Animaciones no tiene nada más que decidir |
| **Mínimos** | **5 imágenes** residentes del pool general (sorteo al azar, una vez por arranque o al cambiar de nivel/pool), estáticas; rotación cada 7s por **corte** | 5 imágenes, estáticas; rotación con **cross-fade** de 600ms | 5 imágenes con **drift + cross-fade** completos |
| **Todos** | Todas las imágenes (comportamiento actual), estáticas, rotación por corte; delay de aparición **500ms** | Todas las imágenes, estáticas, con fade; delay **500ms** | **Canon** de esta página; delay de aparición baja de 2000 a **500ms** |

- **La rotación de imagen cada 7s (`CYCLE_MS`) es contenido, no
  animación** (D-a) — sigue ocurriendo en TODOS los niveles de
  Animaciones; lo que Animaciones gobierna es el *cómo* del cambio
  (drift + fade vs. quieto + corte), nunca el *cuándo*.
- Con Animaciones ≠ Todas, la imagen activa queda **quieta en el
  centro** (distancia de deriva forzada a 0) — nunca revela el fondo del
  panel, el mismo invariante geométrico de siempre sigue garantizado.
- El umbral de montaje sigue siendo **3 imágenes disponibles** (arriba en
  esta página) — independiente de Gráficos; con Gráficos = Mínimos y un
  pool de 3-4 álbumes, el tope de 5 simplemente no se alcanza (`cap =
  MIN(5, pool)`).
- El modo de 5 imágenes **no ahorra RAM** respecto al comportamiento
  actual (que ya solo mantiene 2 imágenes residentes sin importar el
  tamaño del pool) — gasta ~600KB MÁS (5×200KB vs. ~400KB de hoy). Lo que
  compra es que el disco deja de despertar cada 7s una vez decodificadas
  las 5: batería/silencio, no memoria. Implementación:
  `aura_fx_coverdrift_pool_cap()` (`aura_fx.h`) +
  `ensure_drift_effective_pool()` (`aura_screens.c`).

## Reutilización (D-316: ya conectado a las 3 fuentes)

El mismo componente (`aura_coverdrift.c`/`.h`) es agnóstico de la fuente de
imágenes — confirmado en la práctica: Video y Fotos se conectaron (D-316)
sin tocar `aura_coverdrift.c`/`.h` en absoluto, todo el cableado nuevo vive
en `aura_screens.c` (selección de pool + decodificación por fuente) y en
`aura_video.c`/`aura_photos.c` (filtrado por categoría). Un solo CoverDrift
visible a la vez — Música, Video y Fotos comparten los mismos buffers de
decodificación (`s_drift_album_pixels_a/b`), invalidados explícitamente al
cambiar de fuente (ver "Filas que califican" arriba).

## Preguntas ya resueltas por la implementación (D-254)

- **¿Ciclos independientes o sincronizados?** Una sola imagen visible a la
  vez, con un ciclo de 7s COMPARTIDO — `s_index` rota secuencialmente por
  el pool, no hay ciclos independientes desfasados por imagen.
- **¿Cómo se elige el ángulo?** Aleatorio entre los 8, sin repetir el
  inmediatamente anterior.
- **¿Qué pasa al llegar al centro?** El siguiente movimiento arranca de
  inmediato, en el mismo cuadro — no hay pausa entre un movimiento y el
  siguiente.
