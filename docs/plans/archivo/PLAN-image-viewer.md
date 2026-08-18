# PLAN — Visor de imágenes (Fotos)

> **ESTADO: EJECUTADO** — 2026-08-17. Histórico. No es trabajo pendiente.
> Decisiones en `DECISIONS.md` (D-291); el contrato de `Photos/` que este plan
> propuso vive en `CONTRATO-firmware-studio.md` §D.1.

**Estado**: Fase 0 (diagnóstico) y Fase 1 (plan) terminadas — 2026-08-17. **BARRERA**: la Fase 2 no arranca hasta aprobación explícita del dueño.
**Alcance**: firmware (`Aura-Firmware`). El contrato de la §6 es lo que desbloquea la pasada posterior en `Aura-Studio`.
**Numeración**: la bitácora va en `DECISIONS.md` a partir de **D-291** (el encargo decía D-287+, pero D-286…D-290 ya existen).

---

## 0. Resumen ejecutivo

**"No aparece nada" NO es un problema de formato ni de rutas: la pantalla "Todas las fotos" no tiene caso de dibujo.** `AURA_SCREEN_PHOTOS_ALL` (el destino real de la fila "Todas las fotos") cae al fallback genérico y muestra "Nada sonando" con el directorio lleno de JPEG válidos. El visor completo (lista + decodificación JPEG/BMP con el decoder del core) existe, está cableado a los botones y **funciona** en cuanto se le devuelve el caso de dibujo — se verificó en el simulador con un parche temporal de una línea (revertido después, ver §1.4).

El arreglo ya había existido: lo introdujo D-251 (commit `246f99f`, 2026-08-15) y se perdió 40 minutos después con el `git revert` en bloque de D-253, que lo documenta textualmente: *"esto incluye deshacer… el arreglo real de `AURA_SCREEN_PHOTOS_ALL` sin caso de dibujo… la pantalla de Fotos vuelve a ser inalcanzable por navegación normal — si se quiere conservar ese arreglo puntual… hay que reaplicarlo aparte"*. Nunca se reaplicó. **`AURA_SCREEN_VIDEOS_ALL` tiene exactamente el mismo bug** (también documentado y también sin reaplicar).

La hipótesis HEIC queda **descartada**: Aura Studio ya convierte toda foto a JPEG baseline de ≤320/640 px antes de copiarla, y solo copia lo ya convertido.

---

## 1. Fase 0 — Hallazgos

### 1.1 — Qué archivos llegan al dispositivo y con qué extensión

No hay iPod montado en esta sesión (ningún volumen bajo `/Volumes` con `.rockbox/`), así que se inspeccionó **lo que Studio produce y lo que copia**, que es determinista:

- `LibraryViewModel.process` (`studio/AuraStudio/Sources/AuraStudio/ViewModels/LibraryViewModel.swift:274-288`): toda foto pasa por `ImageResizer.resizeToLCDOptimal()` y su `preparedURL` es siempre `.preparados/<nombre>.jpg`.
- `ImageResizer` (`Services/ImageResizer.swift:33-60`): ImageIO `CGImageSourceCreateThumbnailAtIndex` con `kCGImageSourceThumbnailMaxPixelSize` = 320 (u 640 en "Versión HD"), `CreateThumbnailWithTransform` (orientación EXIF horneada), salida `UTType.jpeg` calidad 0.85. Lee HEIC/PNG/GIF/TIFF/BMP de origen sin problema (`LibraryItem.swift:15` acepta `jpg jpeg png gif bmp heic tiff`).
- `LibrarySync.sync` (`Services/LibrarySync.swift:157-160` y `204-208`): **solo copia items con `preparedURL`** (`guard let prepared = item.preparedURL`), al destino `Photos/<nombre>.jpg` (`LibrarySync.swift:523`), plano, en la raíz del volumen.
- Verificado sobre la biblioteca real de esta Mac (`~/Documents/Aura Library/.preparados/`): 13 JPEG, todos `JFIF… baseline, precision 8, components 3`, 200–320 px de lado mayor, 17–58 KB. Ni un HEIC ni un progresivo.

**Conclusión 0.1**: al dispositivo llegan únicamente `Photos/*.jpg` baseline ≤ 640 px. La extensión y el formato son correctos; el síntoma no viene de aquí.

Hallazgo lateral (Studio, no bloqueante, para la pasada posterior): `biblioteca.json` tiene **701 fotos, 688 en estado `queued`** — son `cover.jpg` de carpetas de música importadas como fotos — y hay **colisión de nombres**: `Imágenes/Fotos/cover.jpg` e `Imágenes/Imágenes/cover.jpg` ambos preparan a `.preparados/cover.jpg` y sincronizan a `Photos/cover.jpg` (uno pisa al otro). Ver §9.

### 1.2 — Dónde busca el firmware y dónde escribe Studio

| Lado | Ruta | Referencia |
|---|---|---|
| Firmware | `/Photos` (raíz del volumen), plano, sin recursión | `apps/aura/aura_photos.c:44` (`PHOTOS_DIR`), `:99-122` (`ensure_photo_list`) |
| Studio | `<volumen>/Photos/<nombre>.jpg`, plano | `LibrarySync.swift:523` |
| Contrato | fila `Music/`, `Photos/`, `Videos/` — "Studio (sync) → Firmware" | `CONTRATO-firmware-studio.md` §D |

**Coinciden.** No hay desajuste de rutas.

### 1.3 — ¿Existe la pantalla de Fotos o es un stub?

Existe y es completa (no stub), pero es **inalcanzable**:

