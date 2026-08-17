# PhotoViewer

🟢 Definido (D-291, 2026-08-17). Pantalla "Fotos → Todas las fotos": lista
con miniaturas reales (`LISTA-COMPLETA`) que abre un visor de fotos a
pantalla completa (`FULL-COLD`). Nombre interno en código:
`aura_photos_draw()`/`aura_photo_viewer_draw()` (`apps/aura/aura_photos.c`).
Diagnóstico completo y contrato con Aura Studio en `PLAN-image-viewer.md`
(raíz del repo).

## Dónde vive

- **`(list)`**: `LISTA-COMPLETA` (nivel 3, regla de profundidad por
  defecto) — **no** es `SPLIT`; la clasificación FULL/SPLIT de cada
  pantalla es una decisión cerrada del proyecto, esta pantalla no la toca.
  Entra desde el menú "Fotos" (`SPLIT`, nivel 2) con el push de ancho
  completo T3 que ya aplica el despacho centralizado de navegación
  (`aura_screens.c`).
- **`(viewer)`**: `FULL-COLD` — pantalla nueva sin relación visual con el
  panel derecho (no hay panel del que "cargar" un elemento, por eso no es
  `FULL-CARRY`). **Sin `StatusBar`**: la foto ocupa los 320×240 completos
  (`docs/design/…(2008).md` §3.4: "la foto es la pantalla").

## Cuándo aparece

Siempre que exista `/Photos` con al menos una imagen listable
(`.jpg/.jpeg/.bmp/.png/.gif`). Con 0 imágenes, `(list)` muestra el estado
vacío (ver abajo) en vez de abrir.

## `(list)` — anatomía

Mismo patrón que la lista de Álbumes (`draw_album_list()`/
`draw_album_thumb()`, D-221) — única lista de CONTENIDO con miniatura real
hasta que esta pantalla se sumó:

| Elemento | Medida | Origen |
|---|---|---|
| Fila | 54px de alto | mismo `ALBUM_ROW_H` que Álbumes |
| Miniatura | 48×48, esquinas `A26_LAYOUT_CORNER_RADIUS_CARD` | mismo `ALBUM_ART_SIZE` que Álbumes |
| Filas visibles | 4 (218px útiles / 54) | — |
| Nombre | sin extensión (Principio 7), `DS_REG_12`, clip a la derecha | `strip_ext_for_display()` |

**Miniaturas**: las genera el firmware bajo demanda para la ventana
visible (nunca toda la lista de una vez), cacheadas en disco
(`.rockbox/aura/photocache/<nombre>-48.pfraw`) con el mismo formato
`.pfraw` transpuesto+esquinas-horneadas que Cover Flow — pipeline
compartido vía `aura_art.c` (`aura_art_read_pfraw()`/`write_pfraw()`/
`transpose()`/`mask_corners_transposed()`), no reimplementado. Llave del
cache: nombre de archivo (único dentro de `/Photos`, plano) + `mtime` del
archivo fuente — un re-sync con el mismo nombre pero contenido distinto
invalida la miniatura vieja. Fotos que exceden el tope de tamaño (ver
"Presupuesto de memoria" abajo) o son JPEG progresivo componen un tile
liso de fondo en vez de intentar decodificar — ese placeholder también se
cachea, para no reprobar la misma foto en cada redibujado.

**Orden**: natural, insensible a mayúsculas (`strnatcasecmp` —
"Foto 2" antes que "Foto 10"), no el orden físico del disco.

**Límite**: hasta 500 imágenes. Si `/Photos` tiene más, la lista agrega
una fila final **inerte** (atenuada, sin miniatura, SELECT no hace nada)
con el texto "…y N más" — nunca trunca en silencio.

**Estado vacío**: `StatusBar` + "Sin fotos todavía" + una segunda línea de
ayuda, más chica y atenuada: "Sincroniza fotos desde Aura Studio" — dice
dónde resolverlo, no solo que está vacío (relevante: este vacío era
justo el síntoma reportado que motivó D-291).

## `(viewer)` — anatomía

- Imagen ajustada al marco ("fit", nunca zoom — `docs/design/…(2008).md`
  §3.4), centrada, franjas del color `--color-bg-base` del tema activo
  (nunca negro fijo).
- **Indicador de carga** ("Cargando…", texto centrado, `TEXT_SECONDARY`):
  solo cuando la sonda de dimensiones determina que la fuente supera
  640px de lado — único caso en que la decodificación es perceptible
  (las fotos de Aura Studio, ≤640px, nunca lo muestran). Nota: como
  entrar/avanzar en el visor siempre pasa por una transición (propia o
  centralizada), y toda transición prerrenderiza el destino ANTES de
  animar, el aviso solo llega a verse en la práctica cuando la transición
  está desactivada (`animation_mode = Ninguna`) o la pantalla se está
  despertando — en el camino normal, la decodificación ocurre offscreen
  durante el prerender y el aviso queda invisible. No es un bug: es la
  misma característica que ya tienen el resto de las transiciones de
  este archivo.
