# LyricsPanel (Modo 4 del reproductor)

La hoja de letras del Modo 4 de `NowPlaying` — el único lugar del
sistema donde el reproductor se vuelve pantalla completa a dos paneles.
Todo lo confirmado aquí es del 2026-08-12 (sesión de cierre del
reproductor, DECISIONS.md D-136 a D-148).

## Anatomía

| Zona | Contenido |
|---|---|
| Panel izquierdo (130×240) | Estado comprimido del reproductor: carátula cuadrada con sombra difusa, barra de 122px, transporte completo y fila de modos — **fondo normal** (`SHELL_BG`), NO es el componente `LeftPanel` |
| Hoja derecha (190×240) | El `LyricsPanel` en sí: vidrio traslúcido del color del álbum, cabecera con los textos de la canción y la letra sincronizada |

Sin `StatusBar`: se desliza hacia arriba durante el morph de entrada y
regresa deslizándose al salir.

## La hoja de vidrio

- **Entrada/salida**: la hoja **se desliza desde la derecha** (no un
  fade), con todo su contenido montado — los textos llegan ya
  renderizados, moviéndose con ella.
- **Material**: vidrio traslúcido — tinte del **~85%** sobre lo que ya
  esté dibujado debajo (durante el morph se adivinan los restos del
  layout anterior a través). ⚠️ Excepción deliberada del dueño del
  diseño a "el vidrio vive solo en la capa de controles" (pedido
  explícito 2026-08-12).
- **Color**: el **PROMEDIO RGB de la carátula en curso**, derivado en
  runtime de la imagen real (nunca un RGB fijo), en **degradado
  DIAGONAL**: aclarado (+15%) en la esquina superior izquierda,
  oscurecido (−15%) en la inferior derecha.
- **Esquinas**: la hoja es **CUADRADA** — las esquinas redondeadas de
  pantalla no se estampan sobre ella (solo sobreviven las del lado
  izquierdo).
- **Jerarquía de capas**: el panel izquierdo es la capa ELEVADA —
  proyecta una **sombra paralela sobre la hoja** desde su borde
  (franja de 8px decayendo a la derecha). La sombra solo cae donde la
  hoja ya está debajo, y aparece con su **propio fade** (~165ms)
  después del acople.

## Tinta sobre el vidrio

Decidida por **luminancia** del color promedio: blanco constante sobre
vidrio oscuro; una versión muy oscura del mismo color (22% de sus
canales) sobre vidrio claro. La jerarquía es por atenuación:

| Elemento | Fuerza de tinta |
|---|---|
| Título de la canción / línea activa | Plena |
| Artista y álbum (cabecera) | ~69% |
| Líneas de contexto de la letra | ~59% |
| Puntos de silencio apagados | ~30% |

## Cabecera

Título (Bold 12), artista y álbum (Regular 12) viven en la **parte
superior de la pantalla**, montados en la hoja — los mismos textos que
se desvanecen del layout normal del reproductor renacen aquí.

## La letra

- Avance **automático sincronizado** con la canción (formato `.lrc`,
  archivo hermano de la pista).
- **Línea activa**: Bold 14, y se **divide por palabras en hasta 3
  renglones** para leerse completa — aquí NO hay Marquee Loop.
- **Líneas de contexto** (Regular 12): hasta 2 arriba y 2 abajo de la
  activa, **recortadas** al ancho del panel.
- **Silencios**: cuando el hueco entre líneas es largo (**≥8s, con ~3s
  de "lectura" de la línea saliente** — ratificado por el dueño el
  2026-08-16, D-274; ya no es provisional), el centro muestra
  **3 puntos de 6px** que se **iluminan uno a uno** conforme avanza el
  silencio — los tres encendidos = la letra está por volver. Funciona
  también como intro instrumental antes de la primera línea. La última
  línea cantada queda arriba y las próximas debajo.

## Interacción

- **Scroll**: **avanza la canción** (mismo paso de 3s por click y mismo
  estrangulado de audio que el Modo 2), **sin números** — el ajuste se
  visualiza en la barra de progreso del panel izquierdo y en el avance
  de la propia letra.
- **Select**: sale al modo siguiente con el morph invertido.
- **Menu**: sale del reproductor. Si se **entró por coverflow**, el
  despliegue inverso del panel se **encadena con el morph de regreso
  al carrusel** (un solo gesto fluido); a cualquier otro destino, la
  pantalla completa **se desplaza hacia la derecha** dando paso al
  menú anterior.

## Disponibilidad

Si la canción no tiene `.lrc`, el Modo 4 se desactiva: su ícono queda
al 50% de opacidad en la fila de modos y el loop lo salta. Si la pista
cambia a una sin letras estando dentro, el reproductor regresa solo al
Modo 1.

## El morph de entrada/salida (~330ms, fundido lineal de contenido)

Detallado en `componentes/now-playing.md` ("Modo 4"): carátula de
135px/7° con reflejo → cuadrado perfecto de 106px con sombra difusa
(mismo proyector de columnas del morph de regreso al carrusel); barra a
122px; transporte y fila de modos viajan al centro del panel; textos se
desvanecen y renacen en la cabecera; las estrellas desaparecen; la
StatusBar se desliza hacia arriba.