- Módulo real: `apps/aura/aura_photos.c` (270 líneas): lista `.jpg/.jpeg/.bmp/.png/.gif` (`:73-81`), estado vacío con StatusBar (`:141-150`), lista con `aura_widgets_draw_list` (`:164`), visor a pantalla completa con `read_jpeg_file()`/`read_bmp_file()` del core en `FORMAT_NATIVE|FORMAT_RESIZE|FORMAT_KEEP_ASPECT` (`:197-226`), navegación LEFT/RIGHT entre fotos y MENU/SELECT para salir (`:251-270`). Buffer estático de 240 KB (`s_view_scratch`, `:50,62`).
- Navegación: raíz → `AURA_SCREEN_PHOTOS` (menú SPLIT con una fila "Todas las fotos", `aura_screens.c:311-313`) → `aura_nav_push(AURA_SCREEN_PHOTOS_ALL)` (`handle_nav_list`, `aura_screens.c:~4707`).
- **El bug** (`aura_screens.c:4528-4623`, `aura_screens_draw`): `AURA_SCREEN_PHOTOS` lo intercepta primero la rama de menús (`draw_nav_list`, `:4537-4541`); más abajo, `else if (screen == AURA_SCREEN_PHOTOS) aura_photos_draw(nav);` (`:4613-4614`) es **código muerto**, y `AURA_SCREEN_PHOTOS_ALL` no aparece en ninguna rama → cae a `draw_empty_state(screen)` (`:4622`), cuyo `switch` tampoco lo contempla (`:4033-4046`) → `AURA_STR_NOTHING_PLAYING` = "Nada sonando". Los botones sí están cableados a `AURA_SCREEN_PHOTOS_ALL` (`:4979-4980`), por eso SELECT sobre una foto empuja el visor… que sí se dibuja (`:4615-4616`) — pero nadie llega ahí porque la lista nunca se ve.
- **Idéntico para Video**: `:4617-4618` dibuja `AURA_SCREEN_VIDEOS` (interceptado por el menú) y `AURA_SCREEN_VIDEOS_ALL` no tiene rama; botones en `:4983-4984`.
- Historial: introducido en `b417e069` (2026-08-10, cuando Videos/Fotos ganaron submenú propio, `18ef843`); corregido en `246f99f` (D-251); revertido en `dab2c76` (D-253, `DECISIONS-ARCHIVE.md:3005`).
- Documentación desactualizada: `docs/aura-design-system/sistema/03-arbol-de-menus.md:63,66` marca "Todos los videos ✅ (visor real)" y "Todas las fotos ✅ (visor real)".

Dos defectos secundarios que el mismo síntoma tapa (con archivo:línea):

1. **Descripción del panel derecho desacoplada del disco**: la fila "Todas las fotos" muestra "Sin fotos todavía" según `sync_summary.cfg → photo_count` (`aura_screens.c:1716-1724`, `photos_row_empty_description` → `cached_manifest()`), no según `/Photos`. En el simulador (sin `sync_summary.cfg`) dice "Sin fotos todavía" con 3 fotos en el disco (captura `01`). En hardware, tras un sync de Studio, `photo_count` cuenta solo items con `preparedURL` — coincide *normalmente*, pero un usuario que copie a mano o un sync interrumpido lo desalinean.
2. **La lista se escanea una sola vez por arranque**: `s_photo_count = -1` se llena en `ensure_photo_list()` y nunca se invalida (`aura_photos.c:59,104`); igual `aura_video.c:57,88`. Nadie escucha `SYS_FS_CHANGED` en `apps/aura/` (grep vacío). Secuencia real: abrir Fotos (vacío, cache = 0) → conectar USB → sincronizar → desconectar → Fotos sigue "Sin fotos todavía" hasta reiniciar.

Límites que hoy tiene el módulo y que el contrato debe conocer: `MAX_PHOTOS 200` (`:45`), `PHOTO_NAME_LEN 64` (`:46`, un nombre de ≥64 bytes se trunca con `strlcpy` y el archivo ya no abre → "Formato no soportado"), sin ordenamiento (orden de `readdir`, es decir, orden físico de la FAT).

### 1.4 — ¿Decodifica el hardware?

**No hace falta el plugin `imageviewer` para responderlo**: el visor de Aura no usa el plugin — usa `apps/recorder/jpeg_load.c` y `apps/recorder/bmp.c`, **los mismos decoders del core que ya decodifican las carátulas** en Cover Flow, la lista de Álbumes, CoverDrift y Ahora suena (`aura_albumart.c:395-397,544`), que el dueño ya ha visto funcionar en el iPod físico. Correr el plugin no es viable desde la UI de Aura (no hay navegador de archivos, y mostraría cromo de Rockbox); en el simulador tampoco hay forma de invocarlo sin código.

Lo que sí se hizo, en el simulador (`firmware/tools/apple2026_sim_shot.sh`, capturas en `docs/screenshots/image-viewer/`):

| Captura | Estado del árbol | Botones | Resultado |
|---|---|---|---|
| `00-fotos-all-antes.png` | HEAD limpio, 3 JPEG en `simdisk/Photos/` | `SCROLL_FWD,SCROLL_FWD,SELECT,SELECT` | **"Nada sonando"** — bug reproducido |
| `01-fotos-menu-antes.png` | HEAD limpio | `…,SELECT` | Menú Fotos, panel derecho "Sin fotos todavía" con 3 fotos en disco (defecto secundario 1) |
| `02-fotos-lista-diag.png` | parche temporal de 1 línea (`AURA_SCREEN_PHOTOS` → `AURA_SCREEN_PHOTOS_ALL` en `:4613`), rebuild sim | idem 00 | Lista "Fotos" con las 3 fotos, StatusBar (full), sin extensión |
| `03-fotos-visor-diag.png` | idem | `…,SELECT` | JPEG 640×480 (baseline, hecho de una carátula) a pantalla completa 320×240 — decodifica |
| `04-fotos-visor-640-diag.png` | idem | `…,SCROLL_FWD,SCROLL_FWD,SELECT` | JPEG 320×278 → 276×240 centrado con franjas laterales (`FORMAT_KEEP_ASPECT` correcto, medido por píxel) |

