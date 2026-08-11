# AUDITORÍA-01 — Estado real del árbol contra los tres documentos de diseño

**Fase A — solo lectura.** Ningún archivo de código, asset ni documento fue
modificado para producir este reporte; las únicas escrituras fueron este
archivo y las capturas de evidencia en `docs/screenshots/auditoria01/`.

- **Base auditada**: commit `912abe0` (HEAD de `main`, cierre de Fase 32).
- **Documentos fuente, leídos completos**: `Reglas de diseño Apple2026
  (v2).md` (doc-D), `Reglas de comportamiento - iPod Classic original
  (2008).md` (doc-C), `Reproductor - Ahora suena.md` (doc-R).
- **Código leído completo**: los 7 002 renglones de `apps/aura/` más
  `design-system/generate.py`, `tokens.json` y los scripts de captura.
- **Evidencia mecánica**: conteo de tonos por BMP (Pillow), inspección de
  cabeceras `.fnt`, greps de tokens/anti-patrones, y la matriz de capturas
  regenerada hoy desde este mismo árbol (2 temas × 3 modos × 30 pantallas,
  0 fallos — `docs/screenshots/matrix/`).

---

## 0. Bug prioritario (diagnóstico confirmado, corrección para Fase B)

Confirmado mecánicamente, no solo a ojo: **los 468 BMP de iconos (234 por
tema) tienen exactamente 1 tono de tinta cada uno**. La distribución de
"tonos distintos por archivo" es `{1: 234}` en `light` y `{1: 234}` en
`dark` — binarización total, cero rampa. La causa es la que ya
diagnosticaste: `design-system/generate.py:313` (`ALPHA_THRESHOLD = 128`) y
`generate.py:331-337` — tras el LANCZOS se umbraliza el alfa y se pega
tinta sólida sobre magenta puro. El supersampleo de D-075 mejoró DÓNDE cae
el borde, pero cada píxel sigue siendo todo-o-nada; los dientes de sierra
son inevitables con ese paso.

**Corrección comprometida para Fase B** (descripción, no ejecutada):

1. `render_symbol_shapes()` pasa de `SUPERSAMPLE = 8` a **16** y la
   reducción a 18/20 px se hace con **filtro de caja** (`Image.BOX` —
   promedio puro de cobertura, sin el ringing de LANCZOS), conservando el
   canal alfa completo como mapa de cobertura.
2. `generate_icons()` deja de umbralizar: por cada píxel compone
   `tinta_variante` sobre `SHELL_BG` **del tema en curso** usando la
   cobertura como alfa (`out = bg + (fg - bg) * cobertura`). La tinta de la
   variante `""` es `TEXT_PRIMARY` del tema; la de `-on` es `ACCENT` del
   tema — igual que hoy, pero compuestas, no pegadas.
3. Magenta (255,0,255) **solo** donde cobertura == 0 exacto. Ningún píxel
   con cobertura parcial toca el magenta: queda pre-compuesto contra el
   fondo del tema. Así `lcd_bitmap_transparent()` (transparencia binaria,
   D-010) sigue funcionando sin cambios en el código C.
4. El set claro y el set oscuro se generan **independientes, cada uno
   contra su propio `SHELL_BG`** — el oscuro no es el claro recoloreado
   (anti-patrón §8 doc-D, "halo blanco sobre fondo oscuro"). El bucle
   actual `for theme_name` ya itera así; lo que cambia es que la
   composición usa el `shell_bg` de ese tema, no un marcador único.
5. **Verificación mecánica en el propio script**: tras generar cada tema,
   contar los tonos de tinta presentes (excluido el magenta). Una salida
   sana tiene decenas de valores intermedios entre `ACCENT`/`TEXT_PRIMARY`
   y `SHELL_BG`; si algún archivo reporta ≤3 tonos, el script lo declara
   FALLO y sale con error. El conteo se imprime en la salida del pipeline.

Nota de alcance: el origen de símbolos NO se toca —
`scripts/apple2026_sf_render.swift` vía AppKit ya es el correcto (doc-D §4:
"no hay un catálogo alternativo más oficial que pedírselo al sistema") y no
se descarga nada de internet.

Limitación honesta que este fix arrastra: la composición fija el fondo en
`SHELL_BG`. Sobre la **pastilla de selección** (`SELECTION_FILL`, gris) el
borde compuesto-contra-blanco/negro deja un halo tenue del color del fondo.
Con la rampa completa será mucho menos visible que la escalera actual, y la
salida limpia existe si algún día molesta: una tercera composición contra
`SELECTION_FILL` para la variante `-on` (la única que se dibuja sobre la
pastilla) — se deja anotado, no decidido (ver Ambigüedad A-9).

