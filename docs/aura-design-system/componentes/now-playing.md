# NowPlaying (Reproductor)

La pantalla de reproducción — una **habitación** en términos de
`sistema/02-navegacion-menus-contenido.md`. Pantalla completa, con
`StatusBar` en modo `(full)` (título "Ahora suena" / "Now Playing").

## Geometría de la carátula (rescatada del original, confirmada)

Decisión: se rescata la **geometría, tamaño y posición de la carátula del
Now Playing original del iPod Classic** — con todo y reflejo — pero
manteniendo la interfaz nueva de Aura alrededor y aplicando las esquinas
redondeadas ya definidas (5-6px, mismas de CoverFlow).

Medidas estimadas de la captura del firmware original (escala a 320×240
reales — ⚠️ valores aproximados leídos de captura, a validar contra el
dispositivo o el prototipo):

| Propiedad | Valor aproximado |
|---|---|
| Posición (esquina sup. izq. del área de carátula) | x ≈ 10px, y ≈ 43px |
| Tamaño de la carátula | ≈ 135×135px |
| Inclinación | **7° (fijado formalmente)** — rotación sutil en Y: el borde derecho ligeramente comprimido/retrocedido, la carátula "mira" apenas hacia la derecha |
| Reflejo | Debajo de la carátula, hereda la misma perspectiva, con desvanecimiento hacia abajo. **Proporción: "más sutil que el original", idéntica a la definida en CoverFlow** — mismo reflejo en ambas pantallas, sin cambio durante la transición |

## Barra unificada de progreso/volumen (confirmada 2026-08-12)

Una sola pieza con cuatro caras:

| Estado | Aspecto |
|---|---|
| Reposo | Carril **300×7px centrado**, color del Selector de menús (`SELECTION_FILL`), puntas completamente redondeadas; relleno **298×5px BLANCO**. **Sin números de tiempo.** |
| Avance (rueda en modo avance) | El relleno pasa a **ACENTO**, con el **indicador de avance de 15×11px blanco con sombra paralela sutil** (su centro en el borde derecho del relleno) y los tiempos (`00:00` / `-00:00`) SOLO mientras dura el ajuste, **debajo de la barra, centrados en el eje horizontal de la fila de transporte** y alineados a los extremos de la barra. (Comportamiento heredado íntegro de la búsqueda por botones sostenidos, retirada el 2026-08-12.) |
| Volumen (rueda en modo volumen) | La misma barra muestra el **nivel de volumen** en acento (con el mismo indicador de 15×11); `speaker.minus`/`speaker.plus` a **16px, debajo de la barra y centrados en el eje de la fila de transporte**, alineados a los extremos de la barra; el play/pausa del transporte se convierte en la **bocina dinámica de 5 estados** (0-2% mute, 2-15% bocina sola, 15-50/50-80/80-100% una/dos/tres ondas) y todo se desvanece con un fade sutil al soltar. |

**Posición de la barra (confirmada 2026-08-12):** borde superior a
**44px del borde inferior** de la pantalla — los elementos transitorios
(tiempos de búsqueda, bocinas −/+ de volumen) viven **debajo** de ella,
nunca encima.

### Fila de transporte (confirmada 2026-08-12)

Centrada verticalmente **entre la base de la barra y el borde inferior**:

- **Play/pausa al centro, 24px** (los glifos de backward/forward se
  retiraron — la interacción vive en los botones físicos). El icono
  muestra el **estado real**: play mientras suena, pause en pausa
  (confirmado 2026-08-12).
- **Repetir (izq.) y aleatorio (der.) a 16px**, a los **costados del
  icono central con 20px de separación** — ya no en los extremos de la
  barra. Activo = acento, inactivo = tinta normal. Repetir tiene
  **variante de glifo**: `repeat` (todo) y `repeat.1` (una canción).

### Mantener backward/forward = modos de reproducción (confirmado 2026-08-12)

En **cualquier modo de la rueda**, incluido el principal:

- **Mantener FORWARD**: alterna **aleatorio** (activado/desactivado).
- **Mantener BACKWARD**: cicla **repetir** entre sus 3 estados —
  Repetir todo → Repetir una canción (`repeat.1`) → desactivado.