El parche temporal se revirtió con `git checkout` y el simulador se recompiló a HEAD; el árbol quedó limpio (solo quedan las capturas nuevas, sin trackear).

**Hardware**: sin iPod en esta sesión. Queda pendiente de la Fase 2 (§8), con la evidencia indirecta de que el decoder es el mismo que ya corre en el dispositivo para carátulas.

Fixtures usados: `Portada A.jpg` (317×320) y `Portada B.jpg` (320×278) son salidas reales de Studio (`~/Documents/Aura Library/.preparados/`); `Paisaje 640.jpg` (640×480) se generó con `sips` a partir de una carátula de 500×500. Están en `firmware/build-sim/simdisk/Photos/` (ignorado por git).

---

## 2. Estrategia de reutilización de decodificadores (1.1 / 1.2)

### 2.1 — Inventario en este fork

| Módulo | Ubicación | Modelo | Estado para Aura |
|---|---|---|---|
| **JPEG baseline** | `apps/recorder/jpeg_load.c` (+ `jpeg_idct_arm.S`) | **Core**, función `read_jpeg_file()` linkeable, IDCT escalado 1/8·1/4·1/2·1/1 desde coeficientes, remuestreo por líneas (`resize_on_load`), memoria acotada por fila de MCU | **Ya en uso** por `aura_photos.c` y `aura_albumart.c`. Nada que portar. Solo Huffman baseline (SOF0/SOF1 no; progresivo → `-4`, `jpeg_load.c:1055-1070`; aritmético → `-6`) |
| **BMP** | `apps/recorder/bmp.c` | **Core**, `read_bmp_file()`, 1/4/8/16/24/32 bpp sin compresión (BI_BITFIELDS sí, RLE no), remuestreo por líneas | **Ya en uso** |
| JPEG (plugin) | `apps/plugins/imageviewer/jpeg/` (`jpeg_decoder.c` 1526 l., `yuv2rgb.c`) | Plugin (`rb->`), decodifica **la imagen completa** a memoria y luego escala; requiere `plugin_get_audio_buffer()` (**detiene la música**) | No aporta nada sobre el core para nuestro caso |
| JPEG progresivo | `apps/plugins/imageviewer/jpegp/` (`jpeg81.c` 1000 l., `mempool.c`) | Plugin; imagen completa en memoria + pool | Portable en teoría (~1.3k líneas + glue), **no vale la pena**: Studio nunca emite progresivo |
| PNG | `apps/plugins/imageviewer/png/` (LodePNG: `png_decoder.c` 2192 l., `tinflate.c`, `tinfzlib.c`, `crc32.c`) | Plugin; carga **el archivo entero** en memoria + decodifica **la imagen completa** en RGB (`png.c:135-170`) | Portable (~2.9k líneas + adaptar `rb->` a core), pero exige memoria proporcional a W×H — ver §3 |
| GIF | `apps/plugins/imageviewer/gif/` (giflib recortada, ~2.2k l.) | Plugin; **todos los cuadros** decodificados en memoria (`gif.c:168`: `native_img_size × frames_count`) | Igual que PNG pero peor (animados) |
| PPM | `apps/plugins/imageviewer/ppm/` | Plugin | Irrelevante |
| **Chrome del plugin** | `imageviewer.c` (1235 l.) | Menús de Rockbox, `rb->splash`, ajustes propios, roba el buffer de audio | **Prohibido** mostrarlo (regla always-on: cero cromo de Rockbox) |

### 2.2 — Recomendación: integración nativa sobre el core, sin lanzar el plugin, sin copiar módulos

- **Lanzar `imageviewer.rock` con `plugin_load()` (como se hace con `mpegplayer` para video, D-029): descartado.** A diferencia de `mpegplayer` (que en el flujo de Aura no muestra menús propios), `imageviewer` sí dibuja su UI (splash "Loading…", menú de contexto, mensajes en inglés, `rb->splash` de errores) y además llama a `plugin_get_audio_buffer()`, cortando la música que esté sonando — dos violaciones de reglas duras.
- **Extraer los decoders del plugin a `apps/aura/`: innecesario para JPEG/BMP** (ya son core, cero deuda de sincronización con upstream) **y no recomendado para PNG/GIF** (memoria no acotada, ver §3, y Studio ya los convierte a JPEG). Si algún día se quiere PNG nativo, el camino es portar `png_decoder.c` + `tinflate` a `apps/aura/` con cabecera GPL, entrada en `MODIFICATIONS.md`, y un tope de resolución con buffer propio (o `core_alloc` de buflib, con la complejidad de convivir con la reproducción). Se deja como decisión abierta (Q4), recomendación: **no ahora**.
- **Costo de mantener el fork sincronizado**: nulo para esta pieza — `jpeg_load.c`/`bmp.c` no se tocan (salvo si se decide exponer una sonda de dimensiones, ver §3.3, que sería un cambio de ~10 líneas registrado en `MODIFICATIONS.md`; la alternativa sin tocar upstream es leer el SOF0 del archivo desde `aura_photos.c`, ~40 líneas, y es la que se recomienda).
- **Lo que sí es trabajo nuevo** vive todo en `apps/aura/aura_photos.c` (+ `aura_screens.c` para el despacho): dispatch, rescan, orden, miniaturas con caché `.pfraw` (mismo pipeline que `aura_albumart.c`), sonda de tamaño para degradar, textos, transición entre fotos.

---

## 3. Matriz de formatos (1.3)

Presupuesto de referencia: `s_view_scratch` de 240 KB (ya asignado), pantalla 320×240 RGB565 = 153,600 B para el bitmap final.