**Efecto colateral valioso**: con la composición por variante, agregar
variantes `-tertiary` (tinta `TEXT_TERTIARY`) y `-rail` (`SHELL_RAIL`)
cuesta dos líneas en `tokens.json → icon.variants`. Eso destraba dos
desviaciones que D-078 documentó como límite de D-010: los iconos de modo
inactivos de Ahora suena (doc-R §5 pide `TEXT_TERTIARY`, hoy van en
primario — `aura_nowplaying.c:318-324`) y las estrellas vacías (doc-R §4
pide `SHELL_RAIL`, hoy primario — `aura_nowplaying.c:261-273`). Ver A-16.

---

## 0-bis. Decisión del usuario que anula la lectura literal del documento

**Fila seleccionada = pastilla `SELECTION_FILL` + texto E icono en
`ACCENT`.** Estado real del código: **ya implementado así** —
`aura_widgets.c:434-435` (texto en `A26_ACCENT` si `is_selected`) y
`aura_widgets.c:426-427` (variante `-on` del icono), con la justificación
escrita en `apple2026_shell.h:26-30` ("el acento ES el estado activo").
No se marca como hallazgo ni se propone "corregirlo" a primario. Lo que
falta es lo inverso: **el documento** (doc-D §5.1 y Principio 2) no lo dice
— queda como tarea de Fase B actualizar §5.1 y el Principio 2 para que la
fuente de verdad deje de contradecir la decisión. Misma decisión aplicada
consistentemente en: valor de fila booleana (`aura_widgets.c:493`), dígito
en foco (`aura_widgets.c:573`), chips de confirmación (`aura_widgets.c:693-698`),
lista resaltada del panel de playlists (`aura_nowplaying.c:469`).

---

## 1. Tabla principal de hallazgos

Severidad: **Bloqueante** / Alta / Media / Cosmética. "Cubierto por plan"
cita la fase/decisión SOLO si el vacío ya está documentado ahí; "—" = nadie
lo tiene anotado.

