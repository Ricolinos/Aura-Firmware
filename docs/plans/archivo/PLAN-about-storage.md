# PLAN-about-storage — Fase 1: "Acerca de" y su barra de almacenamiento

> **ESTADO: EJECUTADO** — 2026-08-17. Histórico. No es trabajo pendiente.
> Decisiones en `DECISIONS.md` (D-278/D-279).

**Fecha: 2026-08-16. Estado: PENDIENTE DE APROBACIÓN DEL DUEÑO. Ningún código editado.**

Investigación de solo lectura cruzando `docs/aura-design-system/` (selection-summary, transiciones, color por categoría, taxonomía), el código real (`aura_screens.c`, `aura_selection_summary.c`, `aura_transitions.c`, `aura_manifest.c`, `aura_category.h`, `mv.h`), Aura Studio (`LibrarySync.swift`, `CatalogSummary.swift`) y `DECISIONS.md`.

---

## 0. Viabilidad del dato de almacenamiento — PRIMERO

**Conclusión: viable sin escaneo del sistema de archivos, con una limitación honesta que hay que aceptar o resolver aparte.**

| Pregunta | Respuesta verificada |
|---|---|
| ¿De dónde salen hoy los bytes por categoría? | De `/.rockbox/aura/sync_summary.cfg` (`aura_manifest.c:10-11` → `aura_manifest_t` con `music/video/photo_bytes`, `aura_manifest.h:11-19`). **Lo escribe únicamente Aura Studio** al terminar cada sincronización (`LibrarySync.swift:234-235, 349-352`; `CatalogSummary.swift:52-63`), sumando el tamaño de los ítems del plan de sync (`:156-172`) — no midiendo el disco. |
| ¿La base de datos de Rockbox expone tamaño por tipo? | **No.** `tagcache.h:35` tiene `tag_length` (duración) y `tag_bitrate`, no tamaño de archivo; y solo indexa música. |
| ¿Videos/imágenes requieren escaneo? | Son carpetas planas (`/Videos`, `/Photos`; `aura_video.c:29,70`, `aura_photos.c:22,86`) que ya se listan con `opendir/readdir` al entrar. `dir_get_info()` (`dir.h:83-90`) devuelve `size` desde la entrada FAT **sin `stat()` extra** → sumar bytes cuesta lo mismo que listar (decenas de archivos, instantáneo). |
| ¿Y música? | Recursiva `/Music/<Artista>/<Álbum>/*` con miles de archivos → miles de lecturas de directorio en un disco de 2008. **Sin medición previa** en `DECISIONS.md`; el precedente más cercano (pre-caché de carátulas, `aura_music.c:271-296`) es síncrono con pantalla de progreso y `yield()`, **sin hilos por mandato explícito del dueño** tras los freezes D-204/D-206/D-214. Un escaneo de bytes de música sería del orden del escaneo de tagcache del primer arranque (segundos). |
| ¿Total y libre en tiempo real? | Sí: `volume_size()` (`mv.h:129`, `disk.c:609`) lee el contador de clusters libres de FAT, barato, ya usado hoy en `draw_about_storage_bars()` (`aura_screens.c:1640-1642`). |
| ¿Existe "extras" en el manifest? | **No.** Ni en el manifest ni como algo medible: de Extras cuelgan Cronómetro, Reloj internacional, Notas, Juegos (plugin dentro de `.rockbox/`), Calendarios, Bloqueo, Alarmas, Agenda (`03-arbol-de-menus.md:68-80`) — **ninguno tiene archivos de contenido de usuario**. "Extras en bytes" solo podría ser `.rockbox/` completo (~65MB de firmware, no contenido) o el residual "Otros" que el código ya calcula (`total − libre − música − video − fotos`: `.rockbox/`, playlists, caché de carátulas, sueltos). |
| ¿Qué pasa si el usuario copia por Finder? | El manifest queda desactualizado hasta el siguiente sync desde Studio; si nunca sincronizó, no existe (`draw_about()` ya muestra `AURA_STR_ABOUT_NO_SYNC`, `aura_screens.c:2223-2228`). Además el SelectionSummary lo cachea **una vez por sesión** (`cached_manifest()`, `:1441-1453`). |