| Formato | Veredicto | Límite | Qué pasa fuera del límite | Justificación |
|---|---|---|---|---|
| **JPEG baseline** (SOF0, Huffman, 8 bits, 3 componentes YCbCr con submuestreo 4:4:4/4:2:2/4:2:0, o 1 componente gris — `jpeg_load.c:1033-1051`) | **Soportado** | Hasta **12 megapíxeles** o **4096 px de lado** (el que se cruce primero); tope duro del decoder: 32767 px de lado (`jpeg_load.c:2143`) | Mensaje "Foto demasiado grande" (nuevo texto), sin decodificar. Estado bloqueante ausente: la sonda de dimensiones lee solo cabeceras | Memoria **constante** respecto al tamaño de la fuente (§4): el decoder escala en la IDCT (1/8 mínimo) y remuestrea por líneas; el único costo que crece es **tiempo de CPU** (Huffman de todos los coeficientes). El tope de 12 MP es por tiempo (estimado 5–10 s en el S5L8702 a 1/8; a medir en Fase 2 — Q6), no por RAM |
| JPEG progresivo / aritmético / 12 bits / CMYK | **No soportado** | — | "Formato no soportado" (texto existente); se listan (extensión `.jpg`) | El core devuelve `-4`/`-6` sin tocar memoria. Studio nunca los emite; solo llegan copiados a mano |
| **BMP** (sin compresión, 1–32 bpp) | **Soportado con límite** | Mismo tope de 12 MP / 4096 px | "Foto demasiado grande" | Memoria acotada (`BM_SCALED_SIZE`: final + ~9×ancho×4 B), pero un BMP grande son decenas de MB de lectura de disco (12 MP × 3 B = 36 MB) → segundos girando el disco. RLE → `read_bmp_file` falla → "Formato no soportado" |
| **PNG** | **No soportado** (se lista, muestra "Formato no soportado" — statu quo, D-028) | — | — | No hay decoder en el core; el del plugin necesita **archivo + W×H×3 en RAM** (un PNG 1024×1024 ≈ 3 MB + archivo). Con Studio convirtiendo siempre a JPEG, portar 2.9k líneas + inventar un tope y un buffer no aporta nada al usuario final. Reabrir solo si el dueño quiere que fotos copiadas a mano en PNG se vean (Q4) |
| **GIF** (estático o animado) | **No soportado** (se lista con "Formato no soportado" — statu quo) | — | — | Solo en plugin; animados = **todos los cuadros en RAM** (`gif.c:168`). Ni siquiera el original de 2008 reproducía GIF animados. Studio convierte el primer cuadro a JPEG |
| HEIC / HEIF | **No soportado — ni listado** | — | No aparece en la lista (statu quo) | HEVC intra: imposible en el S5L8702 por la misma razón que H.264. Studio lo convierte a JPEG (ImageIO lo lee nativo) |
| WebP | **No soportado — ni listado** | — | — | VP8: sin decoder en Rockbox; no vale la pena portar libwebp. Studio lo convierte |
| TIFF, PPM, otros | **No soportado — ni listado** | — | — | Studio convierte TIFF; PPM es curiosidad de plugin |

Regla de degradación (Principio 7, español): **nunca colgarse, nunca reiniciar** — la sonda de dimensiones decide antes de reservar/decodificar; un fallo del decoder (`ret <= 0`) muestra "Formato no soportado"; el resto de la lista y la navegación siguen operativos.

---

## 4. Presupuesto de memoria (1.4)

Medido sobre `firmware/build-ipod6g/rockbox.map` y `arm-elf-eabi-size` (build actual):

| Región | Tamaño | Notas |
|---|---|---|
| DRAM total útil | 0x03CFC000 ≈ **61 MB** (de 64 MB físicos) | `DRAM 0x08000000` |
| Imagen del firmware (text+data+bss) | **7.0 MB** (`_end` = 0x08701AC8; bss = 6,138,716 B) | Aura ya suma ~5 MB de estáticos: `s_slots` 1.6 MB (caché de carátulas), `s_decode_scratch`+`s_transpose_scratch` 2×400 KB, `s_view_scratch` 240 KB, framebuffers de transición 5×150 KB, CoverDrift 2×200 KB… |
| Buffer de audio (buflib: playback, tagcache…) | **≈ 54 MB** (0x08701AC8 → 0x0BCFC000) | Intocable por el visor: la música sigue sonando mientras se ven fotos |
| Codec buffer / plugin buffer | 1 MB / 2 MB | Fuera del área de audio |

**Costo del visor a pantalla completa (JPEG, peor caso real)** — con `read_jpeg_file()` en `FORMAT_RESIZE`, tomado de `jpeg_load.c:2216-2260`:

```
bitmap final 320×240×2 ...................... 153,600 B
struct jpeg (tablas Huffman/cuant. + buf 16 B) ~10 KB (JPEG_DECODE_OVERHEAD reserva 38 KB + struct)
decode buffer = x_mbl << h_scale[1] << v_scale[1] × 4 B
  · fuente 32767 px de ancho a 1/8 (2048 MCU) ..... 32 KB   (peor caso absoluto)
  · fuente 640 px a 1/2 (40 MCU × 4 × 4 × 4) ......  2.5 KB
líneas de remuestreo 3 × 320 × 4 .................  3.8 KB
TOTAL peor caso ................................. ≈ 200 KB  < 240 KB (s_view_scratch)
```

Además el decoder hace `yield()` al terminar cada fila de MCU (`jpeg_load.c:1946`), así que la reproducción de música no se corta durante una decodificación larga.

Es decir: **el buffer intermedio no existe como imagen completa** — el decoder produce una fila de MCU a la escala elegida y la remuestrea al vuelo. La resolución de la fuente no cambia la RAM, solo el tiempo. Por eso el "reescalado durante la decodificación" que pide el brief **ya es lo que hace el core**; no hay etapa "después".