- **Errores** (mismo estilo, texto centrado `TEXT_SECONDARY`):
  "Formato no soportado" (PNG/GIF listados pero no decodificables — D-028
  — o JPEG progresivo/aritmético) y "Foto demasiado grande" (fuente sobre
  el tope de tamaño).

## Navegación

- **Rueda** (`SCROLL_FWD`/`SCROLL_BACK`): navegación primaria entre fotos.
- **LEFT/RIGHT**: atajo al mismo destino — se conservan, no se retiran.
- **MENU o SELECT**: vuelve a `(list)`, con la selección sincronizada a la
  foto que se estaba viendo (no la que tenía al entrar) —
  `aura_nav_set_selection()` después de `aura_nav_pop()`.
- Dentro de `(list)`: rueda mueve la selección, SELECT sobre una fila real
  abre `(viewer)`; sobre la fila "…y N más" no hace nada.

## Transiciones

- `(list)` → `(viewer)` y de vuelta: el push/pop de ancho completo (T3)
  que ya resuelve el despacho centralizado de `aura_screens.c` — sin
  código propio.
- Entre fotos, dentro de `(viewer)`: **`Fade-Slide`, variante de región**
  (`transiciones/00-vocabulario.md`, D-283 — antes solo usada por las 3
  páginas de "Acerca de") sobre la pantalla completa (sin `StatusBar` que
  excluir de la región). `SCROLL_FWD`/`RIGHT` = dirección `+1` (entra
  desde la derecha); `SCROLL_BACK`/`LEFT` = dirección `-1`. Mismos tokens
  de timing que `Push-and-Drop` (D-274), respeta `lcd_active()` y
  `aura_settings.animation_mode` (con animaciones desactivadas, corte
  directo).

## Formatos soportados

Matriz completa y justificación en `PLAN-image-viewer.md` §3. Resumen:

| Formato | Veredicto |
|---|---|
| JPEG baseline (SOF0/SOF1) | Soportado, hasta 12 megapíxeles o 4096px de lado |
| BMP sin compresión | Soportado, mismo tope |
| JPEG progresivo/aritmético | No soportado — se detecta por cabecera, nunca se intenta decodificar |
| PNG, GIF | Se listan, "Formato no soportado" al abrir (D-028: sus decoders solo existen en el plugin `imageviewer`, no como función de app reutilizable) |
| HEIC, WebP, TIFF | Ni se listan — Aura Studio los convierte antes de sincronizar |

## Presupuesto de memoria

El decoder (`read_jpeg_file()`/`read_bmp_file()` del core, los mismos que
ya usan Cover Flow/Álbumes/CoverDrift/Ahora suena para carátulas) escala
en la IDCT y remuestrea por línea — **nunca materializa la imagen fuente
completa**, así que el costo es constante en memoria sin importar el
tamaño de la fuente. El tope de 12MP/4096px es por **tiempo de CPU**, no
por RAM (una fuente grande tarda varios segundos en decodificar en el
S5L8702 aunque quepa de sobra en el buffer). `s_view_scratch` (240KB,
`VIEW_SCRATCH_SIZE`) es el único buffer de trabajo del decoder — lo
comparten el visor de pantalla completa y la generación de miniaturas de
la lista (nunca están activos a la vez), invalidando explícitamente la
caché de la foto "cargada" del visor cada vez que una miniatura se
decodifica, para que volver al visor después de pasar por la lista
siempre redecodifique en vez de mostrar el contenido del buffer
compartido.

## Textos (español / inglés)

| Contexto | es-MX | en |
|---|---|---|
| Estado vacío, línea 1 | Sin fotos todavía | No photos yet |
| Estado vacío, línea 2 | Sincroniza fotos desde Aura Studio | Sync photos from Aura Studio |
| Fila final de límite | …y %d más | …and %d more |
| Formato no soportado | Formato no soportado | Unsupported format |
| Fuente sobre el tope | Foto demasiado grande | Photo too large |
| Indicador de carga | Cargando… | Loading… |

## Contrato con Aura Studio

`CONTRATO-firmware-studio.md` §D, fila `Photos/` — resolución máxima,
formato exacto, orientación horneada, unicidad de nombres. Detalle
completo en `PLAN-image-viewer.md` §6.

## Pendiente de definir

- [ ] Modo "pantalla completa recortada con paneo" del original (además
      del "ajustado al marco" actual) — requiere un buffer más grande
      (~512KB) y paneo con la rueda; fuera de alcance de D-291.
- [ ] Rejilla de miniaturas (2D) en vez de lista — el original la usa;
      esta pasada eligió lista por reutilizar el patrón ya aprobado de
      Álbumes sin inventar un componente de selección 2D nuevo.
- [ ] Modo presentación (tiempo por diapositiva, aleatorio, transiciones)
      del submenú Ajustes → Fotos del original — no construido.
- [ ] PNG nativo (portar `LodePNG` del plugin al core) — descartado por
      ahora: Aura Studio ya convierte a JPEG, portar ~2.9k líneas GPL más
      un tope de memoria propio no paga para archivos copiados a mano.
