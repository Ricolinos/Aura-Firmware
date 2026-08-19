# Movie Flow

Vista de navegación de Video por carteles — hermana de `componentes/music-flow.md`,
misma **habitación** (contenido a pantalla completa, nunca `LeftPanel`) y
mismo motor de proyección por columnas (`aura_flow.c`, compartido sin
tocarlo). Vive en el submenú Videos, donde **"Movie Flow" es su propia
puerta** (primera fila, mismo criterio que "Music Flow" en Música) — nunca
un disfraz de "Todos los videos".

Encargo del dueño del producto (2026-08-18, D-318): copia **casi exacta**
de Music Flow, con cuatro diferencias deliberadas. Este documento solo
cubre esas diferencias — todo lo demás (proyección, cache de slots,
zoom-on-scroll, energy gate) remite a `music-flow.md`.

## Las cuatro diferencias

### 1 — Formato rectangular 3:4, no cuadrado

El cartel mide **120×160px** (exacto 3:4) en vez del cuadrado de Music
Flow. `aura_flow.c` ya es agnóstico de ancho/alto — solo pide
`slide_width_px`, el alto lo decide el llamador vía
`aura_flow_vertical_scale()` por columna — así que generalizar de
cuadrado a 3:4 no tocó el motor compartido, solo `aura_movieflow.c`.

El reverso (lista de episodios, diferencia 3 abajo) **sí** se queda
cuadrado (200×200, mismo tamaño que el de Music Flow) — ese panel nunca
dibuja el cartel, solo cabecera + lista de texto, así que su forma es
independiente del 3:4 del carrusel. Al girar, el cartel crece hacia un
ancho de 150px (no 200 como en Music Flow) — la proyección conserva la
proporción 3:4 automáticamente (es una cámara real, no un escalado en
dos ejes independientes), así que 150 de ancho da 150×(160/120)=200 de
alto: llena el panel en su eje vertical sin lógica nueva.

### 2 — Una película se reproduce al instante; una temporada gira

Un slide de Movie Flow es una **película** o una **temporada** — nunca
un episodio individual.

- **Película**: SELECT la reproduce de inmediato (ver diferencia 4) —
  sin el giro a lista, porque no hay nada que listar para un solo
  archivo.
- **Temporada**: SELECT gira el cartel (mismo mecanismo Flip-and-Flow de
  Music Flow) y revela sus episodios como **lista de texto** — decisión
  explícita del dueño del producto: reusar el panel de reverso que ya
  existe, no un componente nuevo de miniaturas. El panel es
  literalmente el mismo código que el reverso de canciones de Music
  Flow, solo cambia la fuente de datos (episodios en vez de canciones).

Las temporadas se arman agrupando los archivos de la categoría `series`
por el patrón de nombre `SxxEyy` — ver
`docs/contracts/library-layout-v1.md` §1 (nota D-318) para la regla
completa y la convención de póster de temporada
(`<Programa> S0N.jpg`, archivo aparte del póster de episodio). Un
archivo `series` sin ese patrón no se descarta: se vuelve su propia
"temporada" de un solo episodio, con su nombre completo como título —
fallback honesto, no un archivo perdido en silencio.

### 3 — Fuente de datos: Películas + Series, nunca Videoclips

Sin tagcache de por medio (Video no lo usa) — el pool combinado sale de
`aura_video.c` (`aura_video_count_filtered()`/`_filtered_filename()`,
D-316), mismo filtro "Películas+Series, nunca Videoclips" que ya usa
`CoverDrift` de Video (restricción textual del dueño, D-316) — Movie
Flow lo construye aparte porque necesita AGRUPAR por temporada, algo que
CoverDrift nunca necesitó (solo cuenta y nombra archivos).

### 4 — Entrar al reproductor: fundido a negro, nunca Flip-and-Flow

Music Flow entra a Ahora suena con el morph completo (`now-playing.md`,
Modo 4). Movie Flow **nunca hace eso** — encargo textual del dueño: un
fundido a negro simple (framebuffer actual atenuado hacia negro sobre
varios cuadros, respetando `lcd_active()`/Animaciones=Ninguna igual que
el resto de `aura_transitions.c`) y de ahí directo al reproductor.

Tampoco entra por `aura_nav_push(AURA_SCREEN_NOWPLAYING)` — esa pantalla
es el reproductor **interno** de Música. Un video toma la pantalla
completa vía `plugin_load(VIEWERS_DIR "/mpegplayer.rock", path)`, el
mismo mecanismo que ya usa la lista plana de Video (`aura_video.c`) —
Movie Flow es un llamador más de ese mismo plugin, no un reproductor
propio.

`BUTTON_PLAY` se deja **sin** semántica especial en Movie Flow (a
diferencia de Music Flow, donde reproduce el álbum enfocado al
instante) — y Movie Flow **no** se excluye de la intercepción global de
PLAY (pausa/reanuda música de fondo), a diferencia de
`is_musicflow_screen()`. Justificación: Movie Flow no reproduce nada
internamente mientras se navega — SELECT es la única acción que
reproduce, y ya tiene su propio camino (instantáneo o vía giro).

## Simplificación deliberada: transición de entrada

Music Flow tiene una coreografía de entrada especial cuando `CoverDrift`
está activo detrás de su fila en el submenú Música
(`aura_transition_musicflow_enter()`, D-259). Movie Flow **no** replica
esa coreografía — entra con el push genérico T1/T3 de ancho completo,
igual que la mayoría de pantallas. El encargo del dueño no mencionó la
transición de *entrada* a Movie Flow (solo la de *salida* hacia el
reproductor, diferencia 4) — replicar la coreografía de CoverDrift
habría exigido generalizar `video_row_wants_coverdrift()` y el bloque de
`aura_screens.c` que decide la transición, trabajo no pedido. Queda como
decisión abierta si el dueño la quiere más adelante.