**Miniaturas (lista)**: 48×48×2 = 4.6 KB por miniatura; ventana visible de 4 filas + 2 de margen = 28 KB en RAM (estático), más caché `.pfraw` en disco (mismo formato/pipeline que `aura_albumart.c`, `<clave>-48.pfraw`), de modo que una foto se decodifica una sola vez en la vida del dispositivo. Decodificar una miniatura desde un JPEG de 320 px cuesta una IDCT a 1/4 (80 px) + remuestreo → decenas de ms.

**Resolución máxima que "cabe"**: cualquiera hasta el tope de 12 MP definido por tiempo (§3); el bitmap final nunca excede 320×240. Si se quisiera el modo "pantalla completa recortada con paneo" del original (Q3), el bitmap final sería como mucho 480×360 (fuente 640 px) = 345 KB → habría que crecer `s_view_scratch` a 512 KB. Se contempla, no se implementa ahora.

**Regla de oro**: ninguna reserva nueva del lado del buffer de audio (`core_alloc`) — todo estático y acotado, como hoy.

---

## 5. Diseño de UI (1.5)

Referencias: `docs/design/Reglas de comportamiento - iPod Classic original (2008).md` §1 (taxonomía), §2.3, §3.4 (Fotos: *"Dos modos: ajustada al marco (con bordes negros…) o pantalla completa (recortada y centrada, con paneo…), y nunca zoom"*), §4.3; `docs/aura-design-system/transiciones/00-vocabulario.md`; `componentes/status-bar.md` ("Regla dura: (split) ⇔ LeftPanel"); memoria del proyecto: **la clasificación FULL/SPLIT de cada pantalla está cerrada** (D-253) — este plan **no cambia ninguna**.

### 5.1 — Taxonomía

| Pantalla | Tipo | Justificación |
|---|---|---|
| Fotos (menú, `AURA_SCREEN_PHOTOS`) | `SPLIT` — **sin cambios** | Nivel 2; fila "Todas las fotos" + `SelectionSummary` (ícono de la categoría, color naranja de Fotos, D-250) |
| Todas las fotos (`AURA_SCREEN_PHOTOS_ALL`) | `LISTA-COMPLETA` (FULL) — **sin cambios** | Nivel 3, regla de profundidad por defecto; hoy ya se declara FULL (`screen_uses_split_layout` no la incluye). Entrada desde el menú = `SPLIT → FULL-COLD` (§2.3 del original): push de ancho completo + StatusBar `Push-and-Drop`, **ya lo hace el despacho centralizado** (`aura_screens.c:5107-5170`) sin código nuevo |
| Visor (`AURA_SCREEN_PHOTO_VIEWER`) | `FULL-COLD` | Pantalla nueva sin herencia visual: la lista es FULL, no hay panel derecho del que "cargar" un elemento (por eso no es `FULL-CARRY`). Entra con el mismo push de ancho completo (T3) que ya aplica el despacho centralizado |

### 5.2 — Lista "Todas las fotos"

- Renderizador dedicado tipo `draw_album_list()` (`aura_screens.c:4174-4285`, la **única lista de contenido con miniaturas reales**, patrón ya aprobado): filas de 54 px, miniatura 48×48 a la izquierda, nombre sin extensión, pastilla de selección, `ScrollIndicator`, StatusBar (full) con título "Fotos". No se inventa un patrón nuevo (rejilla de miniaturas del original → Q1).
- Miniaturas: **las genera el firmware**, bajo demanda para la ventana visible, con caché `.pfraw` en disco (§4). No es trabajo de Studio (Q2), porque las fuentes ya son JPEG de ≤ 640 px: la decodificación es barata y el caché la hace única. Placeholder mientras se decodifica: tile sólido con el color de categoría (mismo criterio que la carátula Default de Álbumes).
- Orden: **por nombre, natural e insensible a mayúsculas** (`strnatcasecmp`, ya en `string-extra.h`) en vez del orden físico de la FAT actual. Alternativa por fecha → Q5.
- Rescan **cada vez que se entra a la pantalla** desde el menú (no solo al arrancar) — `opendir` de unos cientos de entradas es despreciable; corrige el defecto secundario 2. Mismo tratamiento para Video.
- Límites: `MAX_PHOTOS` 200 → **500** y `PHOTO_NAME_LEN` 64 → **96**; ambos estáticos (500 × (96+96+1) ≈ 97 KB). Si hay más archivos, se listan los primeros 500 en orden y **se dice**: última fila inerte "…y N fotos más" (texto nuevo) — nunca truncar en silencio.
- Estado vacío (**relevante para el síntoma**): se conserva el existente — StatusBar (full) "Fotos" + "Sin fotos todavía" centrado (`aura_photos.c:141-150`), en `--color-text-secondary`. Se agrega debajo, en el mismo estilo, una línea de ayuda **honesta y sin jerga**: "Sincroniza fotos desde Aura Studio". El panel derecho del menú (SPLIT) pasa a leer el conteo real de `/Photos` (`aura_photos_count()` nuevo) en vez de `sync_summary.cfg` — corrige el defecto secundario 1; `sync_summary.cfg` sigue alimentando "Acerca de".

### 5.3 — Visor

