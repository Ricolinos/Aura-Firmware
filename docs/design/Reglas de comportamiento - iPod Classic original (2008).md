Fuente: auditoría manual menú por menú (`Firmware orginal del ipod.md`, `Uso de la interfaz.mp4`) más la descripción directa de las transiciones de Cover Flow dada en el chat. Este documento describe **cómo se comporta de verdad el iPod Classic original** — es la base empírica sobre la que se construyen las reglas de `Apple2026`. No se omite nada de la investigación original: lo que cambia es la organización.

Los estados "sin archivos" y "con archivos" del firmware original son casi idénticos — la única sección que cambia de comportamiento es **Música** (el resto de las secciones no dependen de tener contenido). Por eso este documento no repite el árbol dos veces: la sección 4.1 marca explícitamente qué cambia entre un estado y otro.

---

# 1. Taxonomía de pantallas

Todo estado de la interfaz cae en una de estas categorías. La sección 4 etiqueta cada nodo del menú con su código.

| Código | Nombre | Descripción |
|---|---|---|
| `SPLIT` | Pantalla dividida | Panel izquierdo (lista, ancho fijo) + panel derecho (ícono, imagen o vacío) reactivo a la selección |
| `LISTA-COMPLETA` | Lista de ancho completo | Igual que una lista normal pero sin panel derecho — nivel 3 en adelante, por defecto |
| `FULL-COLD` | Completa sin memoria | Pantalla nueva sin relación visual con lo anterior; entra deslizando y empuja todo lo previo fuera |
| `FULL-CARRY` | Completa con elemento heredado | Un elemento del panel derecho (ícono, imagen) sobrevive a la transición y se estira a pantalla completa |
| `FULL-COVERFLOW` | Completa — Cover Flow | Caso único con dos variantes, según haya o no carátulas (ver 2.5) |
| `MODAL` | Modal flotante | Se superpone sobre una pantalla que ya es de pantalla completa |
| `OPCIÓN` | Fila de opción simple | Booleano o lista corta de valores (Sí/No, NTSC/PAL, etc.) — sigue la regla de profundidad por defecto, no tiene mecánica propia |
| `SIN EXPLORAR` | No auditado | El usuario llegó al nodo pero no entró más profundo, o el submenú no se investigó |
| `INERTE` | Sin contenido | La opción existe en el menú pero "no hace nada" porque no hay datos que mostrar (sin música, sin video, sin fotos, etc.) |

**Regla de profundidad por defecto** (aplica salvo excepción marcada explícitamente):
- Nivel 1 y 2 de navegación → `SPLIT`
- Nivel 3 en adelante → `LISTA-COMPLETA`

---

# 2. Catálogo de transiciones

### 2.1 — Scroll dentro de `SPLIT`
Girar la rueda: el resaltado del panel izquierdo es instantáneo; el panel derecho tarda **~1s** en reflejar la nueva selección (ícono/imagen/vacío). Por eso, al entrar recién a un submenú, el panel derecho suele caer en su vista más inmediata — p. ej. entrar a Música cae sobre Cover Flow, que no tiene nada que mostrar en el panel derecho, así que este llega vacío.

### 2.2 — `SPLIT` → `SPLIT` (bajar de nivel)
Panel izquierdo: instantáneo, barrido de derecha a izquierda, afecta **solo** al panel izquierdo. Panel derecho: se actualiza ~1s **después** de completada la entrada — el mismo retardo que en el scroll (2.1), no uno nuevo.

### 2.3 — `SPLIT` → `FULL-COLD` (Genius, Búsqueda, listas sin herencia)
Ningún elemento previo sobrevive. La pantalla nueva entra completa desde el borde derecho, deslizándose hacia la izquierda: primero cubre el panel derecho; al llegar al borde del panel izquierdo lo **empuja** fuera de pantalla (lo desplaza, no lo tapa). La barra de estado es enteramente nueva, sin relación con la anterior.