**Recomendación (P0)**: manifest como fuente de música/video/fotos + `volume_size()` para total/libre — **sin escaneo del FS** en esta función. Complementos baratos: (i) recargar el manifest al entrar al estado expandido (no solo una vez por sesión); (ii) para Videos/Fotos, sumar `dir_get_info().size` en su carpeta plana al entrar y preferirlo sobre el manifest → refleja copias por Finder para esas dos categorías; música por Finder queda como limitación explícita documentada ("los datos de música vienen del último sync desde Aura Studio"). **No hay estado de carga**: nada es asíncrono. Sin manifest: barra solo con usado/libre (siempre disponible) + la leyenda ya existente de "sincroniza con Aura Studio".

---

## 1. Estado actual del código de "Acerca de"

- **Fila en Ajustes** (`aura_screens.c:1772-1777`): variante dinámica de SelectionSummary — `icon_renderer` = badge de Aura (D-269), texto superior "Mi iPod", `bottom_renderer = draw_about_storage_bars` (D-264).
- **Barra actual** (`:1629-1700`): 5 segmentos — Música `aura_accent()`, Video `category_flat_color(VIDEO)`, Fotos `PHOTOS`, Otros `SETTINGS` gris, Libre `PROGRESS_TRACK` — con separadores de 1px; desde D-277 con extremos de cápsula restaurando la imagen de fondo.
- **Posición actual de la barra** (medida en `draw_summary()`, `aura_selection_summary.c:358-545`): el `bottom_renderer` recibe `(x=164, y=169, w=152, h=12)` (`:437-442, :538-539`) — la barra va **pegada al tile** (tile en [75,165), barra en y=169..181, centro 175). El texto inferior, en cambio, se centra en el margen [165,240) → centro **y=202.5** (D-272). **La barra está 27.5px más arriba que donde va el texto** — esto es exactamente lo que pide el punto 1.
- **SELECT hoy**: entra a `AURA_SCREEN_SETTINGS_ABOUT`, pantalla **FULL-COLD de 3 páginas** (`:2055-2070, 2079-2203`: almacenamiento con barra de 3 segmentos + 3 cifras; contadores; dispositivo; dots de paginación; SELECT/RIGHT avanza, LEFT retrocede, MENU sale). Transición: `use_reveal` (`:4407-4414`, D-264) = `reveal_behind_panels()` — los dos paneles se abren y revelan la pantalla destino prerrenderizada. **El ícono NO se transporta**: la pantalla completa se dibuja de cero, sin badge. Salida: slide genérico.
- Divergencia interna ya existente: la página completa `draw_about_storage()` (`:2103`) sigue con 3 segmentos uniformes `PROGRESS_FILL` + separadores; la del SelectionSummary (D-264) ya es por color y con Otros/Libre.

---

## 2. Patrón de transición: `Shift-and-Reveal` — clasificación FULL-CARRY

**Confirmado en la doc viva** (`transiciones/00-vocabulario.md:156-180`, textual): "Coreografiado, 2 fases simultáneas… el elemento existente **nunca sale de pantalla** — es el mismo elemento que se reposiciona… 1. El elemento existente del lado derecho se reacomoda hacia la izquierda dentro del nuevo layout… 2. Simultáneamente, LeftPanel sale hacia la izquierda siguiendo su comportamiento estándar, y el nuevo componente aparece en el espacio que se libera a la derecha." Pendientes textuales: timing/easing sin valores; "¿aplica también cuando el elemento existente es un SelectionSummary estático?".

**Sin implementación**: cero referencias en `apps/aura/*.c` (solo `AURA_FLOW_SHIFT`, punto fijo de CoverFlow). `componentes/date-editor.md:44-60` (consumidor previsto) añade: los textos auxiliares desaparecen (no persisten), `StatusBar (full)` entra en Drop **al final**, LeftPanel sale empujado. La regla original de 2008 (`Reglas de comportamiento… (2008).md:40-45`, §2.4 SPLIT→FULL-CARRY) coincide salvo que dice barra "simultánea"; **la doc viva gana** (Drop al final).

**Clasificación**: el estado expandido es **`FULL-CARRY`** — definición textual (`(2008).md:16`): "Completa con elemento heredado — un elemento del panel derecho (ícono, imagen) sobrevive a la transición y se estira a pantalla completa". En 2008 "Acerca de" era `[FULL-COLD]` (`:190`); el encargo lo convierte en FULL-CARRY. Hoy en Aura **ninguna** pantalla entra por Shift-and-Reveal (Fecha/Hora/Brillo van por el push genérico). "Acerca de" será el primer consumidor real; `03-arbol-de-menus.md:114` deberá marcarlo `[FULL-CARRY]`.

