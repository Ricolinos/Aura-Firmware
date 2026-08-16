# CoverFlow

Vista de navegación de álbumes por carátulas — versión Aura del Cover Flow
clásico. Es una **habitación** en términos de
`sistema/02-navegacion-menus-contenido.md`: muestra contenido (álbumes),
no es un menú, y por tanto vive a pantalla completa, nunca en `LeftPanel`.
Se entra desde el submenú Música, donde **"Cover Flow" es una puerta
propia** (primera opción del submenú), nunca un disfraz automático de
"Álbumes".

## Base técnica (investigación 2026-08)

Rockbox ya incluye **PictureFlow** (`apps/plugins/pictureflow/pictureflow.c`,
~5,000 líneas en C, GPL v2, portado del PictureFlow de Ariya Hidayat). El
iPod Classic 6G corrió Cover Flow de fábrica — el hardware lo soporta con
certeza. Estrategia: **adaptar y re-estilizar PictureFlow para Aura**, no
reescribir desde cero.

### Técnicas clave que usa (la receta de rendimiento)

- **Punto fijo** (`PFreal`, shift de 10 bits) — nada de floats.
- **LUTs**: `sin_tab` (64 entradas + interpolación) para rotación;
  `reflect_table` precalculada para el degradado del reflejo.
- **Pseudo-3D por columnas**: cada slide tiene ángulo/posición/distancia; la
  perspectiva se rasteriza columna por columna sobre bitmaps guardados
  **transpuestos** (recorrer columnas = memoria contigua = rápido). Cada
  columna se **centra en la línea media vertical** de la carátula — el giro
  es sobre el eje central y ambos bordes convergen hacia el punto de fuga
  (anclar todas las columnas al mismo borde superior produce techos
  horizontales, error corregido en 2026-08).
- **Cache `.pfraw`**: carátulas pre-escaladas y transpuestas se guardan en
  disco una sola vez; runtime solo carga de un cache. La clave incluye
  tamaño Y radio de esquina — cambiar cualquiera regenera el cache solo.
  Cero decodificación JPEG durante la animación.
- **Reflejo**: la misma imagen invertida verticalmente + desvanecimiento.
- **Máquina de estados**: `idle → scrolling → cover_in → show_tracks →
  cover_out` — compatible con nuestro modelo de estados+transiciones.
- **Orden pintor real** (corregido 2026-08-12): las laterales se dibujan
  por distancia decreciente al centro, ambos lados por nivel (t±3, t±2,
  t±1) — la aproximación de "alternar extremos" dejaba carátulas del
  fondo encima de las de adelante durante los morphs.
- **El tema es parte de la llave de los cachés** (2026-08-12): el
  `.pfraw` en disco y los slots decodificados en RAM hornean las
  esquinas contra el fondo del tema vigente — al cambiar de tema se
  auto-invalidan y regeneran (sin esto quedaban halos del tema
  anterior).

### Carátulas: archivos y embebidas (resuelto 2026-08)

El pipeline busca primero archivos de imagen (`cover.jpg` y variantes, vía
`find_albumart()` — ojo: también busca en el directorio padre) y si no hay,
**extrae la carátula embebida en el track** (chunk `covr`/APIC, solo JPG —
no hay decoder PNG en el core; un `covr` PNG cae a la carátula Default).
La limitación heredada de PictureFlow quedó superada.

### Carátula "Default" (confirmada, con imagen de referencia)

Álbum sin arte: **nota musical gris sobre tile gris claro plano**
(referencia visual aportada por el dueño del diseño). Derivada de tokens:
fondo `SELECTION_FILL`, tinta = punto medio `SHELL_RAIL`↔`TEXT_SECONDARY`;
la nota es la máscara del icono `music` a 60px. Pasa por el mismo pipeline
de esquinas + reflejo que una carátula real — el renderer no distingue.
Se usa igual en el carrusel, el reproductor y las transiciones de vuelo.

## Geometría (confirmada contra captura del original de Apple, 2026-08)

| Propiedad | Valor |
|---|---|
| Carátula central | **130×130px**, borde superior en y=30, centrada horizontal |
| Laterales | 3 por lado, apretadas y traslapadas (~29px entre vecinas), tilt ~70° |
| Separación centro→primera lateral | la lateral queda pegada al borde del central |
| Atenuación de laterales | 165/255 — visibles, no apagadas |
| Reflejo | 25% del alto del slide, **opacidad pico 45%** (desvanece a 0 hacia abajo) |
| Esquinas | **8px de radio** (confirmado 2026-08, antes 5 provisional) — aplica en TODAS las pantallas donde se renderice el álbum, con antialias de cobertura fraccional |
| Texto | DOS líneas centradas bajo el central: título del álbum (Bold 12) + artista (Regular 12, secundario) |