- Imagen ajustada al marco ("fit", con franjas del color de fondo del tema `--color-bg-base`, no negro fijo — Principio "sin RGB hardcodeado", como hoy `a26_shell_clear_screen()`), centrada. Modo "recortado con paneo" → Q3.
- **Sin StatusBar**: la foto ocupa los 320×240 (§3.4 del original: la foto es la pantalla). No contradice la regla dura de `status-bar.md` (que ata la *forma* de la barra al `LeftPanel`, no exige barra en todo estado); sí es un estado nuevo que hay que documentar en `componentes/` (Q7: aceptar "sin barra" para esta pantalla).
- Navegación: **rueda** (`SCROLL_FWD` = siguiente, `SCROLL_BACK` = anterior — hoy solo LEFT/RIGHT, que se conservan como atajo), `MENU` regresa a la lista **conservando la selección** en la foto que se estaba viendo (hoy la lista vuelve a la selección de entrada; se sincroniza `aura_nav_set_selection`).
- Transición entre fotos: **`Fade-Slide`, variante de región** (`aura_transition_fade_slide_region()`, D-283 — la única del vocabulario para cambio full→full sin cambio de layout): región = pantalla completa; siguiente entra desde la derecha, anterior desde la izquierda; mismos tokens `push_and_drop.*`. Respeta `aura_settings.animation_mode` (modo reducido → sin animación, corte directo) y **`lcd_active()`** (pantalla apagada → sin animar, se salta al estado final). Alternativa `Scroll-Slide` (vertical, "atada a la rueda") → Q8; se recomienda horizontal por fidelidad al original.
- Indicador de carga: si la sonda dice que la fuente es > 640 px (única situación en la que la decodificación es perceptible), antes de decodificar se dibuja la pantalla vacía con "Cargando…" centrado (texto nuevo, `--color-text-secondary`, tipografía body) y `lcd_update()`; después la foto reemplaza. **Sin spinner** — es una animación más para gatear con `lcd_active()` que no aporta sobre un texto estático de < 1 s típico. Las fotos de Studio (≤ 640) no muestran el indicador.
- Errores: "Formato no soportado" (existente) y "Foto demasiado grande" (nuevo), centrados, en el mismo estilo; la navegación a la siguiente foto sigue funcionando.
- Retroiluminación: sin cambios (timeout normal). `backlight_ignore_timeout()` solo tendría sentido en un modo presentación (Q9, no ahora).

### 5.4 — Textos nuevos (`aura_lang.c`, al final de ambos idiomas)

| id | es-MX | en |
|---|---|---|
| `AURA_STR_PHOTO_TOO_LARGE` | Foto demasiado grande | Photo too large |
| `AURA_STR_LOADING` | Cargando… | Loading… |
| `AURA_STR_EMPTY_PHOTOS_HINT` | Sincroniza fotos desde Aura Studio | Sync photos from Aura Studio |
| `AURA_STR_LIST_MORE_FMT` | …y %d fotos más | …and %d more photos |

### 5.5 — Documentación viva a tocar en la misma pasada

- `docs/aura-design-system/componentes/photo-viewer.md` (nuevo): estados `(list)` y `(viewer)`, geometría, transiciones, textos.
- `docs/aura-design-system/sistema/03-arbol-de-menus.md:63,66`: corregir "✅ (visor real)" mientras siga roto; marcar ✅ real al cerrar la Fase 2.
- `docs/aura-design-system/transiciones/00-vocabulario.md`: añadir el visor a "Usado por" de `Fade-Slide` (variante de región).

---

## 6. 🔗 Contrato para Aura Studio — "Photos/" (v1 propuesta)

Autocontenido a propósito: Studio lo puede consumir sin leer código ni rutas del firmware. Al aprobarse, este texto se incorpora a `CONTRATO-firmware-studio.md` §D (fila `Photos/`, ampliada) en **ambos** repos, y la fila del contrato pasa a citar D-291 (firmware) y el ST-NNN de la pasada de Studio.

**6.1 Ruta y estructura**
- Directorio: **`/Photos/` en la raíz del volumen del iPod**, **plano**. El firmware **no recorre subdirectorios** ni sigue nada fuera de esa carpeta. Cualquier subcarpeta que Studio cree ahí (p. ej. una futura `.thumbs/`) es ignorada — no rompe, pero tampoco se ve.
- Studio es el único escritor. El firmware solo lee, y además escribe **su propio caché de miniaturas fuera de `/Photos/`** (bajo `/.rockbox/aura/`), que Studio no debe tocar ni necesita conocer.
- Borrado diferencial: si Studio elimina un archivo de `/Photos/`, el firmware lo deja de listar en la siguiente entrada a la pantalla; su miniatura cacheada expira sola (clave = nombre + tamaño + fecha de modificación).

**6.2 Formato del archivo**
- **JPEG baseline** (SOF0), codificación Huffman, **8 bits por componente**, **3 componentes YCbCr** (submuestreo 4:2:0 o 4:4:4, ambos OK), sin capa alfa, extensión **`.jpg`** (también se acepta `.jpeg`).
- **Prohibido**: JPEG progresivo, aritmético, 12 bits, CMYK (4 componentes), submuestreo 4:1:1; PNG, GIF, HEIC/HEIF, WebP, TIFF, BMP (BMP se decodifica pero no tiene sentido: 10–20× el peso sin ganancia visible). Todo eso lo convierte Studio **antes** de copiar — como ya hace hoy con ImageIO (`ImageResizer`), cuya salida cumple este contrato tal cual (verificado en las 13 preparadas de esta Mac).
- Espacio de color: sRGB. El dispositivo muestra RGB565 (16 bits); no incrusta ni lee perfiles ICC. Metadatos EXIF/XMP: se ignoran (se pueden dejar; recortarlos ahorra unos KB por foto).
- Sin límite de peso de archivo, pero con las resoluciones de abajo el rango esperado es 15–120 KB.

**6.3 Resolución y orientación**
- **Lado mayor ≤ 640 px** (opción "Versión HD" de Studio) o **≤ 320 px** (opción "Optimizar espacio"). **Nunca escalar hacia arriba** una foto más chica (hoy correcto: una fuente de 200×200 queda de 200×200).
- Recomendación de precisión: 640 es el valor ideal, no un tope arbitrario — 640×480 se decodifica a 320×240 con la IDCT a 1/2 **sin remuestreo posterior** (la salida más nítida posible del decoder), y 320×240 se decodifica sin escalar. Otros lados mayores funcionan pero pasan por remuestreo.
- **Orientación EXIF horneada** en los píxeles (rotar/voltear al exportar, como ya hace `kCGImageSourceCreateThumbnailWithTransform`) — el firmware **no lee la etiqueta Orientation**.
- Aspecto: se preserva; el firmware ajusta al marco 320×240 con franjas del color de fondo. Recortar al 4:3 es opcional del lado de Studio (Q3 decide si el firmware ofrecerá recorte con paneo).
- Tope duro del firmware para archivos que no vengan de Studio: 12 megapíxeles o 4096 px de lado → "Foto demasiado grande"; progresivo → "Formato no soportado". Studio nunca debe acercarse a esos límites.