### 2.4 — `SPLIT` → `FULL-CARRY` (Hora, Fecha, Candado, Zona horaria, Brillo…)
El panel derecho ya mostraba el ícono relevante *antes* de entrar (p. ej. con "Hora" seleccionada, el panel derecho ya tiene un reloj analógico animado, hora arriba y día abajo). Al entrar:
1. Los textos auxiliares (hora, día) desaparecen.
2. El panel derecho se estira/desplaza de derecha a izquierda hasta cubrir el ancho completo, conservando el ícono — que se combina con el nuevo control de edición.
3. El panel izquierdo (y su mitad de barra de estado) sale de pantalla por la izquierda.
4. **Simultáneamente** aparece una barra de estado nueva, de ancho completo, con contenido contextual distinto (p. ej. la fecha en vez del título del menú).

### 2.5 — `SPLIT` → `FULL-COVERFLOW`
Caso especial. Depende de si la biblioteca tiene carátulas:

- **(a) Sin carátulas.** Cover Flow entra como pantalla completa, deslizándose de derecha a izquierda, pasando **por encima** del panel derecho (lo cubre, no lo empuja) y empujando **solo** al panel izquierdo fuera de pantalla. Es una variante de `FULL-COLD` sin memoria.
- **(b) Con carátulas.** La carátula ya se mostraba en el panel derecho, con su animación ambiental (ver 3.1). Cover Flow, en este caso, vive renderizado **detrás de ambos paneles todo el tiempo**: al entrar, el panel izquierdo sale por la izquierda y el derecho sale por la derecha — como una cortina que se abre — revelando Cover Flow, que "emerge" desde el centro. La barra de estado tiene movimiento propio e independiente de los paneles: no sale con el panel izquierdo, sino que la nueva barra **cae desde arriba**, ya en formato pantalla completa.

### 2.6 — `MODAL` (álbum seleccionado dentro de Cover Flow)
Tocar una carátula: gira sobre su eje vertical, revelando una lista de canciones en forma de modal flotante detrás (título de álbum + artista arriba, lista scrolleable debajo). Elegir una canción: la reproducción empieza de inmediato; el álbum vuelve a girar y se ubica a la izquierda (conserva efecto 3D y reflejo), el resto de carátulas sale de pantalla y aparece la interfaz del reproductor. Salir con Menú: la carátula activa regresa al centro y el resto reingresa desde los bordes.

---

# 3. Reglas transversales

### 3.1 — Animación ambiental de carátulas
Aparece en Música, Videos (películas) y Fotos — cada una limitada a su propio menú y submenús, nunca cruzada. Fundido de <0.5s entre carátulas; cada una se sostiene **7s**; el movimiento de deriva es aleatorio entre 8 direcciones (vertical arriba/abajo, horizontal izquierda/derecha, y 4 diagonales: 60°, 120°, 240°, 300°).

### 3.2 — Barra de estado
- Batería: siempre visible, siempre a la derecha.
- Control de reproducción (play/pausa): junto a la batería, a su izquierda — pero si el dispositivo está bloqueado y nada suena, ese lugar lo ocupa el candado; si además hay algo sonando, el candado se recorre a la izquierda del ícono de reproducción.
- Título: alineado a la izquierda si la pantalla es un menú de navegación; centrado y contextual si la pantalla tiene identidad propia (reloj internacional → fecha, calendario → hora, reproductor → "Ahora suena").
- La hora casi nunca se renderiza en la barra de estado; solo aparece en pantallas puntuales.
- La barra dividida (mitad de ancho, en `SPLIT`) y la barra completa (`FULL-*`) son elementos independientes que se alternan — no es la misma barra redimensionándose.

### 3.3 — Modelo de capas (de arriba hacia abajo, orden de renderizado)
1. Barra de estado lateral (mitad, sobre el panel izquierdo)
2. Panel izquierdo (lista)
3. Imagen del panel derecho (carátula de álbum/video/foto, si aplica)
4. Barra de estado de ancho completo (entra junto con la pantalla completa, o por separado — ver 2.5b)
5. Pantalla completa (si aplica)
6. Panel derecho (ícono o imagen + info) — dos variantes: (a) se convierte en pantalla completa (`FULL-CARRY`), (b) se queda como panel con imagen + info

