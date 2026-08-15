# CoverDrift

Nombre provisional — corrígeme si tienes uno ya definido en tu investigación.

## Capa

`--layer-content`. **Opcional** — solo se monta cuando hay contenido visual
rico disponible (carátulas de álbum cargadas, fotos). Cuando no lo hay, es
`SelectionSummary` en `--layer-base` quien se muestra en su lugar (ver
`componentes/selection-summary.md`). Siempre por debajo de `LeftPanel`
(`--layer-panel`) cuando este está montado.

## Comportamiento

Animación fluida donde las imágenes (carátulas de álbum, fotos) se mueven en
8 direcciones posibles: **0°, 60°, 90°, 120°, 180°, 240°, 270° y 300°**.

**Mecánica del movimiento (no es un recorrido simple):**

- Cada movimiento **termina en el centro** — no empieza ahí. La imagen
  arranca desde una posición descentrada (en alguna de las 8 direcciones) y
  se desplaza *hacia* el centro.
- **No siempre recorre el trayecto completo de extremo a extremo** — la
  distancia recorrida en cada movimiento puede variar, no es fija.
- **Duración: 7 segundos de movimiento constante** por desplazamiento. La
  velocidad exacta (px/s) no es un valor fijo — varía según la distancia a
  recorrer en cada instancia, pero siempre dura 7s.
- **Sensación deseada:** lento pero fluido — nunca entrecortado/trabado.

⚠️ **Interpretación:** leo "movimiento constante" como velocidad
uniforme/lineal durante esos 7s (sin easing de aceleración/desaceleración)
— avísame si en realidad querías algo de easing y solo te referías a que
no se sintiera entrecortado.

## Montaje

**Se necesitan al menos 10 imágenes disponibles** para que `CoverDrift` se
monte — con menos de eso, se queda `SelectionSummary`
(`componentes/selection-summary.md`) en su lugar.

## Transición con `SelectionSummary`

Cross-fade al montarse/desmontarse — spec completo en
`componentes/selection-summary.md`, sección "Transición con `CoverDrift`".

## Sombra de `LeftPanel`

Debe renderizarse una sombra que simule que `LeftPanel` está por encima de
este componente — spec completo en `efectos/01-sombras.md` (regla
actualizada, compartida con `SelectionSummary`).

## Reutilización

Este es el primer candidato real a **módulo** (no solo componente): el mismo
comportamiento de animación se usa tanto para carátulas de álbum (contexto
Música) como para fotos (contexto Fotos). Es un componente que se instancia
con distinta fuente de imágenes pero el mismo comportamiento de movimiento.

Cuando tengamos un tercer caso de reutilización confirmado, esto se mueve de
`componentes/` a `modulos/` formalmente — por ahora se queda aquí porque
solo hay dos contextos confirmados.

## Confirmado con el consumidor real (D-251, 2026-08-15)

Las 3 preguntas de abajo quedaban "pendientes" desde la construcción del
componente (D-098) — el propio código ya las había respondido de facto, y
D-251 (primer consumidor real, ver abajo) las deja confirmadas:

- **Una sola imagen visible a la vez**, con un ciclo de 7s COMPARTIDO — no
  hay ciclos independientes desfasados por imagen. `s_index` rota
  SECUENCIALMENTE (`(s_index+1) % count`) por la biblioteca, una imagen
  activa a la vez (G10).
- El ángulo de arranque de cada movimiento es **aleatorio entre los 8**,
  con una única restricción: nunca repite el ángulo inmediatamente
  anterior (evita que dos movimientos seguidos se vean iguales).
- Al llegar al centro, **el siguiente movimiento arranca de inmediato**,
  en el mismo cuadro — no hay pausa entre un movimiento y el siguiente.

**Consumidor real: Fotos** (`aura_photos.c`, lista de fotos). Miniatura de
90px decodificada bajo demanda con `read_jpeg_file()`
(`FORMAT_NATIVE | FORMAT_RESIZE`, sin `FORMAT_KEEP_ASPECT` — llena el tile
cuadrado completo). Solo se decodifica la imagen ACTIVA y la ANTERIOR
(nunca todo el pool de una vez, ver `aura_coverdrift_active_index()`/
`_prev_index()` en `aura_coverdrift.h`) — con hasta 200 fotos posibles
(`MAX_PHOTOS`), decodificar todas de antemano sería inviable en memoria.

**Musica queda SIN conectar en esta pasada** — investigado a fondo: las
pantallas de Canciones/Artistas/Géneros corren hoy en layout FULL
(`screen_uses_split_layout()` no las incluye), sin ningún panel derecho.
Volverlas SPLIT para darle un hueco a CoverDrift no es un simple cableado
— en layout FULL usan el riel A-Z (`draw_index_rail()`), que desaparece
en SPLIT; quitar el riel A-Z justo de las listas alfabéticas más largas
del sistema para ganar un fondo ambiental es un cambio de UX de fondo,
pendiente de decisión del dueño del producto. La infraestructura
(`aura_widgets_draw_list_with_art()`, los getters de índice de
`aura_coverdrift.h`) es genérica y ya está lista para Música en cuanto
se tome esa decisión.

**Bug real encontrado y corregido de paso** (no relacionado al motivo original
de esta tarea, pero bloqueaba por completo poder verificar Fotos):
`AURA_SCREEN_PHOTOS_ALL` (el destino real de la fila "Todas las fotos" del
menú de Fotos) no tenía NINGÚN caso de dibujo en `aura_screens.c` — caía al
`draw_empty_state()` genérico, mostrando "Nada sonando" en vez de la lista
real de fotos. Corregido (dispatch de dibujo + entrada en
`screen_uses_split_layout()`). El mismo bug, por el mismo motivo, existe
para `AURA_SCREEN_VIDEOS_ALL` — fuera de alcance de esta tarea, señalado
para quien decida abordarlo.