| ID | Ubicación | Regla violada | Severidad | Evidencia | Corrección propuesta | ¿Cubierto por plan? |
|---|---|---|---|---|---|---|
| A-01 | `design-system/generate.py:313,331-337` | doc-D §4 (bordes limpios) + Principio 5 (assets por tema con su propio antialias) | **Bloqueante** | 468/468 BMP con exactamente 1 tono de tinta (conteo Pillow); dientes visibles en `matrix/settings-light-all.png`, `matrix/nowplaying-dark-all.png` | La de la sección 0 (cobertura 16×→caja, composición por tema, magenta solo en cobertura 0, verificación mecánica de rampa) | — (supersede D-075) |
| A-02 | `aura_lang.c:4-87` (tabla ES completa), `aura_splash_lang.c:30-43` | doc-D Principio 7 "Español impecable" + checklist §10.6 | **Bloqueante** | En pantalla: "Musica", "Albumes", "Graficos", "Limite volumen" (`matrix/root-light-none.png`, `matrix/music_albums-dark-all.png`, `matrix/settings-light-all.png`). Lista completa abajo. Las `.fnt` cubren U+0020–U+FFFD (cabecera `RB12` verificada): **no hay excusa técnica** | Reescribir con ortografía correcta: Música, Álbumes, Géneros, Gráficos, Límite volumen, Menú principal, Mínimas/Mínimos, Español, Inglés, Sí, "valor de fábrica", "Sin música todavía", Batería (splash ×2). Y "Todavia no sincronizaste con Aura Studio" → **"Aún no has sincronizado con Aura Studio."** (pretérito rioplatense; el mexicano natural es antepresente). Verificar glifos con una captura al primer cambio | — |
| A-03 | `aura_screens.c:1023-1036` (`screen_uses_split_layout`) | doc-C §1 regla de profundidad ("nivel 3 en adelante → LISTA-COMPLETA") + §4.6 (Menú pral., EQ, Idioma marcados `[LISTA-COMPLETA]` explícitos) | Alta | Tema/Animaciones/Gráficos/EQ/Idioma/Repetir/Temporiz. luz/reposo/Menú pral. (nivel 3) y todo el árbol de Música desde nivel 3 (Artistas→Álbumes→Canciones, Listas) se dibujan SPLIT — `matrix/settings_eq-light-minimal.png`, `matrix/settings_mainmenu-*.png`, `matrix/music_songs_by_album-*.png` | Anotar nivel de profundidad (o tipo doc-C) por pantalla en la tabla y devolver FULL para nivel ≥3; el push T1/T3 ya distingue ancho por esa misma tabla, así que la transición se corrige sola | Parcial: el "motor de transiciones v2 dirigido por la taxonomía" (Fase 28) quedó diferido en D-076, pero el mapa split/full NO necesita ese motor y nadie lo tiene anotado |
| A-04 | `aura_statusbar.c:113` + `tokens.json:69-74` | doc-D §3 ("Semibold → títulos de pantalla") | Alta | Todo título de barra se dibuja con `A26_FONT_STYLE_CAPTION` = SF Compact **Regular** 13; el pipeline no genera NINGUNA cara Semibold a 13px (solo `title` 20px Semibold) | Añadir estilo `caption_semibold` (o similar) a `styles_by_size` → `a26-…-13.fnt` Semibold, y usarla en el título de la barra (y futura letra activa del riel A-Z, mismo §3) | — (ligado a Ambigüedad A-a) |
| A-05 | `aura_photos.c:108` y `aura_video.c:89` | doc-D Principio 7 + anti-patrón §8 "nombre crudo" | Alta | Las filas muestran el nombre de archivo con extensión (`IMG_1234.jpg`, `video.mpg`) — mismo defecto que D-081 ya corrigió en playlists | Pelar extensión para mostrar (patrón `aura_music_playlist_display_name()`, `aura_music.c:226-236`); el título del video real vive en el manifest de Aura Studio si se quiere el nombre curado | — |
| A-06 | `aura_photos.c:109` y `aura_video.c:90` | anti-patrón §8 "ícono repetido sin variación entre hermanos" | Alta | Cada fila de Fotos lleva `"image"` y cada fila de Videos `"video"` — N hermanos idénticos | Sin icono por fila en listas de contenido (el original tampoco lo tiene); la miniatura real es mejora futura | — |
| A-07 | `aura_nowplaying.c:221-235` (`draw_cover`) | doc-R §3 ("Carátula: cuadrado, **radio de esquina 8 px**") + doc-D §5.4 nivel 2 | Alta | `lcd_bitmap()` directo, esquinas vivas — visible en `matrix/nowplaying-dark-all.png` | Estampar las 4 esquinas de la carátula (y las 2 superiores del reflejo, que espejan el borde inferior de aquella) con el corte por distancia ya existente (`stamp_corner`), radio 8, fondo del tema | — |
| A-08 | `aura_transitions.c:97,197` | doc-D §6 / CLAUDE.md: "Toda animación se detiene con la pantalla dormida (`lcd_active()`)" | Media | `aura_transition_slide/reveal` solo consultan `animation_mode`; la puerta central de `aura_main.c:129` no las cubre (corren dentro del manejo de botón) | `if (!lcd_active()) return;` junto al chequeo de `AURA_ANIM_NONE` en ambas | — |
| A-09 | `aura_screens.c:379-380,515-517` | doc-C §1 FULL-COLD ("sin memoria") + comentario propio del código que promete lo contrario | Media | `s_about_last_screen` se asigna a `SETTINGS_ABOUT` y **nunca** vuelve a otro valor: el "reinicia solo al ENTRAR de nuevo" funciona una única vez por proceso; después la página persiste entre visitas | Resetear `s_about_page` al salir (rama `BUTTON_MENU` de `handle_about`) y retirar la variable centinela | — |
| A-10 | `aura_screens.c:507` (`rbversion` crudo) | doc-D Principio 7 (jerga técnica) | Media | Página 3 de Acerca de muestra `28a30450a8M-260811` (hash git + fecha) — `auditoria01/about-p3-light.png` | Mostrar versión propia de Aura (p.ej. del manifest de Studio o un `AURA_VERSION` propio); el destino de la línea "Basado en Rockbox" es decisión aparte (Ambigüedad A-c) | — |
| A-11 | `aura_coverflow.c:297-298` | doc-D Principio 1 ("nada de marcos, biseles") + Principio 2 (acento jamás decoración) | Media | Marco `ACCENT` de 2px alrededor de la carátula central — `matrix/music_albums-dark-all.png` | Quitarlo: la central ya se distingue por brillo (255 vs 130), tamaño y perspectiva, como en cualquier coverflow real | — |
| A-12 | `aura_coverflow.c:239-243` | doc-D Principio 3 (esperas = cápsula flotante, no página) | Media | "Preparando la biblioteca..." centrado en página completa — el mismo anti-patrón que D-073 ya corrigió en `draw_music_browse()` con `draw_waiting_state()`, no replicado aquí | Usar la cápsula (`aura_widgets_draw_wait_capsule`) también en coverflow | — |
| A-13 | `aura_coverflow.c:319-333` (`scroll_step`) | doc-D §7 (aceleración v² con velocidad real) + plan Fase 29 ("módulo único consumido por listas, **coverflow** y scrub") | Media | Coverflow conserva la heurística vieja de doble-evento (HZ/6 → paso 2); las listas ya usan `aura_wheel_step()` con la velocidad real del driver | Migrar `scroll_step` a `aura_wheel_step(aura_main_wheel_velocity())` | Fase 29 lo pedía; D-077 no lo anotó como pendiente |
| A-14 | `aura_nowplaying.c:544-552` | doc-R §5 fila 5.1 ("mismo tratamiento que §5.2 del doc base") + doc-D §5.2 | Media | Barra del overlay de volumen: carril `lcd_drawrect` (delineado, no relleno), relleno `lcd_fillrect` a bordes vivos, color `ACCENT` — `auditoria01/nowplaying-volumeoverlay-dark.png` | Misma pieza que el resto: carril `PROGRESS_TRACK` relleno + `PROGRESS_FILL`, extremos redondeados vía `a26_shell_fill_rounded_rect` (radio = alto/2). El color bajo edición es la Ambigüedad A-f | — |
| A-15 | `aura_photos.c:99-104`, `aura_video.c:76-85` | doc-D §5 ("La barra nunca cambia de forma entre pantallas") | Media | Estados vacíos de Fotos y Videos dibujan solo el texto, sin `aura_statusbar_draw()` — únicas pantallas del sistema sin barra (el resto usa `draw_message_centered`, que sí la dibuja) | Pasar ambos por `draw_message_centered()` o añadir la barra | — |
| A-16 | `aura_nowplaying.c:261-273,318-324` | doc-R §4 (vacías `SHELL_RAIL`) y §5 (inactivos `TEXT_TERTIARY`) | Media | Estrellas vacías e iconos de modo inactivos van en tinta primaria — hoy desviación documentada (D-078, límite D-010), pero el fix de A-01 la vuelve trivialmente corregible | Generar variantes `-tertiary` y `-rail` en el pipeline nuevo y usarlas aquí | D-078 la documenta; el destrabe es nuevo |
| A-17 | `aura_widgets.c:464-475` + doc-D §5.1-bis | §5.1-bis: "Sin animación de deslizamiento propia **todavía (Fase 28... la añade)**" | Media | Fase 28 pasó (D-076) sin añadirla y sin anotarla como diferida — el círculo salta de lado sin transición | Deslizamiento de ~150ms del círculo con `aura_motion_linear` (es un control: admite el resorte corto si se prefiere — decidir en Fase B, es un valor ya tabulado) | — (vacío no documentado) |
| A-18 | `aura_screens.c:1258-1264` (`handle_playlists`) | doc-D §7 (la aceleración aplica a todas las listas) | Media | Único manejador de lista que avanza ±1 fijo en vez de `wheel_advance()` | Usar `wheel_advance(sel, s_playlist_cache_count, ±1)` | — |
| A-19 | `aura_statusbar.c:81-91` | doc-D §5 ("lo lleva la **tarjeta de reproducción del panel derecho**, centrado sobre la barra de progreso") | Media | La barra dividida omite ▶/⏸ (correcto), pero la tarjeta que el doc nombra como su reemplazo sigue sin existir — D-073 la mandó a Fase 30 y D-078 no la construyó ni la re-anotó | Construir la tarjeta (mini-progreso + ▶/⏸ en el panel derecho mientras suena algo) o re-documentar el vacío con fase dueña | Se cayó entre D-073 y D-078 |
| A-20 | `aura_widgets.c:752-774` + convttf | doc-D §5.2 (cápsula de 12px) vs texto de 13px (caja de glifo 14px, `.fnt` verificada) | Media | El texto de la cápsula de espera se centra con `(12-14)/2 = -1`: la tinta pisa el borde superior e inferior de la cápsula | Decidir geometría (Ambigüedad A-d): texto micro 7px dentro de 12px, o cápsula de 20px (radio 10) manteniendo el cuerpo 13px | — |
| A-21 | `aura_statusbar.c:124` | doc-D §5 (alineaciones del sistema; inset de listas 16px) | Cosmética | Título de barra en x=12 (`A26_SPACING_LG`); las filas de lista arrancan en 16 — el título queda 4px fuera de la vertical del contenido | `A26_LAYOUT_LIST_INSET` como inset del título | — |
| A-22 | `aura_screens.c:396-400` y `aura_widgets.c:568-570` | doc-D §5.4 (ningún radio suelto; nada de cajas vivas) + anti-patrón §8 radios inconsistentes | Cosmética | Puntos de página de Acerca de = `lcd_fillrect` 4×4 cuadrados (`auditoria01/about-p3-light.png`); cajas del selector de dígitos = rectángulos vivos (sin consumidor hoy — `aura_widgets_draw_digits` no tiene llamadores) | Puntos: `a26_shell_fill_rounded_rect` radio 2 (círculo). Dígitos: radio de pastilla al construir su primer consumidor (Fecha y hora, diferida D-060) | Dígitos: D-060 |
| A-23 | `aura_widgets.c:712` (`draw_progress`) | higiene (D-073 la reescribió "para Fase 30"; Fase 30 no la usó) | Cosmética | Cero llamadores fuera de widgets.c/h — código muerto por segunda vez | Conectarla (es la pieza que A-14 necesita) o retirarla | — |
| A-24 | `aura_lang.c:77`, `aura_splash_lang.c` | doc-D Principio 7 (los ejemplos del doc usan "…") | Cosmética | "Preparando la biblioteca..." y splash usan tres puntos ASCII | Unificar a "…" (un solo glifo, ya cubierto por la fuente) | — |
| A-25 | `aura_nowplaying.c:373-376` | doc-R §2 ("con tiempos **a los lados**") | Cosmética | Los tiempos van encima de la barra, no a los costados | Moverlos a los flancos de la pastilla, o documentar la variante si se prefiere así | — |
| A-26 | `tokens.json:69-74` | higiene de assets (doc-D §3: dos pesos por rol) | Cosmética | `caption` duplica `body` (misma cara, mismo tamaño → dos `.fnt` idénticas instaladas) | Colapsar a un estilo o darle a caption un rol real (p.ej. el Semibold 13 de A-04) | — |