### 3.4 — Fotos
Solo se muestran fotos ya optimizadas por Apple para el dispositivo. Dos modos: ajustada al marco (con bordes negros si no comparte aspect ratio con la pantalla) o pantalla completa (recortada y centrada, con paneo si el aspect ratio lo amerita — vertical **u** horizontal, nunca ambos a la vez — y **nunca** zoom).

### 3.5 — Búsqueda
Teclado en mayúsculas para la selección, pero el texto se escribe en minúsculas. Truncamiento: en la caja de búsqueda, 3 puntos antes de los últimos 3 caracteres si el texto es largo, o solo 5 caracteres visibles por defecto. En el recuadro superior (fuente más pequeña) se ve el texto completo hasta 22 caracteres; después, 3 puntos a la derecha. Cursor `|` parpadeante. Forward = espacio, backward = borrar (mantener presionado = borrar todo). Si se sale por accidente con Menú, el texto se conserva sin importar cuántos niveles se retrocedan.

### 3.6 — Cronómetro
Ícono + botón Play/Pausa. Select inicia el contador (formato `00:00:00.00`, milisegundos en tamaño menor). Select otra vez guarda una vuelta (máximo 3 visibles bajo el contador corriendo, aunque se registran todas — verificado con una prueba real: registro "10-8-26, 1:13pm", donde el detalle interno mostraba `Total 00:01:37.871`, con vueltas `Corto`/`Largo`/`Media` en cero y los `Tiempo N` individuales sí registrados aunque no se rendericen en vivo). Play/Pausa detiene y regresa al panel lateral con una transición de pantalla completa → panel (la inversa de `FULL-CARRY`), conservando el último tiempo con un ícono de pausa; aparecen las opciones Reanudar / Nuevo temporizador / Borrar registros / Registro actual / historial por fecha. Reanudar retoma con la misma transición fluida, desde el último punto.

### 3.7 — Bloqueo de pantalla
Candado + "Falta la clave" en el panel derecho; transición `FULL-CARRY` (el mismo ícono se estira a pantalla completa). Configuración con 4 dígitos (0–9) mediante la rueda, pide confirmación; Menú cancela sin guardar, Select confirma al final.

---

# 4. Inventario completo del menú

Cada nodo lleva su código de pantalla entre corchetes. Los nodos sin código heredan la regla de profundidad por defecto (sección 1).

## 4.0 — Menú principal `[SPLIT]`
Barra de estado: "iPod" + batería. Entradas: Música, Videos, Fotos, Podcast, Extras, Ajustes, Canciones aleat.

## 4.1 — Música

**Diferencia entre estados:** sin archivos, todas las entradas de Música están `INERTE` excepto Genius (su pantalla explicativa no depende de contenido) y Búsqueda (la interfaz existe igual, solo no devuelve resultados). Con archivos, se activan como se detalla abajo.

- **Cover Flow** `[FULL-COVERFLOW]` — inerte sin carátulas cargadas; ver 2.5 para el comportamiento con archivos.
- **Genius** `[FULL-COLD]` — pantalla explicativa, texto se corta a mitad de oración: *"Genius reproduce canciones de su iPod que combinan a la perfección. Para visualizar la opción Genius, puede empezar desde la pantalla Ahora suena o seleccionar una canción que le g…"*
- **Listas repr.** — lista de listas de reproducción → canciones de la lista seleccionada.
- **Artistas**
  - Álbumes → Canciones (todas, alfabético) / Álbumes (alfabético) → canciones del álbum
  - Artistas (alfabético) → álbumes del artista (alfabético) → canciones del álbum
- **Álbumes**
  - Canciones (todas, alfabético)
  - Álbumes (alfabético) → canciones del álbum
- **Recopilaciones** `[SIN EXPLORAR]` — no investigado.
- **Canciones** — todas, alfabético.
- **Géneros**
  - Artistas → Álbumes → Canciones (misma estructura anidada que la rama Artistas)
  - Géneros (alfabético) → Álbumes del género (alfabético) → canciones del álbum
  - Géneros (alfabético) → Artistas del género → canciones del artista dentro del género