### Diseño de la utilidad reutilizable (propuesta) — `aura_transition_shift_and_reveal()`

Convenciones que ya cumple `aura_transitions.c` y que se respetan: sin callbacks de dibujo — el destino se prerrenderiza con `aura_screens_draw(nav)` al offscreen `s_push_fb` (`:82-88, 313-321`); el frame actual se captura con `capture_outgoing_split_frame()` (`:120-131`); ease-out `eased_offset()` (`:93-97`); empuje del LeftPanel+StatusBar(split) como en `reveal_behind_panels()` (`:146-206`); Drop de barra con `PUSH_AND_DROP_DROP_FRAMES_*` (`:465-483`); puerta `lcd_active()`/`AURA_ANIM_NONE` (`:272`); `drain_button_queue_if_full()`; precedente de "regiones que hacen fade/deslizan en paralelo" = morph de entrada a NowPlaying (`:847-940`).

```c
/* aura_transitions.h */
typedef struct {
    int from_x, from_y, from_w, from_h;   /* rect del elemento en pantalla (split) */
    int to_x,   to_y,   to_w,   to_h;     /* rect del mismo elemento en el destino (full) */
} aura_shift_rect_t;

/* direction > 0: split -> full (entrar); < 0: full -> split (Menu, inversa exacta). */
void aura_transition_shift_and_reveal(aura_nav_t *nav, int direction,
                                      const aura_shift_rect_t *carry);
```

Algoritmo por cuadro `i` de `PUSH_AND_DROP_PUSH_FRAMES_*` a `PUSH_FPS_*` (mismos tokens D-274; la doc dice "2 fases simultáneas" y el empuje ocurre en paralelo), `prog = eased_offset(256, i, frames)`:
1. **Base**: blit del destino (`s_push_fb`) en filas `[bar_h, H)`; el contenido nuevo a la derecha del elemento entra con **fade** `blend(bg, dst, prog)` ("aparece en el espacio que se libera").
2. **LeftPanel + StatusBar(split) salen empujados**: columnas `[d, panel_w)` de `s_outgoing_fb` en x=0, `d = eased_offset(panel_w, i, frames)`; banda `[0, bar_h)` en `bg`. Idéntico a `reveal_behind_panels()`.
3. **Elemento persistente**: bitmap capturado del rect `from` de `s_outgoing_fb` (lo que ya está pintado — el llamador no necesita saber dibujarlo), blitteado en `(lerp(from_x,to_x,prog), lerp(from_y,to_y,prog))` **encima** de todo. Los textos auxiliares no están en ese rect → quedan tapados por la base desde el primer cuadro (cumple "desaparecen"). Sin escalado en la primera versión (`from` y `to` del mismo tamaño para Acerca de y DateEditor); si `to_w ≠ from_w` queda documentado como extensión.
4. `lcd_update()`, `drain_button_queue_if_full()`, `sleep(HZ/fps)`.
5. Al terminar: **Drop de StatusBar(full)** con `DROP_FRAMES_*` (bloque `:465-483`). El cuadro final coincide con el destino prerrenderizado (mismo "requisito de continuidad" que el vuelo de carátula).

**Inversa** (`direction < 0`, Menu): Lift de la barra(full) primero (`:359-380`, fase 1 de Lift-and-Push); `s_outgoing_fb` = full actual, `s_push_fb` = split destino; el elemento viaja `to → from`; el LeftPanel **entra** empujado (`left_w` crece, estilo `reveal_behind_panels_exit()`); el contenido de la derecha hace fade-out `blend(bg, src, 256−prog)`.

**Enganche**: `aura_screens_handle_button()` `:4407-4413`, donde `to == AURA_SCREEN_SETTINGS_ABOUT` ya se trata aparte (`use_reveal`) → pasa a llamar la utilidad. **DateEditor** la consume después con `carry` = rect del ícono de calendario en split y en el editor, sin reescribir nada.

**Timing**: la doc lo deja `[ ]`. Propuesta: Shift-and-Reveal usa **los mismos cuadros/fps que Push-and-Drop** (`push_and_drop.*`, D-274), porque el empuje del LeftPanel que ocurre en paralelo ya está tokenizado a ese ritmo — dos velocidades simultáneas se verían desacopladas. Se marca como decisión a ratificar (Q6).

---

## 3. Colores — lo que existe vs lo pedido

`sistema/04-color-por-categoria.md` (D-250, `tokens.json:115-124`):