### Lista exhaustiva de cadenas ES con defecto (para A-02)

`aura_lang.c` — línea: texto actual → corrección:
5 "Musica"→"Música" · 13 "Graficos"→"Gráficos" · 27 "Limite
volumen"→"Límite volumen" · 29 "Menu principal"→"Menú principal" · 38
"fabrica"→"fábrica" · 45 "Minimas"→"Mínimas" · 49 "Minimos"→"Mínimos" ·
57 "Espanol"→"Español" · 58 "Ingles"→"Inglés" · 61 "Musica"→"Música" · 65
"Todavia no sincronizaste con Aura Studio."→"Aún no has sincronizado con
Aura Studio." · 67 "Sin musica todavia"→"Sin música todavía" · 73
"Albumes"→"Álbumes" · 76 "Generos"→"Géneros" · 81 "Si"→"Sí".
`aura_splash_lang.c`: "Bateria baja"→"Batería baja" · "Bateria
agotada"→"Batería agotada".
(El cambio es de **valor**, no de orden: no altera la disciplina
"solo-añadir-al-final" de D-013, que aplica a los IDs del enum.)

---

## 2. Ambigüedades del documento — para tu revisión, ninguna resuelta en código

**A-a. Tamaños de título fantasma.** doc-D §3 solo define 13px (Compact) y
7px (Pro) + "SF Pro Display para relojes"; pero §5 razona la línea base de
la barra con "título **16 px** → y=0; reloj completo **14 px** → y=2", dos
tamaños que no existen en el sistema, y `tokens.json` define `title: 20px`
que §3 tampoco menciona. El código ya optó por centrado por altura medida
(`aura_statusbar.c:116-123`, comentario honesto: los números del doc "no
calzan"). *Propuesta*: título de barra = 13px **Semibold** (cierra A-04),
conservar 20px Semibold como estilo de números grandes documentándolo en
§3, y reescribir §5 en términos de "misma línea base por centrado medido
dentro de la franja de 20px" en vez de compensaciones por fuente.

**A-b. Símbolos `.fill` sin excepción documentada.** `tokens.json:129-130`
usa `backward.fill`/`forward.fill` (transporte de Ahora suena) y
`tokens.json:111` usa `circle.lefthalf.filled` (fila Tema) — doc-D §4 solo
exceptúa `star.fill`. Además la fila de transporte quedó mixta: retroceso y
avance rellenos, play/pausa lineales (`aura_nowplaying.c:410-415`).
*Propuesta*: documentar en §4 la excepción "controles de transporte y
símbolos cuyo significado ES el relleno (mitad claro/oscuro)" y unificar la
fila completa a rellenos (`play.fill`/`pause.fill`), que es lo que hace
Apple en todo transporte. Alternativa coherente: todo lineal. Queda a tu criterio.

**A-c. "Basado en Rockbox" en Acerca de vs anti-patrón "cromo de
Rockbox".** `aura_lang.c:60` + `aura_screens.c:504` muestran la atribución
en la página 3 (`auditoria01/about-p3-*.png`). El anti-patrón §8 dice
"logotipo o cromo de Rockbox visible en cualquier estado", sin excepción
escrita. La atribución en una página de información es práctica correcta
con un proyecto GPL (y el original listaba info del dispositivo ahí).
*Propuesta*: conservarla exactamente ahí y documentar la excepción en §8
("la atribución textual en Acerca de no es cromo"); lo que sí sale es el
hash crudo (A-10). Alternativa: quitar también la atribución.

**A-d. Cápsula de espera de 12px vs cuerpo de 13px.** La geometría de
§5.2 (alto 12) no admite el único texto que hoy vive dentro (A-20).
*Propuesta*: cápsula a 20px de alto (radio 10, concéntrico, sigue siendo
discreta a y=alto−22) conservando cuerpo 13px legible. Alternativa: texto
en micro 7px conservando los 12px del doc — más fiel, menos legible.

**A-e. Bordes de foto "negros" (doc-C §3.4) vs fondo del tema.** El visor
(`aura_photo_viewer_draw`) rellena con `SHELL_BG`: en claro, las franjas
laterales son blancas. El doc describe bordes negros (pantalla inmersiva).
*Propuesta*: negro real en el visor de fotos — es contenido inmersivo, no
una pantalla de UI; el "fondo de todo" de §2 aplica a la shell, no al
lienzo de una foto. Nota: hoy el visor tampoco ofrece el segundo modo
(recorte a pantalla completa con paneo) que §3.4 describe — sin fase dueña
anotada; decidir si entra al plan o se documenta fuera de alcance.

**A-f. ¿Acento en barras bajo edición?** `apple2026_shell.h:15-17` declara
el acento para "relleno de progreso/sliders **bajo edición**"; D-081 quitó
el acento del slider de Brillo/Límite; el overlay de volumen (A-14) aún lo
usa. Dos criterios conviviendo. *Propuesta*: `PROGRESS_FILL` siempre para
el relleno de progreso/nivel, acento reservado a iconos/estado (coherente
con D-081), y corregir el comentario de `apple2026_shell.h`. Alternativa:
acento mientras la rueda edita (lectura Principio 2), aplicado entonces
TAMBIÉN a Brillo/Límite — revirtiendo la mitad de D-081.

**A-g. "8 px respecto al inset de 16 px" (§5.1).** Redacción ambigua
(¿16−8=8 o 16+8=24?). El código eligió x=8 — pastilla más ancha que el
texto, sin tocar bordes (`aura_widgets.c:38`), que es la única lectura
compatible con "nunca toca el borde de la lista". *Propuesta*: reescribir
la frase del doc fijando "margen lateral de 8px respecto al borde de la
columna de lista". Sin cambio de código.

**A-h. Paso de scrub 3 s/click.** `aura_nowplaying.c:668` lo comenta como
"doc SS5.2", pero doc-R §5.2 no define ningún valor. Es razonable;
*propuesta*: documentarlo en doc-R §5 (con su aceleración v² pendiente de
A-13/§7 si se quiere). Sin cambio de código.

**A-i. "Las dos tiras" del pedido de corrección.** Hoy no existen
`Apple2026Icons.bmp`/`...Dark.bmp` como archivos únicos: doc-D §4 dejó el
empaquetado en tiras "diferido, sin fecha" y el layout real es un BMP por
icono/tamaño/variante en `out/icons/<tema>/`. *Propuesta*: aplicar la
composición del punto 0 al layout actual (los dos SETS por tema, cada uno
contra su fondo — el espíritu del requisito) y dejar el empaquetado físico
en tiras como estaba, diferido. Si quieres el empaquetado real ahora, es
alcance extra de Fase B: dilo explícito.

---

## 3. Desviaciones deliberadas ya justificadas en DECISIONS.md — vistas, no tocadas

- **D-010/D-075** — transparencia binaria + supersampleo 8×: era la mejor
  respuesta dentro del diagnóstico de entonces; **superada por tu
  corrección del punto 0** (que la reemplaza sin tocar el mecanismo C).
- **D-025 / D-057** — Cover Flow vive como el modo "Todos" de Álbumes (no
  como entrada propia del menú Música, doc-C §4.1) y la pantalla dividida
  se generalizó a casi toda lista. La generalización es anterior al doc-C
  actual: la parte "nivel 3+ también split" pasó a ser el hallazgo A-03; el
  resto (split como mecánica L2, retiro de `aura_home.c`) sigue vigente.
- **D-058** — entrada a coverflow con T4 (revelado desde ambos bordes)
  como aproximación de la cortina 2.5(b); la cortina real con barra
  cayendo desde arriba pertenece a 31.3 (hardware, D-079/D-080).
- **D-062 / D-029** — video delegado en mpegplayer con OSD propio.
- **D-073** — diferidos con razón: contenedor modal genérico, riel A-Z
  indexado (sin lista consumidora), páginas de símbolo 96×96 (los estados
  vacíos siguen como texto centrado), teclado de búsqueda (la pantalla no
  existe).
- **D-076** — diferidos: transición FULL-CARRY (por eso Brillo/Límite
  volumen entran hoy con push plano — el vacío de transición ESTÁ
  documentado ahí, aunque el heading de Fase 32 pueda leerse como si la
  taxonomía hubiera quedado cerrada), deriva ambiental de carátulas
  (doc-C §3.1), motor v2 dirigido por taxonomía.
- **D-077** — diferidos: Quickscreen (falta el gesto "hold" en el
  vocabulario de botones), salto por letras >420°/s (helper listo y
  testeado, sin lista indexada consumidora), sombra del arte por campo de
  distancia (§5.4).
- **D-078** — simplificaciones de Ahora suena: vista de letra sin el panel
  comprimido FULL-CARRY, sin crossfade de carátula+reflejo, sin listas
  automáticas "N estrellas", variantes de tinta limitadas (ver A-16 para
  el destrabe).
- **D-079/D-080** — modal 2.6 (giro de carátula) y medición de rendimiento
  en ARM: sesión guiada con hardware pendiente.
- **D-081** — Acerca de sin categoría "Otros" (exigiría API de disco
  nueva), Límite volumen sin la flecha del original (la barra comunica lo
  mismo), EQ sin icono de curva por preset (exige callback de dibujo por
  fila en el widget de lista).

Fuera de alcance declarado (plan §32 y checklist Fase 32): Podcasts,
Extras completo, Salida TV, búsqueda, Recopilaciones/Autores — secciones
del original que Aura no tiene; construirlas es fase futura propia.

---

## 4. Falsos positivos descartados — el razonamiento, no el silencio

1. **`SHELL_RAIL` como relleno de superficies** (chips no seleccionados,
   pista de switch inactiva). El §2 lo describe solo como
   "separadores/bordes finos", pero §5.1-bis lo usa explícitamente como
   relleno de pista inactiva — el propio doc ya lo emplea como superficie
   neutra; `apple2026_shell.h:19-23` documenta la lectura. No es violación.
2. **Cápsula §5.2 sin la tabla de retranqueos {4,2,1,1,0,0}**: la
   primitiva de corte por distancia produce el mismo resultado visual con
   una sola implementación (D-073). Equivalencia, no desvío.
3. **PLAY global activo dentro de Acerca de** contradice doc-C §4.6
   ("play/pausa no hacen nada"): gana doc-D §7 ("PLAY en cualquier lista es
   reproducir/pausar global") — doc-C es base empírica, doc-D es normativo.
4. **"Video" en singular** en los contadores de Acerca de: coincide con la
   categoría del original (doc-C §4.6: "Audio, Video, Fotos, Otros").
5. **Los 3 `LCD_RGBPACK` en código C** (`aura_coverflow.c:126`,
   `aura_art.c:37`, `apple2026_shell.c:181`): re-empaquetado tras
   interpolar dos colores que YA salieron de tokens — blends legítimos, no
   RGB hardcodeado (auditados también en D-081).
6. **Radio de barra de deslizamiento 1px vs "1.5px" del §5.3**: en una
   cápsula de 3px de grosor con corte entero, 1 es la única aproximación —
   no hay medio píxel.
7. **Checkmark en listas de elección única** (Tema/EQ/Idioma/valores
   numéricos): §5.1-bis reserva el switch para booleanas y conserva el
   checkmark para pertenencia — marcar "el valor vigente" es la mecánica
   del original y no es una fila booleana.
8. **Menú Música sin iconos por fila**: el original tampoco los tiene y tu
   propia instrucción fue "no todos los menús lo van a necesitar igual".
   Se deja como está salvo pedido explícito.
9. **"Aura" como título del menú raíz** en vez de "iPod": marca del
   producto, no cromo.
10. **La barra dividida no muestra ▶/⏸**: es exactamente lo que §5 manda;
    el hallazgo real es que la tarjeta sustituta no existe (A-19).
11. **Los iconos no cambian de color al pasar la pastilla por encima
    durante el resorte**: bitmaps pre-horneados (D-010) — el color del
    contenido cambia al asentarse la selección, límite conocido y
    documentado; con A-01 el borde se verá limpio en tránsito.
12. **`draw_about` sin logo**: doc-C describe "logo de Apple centrado" —
    reproducir el logo de Apple sería impostura de marca; la página de
    versión propia es la adaptación correcta.

---

## 5. Capturas de evidencia

**Matriz completa** (regenerada hoy desde `912abe0`, 0 fallos): por cada
pantalla existen 6 archivos —
`docs/screenshots/matrix/<pantalla>-{light,dark}-{none,minimal,all}.png` —
para las 30 pantallas:

`root`, `music`, `music_artists`, `music_albums`, `music_albums_by_artist`,
`music_songs`, `music_songs_by_album`, `music_songs_by_genre`,
`music_genres`, `music_playlists`, `videos`, `photos`, `photo_viewer`,
`nowplaying`, `settings`, `settings_theme`, `settings_animations`,
`settings_graphics`, `settings_eq`, `settings_brightness`,
`settings_shuffle`, `settings_repeat`, `settings_backlight`,
`settings_sleeptimer`, `settings_volume_limit`, `settings_clicker`,
`settings_mainmenu`, `settings_language`, `settings_about`,
`settings_reset` — 180 PNG en total.

**Capturas dirigidas de esta auditoría** (estados que la matriz no
retrata), en `docs/screenshots/auditoria01/`:

- Acerca de — página 2 (contadores): `about-p2-light.png`
- Acerca de — página 3 (hash crudo, atribución, puntos cuadrados):
  `about-p3-light.png`, `about-p3-dark.png`
- Ahora suena — modo Playlist con estado neutro "Gira la rueda para
  elegir": `nowplaying-playlistmode-light.png`
- Ahora suena — overlay de volumen (bordes vivos + acento, A-14):
  `nowplaying-volumeoverlay-dark.png`

Estados auditados por código que el arnés actual **no puede retratar**
(quedan para verificación visual durante la Fase B, con sus fixes):
cápsula de espera con texto (A-20 — exige base de datos a medio escanear),
pastilla en tránsito de resorte, fundido de barra de deslizamiento, switch
conmutando, transiciones en vuelo.

---

## 6. Orden propuesto de lotes para Fase B (un lote = un commit) — sujeto a tu aprobación

1. **Lote 1 (Bloqueante A-01)**: pipeline de iconos con composición por
   cobertura + verificación mecánica de rampa + regeneración de ambos sets
   (+ variantes `-tertiary`/`-rail` si aprobás A-16 junto).
2. **Lote 2 (Bloqueante A-02)**: tabla ES/splash con ortografía correcta +
   captura de verificación de glifos.
3. **Lote 3 (doc)**: actualizar doc-D §5.1 + Principio 2 con tu decisión de
   selección en acento; más las ambigüedades que resuelvas de la sección 2
   (cada una toca su § correspondiente).
4. **Lote 4 (Altas A-03…A-07)**: profundidad/taxonomía, Semibold de
   títulos, Fotos/Videos (nombres+iconos), radio de carátula.
5. **Lote 5 (Medias A-08…A-20)** agrupadas por módulo.
6. **Lote 6 (Cosméticas A-21…A-26)**.

**Fin de la Fase A.** No se corrigió nada; espero tu revisión del reporte
y tus respuestas a las ambigüedades A-a…A-i antes de tocar un solo archivo.