- **Una sola conmutación por pulsación sostenida** (soltar y volver a
  mantener para la siguiente); el tap corto sigue siendo pista
  anterior/siguiente, decidido al soltar.
- El efecto es **inmediato** sobre la reproducción en curso: aleatorio
  re-baraja (o re-ordena) el playlist vivo conservando la pista actual;
  repetir recarga la cola de pistas próximas.
- Al ajustar volumen, el centro lo ocupa la **bocina dinámica** (lienzo
  de 36px): sus 5 estados se renderizan a **pointSize fijo** (el de un
  icono de 24px) sin escalar-para-caber, de modo que **el cuerpo de la
  bocina mide exactamente igual en los 5 estados** y solo las ondas
  ocupan más lienzo. Renderizada desde el centro.

**Punto clave de continuidad:** esa inclinación sutil ES el ángulo de
aterrizaje de `Flip-and-Flow` — el original ya estaba diseñado como si la
carátula viniera de Cover Flow. La spec de ambos componentes queda acoplada
por diseño: el slide central de CoverFlow al hacer flip/unflip termina
exactamente en esta posición/ángulo/tamaño.

**Esquinas redondeadas:** la carátula y su reflejo llevan el radio de 5-6px
definido en `componentes/cover-flow.md` (heredadas del bitmap `.pfraw` ya
enmascarado — sin costo extra en runtime).

## Regla de continuidad con CoverFlow (requisito duro)

La carátula del álbum y su reflejo en NowPlaying **deben renderizarse en la
misma posición y ángulo en los que "cae" la carátula al final del patrón
`Flip-and-Flow`** (`transiciones/00-vocabulario.md`,
`componentes/cover-flow.md`). Esto es lo que produce la ilusión de
transición continua: la carátula nunca se corta ni se re-renderiza — es el
mismo elemento que viaja de una pantalla a la otra.

**Implicación de diseño:** la posición/tamaño/ángulo de la carátula en
NowPlaying no es una decisión libre de esta pantalla — está acoplada a la
coreografía de `Flip-and-Flow`. Si se cambia una, se cambia la otra.

### Entrada desde CoverFlow (confirmada)

Cuando se llega a NowPlaying vía `Flip-and-Flow`, la carátula es el único
elemento que ya está "en su lugar" (llegó con la transición). Los demás
elementos entran **escalonados (stagger)**, cada grupo con su propia
coreografía:

| Grupo | Entrada |
|---|---|
| Títulos y datos de la canción (título, artista, álbum, contador, rating) | Fade in |
| Íconos de modos | Desde la derecha |
| `StatusBar` | Desde arriba (su Drop estándar) |
| Barra de progreso | Desde abajo |

🔴 Pendiente menor: orden exacto del stagger y duración de cada entrada.

La carátula llega con el vuelo de media vuelta descrito en
`componentes/cover-flow.md` ("Vuelo CoverFlow → reproductor", ~500ms):
aterriza en el tilt/posición exactos de esta pantalla con su reflejo ya
apareciendo en proporción — el morph de grupos de arriba arranca justo
al aterrizar.

### Salida hacia CoverFlow (confirmada 2026-08)

