# PLAN-about-fixes — Fase 1: correcciones y ampliación de "Acerca de"

> **ESTADO: EJECUTADO** — 2026-08-17. Histórico. No es trabajo pendiente.
> Decisiones en `DECISIONS.md` (D-280…D-283).

**Fecha: 2026-08-16. Estado: PENDIENTE DE APROBACIÓN DEL DUEÑO. Ningún código editado.**

Base: D-278/D-279 (sesión anterior). Investigación de solo lectura sobre `aura_screens.c` (`about_storage_collect`, `draw_storage_segments`, `draw_about*`, `handle_about`), `aura_selection_summary.c`, `aura_transitions.c`, `aura_manifest.c`, `firmware/common/fat.c`/`disk.c`, `dircache`, `tagcache`, Aura Studio (`LibrarySync.swift`, `MediaCategory.swift`, `CatalogSummary.swift`, `AppPreferences.swift`), y las docs vivas.

---

## 1. Corrección 2 — Porcentajes incorrectos: causa raíz CONFIRMADA (bug de datos, ×2)

**Causa raíz principal — unidad equivocada de `volume_size()`.** `about_storage_collect()` (`aura_screens.c:1513-1514`) hace `total_b = vol_size * SECTOR_SIZE` (512). Pero `volume_size()` → `fat_size()` (`firmware/common/fat.c:3034-3045`) devuelve `dataclusters * (secperclus * sector_size / 1024)` → **la unidad es KiB, no sectores**. Rockbox mismo lo formatea con `kibyte_units` (`apps/menus/main_menu.c:317-319, 442`). Verificado leyendo ambos archivos.

Efecto en cadena: `total_b` y `free_b` valen **la mitad** de lo real → (1) `pct = bytes*100/total_b` (`:2189`) **duplica** el porcentaje de música/video/fotos; (2) `other_b = total − free − contenido` sale distorsionado (el "740 GiB · 79%" del sim es 79% de la mitad de un disco de 1.86 TiB); (3) el ancho de cada segmento (`draw_storage_segments()`, `:1717`) usa el mismo `total_b` → segmentos de contenido al doble. Heredado de D-264 (ya multiplicaba por `SECTOR_SIZE`); D-279 lo movió sin revisarlo. **Arreglo: `* 1024`, no `* SECTOR_SIZE`** — una línea, dos sitios.

**Segundo defecto (presentación, menor)**: `pct` entero truncado (`(int)(bytes*100/total_b)`) → cualquier categoría bajo el 1% muestra "0%" (119 MiB sobre un disco grande). Propuesta: un decimal bajo 10% ("0.1%") o "<1%".

**Hipótesis descartadas con código**: (a) mock/placeholder — no: el fixture `sync_summary.cfg` del sim (`music_bytes 124918140`) coincide con los 126 MB reales de `Music/` del simdisk (la diferencia son 21 `cover.jpg` que Studio no cuenta); (b) sync incremental — no: `LibrarySync.sync()` (`:157-172`) suma TODOS los ítems de la selección antes de que `SyncPlanner` decida qué copiar; (d) bytes vs clusters — el manifiesto y `dir_get_info()` suman bytes reales, `volume_size()` cuenta clusters: en FAT32 de 32 KiB con ~10 300 archivos de `.rockbox/` la holgura es ~160 MiB — sesgo real pero solo infla "Otros", no explica los porcentajes; (e) iPod real sin sync desde Studio — **hipótesis secundaria vigente**: música solo viene del manifiesto (`:1509`); si copiaste música por Finder o nunca sincronizaste, `music_bytes` es 0/viejo. No es la causa en el sim, pero en hardware se sumaría al ×2.