- **Autores**
  - Álbumes → Canciones / Álbumes (alfabético) → canciones del álbum
  - Autores (alfabético) → álbumes del autor (alfabético) → canciones del álbum
- **Audiolibros** `[INERTE]` — sin audiolibros en la biblioteca de prueba, incluso con archivos.
- **Búsqueda** `[FULL-COLD]` — ver 3.5 para el detalle completo de la interfaz.

## 4.2 — Videos
Idéntico en ambos estados (sin contenido de video en la prueba).

- **Películas** `[INERTE]`
- **Alquileres** `[INERTE]`
- **Programas de TV** `[INERTE]`
- **Videoclips** `[INERTE]`
- **Listas repr. vídeo** `[INERTE]`
- **Ajustes**
  - Salida TV `[SIN EXPLORAR]` — múltiples opciones, no se entró al submenú.
  - Señal TV `[OPCIÓN]` — NTSC / PAL
  - Pantalla TV `[OPCIÓN]` — Estándar / Panorámico
  - Ajustar a pantalla `[OPCIÓN]` — Activado / Desactivado
  - Audio alternativo `[OPCIÓN]` — Activado / Desactivado
  - Subtítulos opcionales `[OPCIÓN]` — Activado / Desactivado
  - Subtítulos `[OPCIÓN]` — Activado / Desactivado

## 4.3 — Fotos
Idéntico en ambos estados (sin fotos en la prueba).

- **Todas las fotos** `[INERTE]`
- **Ajustes**
  - Tiempo por diapositiva `[OPCIÓN]` — Manual / 2s / 3s / 5s / 10s / 20s
  - Música `[OPCIÓN]` — Ahora suena / Desactivada / On-The-Go / (cuarta opción sin etiqueta clara, pendiente de confirmar)
  - Repetir `[OPCIÓN]` — Sí / No
  - Aleatorio fotos `[OPCIÓN]` — Sí / No
  - Transiciones `[OPCIÓN]` — Ninguna / Aleatorio / Fundido / Fundido negro / Reducir Zoom / Barrido transversal / Barrido centrado
  - Salida TV `[SIN EXPLORAR]` — múltiples opciones, no se entró al submenú.
  - Señal TV `[OPCIÓN]` — NTSC / PAL

Ver 3.4 para el comportamiento de renderizado de fotos (ajuste vs. pantalla completa, paneo).

## 4.4 — Podcast `[INERTE]`
Sin podcasts en la prueba; el menú no despliega submenú.

## 4.5 — Extras

- **Reloj internacional** `[FULL-CARRY]` (variante) — ubicación + reloj analógico + hora numérica AM/PM. Manecillas blancas = hora local; manecillas negras = otras regiones agregadas. Select abre un menú (renderizado como imagen/PNG) con:
  - **Añadir** → selector jerárquico continente → país: África, Asia, Atlántico, Australia, Europa, Norteamérica, Pacífico, Suramérica
  - **Editar**
  - **Eliminar**
- **Calendarios**
  - **Todos** `[FULL-COLD]` — interfaz de calendario mensual propia; el click wheel mueve el día seleccionado, avanzar/retroceder cambia de mes, botón central entra al día → *"No hay eventos este día"*. Play/pausa no hace nada. Menú regresa a la pantalla anterior.
  - **Tareas** `[FULL-COLD]` — pantalla de instrucciones: *"Para ver sus tareas aquí, active la sincronización en iTunes (en la sección Calendario de la pestaña contactos). Para más información, consulte el Manual de funciones del iPod o visite www.apple.com/es/support/ipod."*
  - **Alarmas** `[OPCIÓN]` — Bip / Ninguna / Desactivada