Al regresar con MENU, NO se repite el vuelo en inversa: la carátula ya
está de frente, así que hace un **morph fluido de posición y geometría**
directo al centro del carrusel (sin giro, sin pasar por el reverso), con
el reflejo visible todo el trayecto; las tapas laterales entran desde los
bordes y el título/artista suben desde el borde inferior. Detalle
completo en `componentes/cover-flow.md` ("Regreso reproductor →
CoverFlow").

### Entrada desde otros orígenes (no CoverFlow) — confirmada

Cuando se llega a NowPlaying sin pasar por CoverFlow (ej. seleccionando
una canción desde una lista): **toda la pantalla, excepto la `StatusBar`,
entra desde la derecha empujando a la pantalla anterior**; la `StatusBar`
entra desde arriba **después** de que la pantalla del reproductor terminó
de entrar.

**Esto es exactamente el patrón `Push-and-Drop`**
(`transiciones/00-vocabulario.md`) — el mismo definido al inicio del
sistema para las transiciones `(split) → (full)`. No es un patrón nuevo:
NowPlaying es un caso de uso más de `Push-and-Drop`.

## Layout actual (capturado de la implementación existente, 2026-08)

Orden general a mantener (con correcciones pendientes de detallar):

| Zona | Contenido |
|---|---|
| Superior izquierda | Carátula del álbum, con reflejo debajo |
| Superior derecha | Título de la canción (bold), artista debajo, rating de 5 estrellas debajo |
| Media derecha | Fila de íconos de acción: volumen, ¿repeat/crossfade?, lista, ¿letras/comentarios?, favorito |
| Inferior media | Barra de progreso con tiempo transcurrido (izq) y restante/total (der) |
| Inferior | Controles de transporte: repeat, retroceder, play/pausa, adelantar, shuffle |

🔴 Pendiente: el propósito exacto de cada ícono de la fila de acciones (los
signos de interrogación arriba) — a confirmar del dueño del diseño.

## Modos del reproductor

El reproductor tiene **5 modos** que se alternan **en loop con un clic del
botón Select** estando en la pantalla del reproductor. Herencia del iPod
Classic original (que ya alternaba funciones del click wheel con Select),
con un modo extra nuevo y mejoras de interfaz.

**Constante en todos los modos:** los botones de reproducción (Play/Pausa,
Backward, Forward) siempre hacen lo mismo — nunca cambian de función según
el modo. Lo que cambia entre modos es qué controla el **scroll** de la
click wheel (y en algunos modos, qué hace Select).

### Fila de iconos de modos (confirmada 2026-08-12)

| Modo | Inactivo (lineal) | Activo (fill, acento) |
|---|---|---|
| 1 Volumen | `speaker.wave.1` | `speaker.wave.2.fill` |
| 2 Avance | `arrowtriangle.left.and.line.vertical.and.arrowtriangle.right` | su `.fill` |
| 3 Playlist | `text.badge.plus` | el mismo glifo en acento (SF no tiene variante fill) |
| 4 Letras | `quote.bubble` | `quote.bubble.fill` |
| 5 Favorito | `star` | `star.fill` |

**Estados** (excepción documentada a la regla "nunca .fill" del doc SS4,
misma familia que play-fill/pause-fill):

- **Inactivo**: versión **lineal** del símbolo, tinta terciaria.
- **Activo**: versión **`.fill`** del símbolo, en **ACENTO**.
- **Deshabilitado** (solo Playlist sin listas y Letras sin `.lrc`):
  mismo glifo lineal y color del inactivo, al **50% de opacidad**; el
  loop de modos lo salta.

El cambio de estado es **INMEDIATO** — se retiró el salto de resorte del
icono que se activa (la entrada al Modo 4 tendrá su propio tratamiento,
pendiente de detallar).

**Posición**: iconos de 20px alineados entre sí por su centro, separados
**4px**; el borde inferior de la fila queda **10px por encima de la barra
de progreso** y el último icono (la estrella) respeta el **padding
interno derecho** de la pantalla.

### Modo 1 — Normal (reproducción)

- Scroll = subir/bajar volumen.
- Es el modo por default y al que se regresa desde otros modos en ciertos
  flujos (ver Modo 3).

### Modo 2 — Búsqueda precisa (scrub)

- Scroll = desplazarse con precisión sobre la línea de tiempo de la
  canción, **visualizando** el punto donde uno quiere posicionarse.
- Coexiste con el comportamiento estándar de mantener presionado
  Backward/Forward para adelantar rápido — este modo es la versión visual
  y precisa de eso.

### Modo 3 — Agregar a playlist (nuevo, no existía en el original)

- Abre un modal de selección de playlist.
- **Si no hay playlists existentes, el modo no se activa** (se salta en el
  loop de modos) y su icono se muestra **deshabilitado al 50% de
  opacidad**, igual que el Modo 4 sin letras.
- **Ninguna playlist viene seleccionada de inicio** — decisión deliberada
  para evitar agregar canciones sin querer a la primera de la lista.
- Scroll = desplazarse entre playlists.
- Select **con** una playlist seleccionada = agrega la canción y **regresa
  al Modo 1**.
- Select **sin** selección = avanza al siguiente modo (el loop normal).
- Botón Menu = cancela la adición y cierra el modal.

### Modo 4 — Letras (mejora mayor sobre el original)

Este modo **transforma el layout completo** del reproductor con una
transición **morph fluida** de ~330ms (fundido lineal de contenido),
confirmada 2026-08-12. Es **pantalla completa**: sin StatusBar.

- **Panel izquierdo de 130×240** — ⚠️ NO es el componente `LeftPanel`.
  Es un **estado del propio reproductor**, con el **fondo normal**
  (corrección 2026-08-12: el color promedio NO lo toca). La carátula va
  **centrada verticalmente en el espacio sobre la fila de modos** (no
  invade los controles de abajo), con una **sombra difusa alrededor de
  toda la imagen** (corrección 2026-08-12: no una rebanada inferior —
  un resplandor de ~8px con decaimiento cuadrático, opacidad pico ~19%
  en el borde, siguiendo la **silueta redondeada** del álbum) que **se desvanece con el morph** (aparece al entrar,
  se va al salir).
- **Panel derecho** (`LyricsPanel`) — una **HOJA que se desliza desde
  la derecha** (no un fade; corrección 2026-08-12) de **vidrio
  traslúcido** (~85% de tinte: lo de abajo se adivina a través)
  teñido con el **PROMEDIO de la carátula en curso en degradado
  DIAGONAL** (aclarado en la esquina superior izquierda, oscurecido en
  la inferior derecha), derivado en runtime de la imagen real. El **panel
  izquierdo es la capa elevada**: proyecta una **sombra paralela sobre
  la hoja** desde su borde (franja de 8px decayendo a la derecha,
  corrección 2026-08-12). La hoja es **cuadrada** — las esquinas
  redondeadas de pantalla no se estampan sobre ella. La tinta de los textos se decide por **luminancia**
  (blanco constante sobre vidrio oscuro; una versión muy oscura del
  mismo color sobre claro), con la jerarquía por atenuación. **Título,
  artista y álbum viven en la parte superior de la pantalla**, montados
  en la hoja (llegan ya renderizados, deslizándose con ella), y la
  letra corre debajo con avance automático sincronizado. ⚠️ Excepción
  deliberada del dueño del diseño a "el vidrio vive solo en la capa de
  controles" (pedido explícito 2026-08-12).

**El morph hacia el Modo 4 (mismo motor de proyección por columnas que
el morph de regreso al carrusel):**

- La **carátula es el mismo elemento**: pasa de 135px inclinada 7° a un
  **cuadrado perfecto de 106px** centrado en el panel (horizontal y
  verticalmente en el área sobre la fila de modos); su **reflejo se
  desvanece** en el trayecto (no existe en el Modo 4) y su **sombra
  difusa aparece** en proporción.
- La **barra de progreso interpola a 122px** dentro del panel.
- El **transporte completo** (repetir + play/pausa + aleatorio) y la
  **fila de modos** viajan al centro del panel, conservando sus anclas
  verticales (misma línea que en el layout normal).
- **Título/artista/álbum se desvanecen** de su posición vieja y
  **renacen ya renderizados** en la parte superior del panel derecho
  (desvanecimiento fluido en ambos sentidos).
- **Las estrellas desaparecen** en este modo.

**Salidas:**

- **Al modo siguiente** (Select): la misma transición, invertida.
- **Del reproductor** (Menu): si se **entró por coverflow, SÍ se
  regresa al coverflow** (corrección 2026-08-12): el despliegue inverso
  del panel se **encadena con el morph de regreso al carrusel** — dos
  morphs confirmados, un solo gesto fluido. A cualquier otro destino,
  la pantalla se comporta como pantalla completa y **se desplaza hacia
  la derecha dando paso al menú anterior**.

**Si la canción no tiene letras: el modo se desactiva** — su ícono sigue
apareciendo en la fila de modos pero al **50% de opacidad** y no se puede
seleccionar (el loop de modos lo salta, igual que el Modo 3 sin playlists).

**Tipografía de `LyricsPanel`:** letras a 12px Regular; la línea activa a
**14px Bold**; cabecera con los mismos estilos del bloque de texto del
reproductor (título Bold 12, artista/álbum Regular 12).

**Líneas largas (confirmado 2026-08-12):** aquí NO hay Marquee Loop —
las líneas **no activas se recortan** al ancho del panel; la línea
**activa se divide en los renglones necesarios** (hasta 3) para leerse
completa, cortando por palabras.

**Silencios (confirmado 2026-08-12):** cuando el hueco entre líneas es
largo (provisional: ≥8s, con ~3s de "lectura" de la línea saliente), el
centro muestra **3 puntos indicadores** que se **iluminan uno a uno**
conforme avanza el silencio — los tres encendidos = la letra está por
volver. La última línea cantada queda arriba y las próximas debajo.

**La StatusBar se desliza hacia arriba** durante el morph de entrada (y
regresa deslizándose al salir) — no desaparece de golpe. La sombra del
panel izquierdo aparece con su **propio fade** después del acople de la
hoja (~165ms).

🔴 Pendiente menor: comportamiento del scroll en este modo (¿desplaza las
letras manualmente, o el scroll queda sin función aquí?).

### Modo 5 — Puntuar (estrellas)

- Scroll (presumiblemente) = elegir cantidad de estrellas.
- Al puntuar, la canción **se agrega a la lista de favoritos
  correspondiente a esa cantidad de estrellas** (una lista por nivel de
  estrellas).

**Interacción confirmada:** scroll = elegir cantidad de estrellas; Select
= confirmar; al confirmar **regresa al Modo 1**, igual que el Modo 3.

### Reglas confirmadas del sistema de modos

- **Indicadores de modo:** la fila de íconos (bocina, flechas, lista,
  globo, estrella) son los indicadores de los 5 modos — un ícono por modo,
  en orden (M1→M5). 🔴 Pendiente menor: cómo se resalta el activo
  (presumiblemente `--color-accent`, a confirmar).
- **Orden del loop:** 1→2→3→4→5→1.
- **Cambio entre modos: instantáneo** — con la única excepción del Modo 4
  (Letras), que entra y sale con la transición morph fluida.
- **Reflejo de la carátula: existe en los modos 1, 2, 3 y 5 — NO en el
  Modo 4** (Letras). Al hacer el morph hacia el Modo 4, el reflejo
  desaparece; al salir, regresa. **Confirmado: se desvanece durante
  la transición morph**, no desaparece de golpe al completarse.

## Tipografía (confirmada)

Familia: **SF Pro siempre** (tokens en `fundamentos/02-tipografia.md`):

| Elemento | Peso | Tamaño |
|---|---|---|
| Título de la canción | Bold | 12px |
| Álbum | Regular | 12px |
| Artista | Regular | 12px |
| Contadores (tiempos, "1 of 11") | Bold | 10px |
| Letras (Modo 4) | Regular | 12px |
| Letra activa (Modo 4) | Bold | 14px |

## Barra de progreso (confirmada)

Dos capas:

- **Track persistente (fondo):** negro al 60% de opacidad, 6px de grosor.
- **Barra de avance (encima):** 4px de grosor, puntas redondeadas.
  **Blanca** en reposo; cambia a **`--color-accent`** mientras se está
  manipulando (Modo 2 / scrub).

**Formato de tiempos:** como el original — transcurrido a la izquierda,
restante con signo negativo a la derecha (ej. `1:56` / `-1:52`). Bold 10px.

En el Modo 4 (Letras), la barra se comprime a 122px de ancho (ver Modo 4).

## Pendiente de definir

- [ ] Validar las medidas estimadas de la carátula (x≈10, y≈43, ≈135×135px)
      contra dispositivo/prototipo — el ángulo ya quedó fijado en 7°
- [ ] Orden exacto del stagger de entrada y duración de cada grupo
- [ ] Diseño fino de los controles de transporte (tamaños de íconos)
- [ ] Cómo se resalta el ícono del modo activo (¿`--color-accent`?)
- [ ] Scroll en Modo 4: ¿desplaza letras manualmente o queda sin función?