**Método correcto para música (propuesta P2b)**: en hardware real `HAVE_DIRCACHE` está activo (`config.h:966-968`, encendido por defecto `settings_list.c:2016`) y `readdir` se sirve **de RAM** cuando `dircache_get_info().status == DIRCACHE_READY` (`dircache_redirect.h:262-267`, `filesize` por entrada). Un recorrido recursivo de `/Music` con la misma API que ya usa `sum_dir_bytes()` cuesta iterar N entradas en RAM (miles → decenas de ms), **sin acceso a disco**. Si dircache no está listo → fallback al manifiesto (como hoy). Descartado: sumar `stat` de las rutas de tagcache (miles de accesos a disco sin dircache; con dircache no aporta nada). Ejecutar **una vez al entrar** al expandido y cachear en RAM por sesión (invalidar por `dircache_get_info().entry_count` o cambio del manifiesto) — hoy `draw_about_storage_expanded()` relee y recorre **en cada redibujo** (visto de paso, `:2338-2362`); hay que cachear por entrada, no por cuadro. Sin hilos.

---

## 2. Corrección 1 — Fondo variante del SelectionSummary (solo Acerca de)

**Cómo funciona hoy**: `draw_summary()` (`aura_selection_summary.c:432-437`) recibe `x, width, icon_name, renderer, category, top_text, bottom_text, bottom_renderer` — **ningún id de pantalla**; el fondo está fijo: `ensure_panel_background("pink")` literal en `:472`. El llamador sí sabe quién es: `draw_panel_identity()` (`aura_screens.c:1029-1057`) construye desde `panel_identity_t` (`:930-941`, con `selected_target`, `icon_renderer`, `bottom_renderer`); la fila Acerca de se arma en `:1852-1857`.

**Recomendación: variante parametrizada** (verificado el costo, no solo por principio):
- Enum `aura_ss_background_t { AURA_SS_BG_ACCENT_IMAGE = 0, AURA_SS_BG_NEUTRAL_FADE }` en `aura_selection_summary.h`; campo `background` en `panel_identity_t` (+1 comparación en `panel_identity_equal()` `:960-972` — **importa**: cambiar de fondo debe contar como cambio de identidad para el debounce D-262/D-266); `draw_summary()` gana el parámetro y un `switch` en `:466-479`; `_draw_dynamic()` gana el parámetro (un solo llamador real, `:1048`); la fila Acerca de setea `NEUTRAL_FADE`. **~4 archivos, ~40 líneas netas.**
- El degradado en **runtime** (líneas con `a26_shell_blend()`, infraestructura de D-097), no BMP: no requiere PNG del dueño ni entrada en `generate_panel_backgrounds()`, y debe ser **por tema** (un BMP fijo no lo resuelve).
- Caso especial hardcodeado (`if (renderer == draw_about_icon_renderer)` dentro del componente): ~15 líneas, pero obliga al componente a conocer un símbolo `static` de `aura_screens.c`, rompe la dirección de dependencia (B-04/D-108) y no cuenta como cambio de identidad para el debounce (bug latente). Peor por ~25 líneas de diferencia.
- Extensible: cualquier fila elige fondo asignando el campo en `panel_identity_for()`, sin más código.

**Conflicto encontrado**: el texto superior "Mi iPod" es **blanco fijo** (D-267: "el fondo es siempre saturado", `:462, :525-529`) — sobre gris claro→blanco **no se lee** (1:1). Propuesta: la tinta se decide por variante: `ACCENT_IMAGE → blanco fijo` (como hoy), `NEUTRAL_FADE → A26_TEXT_PRIMARY` (por tema). Sombras (LeftPanel, SDF del tile) componen contra lo que haya debajo — sin conflicto.