- **Agenda** `[INERTE]`
- **Alarmas**
  - **Alarmas (crear nueva)**
    - Alarma `[OPCIÓN]` — Activado / Desactivado
    - Compromiso `[FULL-CARRY]` — ícono de calendario que se anima en tiempo real junto con la configuración
    - Hora `[FULL-CARRY]` — reloj analógico que mueve las manecillas según la configuración
    - Repetir `[OPCIÓN]` — Una vez / Cada día / Entre semana / Fin de semana / Cada semana / Cada mes / Cada año
    - Sonido
      - Tonos `[OPCIÓN]` — Ninguna / Bip
      - Listas de reproducción `[OPCIÓN]` — Ninguna (sin listas configuradas como tono en la prueba)
    - Etiqueta `[OPCIÓN]` — lista fija: Despertador, Trabajo, Clase, Alarma, Cita, Llamada telefónica, Desayuno, Comida, Cena, Importante, Receta, Medicamento, Devolver llamada, Entrega, Recogida, Reunión, Recordatorio, Compromiso, Aniversario, Cumpleaños, Festividad, Ir a casa, Fiesta
    - Eliminar
  - **Temporiz. reposo** `[OPCIÓN]` (inactivo en la prueba) — Desactivado / 15 / 30 / 60 / 90 / 120 minutos
  - Si ya existen alarmas activas, aparecen como entradas adicionales del menú, editables con las mismas opciones de arriba.
- **Juegos** — iQuiz, Klondike, Vortex `[SIN EXPLORAR]` — no se entró a ningún juego.
- **Notas** `[FULL-COLD]` — pantalla de instrucciones: *"Para ver archivos de texto aquí, active la opción de usar el iPod como disco en iTunes y, a continuación, arrastre los archivos de texto a la carpeta Notes del iPod. Para más información, consulte el Manual de funciones del iPod o visite www.apple.com/es/support/ipod."*
- **Bloqueo pantalla** `[FULL-CARRY]` — ver 3.7.
- **Cronómetro** — ver 3.6.

## 4.6 — Ajustes

- **Acerca de** `[FULL-COLD]` — logo de Apple centrado, 3 modos que se navegan con backward/forward (select = forward; play/pausa y click wheel no hacen nada):
  1. Barra de espacio de almacenamiento usado, por categoría: Audio, Video, Fotos, Otros
  2. Contador de archivos por medio: Canciones, Videos, Podcast, Fotos, Juegos, Contactos
  3. Info específica del dispositivo — ejemplo registrado: número de serie `8K8444S12C5`, modelo `MB562LL`, versión `2.0.1 Mac`
- **Aleatorio** `[SIN EXPLORAR]` — múltiples opciones, no se entró al submenú.
- **Repetir** `[SIN EXPLORAR]` — múltiples opciones, no se entró al submenú.
- **Menú pral.** `[LISTA-COMPLETA]` — checklist de qué aparece en el menú principal (checkmark = visible, sin marca = oculto). También expone en "submenús" opciones que podrían vivir en el nivel raíz aunque por defecto estén anidadas (ej. Coverflow siempre vive dentro de Música, pero puede además mostrarse en el menú principal). Entradas:
  - Música → Coverflow, Genius, Listas repr, Artistas
  - Video…
  - Fotos…
  - Podcasts
  - Extras…
  - Canciones aleatorias
  - Reposo
  - Restaurar menú principal — regresa al estado configurado de fábrica por Apple.