| Categoría | Vigente (fijo por encargo 2026-08-14) | Pedido | Veredicto |
|---|---|---|---|
| Música | **acento configurable** `aura_accent()`, default `#FF2D55` rosa | rosa | Coincide solo con el default → **Q2** |
| Videos | azul marino **fijo** `#1E3A5F` (`video_hex`), elegido a propósito distinto del azul de sistema `#007AFF` "para que Video se lea distinto de Música si el acento es azul" | azul | Es azul, pero navy oscuro — **Q3** |
| Imágenes | naranja **fijo** `#FF9500` (`photos_hex`) | **verde** | **Conflicto directo** con D-250 — **Q4** |
| Extras | degradado `#FFCC00` amarillo → acento (`extras_yellow_hex`, sin hex plano) | amarillo | Coincide con el extremo amarillo; en barra plana sería solo `#FFCC00` — pero **no hay bytes de extras** (§0) — **Q5** |
| Otros / Libre | gris `#8E8E93` / `PROGRESS_TRACK` | (no pedidos) | Hoy existen como 4.º y 5.º segmento — **Q8** |

**No se inventa ningún hex nuevo**: el único verde del sistema es el preset de acento `#34C759`; si el dueño confirma verde para imágenes, se registraría como decisión de color de categoría (cambiando `photos_hex`, o como token aparte solo para la barra — ver Q4).

---

## 4. Posición de la barra (leída del código, no de la doc)

- Hoy: `y=169..181` (h=12), pegada al tile.
- Pedido: "misma posición vertical que el texto inferior" = centrada en el margen `[tile_y+TILE_SIZE, 240) = [165, 240)`, la fórmula de D-272 (`bottom_y = region_top + (240 − region_top − content_h)/2`, `aura_selection_summary.c:521-523`).
- **Propuesta P2**: `draw_summary()` centra la franja del `bottom_renderer` con esa misma fórmula → con h=12: **y = 196..208 (centro 202)**; con h=16: y=194..210. La altura de la barra sale de un token nuevo `aura_ds.metrics.about.bar_h` (hoy `A26_SPACING_LG`=12 implícito, sin token propio) — propongo conservar 12 (Q9).
- **Estado expandido**: mantener el **mismo eje vertical (y≈202)** para que el Shift-and-Reveal no salte verticalmente — el badge se desplaza solo en X, la barra crece solo en ancho. Propuesta P5.

---

## 5. Cambios propuestos (uno por uno)

Todos los valores nuevos en `design-system/tokens.json`; sin RGB ni medidas literales en `.c`.

| # | Cambio | Archivo:línea | Valor / detalle | Depende de |
|---|---|---|---|---|
| P0 | Fuente del dato: manifest + `volume_size()`; recargar manifest al expandir; sumar `dir_get_info().size` de `/Videos` y `/Photos` al expandir y preferirlo al manifest | `aura_screens.c:1441-1453` (`cached_manifest`), nuevo helper `about_storage_collect()` | Sin hilos, sin escaneo de música | — |
| P1 | Barra segmentada del SelectionSummary: 4 segmentos de contenido con colores por categoría + (según Q8) libre/otros; extras solo si Q5 lo define | `aura_screens.c:1629-1700` | Colores desde `aura_ds.color.category.*` (o token nuevo `about.segment_*` si el dueño separa "color de barra" de "color de categoría", Q4) | Q2–Q5, Q8 |
| P2 | Reposicionar la barra a la altura del texto inferior | `aura_selection_summary.c:437-442, 538-539` | Franja del `bottom_renderer` centrada en `[165,240)` con la fórmula de D-272; token `about.bar_h = 12` | Q9 |
| P3 | Utilidad `aura_transition_shift_and_reveal()` + `aura_shift_rect_t` | `aura_transitions.c/.h` (nueva, junto a `reveal_behind_panels`) | Algoritmo de §2; tokens `push_and_drop.*` reutilizados (Q6) | Q6 |
| P4 | Enganche: SELECT en Acerca de usa Shift-and-Reveal (entrada) e inversa (Menu) en vez de `use_reveal`/slide | `aura_screens.c:4407-4414` (+ `aura_screens_about_reveal_active()`, D-261) | `carry.from` = rect del badge en split (`tile_x=195, tile_y=75`, 90×90 — el badge es de 60px dentro del tile; decidir si viaja el tile completo o solo el badge, Q10); `carry.to` = posición izquierda en full | Q10 |
| P5 | Pantalla expandida (FULL-CARRY): badge a la izquierda, barra expandida a la derecha en el mismo eje y≈202, textos por categoría; **reemplaza** la página 1 de las 3 actuales | `aura_screens.c:2079-2203` (`draw_about`, `handle_about`) | Layout con tokens `about.*` (x del badge, gap, ancho de barra, alto de fila de texto) | Q7, Q11 |
| P6 | Textos de tamaño por categoría | mismo | Formato ya existente `output_dyn_value(..., byte_units, 4, true)` → "119MiB"/"1.2GiB"; etiquetas `DS_REG_12` `TEXT_SECONDARY`, cifras `DS_BOLD_12` (par título/dato de NowPlaying); sin fuente nueva (14/14); strings nuevas al final de ambos `.lang` (Extras/Otros/Libre si aplica) | Q12 |
| P7 | Doc: `transiciones/00-vocabulario.md` Shift-and-Reveal → implementado, timing tokenizado, respuesta al pendiente "¿aplica a SelectionSummary estático?" (sí, Acerca de lo prueba); nuevo `componentes/about.md` (nombre **`About`**, esqueleto en §7); `03-arbol` fila Acerca de `[FULL-CARRY]`; `selection-summary.md:226` enlaza; `date-editor.md` referencia la utilidad como lista para consumir | docs | — | — |
| P8 | Limpieza al paso: `morph_frames = ... ? 8 : 4` hardcodeado en `aura_transitions.c:898` → tokens `push_and_drop.*` (D-274 lo dejó pasar) | `aura_transitions.c:898` | — | — |