La `StatusBar` se muestra en modo `(full)` — CoverFlow vive debajo de ella.

## Comportamientos (confirmados 2026-08)

- **Scroll**: snap con aceleración por velocidad real de la rueda. Acotado
  en ambos extremos — **sin loop**: nunca salta de inicio a final ni
  viceversa.
- **BACKWARD/FORWARD**: salto rápido de **10 álbumes** por pulsación, con
  el mismo deslizamiento suave con redirección; también acotado.
- **PLAY sobre la tapa enfocada**: reproduce el álbum COMPLETO al instante,
  **sin navegar al reproductor** — el usuario sigue en el carrusel (la
  confirmación visual es el icono de reproducción de la StatusBar). Sobre
  el álbum que ya suena, alterna pausa/reanudar. Funciona igual con la
  lista de canciones abierta.
- **Estado persistente**: la posición del carrusel se mantiene al abrir y
  cerrar un álbum, y al ir y volver del reproductor — nunca regresa sola
  al inicio.
- **SELECT**: voltea la tapa enfocada para mostrar su reverso (solo con el
  carrusel en reposo exacto).
- **Zoom al scrollear** (encargo del dueño del producto, 2026-08-14/15,
  ver detalle abajo): **todo el carrusel** (tapa central y laterales
  por igual) se encoge ligeramente al empezar a scrollear y vuelve a su
  tamaño normal al asentarse.

## Zoom del carrusel al scrollear (confirmado 2026-08-14, ampliado a todas las tapas 2026-08-15)

Al empezar a scrollear (paso real de rueda, `BACKWARD`/`FORWARD`, o
seguir girando a mitad de una ráfaga), **todas las tapas visibles del
carrusel** (no solo la central) se **encogen a ~84% (216/256) con
ease-in en 150ms**; en cuanto el carrusel se asienta en un álbum,
vuelven a su tamaño normal (256/256) con **ease-out, en 220ms** — el
mismo tiempo que ya usa el asentamiento de posición
(`CF_SCROLL_ANIM_MS`), para que ambos movimientos se sientan
coordinados. Si el usuario sigue girando la rueda mientras las tapas ya
están encogidas, no vuelve a "pulsar" con cada paso — se quedan
encogidas hasta que el carrusel de verdad se detiene.

**Curva, historial de ajustes** (probadas en el simulador, en este
orden, a pedido directo del dueño del producto):
1. 2026-08-14 (D-245): ease-in al encoger, ease-out al volver a normal.
2. 2026-08-15 (D-247): ease-out en las dos direcciones.
3. 2026-08-15 (D-248): ease-in en las dos direcciones.
4. 2026-08-15 (D-249, actual): **de vuelta a ease-in al encoger /
   ease-out al volver a normal** — la combinación que el dueño del
   producto confirmó como la correcta, la misma de D-245.

**Magnitud, historial de ajustes**:
1. 2026-08-14 (D-245): 240/256 (~6.25% de encogimiento) — elegida por
   comparación de capturas contra 248/256 (~3%, casi imperceptible) y
   228/256 (~11%, se sentía "de más") en ese momento.
2. 2026-08-15 (D-249, actual): **216/256 (~15.6% de encogimiento)** —
   el dueño del producto pidió un efecto más pronunciado tras probar
   240/256 con la combinación de curva ya confirmada. Verificado en el
   simulador que a esta magnitud, con todas las tapas del carrusel
   encogiendo a la vez (D-246), no hay recorte ni artefactos de render
   en ninguna tapa, y el reposo tras asentarse vuelve exacto al tamaño
   original.

`aura_motion_ease_out()` sigue en uso (ahora para el zoom-in de vuelta
a normal); ambas curvas conviven en este efecto, cada una en su
dirección.

**Ajuste 2026-08-15**: la primera versión (D-245) aplicaba el efecto
solo a la tapa central, atenuándolo a 0 hacia las laterales vía la
misma variable `t_center` que ya gradúa ángulo/posición/fade. El dueño
del producto pidió explícitamente que el encogimiento incluya a todas
las tapas del carrusel, para que se sienta como una sensación de
velocidad fluida del conjunto en vez de un efecto aislado en una sola
tapa (D-246) — se quitó esa atenuación solo para `distance`, que ahora
se aplica idéntico a cada tapa visible.