**6.4 Nombre de archivo**
- UTF-8, **≤ 95 bytes incluyendo `.jpg`** (límite nuevo del firmware; hoy 63). Recomendado ≤ 60 caracteres.
- El firmware muestra **el nombre sin extensión** como título de la foto (Principio 7: sin jerga), y **ordena por nombre** (natural, insensible a mayúsculas: "Foto 2" antes que "Foto 10"). Studio controla el orden a través del nombre; se recomienda conservar el nombre original del usuario (`IMG_1234` es lo que el usuario reconoce), no un prefijo técnico.
- **Los nombres deben ser únicos dentro de `/Photos/`** — hoy dos fuentes homónimas de carpetas distintas (`…/Fotos/cover.jpg` y `…/Imágenes/cover.jpg`) colisionan en `.preparados/cover.jpg` y en `Photos/cover.jpg` (§9). Studio debe desambiguar (sufijo ` 2`, ` 3`, como ya hace `copyIntoLibrary` para la biblioteca local).
- Caracteres: cualquier UTF-8 válido en FAT32 (Rockbox maneja nombres largos); evitar `\ / : * ? " < > |` — Studio ya tiene `PathSanitizer`.

**6.5 Cantidad**
- El firmware lista hasta **500** fotos (hoy 200) ordenadas por nombre y anuncia "…y N fotos más" si hay más. Studio no necesita limitar la copia, pero conviene avisar al usuario cuando la biblioteca de fotos supere 500 (Q10).

**6.6 `sync_summary.cfg`**
- `photo_count`/`photo_bytes` y las claves por colección se siguen escribiendo igual (las lee "Acerca de"). **Dejan de decidir** el estado vacío de Fotos: ese sale del contenido real de `/Photos/`. Sin cambio de formato; sin `contract_version` nuevo por esto.

**6.7 Lo que Studio NO tiene que hacer**
- No generar miniaturas ni sidecars: el firmware las produce y cachea.
- No escribir nada en `/.rockbox/aura/` por las fotos.
- No convertir a BMP "porque es más seguro" — no lo es (peso), y JPEG es el formato de primera clase.

**6.8 Ajuste de texto en Studio (pasada posterior, no bloqueante)**
- `PhotoSettingsView` describe la opción HD como *"se ve un poco más nítida al hacer zoom en el visor"* — el visor **no tiene zoom** (ni lo tendrá: §3.4 del original, "nunca zoom"). Texto propuesto: *"Versión HD (640px): el visor la reduce a la pantalla con más precisión; útil si algún día se activa el modo de pantalla completa recortada"* — o simplemente quitar la mención al zoom.

---

## 7. Preguntas abiertas (con recomendación razonada)

| # | Pregunta | Recomendación |
|---|---|---|
| **Q1** | ¿Lista con miniaturas (patrón Álbumes) o **rejilla** de miniaturas como el iPod original? | **Lista con miniaturas ahora.** Reusa un patrón aprobado (`draw_album_list`) y el caché existente; la rejilla es un componente nuevo (selección 2D con rueda, geometría, `ScrollIndicator`) que merece su propia decisión de diseño con el dueño. Si el dueño quiere la rejilla, este plan la deja preparada (las miniaturas y el caché son los mismos) |
| **Q2** | ¿Miniaturas del firmware o de Studio? | **Firmware** (§5.2): fuentes ya chicas, caché `.pfraw` único, cero contrato extra. Studio no hace nada |
| **Q3** | ¿Modo "pantalla completa recortada con paneo" del original además del "ajustado al marco"? | **No en esta fase.** Requiere buffer de 512 KB, paneo con la rueda, un toggle. Fit-only cubre el 100 % de las fotos de Studio. Documentar como pendiente en `photo-viewer.md` |
| **Q4** | ¿Portar PNG (LodePNG) al core para fotos copiadas a mano? | **No.** Studio convierte; el costo (2.9k líneas GPL a mantener + memoria W×H×3 + tope propio) no paga. Mantener "se lista, no se abre" |
| **Q5** | ¿Orden por nombre o por fecha (mtime / EXIF)? | **Nombre natural.** Es determinista, barato (sin abrir archivos), y Studio lo controla. Fecha EXIF obligaría a leer cada archivo al listar |
| **Q6** | Tope de 12 MP / 4096 px: ¿medirlo en hardware y ajustar? | **Sí, en Fase 2**: cronometrar 3 MP, 8 MP y 12 MP en el iPod; si 12 MP tarda > 8 s, bajar el tope a 8 MP. El tope solo afecta archivos ajenos a Studio |
| **Q7** | Visor **sin StatusBar** (foto a sangre) — ¿lo acepta el dueño como estado documentado? | **Sí, recomendado**: es lo que hace el original y lo que hace hoy el código; se documenta explícitamente en `componentes/photo-viewer.md` para que no parezca omisión |
| **Q8** | Transición entre fotos: `Fade-Slide` horizontal (región) vs `Scroll-Slide` vertical | **`Fade-Slide` horizontal**: la foto anterior/siguiente se lee como "página", igual que "Acerca de"; el original desliza horizontal. `Scroll-Slide` es para textos de título |
| **Q9** | ¿Modo presentación (tiempo por diapositiva, repetir, aleatorio — Ajustes de Fotos del original, §4.3)? | **No en esta fase.** Fuera del síntoma; requiere submenú de ajustes + retroiluminación forzada. Registrar como rama pendiente en el árbol |
| **Q10** | ¿Debe Studio avisar por encima de 500 fotos, o subimos más el tope? | **500 + aviso en el firmware** ("…y N fotos más"). 1000 costaría 200 KB estáticos por algo que hoy nadie tiene; subir después es trivial |
| **Q11** | ¿Corregir `AURA_SCREEN_VIDEOS_ALL` (mismo bug) en esta misma pasada? | **Sí**, es una línea en el mismo `switch` y el mismo rescan; se documenta en la misma D-291 como corrección hermana. No se toca nada más de Video |
| **Q12** | ¿La sonda de dimensiones se hace leyendo el SOF0 en `aura_photos.c` (sin tocar upstream) o exponiendo algo en `jpeg_load.c`? | **En `aura_photos.c`** (~40 líneas: buscar marcador `FFC0/FFC1/FFC2`, leer alto/ancho; `FFC2` = progresivo → "no soportado" antes de decodificar). Cero entradas en `MODIFICATIONS.md` |