- **Menú Música** `[LISTA-COMPLETA]` — mismo mecanismo de checklist que "Menú pral.", pero para configurar qué aparece dentro del submenú de Música.
- **Límite volumen** `[FULL-CARRY]` — barra de volumen de menos a más, con una flecha gráfica que marca el tope configurado.
- **Temporiz. luz** `[OPCIÓN]` — Desactivado / 2s / 5s / 10s / 20s / 30s / Siempre activada
- **Brillo** `[FULL-CARRY]` — barra gráfica de configuración.
- **Audiolibros** `[FULL-COLD]` — pantalla con texto: *"¿Desea ajustar la velocidad de reproducción de los audiolibros? La velocidad de reproducción no afectará al tono de voz de la reproducción de los audiolibros."* Opciones: Lenta / Normal / Rápida.
- **EQ** `[LISTA-COMPLETA]` — cada opción muestra un ícono con una gráfica distinta representando la curva. Lista completa: Desactivado, Acústica, Amplif. graves, Reducir graves, Clásica, Dance, Deep, Electrónica, Flat, Hip Hop, Jazz, Latina, Loudness, Lounge, Piano, Pop, R&B, Rock, Minialtavoces, Voz, Amplif. agudos, Reducir agudos, Aumentar voz.
- **Ajuste volumen** `[OPCIÓN]` — Sí / No
- **Clicker** `[OPCIÓN]` — Sí / No
- **Fecha y hora**
  - Fecha `[FULL-CARRY]` — calendario personalizado que refleja la configuración numérica seleccionada.
  - Hora `[FULL-CARRY]` — reloj analógico personalizado que mueve las manecillas junto con la configuración numérica.
  - Zona horaria `[FULL-CARRY]` — mapa con un solo pin; al scrollear con el click wheel, el pin se mueve en orden de latitud entre países. Abajo aparece una barra de confirmación con nombre del país, formato de hora y hora resultante. Ejemplo registrado: *"Ciudad de México — GMT-6 horas — 2:37PM"*.
  - Reloj 24 horas `[OPCIÓN]` — Sí / No
  - Hora en el título `[OPCIÓN]` — Sí / No
- **Ordenar por…** `[OPCIÓN]` — Nombre / Apellido
- **Idioma** `[LISTA-COMPLETA]` — lista de idiomas disponibles (English, Dansk, Español, etc. — lista completa no registrada en la auditoría original).
- **Copyright** `[FULL-COLD]` — texto legal extenso de Apple y el dispositivo, sin interacción más allá de scroll y salir.
- **Reset Settings** `[SIN EXPLORAR]` — no se ejecutó durante la auditoría.

## 4.7 — Canciones aleat. `[SIN EXPLORAR]`
Entrada de nivel raíz junto a Ajustes; no se registró qué pantalla dispara al seleccionarla (es de suponer que inicia reproducción aleatoria directamente, sin pantalla propia, pero no está confirmado).

---

# 5. Pendientes de esta auditoría

Preguntas abiertas para revisar contra el video o el dispositivo físico cuando puedas — se mantienen de la versión anterior de este documento más las que surgieron al integrar el inventario completo:

- [ ] ¿"Recopilaciones" dentro de Música tiene pantalla propia si llega a tener contenido, o cae en `LISTA-COMPLETA` genérica?
- [ ] En Ajustes → EQ, ¿el ícono con la gráfica por cada opción vive en un panel derecho tipo `SPLIT`, o es parte de la fila misma (lista con ícono inline)? Se documentó como `LISTA-COMPLETA` por ahora, pendiente de confirmar.
- [ ] Confirmar si "Notas" y "Agenda" (ambas sin contenido sincronizado) muestran alguna pantalla de estado tipo página de símbolo, o simplemente quedan inertes — "Notas" sí tiene texto de instrucciones confirmado, pero "Agenda" quedó marcada solo como `INERTE` sin más detalle.
- [ ] Fotos → Ajustes → Música: falta confirmar la cuarta opción sin etiqueta clara.
- [ ] Videos → Ajustes → Salida TV y Fotos → Ajustes → Salida TV: no se exploró el submenú, se desconoce su tipo de pantalla.
- [ ] Ajustes → Aleatorio y Ajustes → Repetir: no se exploró el submenú, se desconoce si son `OPCIÓN` simple o algo con interfaz propia.
- [ ] Ajustes → Idioma: falta registrar la lista completa de idiomas disponibles.
- [ ] Ajustes → Reset Settings: no se ejecutó, se desconoce si pide confirmación con interfaz propia o es un `OPCIÓN` con diálogo simple.
- [ ] Extras → Juegos: no se entró a iQuiz, Klondike ni Vortex — se desconoce si tienen splash screen propia al entrar.
- [ ] Canciones aleat. (nivel raíz): no se confirmó si dispara reproducción directa o tiene pantalla intermedia.
