# Árbol de menús

Estructura de navegación de Aura, replicando la del firmware original
del iPod Classic (referencia levantada por el dueño del diseño el
2026-08-13 desde el aparato real) con los añadidos propios de Aura.

**Cómo leer el estado**: ✅ construido · ◐ presente como fila **inerte**
(visible al 50%, no seleccionable) · ⬜ presente con **estado vacío**
(entra, muestra su título y un mensaje) · ⛔ omitido a propósito.

## Reglas que aplican a todo el árbol

- **Layout**: las listas de MENÚ van en `LeftPanel` (split); las de
  ELEMENTOS a pantalla completa. Ver `02-navegacion-menus-contenido.md`.
- **Filas que no existen todavía**: nunca se ocultan ni se dejan mudas.
  O son **inertes** (categorías del original sin contenido propio en
  Aura) o entran a un **estado vacío honesto** con su título. Una fila
  que no responde y no explica nada es un bug, no una omisión.
- **Iconos**: cada fila de menú lleva su símbolo SF lineal, elegido con
  el criterio de Apple Music/Finder; ninguno se repite entre hermanos.
  El ajuste **Mostrar iconos** los apaga globalmente (y recupera su
  columna, sin dejar sangría vacía).

## Menú de inicio

| Fila | Estado |
|---|---|
| Música | ✅ |
| Videos | ✅ (submenú) |
| Fotos | ✅ (submenú) |
| Extras | ✅ (submenú) |
| Ajustes | ✅ |
| Ahora suena | ✅ — **solo con reproducción activa**, como el original |
| Canciones aleat. | ✅ — acción: baraja toda la biblioteca y abre el reproductor |
| Podcasts | ⛔ hasta que exista soporte real |

## Música

| Fila | Estado |
|---|---|
| Cover Flow | ✅ |
| Listas repr. | ✅ → canciones |
| Artistas | ✅ → *Todos* + artistas → álbumes → canciones |
| Álbumes | ✅ → *Canciones* + álbumes → canciones |
| Recopilaciones | ⬜ |
| Canciones | ✅ |
| Géneros | ✅ → *Todos* + géneros → artistas → álbumes → canciones |
| Autores | ✅ (`tag_composer`) → *Todos* + autores → álbumes → canciones |
| Audiolibros | ◐ |
| Búsqueda | ✅ teclado clásico con resultados en vivo — ver `componentes/search-keyboard.md` |
| Genius | ⛔ (no existe la función; no se simula) |

**Fila sintética**: las listas que agrupan encabezan con *Todos*
(*Canciones* en Álbumes), que abre la lista agregada del nivel siguiente
conservando los filtros vigentes. Vive en el índice 0 de la caché de la
lista, así que rueda, selección y ScrollIndicator la tratan como una
fila más.

## Videos · Fotos

| Fila | Estado |
|---|---|
| Videos → Todos los videos | ✅ (visor real) |
| Videos → Películas · Programas de TV · Videoclips | ◐ |
| Videos → Ajustes (Salida TV, Señal TV…) | ⛔ sin hardware que los soporte |
| Fotos → Todas las fotos | ✅ (visor real) |

## Extras

| Fila | Estado |
|---|---|
| Cronómetro | ✅ contador `HH:MM:SS` con centésimas más chicas; SELECT arranca y luego marca registros; PLAY detiene conservando el tiempo (icono de pausa = reanudable); MENU sale guardando |
| Reloj internacional | ✅ relojes analógicos con esfera clara (local) y oscura (otros husos); menú Añadir/Editar/Eliminar — ver `componentes/world-clock.md` |
| Notas | ✅ pantalla de instrucciones del original |
| Juegos | ◐/✅ **Klondike** funciona (plugin `solitaire` de Rockbox); iQuiz y Vortex inertes |
| Calendarios | ✅ rejilla del mes (semana desde lunes); rueda = días, botones = meses, Select = día |
| Bloqueo pantalla | ✅ candado + clave de 4 dígitos con confirmación; MENU restablece sin configurar |
| Alarmas | ✅ lista + editor con las filas del original (Alarma, Hora con reloj analógico en vivo, Repetir, Sonido, las 23 Etiquetas, Eliminar) |
| Agenda | ◐ |

**Juegos** (investigado 2026-08-13): los originales (iQuiz, Klondike,
Vortex) son **inviables** — sus ejecutables están cifrados con FairPlay
con llave única por dispositivo, y correrían sobre la API de RetailOS,
indocumentada. La ruta viable es: Klondike vía el plugin `solitaire` de
Rockbox re-estilizado, e iQuiz como pantalla nativa (su banco de
preguntas es texto plano). Vortex requeriría motor propio.

## Ajustes

Orden por **secciones sin separadores visibles** — el orden ES la
agrupación, como en el original:

| Sección | Filas |
|---|---|
| Información | Acerca de ✅ (3 modos) |
| Reproducción | Aleatorio ✅ · Repetir ✅ · Menú principal ✅ |
| Apariencia (propios de Aura) | Tema ✅ · Color de acento ✅ · Animaciones ✅ · Gráficos ✅ · Mostrar sombras ✅ · Mostrar iconos ✅ |
| Pantalla | Brillo ✅ · Temporiz. luz ✅ |
| Sonido | Ecualizador ✅ (23 presets) · Límite volumen ✅ · Ajuste volumen ✅ (replaygain real) · Audiolibros ✅ · Sonido de clic ✅ |
| Sistema | Temporiz. reposo ✅ · Fecha y hora ✅ (Zona horaria real, Reloj 24 h, Hora en el título) · Ordenar por ✅ · Idioma ✅ (catálogo completo, no traducidos inertes) · Avisos legales ✅ · Restablecer ajustes ✅ |

Del original quedan pendientes: **Menú Música** configurable, los
**editores de Fecha y Hora** (calendario y reloj analógico) y el **mapa
con pin** de Zona horaria — la lista de ciudades ya cumple su función.
La velocidad de
audiolibros y el orden por apellido se guardan pero todavía no cambian
nada: Rockbox no tiene backend para ellos.