---

## 6. Preguntas abiertas (con recomendación)

**Q1 — Fuente del dato (§0)** — ¿aceptas manifest + `volume_size()` sin escaneo del FS, con Videos/Fotos medidos en vivo de su carpeta plana y música solo desde el último sync? Recomendación: **sí** — es instantáneo, no bloquea, y el escaneo de música en el hilo de UI (única opción sin hilos, por tu mandato) tardaría segundos cada vez.

**Q2 — Rosa de Música: ¿fijo o sigue al acento?** — Hoy la barra usa `aura_accent()` (D-264) y toda la jerarquía de color por categoría (D-250) dice "Música = acento". Recomendación: **(b) sigue al acento**, por coherencia con el resto del sistema (el tile de Música, el CoverDrift, los íconos ya lo hacen) — y para el choque con Videos si el usuario elige azul, el navy fijo `#1E3A5F` de Video **ya fue elegido justamente para eso** (`tokens.json:122`: distinto del `#007AFF` del preset azul); en la barra además hay separadores de 1px entre segmentos. Si prefieres (a) rosa fijo, sería un token nuevo `about.music_hex` que rompe con D-250 solo en esta barra.

**Q3 — "Azul" para Videos: ¿vale el navy `#1E3A5F` vigente?** — Recomendación: **sí**, usar el color de categoría (es azul, y es el que ya identifica a Video en todo el firmware). Si quieres un azul más vivo, sería cambiar `video_hex` para todo el sistema (decisión de D-250), no solo la barra.

**Q4 — "Verde" para Imágenes: conflicto con Fotos = naranja (D-250, tu encargo del 2026-08-14)** — Opciones: (i) la barra usa el color de categoría (**naranja**) para que "el mismo color = la misma cosa" en todo el firmware; (ii) cambias `photos_hex` a un verde para TODO el sistema (tile, íconos, CoverDrift de fotos); (iii) verde solo en la barra (token aparte). Recomendación: **(i) naranja** — un segmento verde que en el resto de la interfaz es naranja sería la única excepción a la jerarquía de color; si de verdad quieres verde, mejor (ii) para no romper la regla.

**Q5 — "Extras" no tiene bytes medibles (§0)** — ¿Qué quieres que represente el 4.º segmento amarillo? (a) el residual "Otros" (`.rockbox/`, playlists, caché, sueltos) — es lo único que existe; (b) nada: barra de 3 categorías + libre; (c) algo concreto que definas. Recomendación: **(a) "Otros" en amarillo `#FFCC00`** (el extremo amarillo de Extras) en vez del gris actual — cumple "4 colores" con un dato real; documentado como "Otros: firmware, playlists y archivos sueltos".

**Q6 — Timing de Shift-and-Reveal** — ¿ratificas usar los mismos cuadros/fps de Push-and-Drop (8@60Hz / 4@45Hz, D-274)? Recomendación: **sí** — el empuje del LeftPanel ocurre en paralelo y ya va a ese ritmo; queda tokenizado sin valor nuevo.