**Magnitud elegida**: 240/256 (~6.25% de encogimiento), comparado por
captura contra candidatos más sutiles (248/256, ~3%) y más marcados
(228/256, ~11%) — el elegido se nota con claridad en la pantalla
pequeña del iPod sin leerse exagerado; el más sutil casi no se
percibía, el más marcado ya se sentía "de más".

**Excepción deliberada a la regla de movimiento** (`Reglas de diseño
Apple2026 (v2).md` §6/§9.2): esa regla fija que las carátulas
(contenido) van en **fundido lineal**, y reserva el **resorte con
sobrepaso** (`aura_motion_spring`) exclusivamente a la capa de
controles, nunca al contenido. Este efecto es una excepción puntual y
deliberada a esa regla, pedida explícitamente por el dueño del
producto — pero la curva usada (`aura_motion_ease_in`/`_ease_out`,
cuadráticas, nuevas en `aura_motion.c`) sigue respetando el espíritu de
la regla histórica: son curvas simples que aceleran/desaceleran **sin
ningún sobrepaso/rebote**, muy distintas del resorte tipo iOS que sigue
prohibido sobre contenido. No es lo mismo "una curva no lineal sobre
contenido" que "el resorte con overshoot sobre contenido" — solo lo
segundo sigue vedado.

**Implementación**: reusa el canal `slide.distance` del motor de
perspectiva (`aura_flow.c`) que ya usa el crecimiento 130→200px del
giro del reverso — antes de D-245 ninguna tapa del carrusel normal lo
usaba (siempre 0). Se aplica idéntico a cada tapa visible del carrusel
(central y laterales) — a diferencia de ángulo/posición/fade (que sí
se siguen graduando con `t_center` según qué tan lateral es cada una,
eso no cambió). La coreografía de vuelo de entrada/salida al abrir/
cerrar Cover Flow (tapas que aparecen/desaparecen por los bordes) sigue
excluida del efecto — es una animación de otro tipo, no scroll dentro
del carrusel.

## El reverso del álbum (`show_tracks`) — diseño confirmado 2026-08

Al voltearse, el álbum **crece de 130 a 200×200px** (decisión del dueño
sobre tabla de opciones 180/200/220), centrado en el área útil bajo la
StatusBar. Estilos del menú:

| Elemento | Spec |
|---|---|
| Fondo | `#F4F4F4` en tema claro (derivado: `blend(SHELL_BG, TEXT_PRIMARY, 4.3%)` — mismo efecto relativo en oscuro), esquinas de 8px |
| Padding interno | 4px |
| Cabecera | 20px de alto, **"Álbum - Artista"** (Bold 10, centrado), con **Marquee Loop** si no cabe |
| Separador | barra de 1px (`SHELL_RAIL`) entre cabecera y lista, al ancho del contenido |
| Filas | 20px de alto, formato **"N. Nombre de la canción"** — N es la posición real en el disco (ordenado por `tracknumber` del tag; el playlist de reproducción usa el MISMO orden) |
| Texto de fila | a padding+10px de los bordes; seleccionada en ACENTO, resto en primario |
| Selección | pastilla GRIS (`SELECTION_FILL`) redondeada (radio del Selector), respeta el padding de 4px — más ancha que el texto, como en el menú |
| Scroll | `ScrollIndicator` (Fade-on-Idle) pegado al borde derecho interno, cuando la lista desborda |

Durante el giro de apertura: los textos del carrusel (título/artista) se
**deslizan hacia abajo hasta salir de pantalla** (ceden el espacio al
reverso crecido) y el **reflejo gira con la tapa, se desvanece y se
desliza hacia abajo, agrandándose en proporción** con el zoom del giro
(la tapa crece DURANTE la rotación, no en un corte). Al cerrar
(`cover_out`), todo ocurre en inversa.

**Giro al reverso (SELECT ↔ reverso, fases `cover_in`/`cover_out`):
300ms por fase** (D-274, decisión del dueño 2026-08-16; antes 260ms
provisional de D-104). Es un timing distinto del vuelo CoverFlow →
reproductor de 500ms descrito abajo — son dos coreografías, no una.

## Transiciones (confirmadas 2026-08)

### Entrada a CoverFlow (desde el submenú Música)