**Dirección y estado expandido — análisis para Q**: el Shift-and-Reveal (`aura_transitions.c:274-360`) hace fade-in del destino (full, `SHELL_BG` plano) mientras el LeftPanel sale y el tile viaja x=195→16 en y=75. (i) **Horizontal (gris en x=160 → `SHELL_BG` en x=320)**: la franja derecha del split ya termina en `SHELL_BG`; durante el fade solo cambia la franja izquierda del panel, que además queda tapada por el LeftPanel saliendo la mitad del tiempo → **la más continua sin tocar el expandido**. (ii) Vertical: barra y texto sobre casi-blanco (bien para contraste) pero el expandido tendría que replicar el degradado o aceptar salto en la mitad superior. (iii) Diagonal: lo peor de ambas. **Tema oscuro**: "blanco" = `A26_SHELL_BG` del tema (#1C1C1E) y "gris" un tono más claro que él — tokens **de paleta**, no hex fijos: `light: from = progress_track #E5E5EA → to = shell_bg`, `dark: from = shell_rail #3A3A3C → to = shell_bg`. Sin hex nuevo.

---

## 3. Corrección 3 — "Sistema" separado de "Otros"

**Nomenclatura, confirmada**: el 4.º segmento se etiqueta **"Otros"** (`AURA_STR_ABOUT_OTHER`, `aura_lang.c:214, 426`) con el amarillo de Extras (`:1706, 2270`). "Extras" (menú: cronómetro, notas, juegos, calendarios, alarmas — sin archivos de contenido) y "Otros" (residual `total − libre − música − video − fotos`) son conceptos distintos que D-279 unió a propósito (Q5). Lo que ahora pides es sacarle al residual la parte del firmware.

**Qué es medible como "Sistema"**:
| Opción | Medible | Pros | Contras |
|---|---|---|---|
| (a) Partición de firmware de Apple | Sí: `disk_partinfo(0, &info)` (`disk.h:26-42`, `disk.c:381-390`) da `size` en sectores desde el MBR **aunque no esté montada** | Una llamada, gratis | **En tu iPod fue destruida** (D-185/D-186: el formateo de una sola partición la eliminó; "el firmware original NO está más") → devolvería 0. No es espacio de Aura sino de Apple. El arranque de Aura vive en NOR, no en disco |
| (b) `/.rockbox/` en la partición de datos | Sí: recorrido recursivo con `dir_get_info().size` — mismo caso que `/Music` (RAM con dircache; ~9 300-10 300 archivos, ~65-117 MiB) | Es exactamente "el espacio del firmware Aura" (rockbox.ipod, fuentes, íconos, códecs, plugins) | Costo de recorrido → cachear por sesión (o en disco invalidado por `rbversion.h`); casi constante entre arranques |
| (c) Ambas | — | — | Suma naturalezas distintas y en tu iPod (a) es 0 |

**Recomendación: (b)** — "Sistema" = bytes de `/.rockbox/` (recorrido con dircache, cacheado por sesión); "Otros" = residual restante (playlists, caché de carátulas, sueltos, holgura de clusters). Anotar en la doc que la partición de Apple no se cuenta.

**Color para "Sistema"**: `sistema/04-color-por-categoria.md` (D-250) ya asigna gris `#8E8E93` (`settings_gray`) a **Ajustes** — y ese gris era el color de "Otros" hasta D-279. No colisiona con rosa/navy/naranja/amarillo ni con los 6 presets de acento (ninguno es gris). Semánticamente Sistema = Ajustes. **Recomendación: `AURA_DS_COLOR_CATEGORY_SETTINGS_GRAY`, sin hex nuevo**; "Otros" conserva el amarillo.

---

## 4. Verificación de contraste (WCAG, umbral 3:1 para gráficos)

| Color | Sobre blanco #FFFFFF | Sobre carril #E5E5EA | Sobre `SHELL_BG` oscuro #1C1C1E |
|---|---|---|---|
| Rosa `#FF2D55` (Música) | 3.65 ✅ | 2.90 (límite) | 4.7 ✅ |
| Navy `#1E3A5F` (Video) | 11.5 ✅ | — | **1.48 ✗** (ya era así hoy) |
| Naranja `#FF9500` (Fotos) | **2.20 ✗** | 1.75 ✗ | 7.7 ✅ |
| Amarillo `#FFCC00` (Otros) | **1.51 ✗** | 1.20 ✗ | 11.2 ✅ |
| Gris `#8E8E93` (Sistema, candidato) | 3.26 ✅ | 2.60 | 5.2 ✅ |
| Textos `DS_REG_12` secundario / `DS_BOLD_12` primario | 5.07 / 21 ✅ | — | 3.36 / 13.5 ✅ |

**Fallan**: amarillo y naranja como segmentos y como puntos de color sobre fondo claro; navy en tema oscuro (defecto preexistente). El carril no ayuda (empeora). **Ningún tono "amarillo más oscuro" pasa 3:1 sobre blanco** sin dejar de ser amarillo (`#D69E00` = 2.40). **Propuesta, sin tocar los tokens de categoría globales: contorno de 1px** en cada segmento y punto — `blend(color, negro, ~30%)` sobre fondo claro / `blend(color, blanco, ~30%)` sobre oscuro — resuelve los tres casos con una sola regla, tokenizado como `about.segment_outline_pct`. Se reporta, no se aplica sin tu OK (Q4).

---

## 5. Estado 2 — Conteos detallados: viabilidad

| Conteo | ¿Fuente hoy en el iPod? | Detalle |
|---|---|---|
| Música: canciones | ✅ | manifiesto `music_count` (página 2 actual, `draw_about_counts()` `:2292-2317`) o `tagcache_get_stat()->total_entries` en vivo |
| Música: artistas | ✅ | tagcache en RAM (`tagcache_ram=1` desde D-023): `tagcache_search(tag_artist)` + `set_uniqbuf` como ya hace `aura_music.c:553-567` para el navegador; decenas de ms |
| Música: listas | ✅ | `playlist_count` del manifiesto o contar `/Playlists` (carpeta plana) |
| Video: película / videoclip / serie | ❌ en firmware, **✅ en Studio** | Rockbox no tiene DB de video. Pero **Aura Studio ya clasifica cada video** en Videos / Series / Películas (`MediaCategory.swift`, heurística `classifyVideo()` >40 min = película, series a mano; encargo 2026-08-13) en `LibraryItem.category` — **hoy no lo escribe al iPod**: `Videos/` va plana y `CatalogSummaryWriter` solo emite `video_count/video_bytes` |
| Fotos: IA / fondos / fotografías | ❌ en firmware, **parcial en Studio** | Sin parser EXIF en `apps/aura` ni en el visor. Studio sí: `classifyPhoto(softwareTag:hasCameraExif:)` → "IA" (tag Software con Midjourney/DALL-E/Stable Diffusion/…), "Fotos" (EXIF de cámara), "Imágenes" (resto); colecciones editables (`AppPreferences.photoCollections`, default `["Imágenes","Fotos","IA"]`). **"Fondos de pantalla" no existe** como categoría ni heurística |

**Mecanismo recomendado — opción (c), etiquetado desde Studio**: extender `sync_summary.cfg` con conteos por categoría (`video_movies_count / video_series_count / video_clips_count`, `photo_ai_count / photo_camera_count / photo_images_count`), leerlos en `aura_manifest.c` (mismo parser) y mostrarlos. Es el canal que ya existe, la clasificación ya está hecha y es corregible por el usuario en Studio, y el firmware no necesita EXIF. Contras: solo tan actual como el último sync (mismo límite que música); "fondos de pantalla" requiere definir qué es. Descartadas: (a) convención de carpetas — Studio sincroniza `Videos/`/`Photos/` planas a propósito porque el navegador del firmware no recorre subcarpetas; (b) EXIF en firmware — no existe parser y no cubre IA/fondos.

**BLOQUEANTE del Estado 2 tal como se pidió**: sin el cambio en Studio, hoy solo Música es real. **En Fase 2 se implementa Música completo (canciones/artistas/listas) y video/fotos quedan explícitamente "pendiente de sincronización" — nunca con números simulados** — salvo que apruebes hacer el cambio en Studio en la misma pasada (Q6).

---

## 6. Estado 3 — Créditos

**Hallazgo**: **ya existe** una pantalla legal completa — Ajustes → **Avisos legales** (`AURA_SCREEN_SETTINGS_COPYRIGHT`, `draw_long_text()` `:2755` con `aura_widgets_wrap_text` + scroll por rueda, `handle_legal_text()` `:2792`, `DS_REG_10` `TEXT_SECONDARY`) con `AURA_STR_COPYRIGHT_BODY` (`aura_lang.c:155/367`): menciona Rockbox + GPL v2, marcas de Apple sin afiliación, créditos ("Diseño: Ricardo Gomez (Ricolinos). Implementación del código: Claude…"). **Dos problemas**: (1) le falta la **URL del código fuente** (GPL v2 §3); (2) contiene una cláusula **"PROHIBIDA su distribución… venta" que contradice la GPL v2** (§6: no se pueden imponer restricciones adicionales) — hay que corregirla en el mismo movimiento (Q7). La página 3 actual de Acerca de solo dice "Basado en Rockbox". README.md:26-28: "`firmware/` — GPL v2 (heredada de Rockbox)"; **no hay `LICENSE`/`COPYING` en la raíz del repo** (recomendado añadirlo). Rockbox: `docs/COPYING` (GPL v2), plugin `credits.rock`.

**Borrador (español impecable, con tildes — verificado que las SF cubren acentos)**:

```
Aura
Creado por Ricardo Gómez.

Basado en Rockbox
© Rockbox y sus colaboradores. Rockbox se distribuye bajo la
Licencia Pública General de GNU, versión 2 (GPL v2), y Aura
hereda esa licencia.
Código fuente: github.com/Ricolinos/Aura-Proyect

Sobre el hardware
iPod e iPod Classic son marcas de Apple Inc. Aura no está
afiliado, patrocinado ni respaldado por Apple.
```

Conteo con `DS_REG_12` (~6 px/carácter, 288 px útiles → ~48 chars/línea, 15 px/línea, área ≈196 px → 13 líneas): **11-12 líneas → cabe estático a ancho completo**; a la derecha del tile (200 px, ~33 chars) sube a ~18 → necesita scroll. Mecanismo de scroll existente y reutilizable: `draw_long_text()`/`handle_legal_text()`. Tipografía: "Aura" `DS_BOLD_14`, subtítulos `DS_BOLD_12`, cuerpo `DS_REG_12` secundario, URL en primario — sin fuente nueva (14/14).

---

## 7. Secuencia 0 → 1 → 2 → 3 — diseño

**Hoy**: `handle_about()` (`:2366-2390`): SELECT/RIGHT `page++` (tope en 2, sin ciclar), LEFT `page--`, MENU sale directo a split desde cualquier página con la inversa de Shift-and-Reveal (`:4483-4520`). **Cambio seco** entre páginas. Dots de 4 px (activo `A26_ACCENT`). El original 2008 (`:190`): "3 modos que se navegan con backward/forward (select = forward)" — no cicla.

**Transición full→full (clics 2 y 3)**: `aura_transitions.c` no tiene nada para "cambio de página dentro de full" (NowPlaying tampoco anima el cambio de modo, salvo el Modo 4). El vocabulario ofrece **`Fade-Slide`** (horizontal, fundido + desplazamiento, "profundizar = derecha→izquierda, regresar = izquierda→derecha", `00-vocabulario.md:212-224`). **Propuesta**: `Fade-Slide` de la **región derecha** (x≥122, y∈[20,240)) — el contenido sale hacia la izquierda con fade-out mientras el nuevo entra desde la derecha con fade-in; el tile y la StatusBar no se mueven. Timing: cuadros de `push_and_drop.push_frames_*` (8/4) con desplazamiento ≈¼ del ancho de la región (`eased_offset`), sin token nuevo salvo `about.page_slide_px` si se quiere tokenizado. Implementable como utilidad `aura_transition_fade_slide_region(rect, direction)` en `aura_transitions.c` (offscreen del destino ya disponible), **reutilizable luego por DynamicTitle** (hoy no cableado, RESYNC-GAPS B2).

**Persistencia del tile**: con el tile en x=16..106 quedan 200 px (x≈122..304): los conteos (3 grupos × 3 líneas + títulos ≈ 12 líneas `DS_REG_12` a 15 px = 180 px) **caben** a la derecha; los créditos (~18 líneas a 33 chars) **no** sin scroll.

---

## 8. Cambios propuestos (uno por uno)

| # | Cambio | Archivo:línea | Detalle | Depende |
|---|---|---|---|---|
| P1 | **Fix ×2**: `* 1024` en vez de `* SECTOR_SIZE` | `aura_screens.c:1513-1514` | Corrige porcentajes, anchos y "Otros" de un golpe | — |
| P2a | Porcentaje con un decimal bajo 10% (o "<1%") | `:2189` | Presentación | Q1 |
| P2b | Música medida en vivo por recorrido de `/Music` con dircache listo, fallback al manifiesto; resultado cacheado por entrada (no por cuadro) | `about_storage_collect()` + `sum_dir_bytes()` recursivo | Sin hilos; RAM con dircache | Q2 |
| P3 | Fondo variante parametrizado: enum + campo en `panel_identity_t` + `switch` en `draw_summary()`; tinta del texto por variante; degradado runtime por tema con tokens de paleta | `aura_selection_summary.h/.c`, `aura_screens.c:930-972, 1029-1057, 1852-1857`, `tokens.json` (`selection_summary.neutral_fade_from/to` por tema) | ~40 líneas | Q3 (dirección) |
| P4 | Categoría "Sistema" = `/.rockbox/` recursivo con dircache, cacheado por sesión; "Otros" = residual restante; 6 segmentos (Música, Video, Fotos, Sistema, Otros, Libre); string `AURA_STR_ABOUT_SYSTEM` en ambos `.lang` | `about_storage_collect()`, `draw_storage_segments()`, filas del expandido | Color `SETTINGS_GRAY` | Q5 |
| P5 | Contorno de 1px en segmentos y puntos (`about.segment_outline_pct`) | `draw_storage_segments()`, filas | Solo si Q4 = sí | Q4 |
| P6 | Estado 2: conteos — Música (canciones/artistas de tagcache, listas); video/fotos con texto "Sincroniza con Aura Studio para ver el detalle" mientras el manifiesto no traiga los campos nuevos | `draw_about_counts()` reescrita | Sin números simulados | Q6 |
| P6b | (Opcional, cruza a Studio) `CatalogSummaryWriter` emite conteos por categoría; `aura_manifest.c` los lee | `CatalogSummary.swift`, `LibrarySync.swift`, `aura_manifest.c/.h` | Solo si Q6 = sí | Q6 |
| P7 | Estado 3: créditos con el borrador de §6, `draw_long_text()` con scroll si no cabe a la derecha del tile; corregir `AURA_STR_COPYRIGHT_BODY` (quitar cláusula anti-GPL, añadir URL); añadir `LICENSE` en la raíz del repo | `aura_screens.c` página 3, `aura_lang.c`, raíz | | Q7, Q8 |
| P8 | Utilidad `aura_transition_fade_slide_region()` para clics 2/3 (y LEFT); `handle_about()` la invoca | `aura_transitions.c/.h`, `handle_about()` | Cuadros `push_and_drop.*` | Q9 |
| P9 | Doc: `about.md` (5-6 categorías, fondo variante, secuencia de 4 estados, fuente de cada dato y sus límites), `selection-summary.md` (parámetro de fondo), `00-vocabulario.md` (Fade-Slide gana consumidor de región; nota full→full), `04-color-por-categoria.md` (Sistema = gris de Ajustes, contorno si aplica), `01-color.md` tokens del degradado | docs | | — |
| P10 | DECISIONS: D-280 (fix ×2 + método de medición), D-281 (fondo variante), D-282 (Sistema/Otros + contraste), D-283 (secuencia: Estado 2 con su bloqueante, Estado 3 créditos/GPL, Fade-Slide) | | | — |

---

## 9. Preguntas abiertas (con recomendación)

**Q1 — Formato del porcentaje bajo 1%**: ¿"0.1%" (un decimal bajo 10%) o "<1%"? Recomendación: **un decimal bajo 10%** — es más informativo y cabe igual.

**Q2 — ¿Medir música en vivo recorriendo `/Music` (con dircache, RAM) en vez de solo el manifiesto?** Recomendación: **sí, con fallback al manifiesto** cuando dircache no esté listo — refleja copias por Finder y elimina la hipótesis (e) en tu iPod. Costo: decenas de ms una vez por entrada.

**Q3 — Dirección del degradado y estado expandido**: (i) horizontal gris→`SHELL_BG` de izquierda a derecha, expandido sin cambio (plano); (ii) vertical; (iii) diagonal. Recomendación: **(i) horizontal** — la franja derecha ya termina en el color del expandido, así que el Shift-and-Reveal no cambia de fondo en la zona visible; el expandido se queda plano `SHELL_BG`. Colores por tema con tokens existentes (`progress_track → shell_bg` claro; `shell_rail → shell_bg` oscuro). ¿Y extenderlo a otras filas de Ajustes? El mecanismo lo permite; recomiendo **solo Acerca de por ahora** hasta ver el resultado.

**Q4 — Contraste**: amarillo (1.5:1) y naranja (2.2:1) fallan sobre fondo claro, y no hay tono más oscuro que salve al amarillo. ¿Contorno de 1px (`blend` 30% hacia negro/blanco por tema) en segmentos y puntos, o aceptarlos como están? Recomendación: **contorno** — una sola regla que además arregla el navy en tema oscuro (defecto preexistente).

**Q5 — "Sistema"**: ¿(b) `/.rockbox/` en la partición de datos, en gris de Ajustes `#8E8E93`? Recomendación: **sí** — es literalmente el espacio de Aura; la partición de Apple ya no existe en tu iPod y no es de Aura. Nota: la doc dirá explícitamente que no se cuenta la partición de firmware de Apple.

**Q6 — Estado 2, video/fotos**: ¿hacemos en esta misma pasada el cambio en **Aura Studio** (emitir conteos por categoría en `sync_summary.cfg` — Studio ya clasifica Videos/Series/Películas e Imágenes/Fotos/IA) y su lectura en el firmware, o dejamos solo Música ahora y video/fotos como "pendiente de sincronización"? Recomendación: **hacerlo en la misma pasada** (P6b) — es ~30 líneas en Swift + ~15 en C, y sin eso el Estado 2 queda a un tercio. Subpregunta: **"fondos de pantalla" no existe** en Studio — ¿lo quitamos del encargo, o lo defines (¿una colección de fotos con ese nombre?)? Recomendación: **quitarlo** y mostrar las tres categorías que Studio ya distingue (Imágenes / Fotografías / IA); "videoclips" = la categoría "Videos" de Studio (lo que no es serie ni película).

**Q7 — `AURA_STR_COPYRIGHT_BODY` contiene "PROHIBIDA su distribución… venta"**, incompatible con la GPL v2 que Aura hereda. ¿Se corrige en la misma pasada (quitar la restricción, añadir la URL del código fuente) y se añade `LICENSE` (GPL v2) en la raíz del repo? Recomendación: **sí a ambas** — es cumplimiento, no estilo. Créditos y Avisos legales quedan coherentes entre sí (Acerca de → resumen; Avisos legales → texto completo).

**Q8 — Créditos, ¿el tile persiste?** A la derecha del tile los créditos necesitan scroll (~18 líneas); a ancho completo caben estáticos (11-12). Recomendación: **el tile persiste en Estados 2 y 3** (continuidad del viaje) y los créditos usan el scroll por rueda ya existente (`draw_long_text()`); si prefieres créditos sin scroll, el tile se desvanece solo en el Estado 3.

**Q9 — Clic 4 y Menu**: Recomendación: **SELECT/RIGHT avanza 1→2→3 y en 3 no hace nada** (como hoy y como el iPod de 2008: "select = forward", sin ciclar — las páginas no son modos modales como en NowPlaying, son vistas jerárquicamente equivalentes); **LEFT retrocede**; **MENU sale directo a split** desde cualquier estado con la inversa de Shift-and-Reveal (como hoy; retroceder nivel a nivel obligaría a 3 pulsaciones y no tiene precedente en Aura). Transición entre páginas: **Fade-Slide de la región derecha** (P8), en vez del cambio seco actual.

---

**BARRERA: no se edita código hasta que el dueño apruebe este plan y responda Q1–Q9.**