**Q7 — Las 3 páginas actuales de "Acerca de" (almacenamiento / contadores / dispositivo)** — ¿la pantalla expandida nueva las reemplaza por completo, o el estado expandido es la nueva "página 1" y RIGHT sigue paginando a contadores/dispositivo? Recomendación: **reemplaza la página 1 y conserva las otras dos con RIGHT/LEFT** (los contadores y datos del dispositivo son útiles y ya funcionan); Menu desde cualquier página vuelve a split con la inversa de Shift-and-Reveal.

**Q8 — ¿Espacio libre como 5.º segmento?** — Hoy sí (`PROGRESS_TRACK`). Recomendación: **sí, conservarlo** — es el carril "vacío" que hace legible cuánto queda; sin él la barra siempre estaría llena.

**Q9 — Segmentos de tamaño 0 o mínimos** — Recomendación: **0 → no se dibuja** (como hoy, `seg_w <= 0 → continue`); **>0 pero <2px → ancho mínimo de 2px** (token `about.segment_min_px = 2`) para que un video suelto no desaparezca; en el estado expandido su texto se muestra igual con "0 B".

**Q10 — ¿Qué viaja en el Shift-and-Reveal: el tile completo (90×90 con degradado gris) o solo el badge (60px)?** — Recomendación: **el tile completo** — es "el mismo elemento" que ve el usuario en split, y captura/blit del rect es trivial; en full queda a la izquierda como identidad de la pantalla.

**Q11 — Layout del estado expandido** — Propuesta concreta: badge/tile a la izquierda (x=16, mismo y=75), barra a la derecha desde x=122 hasta 304 en y≈202 (mismo eje que en split), y las filas de texto por categoría (punto de color + etiqueta + cifra) arriba de la barra, entre y=30 y y=190. ¿Va, o prefieres los textos debajo de la barra?

**Q12 — Formato de las cifras** — Recomendación: el mismo `output_dyn_value(byte_units, 4 dígitos)` que ya usa la página de Acerca de → "119 MiB", "1.2 GiB" (unidades binarias, consistente con el resto del firmware), **con porcentaje del disco entre paréntesis** ("119 MiB · 3%") porque la doc de SelectionSummary ya promete "con porcentaje real del disco". Tipografía: etiqueta `DS_REG_12` secundaria, cifra `DS_BOLD_12` — sin fuente nueva (presupuesto 14/14, sin hueco).

**Q13 — Segundo SELECT ya expandido** — Recomendación: **no colapsa** (SELECT/RIGHT pagina, como hoy); la vuelta a split es **solo con Menu** — coherente con toda la navegación de Aura (Menu = atrás), y evita que SELECT tenga dos significados en la misma pantalla.

---

## 7. Documentación a crear/actualizar (Fase 2, como parte del trabajo)

1. `transiciones/00-vocabulario.md` — Shift-and-Reveal: "Implementado (D-XXX) — `aura_transition_shift_and_reveal()`, primer consumidor Acerca de; timing = tokens `push_and_drop.*`"; cerrar los dos pendientes.
2. **Nuevo `componentes/about.md`** — nombre `About`, estructura de `index-rail.md`: qué es / dónde vive (split: variante dinámica de SelectionSummary; full: FULL-CARRY) / cuándo aparece / anatomía (split: badge + "Mi iPod" + barra a la altura del texto inferior; full: badge a la izquierda + barra expandida + textos) / estados / comportamiento (SELECT expande, Menu vuelve, RIGHT pagina) / fuente del dato y sus límites (Q1) / relación con SelectionSummary / tokens `about.*` / origen D-081 → D-264 → D-269 → D-277 → D-XXX / pendientes.
3. `sistema/03-arbol-de-menus.md:114` — Acerca de `[FULL-CARRY]`.
4. `componentes/selection-summary.md:226` — enlace a `about.md` y nota "SELECT expande vía Shift-and-Reveal".
5. `componentes/date-editor.md` — la utilidad ya existe, lista para consumir.
6. `sistema/04-color-por-categoria.md` / `fundamentos/01-color.md` — solo si Q4/Q5 cambian un color de categoría.
7. `DECISIONS.md` — D-278 (utilidad Shift-and-Reveal), D-279 (About: dato, barra, pantalla expandida).

---

**BARRERA: no se edita código hasta que el dueño apruebe este plan y responda Q1–Q13.**