Caso sin `CoverDrift` activo (el comportamiento de siempre, sin cambios):
`LeftPanel` Y su StatusBar `(split)` salen empujados hacia la izquierda,
mientras la pantalla del CoverFlow entra desde el borde derecho POR ENCIMA
del `SelectionSummary`; cuando el contenido terminó de entrar, la StatusBar
`(full)` entra CAYENDO desde arriba (el `Push-and-Drop` de
`status-bar.md`).

Caso CON `CoverDrift` activo (D-259, implementado — **prueba acotada SOLO a
esta entrada** a pedido del dueño del producto, antes de replicarla a
cualquier otro punto de entrada a pantalla completa): ambos paneles salen
CADA UNO hacia su propio borde (izquierdo a la izquierda, derecho a la
derecha) y el CoverFlow se revela DETRÁS — ya renderizado completo, quieto,
desde el primer cuadro, no entrando desde ningún borde. La StatusBar
`(full)` de CoverFlow sigue cayendo desde arriba al final, igual que en el
caso sin `CoverDrift`. `aura_screens_coverdrift_active_for()`
(`aura_screens.c`) decide, en el instante exacto del `SELECT`, si
`CoverDrift` estaba realmente montado (no solo armado) para la fila
"Cover Flow" — nunca puede dar verdadero desde ningún otro origen que el
submenú de Música (`music_row_wants_coverdrift()` no califica ningún otro
para este destino).

### Vuelo CoverFlow → reproductor (reemplaza al `Flip-and-Flow` original)

Al seleccionar una canción en el reverso (~500ms, decisión del dueño):

1. El reverso (200px, con su lista real) hace **media vuelta continua en
   un solo sentido** — primera mitad: el panel gira de frente a perfil;
   segunda mitad: la carátula entra desde el perfil opuesto y aterriza en
   el tilt/posición EXACTOS del reproductor, con su reflejo apareciendo
   en proporción. El tamaño interpola de 200 a 135px durante el giro.
2. Mientras tanto, las tapas laterales **salen deslizándose hacia SU
   borde** (las de la derecha por la derecha, las de la izquierda por la
   izquierda).
3. Al aterrizar corre el morph de entrada del reproductor
   (`now-playing.md`, "Entrada desde CoverFlow").

La carátula nunca se corta ni se re-renderiza: es el mismo elemento que
viaja de una pantalla a la otra (requisito duro de continuidad).

### Regreso reproductor → CoverFlow

NO es la inversa del vuelo: la carátula ya está de frente, así que **no
hay giro ni paso por el reverso** — hace un **morph fluido de posición y
geometría** (135px/tilt 7° → 130px/frontal en el centro del carrusel) con
el reflejo visible todo el trayecto. Desde el **Modo 4 (letras)** el
regreso también existe: el despliegue inverso del panel se **encadena**
con este mismo morph (2026-08-12, ver `componentes/lyrics-panel.md`). Las tapas laterales **entran desde
los bordes** de la pantalla y el título/artista **suben desde el borde
inferior**. El carrusel queda EN REPOSO mostrando la carátula (no la
lista abierta).

Una pulsación = una navegación: los repeats del botón sostenido durante
una transición se ignoran hasta soltarlo (nunca navegaciones en cadena).

## Sub-componentes

| Sub-componente | Qué es |
|---|---|
| `CoverStack` | El carrusel de slides — central + 3 por lado en ángulo |
| `CoverReflection` | El reflejo bajo las carátulas (compartido con el reproductor: mismo material, mismo pico de 45%) |
| `CoverLabel` | Las dos líneas de texto (álbum Bold + artista Regular) bajo el central |
| `TrackList` (de CoverFlow) | El reverso de 200px con cabecera, lista numerada y ScrollIndicator |

## Curva de aceleración del scroll (confirmada, D-077/D-103)

La rueda entrega **velocidad angular real** (°/s) y el carrusel usa el
mismo `aura_wheel_step()` que las listas de menú: **paso = 1** a velocidad
baja o cero (precisión absoluta), crece con **v²** en la franja intermedia
y **tope ×3** antes del umbral de hojeo de **420°/s**. No existe un "giro
sostenido" como gesto propio: el clickwheel emite eventos discretos de giro,
no un botón mantenido, así que `scrolling` se define como "la posición
animada todavía no alcanza el álbum objetivo" e `idle` como "ya coinciden"
— mismo resultado observable que el PictureFlow original, sin depender de
un modelo de botón sostenido que Aura no tiene.

## Pendiente de definir

- [x] Sentido del giro del vuelo — confirmado por el dueño (D-274,
      2026-08-16): continúa el mismo sentido que la apertura del flip.
      Sin pendientes abiertos en este componente.