---

## 8. Fase 2 — Ejecución (solo tras aprobación)

Commits atómicos, sin push, build ARM (`build-ipod6g`) y sim (`build-sim`) limpios tras cada uno, `make -C firmware/rockbox/apps/aura/test test` sin regresiones, textos en español, sin RGB hardcodeado (`a26_color(...)`), `lcd_active()` en toda animación.

| # | Commit | Contenido | Aceptación |
|---|---|---|---|
| 1 | D-291 (1/n): despacho de `PHOTOS_ALL`/`VIDEOS_ALL` + rescan | `aura_screens_draw`: ramas `AURA_SCREEN_PHOTOS_ALL → aura_photos_draw`, `AURA_SCREEN_VIDEOS_ALL → aura_video_draw`; eliminar las ramas muertas de `PHOTOS`/`VIDEOS`; `draw_empty_state` contempla `*_ALL`; `aura_photos_invalidate()`/`aura_video_invalidate()` llamados al empujar la pantalla; `photos_row_empty_description`/`video_row_empty_description` leen el conteo real | Capturas sim: lista con 3 fotos; panel derecho "Todas las fotos" sin "Sin fotos todavía" con archivos presentes; Video igual |
| 2 | (2/n): orden natural + límites 500/96 + "…y N fotos más" + hint del vacío | `aura_photos.c` | Fixture con 12 nombres desordenados; nombre de 80 bytes abre; 501 archivos muestran la fila final |
| 3 | (3/n): sonda de dimensiones + "Foto demasiado grande" + "Cargando…" | SOF0 parser en `aura_photos.c`; nuevos `AURA_STR_*` | Fixtures: 640×480 (sin indicador), 3000×2000 (indicador y foto), 5000×4000 (mensaje, sin decodificar), progresivo (no soportado). En sim y **en hardware** (Q6: cronometrar) |
| 4 | (4/n): lista con miniaturas 48 px + caché `.pfraw` | Renderizador dedicado en `aura_photos.c` reusando helpers de `aura_albumart.c` (extraer `pfraw_*` a una API con clave genérica si hace falta — mismo formato de archivo) | Segunda entrada a la lista sin decodificar (medir con `JDEBUGF`/contador); miniaturas correctas tras borrar un archivo |
| 5 | (5/n): rueda en el visor + `Fade-Slide` región + selección sincronizada al volver | `aura_photo_viewer_handle_button`, `aura_transition_fade_slide_region()`; gate `lcd_active()` y `animation_mode` | Captura a mitad de transición; con animación reducida corte directo |
| 6 | (6/n): documentación viva + bitácora | `componentes/photo-viewer.md`, `03-arbol-de-menus.md`, `00-vocabulario.md`, `CONTRATO-firmware-studio.md` §D (fila `Photos/` con la §6), `DECISIONS.md` D-291 (y D-292 si el contrato se separa) | Diff de docs revisado |
| 7 | `package_dist.sh` | Solo si el dueño quiere Release; fuera de este plan por defecto | — |

GPL: no se copia ningún módulo de Rockbox (todo lo nuevo vive en `apps/aura/` con cabecera); `MODIFICATIONS.md` solo cambia si Q12 se resolviera del otro lado (no recomendado).

Pruebas con imágenes reales (punto del encargo): las 13 preparadas de Studio (200–320 px), una 640×480 HD, una 3000×2000 copiada a mano, una 5000×4000 (demasiado grande), un progresivo, un PNG y un GIF (listados, no abribles), un BMP 24 bpp de 320×240 (abre) — en simulador todas; en hardware las de tiempo (Q6).

---

## 9. Hallazgos laterales (fuera del alcance de este plan, para las pasadas correspondientes)

**Studio** (`Aura-Studio`, ST-NNN futuras — no tocar hasta la pasada de Studio):
1. **Colisión de nombres en fotos**: `preparedURL` y destino usan solo el `lastPathComponent` de la fuente (`LibraryViewModel.swift:283`, `LibrarySync.swift:523`); dos fuentes homónimas se pisan. Verificado en la biblioteca real (`Fotos/cover.jpg` vs `Imágenes/cover.jpg` → un solo `.preparados/cover.jpg`).
2. **688 fotos `queued`** que son `cover.jpg` de carpetas de música — al arrastrar carpetas de música, las carátulas entran como fotos a la biblioteca. Probable causa de que el usuario espere "muchas fotos" y vea pocas: solo las 13 `ready` se copian.
3. Texto de "Versión HD" menciona zoom (§6.8).
4. `sync_summary.cfg → photo_count` cuenta solo items preparados; correcto, pero ya no será la fuente del estado vacío del firmware.

**Firmware** (mismo plan, ya cubiertos): bug hermano de Video (Q11); `03-arbol-de-menus.md` desactualizado; caché de listas sin invalidar.
