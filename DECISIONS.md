# DECISIONS.md — Registro de decisiones técnicas (Aura Firmware)

> Continuación de **[`DECISIONS-ARCHIVE.md`](DECISIONS-ARCHIVE.md)**
> (D-001…D-285, bitácora congelada del monorepo original). Este
> archivo registra las decisiones tomadas **en este repositorio**
> desde la separación del 2026-08-16, con numeración **D-286** en
> adelante. Las decisiones equivalentes de Aura Studio viven en el
> `DECISIONS.md` de su propio repositorio, numeradas **ST-001** en
> adelante — sin coordinación de números entre ambas series. Una
> referencia cruzada `ST-NNN` apunta siempre al repositorio de Aura
> Studio.
>
> ⚠️ **Esto es una BITÁCORA, no una spec.** La única fuente de verdad
> del diseño vigente es `docs/aura-design-system/`; para el estado del
> código, el propio código. No tomes ninguna entrada vieja de aquí (ni
> del archivo) como comportamiento actual sin verificarla.

---

## D-286 — Tema compilado por defecto vuelve a Inter + Lucide/Phosphor, revive D-004, sustituye a SF Pro/SF Symbols (D-072) antes de hacer público el firmware

**Encargo**: preparar este repositorio (`Aura-Firmware`, separado de `Aura-Proyect` el 2026-08-16, ver cabecera de este archivo) para publicación pública. El tema compilado por defecto usaba SF Pro (tipografía) y SF Symbols (iconografía) desde D-072 (Fase 26, "Apple2026") — ambos redistribuidos como binarios/bitmaps horneados dentro de `firmware/dist/*.zip` en el historial de git, lo cual viola la licencia de Apple (D-004 ya lo dejaba explícito: "Prohibido: SF Pro y SF Symbols en el firmware... la licencia de Apple prohíbe su redistribución fuera de plataformas Apple" — una regla que D-069/D-072 pasaron por alto en la práctica sin revertir el texto de D-004). Antes de este repositorio existir públicamente había que corregirlo.

**Decisión**: el tema por defecto compilado vuelve a **Inter** (SIL OFL) + **Lucide** (ISC), con **Phosphor** (MIT) como set secundario para los pocos íconos sin equivalente directo en Lucide — exactamente la elección original de D-004, ahora repuesta. El pipeline de generación de fuentes/íconos (`design-system/generate.py`, `convttf`, la máscara de cobertura antialiasada) no cambió: solo cambiaron las fuentes de artwork.

**Tipografía — 3 caras Inter cubren 6 roles abstractos**: se vendorizó `design-system/vendor/inter-ttf/{Inter-Regular,Inter-Medium,Inter-SemiBold}.ttf`. Los 6 `faces` abstractos de `tokens.json` (`reg`/`medium`/`semibold`/`bold`/`pro_bold`/`pro_semibold`) se reapuntaron a esas 3 caras — no hay un Bold real vendorizado, así que `pro_bold` y `pro_semibold` comparten `Inter-SemiBold.ttf` (simplificación pragmática, documentada en `tokens.json` como `comment_faces`, y en `docs/aura-design-system/fundamentos/02-tipografia.md`). Los tamaños de `type_scale` (los px reales de cada `.fnt`) **no se tocaron** — se mantienen los mismos valores afinados visualmente contra SF, verificados ahora contra Inter con capturas (`docs/screenshots/theme-default-inter-lucide/`). `resolve_font_file()` (`generate.py`) tenía un bug real de resolución de rutas relativas en `search_paths` (solo funcionaban absolutas o `~`) — corregido para resolver contra `ROOT` (`design-system/`), necesario para que `vendor/inter-ttf` funcionara sin ruta absoluta hardcodeada.

**Iconografía — mapa completo de 87 icon_key, cobertura real** (`design-system/assets/icon-name-map.json`, consultado por `tokens.json`→`icon.svg_overrides`, que pasó de 2 a 89 entradas — las 2 previas, `music`/`ipod`, son artwork propio de D-263 y no se tocaron): **62 Lucide, 4 Phosphor, 21 reuso** (un icon_key existente cuya forma ya servía para el nuevo rol, sin SVG nuevo — razón documentada por entrada, ninguno es un placeholder vacío ni una aproximación oculta). El campo `icon.names` (mapeo a nombres de SF Symbols) **se conserva intacto**, ya no como fuente activa del pipeline sino como documentación para el futuro tema opcional "Apple (uso personal)" (ver `PLAN-theme-system.md`) — `generate.py` solo lo consulta si `svg_overrides` no tiene entrada, lo cual hoy nunca ocurre. `apple2026_sf_render.swift` (el renderizador que pide símbolos a AppKit) se conserva sin cambios de comportamiento, con un comentario nuevo que lo repositiona como el futuro constructor de ese tema opcional — no corre como parte de `build_sim.sh`/`package_dist.sh` de este repositorio.

**Normalización de SVG vendorizados**: los Lucide son de trazo (`stroke="currentColor"`, `fill="none"`), los Phosphor de relleno (`fill="currentColor"`) — `currentColor` no siempre resuelve bajo `NSImage(contentsOfFile:)` (el mismo cargador que ya usaba `apple2026_sf_render.swift` para los SVG propios de D-263), así que se reemplazó `currentColor` → `#000000` en los 46 SVG nuevos y también en los 19 ya vendorizados de D-004/D-263 (el color real lo decide `generate.py` en la composición, igual que con los SF Symbols antes).

**Dos bugs reales encontrados verificando, no en revisión de código**: (1) `svg_overrides`/`icon-name-map.json` guardaron primero las rutas relativas al repo (`design-system/vendor/...`) en vez de relativas a `design-system/` — la convención que ya usaban `music`/`ipod` — y `generate.py` hace `(ROOT / v).resolve()`, así que la ruta se duplicaba (`design-system/design-system/vendor/...`) y el build fallaba fuerte ("no se pudo cargar el SVG"); corregido quitando el prefijo en las 87 entradas nuevas. (2) el chequeo mecánico de generación `MIN_INK_TONES = 4` (cuenta tonos RGB distintos por BMP horneado, atrapa regresiones de binarización) marcó 81 íconos fallando, los 81 rastreados a 3 SVG fuente reusados en muchos icon_key: `pause.svg` (Lucide) resultó ser un **bug visual real**, no solo de la métrica — estaba dibujado como contorno hueco (`fill="none"` + `stroke`), así que las barras de pausa se habrían visto vacías; reescrito como dos rectángulos rellenos con esquinas redondeadas. `sliders-horizontal.svg`/`menu-list.svg` tenían trazos de 2px con muy poca superficie curva para generar antialiasing a tamaños pequeños; engrosados a `stroke-width="3"`. Arreglados esos 3 archivos fuente y reconstruido, la falla bajó a 20 (todas variantes de `pause`, cuyo `rx="1"` seguía siendo insuficiente) y luego, tras subir `rx` a `2.5` (extremos semicirculares completos), a **0**. El umbral `MIN_INK_TONES` en sí no se tocó — todas las correcciones fueron al artwork fuente, no al chequeo.

**Verificación**: `build_sim.sh` limpio; build ARM limpio (solo el warning preexistente no relacionado, `-Wtype-limits` en `apple2026_shell.c:48`); `make -C firmware/rockbox/apps/aura/test test` 8/8. Capturas en `docs/screenshots/theme-default-inter-lucide/` (menú raíz, lista de Ajustes, selector de Tema, lista de Música, Acerca de expandido) confirman texto e íconos legibles y sin binarización rota — no se persiguió pixel-perfección contra el tema SF anterior, el objetivo era una migración de licencia sin regresión visual. Doc actualizada: `docs/aura-design-system/fundamentos/02-tipografia.md` (distinción rol-vs-cara explicada, Inter como cara compilada por defecto, SF disponible solo vía el tema opcional futuro) y notas cruzadas cortas en `componentes/{index-rail,status-bar,left-panel,now-playing,selection-summary}.md`.

## D-287 — Reparto de contexto: `CLAUDE.md`, bitácora y contrato con Aura Studio antes de crear el repo hermano

**Encargo**: `Aura-Firmware` ya existía, privado y verificado (D-286), pero el contexto operativo (reglas de `CLAUDE.md`, la bitácora `DECISIONS.md`, la skill de diseño, la documentación cruzada con Aura Studio) seguía sin repartirse formalmente — `PLAN-context-split.md` (en el repo archivado `Aura-Proyect`) lo audita y propone el reparto; esta entrada registra lo ejecutado en este repositorio tras su aprobación.

**Bitácora**: el `DECISIONS.md` heredado de la separación (D-286) traía las D-001…D-285 íntegras seguidas de D-286 en un solo archivo. Se separó en `DECISIONS-ARCHIVE.md` (D-001…D-285, con cabecera neutra, pensada para ser **byte-idéntica** a la copia que recibe Aura Studio) y este `DECISIONS.md` vivo, que arranca en D-286 (movida sin editar) y continúa aquí. Motivo: si el corte fuera en D-286 (como decía la cabecera anterior), las copias de ambos repos ya no coincidirían byte a byte el día uno.

**`CLAUDE.md`**: se añadieron reglas explícitas que ya se seguían de facto pero no estaban escritas — que ningún script o generador de este repo asuma la existencia de un checkout hermano de Aura Studio (la clase de acoplamiento que ya causó el bug de `studio/` huérfano en `generate.py`, corregido antes de la separación), la disciplina GPL v2 (cabecera de licencia + `MODIFICATIONS.md` en la misma pasada que toque un archivo de Rockbox), la prohibición de material de Apple en el árbol o en `dist/`, la numeración de la bitácora (D-287+, referencias a Studio como `ST-NNN`), y español de México sin voseo en todo texto de cara al usuario y documentación (ya vivía en la memoria de Claude del monorepo, nunca escrita en un `CLAUDE.md`).

**`generate.py` deja de detectar al hermano**: la detección automática de `../studio` (introducida en la separación para no crear el directorio huérfano) se reemplazó por un flag explícito `--swift-out RUTA` — sin el flag, no se genera ningún `AuraPalette.swift`. `firmware/tools/package_dist.sh` lo invoca apuntando a `firmware/dist/`, así el Swift generado viaja como asset del Release (ver `CONTRATO-firmware-studio.md`) en vez de escribirse directo a un checkout hermano.

**`CONTRATO-firmware-studio.md`** (nuevo, copia idéntica en Aura Studio): fija que Studio consume artefactos **solo por Release** (nunca por ruta relativa a este árbol) — `rockbox.ipod`, `rockbox.zip`, `bootloader-ipod6g.ipod`, `mks5lboot`, `checksums.txt`, `AuraPalette.swift`, `MODIFICATIONS.md` — con verificación de checksums; documenta el cumplimiento GPL que le corresponde a Studio por embeber esos binarios; describe el contrato de datos en el disco del iPod (`.rockbox/aura/*.cfg`, `sync_manifest.json`, `Playlists/`, layouts de `Music/`) que sí sobrevive por diseño, con quién escribe y quién lee cada archivo; y la compatibilidad de versiones (`FIRMWARE_VERSION` que Studio fija por release).

**Documentación**: `docs/guia-instalacion.md` (guía de usuario final de Aura Studio) se retiró de este repositorio — vive solo en Aura Studio. `docs/guia-desarrollo.md` perdió la sección "Aura Studio (macOS)" y la línea `studio/AuraStudio/` de "Estructura del repo". `docs/guia-flasheo-restauracion.md` se recortó a la parte técnica (qué instala, DFU/mks5lboot, restauración) — la sección "qué hace Aura Studio por ti" queda como referencia por URL al repo de Studio, no una copia del flujo. `firmware/dist/README.md` corrigió la cita cruzada ("repositorio de Aura Studio" ya es exacto, antes era ambiguo por venir del monorepo). Se corrigió voseo residual ("abrilo", "usás") en `guia-desarrollo.md`.

**Skill**: `apple2026-design-system/SKILL.md` describía su alcance como "el firmware y Aura Studio" — corregido a "el firmware", porque todos sus documentos fuente (`docs/aura-design-system/`, `docs/design/`, `design-system/tokens.json`, `apps/apple2026_shell.h`) viven únicamente aquí; Studio no recibió copia (lo que necesita del sistema de diseño llega compilado en `AuraPalette.swift` vía Release).

**Verificación**: `firmware/tools/build_sim.sh` limpio, build ARM limpio, `make -C firmware/rockbox/apps/aura/test test` 8/8 — sin regresión respecto a D-286 (ningún cambio de código C, solo de documentación y del flag de `generate.py`). `firmware/tools/package_dist.sh` corrido de punta a punta para producir el primer `dist/` con `AuraPalette.swift` incluido, que sirve de fuente local para poblar `Vendor/firmware-dist/` en Aura Studio (aún sin Release público — ver `ST-001`).

## D-288 — Bug preexistente encontrado investigando temas: `package_dist.sh` nunca empaquetaba fondos de panel ni tile-icons

**Encontrado** investigando la viabilidad del sistema de temas (D-289): `firmware/tools/package_dist.sh` buscaba `design-system/out/icons/{backgrounds,tile-icons}` para armar `rockbox.zip`, pero el pipeline (`generate.py`) los escribe en `design-system/out/icons/aura/{backgrounds,tile-icons}` desde que existe el subdirectorio `aura/` — la condición `[[ -d ... ]]` de `package_dist.sh` nunca encontraba nada y esas dos carpetas se omitían en silencio de **todo** release hecho con este script. `firmware/tools/build_sim.sh` no tiene este bug (usa la ruta con `/aura/` correcta) — por eso nunca se notó: el simulador siempre tuvo el fondo rosa del panel derecho y el badge de "Acerca de"; un iPod real instalado desde un `rockbox.zip` armado con `package_dist.sh` los habría tenido ausentes (el panel derecho sin fondo por preset, el badge cayendo al ícono `ipod` genérico).

**Corrección**: las dos rutas de `package_dist.sh` se apuntan a `out/icons/aura/{backgrounds,tile-icons}`. Verificado con un `package_dist.sh` de punta a punta: `unzip -l rockbox.zip` confirma `.rockbox/icons/aura/backgrounds/pink.bmp` y `.rockbox/icons/aura/tile-icons/aura_badge-{light,dark}.bmp` presentes.

## D-289 — Sistema de temas: paquetes instalables de fuentes+íconos+paleta, con fallback obligatorio al default

**Encargo**: aterrizar `PLAN-theme-system.md` (especificación del 2026-08-16) en una implementación real — investigación de viabilidad primero (bloqueante), luego el submenú "Estilo" en el firmware y el lado constructor/instalador en Aura Studio. Plan completo con el veredicto de viabilidad, el diseño de ambos lados, y las preguntas abiertas con recomendación: `PLAN-themes-impl.md`, aprobado con todas las recomendaciones.

**Veredicto de viabilidad (re-verificado contra el código, no solo la spec)**: viable, modelo híbrido — estructura (radios, espaciado, timings, tamaños de buffer) sigue compilada; apariencia (paleta, fuentes, íconos, fondos) pasa a runtime. Las 14 fuentes ya se cargaban desde disco por ruta (`font_load()` acepta cualquier ruta); la paleta pasaba 100% por `a26_color()` (cero usos directos de los defines de color fuera de `apple2026_shell.c`); los íconos ya se leían de disco en cada dibujo, con la **máscara de cobertura** (no el BMP horneado) como camino primario — esto último es el hallazgo que más cambió el formato: un tema v1 solo necesita las 801 máscaras (~5.2 MB), no los BMP horneados por variante/modo (~52 MB, quedan opcionales como fallback). Consecuencia feliz: eso además elimina por construcción la restricción heredada de antialias (halo por clave magenta, dientes de sierra por umbral binario) para cualquier constructor de temas — una máscara es la cobertura misma, sin paso de composición que pueda introducir esos bugs.

**Módulo nuevo `aura_style` (código, no confundir con `aura_settings.theme`, que sigue siendo claro/oscuro sin relación)**:
- `aura_style_manifest.{c,h}` — parseo puro del manifiesto `theme.cfg`, sin dependencias de Rockbox (mismo criterio que `aura_color.c`/`aura_motion.c`: compila igual en host que en firmware). Host-testeable: **9º test del arnés**, `test_style.c`, 57 checks — valida el formato, ids, hex con/sin `#`, claves desconocidas ignoradas en silencio, roles de paleta sin contaminarse entre sí.
- `aura_style.{c,h}` — el lado que sí toca disco/Rockbox: carga/descarga de las 14 fuentes (unload-y-reload, nunca "cargar al lado": `MAXUSERFONTS` está exactamente al límite, 14/14), tabla de paleta en runtime (`s_palette[2][9]`, indexada por los mismos ordinales de `a26_token_t`, con el slot de `A26_ACCENT` sin usar — el acento sigue siendo 100% del usuario), resolución de rutas de íconos con fallback por archivo al default (`aura_style_read_icon_bmp()`, un solo punto para los 5 sitios que antes construían la ruta a mano), `aura_style_scan()` (lista dinámica desde disco, plantilla `aura_music_list_playlists()`), `aura_style_activate()`/`aura_style_boot()` con reversión segura, `aura_style_delete()` (borrado recursivo propio con `opendir`/`readdir`/`remove`/`rmdir` — **no** vía `apps/fileop.c`, que pide confirmación con el diálogo propio de Rockbox, cromo que Aura no usa nunca).
- `apple2026_shell.c`: `a26_shell_init()` delega en `aura_style_boot()`; `a26_color()` delega en `aura_style_palette_color()` salvo `A26_ACCENT` (sigue resolviendo `aura_accent()` directo, sin pasar por la tabla — el acento no es del tema); `aura_category_gradient()` usa los 4 getters de color de categoría de `aura_style.c` en vez de los defines compilados.
- `aura_widgets.c` (2 sitios), `aura_selection_summary.c` (fondo de panel, ahora con la generación del estilo en la clave de caché — sin esto, cambiar de tema no invalidaba el buffer si el nombre pedido no cambiaba), `aura_screens.c` (badge de "Acerca de", misma corrección de caché; y los 2 usos directos de `AURA_DS_COLOR_CATEGORY_EXTRAS_YELLOW` en las barras de almacenamiento, ahora vía el estilo activo), `aura_albumart.c` (máscara de la nota por defecto) — los 5 sitios que construían una ruta de ícono a mano ahora pasan por `aura_style_read_icon_bmp()`.
- `aura_settings.{c,h}`: campo nuevo `style_id` (33 = 32+NUL); clave `theme_id` en `aura.cfg` (ese nombre, no `style_id`, es el que fija el contrato con Aura Studio); clave `theme_format_supported` (solo escritura, para que Studio la lea del iPod montado sin adivinar versión de firmware — hoy no existe ninguna versión visible en ningún lado del dispositivo).
- `aura_screens.c`: fila nueva **"Estilo"** en Ajustes → Apariencia, justo debajo de "Tema" (decisión sobre el encargo original, que proponía Extras — Extras agrupa utilidades/apps, no ajustes; el árbol documentado ya tenía un grupo "Apariencia" con Tema/Acento/Animaciones/Gráficos, y la propia `ExtrasView` de Aura Studio ya le decía al usuario "se eligen en Ajustes > Tema"). Ícono `sync` (reuso documentado, D-286/D-004: ninguno de los 89 `icon_key` existentes encaja mejor que los ya usados por las filas hermanas del mismo grupo, y agregar un ícono nuevo es fuera de alcance de esta pasada). `draw_style_list()`/`handle_style_list()` — misma maquinaria visual que las pantallas de elección existentes (`MenuList`+`Selector`, checkmark), pero con tabla dinámica leída del disco en cada entrada (no la infraestructura genérica de `is_choice_screen()`, que asume texto compilado vía `aura_str_id_t`). `AURA_SCREEN_SETTINGS_STYLE` agregado al final de los dos enums append-only (`aura_nav.h`, `aura_lang.h`), mismo criterio "solo-añadir-al-final" que el resto de esos archivos. `aura_category_for_screen()` la suma al grupo de Apariencia (misma categoría que "Tema").

**Bug real encontrado y corregido durante la verificación en el simulador** (no de diseño, de implementación): la primera versión de la activación descargaba las 14 fuentes actuales una por una con `font_unload(font_ids[i])` antes de cargar el candidato — en la práctica, un slot (el id 0, "font 0 must be available at system startup" según el propio comentario de `font.h`) quedaba sin liberar (refcount > 1 por algo ajeno a Aura que también lo usa), así que la fuente número 14 del candidato siempre fallaba por falta de cupo, y `try_activate()` revertía silenciosamente sin que se notara nada raro salvo, con captura de pantalla, que el estilo "aplicado" en realidad nunca cambiaba de verdad. Cambiar a `font_unload_all()` (la función que ya trae `firmware/font.c`, que fuerza el refcount de cada slot a 1 antes de liberarlo) lo corrigió — verificado con un tema de prueba real en el simulador, paleta y fuentes cambiando correctamente en caliente y persistiendo entre arranques.

**Verificación**: `build_sim.sh` limpio (cero warnings nuevos — dos ajustes de tamaño de buffer en `aura_style.c` evitan sendos `-Wformat-truncation` que `gcc` marcaba por no poder probar estáticamente que los ids ya validados caben; el único `-Wtype-limits` que queda en `aura_style.c` es el mismo cheque de `a26_font()` que ya existía en `apple2026_shell.c` antes de D-286 — se movió de archivo junto con la función, no es nuevo). Build ARM limpio (`package_dist.sh` de punta a punta, con el toolchain compilado en esta misma sesión). `make -C firmware/rockbox/apps/aura/test test` → **9/9** (938 checks, el 9º es `test_style`). Probado en el simulador con un tema de prueba real (paleta claro invertida a azul marino, category_video/category_photos intercambiados, solo máscaras — sin BMP horneados, para verificar que de verdad son opcionales): aplica en caliente, persiste entre arranques, revierte limpio. Fallback verificado con dos temas rotos a propósito (`theme_format: 99`, y uno con `theme_format` válido pero solo 1 de 14 fuentes) — ambos aparecen como fila inerte en "Estilo", y forzar el arranque con cualquiera de los dos como activo cae al default sin corrupción visual ni cuelgue. Capturas en `docs/screenshots/themes/`.

**Assets nuevos del Release** (`package_dist.sh`): `theme-format-v1.json` (generado por `generate.py`, contrato de formato para que Aura Studio construya/valide sin leer `tokens.json`) y `aura-theme-default.zip` (el default reempaquetado como tema instalable libre, id `aura` — ejemplo canónico del formato). `CONTRATO-formato-tema.md` nuevo (copia idéntica en Aura Studio); `CONTRATO-firmware-studio.md` sube a v2.

**Queda pendiente, documentado, no a medias**: `accent_default`/`accent_presets` del manifiesto se aceptan y validan pero el firmware no los lee — el selector de acento sigue usando siempre los 6 presets compilados (ver `CONTRATO-formato-tema.md` §H). El lado "constructor pleno" de Aura Studio (rasterizar fuentes/íconos del sistema del usuario con un puerto del pipeline) es Fase 2B, posterior — esta pasada entrega 2A (empaquetar desde una carpeta de assets ya generados + instalar/listar/activar/eliminar), suficiente para construir y probar el tema Apple real desde `~/Aura-local/theme-apple-source/`.

## D-290 — Primer release público (`v0.1.0-beta`): marcador de versión en disco, licencias de terceros en el artefacto, bootloader confirmado

**Encargo**: `PLAN-release-updates.md` (carpeta padre del repositorio, cubre ambos repos) — Fase 1 aprobada con las 9 recomendaciones (Q1-Q9). Objetivo: preparar el primer Release público de Aura-Firmware y que Aura Studio pueda detectar actualizaciones contra él.

**§1.0 — marcador de versión (Q1)**: el dispositivo no tenía forma de reportar qué versión de Aura tiene instalada — confirmado investigando `aura_settings.c` (ya reconocía el hueco en un comentario propio de D-289) y el propio `RBVERSION`/`rbversion.h` que genera Rockbox en cada build (hash de git + fecha, no un SemVer comparable, y ni siquiera llegaba a `firmware/dist/`). Solución elegida, de menor riesgo posible: `package_dist.sh --release-tag <tag>` escribe `.rockbox/aura/version.txt` (una línea, el tag exacto) **dentro de** `rockbox.zip`, en tiempo de empaquetado — cero cambios al C del firmware, cero riesgo para `aura_settings.c`/`aura.cfg`. Sin el flag (build de desarrollo) el archivo no se escribe; su ausencia es una señal válida para Studio ("instalado por fuera de un Release etiquetado"), no un error. El mecanismo de comparación por hash que ya tenía Aura Studio (`AuraUpdateChecker`, desde el 2026-08-13, con un comentario que anticipaba textualmente esta tarea) **no se reemplaza** — sigue siendo el respaldo sin versión/sin red.

**§1.1 — estado del release**: había 15 commits locales sin subir (todo D-286…D-289 y su bitácora) — subidos a `origin/main` antes de tocar cualquier otra cosa, requisito para poder crear el tag. Builds verificados de punta a punta en esta misma pasada: simulador limpio, **9/9** binarios de test (no 8/8 — `test_style` se sumó en D-289 como noveno), ARM limpio salvo un `-Wtype-limits` ya aceptado en `aura_style.c:88` (mismo cheque de siempre) y uno idéntico en `aura_lang.c:485` (`id < 0` sobre `aura_str_id_t`, mismo patrón — no apareció en la verificación de D-289 porque ese archivo no se había recompilado en esa corrida incremental; no es una regresión de esta pasada, solo la primera vez que se ve). **Hallazgo que cambia el plan de instalación**: `bootloader-ipod6g.ipod` ya estaba compilado (paso manual del dueño, mismo día) y al día con la fuente del bootloader (sin cambios desde 2026-08-10) — este release incluye bootloader, por lo tanto es instalable desde cero en un iPod con firmware original de Apple, no solo actualización.

**§1.2 — licencias (Q3)**: el texto de licencia de Inter/Lucide/Phosphor ya vivía completo en `design-system/vendor/*/LICENSE*` (confirmado Lucide es **ISC**, no MIT — el propio `CONTRATO-firmware-studio.md` y el `README.md` del firmware ya lo tenían bien), pero no llegaba al artefacto distribuido ni a la pantalla "Acerca de" del dispositivo. `package_dist.sh` ahora genera `firmware/dist/THIRD-PARTY-NOTICES.txt` (los tres textos concatenados, asset nuevo del Release, sin checksum — mismo trato que `MODIFICATIONS.md`), y `AURA_STR_ABOUT_CREDITS_BODY` (ES/EN, `aura_lang.c`) suma un párrafo "Tipografía e íconos" junto al crédito de Rockbox ya existente. GPL v2 (LICENSE, MODIFICATIONS.md, cabeceras) verificado intacto — único archivo sin cabecera es `apple2026_tokens.h`, autogenerado, sin riesgo.

**§1.3/1.4 — versionado y notas**: tag `v0.1.0-beta`, título "Aura iPod-Firmware Beta Version". Convención de nombres de artefactos ya era estable (nunca se embebe versión en el filename) — no se tocó. `tokens.json` crudo **no** se publica como asset (Q4) — `theme-format-v1.json` ya es el subconjunto público deliberado. Notas del release en `PLAN-release-updates.md` §1.4, con advertencia explícita de riesgo de instalación y aclaración de que incluye bootloader.

**Verificación final**: `firmware/dist/` completo (10 archivos), `version.txt` confirmado dentro de `rockbox.zip` con el contenido exacto `v0.1.0-beta` (`unzip -p`), `checksums.txt` con los 4 binarios (`rockbox.zip`, `rockbox.ipod`, `mks5lboot`, `bootloader-ipod6g.ipod`).

**Pendiente, documentado, fuera de esta pasada (Q9)**: `CONTRATO-firmware-studio.md` promete que Aura Studio "cumple §3 mostrando una pantalla de Licencias" — no existe tal vista en el código de Studio hoy. No bloquea este release (Studio sigue privado, sin distribuirse a terceros), pero hay que cerrarlo antes de que Studio mismo se distribuya — ver `ST-006` en la bitácora de Aura Studio.

## D-291 — Fotos no mostraba nada: `AURA_SCREEN_PHOTOS_ALL` sin caso de dibujo (perdido en el revert de D-253), visor completo con miniaturas y sonda de tamaño

**Encargo**: diagnosticar "en Fotos no aparece nada" (ni en simulador ni en hardware) y, si el visor real hacía falta, construirlo — `PLAN-image-viewer.md`, aprobado con todas las recomendaciones tras la Fase 0 (diagnóstico) y Fase 1 (plan).

**Fase 0 — causa real, no la hipótesis principal**: no era HEIC (Aura Studio ya convierte toda foto a JPEG baseline ≤320/640px antes de copiar, verificado contra la biblioteca real del dueño) ni un desajuste de rutas (`/Photos` coincide en ambos lados). El bug: `AURA_SCREEN_PHOTOS` (la fila-menú "Todas las fotos") intercepta el dibujo en la rama de menús de `aura_screens_draw()`, así que la rama más abajo para esa misma constante era código muerto — y `AURA_SCREEN_PHOTOS_ALL` (el destino real que empuja `aura_nav_push()`) no tenía ningún caso, cayendo al *fallback* genérico ("Nada sonando" con JPEG válidos en el disco). Los botones sí estaban cableados a `AURA_SCREEN_PHOTOS_ALL`, por eso SELECT sobre una fila fantasma empujaba el visor, que sí dibujaba — nadie llegaba ahí porque la lista nunca se veía. Idéntico bug en `AURA_SCREEN_VIDEOS_ALL`. El arreglo ya había existido (D-251, `246f99f`) y se perdió con el `git revert` en bloque de D-253 (que lo advertía textualmente sin que se reaplicara nunca) — reaplicado aquí junto con su hermano de Video.

**Commit 1/6**: despacho de `PHOTOS_ALL`/`VIDEOS_ALL` restaurado; `aura_photos_invalidate()`/`aura_video_invalidate()` re-escanean al entrar desde el menú (antes se escaneaba una sola vez por arranque — un sync por USB durante la sesión no se reflejaba hasta reiniciar); el panel derecho del menú deja de leer `sync_summary.cfg` (podía desincronizarse del disco) y pasa a contar `/Photos`/`/Videos` real.

**Commit 2/6**: `MAX_PHOTOS` 200→500 y `PHOTO_NAME_LEN` 64→96 (contrato §6.4/6.5 de Studio); orden natural insensible a mayúsculas (`strnatcasecmp`, ya usado por `filetree.c`/`tagtree.c`) en vez del orden físico de la FAT; fila final inerte "…y N más" si hay más de 500; segunda línea de ayuda en el vacío ("Sincroniza fotos desde Aura Studio").

**Commit 3/6**: `probe_current_photo()` lee solo cabeceras (marcadores JPEG hasta el SOFn, o los 26 primeros bytes de un BMP) antes de decodificar. Tope de 12 megapíxeles o 4096px de lado — por **tiempo de CPU, no por RAM**: `read_jpeg_file()`/`read_bmp_file()` con `FORMAT_RESIZE` escalan en la IDCT y remuestrean por línea, nunca materializan la fuente completa, así que el costo en memoria es constante sin importar el tamaño de origen (verificado contra el propio código del decoder, no por sospecha). Sobre el tope → "Foto demasiado grande" sin decodificar; JPEG progresivo (SOF2+) detectado por cabecera → "Formato no soportado" sin intentarlo (D-028 ya documentaba que Rockbox solo decodifica SOF0/SOF1). Por debajo de 640px de lado (las fotos de Studio nunca lo cruzan) no hace falta avisar; por encima, "Cargando…" con un `lcd_update()` forzado antes del bloqueo de la decodificación real.

**Commit 4/6**: la lista deja `aura_widgets_draw_list()` (texto plano) por un renderizador dedicado con miniaturas de 48px, mismo patrón que `draw_album_list()`/`draw_album_thumb()` (D-221, única lista de contenido con miniatura real hasta ahora). El *pipeline* de caché `.pfraw` (decodificar una vez, transponer, redondear esquinas, guardar en disco) vivía solo en `aura_albumart.c` atado a `album_seek` — se extrajo la parte genérica a `aura_art.c` (ya el módulo de utilidades de arte compartidas) para que `aura_photos.c` la reuse en vez de reimplementar el formato de archivo. Llave de las miniaturas: nombre de archivo (único dentro de `/Photos`) + `mtime` del archivo fuente (campo `extra` nuevo en el header `.pfraw` — un re-sync con el mismo nombre pero contenido distinto no sirve una miniatura vieja; los `.pfraw` de álbum/*playlist* existentes se regeneran solos una vez porque el header cambió de tamaño). Antes de decodificar una miniatura se aplica el mismo tope de tamaño que el visor — una foto "demasiado grande" compone un *tile* liso en vez de intentar decodificar, también cacheado. Las miniaturas reusan `s_view_scratch` (el buffer de 240KB del visor de pantalla completa) como espacio de trabajo del decoder en vez de duplicarlo — lista y visor nunca están activos a la vez; se invalida `s_loaded_index` cada vez que una miniatura se decodifica, para que volver al visor después de pasar por la lista redecodifique en vez de mostrar el contenido que dejó ahí la miniatura (verificado explícitamente: ver foto A, ir a la lista con *cache miss* de otras fotos, reabrir A — redecodifica correcto, no basura).

**Commit 5/6**: la rueda (`SCROLL_FWD`/`SCROLL_BACK`) es la navegación primaria entre fotos dentro del visor — LEFT/RIGHT se conservan como atajo, no se retiran. Cada cambio dispara `aura_transition_fade_slide_region()` (D-283, hasta ahora solo usada por "Acerca de") sobre la pantalla completa — el visor no tiene `StatusBar` que excluir de la región. Al salir (MENU/SELECT), la lista recupera la selección de la foto que se estaba viendo, no la de entrada (`aura_nav_pop()` primero, `aura_nav_set_selection()` después — cada nivel de profundidad tiene su propio *slot* de selección en `aura_nav`, hay que apuntar al de la lista, no al del propio visor).

**Verificado en el simulador** con fixtures reales generadas con PIL (imágenes de ruido aleatorio para que el resultado decodificado fuera comprobable por píxel) y JPEG reales de la biblioteca de Aura Studio del dueño: la secuencia completa menú→lista→visor→siguiente/anterior→volver funciona vía navegación normal, sin ningún parche de diagnóstico; 640×480 sin aviso, 3000×2000 (bajo el tope) decodifica, 5000×4000 (sobre el tope) y progresivo muestran su mensaje sin decodificar; miniaturas reales decodifican/cachean y persisten entre visitas; regresión de Álbumes (carátula embebida real vía el `aura_art.c` refactorizado) sin diferencia visible antes/después. Dos limitaciones de honestidad documentadas en los commits: el frame intermedio de "Cargando…" y de las transiciones entre fotos no se pudo capturar visualmente en el simulador (decodificación de host en microsegundos / transiciones corren síncronas dentro de una sola llamada) — se confirmó por lectura de código, mismo criterio que ya usa este archivo para otras transiciones (`DECISIONES-ARCHIVE.md`, "verificación de humo únicamente"); y el tiempo real de decodificación en hardware físico para el tope de 12MP (Q6 del plan) sigue pendiente de medición, sin iPod disponible en esta sesión.

Build ARM y de simulador limpios tras cada uno de los 5 commits de código (un solo `-Wtype-limits` preexistente en `aura_lang.c`, confirmado contra el baseline antes de cualquier cambio de esta pasada, no relacionado). `make -C firmware/rockbox/apps/aura/test test` sin cambios en los conteos en ningún paso.

**Contrato para Aura Studio** (`CONTRATO-firmware-studio.md` §D.1, nuevo): formato JPEG baseline exacto, resolución máxima recomendada (640px, decodifica sin remuestreo), orientación horneada, nombres únicos ≤95 bytes — desbloquea la pasada del lado Studio (ver hallazgos laterales de Studio en `PLAN-image-viewer.md` §9: colisión de nombres homónimos en `Photos/`, 688 `cover.jpg` de carpetas de música en cola como fotos, texto de "Versión HD" que promete un zoom que el visor no tiene).

**Pendiente, documentado, fuera de esta pasada** (preguntas del plan con recomendación, sin resolver en silencio): modo "pantalla completa recortada con paneo" del original; rejilla de miniaturas en vez de lista; modo presentación (Ajustes → Fotos del original); PNG nativo portado al core (descartado por ahora — Studio ya convierte, portar ~2.9k líneas GPL no paga para archivos copiados a mano); medición de tiempo de decodificación en hardware real para ajustar el tope de 12MP si hiciera falta.

## D-292 — Sección "Personalización" en Ajustes, "Tema" se vuelve "Modo" (patrón `InlineValue` nuevo), fondos del `SelectionSummary` con los 6 presets de acento

**Encargo**: tres trabajos relacionados, con dos preguntas señaladas de antemano como no resolubles en silencio (Modo↔Temas, procedencia de las imágenes de fondo) — `PLAN-personalizacion.md`, aprobado con todas las recomendaciones tras la Fase 0 (inventario) y Fase 1 (plan).

**Fase 0 — hallazgos que corrigieron premisas del encargo**: el fondo del panel derecho **no era** un degradado calculado — desde D-267 ya era una imagen BMP 160×240 por preset de acento, pero solo existía `pink` y todo acento caía ahí ("interino explícito" documentado en `tokens.json`); el toggle "Mostrar sombras" **ya estaba implementado** (D-154), no nacía en esta pasada; la geometría de fila citada por el encargo (152×22, ícono 14px) estaba desactualizada — desde D-195 son 31px de alto e ícono de 20px. `ColorsBack/` (5 PNG sin trackear en la raíz del repo, sin metadatos de procedencia) quedó como pregunta bloqueante hasta que el dueño confirmó: naranja/verde/morado son propias, rojo es de Franck V. y azul de Matthew McBrayer (ambas Unsplash).

**Commit 1/5**: patrón nuevo **`InlineValue`** en `aura_menu_list.c`/`.h` — campo `aura_menu_item_v2_t.value` (texto en `TEXT_SECONDARY` siempre, incluso con la fila seleccionada — dato, no acción), alineado a 4px del borde derecho del `Selector` (mismo margen que switch/checkmark/flecha), presupuesto de 60px (`menu_list.inline_value_max_w`) del que el valor real consume solo lo que mide — la etiqueta cede el resto y se trunca con "…" si hace falta. La flecha del `Selector` queda excluida cuando hay valor, generalizando la exclusión que antes solo cubría a mano el caso de "Repetir" (D-264). Las 6 fuentes existentes de arrays `aura_menu_item_v2_t` (locales, sin inicializar) ganaron `.value = NULL` explícito — sin eso, el campo nuevo quedaba con basura de pila.

**Commit 2/5**: "Tema" se convierte en **"Modo"**, primer consumidor real del patrón. `AURA_SCREEN_SETTINGS_THEME` sale de `is_choice_screen()` (mismo retiro limpio que D-264 hizo con "Repetir"); SELECT alterna Claro↔Oscuro en el sitio, aplica **instantáneo** (sin transición — repintar toda la paleta no tiene patrón en el vocabulario, y es lo que ya hacía la pantalla de elección retirada); el ícono del panel derecho refleja el valor 1:1 (`theme-light`/`theme-dark`). "Estilo" se renombra **"Temas"** y gana una nota en su panel ("Se ve en modo claro y oscuro") que hace explícita la resolución del conflicto conceptual: **un tema respeta el Modo activo, nunca lo reemplaza** — el formato v1 exige paleta clara y oscura obligatorias (`CONTRATO-formato-tema.md` §B), así que no existe el caso "el tema pisa el modo" que resolver en runtime; si un formato futuro admitiera una sola paleta, la regla queda documentada de antemano (`sistema/05-temas.md`): la fila se atenúa con "Fijado por el tema", nunca se oculta ni se ignora en silencio. La clave `theme:` de `aura.cfg` **no se renombra** — es clave:valor por nombre, renombrarla resetearía el modo de cualquier dispositivo ya instalado.

**Commit 3/5**: `AURA_SCREEN_SETTINGS_PERSONALIZATION` nuevo, submenú SPLIT (mismo patrón que "Fecha y hora"), agrupa las 7 filas de apariencia — Modo, Temas, Color de acento, Animaciones, Gráficos, Mostrar sombras, Mostrar iconos — en ese orden (de mayor a menor impacto visual), sin que ninguna cambie de `AURA_SCREEN_ID`, ícono o clave en `aura.cfg`, solo de padre de navegación. Ajustes pasa de 26 a 20 filas. Bug real encontrado y corregido en la verificación: `parent_settings_icon()` solo buscaba en `settings_entries[]` — al mudar Color de acento/Animaciones/Gráficos, sus pantallas de elección perdían el ícono del panel derecho (D-264) y caían al genérico "settings"; se amplió para buscar en ambas tablas.

**Commit 4/5**: `ensure_panel_background()` deja de aceptar `"pink"` fijo — el fondo corresponde al **acento activo** vía `accent_background_preset_name()`, que compara `accent_rgb24` contra los 6 valores de `accent_presets_hex` por índice (mismo orden que `right_panel_background.presets`). Los 5 presets que faltaban (rojo/naranja/verde/azul/morado) se movieron de `ColorsBack/` a `design-system/assets/panel-backgrounds/<id>-source.png`; `generate.py` los horneó a BMP sin cambios de código (el pipeline ya existía). Nuevo **fallback calculado** (`draw_accent_gradient_background()`, degradado vertical de 3 puntos usando `aura_accent_dark()`/`aura_accent()`/`aura_accent_light()`, los mismos derivados ±25% que ya usaba el tile) para cuando el acento activo no tiene imagen — reemplaza el relleno plano `SHELL_BG` que D-267 dejaba para ese caso: ahora el fondo **siempre** refleja el acento elegido, nunca "pink" prestado. Atribución agregada en `THIRD-PARTY-NOTICES.txt` (sección nueva, `package_dist.sh`) y en "Acerca de → Créditos" (ES/EN): "Foto de Franck V. en Unsplash" / "Foto de Matthew McBrayer en Unsplash" — licencia Unsplash, sin atribución legalmente requerida, se da de cualquier forma por decisión del dueño.

**Commit 5/5**: documentación viva — `componentes/left-panel.md`/`selector.md` (spec completa de `InlineValue` + regla dura de exclusión de la flecha), `componentes/selection-summary.md` (correspondencia acento→imagen, fallback calculado, confirmación de que el fondo no varía por Modo), `sistema/05-temas.md` (resolución explícita Modo↔Temas con la regla futura "Fijado por el tema"), `fundamentos/01-color.md` (fondo ya no es "no calculado", pendiente de adaptar el acento libre por Modo anotado sin resolver), `sistema/03-arbol-de-menus.md` (submenú Personalización documentado), `00-INDICE.md`.

**Verificado en el simulador en cada commit**: Ajustes sin regresión visual con `value=NULL` en todas partes (commit 1); fila "Modo" con valor inline, SELECT alterna Oscuro↔Claro con ícono luna/sol y paleta completa cambiando al instante, panel derecho 1:1, "Temas" con la nota nueva, `aura.cfg` conservando la clave `theme:` sin resetear tras el toggle (commit 2); "Personalización" en su lugar exacto en Ajustes, submenú con las 7 filas en orden, "Animaciones" conservando su ícono de estrella en el panel al entrar a su pantalla de elección — confirma el fix de `parent_settings_icon()` (commit 3); acento Rojo muestra la foto de Franck V., acento Azul la de McBrayer, un acento forzado fuera de los 6 presets (`#22AA88`) muestra el degradado calculado (no un color plano ni "pink"), página de Créditos con ambas atribuciones tras hacer scroll (commit 4).

Build ARM y de simulador limpios tras cada uno de los 4 commits de código (los mismos 2 warnings preexistentes ya aceptados en D-289/D-291, `aura_lang.c` y `aura_style.c`, sin relación con esta pasada). `make -C firmware/rockbox/apps/aura/test test` sin cambios en los conteos en ningún paso.

**Pendiente, documentado, fuera de esta pasada** (preguntas del plan con recomendación, sin resolver en silencio): adaptar el acento libre por Modo (como D-274 hace con el rosa de fábrica, `fundamentos/01-color.md`); convertir "Animaciones"/"Gráficos" a `InlineValue` (recomendado, no ejecutado — el encargo pedía empezar con Modo); regla futura del formato de tema para "una sola paleta" (`theme_format` v2, sin implementar); duplicar los 6 fondos por Modo (evaluado y descartado — ninguna de las 6 fotos lo necesita); sincronizar la copia de `PLAN-personalizacion.md` archivado y cualquier renombre de UI relevante hacia Aura Studio (`ExtrasView.swift:52` sigue diciendo "Ajustes > Estilo").

## D-293 — Reconstrucción de la biblioteca tras sincronizar: marcador `/.aura/sync-pending.json`, pantalla "Actualizando biblioteca…", "Reconstruir biblioteca" en Ajustes, y el contrato de estructura de biblioteca

**Encargo** (2026-08-17, cross-repo con Aura Studio ST-012, decisiones cerradas en el propio encargo): las canciones sincronizadas "están en el dispositivo pero no aparecen en Aura". Verificación previa (código + simulador, sin iPod conectado en esta sesión): la lista de Música sale **exclusivamente** de tagcache (`aura_music.c`, `tagcache_search`); Aura no tiene navegador de archivos por diseño, así que "reproducir navegando por archivos" no es una prueba posible en Aura — el equivalente fue reproducir el síntoma en el simulador: copiar 7 archivos a `/Music` sin tocar la base → "Sin música todavía" (captura `00`). **Causa raíz** encontrada en el camino: `aura_music_db_ready()` disparaba `tagcache_start_scan()` una vez por arranque creyendo que re-escaneaba la biblioteca ("sin esto, una base construida en un arranque anterior jamás se entera de la música que Aura Studio sincronizó después") — pero el manejador de `Q_START_SCAN` en `tagcache.c` **solo construye si `global_settings.tagcache_autoupdate` es verdadero**, y Aura nunca lo forzó (por defecto `false`, `settings_list.c`); con `tagcache_ram` (D-021) la rama solo carga la copia en RAM. Es decir: el firmware **nunca** re-escaneaba solo. Studio lo tapaba borrando `database_*.tcd` en cada sync (`triggerFirmwareDBRebuild`) para forzar el camino de "sin base" — ciego, y con la base ausente hasta que terminaba. Sobre la copia de Studio: `LibrarySync.copyFileTransactionally` (streaming por bloques de 4 MB, temporal + `moveItem`) — completa por construcción; y **no preserva fechas**, lo que hace válida la detección por `mtime` de tagcache para todo lo que Studio escribe.

**Mecanismo (contrato `docs/contracts/library-layout-v1.md` §4, copia idéntica en Studio)**: Studio escribe `/.aura/sync-pending.json` (`version: 1`, `timestamp`, `changes.{music,video,images}`) al terminar cada sync que tocó archivos; `aura_sync.c` (nuevo) lo lee **al arrancar y al volver de la pantalla USB** (`aura_main.c`: envuelve `default_event_handler(SYS_USB_CONNECTED)`, que no devuelve hasta que el cable se desconecta — el único punto donde el firmware se entera de que Studio pudo escribir; también en el camino diferido del candado, D-238). Reconstrucción **incremental por sección**: Videos/Fotos solo invalidan su listado (`aura_video_invalidate()`/`aura_photos_invalidate()`; las miniaturas se llevan por `mtime`); Música: con base usable y sin temporal huérfano, `tagcache_update()` (`Q_UPDATE`: recorre todo el árbol, re-lee `mtime` cambiado, agrega, y `check_deleted_files()` quita lo borrado — cubre los tres casos del encargo — con la base vieja usable mientras tanto); sin base, con temporal huérfano, o desde el disparo manual, `tagcache_rebuild()` (`Q_REBUILD`, desde cero). Contador `attempts` **dentro del marcador**, subido a `n+1` antes de empezar (un corte de batería lo deja subido); al 3.º fallo, error en español + oferta del disparo manual; el marcador se borra solo al terminar bien. Marcador de versión mayor: se ignora y se dice en pantalla. Ilegible o sin secciones: se borra sin actuar. Al terminar bien con Música tocada: se vacía `/.rockbox/aura/cfcache/` (la caché de carátulas se indexa por `album_seek`, que cambia con cada commit — hallazgo lateral: hasta hoy, cualquier reconstrucción podía dejar la portada de un álbum sobre otro; Cover Flow verificado después, captura `04`) y se rearma la pasada de "primera vez" de `aura_music_db_ready()` (`aura_music_db_reset_triggers()`: calificaciones + precache), que además **cede** mientras `aura_sync_job_active()` para no competir con un segundo `tagcache_rebuild()`.

**Módulos**: `aura_sync_marker.c/.h` (parser/serializador del JSON, puro C99, host-testeable — busca claves por nombre e ignora el resto, mismo criterio que `aura.cfg`; `test_sync_marker.c` nuevo, 51 checks, 10.º arnés), `aura_sync.c/.h` (máquina de estados: `WAIT_TAGCACHE` → `RUNNING` → fin/`POSTPONED`/`ERROR_VERSION`/`ERROR_ATTEMPTS`/`NEEDS_REBOOT`; `aura_sync_tick()` corre en cada vuelta del loop principal, fuera de `lcd_active()` a propósito, con o sin la pantalla arriba), `aura_fsutil.c/.h` (borrado recursivo/vaciado/lectura/escritura — el borrado recursivo salió de `aura_style.c`, sin cambio de comportamiento). UI: `AURA_SCREEN_LIBRARY_SYNC` (FULL, `draw_library_sync()`/`handle_library_sync()` en `aura_screens.c`: filas Música/Videos/Fotos con estado, barra de progreso con la cápsula real de D-277 y tokens `PROGRESS_*`, conteo "N elementos leídos"/"Indexando k/K", pista "Menú: seguir en el próximo encendido"; **no cancelable, posponible**), `AURA_SCREEN_SETTINGS_REBUILD_LIBRARY` (aviso Sí/No, mismo widget que Restablecer, fila "Reconstruir biblioteca" en la sección sistema de Ajustes, icono `sync`, categoría Ajustes en `aura_category.c`), 18 cadenas ES/EN nuevas al final de `aura_lang`. `aura.cfg` gana `sync_marker_supported: 1` (solo escritura, misma convención que `theme_format_supported`) para que Studio sepa **no** borrar la base con este firmware — sin la clave, Studio conserva su borrado: ninguna combinación de versiones rompe (contrato §4.4; orden de despliegue: firmware primero). Documentación viva: `docs/aura-design-system/componentes/library-sync.md` (la única pantalla completa de progreso del sistema — excepción documentada a la cápsula de espera, anotada también en el comentario normativo de `aura_widgets_draw_wait_capsule()`), índice, árbol de menús, `CLAUDE.md`, capturas en `docs/screenshots/library-sync/`.

**Desviación respecto al texto del encargo, con motivo**: "Menú pospone la reconstrucción al siguiente arranque" se implementó como: Menú cierra la pantalla, el marcador queda intacto, y el trabajo **ya encolado** en tagcache se cierra en fondo (`tagcache_stop_scan()` + `aura_sync_tick()` desde el loop): si tagcache alcanza a terminar, borra el marcador (trabajo hecho); si aborta, se descarta el temporal a medias (`tagcache_discard_pending_temp()`, la base vieja está intacta) y se restaura el contador — posponer nunca cuenta como fallo — y el próximo arranque lo retoma. Detener "de verdad" y dejar el temporal habría chocado con Rockbox: un temporal huérfano bloquea cualquier build posterior en ese arranque (`do_tagcache_build` lo ve y no hace nada) y el hilo lo confirma al arrancar **preguntando con un diálogo sí/no de Rockbox** (cromo, prohibido) — ese diálogo se silenció (confirma siempre, era el default a 5 s). Verificado en el simulador con 4000 archivos: posponer a mitad → raíz de inmediato, marcador con `attempts` restaurado, sin temporal, base vieja intacta.

**Cuatro parches a `apps/tagcache.c` (registrados en `MODIFICATIONS.md`, marcados `Aura (D-293)`)**, todos encontrados a golpes en el simulador y no adivinados: (1) tagcache no ofrece forma de saber cuándo un `Q_UPDATE`/`Q_REBUILD` terminó (`ready` no cambia en un update; `curentry` solo con `syncscreen`, que además bloquea el escaneo al ritmo de la UI; `queue_length` ya vale 0 mientras se procesa) → contador `tagcache_get_build_jobs_done()` + `tagcache_has_pending_temp()`/`tagcache_discard_pending_temp()` para no exportar el nombre del temporal. (2) La copia en RAM se dimensiona **una vez, al arrancar** (+32 KB); tras un update que agregó cientos de canciones ya no cabe, `load_tagcache()` falla, y el original lo tomaba por "base corrupta": la deshabilitaba hasta reiniciar (Música vacía sin motivo) y — peor — dejaba `ramcache_allocated > 0` con el handle ya liberado, así que el siguiente `commit()` intentaba robar ese buffer y buflib entraba en pánico (`invalid handle pin: 0`, reproducido). Ahora `load_ramcache()` distingue "no cabe" (`tc_load_failed_too_big`: libera, redimensiona desde el master nuevo con holgura y reintenta; si tampoco, la base sigue usable **desde disco**) de "corrupta" (criterio original: deshabilitar — y `aura_sync` encadena entonces **un** `Q_REBUILD` de recuperación, verificado tras corromper la base a propósito matando el simulador a mitad de un borrado). (3) `commit()` prefería robar la copia en RAM aunque fuera claramente chica para el commit pendiente y `build_index()` fallaba con "Buffer way too small!" → commit "pospuesto hasta el próximo arranque"; ahora prueba primero un buffer general si el de RAM no alcanza (en el aparato el bloque de dircache suele bastar y esto no se alcanza). (4) El diálogo sí/no del arranque, arriba. Y el estado `NEEDS_REBOOT` de `aura_sync` cubre honestamente el caso en que tagcache igual pospone el commit: lo dice en pantalla y el intento no cuenta.

**Hallazgo lateral corregido de paso**: `aura_music_precache_album_art()` dibuja su propia pantalla ("Preparando carátulas N/M") por fuera de `aura_screens_draw()` y, al terminar, el loop se quedaba esperando un botón **con ese cuadro en pantalla** — visible en cuanto algo vacía `cfcache/` (esta pasada) y tapaba la pantalla siguiente hasta tocar la rueda; `aura_music_take_redraw_request()` fuerza un redibujo (`timeout_ticks = 0`).

**§B — letras `.lrc`**: verificado con archivo:línea, **sin ajuste**: el firmware busca **una sola ruta**, el hermano del audio con la extensión reemplazada por `.lrc` (`aura_nowplaying.c`, `derive_sibling_path()` + `load_lyrics()`, 8 KB), y `aura_lrc.c` descarta líneas sin `[mm:ss]` (letra plana = sin letra, ícono del Modo 4 al 50 %). Studio no escribía ese archivo al iPod (solo en su `.preparados/` local) — lo escribe desde ST-012; documentado en el contrato §3.

**Verificación**: `build_sim.sh` limpio; ARM (`package_dist.sh`) limpio salvo los dos `-Wtype-limits` de siempre (`aura_lang.c`, `aura_style.c`, baseline D-291); `make test` **10/10** (nuevo `test_sync_marker`, 51/51). Simulador, con fixtures de 7 y de 4000 archivos: marcador → pantalla → canciones aparecen y Cover Flow encuentra portadas (capturas `02`–`04`); interrupción a mitad (salida del proceso con `attempts: 1` y temporal huérfano) → el siguiente arranque confirma el temporal sin diálogo, completa y borra el marcador; posponer con Menú (arriba); `attempts: 3` y `version: 7` → las dos pantallas de error (`06`, `07`); Ajustes → Reconstruir biblioteca → Sí → reconstrucción completa (`05`, `02`); marcadores ilegibles/vacíos se descartan (tests + simulador). **Extremo a extremo con el Studio real, sin iPod** (lo más cerca del hardware que permite esta sesión): el `LibrarySync` real de Aura Studio (ST-012, ejecutado desde su suite contra el `simdisk` del simulador como volumen montado) sincronizó 3 canciones a `/Music/Aura QA/Sync E2E/` con su `cover.jpg`, un `.lrc` y el marcador (`aura.cfg` con `sync_marker_supported` → la base **no** se borró); el simulador arrancó, reconstruyó por el marcador (una base previa de 4008 entradas quedó en las 3 reales: altas y bajas detectadas), y las 3 aparecen en Canciones (`08`); una segunda pasada de Studio "solo letras" (sin recopiar audio, sin marcador) dejó los 3 `.lrc` y el Modo 4 de Ahora Suena las muestra (`09`). Sin iPod físico en esta sesión: la prueba en hardware (sincronizar desde la app → expulsar → arrancar) queda a cargo del dueño; en el aparato dircache y ramcache hacen rápido lo que en el simulador fue lento (borrados en disco).

**Contratos**: `docs/contracts/library-layout-v1.md` nuevo (canónico aquí, copia idéntica en Studio); `CONTRATO-firmware-studio.md` sube a **v4** y **reconcilia las dos copias**, que habían divergido en direcciones opuestas (la de aquí tenía §D.1 `Photos/` de D-291; la de Studio la fila `device.cfg` de ST-011 y decía v3): desde v4 ambas traen todo, y `CONTRATO-dispositivo.md` (que ST-011 declaraba "copia idéntica en Aura-Firmware" pero solo existía en Studio) queda copiado aquí.

## D-294 — El iPod adopta el nombre que Aura Studio le puso: `device_name` de `device.cfg` en "Acerca de"; el contrato del dispositivo sube a v2 con `device_owner`

**Encargo** (2026-08-17, cross-repo con Aura Studio ST-013): "Aura Studio ya tiene el nombre del iPod; que el firmware lo adquiera. Aunque el iPod se sincronice con varias computadoras, solo adquiere el nombre de la primera con la que se sincronice y solo desde ahí se puede volver a cambiar." La segunda mitad es una regla de **propiedad** que `CONTRATO-dispositivo.md` v1 (ST-011) no tenía forma de expresar (`device_id`, `device_name`, `device_name_updated_at` — nada dice quién nombró): el contrato sube a **v2** con la clave `device_owner` (el `installationID` de la instalación de Studio que nombró el iPod la primera vez, escrito una sola vez, mismo identificador que `SyncRecord.writtenBy`) y la regla completa en su §C bis — otra Mac muestra el nombre pero no lo edita; un `device.cfg` v1 sin propietario lo reclama la primera instalación que lo guarde; reinstalar Studio pierde la propiedad (mismo criterio que `writtenBy`), sin "transferencia" deliberadamente. Todo eso es del lado Studio (ST-013); aquí solo se documenta y se copia idéntico.

**Firmware**: `aura_device_name.c/.h` (puro C99, host-testeable: recorta/colapsa espacios, descarta caracteres de control, trunca a 48 bytes **sin partir una secuencia UTF-8** — `test_device_name.c`, 15 checks, 11.º arnés) + `aura_device.c/.h` (lee `/.rockbox/aura/device.cfg` con `read_line()`/`settings_parseline()` como los demás `.cfg`, buffer de 64 bytes; **solo** consume `device_name`, ignora `device_owner`/`device_id`; el firmware nunca escribe el archivo). Se relee en los mismos dos momentos que el marcador de sincronización (D-293): al arrancar y al volver de la pantalla USB — únicos instantes en que Studio pudo escribirlo — desde `aura_main_sync_after_disk_handoff()`. Consumo: el slot superior del panel derecho de "Acerca de" (`panel_top`), exactamente el que el contrato v1 §E dejaba anotado como "consumo natural": `aura_device_name()` sustituye al literal `"Mi iPod"` (`AURA_STR_ABOUT_MY_IPOD`), que queda como respaldo sin archivo o sin nombre válido; mismo Bold 18 y mismo marquee si no cabe. `aura_device_name()` devuelve un puntero **estable** a un buffer interno a propósito: el panel derecho compara `panel_top` por puntero para su debounce (`draw_nav_list`). La pantalla expandida de Acerca de no muestra ese slot (muestra el modelo de disco), no se toca.

**Verificación**: `build_sim.sh` limpio; ARM limpio; `make test` **11/11**. Simulador con un `device.cfg` v2 de prueba: Ajustes → Acerca de muestra "iPod de Ricardo" en el slot (`docs/screenshots/device-name/01-acerca-de-nombre-del-ipod.png`); sin archivo vuelve a "Mi iPod". Contratos: `CONTRATO-dispositivo.md` v2 y la fila `device.cfg` de `CONTRATO-firmware-studio.md` (sin subir su versión: la semántica nueva vive en el contrato hermano) — ambos copiados idénticos a Studio en ST-013.

## D-295 — Tres íconos de la sección "Personalización" de Ajustes: brochita, paleta y cuentagotas

**Encargo** (2026-08-17): el dueño pidió, sin más contexto, tres reasignaciones puntuales de ícono dentro de Ajustes → Personalización (D-292): la fila padre "Personalización" con una brochita/pinceles, "Temas" (hija) con una paleta de colores, "Color de acento" (hija) con un cuentagotas.

**Vendorizado**: de los tres íconos pedidos, `paintpalette` ya existía en el catálogo (89 íconos, D-286) — solo se le cambió de fila. `paintbrush` y `pipette` (nombre real de Lucide para "cuentagotas") se vendorizaron nuevos desde `raw.githubusercontent.com/lucide-icons/lucide` con la misma normalización `currentColor`→`#000000` que exige `apple2026_sf_render.swift` (D-286) — catálogo sube a 91. Entradas nuevas en `tokens.json` (`icon.names` documental + `icon.svg_overrides`, la fuente real) y en `design-system/assets/icon-name-map.json` (registro de procedencia), alfabetizadas junto a las existentes. `MIN_INK_TONES` (4) sin fallos para ninguno de los dos; verificación visual manual a 20px sin necesitar el ajuste de grosor de trazo que D-286 sí tuvo que aplicar a otros tres íconos.

**Reasignaciones** (`aura_screens.c`, sin tocar `AURA_SCREEN_ID` ni claves de `aura.cfg` de ninguna fila, mismo criterio de D-292): `settings_entries[]` — "Personalización" `"theme"` → `"paintbrush"`; `personalization_entries[]` — "Temas" `"sync"` → `"paintpalette"`, "Color de acento" `"paintpalette"` → `"pipette"`.

**Verificado en el simulador** (`build_sim.sh` completo — el `make` incremental no basta, no sincroniza los BMP nuevos hacia `simdisk/.rockbox/icons/`): Ajustes muestra la brochita en "Personalización"; dentro del submenú, "Temas" con la paleta y "Color de acento" con el cuentagotas, ambos legibles junto a los íconos ya existentes de la lista. Build ARM y de simulador limpios (mismos dos `-Wtype-limits` preexistentes de siempre, sin relación). `make -C firmware/rockbox/apps/aura/test test` **11/11**, sin cambio en los conteos — este pase no toca código de comportamiento, solo el nombre de ícono de tres filas.

**Sin THIRD-PARTY-NOTICES.txt nuevo**: Lucide (ISC) ya tiene un solo bloque de licencia que cubre los ahora 91 íconos (criterio de D-286, no por-ícono).

## D-296 — Segundo release público (`v0.2.0-beta`): salto de MINOR por dos cambios de contrato de datos desde v0.1.0-beta

**Encargo**: "crea un nuevo release en beta v0.1.2" — se le señaló al dueño, antes de tocar nada, que `CONTRATO-firmware-studio.md` §E ya documenta la regla "un cambio a la sección D exige MINOR nuevo en ambos [repos]" y que desde `v0.1.0-beta` hubo dos: D-293 (`docs/contracts/library-layout-v1.md` nuevo, contrato maestro sube a v4) y D-294 (`CONTRATO-dispositivo.md` sube a v2 con `device_owner`). El dueño confirmó seguir la regla propia: **v0.2.0-beta**, no v0.1.2-beta.

**Contenido del release** (16 commits desde `v0.1.0-beta`): D-291 (visor de Fotos), D-292 (sección Personalización, Modo/Temas, fondos por acento), D-293 (reconstrucción de biblioteca tras sync), D-294 (nombre del iPod), D-295 (íconos de Personalización).

**Proceso** (mismo que D-290): commit pendiente (D-295) subido a `origin/main` antes de crear el tag; `package_dist.sh --release-tag v0.2.0-beta` — ARM y `mks5lboot` recompilados limpios (mismo `-Wtype-limits` de siempre en `aura_style.c`), `bootloader-ipod6g.ipod` sobrevivió intacto de la compilación manual de D-290 (sin cambios en su fuente desde entonces); `version.txt` confirmado dentro de `rockbox.zip` con el contenido exacto `v0.2.0-beta`; `checksums.txt` con los 4 binarios. `make -C firmware/rockbox/apps/aura/test test` **11/11**, sin cambio en los conteos. Tag anotado `v0.2.0-beta` sobre `cffffd9`, empujado; Release publicado como pre-release con los mismos 10 assets que v0.1.0-beta (título igual, notas nuevas con la sección "Novedades desde v0.1.0-beta" y la explicación del salto de MINOR).

**Pendiente, documentado, fuera de esta pasada**: la tabla de compatibilidad de `CONTRATO-firmware-studio.md` §E sigue con una sola fila placeholder ("0.1.x — sin Release público todavía") — no se tocó porque el otro lado de esa fila es un `FIRMWARE_VERSION`/versión de Aura Studio real, que es trabajo del propio repo de Studio (Studio sigue sin Release público); actualizar el pin y la tabla es tarea de una sesión en `Aura-Studio`, no de esta.

## D-297 — `rockbox.zip` sin códecs ni plugins desde la separación de repos: `package_dist.sh` nunca corría `make zip`

**Encargo**: el dueño reportó video y fotos que "no se pueden visualizar" en el iPod real, y sincronizaciones que se detienen siempre en el mismo punto (`PLAN-sync-media-hardening.md`, carpeta padre). Investigación previa a tocar código (Fase 1 de ese plan, hecha con Fable): `unzip -l firmware/dist/rockbox.zip` sobre el artefacto **real** de `v0.2.0-beta` — 9044 archivos, **0 `.codec`, 0 `.rock`, sin `viewers.config`, sin `codepages/`**. `firmware/tools/package_dist.sh` armaba el árbol `.rockbox/` **a mano**, solo con `design-system/out/` (fuentes, íconos, `aura/`, `version.txt`, `rockbox.ipod`) — nunca llamaba a `make zip`. `firmware/tools/build_sim.sh` sí usa `make install`, por eso el simulador siempre tuvo todo y nadie lo notó en meses de trabajo verificado ahí.

**Es una regresión, documentada dos veces sin que se conectaran los puntos**: `DECISIONS-ARCHIVE.md` D-178 (2026-08-13, monorepo) ya describía el pipeline correcto — `make zip` (árbol ARM real: códecs, plugins) + fuentes/íconos de Aura encima → `rockbox.zip`. Al separar los repos, `020f746` (2026-08-16, "script reproducible de empaquetado") reescribió el empaquetado desde cero y silenciosamente sustituyó la mitad `make zip` por la copia manual de `design-system/out/` — su propio mensaje de commit dice "arma el árbol `.rockbox/` desde `design-system/out/`", sin mencionar que eso reemplazaba a `make zip`. D-288 (2026-08-17) ya había encontrado *una* consecuencia de este mismo punto ciego (fondos/tile-icons en la ruta vieja) sin ver el problema completo. Los dos releases públicos hasta hoy (`v0.1.0-beta`, `v0.2.0-beta`) salieron así: sin `mpegplayer.rock` no hay video (`aura_video.c` ignora el valor de retorno de `plugin_load()`, así que el usuario solo ve pasar 2 s un splash de Rockbox **en inglés** y vuelve a la lista, indistinguible de "no pasó nada" — corregido en D-298); música solo sonaba si el iPod conservaba códecs de una instalación anterior (Studio instala con `ditto`, que mezcla, no reemplaza el árbol).

**Arreglo**: `package_dist.sh` corre `PATH="$TOOLCHAIN:$PATH" make zip` en `firmware/build-ipod6g` justo después de compilar `rockbox.ipod`, y arma `$STAGE` **descomprimiendo ese zip primero** (`unzip -q "$BUILD_DIR/rockbox.zip" -d "$STAGE"`) — con las fuentes/íconos/tema de Aura copiados **encima**, sin borrar lo que `make zip` ya puso (mismo criterio que D-178: los assets de Aura ganan si colisionan, todo lo demás del árbol real de Rockbox se conserva). Corrección de un supuesto que llevaba desde el script original: `.rockbox/fonts/` de `make zip` **no** está vacío — trae `15-Adobe-Helvetica.fnt` (la fuente de fábrica de Rockbox); el script viejo lo pisaba sin querer al crear el directorio desde cero con solo las 14 fuentes de Aura, y nadie lo notó porque el simulador (que sí tiene esa fuente, vía `make install`) nunca reveló la diferencia. Ahora se copia `cp -R design-system/out/fonts/. .../fonts/` (con punto final, no reemplaza el directorio) — mismo comportamiento que `build_sim.sh` siempre tuvo. `.rockbox/rockbox.ipod` ya viene dentro del zip de `make zip`; se quitó la copia redundante.

**Centinelas** (nueve rutas, `mpegplayer.rock`, 4 códecs representativos, `viewers.config`, una fuente de Aura, `icons/aura/masks`, `rockbox.ipod`) verificados antes del `zip -qr` final — el script aborta con mensaje claro si falta alguno, en vez de volver a publicar un `rockbox.zip` mudo. Aviso nuevo (bloqueante solo con `--release-tag`): árbol de git sucio → aborta, para que ningún release futuro repita la `M` de `rockbox-info.txt` que D-296 ya había notado sin perseguir la causa (`cffffd9f2bM` en `v0.2.0-beta` — build sobre árbol con cambios sin commitear).

**Resultado**: `rockbox.zip` pasa de ~6.7 MB / 9044 archivos a **~16 MB / 9463 archivos** (43 códecs, 168 `.rock`, incluido `mpegplayer.rock`, `viewers.config`, `codepages/`, `langs/`, `.rockbox/debug/rockbox.map`). `firmware/dist/README.md` corregido (la descripción "códecs, plugins" ya existía ahí como texto aspiracional falso; ahora es cierta, con la nota de qué se rompió y por qué).

**Verificación**: `build_sim.sh` limpio (sin recompilar, nada de C tocado en esta pasada); `package_dist.sh` completo limpio, sin warnings nuevos (incremental, no recompiló `apps/aura/`); `make test` **11/11** sin cambios (no se tocó C). Sin iPod en esta sesión: la prueba real (¿aparecen ahora `.rockbox/rocks/` y `.rockbox/codecs/` tras instalar este build?) queda para el protocolo de hardware de `PLAN-sync-media-hardening.md` §F0.5.

## D-298 — F0.2: versión visible en "Acerca de", mensaje propio (español) al no poder abrir un video, paridad de nombre largo en `/Videos/`

**Encargo**: F0.2 de `PLAN-sync-media-hardening.md` (carpeta padre) — tres correcciones puntuales antes de cortar `v0.2.1-beta`, todas descubiertas o motivadas por el diagnóstico de D-297 (video "no se puede visualizar" en hardware).

**1. Versión de firmware visible.** No había forma de saber, mirando el aparato, con qué Release estaba instalado — un reporte de hardware nunca era contrastable contra un commit real. `aura_device.c`/`.h`: `aura_firmware_version()` lee `.rockbox/aura/version.txt` (una sola línea, escrita por `package_dist.sh --release-tag`; **no** es `clave: valor` como `device.cfg`, así que usa `read_line()` a secas, no `settings_parseline()`) — recargado junto con el nombre del dispositivo en `aura_device_reload()`. `aura_screens.c`: `credits_body_with_version()` añade "Versión / `<tag>`" (o "Versión de desarrollo" sin `version.txt`, build local) al final del cuerpo de créditos de "Acerca de" (página 3, `ABOUT_PAGE_DEVICE`). Dos strings nuevas por idioma.

**2. `plugin_load()` mostraba su propio splash nativo en inglés antes de que `aura_video.c` pudiera hacer nada.** El primer intento — revisar el valor de retorno de `plugin_load()` (`< 0` = `PLUGIN_ERROR`, único caso real de falla; los sentinelas positivos como `PLUGIN_USB_CONNECTED` no lo son) y mostrar un mensaje propio en español — **no alcanzaba**: confirmado en el simulador (quitando `mpegplayer.rock` de `simdisk/` a propósito) que `plugin_load()` (`apps/plugin.c`, ambas ramas de error: archivo ausente/corrupto en `lc_open()`, línea ~947; versión de API incompatible, línea ~971) llama a `splash()`/`splashf()` **internamente, de forma incondicional**, antes de devolver `-1` — un splash de Rockbox nativo, en inglés, cromo visible sin ninguna forma de evitarlo revisando solo el valor de retorno (viola la regla de diseño "cero cromo de Rockbox visible", `CLAUDE.md`). Arreglo real, en `apps/plugin.c`/`.h` (fuera de `apps/aura/`, registrado en `MODIFICATIONS.md`): `plugin_set_silent_open_errors(bool)`, flag opt-in (`false` por defecto, sin efecto en ningún otro llamador de Rockbox) que silencia esos dos `splash()` cuando está activo. `aura_video.c` lo activa justo antes de su `plugin_load()` y lo desactiva justo después; si `ret < 0`, dibuja `show_cant_open_video()` (pantalla completa, "No se pudo abrir el video" / "Reinstala Aura desde Aura Studio", 2 s o hasta soltar cualquier botón).

**2b. Bug real encontrado al verificar el mensaje propio en el simulador (no solo un defecto de la herramienta de pruebas): `show_cant_open_video()` se cerraba sola casi al instante, mucho antes de los 2 s pedidos.** Causa: esperaba con `button_get_w_tmo(HZ*2)` a secas, y ese `BUTTON_SELECT` que abrió el mensaje **todavía tiene su propio evento de "soltar" (`BUTTON_REL`) pendiente** — en hardware real, el usuario sigue con el dedo sobre el botón al momento en que aparece el mensaje, así que ese `REL` llega casi de inmediato y `button_get_w_tmo()` lo toma como si fuera una pulsación nueva, cerrando el mensaje en un puñado de ticks. **No es un artefacto exclusivo del simulador** — la misma secuencia (pulsación → falla → mensaje → `REL` de la pulsación original todavía pendiente) ocurre igual en el aparato. `next_button()` (`aura_main.c`) ya filtra `BUTTON_REL` para todo el resto de la app; `show_cant_open_video()` llamaba a la primitiva cruda directamente, sin pasar por ese filtro. Arreglo: `wait_dismiss()`, un lazo local en `aura_video.c` que descarta cualquier `BUTTON_REL` y solo cierra el mensaje ante una pulsación nueva de verdad o el timeout real.

**3. Paridad de nombre largo en `/Videos/`.** `VIDEO_NAME_LEN` era 64 (vs. `PHOTO_NAME_LEN` en Fotos, ya en 96) — un nombre de archivo más largo se truncaba en silencio al leer el directorio, y el archivo truncado no coincidía con ninguno real al intentar abrirlo (mismo síntoma "no se pudo abrir" que D-297, pero por una causa distinta y ya cubierta por el mensaje del punto 2). Subido a 96, paridad exacta con Fotos. `docs/contracts/library-layout-v1.md` sube a **v1.1** (fila `/Videos/`: nota de límite de 95 bytes UTF-8 con extensión) — no toca el campo `version` del marcador de §4 (no es un cambio de su esquema), copiado idéntico a `Aura-Studio/docs/contracts/library-layout-v1.md` (`cmp` verificado). De paso, `aura_video.c` gana la fila inerte "…y N más" cuando hay más de `MAX_VIDEOS` (100) archivos — mismo patrón que Fotos (D-291): `s_video_total_count` sigue contando más allá de `MAX_VIDEOS` en vez de cortar el `while` de `ensure_video_list()`, fila dimmed añadida al final de `items[]` (ahora `MAX_VIDEOS + 1`) sin entrar en el rango seleccionable de `aura_video_handle_button()`.

**Verificado en el simulador** (`build_sim.sh` limpio, capturas con `apple2026_sim_shot.sh`, forzando cada caso a mano):
- "Acerca de" → créditos, con y sin `.rockbox/aura/version.txt` inyectado a mano: "Versión / v0.2.1-beta" y "Versión de desarrollo" respectivamente, ambos visibles al hacer scroll hasta el final de la página (texto largo, hay que bajar bastante).
- Mensaje de video sin abrir (quitando `mpegplayer.rock` de `simdisk/` temporalmente): **antes** del punto 2b, capturado el splash nativo en inglés ("Can't open /.rockbox/rocks/viewers/mpegplayer.rock") apareciendo solo, y por separado el bug de cierre instantáneo del mensaje propio (invisible en cualquier captura entre tick 5 y tick 400 pese a que `plugin_load()` sí devolvía `-1`, confirmado con un `fprintf` de diagnóstico temporal, retirado antes de este commit). **Después** de los dos arreglos: mensaje propio en español visible de inmediato, sin splash nativo, se sostiene los 2 s y luego vuelve sola a la lista de Videos.
- Fila "…y N más": 111 archivos `.mpg` de prueba en `simdisk/Videos/` (retirados al terminar) → "…y 11 más" visible al final de la lista, atenuada, no seleccionable.
- `make -C firmware/rockbox/apps/aura/test test` **11/11**, sin cambio en los conteos (este pase es de UI/plugin, sin lógica nueva testeable en host).

**Archivos**: `apps/aura/aura_device.c`/`.h` (nuevo), `apps/aura/aura_screens.c`, `apps/aura/aura_lang.c`/`.h` (4 strings × 2 idiomas), `apps/aura/aura_video.c`, `apps/plugin.c`/`.h` (fuera de `apps/aura/`, en `MODIFICATIONS.md`), `docs/contracts/library-layout-v1.md` (v1.1, copiado a Aura Studio).

**Pendiente**: F0.3 (cortar `v0.2.1-beta`), F0.4 (Aura Studio consume el Release y verifica contenido del zip), F0.5 (protocolo de hardware) — siguientes pasos de `PLAN-sync-media-hardening.md`.

## D-299 — Empaquetado de `v0.2.1-beta` (PATCH), publicación pendiente de OK del dueño

**Encargo**: F0.3 de `PLAN-sync-media-hardening.md`. PATCH, no MINOR: ningún contrato de datos cambió (`CONTRATO-firmware-studio.md` §E) — la subida de `docs/contracts/library-layout-v1.md` a v1.1 en D-298 documenta un límite que ya regía de facto (`PHOTO_NAME_LEN` ya era 96; Studio nunca tuvo que cambiar nada), no una nueva obligación para Aura Studio.

**Empaquetado local** (`package_dist.sh --release-tag v0.2.1-beta`, árbol limpio tras el commit de D-298): limpio, sin warnings nuevos. Centinelas (D-297) verificados: **9431 archivos**. `unzip -p rockbox.zip .rockbox/aura/version.txt` → `v0.2.1-beta` exacto. Confirmados dentro del zip: `mpegplayer.rock`, `viewers.config`, `mpa.codec`, `flac.codec`, `a26-title-20.fnt`, `icons/aura/masks/`. `checksums.txt` con los 4 binarios (`rockbox.zip`, `rockbox.ipod`, `mks5lboot`, `bootloader-ipod6g.ipod` — este último sobrevive intacto de la compilación manual de D-290, sin cambios en su fuente desde entonces).

**Publicación NO ejecutada en esta pasada** — el propio plan (F0.3) pide dejarla para el dueño ("Publicar solo con OK del dueño: dejar borrador y el comando"), y son acciones visibles hacia fuera (push, tag, Release público) que no se toman sin confirmación explícita. `main` local queda 2 commits adelante de `origin/main` (D-297, D-298/F0.2) sin empujar, sin tag creado. Comando exacto para publicar, una vez confirmado:

```bash
git -C Aura-Firmware push origin main
git -C Aura-Firmware tag -a v0.2.1-beta -m "v0.2.1-beta"
git -C Aura-Firmware push origin v0.2.1-beta
gh release create v0.2.1-beta \
  firmware/dist/rockbox.zip firmware/dist/rockbox.ipod firmware/dist/mks5lboot \
  firmware/dist/bootloader-ipod6g.ipod firmware/dist/AuraPalette.swift \
  firmware/dist/MODIFICATIONS.md firmware/dist/THIRD-PARTY-NOTICES.txt \
  firmware/dist/theme-format-v1.json firmware/dist/aura-theme-default.zip \
  firmware/dist/checksums.txt \
  --title "v0.2.1-beta" --prerelease \
  --notes "Corrige el rockbox.zip de v0.1.0-beta/v0.2.0-beta, que no incluía códecs ni plugins (sin video; sin audio en instalaciones desde cero) -- D-297. Si ya tenías Aura instalado, actualiza desde Aura Studio -> Actualizar Aura."
```

**Publicado** (OK del dueño recibido en la misma sesión): `main` empujado (`5d1af20..2da4c29`), tag anotado `v0.2.1-beta` empujado, Release creado como pre-release con los 10 assets — https://github.com/Ricolinos/Aura-Firmware/releases/tag/v0.2.1-beta

**F0.4 hecho del lado de Aura Studio** (ST-018 allá): pin a `v0.2.1-beta`, centinela de contenido de `rockbox.zip` (checksum correcto no detecta un zip incompleto como el de D-297 — verificación nueva por contenido real del zip), y de paso la tabla §E de `CONTRATO-firmware-studio.md` de este repo (que llevaba desde D-296 con el placeholder viejo sin actualizar) se sincronizó completa con la copia de Studio, `cmp` limpio.

**Pendiente**: F0.5 (protocolo de hardware, a cargo del dueño).

## D-300 — Morph de entrada al Modo 4 (Letras): la fase de sombra ya no repinta la pantalla completa por nada

**Encargo**: el dueño, tras confirmar en hardware real que música/carátulas/letras ya funcionan bien (D-298/D-299, ST-019), reportó que el morph de entrada al modo Letras "se siente lento" y pidió una optimización para que se sienta tan fluido como el resto de la interfaz.

**Instrumentación primero** (mismo criterio que `TRANSITION_LOG` en `aura_transitions.c`, Fase 16 de PLAN-UX.md: "decidir con datos reales, no a ojo, si un modo gráfico necesita degradar en hardware real"): `mode4_morph()` no tenía ninguna medición — se le agregó el mismo patrón de `DEBUGF` con `current_tick`, separando el barrido principal de la fase extra. En el simulador (hardware rápido, no representativo del ARM real del iPod) el tiempo total ya estaba cerca del presupuesto nominal (330 ms + fase de sombra) — confirma que el simulador no sirve para medir el costo real por cuadro aquí, solo para verificar corrección visual antes/después.

**Causa estructural encontrada por lectura de código** (no por medición, dado que el simulador no la revela): `draw_m4_right_panel()` hace un blend por pixel de TODO el panel derecho (~190×240px, tinte de "vidrio" con degradado diagonal calculado por pixel, `m4_grad_diag()`, sin cachear) más la franja de sombra del panel izquierdo (8px de ancho). La fase extra del morph (fundido de esa sombra, ~la mitad de los cuadros del barrido principal) llamaba a `draw_player()` completo —con `a26_shell_clear_screen()` antes— en cada uno de sus cuadros, pese a que en esa fase `t256` está fijo en 256: la carátula (ya cacheada por `aura_flow_*`), el texto, el progreso, el transporte y el TINTE DE VIDRIO COMPLETO del panel derecho son bit-a-bit idénticos cuadro a cuadro — lo único que cambia de verdad es esa franja de 8px de la sombra del panel izquierdo. La versión vieja repetía el blend de ~45 000 píxeles del panel entero para producir, en el 100% de los casos, el mismo resultado ya visible en pantalla.

**Arreglo**: la fase extra ya no llama a `draw_player()`. Al entrar a esa fase (con la sombra todavía en 0, el estado exacto en el que quedó el último cuadro del barrido principal) se captura una sola vez la franja de 8px × 240 ya teñida de vidrio (`stripe_base`, arreglo `static` — mismo patrón de caché que `s_cover_tilted_buf`, ~3.75 KB). Cada cuadro siguiente de la fase de sombra solo recalcula el segundo blend (sombra sobre esa base fija) para esos 8 píxeles de ancho, y usa `lcd_update_rect()` acotado a esa franja en vez de voltear la pantalla completa (mismo patrón ya usado 7 veces en `aura_transitions.c`, nunca antes en `aura_nowplaying.c`). Matemáticamente equivalente a la fórmula original (`a26_shell_blend` es exactamente no-op en alpha ≤ 0, así que capturar la franja en el instante en que la sombra todavía es 0 produce el mismo valor base que recalcular el tinte desde cero).

**Verificado**: capturas del simulador antes/después de la fase extra — sin artefactos ni costura visible en el borde del panel. `make test` **11/11**. Build ARM real limpio (`firmware/build-ipod6g`, sin warnings nuevos — las dos variables de instrumentación quedan sin usar cuando `DEBUGF` es no-op fuera de builds DEBUG, silenciadas con `(void)` como ya hace `TRANSITION_LOG`). El barrido PRINCIPAL (donde la geometría sí cambia cuadro a cuadro, redibujo completo genuinamente necesario) no se tocó — sin datos de hardware real que justifiquen arriesgar ese camino todavía.

**Pendiente**: el dueño prueba en hardware real si el morph ya se siente fluido; si sigue lento, la instrumentación nueva (`DEBUGF`, visible con un build de depuración o revisando `aura_nowplaying: mode4_morph ... en N ticks` en el log) da el dato real que hacía falta para decidir el siguiente paso (candidato más probable si aún no alcanza: el barrido principal en sí, o el propio `draw_art_soft_shadow`/proyección de columnas de la carátula).

## D-301 — Versión visible sin scroll en "Acerca de"

**Encargo**: "sube la información de la versión en la sección Acerca de dentro de Ajustes, en el iPod, para que esté más accesible" — la ubicación de D-298 (al final del cuerpo de créditos, página 3 de "Acerca de") obligaba a bajar todo el texto con la rueda para verla, confirmado al verificar D-298/D-299/D-300 en el simulador esta misma sesión (30 clics de rueda para llegar).

**Hecho**: `credits_body_with_version()` (`aura_screens.c`) ya no concatena el bloque de versión al FINAL del cuerpo de créditos — lo inserta justo después de la primera línea ("Aura"), antes de "Creado por Ricardo Gómez.". Sin tocar las cadenas traducidas (`AURA_STR_ABOUT_CREDITS_BODY` de `aura_lang.c`, ES/EN): se parte el cuerpo en la primera `\n` (`strchr`) y se arma `"Aura\n" + bloque-de-versión + "\n\n" + resto-del-cuerpo-original` en el mismo buffer estático de 700 bytes de siempre. Mismas dos ramas que D-298 (versión real del Release, o "Versión de desarrollo" sin `version.txt`).

**Verificado en el simulador**: capturas de la página de créditos nada más entrar (sin ningún scroll) — la versión aparece de inmediato bajo "Aura", en ambas ramas (con `version.txt` inyectado a mano y sin él). `make test` **11/11**. Build ARM real limpio, sin warnings nuevos (`-Wtype-limits` de siempre en `aura_style.c`, sin relación).

## D-302 — Sidecars AppleDouble de macOS ("._Nombre") ya no se listan como foto/video real

**Encargo**: el dueño reportó en hardware real (tras probar PARTE 2A de Studio) que muchas fotos se ven bien, pero otras "no se ven" — con nombres que empiezan con `._`.

**Causa**: `is_listable_image()` (`aura_photos.c`) e `is_video_file()` (`aura_video.c`) deciden solo por EXTENSIÓN. Un sidecar AppleDouble de macOS (`._Foto.jpg`, el resource fork/xattrs que macOS deja junto al archivo real al escribirlo en un volumen sin ese soporte nativo — el FAT32 del iPod, o al copiar/extraer desde un ZIP/USB de origen) comparte la extensión del archivo real pero su contenido no es una imagen/video decodificable — se listaba igual y nunca abría. Se investigó primero si Aura Studio los estaba importando como si fueran fotos reales (0 evidencia en `biblioteca.json` real del dueño) — la causa está del lado del FIRMWARE, que lista lo que sea que encuentre en `/Photos/`/`/Videos/` sin filtrar estos sidecars, sin importar cómo llegaron ahí.

**Hecho**: `is_apple_double_sidecar()` (nueva, un chequeo trivial: `name[0]=='.' && name[1]=='_'`) en ambos archivos — descarta esos nombres antes de cualquier chequeo de extensión.

**Verificado**: `make test` **11/11**. Build ARM real limpio.

## D-303 — Visor de fotos: modo "cubrir" (llenar pantalla, recortando) alternado con Select

**Encargo**: "que una vez abierta la imagen, al darle Select, se alterne entre dos modos: imagen a pantalla completa (como está ahorita) y cubriendo los bordes (escalando la imagen lo suficiente para que cubra toda la pantalla, pero sin que se deforme)".

**Diseño, con los límites reales del decodificador de JPEG de Rockbox por delante** (no a ciegas):
1. El decoder (`apps/recorder/jpeg_load.c`) escala SOLO hacia abajo durante la decodificación (dominio DCT, potencias de 2) — nunca agranda. El factor de "cubrir" (`max(pantalla/ancho, pantalla/alto)`) se acota a 1.0: una foto más chica que la pantalla en el eje que le falta no se agranda más allá de su tamaño real — límite real del hardware, no un descuido. Con la calidad "Optimizada" (320px) de Studio esto es frecuente (una foto casi del tamaño de la pantalla no tiene margen para "cubrir" de verdad); con "HD" (640px) casi siempre hay margen de sobra.
2. `s_view_scratch` es un buffer fijo (`VIEW_SCRATCH_SIZE`, 240 KiB) — una foto "HD" en modo cubrir puede pedir más píxeles de los que caben (640×640 a 16bpp ya excede el buffer). Si el tamaño ideal no entra, se reduce proporcionalmente (raíz cuadrada entera por bisección, sin FPU) hasta que sí.

**Hecho** (`aura_photos.c`): `s_cover_mode` (alternado con Select, reiniciado a "ajustar" cada vez que se entra al visor desde la lista — no persiste entre fotos de sesiones distintas, sí mientras se navega dentro de una misma sesión abierta). `compute_cover_target()` calcula el tamaño objetivo de decodificación con los dos topes de arriba, aritmética en punto fijo Q16.16 (mismo criterio sin-FPU que `aura_flow.c`, otro ancho de shift). El dibujo final se unificó a una sola fórmula con `lcd_bitmap_part()` (antes `lcd_bitmap()` simple) que centra con bandas cuando el bitmap decodificado es más chico que la pantalla ("ajustar") Y recorta del centro cuando es más grande ("cubrir") — el caso degradado por el tope de memoria cae solo en la misma fórmula. Select ya no cierra el visor (eso quedó solo en Menú) — ahora alterna el modo, forzando `s_loaded_index = -1` para redecodificar con el tamaño objetivo nuevo.

**Bug real encontrado verificando en el simulador** (no solo en teoría): al reabrir la MISMA foto que se había dejado en "cubrir", `s_cover_mode` sí se reiniciaba a "ajustar" pero la pantalla seguía mostrando "cubrir" — `s_loaded_index` ya coincidía con `s_current_index` de la sesión anterior, así que `load_current_photo()` se saltaba la redecodificación. Corregido invalidando `s_loaded_index` también en el punto donde se reinicia `s_cover_mode`, no solo en el toggle de Select.

**Verificado en el simulador**: "Portada B.jpg" (320×278, requiere recorte vertical real sin necesitar agrandar) — captura en "ajustar" (barras finas) y en "cubrir" (llena la pantalla, recorte visible arriba/abajo, sin deformar el arte); reabrir la misma foto tras salir confirma el reinicio a "ajustar". `make test` **11/11**. Build ARM real limpio.

**Corrección tras prueba en HARDWARE REAL** (el dueño, tras `v0.2.3-beta`: "aún no se ve la imagen ocupando la pantalla completa... aunque se corten los bordes"): el tope de la premisa 1 de arriba ("el decoder solo reduce, nunca agranda") describía mal el límite real -- ese límite existe de verdad SOLO para el paso de escalado en el dominio DCT (potencias de 2, adentro de `jpeg_load.c`), pero el árbol de Aura ya enlaza `apps/recorder/resize.c` con `HAVE_UPSCALER` definido incondicionalmente (`resize.h:43`) -- el paso de *reescalado en dominio de píxeles* que sigue al DCT (`recalc_dimension()`) SÍ agranda cuando hace falta, confirmado con "Portada B.jpg" en el propio Modo "ajustar": una foto de 320×278 se veía agrandada por `recalc_dimension` sin que nadie se lo hubiera pedido explícitamente. El bug real estaba en mi PROPIO código: `compute_cover_target()` capaba el factor de "cubrir" a 1.0 por una premisa equivocada -- una restricción mía, no del hardware -- y por eso las fotos de calidad "Optimizada" (320px, sin margen de sobra) se quedaban sin agrandar.

**Arreglo real**: en vez de confiar en que el decoder agrande durante la decodificación (in-scope de `jpeg_load.c`/`resize.c`, sin verificar a fondo su comportamiento exacto con un target explícito mayor al de origen sin `FORMAT_KEEP_ASPECT`), se separaron dos tamaños -- `compute_decode_and_display_size()` reemplaza a `compute_cover_target()`: `decode_w/h` (lo que se le pide al decoder, tope de memoria aparte, nunca mayor al tamaño de origen -- eficiente y sin depender de un camino de agrandado del decoder sin verificar) y `display_w/h` (el tamaño FINAL en pantalla, sin ningún tope salvo el propio de la pantalla -- puede exceder `decode_w/h`). `draw_scaled_centered()` (nueva, reemplaza el `lcd_bitmap_part()` de antes) hace el agrandado DE VERDAD por muestreo nearest-neighbor cuando `display > decode`, y sigue siendo un blit 1:1 sin muestreo cuando coinciden (el caso normal de "ajustar" y de "cubrir" sin necesitar agrandar más allá de lo decodificado).

**Verificado en el simulador con un fixture que SÍ necesita agrandar** (una foto cuadrada 200×200 generada a propósito, con cruz y marcas de esquina para que el recorte se note): "ajustar" la muestra a 240×240 con bandas negras a los lados; "cubrir" ahora SÍ llena las 320×240 completas, sin ninguna banda, recortando arriba/abajo -- la cruz sigue centrada y sin deformar, confirmando que el agrandado no distorsiona proporciones. Reabrir la foto tras salir sigue reiniciando a "ajustar" correctamente. `make test` **11/11**. Build ARM real limpio (mismo `-Wtype-limits` de siempre en `aura_style.c`).

## D-304 — Reproductor de video: menú de ajustes en español + modo "cubrir pantalla" con Select

**Encargo**: tras confirmar que los videos ya se reproducen bien en hardware real (F0.2/D-298 y ST-026), el dueño reportó que "se sigue mostrando la interfaz de Rockbox del reproductor de videos" y pidió (1) terminar de alinear esa interfaz con el sistema de diseño, y (2) agregar el mismo ajuste "ajustar/cubrir" del visor de fotos (D-303), alternable con Select durante la reproducción y desde el menú de ajustes del propio reproductor.

**Diagnóstico** (agente de exploración de solo lectura sobre `apps/plugins/mpegplayer/`, antes de tocar nada): el OSD que se ve sobre el video (tiempo, volumen, barra de progreso) ya estaba re-vestido con la paleta de Aura desde D-062/D-069 -- lo que realmente seguía siendo "Rockbox puro" era el **menú de ajustes** (tecla Menú durante la reproducción → `mpeg_menu()`/`mpeg_settings()` en `mpeg_settings.c`), construido con el widget de lista nativo de Rockbox (`rb->do_menu()`, distinto del widget propio de Aura) y con **absolutamente todo su texto en inglés**: no por un descuido de traducción, sino porque usa `rb->str(LANG_X)` y, por D-013, Aura nunca carga el sistema de idiomas `.lang` de Rockbox -- esas llamadas resuelven a inglés de fábrica sin excepción, sin importar cuántas veces se revise. También se encontró que el menú de "Reanudar" (`mpeg_start_menu()`) fue ya neutralizado por D-062 pero dejó atrás una entrada de submenú (`resume_options()`, "Resume options") que edita una variable que el código ignora a propósito -- un ajuste que no hacía nada, visible y confuso.

Sobre "cubrir pantalla": investigar el escalado de video reveló que la premisa de la que partía el encargo (bandas negras que el firmware podría recortar) no reflejaba el estado real del pipeline: `FFmpegTranscoder.arguments()` (Aura Studio) escalaba **y rellenaba con `pad` a exactamente 320×240** -- el video que le llega al firmware siempre tenía las bandas negras HORNEADAS como píxeles reales dentro del archivo, así que `vo_setup()` (`video_out_rockbox.c`) nunca veía una secuencia más chica que la pantalla y su lógica de centrado/recorte (que en teoría ya existía) era letra muerta. Sin corregir esto en Studio, "cubrir" en el firmware solo habría recortado bandas negras ya rasterizadas, dejando el contenido real del video sin agrandar -- el mismo tipo de error de premisa que D-303 corrigió del lado de fotos.

**Hecho, Aura Studio** (`FFmpegTranscoder.swift`): se quitó el filtro `pad` -- ahora solo `scale=320:240:force_original_aspect_ratio=decrease:force_divisible_by=2` (preserva el ancho o alto real del contenido, par para el submuestreo 4:2:0 de MPEG-2). El `.mpg` resultante conserva su aspecto real en la cabecera de secuencia; es el firmware el que decide en tiempo de reproducción si deja franjas o recorta.

**Hecho, firmware**:
- `mpeg_settings.h`/`.c`: nuevo campo `settings.scale_mode` (`MPEG_SCALE_MODE_FIT`/`_COVER`), persistido en `mpegplayer.cfg` (`SETTINGS_VERSION` 5→6), con entrada "Modo de ajuste" en el submenú "Opciones de pantalla". Se tradujeron al español TODOS los menús y `opt_items` de este plugin (títulos, ítems, "Sí"/"No", etc.) reemplazando cada `ID2P(LANG_X)`/`rb->str(LANG_X)` por literales -- `MENUITEM_STRINGLIST`/`struct opt_items` aceptan strings planos igual que `LANG_X` (confirmado con precedente ya existente en `apps/plugins/fireworks.c`/`otp.c`), sin necesitar el sistema de voz/talk que Aura tampoco usa. Se eliminó la entrada de menú "Resume options" y su función (`resume_options()`) por ser vestigial desde D-062 -- un ajuste que no tenía ningún efecto observable, ahora ya no aparece.
- `mpegplayer.c`: la tecla SELECT (libre para `ipod6g` en este keypad, confirmado por el mismo agente -- no pisa ningún otro gesto) ahora alterna ajustar/cubrir **solo para la sesión actual de reproducción**, sin persistir el cambio -- mismo criterio que `s_cover_mode` en el visor de fotos (D-303): cada video nuevo vuelve a arrancar en el default guardado en ajustes. Se corrigió además un color semánticamente invertido en la barra de progreso del OSD: el tramo YA reproducido se pintaba en blanco liso y el POR reproducir en el acento rosa (`#FF456C`) -- al revés de como se usa en el resto de la app, donde el acento no aparece nunca en sliders de progreso (`dark.progress_fill`/`progress_track` de `tokens.json` son gris claro/gris oscuro, no el acento). Ahora el tramo reproducido usa `progress_fill` (`#E5E5EA`) y el restante `progress_track` (`#48484A`), como en el resto de Aura. Todos los splashes de error en inglés (`mpegplayer.c`, `stream_mgr.c`) se tradujeron al español.
- `video_out_rockbox.c`/`video_out.h`: el rectángulo de destino (`vo_recalc_rect()`, factoreado de `vo_setup()`) ahora depende de una copia de sesión del modo (`vo.scale_mode`, sincronizada con `settings.scale_mode` al iniciar cada video o al cambiarlo desde el menú, alternada en caliente por `vo_toggle_scale_mode()` sin esperar una nueva cabecera de secuencia). En modo "cubrir", `vo_draw_frame_cover()` (nueva) recorta -- dentro del frame YUV ya decodificado -- el área con la relación de aspecto exacta de la pantalla (sin deformar) y la escala a 320×240 con `stretch_image_plane()` (nearest-neighbor, ya existía para la miniatura del extinto selector de inicio, reutilizada tal cual) antes de blitear. El buffer temporal (320×240 luma + 2×160×120 croma, 115200 B) sale de `mpeg2_get_buf()`, la memoria "sobrante" del arena de 2 MB de libmpeg2: una investigación previa confirmó que ese arena solo lo toca el hilo de video (decode y draw ocurren síncronos, mismo hilo, sin paralelismo real que proteger) y que sobran típicamente ~450 KB a 320×240 -- ampliamente suficiente. Si algún video excediera ese margen (resolución muy superior a la pantalla), se degrada al blit directo sin cubrir, mismo patrón de fallback que ya usaba `vo_draw_frame_thumb()`.

**Deliberadamente fuera de alcance esta vez**: los íconos de estado del OSD (play/pausa/avance/retroceso, `mpegplayer_status_icons_*x1` en `apps/plugins/bitmaps/mono/`) siguen siendo los bitmaps mono genéricos de Rockbox -- ya se dibujan con los colores de Aura (son bitmaps de 1 bit, toman el color del contexto de dibujo actual), pero su FORMA (flechas simples) no se tocó: rediseñarlos implica generar assets nuevos y no es lo que el dueño pidió esta vez. Tampoco se tocó `FONT_SYSFIXED` en las pantallas negras transitorias de entrada/salida del reproductor (`mpegplayer.c`, cambio de fuente momentáneo, cosmético).

**Verificado**: `make test` **11/11**. Build ARM real limpio (`firmware/build-ipod6g`, sin warnings nuevos -- mismo `-Wtype-limits` de siempre en `aura_style.c`; hubo que agregar una declaración adelantada de `stretch_image_plane()` porque `vo_draw_frame_cover()` quedó antes de su definición original en el archivo). Simulador: el menú de ajustes y los splashes no se pudieron verificar visualmente por captura automática -- la inyección de botones (`AURA_SIM_BUTTONS`) no logró entrar de forma confiable a la reproducción/menú del plugin dentro de esta sesión (mismo problema, sin resolver, que ya había bloqueado la verificación visual de la reproducción de video en la sesión anterior); la build del simulador sí compila y enlaza limpio. Verificación real de esto queda, como el resto de F0.x, a cargo del dueño en hardware.

**Pendiente**: confirmación del dueño en hardware real -- especialmente si el modo "cubrir" se siente fluido en reproducción real (el escalado nearest-neighbor corre una vez por cuadro decodificado; no se perfiló en el ARM real, solo se verificó que compila y que la memoria alcanza).

## D-305 — Corrige el parpadeo del OSD en modo "cubrir" + barra de progreso en píldora delgada

**Encargo**: el dueño probó `v0.2.5-beta` en hardware real y reportó dos problemas: "no funciona el modo de ajuste 'cubrir'... mientras esté la interfaz de reproducción, esta parpadea, no el video, solo la interfaz (la barra de progreso y los iconos)"; y, por separado, pidió que la barra de progreso fuera "más delgada y con los extremos redondeados (no cuadrados como actualmente está)".

**Causa del parpadeo -- bug real en mi propio código de D-304, no en la arquitectura heredada**: `osd_show()` (`mpegplayer.c`) ya recortaba correctamente el área de destino del video para excluir la franja del OSD mientras está visible -- llama a `stream_vo_set_clip()` con `{0, 0, SCREEN_WIDTH, osd.y}`, un mecanismo de Rockbox que ya existía en el árbol antes de este trabajo (`VIDEO_SET_CLIP_RECT`, `video_thread.c`/`stream_mgr.c`) y que el blit normal (modo "ajustar") ya respetaba desde siempre, usando `vo.output_x/y/width/height` (la intersección resultante) en vez de la pantalla completa. `vo_draw_frame_cover()` (la función nueva de D-304 para "cubrir") ignoraba por completo ese recorte y bliteaba SIEMPRE `0,0..SCREEN_WIDTH,SCREEN_HEIGHT` -- tapando la barra de progreso y los iconos en cada cuadro decodificado, un parpadeo constante entre "el video la tapa" y "el próximo refresco del OSD la vuelve a dibujar encima". El modo "ajustar" nunca tuvo este bug porque su blit ya usaba el rectángulo recortado desde el origen.

**Hecho**: `vo_draw_frame_cover()` ahora blitea solo `vo.output_x/y/width/height` (tanto como origen como destino, ya que el buffer temporal que arma se construye con coordenadas 1:1 a las de pantalla) en vez de la pantalla completa siempre.

**Nota honesta sobre "no funciona"**: la queja de que "cubrir" no se ve distinto de "ajustar" puede tener una segunda causa, no un bug: si el video que se probó fue sincronizado ANTES de actualizar Aura Studio a la versión que trae ST-027 (que quitó el relleno de franjas horneadas del `.mpg`), ese archivo específico sigue siendo exactamente 320×240 con las franjas ya rasterizadas como píxeles -- no hay nada que "cubrir" recorte de más ahí, el modo se ve idéntico al ajustar por diseño (nada que el firmware pueda arreglar del lado de un archivo ya transcodificado con el pipeline viejo). Solo videos re-sincronizados con Studio actualizado muestran una diferencia real entre ambos modos.

**Barra de progreso en píldora**: `draw_scrollbar_draw()` dibujaba un rectángulo grueso con borde de 1px y esquinas cuadradas (heredado tal cual de Rockbox). Se reemplazó por una franja fija de 4px de alto (`PROG_PILL_HEIGHT`), centrada verticalmente dentro del mismo rectángulo asignado de siempre (no se tocó el layout general del OSD, solo cómo se pinta dentro de su caja), sin borde. Los extremos redondeados se aproximan con una tabla de inset de 1px en la fila superior/inferior y 0px en las del medio (radio 2, la escala es tan chica que un círculo de verdad no se distinguiría de esta aproximación octogonal). El segmento "ya reproducido" y "por reproducir" ahora comparten el mismo radio en su borde compartido -- el extremo izquierdo de la píldora completa (track) y el extremo izquierdo del relleno (fill) son geométricamente idénticos, sin costura visible; el relleno tiene el borde derecho recto donde se encuentra con el track (o, en 0%/100%, se dibuja la píldora completa de un solo color con ambos extremos redondeados, sin costura en absoluto).

**Verificado**: `make test` **11/11**. Build ARM real limpio -- hubo que eliminar `draw_vline()` (quedó sin ningún llamador tras quitar el borde rectangular; se removió por completo en vez de dejarla muerta). Build del simulador limpio.

## D-306 — El reproductor de video ahora respeta modo claro/oscuro, tema y acento del usuario

**Encargo**: mismo reporte de hardware que D-305 -- "hay que ajustar toda la interfaz de ajustes del reproductor, ya que no respeta los ajustes de personalización (el modo, el tema, los iconos, el color de acento etc)".

**Investigación de viabilidad primero** (agente de exploración de solo lectura): confirmado que un plugin SÍ puede leer la configuración real de Aura sin tocar la tabla `rb->` -- `aura_settings_save()` (`apps/aura/aura_settings.c`) escribe `/.rockbox/aura/aura.cfg` en texto plano (`clave: valor`), con las claves `theme` (0=claro/1=oscuro), `accent_rgb24` (hex `RRGGBB`) y `theme_id` (id del "Estilo" activo, vacío = default compilado); si `theme_id` no está vacío, la paleta real vive en `/.rockbox/aura/themes/<theme_id>/theme.cfg`, mismo formato, claves `palette_{light,dark}_{shell_bg,text_primary,progress_fill,progress_track}`. Es exactamente el mismo mecanismo genérico de archivo (`rb->open`/`rb->read_line`/`rb->settings_parseline`) que ya usa `configfile_load()` para `mpegplayer.cfg` -- no hace falta ninguna función nueva en la tabla de plugin.

**Fuera de alcance, evaluado y descartado por ahora**: el sistema de íconos por tema (D-289) usa un modelo de composición alfa por máscara de 24 bits contra el framebuffer (`draw_icon_mask_2()` en `apps/aura/aura_widgets.c`), incompatible con el blit mono de 1bpp que usa `mpegplayer.c` hoy -- portar ese algoritmo al plugin es viable en principio pero grande, y además no existe hoy ningún `icon_key` reservado para los íconos de video en el contrato de formato de tema (`CONTRATO-formato-tema.md`), así que agregar el soporte en el firmware no tendría ningún efecto real hasta coordinar un cambio de `theme_format` con Aura Studio. Se deja como ítem de roadmap aparte, documentado para no perderlo.

**Hecho, `apps/plugins/mpegplayer/mpegplayer.c`**: `aura_load_personalization()` (nueva) abre `aura.cfg` con las primitivas de archivo genéricas, parsea `theme`/`theme_id`/`accent_rgb24`, y si hay un `theme_id` válido (mismo alfabeto y límite que `aura_style_id_is_valid()`, re-validado acá porque el valor pasa a formar parte de una ruta de archivo -- nunca se confía en el contenido de `aura.cfg` para eso) intenta también `themes/<id>/theme.cfg` para la paleta del modo activo. Ante CUALQUIER archivo ausente, clave faltante o valor malformado, cae a los literales compilados de siempre (Oscuro, Fase 20/D-062/D-304) -- mismo criterio de "fallback de seguridad" que ya exige `apps/aura/aura_style.c` para el sistema de temas. `osd_init()` llama a esto en vez de hardcodear los 4 colores del OSD directamente. El acento se aplica al trazo principal del ícono de estado (play/pausa/avance/retroceso) -- el único lugar del OSD donde tiene un punto de aplicación natural, ya que la barra de progreso deliberadamente NO usa acento en el resto de la app (D-304).

**Hecho, `apps/aura/aura_settings.c`/`.h`/`aura_main.c`/`aura_screens.c`**: hallazgo colateral investigando esto -- el menú de ajustes NATIVO del reproductor (`rb->do_menu()`, el que se abre con la tecla Menú) nunca tuvo este problema porque el plugin no controla su color en absoluto: usa `global_settings.fg_color`/`bg_color` del binario principal, que `apps/main.c` fija a la paleta Oscura EN EL ARRANQUE (D-055) y nunca se vuelve a tocar -- ni siquiera cuando el usuario cambia de modo después. Ese hardcodeo en `apps/main.c` sigue siendo correcto y deliberado para el momento exacto en que corre (antes de que Aura cargue `aura.cfg`, splash.c literalmente no puede saber el tema todavía), pero nada sincronizaba esos mismos campos una vez que sí se sabe. `aura_settings_sync_rockbox_theme_colors()` (nueva) hace esa sincronización -- se llama una vez justo después de `aura_settings_load()` en el arranque de Aura (`aura_main.c`), y de nuevo cada vez que el usuario alterna el modo desde Ajustes (`aura_screens.c`). Efecto colateral positivo, no buscado pero correcto: cualquier `splash()` re-vestido por D-055/D-056 después de este punto también empieza a respetar el modo real, no solo el menú de mpegplayer.

**Verificado**: `make test` **11/11**. Build ARM real limpio, sin warnings nuevos. Build del simulador limpio. No se pudo verificar visualmente en el simulador (mismo problema de siempre con la inyección de botones dentro del plugin) -- verificación real en hardware a cargo del dueño.

**Pendiente**: confirmación del dueño en hardware real, en ambos modos (claro y oscuro) y con al menos un tema/Estilo personalizado si tiene alguno instalado. Si el dueño quiere que los íconos de video también respeten el sistema de temas, es un proyecto aparte que empieza por reservar `icon_key`s nuevos en el contrato de formato de tema.

## D-307 — El menú de ajustes del reproductor deja de ser Rockbox nativo

**Encargo**: tras probar `v0.2.6-beta`, el dueño confirmó que el OSD (D-306) "cambió y se ve bien", pero señaló que el propio menú de ajustes del reproductor (tecla Menú → Ajustes) "aún se ve con la interfaz de Rockbox incluido el back, y los iconos" -- pidió aplicar ahí también los estilos del sistema.

**Causa**: D-306 solo llega al OSD (el overlay que Aura dibuja a mano sobre el video, código 100% de este plugin) -- nunca podía llegar al menú de ajustes porque ese menú NO es código de Aura: es `rb->do_menu()`/`rb->set_option()`/`rb->set_int_ex()`, el widget de lista nativo de Rockbox (`apps/gui/list.c`), con su propio resaltado de selección, su propio manejo del botón de salida ("back") y sus propios íconos -- nada de eso pasa por `osd.*` ni por ningún color que D-306 pueda tocar. Es exactamente la brecha que ya había identificado la investigación inicial de esta tarea (antes de D-304): "es la única puerta trasera real a la UI de Rockbox".

**Hecho**: se reemplazó por completo el uso de `rb->do_menu()`/`rb->set_option()`/`rb->set_int_ex()` en `mpeg_settings.c` por un menú propio, mínimo pero enteramente de Aura:
- `aura_menu_draw()`/`aura_menu_pick()` (nuevas): dibujan una lista de texto plana con `rb->lcd_*` directo (mismo patrón que ya usaba `button_loop()` para limpiar la pantalla antes/después de este menú) -- fondo y texto con los colores reales del usuario vía `aura_osd_colors()` (getter nuevo en `mpegplayer.c`, expone los mismos `osd.bgcolor/fgcolor/accent` que ya carga D-306), fila seleccionada resaltada con el acento del usuario (texto invertido sobre el acento, no un cursor/ícono de Rockbox). SCROLL arriba/abajo navega, SELECT confirma, MENU cancela -- sin ningún ícono de "atrás": salir es simplemente el mismo botón MENU que ya abre el menú, sin una fila ni un glifo dedicados a eso.
- `aura_menu_adjust_int()`/`aura_adjust_draw()` (nuevas): el único ajuste que no es una elección entre pocas opciones fijas (brillo de la luz de fondo) -- pantalla centrada con el valor actual en el acento, IZQUIERDA/DERECHA lo cambian de a uno, aplicado en vivo igual que antes.
- Cada llamada a `rb->do_menu()`/`mpeg_set_option()`/`mpeg_set_int()` en `mpeg_menu()`, `display_options()`, `audio_options()` y `mpeg_settings()` se reescribió sobre estas dos funciones. Los `struct opt_items` (`noyes`, `singleall`, `globaloff`, `scalemodes`) ya no hacían falta -- las opciones ahora son arrays `const char*` simples, locales a cada función.
- Limpieza de paso: se eliminó el bloque completo de `#define MPEG_START_TIME_*` por keypad (~400 líneas) -- código muerto desde D-062 (sus únicos llamadores, `get_start_time()`/`show_start_menu()`, ya no existen), confirmado sin ningún otro uso en todo el árbol antes de borrarlo.

**Verificado**: `make test` **11/11**. Build ARM real limpio, **sin ningún warning nuevo** (el archivo entero compiló limpio al primer intento tras el reemplazo). Build del simulador limpio. No se pudo verificar visualmente en el simulador -- a diferencia del frame de video (que usa `lcd_blit_yuv`, fuera del framebuffer que lee `screen_dump()`), este menú nuevo SÍ dibuja sobre el framebuffer normal y en principio debería ser capturable, pero la inyección de botones del simulador (`AURA_SIM_BUTTONS`) no logró llegar de forma confiable hasta la pantalla de reproducción/menú en varios intentos con distintas secuencias -- mismo problema de navegación que ya bloqueó la verificación visual de D-304/D-305/D-306. Verificación real queda a cargo del dueño en hardware.

**Pendiente**: confirmación del dueño en hardware real -- que el menú se vea con sus colores, que la navegación (scroll/select/menú) se sienta igual de responsiva que antes, y que el ajuste de brillo de fondo siga funcionando correctamente.

## D-308 — El toggle de "cubrir" se revertía solo durante la reproducción normal

**Encargo**: el dueño reportó que "aún no se ve el ajuste 'Cubrir pantalla', sólo no funciona" -- alternar con Select no producía ningún cambio visible observable.

**Investigación** (agente de solo lectura, dos hipótesis a distinguir): encontró **dos causas independientes**, cualquiera de las dos basta para explicar el síntoma:

1. **Aura Studio: el pipeline de sync es idempotente.** `processAll()` (`LibraryViewModel.swift`) solo llama a `process(itemAt:)` (lo único que invoca `FFmpegTranscoder.transcode(...)`) para items con `status == .queued` -- un video que ya se sincronizó con éxito en CUALQUIER pasada anterior queda `.ready` para siempre y nunca se vuelve a transcodificar, sin importar cuántas veces el dueño corra "sincronizar" después de actualizar Studio. `SyncPlanner.plan` tampoco ayuda: compara tamaño/fecha del archivo ORIGINAL del usuario, nunca el contenido de `preparedURL` ni una versión del transcodificador -- si el original no cambió, ni siquiera se vuelve a copiar el `.mpg` al iPod. Esto ya estaba anotado como "nota honesta" en D-305, pero quedaba sin confirmar formalmente hasta ahora: **un video ya sincronizado ANTES de actualizar a una versión de Studio con el `-vf` sin `pad` (ST-027) se queda con las franjas horneadas para siempre a menos que se quite de la biblioteca y se vuelva a agregar el archivo original.**

2. **Bug real en el firmware, independiente del punto 1**: `vo_setup()` (`video_out_rockbox.c`) se llama cada vez que libmpeg2 vuelve a parsear una cabecera de secuencia MPEG (`STATE_SEQUENCE`) -- y eso pasa más de una vez por archivo: los codificadores MPEG-2 con GOP corto (`mpeg2video` de ffmpeg con `-g 15`, ST-026, activo siempre que la fuente supera 24fps) repiten esa cabecera antes de cada GOP, cada ~0.6s, DURANTE LA REPRODUCCIÓN NORMAL, no solo al abrir el archivo. `vo_setup()` reasignaba `vo.scale_mode = settings.scale_mode` (el default persistido, FIT) en cada una de esas repeticiones, sin ningún guard -- revirtiendo el toggle de SELECT casi al instante, mucho más rápido de lo perceptible. Esto pasaría incluso con un video correctamente re-transcodificado sin franjas.

**Hecho**: nuevo campo `vo.scale_mode_locked` (`video_out_rockbox.c`) -- `vo_init()` lo limpia (corre una vez por sesión de reproducción, al arrancar el hilo de video); `vo_setup()` solo adopta `settings.scale_mode` la PRIMERA vez que corre tras eso, y lo deja intacto en cualquier repetición posterior de la cabecera de secuencia. Mismo criterio que `s_cover_mode` en el visor de fotos (D-303): el modo persiste mientras dure la sesión de reproducción (incluso pasando al siguiente video en modo "Todos", que reutiliza el mismo hilo), y solo vuelve al default persistido al iniciar una sesión nueva.

**Deliberadamente no resuelto en esta pasada** (es de Aura Studio, no del firmware, y afecta la biblioteca ya sincronizada del dueño -- decisión de producto, no solo técnica): no se agregó ningún mecanismo para forzar el re-transcode de un video ya `.ready` sin quitarlo y re-agregarlo. Si el dueño lo quiere, es un cambio de UI en Aura Studio (un botón "reprocesar" por ítem o masivo) que vale la pena diseñar aparte.

**Verificado**: `make test` **11/11**. Build ARM real limpio, sin warnings nuevos. Build del simulador limpio.

**Pendiente**: el dueño necesita RE-SINCRONIZAR (quitar y volver a agregar el archivo original) cualquier video que ya estuviera en su biblioteca antes de `v0.2.5-beta`/ST-027 para que el modo "cubrir" tenga algo real que recortar -- de otro modo seguirá viéndose idéntico a "ajustar" en esos archivos específicos, sin que sea ya ningún bug. Confirmación en hardware con un video efectivamente re-sincronizado.

## D-309 — El menú propio (D-307) tenía un bug real de fondo opaco + no calzaba con el sistema de diseño

**Encargo**: el dueño probó `v0.2.7-beta` en el simulador (sin llegar a hardware) y reportó, con captura de pantalla: el "seleccionador" no es igual al que ya existe en el resto de la app, los textos y márgenes tampoco respetan lo ya establecido, y al seleccionar un elemento "se construye con un cuadro que lo tapa" -- confirmó que el modo/acento sí se heredan bien (D-306), pero el menú en sí necesita más trabajo.

**Bug real encontrado, no solo un desajuste de estilo**: la captura mostraba la fila seleccionada como un bloque verde con un RECUADRO BLANCO VACÍO adentro en vez de su texto. Causa: `aura_menu_draw()` (D-307) usaba `DRMODE_SOLID` para dibujar el texto -- ese modo hace que `rb->lcd_putsxy()` pinte un parche opaco del color de fondo VIGENTE detrás de cada carácter, no solo el trazo del glifo. Al resaltar la fila seleccionada, el código cambiaba `lcd_set_foreground()` a `bg` (para "invertir" el texto sobre el relleno de acento) pero nunca tocaba `lcd_set_background()`, que seguía siendo `bg` de la pasada anterior -- el resultado era texto blanco sobre un parche de fondo TAMBIÉN blanco: invisible, dejando ver solo el rectángulo opaco vacío. Confirmado leyendo `firmware/drivers/lcd-bitmap-common.c` (`putsxyofs()` respeta el drawmode del viewport para decidir si pinta el fondo o no).

**Causa del desajuste de diseño**: mi primer intento (D-307) inventó una convención de selección propia (bloque sólido de acento a todo el ancho, texto invertido) en vez de replicar la que la propia app ya usa. Se leyó `aura_widgets_draw_list()` (`apps/aura/aura_widgets.c`) como fuente de verdad: la fila seleccionada es una **píldora redondeada** (`a26_shell_fill_rounded_rect`, radio = `corner_radius_pill` = 8), insertada `ROW_PAD_X` (`layout.list_inset` = 16px) desde los bordes izquierdo/derecho y `PILL_MARGIN_Y` (2px) desde arriba/abajo de la fila, rellena con **`selection_fill`** (un tinte sutil casi igual al fondo -- `#2C2C2E` en oscuro, `#E5E5EA` en claro -- NO el acento) -- y el texto de esa fila cambia a **color de acento**, nunca se invierte sobre un bloque sólido. `ROW_HEIGHT` = `type_scale.body` (13) + 2×`spacing.md` (8) = 29px.

**Hecho**:
- `aura_osd_colors()` (`mpegplayer.c`) gana un 4to color de salida, `selection_fill` -- nuevo campo en `struct aura_osd_palette`/`struct osd`, con default compilado y lectura desde `theme.cfg` (clave `palette_{light,dark}_selection_fill`, ya reservada en el contrato de formato de tema -- `AURA_STYLE_ROLE_SELECTION_FILL`, confirmado en `aura_style_manifest.c`, no hubo que inventar nada nuevo del lado del contrato).
- `aura_menu_draw()` reescrita: geometría real (`AURA_ROW_HEIGHT`=29, `AURA_ROW_PAD_X`=16, `AURA_PILL_MARGIN_Y`=2), nueva `aura_fill_rounded_pill()` (aproximación de esquina redondeada por inset de fila, radio 8, mismo criterio que la píldora de la barra de progreso de D-305 pero con más filas por el radio mayor), fila seleccionada = píldora en `selection_fill` + texto en `accent`, filas normales = texto en `fg`. **Todo el texto ahora se dibuja con `DRMODE_FG`** (glifos transparentes, sin parche de fondo) -- elimina la clase entera de bug de fondo opaco, sin importar sobre qué superficie se dibuje el texto. `aura_adjust_draw()` (brillo de luz de fondo) recibió el mismo tratamiento por consistencia.

**Verificado**: `make test` **11/11**. Build ARM real limpio, sin warnings nuevos. Build del simulador limpio. El dueño está probando directamente en una instancia interactiva del simulador (no hardware todavía) -- pendiente su confirmación visual de que el recuadro blanco desapareció y que la píldora/colores ahora sí se parecen a las listas reales de Aura.

**Pendiente**: confirmación visual del dueño (simulador interactivo primero, hardware después). Si el veredicto es que la tipografía tampoco calza (Aura usa `DS_BOLD_14` para filas de lista, `mpeg_settings.c` sigue usando `FONT_UI`, la fuente de interfaz general del usuario) -- cargar esa fuente específica en el plugin es un problema aparte, no evaluado todavía: implica `font_load()` sobre un slot de `MAXUSERFONTS` ya ajustado, sin la infraestructura de fallback de `aura_style.c` disponible en el plugin.

## D-310 — Arquitectura de niveles de Animaciones/Gráficos: regla de precedencia + punto único de decisión

**Encargo**: matriz normativa del dueño del producto (2026-08-18) definiendo, celda por celda, qué debe hacer `CoverDrift`, `CoverFlow`, el reproductor y las transiciones generales en cada combinación de los ajustes Animaciones (Ninguna/Mínimas/Todas) y Gráficos (Ninguno/Mínimos/Todos) — hasta ahora, `animation_mode` tenía 3 niveles reales consultados a mano en ~24 sitios de `aura_transitions.c`/`aura_nowplaying.c` (sin punto único), mientras que `graphics_mode` era, de hecho, binario: ningún sitio distinguía Mínimos de Todos. Investigación completa y plan de ejecución en `PLAN-niveles-fx.md` (raíz del repo, histórico una vez ejecutado).

**Regla de precedencia (D-a)**: Gráficos decide QUÉ existe; Animaciones decide CÓMO se mueve lo que existe. Cuando Gráficos elimina un elemento, Animaciones ya no tiene nada que decidir sobre él. Corolario: la rotación de contenido de `CoverDrift` (cambiar de imagen cada 7s) es contenido, no animación — ocurre en todos los niveles de Animaciones; lo que ese ajuste gobierna es el *cómo* del cambio (drift+fade vs. quieto+corte).

**Dónde viven los delays (D-b)**: el cambio de delay del panel derecho (2000/1000ms → 500ms) vive bajo **Gráficos**, no Animaciones, aunque un lector razonable lo buscaría ahí — el delay no gobierna movimiento sino cuánto trabajo de dibujo se dispara (decodificar una carátula, repintar el panel), el dominio de Gráficos. Registrado explícitamente en `docs/aura-design-system/sistema/06-niveles-de-fx.md` (página nueva, índice transversal de las 6 tablas) para que quien lo busque en el lugar "razonable" encuentre la referencia cruzada.

**Hecho**: header nuevo `firmware/rockbox/apps/aura/aura_fx.h` (solo funciones `static inline`, sin `.c`/entrada en `SOURCES`) — punto único para las decisiones que CRUZAN ambos ajustes (`aura_fx_coverdrift_pool_cap()`, `aura_fx_panel_debounce_ms()`, `aura_fx_coverdrift_motion()`, `aura_fx_coverdrift_crossfade()`, `aura_fx_ss_show_tile()`). Los ~24 gates binarios preexistentes de `animation_mode` (Ninguna apaga una transición) **no se tocaron** — son triviales, no cruzan ejes, forzarlos por este header habría sido un refactor sin cambio de conducta sobre un diff ya grande.

**Verificado**: build ARM + simulador limpios, `make -C firmware/rockbox/apps/aura/test test` **11/11 suites, todas OK**, sin regresiones. No se pudo verificar visualmente (sin acceso a hardware ni a una sesión interactiva del simulador en este entorno) — verificación real de las 9 combinaciones de niveles, en hardware, queda a cargo del dueño (misma limitación ya anotada en D-307/D-309).

## D-311 — CoverDrift en los 3 niveles de Gráficos: degradado de acento, 5 imágenes residentes, loop completo

**Decisión (D-c)**: con Gráficos=Ninguno, `CoverDrift` no decodifica ni dibuja ninguna imagen — el panel muestra el degradado VERTICAL de acento que `SelectionSummary` ya usaba como fallback de su fondo (`aura_selection_summary_draw_accent_gradient_background()`, exportada). Se descartó el degradado DIAGONAL del tile (el otro candidato obvio) porque su propio comentario ya documentaba bandas visibles en RGB565 a 160×240 — el vertical está validado a ese tamaño exacto desde D-292. La mecánica de identidad/debounce/fundido de D-262 no cambia: solo cambia qué se dibuja.

**Decisión (D-d)**: con Gráficos=Mínimos, `CoverDrift` mantiene hasta `AURA_DS_METRICS_COVER_DRIFT_POOL_CAP_MINIMAL` (5) imágenes RESIDENTES del pool general, sorteadas al azar una vez por arranque (o al cambiar de nivel, o si el pool de la biblioteca se regenera) — nunca "el loop completo cargado en RAM": el loop de hoy YA solo mantiene 2 imágenes residentes sin importar el tamaño del pool (cargar 300 carátulas de ~200KB sería ~61MB, inviable). El modo de 5 imágenes **no ahorra RAM** respecto al comportamiento actual — gasta ~600KB MÁS (5×200KB vs. ~400KB de hoy); lo que compra es que el HDD del dispositivo deja de despertar cada 7s una vez decodificadas las 5 (batería/silencio, no memoria). El umbral de montaje sigue siendo 3 imágenes (D-254), independiente de este nivel.

**Hecho**: `ensure_drift_effective_pool()` (`aura_screens.c`) construye el subconjunto de 5 con Fisher-Yates parcial sobre un arreglo `static` (no en la pila, mismo criterio de D-226 dado el stack de 8KB del hilo "main"); `effective_drift_seek()` resuelve cada índice contra el pool efectivo o el completo según el nivel. `decode_album_drift_tile()` y `draw_panel_identity()` consumen ese punto único. El eje de Animaciones (D-a) se implementó en `aura_coverdrift.c`: con Animaciones≠Todas la imagen activa queda quieta en el centro (distancia de deriva forzada a 0); con Animaciones=Ninguna el cambio de imagen es corte en vez de cross-fade.

**Deliberadamente no resuelto en esta pasada** (hallazgo lateral, `PLAN-niveles-fx.md` §14, fuera del alcance de la matriz): la doble decodificación por ciclo de `ensure_drift_albums_decoded()` (decodifica 2 imágenes por avance cuando bastaría 1 intercambiando roles de buffer) y que `aura_coverdrift_animating()` nunca vuelve a `false` tras el primer montaje (pide cuadros a 20fps permanentemente aunque `CoverDrift` ya no esté en pantalla) quedan sin arreglar — son ineficiencias preexistentes, no bugs de corrección, y el riesgo de introducir un bug real optimizándolas sin poder compilar-y-medir en hardware superó el beneficio en esta pasada.

## D-312 — Gráficos=Ninguno ya no colapsa el panel derecho a pantalla completa (corrige la regla dura de D-149/status-bar.md)

**Conflicto encontrado (Q1 de `PLAN-niveles-fx.md`)**: la regla dura vigente (`aura_widgets_split_active()`, documentada en `componentes/status-bar.md`) decía que con Gráficos=Ninguno no existe `LeftPanel`, así que tampoco hay `(split)` — todo colapsa a `(full)`. La matriz normativa del dueño define contenido PROPIO para el panel derecho en ese nivel (degradado de acento en `CoverDrift`, D-311; solo texto en `SelectionSummary`, D-313), lo que exige que el panel siga EXISTIENDO.

**Decisión**: manda la matriz — es la especificación normativa vigente del dueño, y sus celdas para Gráficos=Ninguno carecen de sentido si el panel no existe. `aura_widgets_split_active()` ya no consulta `graphics_mode`, solo la tabla de layout por pantalla (`s_list_layout`); el ajuste de Gráficos pasa a decidir exclusivamente qué se dibuja DENTRO del panel una vez que el layout ya lo puso ahí (consistente con D-a). El ancho del push T1/T3 (`aura_screens.c`, despachador de navegación) recibió el mismo ajuste — antes también colapsaba a ancho completo con Gráficos=Ninguno.

**Hecho**: `status-bar.md` corregido en la misma pasada (fuente viva, no un espejo que se sincroniza después) con la regla nueva y una nota explícita de la corrección, para que quien la haya memorizada la encuentre actualizada.

## D-313 — SelectionSummary con Gráficos=Ninguno: solo texto, recentrado como grupo

**Decisión (Q5)**: con Gráficos=Ninguno, `SelectionSummary` no dibuja el tile (ícono+degradado diagonal+sombra SDF, la parte cara del componente) — solo los dos slots de texto. Sin el ancla del tile, los slots dejan de centrarse cada uno en su propia mitad del panel (la fórmula de D-272) y se recentran como GRUPO único en los 240px completos: altura total = línea superior + separador (si ambos slots están presentes) + contenido inferior, centrada verticalmente.

**Hecho**: `aura_fx_ss_show_tile()` (`aura_fx.h`) decide; `draw_summary()` (`aura_selection_summary.c`) calcula las posiciones Y por una de dos fórmulas (ancladas al tile, o centradas como grupo) según ese booleano. El contrato "`icon_name` nunca es NULL" no se tocó — la omisión es del DIBUJO del tile, parametrizada, mismo precedente que `aura_ss_background_t` (D-281).

## D-314 — Sustitución de morphs en Animaciones=Mínimas: Flip-and-Flow/Flow-Return → push genérico; Modo 4 → Fade-Slide de pantalla completa

**Decisión (D-e)**: con Animaciones=Mínimas, dos pares de morphs se sustituyen por piezas YA EXISTENTES del vocabulario de transiciones — ningún patrón nuevo:
- **CoverFlow ↔ Reproductor** (`Flip-and-Flow`/`Flow-Return`, ~500ms cada uno): sustituidos por el `Push-and-Drop` genérico full↔full (`aura_transition_slide()`) en ambos sentidos — CoverFlow y el reproductor son ambas pantallas FULL (`screen_uses_split_layout()` no las incluye), así que el push ya disponible resuelve el caso sin ninguna coreografía nueva.
- **Modo 4 del reproductor** (Letras, entrada/salida): sustituido por `Fade-Slide` de pantalla completa (`aura_transition_fade_slide_region()`, la misma pieza que ya usa `DynamicTitle`/"Acerca de") — lo viejo se desvanece, lo nuevo entra desde fuera de la pantalla.

**Hecho**: `aura_coverflow.c` (`BUTTON_SELECT` de `CF_STATE_SHOW_TRACKS`) hace `aura_nav_push()` directo en vez de llamar a `aura_transition_flip_and_flow()` cuando Animaciones≠Todas; el despachador central de `aura_screens.c` (manejo de profundidad de pila) aplica el push genérico en los tres puntos de cruce CoverFlow↔Reproductor (entrada, regreso normal, y regreso encadenado desde el Modo 4 de Letras). `mode4_transition()` (nueva, `aura_nowplaying.c`) es el punto único que elige morph/fade-slide/nada según Animaciones — reemplaza las llamadas directas a `mode4_morph()` en `cycle_mode()` (que ahora recibe `nav`) y en `aura_nowplaying_unfold_from_lyrics()` (que también recibe `nav` ahora).

**Verificado**: build ARM + simulador limpios, `make test` **11/11**, sin regresiones. Verificación visual (que el push se sienta como una navegación normal y no dos gestos desacoplados) queda a cargo del dueño en hardware/simulador interactivo.

## D-315 — Perfil estático del morph del Modo 4 (letras): sospechosos identificados, optimización pendiente de ejecutar y medir en hardware

**Encargo**: el dueño reportó que el morph de entrada/salida del Modo 4 "es muy lento y nada fluido en el hardware" — degradar la calidad del efecto para ocultarlo violaría el principio de máxima fidelidad, así que se trató como problema de rendimiento aparte, no como una celda más de la matriz de niveles.

**Perfil (lectura del código, `PLAN-niveles-fx.md` §6 — el simulador no sustituye la medición en hardware real, pendiente)**: el morph (`mode4_morph()`, `aura_nowplaying.c`) es el único efecto del árbol que reconstruye la pantalla completa desde cero en cada uno de sus ~19 cuadros (`a26_shell_clear_screen()` + `draw_player()` completo + `lcd_update()` de 320×240 con espera de DMA). Sospechoso dominante: **ausencia de caché de íconos en RAM** — cada ícono (~13-15 por cuadro: batería, play/pausa, 5 estrellas, 5 modos, 3 de transporte) se lee y decodifica de DISCO en cada cuadro (`aura_style_read_icon_bmp()`/`read_bmp_file()`), ~250-285 aperturas de archivo en 330ms sobre el HDD del iPod. Sospechosos secundarios: divisiones enteras por píxel en el tinte de la hoja de vidrio del panel derecho (~45.600/cuadro, ARM sin divisor por hardware), reproyección de la carátula con ~160 `lcd_bitmap()` de 1px de ancho por cuadro (el caché de perspectiva existente nunca acierta durante el morph, por diseño), y remaquetado de texto de letras invariante recalculado cada cuadro.

**Plan de optimización (conserva la definición del efecto, no la degrada)**: (1) caché de íconos en RAM — mismo patrón que D-224 ya usó para "coverflow muy lento"; beneficia a todo el sistema, no solo al morph. (2) Precalcular invariantes antes del bucle (layout de letras, `aura_shadows_enabled()` fuera de bucles de píxel). (3) LUT del degradado del vidrio (depende solo de `x_rel+y` ∈ [0,430]). (4) `StatusBar` compuesta una vez y solo desplazada. (5) Prerender de estados finales al estilo `s_push_fb` de `Flip-and-Flow`. (6) LUT/interpolación de la sombra SDF del álbum. Protocolo de medición: la instrumentación D-300 (`DEBUGF` por fase) y `TRANSITION_LOG` ya existen para medir ms/cuadro antes y después de cada paso.

**Deliberadamente no ejecutado en esta pasada**: ninguna de las 6 optimizaciones se implementó — requieren perfilado real en hardware (D-300) entre cada paso para confirmar impacto, algo que este entorno no puede hacer (sin iPod físico). Implementarlas a ciegas, sin la medición que el propio protocolo exige, arriesgaría exactamente el tipo de "clever pero sutilmente roto" que este repositorio ya ha tenido que revertir antes (D-253). Trabajo pendiente, con el diagnóstico y el orden de impacto esperado ya documentados en `now-playing.md` § Niveles de reducción del Modo 4 y en `PLAN-niveles-fx.md` §6.

## D-316 — CoverDrift se conecta a Video y Fotos; Películas/Series/Videoclips y Fotos/Imágenes/IA dejan de ser decorativas; índice de categoría por archivo (Studio → firmware, nuevo)

**Encargo**: tras revisar el trabajo de niveles de FX (D-310 a D-315) en el simulador, el dueño pidió extender `CoverDrift` (D-254, hasta ahora exclusivo de Música) a Video y Fotos — "cada uno con sus propias imágenes"; para Video, con la restricción textual (nueva, no existía en ningún documento previo — búsqueda exhaustiva confirmó que no está registrada ni en este repo ni en Aura Studio ni en el contrato) de que solo se vean portadas de **películas y series, nunca videoclips**; y pidió habilitar las filas de categoría de ambas secciones — "Películas", "Series"/"Videoclips" (ya existían como filas **inertes** desde D-157/D-264) y tres filas **nuevas** en Fotos: "Fotos", "Imágenes" e "**IA***" (con asterisco literal marcando contenido generado por IA).

**Bloqueo real encontrado antes de escribir código**: `/Videos` y `/Photos` en el dispositivo son y siguen siendo **planos** a propósito (D-192, `DECISIONS-ARCHIVE.md`: "la categoría es SOLO organización dentro de Aura Studio, nunca cambia dónde se sincroniza en el iPod"). Aura Studio ya calcula la categoría de cada foto/video (`MediaCategoryClassifier`, campo `category` de `biblioteca.json`, confirmado por inspección directa de una biblioteca real del dueño) pero **nunca la exportaba al dispositivo** — solo 3 contadores agregados por sección (`sync_summary.cfg`, D-283), insuficientes para filtrar filas individuales. Se le presentó la disyuntiva al dueño (índice nuevo vs. demo solo-simulador vs. heurística local) — eligió el **índice por categoría**.

**Decisión — índice opcional `nombre_archivo: categoría`** (`CONTRATO-firmware-studio.md` §D.2, v5 del contrato; `docs/contracts/library-layout-v1.md` v1.2): dos archivos nuevos, `.rockbox/aura/video_categories.cfg` y `.rockbox/aura/photo_categories.cfg`, mismo formato/parser que `sync_summary.cfg` (`settings_parseline()` + `read_line()`, sin dependencia de un parser JSON real) — **ambos OPCIONALES**: su ausencia es un caso soportado, no un error (toda fila de categoría se ve vacía y cae a `SelectionSummary`, degradación honesta). `/Videos`/`/Photos` en sí **no cambian** — siguen planos, el índice es un archivo aparte que solo asocia nombres ya existentes con una categoría, respetando D-192 al pie de la letra. **Pendiente**: la escritura real de estos dos archivos desde `biblioteca.json` es trabajo del repositorio de Aura Studio, no incluido en esta pasada (sesión separada, `Aura-Studio/`).

**Hecho — firmware**:
- `aura_media_categories.c`/`.h` (nuevo módulo): carga bajo demanda + cachea los dos índices; `aura_media_categories_video_lookup()`/`_photo_lookup()` por nombre de archivo, `_invalidate()` en los mismos dos momentos que `aura_video_invalidate()`/`aura_photos_invalidate()` (D-291: entrar a la sección, volver de USB).
- `aura_video.c`: `s_videos_cat[]` resuelto una vez por archivo al escanear; `ensure_video_filter()`/`aura_video_draw()`/`_handle_button()` ahora reciben `aura_screen_id_t screen` y filtran por categoría — "Todos los videos" **sin cambio de comportamiento** (filtro identidad). Películas/Series/Videoclips dejan de estar en la lista `dimmed`/inerte de `aura_screens.c` (D-157/D-264 quedan superadas para estas tres filas).
- `aura_photos.c`: mismo patrón — `s_photos[].category`, filtrado en la lista y en el **visor** (`s_viewer_filter`/`s_viewer_pos`: LEFT/RIGHT navegan el subconjunto filtrado con el que se entró, no la lista completa; MENU restaura la posición correcta en la lista de origen). Tres filas nuevas: `AURA_SCREEN_PHOTOS_PHOTO/IMAGE/AI` (`aura_nav.h`, al final del enum). Sin ícono propio para las tres (no existe un asset "cámara"/"destellos" en el set de íconos hoy) — reusan `"image"`, mismo criterio que "Todas las fotos"; decisión pragmática, no un rediseño de iconografía.
- `aura_screens.c`: `drift_pool_t` (más fino que `aura_category_t` — Películas y Series son ambas "Video" pero jamás deben compartir pool, a diferencia de Música, donde toda fila comparte un único pool general de álbumes). `row_wants_coverdrift()` generaliza `music_row_wants_coverdrift()` (sin tocarla) sumando `video_row_wants_coverdrift()`/`photo_row_wants_coverdrift()` — **`AURA_SCREEN_VIDEOS_CLIPS` deliberadamente ausente** de la calificación de CoverDrift (la restricción del dueño). `decode_album_drift_tile()` generalizado: Música sigue por `aura_albumart_load_for_album()` (tagcache); Video/Fotos leen el archivo directo (`read_jpeg_file()`/`read_bmp_file()`, mismos decoders que ya usa `aura_photos.c`) — cartel `<video sin extensión>.jpg` hermano para Video (el contrato ya lo contemplaba desde D-291/v1, el firmware nunca lo leía hasta ahora), el archivo mismo para Fotos. Un solo CoverDrift visible a la vez → los tres orígenes **comparten los mismos buffers** de decodificación (~400KB), invalidados explícitamente al cambiar de fuente (bug real que se evitó en el diseño, no en producción: sin esa invalidación, un índice numéricamente igual entre dos fuentes distintas habría mostrado la imagen de la fuente vieja un cuadro de más).
- Corrección de un desbordamiento real encontrado en el propio diseño (no llegó a producción): `s_drift_album_images[]` está dimensionado a `AURA_MUSIC_MAX_ITEMS` (300), pero `MAX_PHOTOS` es 500 — `drift_pool_count()` acota el conteo de cualquier fuente a 300 antes de usarlo, evitando una escritura fuera de rango si el usuario tuviera más de 300 fotos de una misma categoría.

**Verificado**: build ARM + simulador limpios (recompilación completa, cero *warnings* nuevos bajo `-Wall -Wextra`, un `strlcpy()` sin `#include "string-extra.h"` corregido en el camino), `make -C firmware/rockbox/apps/aura/test test` **11/11 suites, sin regresiones**. Simulador poblado con contenido real de prueba: 3 carátulas texturizadas + 1 fractal para el pool de Música (generadas con `ffmpeg`, reemplazando los colores planos iniciales que no dejaban ver el movimiento), 2 películas reales del dueño (`Avatar Aang...`, `Little Amelie...`) con sus **carátulas reales** extraídas de `.portadas/` de su biblioteca de Aura Studio, más fixtures sintéticas de serie/videoclip para probar la exclusión. Verificación visual completa (las 4 categorías de Fotos, el filtro de Video en las 9 combinaciones de niveles, el visor navegando dentro de un subconjunto filtrado) queda a cargo del dueño en esta misma sesión de simulador.

**Deliberadamente no resuelto en esta pasada**: la escritura de los dos `.cfg` desde Aura Studio (trabajo del otro repositorio); un ícono propio para "Fotos"/"Imágenes"/"IA*" (reusan `"image"`); cualquier ajuste de Gráficos/Animaciones específico para los pools de Video/Fotos (heredan el comportamiento ya definido en D-310–D-315 sin ajuste adicional, ya que el dueño no pidió una matriz de niveles distinta para ellos).

## D-317 — "Cover Flow" se renombra a "Music Flow" en todo el repositorio (riesgo de marca de Apple)

**Encargo**: el dueño del producto pidió renombrar "Cover Flow" (texto visible, identificadores de código, nombres de archivo, documentación) para no infringir la marca/copyright de Apple — riesgo real en un repositorio ya público (D-286). Confirmó explícitamente que el renombre debe llegar hasta el código y los nombres de archivo, no solo el texto visible.

**Hecho**: 326 reemplazos automatizados (script Python con reemplazos ordenados por especificidad, para no corromper coincidencias parciales) en 42 archivos, más una segunda pasada dirigida por dos formas de capitalización que el primer paso no cubría (`CoverFlow`→`Coverflow` sin espacio con solo la primera letra en mayúscula, y su normalización final a "Music Flow" en prosa). Alcance:
- Archivos: `aura_coverflow.c/.h` → `aura_musicflow.c/.h` (`git mv`); `docs/aura-design-system/componentes/cover-flow.md` → `music-flow.md` (`git mv`).
- Identificadores: `AURA_SCREEN_MUSIC_COVERFLOW`→`AURA_SCREEN_MUSIC_FLOW`, `AURA_STR_MUSIC_COVERFLOW`→`AURA_STR_MUSIC_FLOW`, `aura_coverflow_*`→`aura_musicflow_*`, `is_coverflow_screen()`→`is_musicflow_screen()`, `AURA_COVERFLOW_BACK_*`→`AURA_MUSICFLOW_BACK_*`, `AURA_DS_METRICS_COVER_FLOW_*`→`AURA_DS_METRICS_MUSIC_FLOW_*` (`tokens.json` clave `cover_flow`→`music_flow`, header regenerado). Los macros LOCALES del archivo (`CF_*`, `cf_state_t`/`cf_slot_t`/`cf_entry_t`) se renombraron a `MF_*`/`mf_*` — escaneado y confirmado que **no** tocó `CF_CACHE_DIR` (`aura_albumart.c`/`aura_sync.c`), que es el nombre real ya instalado del directorio de caché en disco (`/.rockbox/aura/cfcache`) — cambiarlo habría exigido una migración, y la coincidencia de dos letras con "Cover Flow" es casual, no la marca de Apple.
- Texto de UI: "Cover Flow" → "Music Flow" en ambos idiomas (mismo criterio que el original, que tampoco traducía el nombre al español).
- Términos genéricos de carátula de álbum (`cover_data`, `MF_COVER_SIZE`, estados `cover_in`/`cover_out` del flip) se dejaron intactos a propósito -- "cover" ahí significa la carátula física del álbum, no la marca de Apple.

**Deliberadamente NO tocado**: `docs/design/` (base histórica original, congelada por diseño), `docs/plans/archivo/` (planes ya ejecutados, registro histórico), `DECISIONS.md` (entradas pasadas) y `DECISIONS-ARCHIVE.md` (histórico, de solo lectura) — todos documentan con precisión lo que existía EN SU MOMENTO; reescribirlos retroactivamente falsificaría el registro histórico del propio repositorio.

**Verificado**: build ARM + simulador limpios tras cada pasada, `make -C firmware/rockbox/apps/aura/test test` 11/11 sin regresiones. Grep final de "Cover Flow"/"CoverFlow"/"Coverflow"/"coverflow" en todo el árbol tocado: cero coincidencias residuales.

## D-318 — Movie Flow: copia casi exacta de Music Flow para Video, cartel 3:4, temporadas por `SxxEyy`, fundido a negro al reproductor

**Encargo**: inmediatamente después de D-317 (el renombre a Music Flow), el dueño del producto pidió una "copia casi exacta de Music Flow" para la sección de Video, con cuatro diferencias explícitas: (1) formato de cartel **rectangular 3:4**, no cuadrado; (2) las **películas se reproducen al instante** con SELECT, sin el giro a lista; (3) las **series agrupan un cartel por temporada**, y SELECT sobre una temporada sí gira, mostrando sus episodios; (4) la entrada al reproductor **nunca** es Flip-and-Flow — un fundido a negro simple y de ahí al reproductor. Pidió además archivos de prueba para series y películas, y que se le consultara ante cualquier duda.

**Preguntas resueltas antes de escribir código** (`AskUserQuestion`, las tres respondidas con la opción recomendada):
- **Agrupamiento de temporada**: convención de nombre `SxxEyy` (el mismo patrón que Aura Studio ya usa internamente para ordenar series, ST-033) — sin ese patrón, un archivo `series` se vuelve su propia "temporada" de un episodio (fallback honesto, nunca se descarta en silencio).
- **Reverso de temporada**: lista de texto, reusando **tal cual** el panel de reverso de Music Flow (mismo tamaño 200×200, misma mecánica) — no un componente nuevo de miniaturas por episodio.
- **Alcance del renombre** (pregunta compartida con D-317): ya resuelta ahí.

**Hecho**:
- `aura_movieflow.c`/`.h` (nuevo módulo, ~1000 líneas): mismo motor de proyección por columnas que Music Flow (`aura_flow.c`, **sin tocarlo** — ya es agnóstico de ancho/alto, solo generalizar el llamador a 120×160 en vez de 130×130), mismo patrón de cache de slots (decodificación + transposición a columna-contigua + reflejo), mismo zoom-on-scroll (D-245/246/247/249), mismo Flip-and-Flow para temporadas. Reflejo, esquinas redondeadas y decodificación **propios** (no `aura_art_generate_reflection()`/`aura_art_mask_corners_transposed()`/`aura_art_transpose()`, que asumen cuadrado con un solo parámetro `size`) — `mvf_mask_corners()` porta el mismo algoritmo exacto de `aura_art_mask_corners_transposed()` (distancia radial + rampa antialiasada de 1px vía `a26_shell_isqrt256()`) generalizado a `MVF_COVER_W`×`MVF_COVER_H`, corriendo en el mismo orden que `aura_albumart.c` (esquinas redondeadas ANTES del reflejo, para que el reflejo espeje un cartel ya con esquinas). Únicas funciones nuevas de bajo nivel, el resto reusa tokens/geometría ya derivados de Music Flow (`AURA_DS_METRICS_MUSIC_FLOW_*`, incluido `MF_CORNER_RADIUS`=8px, sin cambio).
- Modelo de datos `mvf_entry_t` (PELÍCULA o TEMPORADA, nunca episodio suelto): el pool combinado sale de `aura_video_count_filtered()`/`_filtered_filename()` (D-316) para `AURA_VIDEO_CAT_MOVIE`+`AURA_VIDEO_CAT_SERIES` — **nunca** `AURA_VIDEO_CAT_CLIP` (mismo criterio que `CoverDrift` de Video). `parse_sxxeyy()` agrupa episodios por temporada; convención de póster de temporada `<Programa> S0N.jpg` nueva, documentada en `docs/contracts/library-layout-v1.md` v1.3.
- SELECT sobre una PELÍCULA llama a `enter_player()` directo desde `MVF_STATE_IDLE`, sin pasar por ningún estado de giro. SELECT sobre una TEMPORADA gira (`MVF_STATE_COVER_IN`→`SHOW_EPISODES`) y reusa la geometría/mecánica exacta del reverso de Music Flow (`draw_episodelist_panel()`, adaptado de `draw_tracklist_panel()` solo en la fuente de datos).
- `enter_player()`: fundido a negro propio (framebuffer actual atenuado hacia negro sobre varios cuadros, gateado por `lcd_active()`/Animaciones=Ninguna igual que el resto de `aura_transitions.c` — nunca un módulo nuevo de transiciones, esta lógica vive dentro de `aura_movieflow.c` porque no es una transición ENTRE pantallas de Aura, es una salida hacia un plugin externo) y después `plugin_load(VIEWERS_DIR "/mpegplayer.rock", path)` **directo** — mismo mecanismo que ya usa `aura_video.c`, nunca `aura_nav_push(AURA_SCREEN_NOWPLAYING)` (esa pantalla es el reproductor interno de Música). Mensaje de error copiado de `show_cant_open_video()` (no exportado desde `aura_video.c`, copia local mínima) para el caso `plugin_load() < 0`.
- `BUTTON_PLAY` se deja sin semántica especial y Movie Flow **no** se agrega a la exclusión global de PLAY (a diferencia de `is_musicflow_screen()`) — Movie Flow no reproduce nada mientras se navega, la música de fondo sigue pausable/reanudable desde ahí.
- Wiring completo: `SOURCES`, `AURA_SCREEN_VIDEOS_MOVIEFLOW` (`aura_nav.h`), `AURA_STR_VIDEOS_MOVIEFLOW`="Movie Flow" sin traducir en ES/EN (mismo criterio que "Music Flow"), mapeo a `AURA_CATEGORY_VIDEO` (`aura_category.c`), fila nueva al tope de `videos_entries[]` (mismo orden que "Music Flow" en Música), `is_movieflow_screen()`/despacho de dibujo y botón, invalidación de caché al entrar (mismo gancho que ya invalidaba `aura_video_invalidate()` al entrar a "Todos los videos"), puerta de energía en `aura_main.c` (idéntica a la de `aura_musicflow_animating()`/`_pending()`).
- **Simplificación deliberada, documentada en `componentes/movie-flow.md`**: Movie Flow entra con el push genérico T1/T3 de ancho completo, **sin** replicar la coreografía de entrada con revelado de `CoverDrift` que sí tiene Music Flow (`aura_transition_musicflow_enter()`, D-259) — el encargo no mencionó la transición de *entrada*, solo la de *salida* hacia el reproductor; replicarla habría exigido generalizar `video_row_wants_coverdrift()` sin que se pidiera. Queda como decisión abierta para una pasada futura si el dueño la quiere.
- Documentación: `componentes/movie-flow.md` (nuevo, remite a `music-flow.md` para todo lo compartido, documenta solo las 4 diferencias + la simplificación anterior), `sistema/03-arbol-de-menus.md` y `00-INDICE.md` actualizados, `docs/contracts/library-layout-v1.md` v1.3 (convención de póster de temporada + nota de agrupamiento `SxxEyy`, ambas de solo-lectura del lado de Studio).
- Fixtures de prueba en el simulador (gitignored, no comiteadas): serie nueva de 2 temporadas/5 episodios ("Mi Serie de Prueba", `S01E01-03`+`S02E01-02`, con pósters de temporada generados con `ffmpeg` — patrones `testsrc2`/`mandelbrot`, distintos por temporada), un póster de temporada añadido a la serie de un solo episodio ya existente de D-316 (`Serie de Prueba S01.jpg`, antes sin poster de temporada), y una serie SIN patrón `SxxEyy` a propósito (`Documental Suelto`, sin póster tampoco) para ejercitar el fallback de "temporada de un episodio" y el color de relleno cuando falta el cartel. Los dos pósters de película reales de D-316 (`Avatar Aang...`, `Little Amelie...`) sirven tal cual para probar la reproducción instantánea.

**Verificado**: build ARM + simulador limpios (`make -j4` en ambos, cero *warnings* nuevos), `make -C firmware/rockbox/apps/aura/test test` **11/11 suites, sin regresiones**. Simulador lanzado sin errores en consola; verificación visual interactiva (giro de temporada, reproducción instantánea de película, fundido a negro, formato 3:4) queda a cargo del dueño en esta misma sesión de simulador.

**Deliberadamente no resuelto en esta pasada**: la coreografía de entrada con `CoverDrift` (ver simplificación arriba); Movie Flow no se agregó a `ROOT_SHORTCUTS` (Music Flow sí puede vivir como atajo en el menú de inicio, Movie Flow no se pidió ahí); ningún ajuste de Gráficos/Animaciones específico para Movie Flow más allá de heredar el mismo `MVF_FLIP_MS`/fundido gateados por los niveles ya definidos en D-310–D-315.

## D-319 — Contrato v6: fotos de artista (§D.3) — solo formato, sin código todavía

**Encargo**: el dueño pidió mostrar fotos de artista (círculo, estilo Apple Music) en Música → Artistas del firmware, con la metadata sincronizada desde Aura Studio (que ya las descarga, ST-032). Se aprobó un plan de dos fases (`PLAN-biblioteca-medios-v2.md`, carpeta padre) antes de escribir código; esta pasada es solo la Tanda 0: fijar el contrato para que firmware y Studio implementen contra el mismo formato, sin adivinar cada uno por su lado.

**Diseño**: dos elementos opcionales, `.rockbox/aura/artists/<archivo>.jpg` (foto cuadrada ≤128px, mismas reglas de formato que D.1) y `.rockbox/aura/artist_images.cfg` (índice). El índice usa el mismo parser `settings_parseline()` de D.2 pero **con las columnas invertidas** (`archivo: artista`, no `artista: archivo`): un nombre de artista puede traer `:` (p. ej. "Panic! At The Disco: Live"), y el parser corta en el primer `:` — solo el nombre de archivo (FAT-seguro) puede ir primero con seguridad. El firmware compara el tag `artist` de tagcache **byte a byte, sin normalizar** (confirmado: `apps/aura` nunca usa `tag_albumartist`, y `label_cmp()` solo hace case-insensitive ASCII) — la clave que Studio escriba tiene que ser el string UTF-8 exacto del tag, no la clave normalizada que usa internamente para agrupar (`albumArtist ?? artist`, con acentos plegados).

**Hecho**: `CONTRATO-firmware-studio.md` → **v6** (§D: dos filas nuevas; §D.3: formato completo, ejemplo, topes, degradación). Copia sincronizada a Aura Studio, `cmp` limpio. **Sin ningún cambio de código en esta pasada** — el módulo `aura_artist_images.c` y el layout circular de Artistas quedan para la Tanda 3 de `PLAN-biblioteca-medios-v2.md`.

**Verificado**: `cmp` entre ambas copias del contrato. No aplica build/test (solo Markdown).

**Pendiente**: implementación firmware (Tanda 3) y Studio (Tanda 5) del plan.

## D-320 — Retira el overlay de depuración de D-214 (texto blanco/negro al pie de pantalla)

**Encargo**: el dueño, en hardware real: "en la esquina inferior izquierda... cada que hay un suceso en el hardware, como cuando se pone a cargar, o cuando suena el piezo. Eso ya no quiero que se vea."

**Causa**: `aura_music_debug_mark()` (`aura_music.c:97-107` antes de este cambio) era instrumentación temporal de D-214 (freeze de reproducción, 2026-08-14) que dibujaba directo al framebuffer, fuera del sistema de viewports, para dejar en pantalla la última marca alcanzada antes de un posible cuelgue. El propio comentario de origen ya decía: "Retirar esta función (y sus llamados) en cuanto el freeze quede confirmado resuelto — no es parte del diseño final." El freeze se confirmó resuelto en **D-226** (desborde real de stack en `aura_transition_flip_and_flow()`, corregido con buffers `static`) y las marcas nunca se retiraron. La marca "K1/K2 piezo" (`aura_main.c`, gateada `#if !defined(SIMULATOR)`) explica por qué el dueño la vio solo en hardware y coincidiendo con cada botón (el Clicker de Aura viene encendido por default); las marcas "B1-B5" (`aura_music_buffer_event()`) y "D1-D3"/"T1-T2" (arranque de playlist) explican la coincidencia con "cuando se pone a cargar" (rebuffer/reingreso de pista al reanudar).

**Hecho**: eliminada la función `aura_music_debug_mark()` completa (`aura_music.c`, `aura_music.h`) y sus 7 llamados (`aura_music.c:237,241,247,250,255,964,967,971,973,1001,1003`; `aura_main.c:167,169`, junto con el comentario que las explicaba). Ningún archivo fuera de `apps/aura/` — sin entrada en `MODIFICATIONS.md`.

**Verificado**: build ARM completo (`firmware/build-ipod6g`, `make`) — limpio, sin warnings nuevos.

**Pendiente**: confirmación del dueño en hardware real (no queda nada que verificar en simulador, la marca de piezo nunca corría ahí).

## D-321 — Hora y zona horaria automáticas desde Aura Studio (contrato v7, §D.4)

**Encargo**: "cada que el ipod se conecte a Aura Studio, deberá actualizar su hora y region local para no tenerlo que configurar manualmente, igual al instalar o actualizar el firmware."

**Diseño**: el S5L8702 tiene RTC real (`CONFIG_RTC RTC_NANO2G`, `rtc-6g.c`) y Aura ya sabía escribirlo (`rtc_write_datetime()`, usado por las pantallas de Ajustes › Fecha/Ajustes › Hora) — lo que faltaba era un canal para que Studio lo alimentara. Se investigó primero si existía algún canal de "ejecutá esto" entre Studio y firmware: no existe ninguno, todo lo que Studio le "dice" al firmware pasa por claves reconocidas de `aura.cfg` (`settings_parseline()`, `strcmp` contra una lista fija) o por el marcador `/.aura/sync-pending.json` — ambos de solo datos, nunca comandos. Siete claves nuevas en `aura.cfg`: `rtc_sync_year/month/day/hour/min/sec` (**transitorias**, un solo uso — Studio las escribe frescas en cada conexión) y `tz_local_quarters` (ya existía desde D-293 como ajuste interno del reloj mundial; v7 es la primera vez que algo externo también la escribe). `aura_settings_apply_pending_clock()` (nueva, `aura_settings.c`) las lee; si las seis del RTC están completas, llama `rtc_write_datetime()` y `aura_settings_save()` — que al reescribir `aura.cfg` entero desde el struct en memoria descarta solas las claves transitorias, sin necesidad de un borrado explícito. Se llama desde `aura_main_sync_after_disk_handoff()` (D-293) — el mismo y único punto donde el firmware ya recupera el disco tras un posible USB de Studio (arranque y vuelta de la pantalla USB) — así que no hace falta un reinicio completo aparte para que la hora quede corregida.

**Causa de por qué el diseño evita reaplicar una hora vieja**: `aura_settings_load()` (que sí corre en cada arranque normal, con o sin Studio de por medio) nunca toca el RTC — solo `aura_settings_apply_pending_clock()`, llamada específicamente en el handoff de disco, lo hace, y solo si las seis claves transitorias siguen presentes (es decir, si Studio las dejó y el firmware todavía no las consumió). Un iPod que arranca sin haber pasado por Studio nunca ve su hora tocada.

**Deliberadamente fuera de alcance**: idioma (el encargo decía "hora y region", no idioma — un usuario puede preferir un idioma de UI distinto al de macOS a propósito); formato de fecha, 12h/24h, primer día de semana (no son parte del RTC).

**Hecho**: `aura_settings_apply_pending_clock()` + declaración en `aura_settings.h`; enganche en `aura_main_sync_after_disk_handoff()` (`aura_main.c`); `CONTRATO-firmware-studio.md` → **v7**, §D.4 nueva, dos filas nuevas en §D. Copia sincronizada a Aura Studio, `cmp` limpio. Contraparte Studio: **ST-035**.

**Verificado**: build ARM completo, limpio. 11/11 tests de host sin regresión (`aura_settings.c` no es un módulo host-testeable, hace I/O real — mismo criterio que el resto de `aura_settings.c`).

**Pendiente**: confirmación del dueño en hardware real (conectar con la hora del iPod desincronizada, verificar que corrige sola tras desconectar).

## D-322 — Fotos de artista circulares en Artistas + "Programas de TV" → "Series" (Tanda 3 de `PLAN-biblioteca-medios-v2.md`)

**Encargo**: Tanda 3 del plan aprobado — implementa el lado firmware del contrato v6 (D-319/§D.3): la sección Artistas de Música muestra la foto real del artista, en círculo, cuando Aura Studio la sincronizó; layout de 4 filas/54px, igual al de Álbumes. Independiente de las Tandas 1/2 (Studio), no requiere que el dueño haya probado esas todavía. **Aclaración del dueño durante esta pasada**: "sólo las imágenes de artistas van en el círculo, la opción de 'Canciones' debe permanecer normal (cuadrado con esquinas redondeadas)" — confirmado: `draw_album_list()`/`draw_album_thumb()` (Álbumes, incluida su fila sintética "Canciones") no se tocaron en ningún punto de esta pasada, siguen con `A26_LAYOUT_CORNER_RADIUS_CARD` de siempre.

**Diseño**: `aura_artist_images_parse.c/.h` (nuevo, C99 puro sin Rockbox — parte en el primer `:`, recorta espacios, ignora `#`/vacías/sin `:`; no reusa `settings_parseline()` de `apps/misc.c` porque esa función no es enlazable en un test de host sin arrastrar el resto de Rockbox) + `test/test_artist_images.c` (12 casos, incluida la reversión de campos respecto a `aura_media_categories.c` y que varias líneas apunten al mismo archivo). `aura_artist_images.c/.h` (I/O, calcado de `aura_media_categories.c`: `.rockbox/aura/artist_images.cfg`, 300 entradas, `loaded` antes de abrir; busca por ARTISTA — no por archivo, al revés de `aura_media_categories`, porque varias líneas pueden compartir archivo). `aura_albumart.c` gana `aura_artist_art_load()`/`aura_artist_art_load_default()`: círculo real (`radius = size/2`, confirmado en simulador que produce un círculo perfecto para `size` par), cache `.pfraw` con prefijo `ar-<hash FNV-1a de 32 bits del tag>-<size>` (no colisiona con `<seek>-<size>` de álbumes ni `pl-...` de playlists), placeholder = tile `A26_SELECTION_FILL` + ícono `"artist"` de 28px fijos (el mismo que ya usa la fila de Artistas del menú de Música) vía un `default_tile_with_icon()` nuevo, compartido con el default de Música (antes duplicaba código, ahora un solo cuerpo parametrizado por ícono/tamaño). `aura_screens.c`: `is_artist_list_screen()`, `draw_artist_list()`/`draw_artist_thumb()` calcados de `draw_album_list()`/`draw_album_thumb()`; halo de selección resuelto igual que D-267 (`aura_selection_summary.c`): `FBADDR` captura los píxeles reales de la pastilla ANTES del blit circular, `a26_shell_round_bitmap_corners_over_content()` los restaura fuera del círculo después. La fila sintética "Todos" (`seek == -1`) y "Artista desconocido" (jerga técnica traducida, nunca un tag real) van directo al placeholder sin buscar — para todo artista real, `label == raw` (confirmado en `run_search()`, `aura_music.c`), así que no hace falta un campo `raw[]` nuevo en `aura_music_item_t`. Invalidación: `aura_artist_images_invalidate()` en `aura_sync.c` (junto al `clear_dir(CF_CACHE_DIR)` de `finish_ok()`, sección música) y al entrar a `AURA_SCREEN_MUSIC_ARTISTS` (mismo gancho central que ya usa `aura_video_invalidate()`/`aura_photos_invalidate()`).

**Ajuste de implementación respecto al plan** (regla §4.1.1: ajustar ubicación, no diseño, y reportar): el plan pedía `extra = mtime` del `.jpg` en la clave del cache `.pfraw` para auto-invalidación. Se usa `PFRAW_EXTRA_NONE` (0) en su lugar: `finish_ok()` ya vacía **todo** `CF_CACHE_DIR` (incluidos los `ar-*.pfraw`) en cada sync de música — la invalidación por mtime sería enteramente redundante con eso, y Rockbox no expone un stat de un solo archivo (solo `opendir`+`readdir`+`dir_get_info`), así que calcularlo en cada fila visible habría sido un escaneo de directorio completo por cuadro sin ganar cobertura que el vaciado de cache no diera ya. Mismo comportamiento observable, sin el costo.

**"Programas de TV" → "Series"**: `aura_lang.c` (`AURA_STR_VIDEOS_TVSHOWS`, ES y EN — unificado con `MediaCategory.series.displayName` de Aura Studio, D-318/finding 12 del plan). `docs/aura-design-system/sistema/03-arbol-de-menus.md` y `componentes/cover-drift.md` actualizados.

**Hecho**: `apps/aura/aura_artist_images_parse.c/.h` (nuevo), `apps/aura/aura_artist_images.c/.h` (nuevo), `apps/aura/test/test_artist_images.c` (nuevo), `apps/aura/test/Makefile`, `apps/SOURCES`, `apps/aura/aura_albumart.c/.h`, `apps/aura/aura_screens.c`, `apps/aura/aura_sync.c`, `apps/aura/aura_lang.c`, dos docs de `docs/aura-design-system/`.

**Verificado**: build ARM completo (`firmware/build-ipod6g`), limpio — solo el `-Wtype-limits` preexistente de `aura_style.c:89`. **12/12** suites de host (`test_artist_images` nuevo, 28/28 checks). Simulador (`build_sim.sh` limpio, capturas con `apple2026_sim_shot.sh`, fixtures sintéticos: dos artistas — uno con foto vía JPEG generado con AppKit/CoreGraphics tal como D-303 exige, no ffmpeg — descubierto en esta misma verificación que un JPEG `mjpeg` de ffmpeg decodifica con colores incorrectos en el decoder liviano de Rockbox, dato nuevo para futuros fixtures de imagen; uno sin foto): lista de Artistas con foto circular real, placeholder circular con ícono "artist" para el artista sin foto y para "Todos", **sin halo visible en la fila seleccionada**, confirmado en **tema claro y oscuro**. Fixtures del simulador limpiados al terminar (no committeados, `simdisk/` está en `.gitignore`).

**Pendiente**: release `v0.3.0-beta` (MINOR — sube el contrato a v6 de verdad, ya no solo el formato) solo con autorización explícita del dueño; luego pin en Studio (ST-039, §E nueva fila). **BARRERA 3** del plan: el dueño actualiza el firmware y confirma en hardware real que la lista de Artistas se ve con el layout de 4 filas y placeholders circulares (las fotos reales llegan recién en la Tanda 5, export desde Studio).

## D-323 — Cuadrícula de miniaturas en Fotos, reemplaza la lista con nombre (D-291)

**Encargo** (el dueño, 2026-08-19, tras cerrar ST-044 en Aura Studio): "una interfaz a pantalla completa, cuadrícula sin nombre, para desplazarnos rápido, muy similar a la del iPod Classic original — con la barra de estado visible". Corrección explícita del dueño en el mismo hilo: esta parte del pedido original era sobre el **Firmware** (el dispositivo), no Aura Studio — la cuadrícula de álbumes de Studio (ST-042) se queda tal cual. Cierra el item "Pendiente de definir" que `componentes/photo-viewer.md` (D-291) dejó abierto explícitamente: "Rejilla de miniaturas (2D) en vez de lista — el original la usa; esta pasada eligió lista por reutilizar el patrón ya aprobado de Álbumes sin inventar un componente de selección 2D nuevo".

**Diseño**: `draw_photos_grid()` (nueva, `apps/aura/aura_photos.c`) reemplaza `draw_photos_list()` para las 4 pantallas de Fotos (`AURA_SCREEN_PHOTOS_ALL/PHOTO/IMAGE/AI`) — sin agregar ningún `aura_screen_id_t` nuevo, así que ninguno de los switches de `aura_screens.c` que despachan por esas 4 pantallas necesitó tocarse. Primer layout 2D de todo `apps/aura/` (confirmado por revisión: cero precedente de grid multi-columna en el árbol — todo lo demás es lista de una columna); la selección sigue siendo el mismo índice LINEAL de siempre (`aura_nav_get/set_selection`), solo el dibujado mapea índice → (fila, columna) en orden de lectura, izquierda a derecha y arriba a abajo — igual que giraba la rueda del iPod Classic original sobre su cuadrícula.

Celda **cuadrada** de 55px: 5 columnas (275px, centradas — sobran ~22px de cada lado, de sobra para el riel de scroll de 4px sin overlap) × 4 filas (220px, exacto el alto útil bajo la barra de estado de 20px, cero sobra) — ambos números salen de `A26_SCREEN_WIDTH`/`A26_SCREEN_HEIGHT`, no son arbitrarios. Miniatura: mismo `PHOTO_ART_SIZE` (48px) de siempre, centrada dentro de la celda — reusa `photo_thumb_decode_and_cache()`/`draw_photos_thumb()` **sin ningún cambio**, mismo cache `.pfraw` en disco (bonus gratis: el cache que ya generó la lista sigue sirviendo). Selección: `a26_shell_fill_rounded_rect()` (mismo color `A26_SELECTION_FILL` de la pastilla de la lista) ANTES de blitear la miniatura encima — mismo orden de dibujo que ya usaba la pastilla, ninguna primitiva nueva. La fila final inerte "…y N más" de la lista (más de `MAX_PHOTOS`) se comprime a una celda "+N" (número solo, sin palabras — no hace falta entrada nueva en `aura_lang.c`); mismo criterio de siempre: nunca trunca en silencio, SELECT no hace nada sobre ella (`aura_photos_handle_button()` ya la excluye vía `s_filtered_count`, sin cambios en esa parte).

**Navegación**: `BUTTON_SCROLL_FWD/BACK` pasan de avanzar ±1 fijo a `aura_wheel_advance()` (dinámica real de la rueda — 1 a 3 celdas por evento según velocidad angular, mismo mecanismo ya usado en Álbumes/menú principal/Alarmas) — encaja exacto con "una forma rápida de desplazarnos" del encargo, sin inventar nada nuevo. Como la cuadrícula recorre en orden de lectura (índice lineal), "avanzar rápido" simplemente salta varias celdas seguidas — sin ejes fila/columna independientes ni binding nuevo de LEFT/RIGHT (la lista tampoco los usaba; se preserva el mismo criterio). El visor de foto completa (`aura_photo_viewer_draw/handle_button`) no se tocó — SELECT sigue abriéndolo sin StatusBar, MENU sigue devolviendo a la cuadrícula con la selección sincronizada a la foto que se estaba viendo (mismo contrato de D-291, `aura_nav_set_selection()` después de `aura_nav_pop()`).

**Hecho**: `apps/aura/aura_photos.c` (`draw_photos_grid()` reemplaza `draw_photos_list()`; constantes `PHOTO_GRID_*` reemplazan `PHOTO_ROW_H/PHOTO_LIST_TOP/PHOTO_ART_X/PHOTO_TEXT_GAP/PHOTO_VISIBLE`; incluye `aura_screens.h` para `aura_wheel_advance()`), `docs/aura-design-system/componentes/photo-viewer.md` (cierra el pendiente, documenta la cuadrícula).

**Verificado**: build del simulador limpio (`build-sim`, sin warnings nuevos). Verificación visual real con `apple2026_sim_shot.sh` contra los 16 archivos de prueba ya presentes en `simdisk/Photos` (fixtures de D-291/D-303/D-316/D-322, sin agregar nada nuevo): cuadrícula 5×4 con la barra de estado visible arriba (reloj/título/batería) y **sin ningún nombre de archivo en pantalla**; verificación programática (Python/PIL sobre el PNG del dump, no solo inspección visual) confirmó que los límites de columna caen exactos en `PHOTO_GRID_LEFT + col*55` para las 5 columnas. Una celda aparece en blanco en las capturas — investigado antes de descartarlo como bug: es `Foto Real 2.jpg` (4608×3072 = 14.2 MP), por encima del tope de 12MP de `photo_dims_too_large()` (comportamiento preexistente de D-291, placeholder honesto, no una regresión de esta pasada). Rueda: 3 `SCROLL_FWD` desde la primera celda mueven la selección a la celda 4 en orden de lectura, confirmado por captura. Visor: SELECT abre la foto correcta a pantalla completa sin StatusBar; MENU vuelve a la cuadrícula con la misma celda resaltada. Build ARM real (`build-ipod6g`) y hardware real **sin correr todavía** en esta pasada.

**Pendiente**: build ARM completo y **verificación en hardware real** por el dueño (la única forma real de juzgar "se siente como el iPod Classic original" es en el dispositivo, con la rueda física). Fuera de alcance de esta pasada, mencionados por el dueño en el encargo original pero no pedidos todavía: modo presentación y paneo/recorte del visor de foto completa (ambos ya listados como pendientes en D-291, sin relación con esta cuadrícula).

## D-324 — Contrato v8 y v9: registro cruzado (sin cambio de código en este repo)

`CONTRATO-firmware-studio.md` sube a **v8** (ST-046 en Aura Studio) y **v9** (ST-047), copia idéntica aquí por ser la canónica. Ninguno de los dos exige nada de Aura-Firmware:

- **v8** agrega a §D la clave `firmware_family` de `aura.cfg`. **Aura nunca la escribe ni la escribirá**: su ausencia es, por contrato, la firma de Aura — es lo que hace el cambio retrocompatible con toda instalación existente. La escribe Metro-Aura (`firmware_family: metro`); Studio la lee para nombrar el firmware y consultar actualizaciones en el repositorio correcto. Motivo: un iPod con Metro se detectaba como Aura y Studio le ofrecía el Release de Aura, que al aceptarse lo habría sobrescrito.
- **v9** generaliza §A (el canal es el Release del repositorio de la familia: `Aura-Firmware` o `Metro-Aura`, mismos assets), §B (la pantalla de Licencias de Studio existe y lista cada familia embebida) y §E (`FIRMWARE_VERSION` con sección por familia). §D no cambia.

Implicación para este repo: **ninguna regla nueva**, pero sí una a conservar — Aura no debe empezar a escribir `firmware_family` sin subir el contrato, porque hoy la ausencia significa "Aura".

## D-325 — Las listas de Música se cortaban en 300 ("no hay canciones después de la E")

**Reporte del dueño (2026-08-23):** con su biblioteca real sincronizada, Canciones muestra 300 y se detiene en la E — en Aura y en Metro-Aura por igual (M-087 allá, misma herencia).

**Causa.** `AURA_MUSIC_MAX_ITEMS = 300`, un solo tope para todas las listas. Tagcache entrega los títulos **ya ordenados**, así que las primeras 300 que caben son de la A hasta donde alcance, y no había ningún aviso en pantalla. Las canciones están en el disco y en la base; la pantalla no tenía dónde ponerlas.

**Decisión.** Un tope por clase de lista, estático (nunca en la pila de 8 KB, D-226):
- `AURA_MUSIC_MAX_SONGS = 5000` — el navegador genérico (`s_music_cache`, que también es Canciones y las canciones de un género), `s_tracknums` y los scratch de `aura_music_play_*` que arman la lista de reproducción.
- `AURA_MUSIC_MAX_GROUPS = 2000` — artistas, álbumes, géneros, listas, sublistas; `AURA_MUSIC_MAX_ITEMS` queda como alias de este para los llamadores que no distinguen.
- **El pool de CoverDrift NO crece**: sigue en 300 a propósito (D-316 — nadie necesita rotar entre más de 300 imágenes para que la sensación de variedad funcione), ahora con constante propia `AURA_DRIFT_POOL_MAX`, y el bucle que lo llena desde la lista de álbumes **se acota** a esa cota — antes el scratch y el pool compartían tamaño; al crecer la lista de álbumes a 2 000 habría desbordado el pool.

`.bss` pasa a 11.2 MB en ARM; el búfer de audio queda en ≈51 MB. Los recorridos de tagcache corren contra la copia en RAM (`tagcache_ram = 1` en `apps/main.c`).

**El tope ya no es silencioso:** el navegador genérico agrega una fila final inerte (`dimmed`) "…y más: la lista está llena" (`AURA_STR_LIST_TRUNCATED`, añadida al final de ambos catálogos) cuando llega al tope; la rueda sigue acotada a las filas reales, así que no es seleccionable. Las vistas con carátula (álbumes, artistas) no la reciben: su índice es el de un elemento real.

**Pendiente:** cronometrar en el iPod cuánto tarda entrar a Canciones con miles de pistas.

## D-326 — Contrato v10: dos firmwares instalados a la vez, conmutación por renombre (registro; implementación en D-327)

`CONTRATO-firmware-studio.md` sube a **v10** (ST-056 en Aura Studio; copia canónica aquí). Define: el árbol activo sigue siendo `/.rockbox/` (lo único que el bootloader compartido sabe arrancar); los árboles dormidos se llaman `/.firmware-aura/` y `/.firmware-metro/` y conservan sus propios ajustes; cambiar de firmware es guardar → dos renombres (saliente primero) → copiar el `rockbox.ipod` de respaldo en la raíz → dejar `/.aura/sync-pending.json` con `music: true` → reinicio en seco. Lo puede hacer Studio o el propio firmware desde Ajustes. Studio escribe los archivos del contrato también en los árboles dormidos y repara un cambio a medias.

**Para este repo** la implementación es la fila "Cambiar a Metro" en Ajustes (D-327, después de que Studio y Metro-Aura la tengan; orden acordado con el dueño: Studio → Metro → Aura). Hasta entonces, nada cambia en el firmware: un árbol dormido en el disco es inerte para Aura.

## D-327 — "Cambiar a Metro" en Ajustes (contrato v10: dos firmwares, cambio por renombre)

Cierra D-326 del lado del firmware; es el calco de M-090 de Metro-Aura (misma secuencia, mismo contrato), hecho en último lugar como acordó el dueño (Studio ST-056 → Metro M-090 → Aura).

**`aura_firmware_switch.c`** (módulo nuevo, cabecera GPL): `aura_firmware_metro_installed()` (existe `/.firmware-metro`) y `aura_firmware_switch_to_metro()`: (1) `aura_settings_save()` + `settings_save()` + `tagcache_shutdown()` + `call_storage_idle_notifys(true)` — todo lo de Aura a disco **ahora**, porque tras el renombre `/.rockbox` es el árbol de Metro; (2) `/.rockbox` → `/.firmware-aura` (saliente primero); (3) `/.firmware-metro` → `/.rockbox`, deshaciendo (2) si falla; (4) `/rockbox.ipod` de raíz := el del árbol (respaldo del bootloader, siempre el activo; copia a trozos con buffer estático, D-226); (5) `aura_sync_write_music_pending_marker()` (nuevo en `aura_sync.c`: marcador con `music: true`, la base vive dentro de cada árbol); (6) `system_reboot()` — nunca el apagado normal. Si ya existiera `/.firmware-aura` aborta sin borrar nada.

**Fila** "Cambiar a Metro" (icono `ipod`, libre en esa lista — D-075) entre "Reconstruir biblioteca" y "Restablecer ajustes"; pantalla `AURA_SCREEN_SETTINGS_SWITCH_FIRMWARE` (al final del enum): aviso Sí/No calcado del de reconstruir cuando hay árbol dormido, texto informativo (como Avisos legales) explicando cómo instalarlo desde Aura Studio cuando no. Tres cadenas nuevas al final de ambos catálogos.

Verificado en simulador con un árbol dormido de prueba: tras "Sí", `.rockbox` es el de Metro (`firmware_family: metro`), `.firmware-aura/aura/aura.cfg` conserva los ajustes de Aura, `/rockbox.ipod` es el binario de Metro y el marcador trae `music: true`. (El `system_reboot()` del simulador no termina el proceso; en el iPod es un reinicio real.) De paso: el simulador estaba configurado contra `gcc-15`, ya retirado por Homebrew; `build_sim.sh --reconfigure` lo dejó en gcc-16.

## D-328 — Contrato v11: actualizaciones selectivas por manifiesto (registro; sin trabajo en este repo)

`install_manifest.cfg` en `.rockbox/aura/`, escrito y leído solo por Aura Studio (ST-058) para extraer únicamente lo que cambió entre releases (medido: ~5 archivos de 9 431). Este firmware lo ignora — la única regla nueva es no adoptar ese nombre de archivo para otra cosa.

## D-329 — El cambio de firmware deja de reconstruir la base sin motivo (contrato v12, sello de biblioteca)

**Reporte del dueño:** *"cada que cambio de un firmware a otro… ambos firmwares crean nuevamente la base de datos: son 5 minutos en los que dejan inutilizable el iPod"*, sin que la biblioteca haya cambiado.

**Causa: nosotros.** El cambio (v10) dejaba el marcador con `music: true` **siempre**. **Corrección (v12):** `/.aura/library-stamp` (solo cambia cuando un sync de Studio toca música) + `.rockbox/aura/db_stamp.txt` por árbol (qué sello tenía la biblioteca cuando ese firmware construyó su base; se anota en `finish_ok()` de `aura_sync.c`, que cubre la reconstrucción por marcador y la manual). Al cambiar (`aura_firmware_switch.c` y el resto de las partes): sellos iguales → **sin marcador, sin reconstrucción**; distintos o ausentes → como antes. Arranque en frío: si el sello compartido no existe, el saliente —cuya base está al día porque acaba de estar corriendo— lo crea y se lo anota, así el primer ciclo de ida y vuelta ya solo reconstruye una vez por árbol.

Verificado en simulador: primer cambio sin sello → marcador + sello anotado al saliente; vuelta con sellos iguales → sin marcador. Studio hace lo mismo (ST-059) y renueva el sello en cada sync con música.

## D-330 — Pantalla USB rediseñada: tile de app con glifo de sincronización (encargo del dueño)

**Encargo:** *"al conectarlo aparece el texto 'aura', casi la misma pantalla que el splash. Me gustaría un gráfico como Apple lo hubiera diseñado en la actualidad para el iPod."*

**Restricción (D-223, sigue igual):** durante la sesión USB las fuentes están descargadas y no se puede leer nada del disco. Lo disponible: funciones puras del shell (`a26_color`, `a26_shell_fill_rounded_rect`, `a26_shell_blend`) y los dos bitmaps embebidos.

**Diseño.** Fondo del tema (D-225); al centro un **tile estilo icono de app**: superficie redondeada de 96 px en el acento (proporción de radio de icono de Apple) con el **glifo de sincronización de Lucide en blanco** (el bitmap `usblogo.176x48x16.bmp` se regeneró: ya no es el logo "USB" de Rockbox sino el glifo con antialiasing horneado, usado como **máscara de luminancia**); debajo, el wordmark "aura" también como máscara, **en el color de texto del tema** — de paso se corrige que el bitmap opaco habría pintado una losa negra en el tema claro. Sin texto de fuente alguna.

De paso: el inyector del simulador gana el token `USB_INSERT` (portado del M-039 de Metro-Aura) — la pantalla se verificó en el simulador con él (`docs/screenshots/d330-usb-screen.png`).

## D-331 — Movie Flow nunca decodificó un cartel real: búfer de `read_jpeg_file()` del tamaño exacto del bitmap final (reporte del dueño, contrato v13 de paso)

**Reporte del dueño (hardware real):** dos películas correctamente catalogadas desde Aura Studio, con su póster `.jpg` hermano en `/Videos`; una ni siquiera aparecía como película y la otra aparecía **sin cartel** en Movie Flow.

**Dos causas independientes, una por síntoma.**

**(1) El cartel (este repo).** `load_slot()` de `aura_movieflow.c` pasaba a `read_jpeg_file()` un búfer de `MVF_COVER_W×MVF_COVER_H` fb_data — 38 400 bytes, el tamaño EXACTO del bitmap final. Pero el decodificador coloca DETRÁS del bitmap final todo su estado (`struct jpeg` ~7 KB + búfer de MCUs + 3 líneas del reescalador — `JPEG_DECODE_OVERHEAD`, `recorder/jpeg_load.h`): con un póster real (427×640 baseline) la llamada devolvía `-1` **siempre** y todo cartel degradaba en silencio al placeholder sólido. Es el mismo modo de fallo silencioso ya documentado en `aura_albumart.c` (D-254); Movie Flow nunca se había verificado visualmente con un póster real (D-318 lo dejó "a cargo del dueño en hardware"). Corrección: +64 KB de margen sobre el bitmap final en el búfer estático. Auditados los demás llamadores de `read_jpeg_file()` del directorio: `aura_photos.c` (240 KB), `aura_albumart.c` (fórmula de D-254) y `aura_nowplaying.c` (64 KB para 135 px, le sobran ~20 KB) están bien dimensionados.

**(2) La categoría (Aura Studio, ST-062 — contrato v13).** El driver `msdosfs` de macOS guarda los nombres largos de FAT32 **precompuestos (NFC)** pero se los reporta **descompuestos (NFD)** a las apps: Studio escribía `video_categories.cfg` en NFD y este firmware compara byte a byte contra el UTF-16 que lee del disco (NFC) — "Avatar Aang el **último** maestro del aire.mpg" no emparejaba jamás; su vecino 100 % ASCII sí. **Este firmware no cambia**: siempre comparó contra lo que hay en el disco, que es lo correcto. La regla nueva del contrato (v13, §D.2): todo nombre/ruta dentro de un archivo del contrato viaja en NFC.

**Verificado**: simulador con los dos pósters reales del iPod del dueño y el cfg en NFC — Movie Flow muestra ambos carteles decodificados y "Avatar Aang el último maestro del aire" (con acento) categorizado como película. Build ARM limpio.

## D-332 — Movie Flow: el cartel llena el lienzo y se recorta al centro (esquinas redondeadas de verdad)

**Reporte del dueño (hardware, tras D-331):** los carteles ya se ven, "sin embargo no se ven con las esquinas redondeadas".

**Causa.** Un póster real casi nunca es 3:4 exacto (cine ~2:3, 427×640). El decode con `FORMAT_KEEP_ASPECT` lo dejaba AJUSTADO POR DENTRO del lienzo de 120×160 (107×160 centrado, barras de fondo a los lados), y `mvf_mask_corners()` redondea las esquinas del LIENZO — que ahí son fondo, invisibles; las del cartel quedaban cuadradas.

**Corrección** (regla del sistema, `componentes/music-flow.md`: la carátula LLENA su geometría y el radio de 8px aplica sobre la imagen): si el primer decode no llena el lienzo, se redecodifica al tamaño que sí lo cubre (mismo aspecto, lado corto = lado del lienzo) y el centrado existente recorta el excedente por igual en ambos extremos — el mismo bucle ya manejaba offsets negativos, no hubo lógica nueva de recorte. Con presupuesto: solo si el resultado cabe en `decode_buf` con el margen del estado del decodificador (D-331); una fuente absurdamente panorámica conserva el ajuste por dentro. `componentes/movie-flow.md` actualizado en la misma pasada.

**Verificado**: simulador con los pósters reales del dueño — el cartel frontal llena los 120×160 y la esquina muestra el radio de 8px antialiasado sobre la imagen (`docs/screenshots/d332-movieflow-fill-crop.png`). Sin release nuevo a pedido del dueño: queda en `main` para el siguiente.

## D-333 — "Cambiar sistema" en Ajustes: una fila por familia hermana, tabla de hermanas, moonlit.aura (contrato v14)

Con la tercera familia (**moonlit.aura**, `Ricolinos/moonlit-aura`, `firmware_family: moonlit`, árbol dormido `/.firmware-moonlit`) la fila única "Cambiar a Metro" de D-327 se quedó corta: Aura solo sabía despertar a Metro. Es el calco de M-093 (Metro) y D-047 (moonlit), en el mismo orden acordado para D-327 (las hermanas primero, Aura al final).

**Tabla pura de hermanas** — `aura_firmware_families.c/.h` (módulo nuevo, sin I/O, cabecera GPL, en `apps/SOURCES`): `{ "/.firmware-metro", AURA_STR_FAMILY_METRO }, { "/.firmware-moonlit", AURA_STR_FAMILY_MOONLIT }` + `AURA_FIRMWARE_OWN_DORMANT` (`/.firmware-aura`). API: `aura_firmware_sibling_count()`, `aura_firmware_sibling_dormant_dir(i)` (NULL fuera de rango), `aura_firmware_sibling_name(i)` (`AURA_STR_COUNT` → `""` fuera de rango). Separada del módulo con I/O a propósito para el test host `test/test_firmware_families.c` (count == 2, ningún dormido es el propio, prefijo `/.firmware-`, directorios y nombres distintos, índice fuera de rango seguro). **`aura_firmware_switch.c`** queda parametrizado: `aura_firmware_sibling_installed(i)` y `aura_firmware_switch_to(i)` con **exactamente** la secuencia de 6 pasos de D-327/D-329 (guardar todo → `/.rockbox` → `/.firmware-aura` → `/.firmware-<hermana>` → `/.rockbox`, deshaciendo si falla → `/rockbox.ipod` de raíz → marcador solo si el sello difiere → `system_reboot()`); los demás dormidos no se tocan. `aura_firmware_metro_installed()`/`aura_firmware_switch_to_metro()` se retiran (sin usos).

**UI** — la fila de Ajustes pasa a ser **"Cambiar sistema"** (icono `ipod`, mismo sitio entre "Reconstruir biblioteca" y "Restablecer ajustes") y abre un submenú `[SPLIT]` calcado de Personalización (`switch_system_entries[]`, `AURA_SCREEN_SETTINGS_SWITCH_SYSTEM`, enganchado en `get_nav_table()`, `screen_title_id()`, `parent_settings_icon()`, los cuatro sitios de lista nav, `aura_category.c` y ambos dispatchers): "Cambiar a Metro" (`ipod` — lista distinta a la de Ajustes, D-075) y "Cambiar a moonlit.aura" (`theme-dark`, la luna). Cada fila abre la misma pantalla de D-327 parametrizada por índice de familia: `AURA_SCREEN_SETTINGS_SWITCH_FIRMWARE` **conserva su id** y pasa a ser la confirmación de Metro (alias `AURA_SCREEN_SETTINGS_SWITCH_TO_METRO`); `AURA_SCREEN_SETTINGS_SWITCH_TO_MOONLIT` es nueva, al final del enum con `SWITCH_SYSTEM` ("solo-añadir-al-final"). `draw_switch_firmware(family)`/`handle_switch_firmware(nav, family, button)` toman título/cuerpo de una tabla por familia (`switch_fw_texts()`): aviso Sí/No si `aura_firmware_sibling_installed(i)`, texto informativo (como Avisos legales) si no.

**Cadenas** (al final de ambos catálogos): `AURA_STR_SETTINGS_SWITCH_SYSTEM` "Cambiar sistema", `AURA_STR_FAMILY_METRO`/`_MOONLIT`, `AURA_STR_SWITCH_TO_METRO_ROW`/`_MOONLIT_ROW`, `AURA_STR_SWITCH_MOONLIT_CONFIRM_BODY`/`_MISSING_BODY` (mismo tono y longitud que las de Metro, mencionando Aura Studio › Extras › Firmware). Las tres de D-327 se conservan; **`AURA_STR_SETTINGS_SWITCH_FIRMWARE` ("Cambiar a Metro") queda sin uso** — se deja por la regla de solo-añadir-al-final, anotado en `aura_lang.h`.

**Verificado**: 13 tests host verdes (12 previos + `test_firmware_families`); simulador sin warnings; ARM `rockbox.ipod` compila. Capturas (`docs/screenshots/v0.4.0-beta-cambiar-sistema-lista.png`, `…-moonlit-no-instalado.png`, `…-moonlit-confirmar.png` — la última con `simdisk/.firmware-metro` y `.firmware-moonlit` creados vacíos y borrados después). Secuencia de botones desde el arranque (`apple2026_sim_shot.sh`): `SCROLL_FWD×4, SELECT` (Ajustes) `, SCROLL_FWD×20, SELECT` (Cambiar sistema) `, SCROLL_FWD, SELECT` (moonlit). Nota de herramienta: el script hace `cd firmware/build-sim` antes del `mkdir -p`, así que una ruta de salida relativa cae en `build-sim/docs/…` — usar ruta absoluta o mover después. No se ejecutó un cambio real a moonlit en el simulador (no hay árbol de moonlit a la mano): la secuencia de renombres es byte a byte la de D-327, solo cambia el directorio entrante.

## D-334 — `__TIME__`/`__DATE__` fuera de los plugins SDL (reproducibilidad del `rockbox.zip`, contrato v11)

`CLAUDE.md` exige que nada dentro de `rockbox.zip` cambie sin necesidad entre releases (la actualización selectiva de Studio extrae solo lo que cambió, D-328). Tres fuentes de Rockbox aún horneaban la hora de compilación en el binario del plugin: `apps/plugins/sdl/progs/quake/host.c:884` y `host_cmd.c:958` (`Con_Printf("Exe: "__TIME__" "__DATE__"\n")`) y `apps/plugins/sdl/progs/duke3d/Engine/src/display.c:711` (`__DATE__` en el banner del driver SDL) — `quake.rock` y `duke3d.rock` salían distintos en cada build aunque no cambiara una línea. Reemplazados por la cadena fija `"rockbox build"`; el guardia `#if (!defined __DATE__) #define __DATE__ …` de duke3d se retira con su único usuario. `grep '__TIME__\|__DATE__'` en esos dos progs solo devuelve los comentarios `aura (D-334)`. Registrado en `MODIFICATIONS.md` (27 → 30 archivos) y marcado inline en los tres.

## D-335 — Contrato v14: tres familias y registro de familias (§A bis)

Canónico en `CONTRATO-firmware-studio.md` de este repo; Aura Studio y las hermanas reciben copia idéntica en su propia pasada. Cláusulas:

- **Nota v14** antes de la v13; todo "la otra familia" pasa a "cualquier otra familia". §D no cambia de formato.
- **§A**: el Release es el del repositorio de la familia según §A bis. **§A bis — Registro de familias** (nuevo): tabla `familia | firmware_family | repositorio | árbol dormido | centinela | prefijo FIRMWARE_VERSION | subdirectorio del bundle` con aura (ausente / `Ricolinos/Aura-Firmware` / `/.firmware-aura/` / `.rockbox/fonts/a26-title-20.fnt` / sin prefijo / raíz), metro (`metro` / `Ricolinos/Metro-Aura` / `/.firmware-metro/` / `metro-list-20.fnt` / `metro.` / `metro/`) y moonlit (`moonlit` / `Ricolinos/moonlit-aura` / `/.firmware-moonlit/` / `moonlit-body-18.fnt` / `moonlit.` / `moonlit/`). `AuraPalette.swift`, `theme-format-v1.json`, `aura-theme-default.zip` exclusivos de Aura; moonlit (sin temas) no los publica ni declara `theme_format_supported`.
- **§B**: moonlit versiona la frontera GPL con `BOOT-N` en su `CONTRATO-moonlit-studio.md` §B; su ausencia en otras familias no es error.
- **§D**: fila de dormidos → `/.firmware-<familia>/` (§A bis), hasta N−1 dormidos, nunca dos de la misma familia ni uno de la activa, activo siempre `/.rockbox/`; `firmware_family` con valores registrados en §A bis (`metro`, `moonlit`), ausente = aura, desconocido NO es Aura; centinela → `.rockbox/fonts/<centinela de la familia>`.
- **Nota v10** generalizada: renombres `/.rockbox/ → /.firmware-<saliente>/`, `/.firmware-<entrante>/ → /.rockbox/`; "Ajustes › Cambiar sistema, una fila por hermana, inerte si su dormido no existe"; recuperación con exactamente un dormido automática, con dos o más Studio pide elegir; espejado a **todos** los dormidos; instalar una familia estaciona la activa y no toca los demás dormidos.
- **§E**: `FIRMWARE_VERSION` una sección por familia (sin prefijo / `metro.` / `moonlit.`); `fetch-firmware.sh --family aura|metro|moonlit`. **§G**: `CONTRATO-moonlit-studio.md` vive solo en moonlit-aura y referencia este contrato.
- **Pendientes**: retirado "Primer Release público de Aura-Firmware" (hay tags publicados desde v0.2.0-beta); anotado que el cambio entre cualquier par de familias desde el dispositivo queda implementado en las tres (D-333/M-093/D-047) y que lo pendiente es solo lado Studio (recuperación con varios dormidos, `--family moonlit`).

## D-336 — Contrato v15: base tagcache y miniaturas compartidas bajo `/.aura`, claves estables de carátula

Canónico en `CONTRATO-firmware-studio.md` de este repo; Aura Studio y las hermanas reciben copia idéntica en su propia pasada (Metro-Aura ya implementa su lado como M-095/M-096). Encargo del dueño (2026-08-26): que los tres firmwares compartan la base tagcache y, Metro↔moonlit, las miniaturas, para no reconstruir al cambiar de familia ni al reinstalar; la base solo se (re)construye cuando Studio sincroniza música. Cláusulas:

- **Nota v15** antes de la v14. **§D**: dos subdirectorios nuevos dentro de `/.aura/` (hasta hoy buzón de Studio), **propiedad del firmware y compartidos entre familias**: `/.aura/tagcache/` (todos los `database_*.tcd` + `db_stamp.txt`, el sello v12 ahora único) y `/.aura/thumbs/{albums,artists,photos}/` (`.mth` de 80 px, Metro-Aura y moonlit.aura; Aura no las usa — su `cfcache/` es de otro formato y sigue en su árbol). Los árboles `/.rockbox/` y `/.firmware-*/` dejan de contener `database_*.tcd` ni `aura/db_stamp.txt`; un firmware que los encuentra en su árbol los **migra por `rename`** al compartido si éste no existe (si existe, los borra: peso muerto). Studio nunca borra `/.aura/tagcache/` ni `/.aura/thumbs/` al instalar, cambiar de familia ni sincronizar — salvo `triggerFirmwareDBRebuild`, que ahora borra `database_*.tcd` y `db_stamp.txt` **en `/.aura/tagcache/`** (además de en los árboles, compatibilidad con firmwares anteriores a v15). `install_manifest.cfg` no los lista.
- **Nota v12 actualizada**: el sello vive en `/.aura/tagcache/db_stamp.txt`; el cambio de familia compara **ese** sello (no el del árbol entrante) con `/.aura/library-stamp`; los firmwares sellan también tras el rebuild de primer arranque y al hallar una base compartida usable sin sello. Paso 5 de v10 reescrito en consecuencia.
- **"Claves de caché de carátulas"** (párrafo nuevo): las cachés de carátula de álbum de los tres se indexan por `crc32(ruta de la pista representativa)` + `mtime` de esa pista, nunca por `seek` de tagcache (cambia en cada rebuild), de modo que una reconstrucción no invalida carátulas. Formatos por familia siguen siendo propios (Aura `cfcache` 130 px transpuesto; moonlit `art` 120 px; Metro/moonlit `.mth` 80 px compartidos).
- **Tabla §D**: filas nuevas `/.aura/tagcache/`, `/.aura/tagcache/db_stamp.txt`, `/.aura/thumbs/…`; las filas `.rockbox/aura/db_stamp.txt` y `.rockbox/database_*.tcd` quedan marcadas como retiradas en v15 (solo origen de migración / compatibilidad de Studio con firmwares viejos). §A bis sin cambios. **Pendientes**: anotado lo que falta del lado Studio y de moonlit.aura.

## D-337 — Base tagcache compartida en `/.aura/tagcache` con migración por `rename`; sello tras el rebuild de primer arranque (contrato v15)

**Hechos de partida.** `apps/tagcache.c`/`.h` son byte-idénticos en los tres repos; la ruta en runtime es `global_settings.tagcache_db_path` (`settings_list.c:1817`, copiada a `tc_stat.db_path` en `tagcache_init()`, `tagcache.c:5624`; `open_db_fd()` hace `mkdir` del directorio en la primera escritura). Aura solo forzaba `tagcache_ram` en las dos variantes de `init()` de `apps/main.c`. El sello v12 (`aura_sync.c`, `ROCKBOX_DIR/aura/db_stamp.txt`) solo se escribía en `finish_ok()`; el rebuild de bootstrap de `aura_music_db_ready()` no sellaba, así que la primera conmutación tras una instalación limpia reconstruía otra vez.

**Dueño de la ruta.** `#define AURA_SHARED_DB_DIR "/.aura/tagcache"` vive en **`aura_sync.h`** — el módulo que ya era dueño de `/.aura` del contrato (`AURA_SYNC_DIR`, marcador, `library-stamp`); nadie más deletrea la ruta (`grep -rn 'database_\|db_stamp' apps/aura/ apps/main.c` → todo vía `AURA_SHARED_DB_DIR`/`AURA_DB_STAMP_PATH`, más el origen de migración `AURA_LEGACY_*`).

**`aura_sync_force_shared_db_path()`** (llamada desde `apps/main.c`, ambas variantes de `init()`, tras `settings_load()` y antes de `tagcache_init()`, comentario `aura (D-337)`, registrada en `MODIFICATIONS.md`): (1) `strmemccpy(global_settings.tagcache_db_path, AURA_SHARED_DB_DIR, …)` en cada arranque — no se confía en `config.cfg`, que en un firmware anterior trae `/.rockbox`; (2) migración: si `/.aura/tagcache/database_idx.tcd` no existe y `ROCKBOX_DIR/database_idx.tcd` sí → `mkdir` + `rename()` de todos los `database_*.tcd` (filtro puro `aura_cache_keys_is_tagcache_file()`: `database_` + `.tcd`; no toca `database.ignore` ni `database_commit.ignore`) y del sello `aura/db_stamp.txt`; si el compartido ya tiene base (la construyó otra familia, y el sello decide si está al día), los del árbol son peso muerto y se **borran**. `rename` dentro del mismo volumen FAT reescribe la entrada de directorio: sin copiar datos, los `.tcd` conservan su mtime.

**Sello.** `aura_sync_record_db_stamp()` escribe `/.aura/tagcache/db_stamp.txt` (creando el directorio si hace falta). `aura_sync_switch_needs_rebuild()` pierde su parámetro: compara solo el sello compartido con `/.aura/library-stamp` (lógica pura en `aura_cache_keys_stamp_needs_rebuild()`: sin sello o distinto → marcador); el arranque en frío (sin `library-stamp`) lo crea y sella la base compartida, que el saliente acaba de usar. `aura_music_db_ready()` sella en el bloque de "primera vez usable": `aura_sync_record_db_stamp()` si este arranque disparó el rebuild de bootstrap, o **`aura_sync_ensure_db_stamp()`** (solo si falta) para una base migrada de un firmware anterior a v15 — usable y sin marcador pendiente (`aura_sync_job_active()` ya devolvió antes), describe la biblioteca vigente; sin esto, la verificación "tras arrancar existe `db_stamp.txt`" fallaba con una base migrada, porque no hubo rebuild.

**Módulo puro nuevo** `aura_cache_keys.c/.h` (cabecera GPL, `apps/SOURCES`) con test host `test/test_cache_keys.c` (14 tests en total): filtro de archivos tagcache, decisión del sello y la clave de carátula de D-338.

**Verificado (simulador, `apple2026_sim_shot.sh` 600–900 ticks)**: con los `.tcd` en `simdisk/.rockbox` y sin `/.aura/tagcache` → tras arrancar están en `simdisk/.aura/tagcache/` con los **mismos mtimes** (rename) y `.rockbox` no tiene ninguno; `db_stamp.txt` existe y es byte-igual a `library-stamp` (`fw-20260826T104612-000000fd`); segundo arranque: `stat` de `database_idx.tcd` idéntico (sin rebuild). Con `simdisk/.rockbox/aura/db_stamp.txt` = `legacy-stamp-test` y la base de vuelta en `.rockbox` → migra a `/.aura/tagcache/db_stamp.txt` con ese contenido y desaparece del árbol. Borrando `/.aura/tagcache/*` → rebuild de bootstrap y el sello reaparece. Sim sin warnings nuevos; ARM `rockbox.ipod` en 0 errores (único warning, preexistente, `aura_style.c:89`).

**Hipótesis abiertas.** (a) Metro-Aura y moonlit.aura migran con la misma regla (M-095); un firmware anterior a v15 que arranque después de la migración no encontrará base en su árbol y reconstruirá la suya ahí — se queda como peso muerto hasta que un firmware v15 la borre. (b) El `config.cfg` de Rockbox persiste `database path` = `/.aura/tagcache` al guardar; un firmware Aura anterior a v15 que lea ese `config.cfg` usaría también el compartido — inocuo. (c) La base vieja del simdisk estaba desactualizada respecto al disco (pistas regeneradas después) y la pasada `Q_UPDATE` de cada arranque no la había refrescado — comportamiento preexistente de tagcache, fuera de esta pasada.

## D-338 — Claves estables de caché de carátulas (crc32 de ruta + mtime); GC de huérfanas en vez de vaciar `cfcache` (contrato v15)

**Hechos de partida.** `cfcache/<seek>-<size>.pfraw` (`aura_albumart.c`, `pfraw_path()`) usaba `album_seek`, que cambia en cada reconstrucción de la base; por eso `finish_ok()` de `aura_sync.c` borraba `CF_CACHE_DIR` entero y cada sync — y, con D-337, cada cambio de familia con sync de por medio — re-decodificaba todas las carátulas (precache D-224, "Preparando carátulas N/M"). Playlists (`pl-<nombre>-<size>`), artistas (`ar-<hash>-<size>`) y fotos ya usaban claves estables.

**Clave de álbum** = `a-<crc32(ruta de la pista representativa) 8 hex>-<mtime>-<size>.pfraw` (`aura_cache_keys_album_name()`/`_parse()`, módulo puro con test host). La pista representativa es la misma que ya usaba `find_any_track_in_album()` (la primera que `tagcache_search(tag_filename)` filtrada por `tag_album` devuelve — la misma con la que `find_albumart()` busca la carátula); `crc_32()` (`firmware/include/crc32.h`) sobre la ruta, y el mtime **tal como tagcache lo guardó** (`tagcache_get_numeric(&tcs, tag_mtime)`, mismo `tcs` de esa búsqueda: cero I/O extra, no hace falta `dir_get_info()` sobre `/Music`). El campo `theme` de la cabecera `.pfraw` se conserva (`aura_art_read_pfraw()`: el tema sigue siendo parte de la llave). `find_any_track_in_album()` gana el parámetro `key` (y artist/album opcionales); `aura_albumart_album_key()`, `aura_albumart_is_cached_key()` y `aura_albumart_gc_orphans()` son API nueva; `aura_albumart_is_cached()`/`load_for_album()` conservan su firma por `seek` (un lookup de tagcache más, en RAM — D-021 — por acierto de caché; en el fallo, `decode_album_art()` repite la búsqueda y devuelve la clave de la pista que de verdad usó).

**Sustituto del `clear_dir`.** `finish_ok()` ya no vacía `cfcache`: `aura_fsutil_clear_dir_except(CF_CACHE_DIR, aura_cache_keys_album_parse_name)` conserva solo las `a-*.pfraw` y sigue tirando `ar-*` (la foto de artista pudo cambiar en ese mismo sync — `aura_artist_art_load()` documentaba que dependía de este vaciado) y `pl-*` (ídem la portada de playlist). Las huérfanas de álbum las recoge **`aura_albumart_gc_orphans()`**, llamada desde `aura_music_precache_album_art()` **tras el pre-pase** (que ya calcula la clave de cada álbum una vez, en `s_precache_keys[]`, 16 KB estáticos) y **antes** de decodificar lo pendiente: borra las `a-*` cuya clave no está en el conjunto (álbumes que se fueron, pistas reescritas por un sync) y los `<seek>-<size>.pfraw` anteriores a esta decisión, con presupuesto `AURA_ALBUMART_GC_BUDGET` = 64 borrados por pasada (el resto cae en el siguiente arranque: el GC es mantenimiento, no un requisito de corrección — una huérfana solo ocupa disco). Elegido el precache y no `finish_ok()` porque es el único sitio que ya tiene el conjunto completo de claves vigentes sobre la base **nueva**; en `finish_ok()` la base acaba de cambiar y habría que recorrer tagcache otra vez. Corre en cada arranque con base usable (`aura_music_db_ready()`, una vez por arranque), aunque no haya nada pendiente.

**Verificado (simulador)**: tras arrancar, `ls simdisk/.rockbox/aura/cfcache/` muestra `a-53cc14bc-1787505971-130.pfraw` etc. (los 17 `<seek>-<size>.pfraw` viejos recogidos por el GC en la primera pasada); borrar `simdisk/.aura/tagcache/*` y reiniciar (rebuild completo) → `stat` (mtime y tamaño) de los seis `.pfraw` **idéntico** entre dos reconstrucciones consecutivas, ninguna entrada nueva: el precache no tuvo pendientes y la cápsula "Preparando carátulas" no se dibujó. Nota: en la primera reconstrucción el mtime de la clave sí cambió (`1787095275` → `1787505971`) porque la base vieja del simdisk era anterior a la regeneración de las pistas de prueba — el mtime real del disco es el nuevo, es decir, la clave siguió a la verdad del disco. 14 tests host verdes; sim sin warnings nuevos; ARM en 0.

**Hipótesis abiertas.** (a) Si Studio reescribe solo el `cover.jpg` de un álbum sin tocar sus pistas, la clave no cambia y la carátula vieja sobrevive hasta que cambie una pista (o el usuario fuerce reconstrucción, que tampoco la invalida ya): decidir si la clave debe incorporar también el mtime del archivo de carátula — costaría un `find_albumart()` + `dir_get_info()` por álbum en cada pre-pase. (b) El orden de `tagcache_search` para un álbum sigue el orden de índice (escaneo de directorios); si una reconstrucción lo alterara, cambiaría la pista representativa y con ella la clave de ese álbum — solo se re-decodifica ese álbum, nunca se sirve la carátula de otro. (c) `docs/contracts/library-layout-v1.md` (§1 y §4, copia idéntica en Studio) todavía describe `cfcache/` "indexado por `album_seek`" y "vaciado al terminar" — se actualiza en la pasada que lleve el contrato v15 a Studio, para que las dos copias sigan idénticas.

## D-339 — Caché negativa de carátulas (`a-<crc>-<mtime>.none`): los álbumes sin arte dejan de rearmar el precache

**Problema** (reportado primero en moonlit, idéntico en Aura). Un álbum sin carátula resoluble — sin `cover.jpg`/`folder.jpg` ni APIC/`covr` JPEG, o con un JPEG que `read_jpeg_file()`/`clip_jpeg_file()` rechaza — nunca produce `.pfraw`, así que el pre-pase de `aura_music_precache_album_art()` (`aura_albumart_is_cached_key()`) lo contaba "pendiente" en **cada** arranque, volvía a buscar y decodificar, y la cápsula "Preparando carátulas N/M" aparecía siempre en cualquier biblioteca con un álbum así. D-224 había dejado fuera la caché negativa a propósito (invalidarla exigía un enganche con Studio); con la clave estable de D-338 ese enganche ya existe: la clave lleva el `mtime` de la pista.

**Marcador.** `aura_albumart_load_for_album()` escribe `cfcache/a-<crc32 ruta pista 8 hex>-<mtime>.none` (0 bytes, `aura_cache_keys_album_none_name()`) cuando `decode_album_art()` concluye **definitivamente** que no hay arte: `find_albumart()` sin resultado y la pista legible sin JPEG embebido útil, o decodificador que rechaza el archivo que sí existe. **Sin lado** (`<size>`) a propósito: "no hay arte" es un hecho del álbum, no de un tamaño de tile — Music Flow (130) y CoverDrift (320) comparten el veredicto, un solo archivo por álbum (el encargo lo pedía con la clave completa; el sufijo de lado se omitió porque habría dado dos `.none` por álbum sin ganar nada — el mismo decodificador sobre el mismo archivo falla igual a cualquier lado). **No se cachea negativamente un fallo transitorio**: `open()` de la pista en error (disco ausente/ocupado, `-EBUSY`) devuelve `false` sin marcador (`*definitive == false`). Con el marcador presente: `load_for_album()` devuelve `false` tras el intento de `.pfraw` y antes de tocar `find_albumart()`/decodificador (el llamador — `get_slot_for()` de Music Flow, lista de álbumes, transiciones — cae a `aura_albumart_load_default()` como siempre, ningún camino nuevo); `is_cached()`/`is_cached_key()` lo cuentan resuelto (`aura_cache_keys_album_resolved()` = `.pfraw` válido O `.none`). Un `.pfraw` recién escrito borra el `.none` de su clave por si sobrevivió uno.

**Invalidación.** Misma clave que D-338: una pista reescrita por un sync (Studio no preserva fechas) cambia el `mtime`, el `.none` queda huérfano y el álbum se reintenta solo. `aura_albumart_gc_orphans()` pasa a decidir con `aura_cache_keys_album_is_orphan()` (puro, con test host), que cubre `a-*.pfraw`, `a-*.none` y los `<seek>-<lado>.pfraw` anteriores a D-338; `aura_cache_keys_album_parse_name()` (filtro de `finish_ok()`) conserva también los `.none` al reconstruir. `aura_cache_keys_album_parse()` sigue siendo solo-`.pfraw`; `_parse_any()` distingue ambos. `aura_albumart_key_t` es ahora alias de `aura_cache_album_key_t` (módulo puro). **Limitación documentada** (misma hipótesis abierta (a) de D-338): un `cover.jpg` nuevo sin tocar la pista no cambia la clave y el `.none` sobrevive hasta el siguiente re-sync que reescriba la pista o hasta que se decida incorporar el mtime de la carátula a la clave.

**Memoria del pre-pase.** `aura_music_precache_album_art()` lee el sello de la base compartida (`aura_sync_read_db_stamp()`, nuevo en `aura_sync.c/.h`, sobre `/.aura/tagcache/db_stamp.txt`) y, tras dejar todo resuelto, lo memoriza (`s_precache_memo_stamp`): una llamada posterior con el mismo sello no recorre tagcache. `aura_music_db_reset_triggers()` (llamada por `finish_ok()` de `aura_sync.c`) descarta la memoria. Hoy el bloque de "primera vez usable" de `aura_music_db_ready()` ya corre una sola vez por arranque, así que la memoria solo actúa cuando ese bloque se rearma; queda como puerta barata para cualquier llamada futura.

**Verificado.** `make -C firmware/rockbox/apps/aura/test test` — 14 suites verdes (`test_cache_keys` con 3 casos nuevos: nombre/parse del `.none`, "resuelto", GC sobre `.pfraw`+`.none`+nombres viejos). Simulador (`apple2026_sim_shot.sh`), fixtures temporales en `simdisk/Music/Aura QA/`: "Sin Arte Uno"/"Sin Arte Dos" (MP3 sin APIC ni `cover.jpg`) más 12 álbumes "Lento NN" con carátula de 1600 px para que el pre-pase dure más de un tick — con 7 álbumes el precache completo cabía en un solo tick y ningún volcado lo alcanzaba. Base reconstruida y `cfcache/` vacío → primer arranque: cápsula "Preparando carátulas 12/19" (`docs/screenshots/v0.4.2-beta-precache-first.png`, volcado en el tick 157) y `ls cfcache | grep -c '\.none$'` → **2** (`a-768d476f-1787768645.none`, `a-b541a77f-1787768645.none`). Segundo arranque, mismo tick 157 y 160: md5 de la captura **idéntico** al cuadro del menú raíz posterior al precache (`7e7b1445…`, capturado en los ticks 156/158 del primer arranque) — sin cápsula (`…-precache-second.png`, tick 300) — y `ls -lT cfcache` byte-igual antes y después (ningún archivo reescrito). Music Flow (`SELECT,SELECT,SCROLL_FWD×17`): "Sin Arte Dos" al centro con el tile por defecto y "Sin Arte Uno" a su lado, igual (`…-precache-flow-default.png`). Sim sin warnings nuevos; ARM `rockbox.ipod` en 0 errores. Fixtures retirados y base del simdisk restaurada al terminar (`simdisk/` no se versiona).

**Hipótesis abiertas.** (a) La de D-338 sobre `cover.jpg` reescrito sin tocar la pista, ahora con un efecto más visible (el tile default persiste, no solo la carátula vieja). (b) Un JPEG rechazado por falta de espacio en `s_decode_scratch` (no por estar corrupto) también deja `.none`; hoy el scratch cubre los 320 px de CoverDrift con margen ×2, así que no se conoce un caso real, pero un tamaño nuevo mayor debería revisar el scratch antes que este marcador.


## D-340 — Contrato v16: caché maestra compartida de imágenes bajo `/.aura/art/` (registro; implementación en D-341)

Canónico en `CONTRATO-firmware-studio.md` de este repo; Aura Studio y las hermanas reciben copia idéntica en su propia pasada. Encargo del dueño (2026-08-26): que las TRES familias compartan también la imagen ya DECODIFICADA, no solo la base tagcache y (Metro↔moonlit) las miniaturas de v15 — hoy cada familia decodifica el mismo JPEG por separado la primera vez que lo necesita, aunque el archivo fuente en disco sea idéntico para las tres. Cláusulas (detalle completo en la nueva §D.5 del contrato):

- **Nota v16** antes de la v15. **§D**: tres filas nuevas, `/.aura/art/{albums,artists,photos}/` — propiedad del firmware **activo**, compartidas entre familias, Studio nunca las toca (ni al instalar, ni al cambiar de familia, ni al sincronizar).
- **§D.5 (nueva)** — formato y claves: nombre `<a|r|p>-<crc32 8 hex>.<mtime>.art` (o `.none`, marcador negativo de 0 bytes), cabecera de 16 bytes LE (`magic` `"MAST"`, `width`, `height`, `flags=0`, `reservado=0`) + píxeles RGB565 LE fila-contigua; cuadrados 130×130 (álbumes y fotos de artista) u 80×80 (fotos) — los lados MÁS GRANDES que cualquier familia necesita hoy, cada una reduce por su cuenta al tamaño final que use. Clave = `(crc32(ruta del archivo fuente), mtime)`, el mismo par que ya usaba la caché privada de álbum de Aura desde D-338/v15, generalizado a fotos de artista y fotos de `/Photos` (antes sin clave formalizada en el contrato). Fill-and-center-crop de dos decodes cuando la fuente no es cuadrada (documentado con el mismo criterio que D-331/D-332 de Movie Flow); proporciones más extremas que 4:1 se centran sobre el color promedio, sin tema.
- **Derivación**: la maestra no lleva tema/esquinas/reflejo/transposición — cada familia los aplica al cargarla a RAM, exactamente como antes aplicaba esos pasos al resultado de su propio decode. Las cachés privadas de cada familia (`cfcache/*.pfraw` de Aura, `.mth` de Metro/moonlit) **no desaparecen**: pasan a ser un nivel L2 regenerable, derivado de la maestra, con su propia clave de siempre.
- **Constructor en segundo plano**: el contrato solo fija el comportamiento observable (cada firmware activo resuelve `.art`/`.none` de lo que le falte, sin pantalla, sin bloquear) — la implementación (hilo, o un paso por vuelta del bucle principal) es de cada firmware; el lado Aura-Firmware está en D-341.
- **GC**: cada firmware barre su propia `/.aura/art/<tipo>/` con la misma tabla de claves vivas que ya usa para el GC de su caché privada (D-338/D-339) — mismo criterio de huérfanas, mismo presupuesto por pasada.

Lo que v16 **no** cambia: §A bis, `/.aura/tagcache/`, `/.aura/thumbs/` (queda un nivel por encima de la maestra: Metro/moonlit lo siguen usando para SU propio formato final), el marcador v4, el `library-stamp` v12 ni el bootloader. De paso se corrigió `docs/contracts/library-layout-v1.md` (v1.4), que desde D-336 traía anotada como hipótesis abierta una nota de D-293 sobre `cfcache/` ya falsa desde D-338 (decía "se vacía por `album_seek`"; la clave es estable desde D-338 y el firmware ya no vacía el directorio, solo recoge huérfanas) — Studio necesita la copia idéntica de ambos documentos (`CONTRATO-firmware-studio.md` y `library-layout-v1.md`) en su propia pasada.

## D-341 — Constructor en segundo plano de la caché maestra (hilo de baja prioridad); retira la cápsula "Preparando carátulas" (D-224)

**Punto de partida de esta pasada.** Una sesión anterior (mismo encargo, D-340/D-341) dejó sin commitear el módulo de formato/IO de la maestra (`aura_master_art_format.c/.h`, `aura_master_art.c/.h`, test host `test_master_art.c`, 5/5 suites nuevas verdes) y ya había enganchado el consumo en `aura_albumart.c` (`aura_albumart_build_master()`/`aura_artist_art_build_master()`), `aura_artist_images.c` (`aura_artist_images_count()`/`_entry()`, mtime por índice), `aura_photos.c` (`aura_photos_dir()`/`_is_listable_name()`/`_build_master()`), retirado por completo la cápsula de D-224 (`aura_music_precache_album_art()`, `draw_precache_progress()`, `AURA_STR_PRECACHE_ART` de ambos `.lang`, `aura_music_take_redraw_request()`) y llamado desde los puntos correctos (`aura_music_db_ready()` → `aura_master_art_builder_start()`; `aura_music_db_reset_triggers()` → `_restart()`; `aura_main.c`/`aura_musicflow.c`/`aura_firmware_switch.c` → `_pause()`/`_suspend()`/`_resume()`) — **pero el archivo central, `aura_master_art_builder.c` (y su `.h`), no existía**, aunque ya estaba en `apps/SOURCES`. Esta pasada lo escribe, verifica lo demás y completa la documentación.

**Decisión hilo vs. plan B — con evidencia, no por default.** El encargo pedía verificar antes de comprometerse a un hilo real:

1. **`tagcache_search()`/`tagcache_get_next()`/`tagcache_retrieve()` desde un hilo que no es el de UI.** Dos evidencias directas, no solo inferencia:
   - **Rockbox ya corre su propio hilo de tagcache** (`tagcache_thread`, `apps/tagcache.c:5628` `create_thread(tagcache_thread, tagcache_stack, ..., PRIORITY_BACKGROUND, ...)`, llamado desde `tagcache_init()`) que hace commits/escaneos **mientras** el hilo de UI hace búsquedas — coordinados por `read_lock`/`write_lock` internos (`apps/tagcache.c:1784`, `tagcache_search()`: `while (read_lock) sleep(1);`, entra a esperar en vez de asumir que es el único llamador). Es el diseño de concurrencia con el que este tagcache YA se distribuye en todos los targets, no algo hipotético.
   - **Este mismo firmware ya llama a tagcache desde el hilo de audio/buffering**, no el de UI: `aura_music_buffer_event()` (`apps/aura/aura_music.c:204-218`, `tagcache_find_index()`/`tagcache_get_numeric()`/`tagcache_search_finish()`) registrada con `add_event(PLAYBACK_EVENT_TRACK_BUFFER, aura_music_buffer_event)` (`aura_music.c:318`) — dispara desde el hilo que compra los buffers de audio, un hilo real distinto del de UI, en producción, desde antes de esta pasada.
   - Se revisó también el patrón citado en el encargo, `apps/plugins/pictureflow/pictureflow.c`: su hilo de fondo (`create_pf_thread()`, `PRIORITY_BUFFERING`) solo decodifica bitmaps ya cacheados (`load_new_slide()`) — la pasada que SÍ llama a tagcache (`incremental_albumart_cache()`) corre en el hilo PRINCIPAL del plugin, un ítem por vuelta cuando `pf_state == pf_idle` (`pictureflow.c:4729`). Pictureflow no contradice que tagcache sea seguro entre hilos (usa el mismo tagcache que ya lo soporta) — solo eligió no hacerlo por su propia arquitectura de un solo hilo "principal". La evidencia de (a) y (b) de arriba es más fuerte que este precedente y decide a favor del hilo real.
2. **Estado global del decodificador JPEG.** `apps/recorder/jpeg_load.c:143`: `static struct jpeg jpeg;` existe **solo** bajo `#ifdef JPEG_FROM_MEM` — `grep -rln JPEG_FROM_MEM apps/` no encuentra ningún `-D` que lo defina en este árbol (es exclusivo de un camino de plugins que decodifican desde memoria, no usado por el build del core). El `apps/recorder/jpeg_load.c` que compila en `rockbox.ipod` es reentrante: todo el estado vive en el `struct bitmap`/buffer que pasa el llamador. Consecuencia práctica: el candado que ya dejó la sesión anterior en `aura_master_art.c` (`aura_master_art_decode_lock()`/`_unlock()`, mutex recursivo) no es por el decodificador en sí, sino porque el hilo de UI (Ahora Suena, Movie Flow, CoverDrift) y este constructor comparten el mismo `s_scratch` estático — cada uno con su propio bitmap de salida no haría falta ningún candado.

**Con las dos verificaciones a favor, se implementa el hilo** (`firmware/rockbox/apps/aura/aura_master_art_builder.c`, nuevo, `apps/SOURCES` ya lo listaba): `create_thread(builder_thread, s_builder_stack, sizeof(s_builder_stack), 0, "aura master art", PRIORITY_BACKGROUND, CPU)`, mismo patrón exacto de llamada que `tagcache_thread` (`apps/tagcache.c:5628-5630`); pila propia `DEFAULT_STACK_SIZE + 0x4000` (mismo tamaño que `tagcache_stack`, nunca la pila de UI de 8 KB, D-226); buffers de trabajo propios y estáticos (`s_flat` 130×130, `s_live_keys`/`s_album_items` con el mismo cupo `AURA_MUSIC_MAX_ITEMS` que usaba el precache retirado).

**Diseño del hilo.** Recorre álbumes (`aura_music_browse(AURA_SCREEN_MUSIC_ALBUMS, ...)` + `aura_albumart_build_master()`) → fotos de artista (`aura_artist_images_count()`/`_entry()` + `aura_artist_art_build_master()`) → fotos (`opendir(aura_photos_dir())` propio, filtrado con `aura_photos_is_listable_name()`, + `aura_photos_build_master()`), con `sleep(HZ/20)` entre elementos. `aura_master_art_builder_pause(bool)` es una bandera ligera (nunca mata el hilo): el elemento en curso siempre termina; la usan `aura_main.c` (misma puerta de cadencia fina de animación que ya gateaba el resto de `lcd_active()`) y `aura_musicflow.c` (al iniciar cada scroll). El hilo **también se detiene solo** mientras `audio_status() & AUDIO_STATUS_PLAY` — no existe una bandera pública de "cargando buffer" en `audio.h` (solo `PLAY/PAUSE/RECORD/PRERECORD/ERROR/WARNING`); `AUDIO_STATUS_PLAY` es el proxy más cercano y el más conservador disponible (se detiene durante toda la reproducción activa, no solo el instante exacto del relleno) — elegido así, sin bandera dedicada, por el historial real de freezes por contención de disco/CPU con este hilo (D-204/D-206/D-214) que motivó que D-224 fuera síncrono en su momento. `aura_master_art_builder_suspend()` (USB, apagado, reinicio, cambio de firmware) fija `s_stop_requested` y hace `thread_wait()`: el elemento en curso termina (nunca queda a medias en `/.aura/art` ni con una búsqueda de tagcache abierta) y el hilo sale; `resume()` lo vuelve a crear, continuando donde quedó. `aura_master_art_builder_restart()` (tras `finish_ok()` de `aura_sync.c`, base recién reconstruida) aborta la fase en curso y reinicia desde álbumes. El GC (`aura_albumart_gc_orphans()` para álbumes — que ya barre `/.aura/art/albums` por dentro, D-338/D-339 — y `aura_master_art_gc(ARTIST|PHOTO, ...)` directo para las otras dos) solo corre al completar una fase ENTERA: una pasada abortada a medias (pausa/reinicio) nunca ve su lista parcial de claves vivas como si fuera la lista completa, para no borrar una maestra viva por error.

**Verificado.**
- `make -C firmware/rockbox/apps/aura/test test` — **15/15 suites verdes**, incluida `test_master_art` (5 casos: tamaños, nombres/parse, huérfanas, cabecera, reducción por caja/recorte/relleno).
- Simulador (`apple2026_sim_shot.sh`, fixtures reales `simdisk/Music/Aura QA/` y `simdisk/Photos/`, más un índice `artist_images.cfg` de una entrada agregado para esta verificación): con `cfcache/` y `/.aura/art/` vacíos, arranque sin entrar a Música ni a Fotos → `/.aura/art/{albums,artists,photos}/` se llena solo (19 álbumes: 17 `.art` + 2 `.none`, coincide con los dos "sin arte" de D-339; artista de prueba `.art`; fotos según ticks disponibles). `python3` confirma cabecera `MAST`, 130×130 en una maestra de álbum y de artista, 80×80 en una de foto, tamaño de archivo exacto `16 + lado²×2`. Segundo arranque (900 ticks adicionales sin tocar nada): `stat` (tamaño y mtime) de **todo** `/.aura/art/**` byte-idéntico al primer arranque — cero reescrituras — confirmado también contando líneas `aura_master_art: decode`/`fill decode` en la consola de depuración: **27** en el arranque en frío, **0** en el segundo. Music Flow (`SELECT,SELECT`) muestra carátulas reales ya derivadas de la maestra, no el tile por defecto, sin ningún decode nuevo. GC probado a mano: un `.art`/`.none` huérfano plantado en `/.aura/art/albums` y en `/.aura/art/artists` desaparece en la siguiente pasada completa; el de `/.aura/art/photos` tardó más en confirmarse por el mismo motivo del párrafo de abajo (fotos reales grandes retrasando la pasada completa).
- ARM (`firmware/build-ipod6g`, `make -j4`): **0 errores**. `.bss` **antes** (build previo a esta pasada, aún sin `aura_master_art_builder.c`): `text=1279752 data=12468 bss=11338876`. **Después**: `text=1286088 data=12468 bss=11235676` — **`.bss` bajó ~103 KB** pese al hilo nuevo: la sesión anterior ya había consolidado en `aura_master_art.c` los DOS scratch de decodificación de 400 KB de `aura_albumart.c` (`s_decode_scratch`+`s_transpose_scratch`, dimensionados para CoverDrift a 320 px) en uno solo compartido, y esta pasada retiró los arreglos estáticos del precache (`s_precache_albums`/`s_precache_keys`, ~150 KB) — ambos ahorros superan lo que suma el hilo nuevo (pila + `s_flat` + `s_live_keys` + `s_album_items`, ~210 KB). Se corrigió de paso un warning nuevo `-Wcomment` en `aura_master_art.h` (un `/*` dentro de un comentario de bloque); el único warning restante (`aura_lang.c:583`, `-Wtype-limits` en `aura_str()`) es preexistente y ajeno a esta pasada (función sin tocar desde D-333, confirmado con `git log`).
- `git status --short` limpio tras los commits de esta pasada.

**Hipótesis abiertas.** (a) La fase de fotos avanza mucho más lento de lo que sugiere `sleep(HZ/20)` nominal cuando la biblioteca tiene fotos reales grandes (`Foto Real 3.jpg`, 3648×2736, ~10 MP, dentro del tope `PHOTO_MAX_PIXELS` de `aura_photos.c` y por tanto SÍ se decodifica): en el simulador, una pasada completa de 16 fotos con dos o tres así de grandes tomó más de un minuto de reloj real con uso de CPU bajo (~7%), sugiriendo que el tiempo lo domina algo distinto al decode puro (¿el propio candado compartido, el costo real del IDCT emulado, o el pacing del simulador SDL) — no se aisló la causa exacta por el límite de tiempo de esta pasada. No bloquea al usuario (corre en segundo plano) pero conviene perfilarlo en hardware real antes de dar por buena la cadencia para bibliotecas de fotos grandes. (b) El proxy `AUDIO_STATUS_PLAY` para "el hilo de audio quiere el disco" es deliberadamente conservador (para toda la reproducción, no solo el relleno real); si en el futuro se expone una bandera más fina en `audio.h`/`buffering.h`, conviene angostar la puerta. (c) `/.aura/thumbs/` (M-096, Metro/moonlit) no se tocó ni se fusionó con la maestra nueva — quedan como dos niveles de caché coexistiendo del lado Metro/moonlit (maestra 130/80 sin tema → `.mth` 80 px con su propio formato); fusionar ambos queda fuera de esta pasada (es trabajo de esos repos hermanos, no de Aura-Firmware).

## D-342 — Esperar un commit de tagcache en vuelo antes de reiniciar por un cambio de familia

**Diagnóstico.** `aura_firmware_switch_to()` (`apps/aura/aura_firmware_switch.c:76-117`) llama `tagcache_shutdown()` (`apps/tagcache.c:5508-5517`) y luego renombra los árboles y reinicia. `tagcache_shutdown()` solo vacía la cola de comandos (`run_command_queue(true)`) — **nunca espera a que un `commit()` en curso termine**. Si en ese instante `tagcache_rebuild()`/`tagcache_start_scan()` (disparados por `aura_music_db_ready()`) están a mitad de `commit()` (`apps/tagcache.c:3538-3593`), el header maestro **compartido** (`/.aura/tagcache/database_idx.tcd`, D-337) se marca `dirty = true` al empezar (`:3539-3540`) y solo se limpia al terminar bien (`:3593`); un reinicio en medio deja ese `dirty` trabado. El siguiente arranque de **cualquier** familia (la base es compartida) ve `check_all_headers()` → `false` ("tagcache is dirty!", `apps/tagcache.c:716-720`) → `tc_stat.ready = false` → `aura_music_db_ready()` dispara `tagcache_rebuild()` completo sobre una base que en realidad estaba íntegra. La API pública `tagcache_get_commit_step()` (`apps/tagcache.c:5669-5672`, expone `tc_stat.commit_step`: `0` = inactivo/sin escritura de índice en curso, `1..TAG_COUNT` = escribiendo) ya existe y basta para detectar el caso — Rockbox mismo la usa así en `tagcache_prepare_shutdown()` (`apps/tagcache.c:5496-5506`), que **rechaza** el apagado si `commit_step > 0` en vez de esperarlo; ese camino corre desde `tree_flush()` (`apps/tree.c:1345`, apagado/USB del árbol Rockbox base, fuera de `apps/aura/`) y queda fuera de alcance de esta decisión (grep `tagcache_shutdown` en todo `apps/aura/` solo encontró la llamada de `aura_firmware_switch.c`).

**Corrección.** `aura_firmware_switch_to()` espera, con tope, antes de `tagcache_shutdown()`: bucle acotado a `AURA_SWITCH_COMMIT_WAIT_TICKS = HZ * 8` (8 s) que sondea `tagcache_get_commit_step()` cada `HZ/10` mientras siga distinto de 0 y no se haya cumplido el tope. Si el tope se agota con un commit todavía en curso, el switch **procede de todas formas** (no bloquear indefinidamente un cambio de familia por un caso raro) — mismo criterio de "mejor esforzarse que colgar" que ya usa `aura_master_art_builder_suspend()` en la línea anterior. Sin pantalla de por medio: no existe hoy una pantalla de este switch a la que engancharle un texto de progreso (el flujo es confirmación Sí/No → reinicio directo), así que un `sleep` silencioso es aceptable — el caso (reinicio justo durante un commit) es raro en la práctica.

**Testeable en host.** La condición "¿debo seguir esperando?" se separó a un módulo puro nuevo, `apps/aura/aura_switch_wait.c`/`.h` (`aura_switch_wait_for_commit(commit_step, now, deadline)`), libre de `kernel.h` (sin `current_tick`/`HZ`/`sleep` reales) para poder compilarse y probarse en host; compara `now`/`deadline` con el mismo criterio con signo que `TIME_BEFORE()` (`system.h:90-91`), incluyendo el wraparound de `current_tick`. `aura_firmware_switch_to()` solo aporta los valores reales (`tagcache_get_commit_step()`, `current_tick`, el `deadline` calculado) y el `sleep(HZ/10)` entre sondeos — el resto de la función (renombrados, `system_reboot()`) sigue sin ser testeable en host por hacer I/O y reinicio reales, igual que el resto de este archivo.

**Verificado.**
- `make -C firmware/rockbox/apps/aura/test test` — **16/16 suites verdes**, incluida `test_switch_wait` nueva (3 casos: inactivo nunca espera pase lo que pase con los ticks, en curso espera hasta cumplir el tope exacto, comparación con signo estable cerca del wraparound de `current_tick`).
- Build ARM (`firmware/build-ipod6g`, `make -j4`): **0 errores, sin warnings nuevos** (solo se compilaron `aura_firmware_switch.o` y el `aura_switch_wait.o` nuevo; el resto del árbol no cambió). `aura_switch_wait.c` se agregó a `apps/SOURCES` junto a `aura_firmware_switch.c`.
- Simulador (`build-sim`, `make -j4` limpio sin warnings nuevos; `make install`): arranque normal verificado con `apple2026_sim_shot.sh` (menú raíz renderiza igual que antes, sin regresión) y además con `rockboxui` lanzado **interactivo** (proceso real en primer plano, confirmado vía System Events — no solo el modo headless con auto-dump) para cumplir la verificación de fase habitual con simulador interactivo, no solo capturas.
- **Límite documentado de esta verificación**: no se reprodujo en vivo el caso "reinicio justo a mitad de un commit" navegando la UI real del switch de familia. Ese camino (`AURA_SCREEN_SETTINGS_SWITCH_TO_METRO`/`_MOONLIT` en `aura_screens.c:3952-3997`) exige `aura_firmware_sibling_installed()` — un `/.firmware-<familia>` real instalado en el disco — y este checkout/simulador no tiene una segunda familia instalada; forzarlo habría significado fabricar un árbol hermano falso y arriesgar el estado de `simdisk/` (el propio switch renombra `/.rockbox`) sin ganar certeza adicional sobre la lógica en sí, ya cubierta exhaustivamente por `test_switch_wait` (incluida la frontera exacta `commit_step != 0` + tope cumplido). Reproducción end-to-end en hardware con dos familias instaladas queda a cargo del dueño, mismo criterio que otras decisiones (D-307/D-309, D-320) para lo que exige un montaje que este entorno no tiene.
- `git status --short` limpio tras el commit de esta pasada.

## D-343 — `*PANIC* stkov main`: los dos `struct mp3entry` del sondeo de carátula salen de la pila de UI

**Diagnóstico.** Reporte del dueño en hardware, build `e554e31cffM-260826` (D-339): `*PANIC* stkov main`, `pc: 080a8ce4`, `sp: 00007fd8`. `stkov` lo lanza `thread_stkov()` (`firmware/kernel/thread.c:200`) cuando `thread->stack[0] != DEADBEEF` en `switch_thread()` (`:1029`) — el canario del fondo de la pila pisado. El hilo es `main`, el de la UI, cuya pila mide exactamente **8 KB** en este target (`rockbox.map`: `stackbegin = 0x7d30`, `stackend = 0x9d30`). El `sp` reportado da 0x9d30 − 0x7fd8 = **7 512 de 8 192 bytes ya consumidos** en el punto de cesión.

La causa está en `apps/aura/aura_albumart.c`. `decode_album_master()` y `decode_album_at()` declaraban cada una **`struct mp3entry fake_id3` en la pila**, más `path[MAX_PATH]` + `artist[128]` + `album[128]` + `art_path[MAX_PATH]`. `struct mp3entry` mide ~2.7 KB en este target (`ID3V2_BUF_SIZE` 1800 para `MEMORYSIZE >= 64`, más `path[MAX_PATH]` y `id3v1buf[4][92]`, `lib/rbcodec/metadata/metadata.h:181-287`). Marcos resultantes medidos sobre `rockbox.elf`: **3 592** y **3 616 bytes**.

Medición del peor camino de la UI (desensamblado de `rockbox.elf` con `arm-none-eabi-objdump -d`, gasto por prólogo `push`/`sub sp` propagado por el grafo de llamadas desde `aura_main`):

```
   1272  draw_choice_list        →  568  decode_album_drift_tile
    304  aura_albumart_load_for_album
   3592  decode_album_master      ←  el marco culpable
     88  aura_master_art_decode_fill  →  read_bmp_file
   2624  read_bmp_fd             ←  Rockbox base, inevitable sin tocar el core
    ...  read → fat_readwrite → ata_read_sectors → yield
  ------
 ~10 200 bytes contra una pila de 8 192
```

El subárbol desde `aura_albumart_load_for_album()` solo daba ya **7 616 bytes**, que es prácticamente el 7 512 observado: en `e554e31cff` ese camino corría en el hilo de UI a través del precache de D-224, con la pantalla "Preparando carátulas" delante.

**El bug sobrevivió a D-341.** Mover el constructor a su propio hilo con pila propia (`s_builder_stack`, `DEFAULT_STACK_SIZE + 0x4000`) quitó *una* de las dos formas de llegar, no el marco. `aura_albumart_load_for_album()` sigue diciendo "Sin maestra todavía (el constructor no llegó), se construye aquí mismo" (`aura_albumart.c`, rama `out->size <= AURA_MASTER_ART_ALBUM_SIZE`) y sigue entrando desde la UI. Metro y moonlit **no** tienen este defecto: su `metro_albumart.c` ya trae el comentario "`struct mp3entry` is ~1.5-2KB … putting on the stack" y usa `static` desde siempre — Aura era la única que quedó con la versión en pila.

**Corrección.** Un solo bloque de archivo en `aura_albumart.c`, compartido por las dos funciones (nunca están vivas a la vez: la segunda se llama después de que la primera regresó):

```c
static struct mp3entry s_probe_id3;
static struct mp3entry s_fake_id3;
static char s_art_path[MAX_PATH];
```

Sustituye a los dos `s_probe_id3` locales que ya existían, a los dos `fake_id3` de pila y a los dos `art_path`. La disciplina de candado es la parte delicada y se resolvió distinto en cada función:

- `decode_album_master()` entra desde **dos hilos** (la UI por `aura_albumart_load_for_album()`, el constructor por `aura_albumart_build_master()`). Tomaba `aura_master_art_decode_lock()` *después* de llenar `fake_id3`, así que la inicialización **baja debajo del candado**: llenarla fuera sería una carrera entre los dos hilos. `path`/`artist`/`album` se quedan en la pila (516 B en total) porque los llena `find_any_track_in_album()` antes del candado, y ensancharlo sobre esa búsqueda de tagcache habría serializado UI y constructor sin necesidad.
- `decode_album_at()` tiene un solo llamador (`aura_albumart_load_for_album()`, rama de lado mayor que la maestra) que **ya toma el candado antes de entrar**, así que ahí todo sale de la pila sin mover nada de sitio, `path`/`artist`/`album` incluidos.

Costo: **+736 bytes de BSS** (`RAM usage` 12 501 272 → 12 502 008); se recuperó más de lo que se gastó al fusionar los duplicados.

**Verificado.**
- Marcos tras el cambio, misma medición sobre el `rockbox.elf` nuevo: `decode_album_master` **3 592 → 568 bytes**; `decode_album_at` desaparece del binario (el compilador lo integra al quedarse sin marco propio). El subárbol completo desde `aura_albumart_load_for_album()` baja de **7 616 → 4 960 bytes**, y el camino de carátulas deja de ser el peor del hilo de UI.
- `make -C firmware/rockbox/apps/aura/test test` — **16/16 suites verdes**, 0 fallos.
- Build ARM (`firmware/build-ipod6g`, `make -j$(sysctl -n hw.ncpu)`): **0 errores, sin warnings nuevos**; solo recompiló `aura_albumart.o`.
- Simulador (`build-sim`, build limpio + `make install`): se **vació** `/.aura/art/albums` y `artists` y el `cfcache` L2 para obligar al camino modificado a decodificar los JPEG de cero. Las **19 maestras de álbum se reconstruyeron y son idénticas byte a byte** a las que había antes del cambio (`diff -rq`) — misma entrada, misma salida, sin crash. Music Flow renderiza las carátulas reales con su reflejo (captura). Además se dejó `rockboxui` corriendo **interactivo** (proceso real en primer plano, confirmado vía System Events), no solo el modo headless con auto-dump. La única maestra de artista del respaldo no se regeneró por una razón ajena: `make install` rehace `.rockbox/` desde el build y `/.rockbox/aura/artists/` solo lo puebla un sync de Studio, así que el simdisk se quedó sin el archivo fuente.
- **Límite documentado de esta verificación**: no se reprodujo el `stkov` original en hardware antes y después — habría que volver a `e554e31cff`, flashear y navegar hasta reventarlo. La evidencia es la aritmética de pila sobre el binario real (marco medido, camino medido, `sp` reportado que cuadra con el cálculo) más la prueba de equivalencia de salida en el simulador. Confirmación en el iPod queda a cargo del dueño, mismo criterio que D-342.
- **Hallazgo colateral, fuera de alcance**: con el camino de carátulas ya corregido, la misma medición reporta otro camino de ~9 568 bytes desde `aura_main`, todo él en Rockbox base (`catalog_insert_into` → `gui_syncyesno_run` → `default_event_handler` → `gui_usb_screen_run` → `settings_apply` → motor de skins → `skin_data_load`). No se tocó: es código heredado, algunas de sus aristas pueden ser falsas (el análisis sobreaproxima, no resuelve llamadas indirectas) y no corresponde al panic reportado. Queda anotado para revisarlo por separado.

## D-344 — "Actualizar biblioteca" prepara también las carátulas; la pasada esperaba a una base que todavía no era consultable

**Encargo del dueño (2026-08-27).** Una opción de Ajustes llamada **"Actualizar Biblioteca"** en las tres familias, con aviso de que puede tardar varios minutos según los archivos y el estado del disco, y que la preparación ocurra **solo** en tres momentos: tras actualizar el firmware, tras un sync de Studio, o al pedirla a mano. Motivo de fondo: *"en el music flow de moonlit se tarda un poco en cargarse las imágenes de los albums […] que al cambiar de sistema ya no haya más cargas de espera."*

**Lo que ya existía y no hacía falta construir.** La caché compartida del contrato v15/v16 ya cumple la mitad del pedido: `/.aura/tagcache/` y `/.aura/art/{albums,artists,photos}/` viven **fuera** de `/.rockbox/` y las tres familias usan la misma clave (`crc_32(ruta, 0xffffffff)` + mtime) y los mismos lados (130/130/80), verificado leyendo las tres. Lo que faltaba no era compartir: era **llenarla del todo en un momento definido**. D-341 quitó la cápsula bloqueante "Preparando carátulas" y dejó solo el constructor en segundo plano, así que la primera visita a Music Flow vuelve a mostrar marcadores de posición mientras el hilo camina — exactamente la espera reportada. En Aura la fila ya existía como "Reconstruir biblioteca" (D-293); se **renombra** a "Actualizar biblioteca" para que las tres familias digan lo mismo, y el texto de `AURA_STR_LIBRARY_ERROR_ATTEMPTS` que la citaba se actualizó con ella.

**Decisión.** Terminado el trabajo de tagcache, `aura_sync.c` entra en un estado nuevo, `AURA_SYNC_BUILDING_ART`, que **termina la pasada completa de la caché maestra antes de devolver el control**, con su progreso en la misma pantalla "Actualizando biblioteca…" (no una pantalla nueva: la excepción documentada de D-293 sigue siendo una sola). Solo cuando la música estuvo en juego — un marcador de solo Videos/Fotos no toca álbumes ni fotos de artista y esperar una pasada entera por eso sería gratis. Posponible con Menú como el resto de esa pantalla: el constructor no se cancela, vuelve a su cadencia de fondo y termina solo.

Esto **no** reabre lo que D-341 cerró. Lo que ahí bloqueaba era una cápsula que aparecía sola, sin que nadie la pidiera; aquí el usuario pidió la preparación y se le advirtió cuánto tarda. La diferencia es el consentimiento, no el mecanismo.

`aura_master_art_builder` gana lo mínimo para eso: `progress()` (fase + hechos/total; total 0 en fotos, cuyo recorrido es en streaming), `pass_done()` (solo tras las tres fases sin cortes — una pasada abortada por stop/restart nunca cuenta), `is_running()`, `set_foreground()` (sustituye la espera de `HZ/20` por un `yield()` e ignora la pausa de animación: no hay carrusel con el que competir; el audio se sigue respetando) y `begin_full_pass()`.

**Los dos bugs que aparecieron al verificar, y que eran el fondo del asunto:**

1. **La pantalla no avanzaba ni se cerraba nunca.** `apps/aura/aura_main.c:495` llama `aura_sync_tick()` **solo** si `aura_sync_job_active()`, y ese predicado no incluía el estado nuevo. Se agregó. Los otros tres llamadores lo quieren igual: no releer el marcador debajo del trabajo (`check_pending`), no disparar un rebuild encima (`aura_music_db_ready`) y no encolar otro manual mientras este se muestra.

2. **La pasada de imágenes no preparaba NADA, y lo hacía en silencio.** Con trazas en el simulador: al entrar al estado nuevo, `aura_music_browse()` devolvía **0 álbumes** — la base acababa de comitirse y todavía no era consultable. La fase de álbumes salía en el acto, la de artistas también, la de fotos corría (16 fotos, no necesita base) y la pasada se daba por **completa** en el primer tick. La pantalla se cerraba como si hubiera terminado y el usuario se quedaba exactamente con la espera que esta decisión viene a quitar. Corregido: `AURA_SYNC_BUILDING_ART` espera a `tagcache_is_usable()` antes de arrancar la pasada, con tope de `HZ * 10` para no dejar a nadie atrapado si la base no se anuncia nunca. Tras el arreglo, la misma traza da `count=19`.

**Bug colateral corregido: el envoltorio de texto rompía el español.** `aura_widgets_wrap_text()` medía `lcd_getstringsize()` sobre **prefijos de bytes**, así que al llegar al primer byte de una letra acentuada el buffer terminaba a mitad de secuencia UTF-8; el ancho de esa secuencia rota se dispara y la línea se cortaba antes de tiempo. Visible en cuanto el aviso creció: *"…y prepara las"* / *"carátulas…"* con media línea vacía, y el cuerpo pasándose de `CONFIRM_MAX_LINES` (4) y truncándose a mitad de frase. Afectaba a **todo** cuerpo de aviso y a Avisos legales, no solo a esta pantalla — en un firmware cuyo idioma primario es el español no es un detalle. Ahora avanza por caracteres UTF-8 completos (`utf8_seq_len()`, con guarda para una secuencia truncada al final del texto). Con eso el aviso entra completo en tres líneas.

**Verificado.**
- Build ARM (`firmware/build-ipod6g`): **0 errores**. Un único warning, `aura_lang.c:589` (`id < 0` sobre un enum sin signo), **preexistente** y ajeno a esta pasada — solo se hizo visible porque ahora `aura_lang.c` recompila; no se tocó.
- `make -C firmware/rockbox/apps/aura/test test` — **16/16 suites verdes**, 0 fallos.
- Simulador (build limpio + `make install`), recorrido real con inyección de botones: Ajustes muestra la fila **"Actualizar biblioteca"**; el aviso Sí/No se lee **completo en tres líneas** (captura); al aceptar corre la base ("Leyendo… 9 668 elementos leídos"), sigue la fase de imágenes con **"Preparando carátulas 2/19"** y la barra llenándose (captura), y la pantalla **se cierra sola** devolviendo a Ajustes. Con la caché maestra vaciada a mano antes de cada corrida, quedaron **19 maestras de álbum y 16 de foto** escritas.
- **Nota sobre esa captura del progreso**: la biblioteca QA del simulador tiene 19 álbumes y la fase se resuelve en menos de un segundo, así que el texto es imposible de atrapar con el volcado automático. La captura con "2/19" se tomó en una corrida con la espera entre elementos subida a `HZ/3` **solo para poder observarla**; se revirtió enseguida y la verificación final corrió con el código tal como queda. El dibujo es el mismo en ambos casos.
- **Límite documentado**: no se probó en hardware con una biblioteca real de miles de pistas, que es donde la fase de imágenes de verdad tarda minutos y donde el aviso cobra sentido. Tampoco se verificó el disparo por marcador de un sync de Studio real (exige una Mac con Studio y un iPod), solo el manual de Ajustes — los dos entran por el mismo `finish_ok()`, que es el punto que se modificó.

## D-345 — Línea base de la ronda "estabilidad e imágenes": simbolización del `*PANIC* stkov main` del dueño y medición de marcos sobre el binario real

**Qué es esta decisión.** No cambia código. Fija la línea base contra la que se
mide toda la ronda (plan maestro `PLAN-ronda-3-firmwares-maestro.md` §0 y §E):
qué reportó el dueño, contra qué binario está simbolizado, cuánto mide de
verdad cada marco del hilo de UI hoy, y cuál de las hipótesis heredadas no
sobrevivió a la medición.

**Punto de partida del árbol.** El trabajo sin commitear de la sesión anterior
compila en 0 errores (target y simulador) y las 16 suites de host quedan
verdes, así que entra al historial antes de tocar nada: `b9f0c4e2` (D-343),
`948043a7` (D-344) y `6d0cf6a8` (contrato v17 / ST-077, que estaba en el mismo
árbol y no pertenece a ninguna de las dos). La cadena de versión de la ronda
nace de ahí.

**El panic del dueño.** `*PANIC* stkov main`, `pc 0x080aa56c`, `sp 0x00007b20`,
binario `fdf5be4e8fM-260827`. Simbolizado por la sesión supervisora y
reverificado aquí contra `firmware/build-ipod6g/rockbox.elf`:

- `0x080aa56c` cae dentro de `queue_empty` (`0x080aa568`..`0x080aa580`,
  `addr2line` lo confirma). **No es el culpable**: `stkov` lo lanza
  `thread_stkov()` desde `switch_thread()` cuando encuentra el canario del
  fondo de la pila pisado, así que el `pc` es simplemente dónde el hilo cedió,
  no dónde se desbordó.
- La pila del hilo `main` sigue midiendo **8 KB en IRAM**: `rockbox.map` da
  `stackbegin = 0x7d30`, `stackend = 0x9d30`, `_fiqstackend = 0xa530`.
- `sp = 0x7b20` está `0x7d30 − 0x7b20 = 0x210` = **528 bytes por debajo de
  `stackbegin`**. Desbordamiento real y más profundo que el de D-343 (que
  reportaba 7 512 de 8 192 usados, todavía dentro).

**Marcos medidos (bytes, `objdump -d` sobre el binario de esta línea base,
prólogo `push` + `sub sp`).** Las 30 mayores del binario, con las de
`apps/aura/` marcadas:

| Bytes | Función | Origen |
|---|---|---|
| 6 424 | `build_index` | tagcache |
| 4 408 | `aura_style_scan` | **aura** |
| 4 120 | `ata_get_phys_sector_mult` | Rockbox |
| 4 112 | `dbg_bootflash_dump` | Rockbox (menú de depuración) |
| 3 824 | `try_activate` | **aura** |
| 3 328 | `iap_handlepkt_mode4` | Rockbox |
| 3 320 | `iap_platform_get_indexed_track_info` | Rockbox |
| 3 216 | `add_tagcache` | tagcache |
| 3 056 | `iap_handlepkt_mode3` | Rockbox |
| 2 624 | `read_bmp_fd` | Rockbox |
| 2 120 | `tagcache_import_changelog` | tagcache |
| 2 088 | `glyph_cache_load` | Rockbox (fuentes) |
| 1 864 | `parse_list_chunk` | Rockbox |
| 1 648 | `retrieve_entries` | tagcache |
| 1 392 | `run_search` | **aura** |
| 1 336 | `fix_huff_tbl` | Rockbox (JPEG) |
| 1 280 | `search_playlist` | Rockbox |
| 1 272 | `count_unique_tag` | tagcache |
| 1 272 | `draw_choice_list` | **aura** |
| 1 160 | `skin_render_viewport` | Rockbox (skins) |
| 1 152 | `skin_render_playlistviewer` | Rockbox (skins) |
| 1 112 | `draw_nav_list` | **aura** |
| 1 104 | `build_playlist_from_songs` | Rockbox |
| 1 104 | `draw_style_list` | **aura** |
| 1 080 | `relocate_tagcache_files` | tagcache |
| 1 064 | `catalog_insert_into` | Rockbox |
| 1 064 | `insert_all_playlist` | Rockbox |
| 1 048 | `parse_menu` | Rockbox |
| 1 032 | `write_marker` | **aura** |
| 1 016 | `import_ratings_from_studio` | **aura** |

Otras que interesan al análisis, fuera del top 30: `aura_music_play_track` 992,
`skin_data_load` 848, `load_manifest` 424, `aura_style_read_icon_bmp` 296,
`settings_apply` 280, `gui_usb_screen_run` 200, `aura_main` 56.

**Desviación respecto al plan maestro: `style_fonts_exist` no existe como marco
propio.** El maestro la lista con 3 640 B y arma con ella el camino sospechoso
#1 (`draw_style_list` → `aura_style_scan` → `style_fonts_exist` ≈ 9.2 KB). En
este binario gcc la **integra** en su único llamador, `aura_style_scan`: sus
`paths[14][MAX_PATH]` (3 640 B) son la mayor parte de los 4 408 B de
`aura_style_scan`, y no hay símbolo `style_fonts_exist` en el `.elf`. Sumar
ambas cifras cuenta la misma memoria dos veces. El camino #1 real es
`draw_style_list` (1 104) + `aura_style_scan` (4 408) + `load_manifest` (424,
este sí es un símbolo aparte) ≈ **5.9 KB**, no 9.2 KB. Sigue siendo el peor
camino de UI que sale de `apps/aura/` y sigue justificando el trabajo de la
Fase 1; lo que cambia es que la cuenta de "antes/después" de esa fase parte de
5.9 KB.

Los otros dos caminos sospechosos se conservan tal como los describe el
maestro y se miden en la Fase 1 con la herramienta de §E.3, que resuelve el
grafo de llamadas en vez de sumar a mano:

- **#2, activar un tema**: `try_activate` (3 824) + `font_load` →
  `glyph_cache_load` (2 088) + `read_bmp_fd` (2 624) por cada ícono del tema.
- **#3, el hallazgo colateral de D-343**, todo en Rockbox base:
  `default_event_handler` → `gui_usb_screen_run` (200) → `settings_apply` (280)
  → motor de skins → `skin_data_load` (848), que en la medición de D-343 sumaba
  ~9 568 B con sus hojas.

**Verificado.**
- `make -C firmware/build-ipod6g`: **0 errores**. `Version: fdf5be4e8fM-260904`,
  `Binary size: 1299688`, `RAM usage: 12503096`.
- `firmware/tools/build_sim.sh` (compila + `make install` al simdisk):
  **0 errores**.
- `make -C firmware/rockbox/apps/aura/test test`: **16/16 suites verdes**.
- `git status --short` limpio tras los tres commits (salvo `.serena/`, sin
  seguimiento, herramienta local ajena al repo).

---

### D-345, Fase 1 — la corrección: pila a 12 KB, marcos grandes fuera del hilo de UI, motor de skins apagado

**1. La pila del hilo `main` pasa de 8 KB a 12 KB.**
`firmware/target/arm/s5l8702/app.lds`: `. += 0x2000` → `. += 0x3000`. Cabe en
la IRAM de core (48 KB, `0xC000`): `_fiqstackend` sube de `0xa530` a `0xb530`
y quedan **2 768 B** de margen. Archivo de Rockbox → registrado en
`MODIFICATIONS.md` (el archivo 31 de la lista).

**2. Herramienta de medición: `firmware/tools/stack_report.py`.**
Falla (salida 1) si una función de `apps/aura/` supera 1 024 B de marco o si
el peor camino estático desde `main` supera el 75 % de la pila. Corre en
`package_dist.sh` **antes** de empaquetar.

*Desviación respecto a §E.3 del maestro*, que pedía `-fstack-usage` + los
`.su`: la herramienta mide el **desensamblado del binario que se publica**
(`objdump -d`, prólogo `push` + `sub sp`). Razones, en orden: (a) los `.su`
describen lo que el compilador reserva por función *fuente*, y tras el
inlining el binario real difiere — el caso de `style_fonts_exist` de la línea
base es exactamente eso, y sumar los dos `.su` cuenta la misma memoria dos
veces; (b) no exige un segundo árbol compilado con otras banderas, así que la
puerta de `package_dist.sh` cuesta segundos y no un build entero; (c) marcos y
aristas salen del mismo desensamblado, así que siempre corresponden al mismo
binario. Se conserva `--su-dir` como contraste opcional.

La herramienta **declara** lo que excluye, en vez de bajar el número en
silencio: los manejadores de fallo (`panicf`, `thread_stkov`, `UnwindStart`…
— solo corren con el firmware ya caído; incluirlos hacía que el peor camino
de cualquier función que cediera el CPU arrastrara el manejador del
desbordamiento que la herramienta existe para evitar) y una arista con guarda
de ejecución (`skin_get_gwps → skin_load`, ver punto 4). Cada exclusión se
imprime con su motivo en el reporte, y `--keep-fault-handlers` /
`--keep-guarded-edges` las devuelven.

**3. Marcos bajados (bytes, mismo método de medición que la línea base):**

| Función | Antes | Después | Qué se movió |
|---|---|---|---|
| `aura_style_scan` | 4 408 | **768** | `paths[14][MAX_PATH]` de `style_fonts_exist` (integrada aquí) → `s_candidate_paths` |
| `try_activate` | 3 824 | **192** | `candidate_paths[14][MAX_PATH]` → el mismo `s_candidate_paths` |
| `run_search` | 1 392 | **904** | `buf[TAGCACHE_BUFSZ]` (552) → `buf[AURA_MUSIC_ITEM_LEN]` (64) |
| `draw_choice_list` | 1 272 | **< 688** | `items[32]` → `s_menu_items` |
| `count_unique_tag` | 1 272 | **784** | mismo cambio de `buf` que `run_search` |
| `draw_nav_list` | 1 112 | **< 688** | `items[32]` → `s_menu_items` |
| `draw_style_list` | 1 104 | **< 688** | `items[32]` → `s_menu_items` |
| `build_playlist_from_songs` | 1 104 | **840** | `path[MAX_PATH]` → `s_playlist_path` |
| `relocate_tagcache_files` | 1 080 | **< 688** | `src`/`dst` (2 × 520) → `static` |
| `write_marker` | 1 032 | **< 688** | `buf[MARKER_BUF_SIZE]` → `s_marker_buf` |

Mayor marco de `apps/aura/` hoy: **1 016 B** (`import_ratings_from_studio`),
por debajo del tope. Los `< 688` son funciones que salieron del top 12 del
reporte; el 12.º puesto mide 688 B.

**Cada buffer compartido lleva escrito su invariante de no anidamiento**, no
solo un `static`:
- `s_candidate_paths` (`aura_style.c`): `style_fonts_exist()` solo la llama
  `aura_style_scan()`; `try_activate()` solo `aura_style_boot()` y
  `aura_style_activate()`; y `aura_style_scan()` no llama a ninguna de las
  dos.
- `s_menu_items` (`aura_screens.c`): cada `draw_*()` arma su lista y se la
  pasa a `draw_menu_screen_v2()`, que la consume y regresa; nadie la conserva
  viva. Se añadieron tres `_Static_assert` para que agregar filas a
  `backlight_values`/`sleeptimer_values`/`mainmenu_rows` no pueda desbordar el
  buffer compartido en silencio — la cota `MAX_MENU_ENTRIES` ya había fallado
  una vez (crash real al abrir Ajustes, ver el comentario de esa constante).
- `s_marker_buf`/`src`/`dst` (`aura_sync.c`): todo el archivo corre en el hilo
  `main`.
- `s_playlist_path` (`aura_music.c`): solo `build_playlist_from_songs()`, que
  solo corre desde la UI.

**Lo que a propósito NO se hizo `static`:** `struct tagcache_search tcs` en
`run_search()`/`count_unique_tag()`/`build_playlist_from_songs()`. Guarda los
descriptores abiertos de la búsqueda, y `aura_music_browse()` corre **también
en el hilo del constructor de carátulas** (`aura_master_art_builder.c:142`):
compartirla sería una carrera con corrupción de la búsqueda, no un ahorro. En
`run_search` y `count_unique_tag` se redujo el buffer de texto en su lugar, y
la salida es idéntica byte a byte (la etiqueta acaba en `out[n].label`, de
`AURA_MUSIC_ITEM_LEN`; la unicidad la resuelve tagcache por el id numérico de
la entrada, `tagcache.c:1644`, no por el texto).

**Hallazgo colateral, fuera de alcance de esta fase:** `s_uniqbuf`
(`aura_music.c:86`, 8 KB) **ya se comparte hoy** entre el hilo de UI y el del
constructor sin candado — `run_search()` se lo pasa a `tagcache_search_set_uniqbuf()`
desde los dos. Es anterior a esta ronda (D-341) y no se tocó: arreglarlo es
una decisión de sincronización propia. Queda anotado.

**4. El camino de ~9.5 KB que D-343 dejó pendiente: cerrado en su raíz.**
El maestro (§ Fase 1.4) suponía que bastaría con dejar `wps_file`/`sbs_file`/
`rsbs_file` vacíos para que `settings_apply()` no entrara al motor de skins.
**Esa premisa es falsa**: `skin_load()` (`skin_engine.c:217`) carga el skin
**por defecto compilado** cuando el archivo no cargó, así que entra igual, con
archivo o sin él. Aura ya los deja vacíos y el camino existía de todos modos.

La entrada real no era `settings_apply` sino `settings_apply_skins()` en el
arranque (`apps/main.c`) y, después, cualquier camino de UI que llamara a
`sb_get_backdrop()`/`sb_skin_update()` — los tres entran por
`skin_get_gwps(CUSTOM_STATUSBAR, …)`, que carga en diferido mientras
`skins_initialised` sea true. Corrección: `settings_apply_skins()` ya no
carga skins (`apps/gui/skin_engine/skin_engine.c`, marcado `Aura (D-345)`,
registrado en `MODIFICATIONS.md`). Con `skins_initialised` en false,
`skin_get_gwps()` sale de inmediato para `CUSTOM_STATUSBAR` — la única
pantalla skinneable a la que Aura puede llegar, porque reemplazó el WPS y la
radio. Se conserva el resto de la función (init de backdrops, recarga del
ajuste, aviso `THEME_STATUSBAR`).

Es seguro porque es **el mismo estado en el que corre Rockbox antes de ese
init**: `gui_wps.data` apunta a memoria válida desde `gui_sync_skin_init()`
(`apps/main.c`, anterior), `sb_get_backdrop()` devuelve −1 y
`skin_backdrop_show(-1)` está contemplado, y `sb_skin_update()` /
`sb_skin_get_info_vp()` salen temprano por `sbs_loaded == false`.

`gui_usb_screen_run`, el camino que D-343 citaba: **6 824 → 4 416 B** (el plan
pedía bajar de 6 KB).

**5. `read_bmp_fd` (2 624 B) no se anida bajo `try_activate`** — medido, que
era la pregunta del paso 5. Son dos caminos separados:
`aura_style_read_icon_bmp → read_bmp_file → read_bmp_fd` = **3 848 B**, y
`try_activate → font_load → glyph_cache_load` = **4 128 B**. Ninguno pasa por
el otro.

**6. Marca de agua de la pila en "Acerca de"** (página 3, Créditos — la página
donde vive la versión). Lee el relleno `DEADBEEF` con que el kernel llena la
pila al crear el hilo (`thread.c`, `create_thread()`): el primer word que ya
no lo lleva marca lo más profundo que llegó la pila en toda la sesión. Es la
única forma de que el dueño confirme en hardware que esto quedó cerrado sin
esperar otro panic. Oculta por defecto en el aparato, se alterna con **SELECT
mantenido** sobre esa página (`AURA_BUTTON_HOLD`, que ahí no chocaba con
nada: un SELECT normal pagina, y en la última página ya no hacía nada).
Siempre visible en el simulador. En el simulador **no se inventa un número**:
el hilo `main` es un hilo de SDL con la pila del host y no hay canario que
contar, así que la fila dice "sin dato en el simulador". Textos nuevos al
final de las dos tablas de `aura_lang.c`.

**De paso**: `aura_str()` avisaba `-Wtype-limits` en cada compilación de
`aura_lang.c` (`id < 0` sobre un enum sin signo, el warning que D-344 dejó
anotado sin tocar). Un cast a `unsigned` cubre las dos formas del enum sin
warning y sin cambiar el comportamiento. El build del target queda **sin
warnings**.

**Resultado.**

| | Antes (línea base) | Después |
|---|---|---|
| Pila del hilo `main` | 8 192 B | **12 288 B** |
| Peor camino desde `main` | 10 620 B = **129.6 %** de los 8 KB originales | **6 608 B = 53.8 %** |
| Peor camino desde `aura_main` | 10 532 B | **6 520 B = 53.1 %** |
| `gui_usb_screen_run` (camino de D-343) | 6 824 B | **4 416 B** |
| Mayor marco de `apps/aura/` | 4 408 B | **1 016 B** |
| Funciones de `apps/aura/` sobre 1 024 B | 10 | **0** |
| `RAM usage` | 12 503 096 | 12 510 136 (**+7 040 B de BSS**) |
| `Binary size` | 1 299 688 | 1 299 736 |

El peor camino ya no es un camino de Rockbox: es de Aura
(`draw_choice_list → … → decode_album_master → read_bmp_fd → ATA`), y su parte
más pesada (`read_bmp_fd`, 2 624 B) es de Rockbox base.

**Verificado.**
- `make -C firmware/build-ipod6g`: **0 errores, 0 warnings**.
  `Binary size 1299736`, `RAM usage 12510136`.
- `firmware/tools/stack_report.py`: **verde** — 6 608 B (53.8 % de 12 288),
  ninguna función de `apps/aura/` sobre 1 024 B.
- `make -C firmware/rockbox/apps/aura/test test`: **16/16 suites verdes**.
- `firmware/tools/build_sim.sh` (compila + `make install` al simdisk):
  **0 errores**. Recorrido real con inyección de botones, capturas en
  `docs/screenshots/ronda-estabilidad/`:
  - **Temas**: se instaló un segundo estilo en el simdisk (`qa-prueba`, 14
    fuentes + íconos + `theme.cfg` con paleta propia) para ejercitar de
    verdad el buffer compartido. La lista muestra las dos entradas y
    "Prueba QA" **no** sale inerte (`style_fonts_exist` encontró las 14
    fuentes a través de `s_candidate_paths`); al activarla, la palomita se
    mueve y la UI queda intacta (`try_activate` recargó las 14 fuentes por
    el mismo buffer, inmediatamente después de que `aura_style_scan` lo
    usara). Capturas `02-temas-lista.png`, `03-temas-activado-qa.png`.
  - **Pantalla USB** (`06-usb.png`): el camino del punto 4. Dibuja la
    pantalla propia de Aura, sin cromo de Rockbox — con el motor de skins
    apagado.
  - **Fotos** cuadrícula y visor (`04`, `05`), **Music Flow** con CoverDrift
    y reflejo (`07`), **"Ahora suena"** con una pista sonando (`08`),
    **"Acerca de"** página 3 con la fila nueva (`01`).
- **Límite documentado**: el simulador tiene la pila del host, así que nada de
  esto prueba la aritmética de pila — solo que ningún cambio rompió el
  comportamiento. La verificación real es en hardware, con la marca de agua
  (lista al final de la ronda). Tampoco se pudo ejercitar el gesto de SELECT
  mantenido: el inyector del simulador no tiene token de "hold".

## D-346 — La búsqueda de tagcache deja de compartir memoria entre el hilo de UI y el del constructor de carátulas

**El hallazgo.** Apareció midiendo marcos para D-345, no por un reporte:
`aura_music_browse()` corre en **dos hilos** — la UI, y el constructor de la
caché maestra (`aura_master_art_builder.c`, fase de álbumes, D-341) — y
`run_search()` les daba a los dos **los mismos dos arreglos estáticos del
módulo**, sin candado:

- `s_uniqbuf[2048]` (8 KB), el buffer de valores únicos que consume
  `tagcache_search_set_uniqbuf()`.
- `s_tracknums[AURA_MUSIC_MAX_SONGS]` (20 KB), la tabla paralela de números de
  pista, que `sort_items_by_label()` **permuta junto con las etiquetas**.

Basta con entrar a Álbumes mientras el constructor está en su fase de álbumes:
el constructor cede el CPU dentro de su recorrido (`breathe()`, `yield()`), así
que las dos búsquedas se intercalan de verdad. La consecuencia no es un
crash sino algo peor de diagnosticar: entradas únicas contadas de más o de
menos, y números de pista intercambiados entre dos listas distintas — es
decir, una lista de canciones cuyo orden no corresponde a lo que se
reproduce, exactamente el bug que D-118/D-325 cerraron.

Es **anterior a esta ronda**: nació cuando D-341 movió el constructor a su
propio hilo. Nadie lo reportó porque en una biblioteca chica la fase de
álbumes dura menos de un segundo.

**Decisión: memoria de trabajo del llamador, no del módulo.** Sin candado
nuevo, sin estado compartido que sincronizar — el hilo que busca trae su
propia memoria. `aura_music.h` gana:

```c
typedef struct {
    uint32_t uniqbuf[2048];
    long    *tracknums;
    int      tracknums_max;
} aura_music_scratch_t;

int aura_music_browse_scratch(aura_screen_id_t screen, aura_music_item_t *out,
                              int max_items, aura_music_scratch_t *scratch);
```

`aura_music_browse()` sigue existiendo con la misma firma: es esa función con
el scratch del hilo de UI, así que ninguno de sus ~10 llamadores cambia. El
constructor usa `aura_music_browse_scratch()` con un `s_builder_scratch`
propio.

**Por qué `tracknums` es un puntero y no otro arreglo de 20 KB.** Los números
de pista solo significan algo en búsquedas de **título** (ordenar las
canciones de un álbum como el disco). El constructor solo lista **álbumes**,
así que no los necesita: su scratch lo deja en `NULL` y se ahorra los 20 KB.
De paso, `run_search()` ya no llama `tagcache_get_numeric()` por fila en las
búsquedas de artista/álbum/género/autor, donde el valor se descartaba.

Para que ese ahorro no se convierta en un bug silencioso, `run_search()`
**rechaza en voz alta** una búsqueda de títulos sin tabla (`DEBUGF` + devuelve
0) en vez de servir una lista mal ordenada. Hoy es inalcanzable — el único
llamador sin tabla es el constructor, que solo lista álbumes — y si alguien
agrega un camino nuevo se entera ahí y no en el orden de una lista. `max`
también se acota a `tracknums_max`.

**Detalle que costó 8 KB del binario descubrir**: `s_ui_scratch` se declaró
primero con inicializador designado (`.tracknums = s_ui_tracknums`). Con un
inicializador, los 8 KB del `uniqbuf` dejan de ser `.bss` y viajan como
`.data` **dentro del binario** — 8 KB de ceros en cada `rockbox.zip` y un
archivo más que cambia en cada actualización selectiva (contrato v11). Se
declara sin inicializador y `aura_music_browse()` pone los dos punteros antes
de usarlo.

**Metro y moonlit deben revisar el mismo patrón**: las tres familias heredaron
el constructor en segundo plano de la misma pasada (D-341/M-100/D-061). Si su
navegador de música usa un `uniqbuf` estático del módulo y el constructor
entra por la misma función, tienen la misma carrera. Lo avisa la supervisora.

**Costo.** `RAM usage` 12 510 136 → 12 518 456 (**+8 320 B de BSS**: el
`uniqbuf` del constructor). `Binary size` 1 299 736 → 1 299 872 (+136 B, el
código del wrapper).

**Verificado.**
- `make -C firmware/build-ipod6g`: **0 errores, 0 warnings** (de paso, el
  mismo `-Wtype-limits` de D-345 en `a26_font()`, que salió a la luz al
  recompilar `aura_style.c`, recibió el mismo cast a `unsigned`).
- `firmware/tools/stack_report.py`: **verde**, sin cambio — 6 608 B (53.8 %).
- `make -C firmware/rockbox/apps/aura/test test`: **16/16 suites verdes**.
- Simulador: se **vació** `/.aura/art/albums` para que el constructor tuviera
  trabajo real (19 JPEG por decodificar) y se entró a Música › Álbumes
  mientras corría. La lista sale completa, en orden alfabético, sin
  duplicados ni etiquetas cruzadas, y las **19 maestras se escribieron**
  durante esa misma corrida — las dos búsquedas se intercalaron de verdad.
  Captura `docs/screenshots/ronda-estabilidad/09-albumes-durante-constructor.png`.
- **Límite honesto de esa prueba**: una carrera no se demuestra ausente
  ejecutándola una vez. Lo que la cierra es estructural — ya no hay memoria
  compartida entre los dos hilos en este camino —; la corrida solo comprueba
  que la separación no rompió nada.

## D-347 — Pantalla de arranque del bootloader: la marca de Aura y la frontera GPL, antes de que exista sistema de archivos

**Por qué.** Desde D-064 el bootloader arranca en silencio absoluto
(`verbose = false`): pantalla negra hasta que el firmware toma el control y
`show_logo_boot()` pinta la marca. Son ~1 s de nada, y —más importante— el
bootloader es la pieza que se flashea en NOR, la única que corre antes de que
haya sistema de archivos, y no decía en ninguna parte de dónde viene el
código. La GPL v2 §1 pide que el aviso de copyright y de garantía viaje con el
programa; una pantalla negra no lo hace.

Diseño canónico del plan maestro §B, cerrado **una vez** en esta ronda: Metro
y moonlit lo portan tomando este diff como referencia. Cambiar el bootloader
implica que Aura Studio ofrezca reflashearlo (§B.5, trabajo del repo hermano),
así que no se retoca por gusto después.

**Qué se ve.** Lienzo 320×240, fondo negro (el mismo con el que el bootloader
ya limpia la pantalla, `lcd_set_background(LCD_BLACK)`):

1. El wordmark **aura**, centrado.
2. Dos leyendas en `FONT_SYSFIXED` (6×8, la única fuente que el bootloader
   tiene), centradas, en `#8E8E93`; la última a 14 px del borde inferior,
   interlineado 12:
   ```
   aura · arranque <rbversion>
   Basado en Rockbox · GPL v2 · rockbox.org
   ```
   La versión es la del **bootloader**: es lo único que él conoce. El firmware
   se actualiza aparte y muestra la suya en "Acerca de".

Sin retardo artificial: la pantalla dura lo que tarde `load_firmware()`.
`verbose` sigue en `false`.

**La parte que importa: la marca no se mueve en el handoff.** El bitmap del
bootloader es el **mismo wordmark** del `rockboxlogo.320x98x16.bmp` que pinta
el firmware, recortado a su caja de tinta — meter el lienzo de 320×98 entero
en la IRAM del bootloader habría costado 62 720 B. Que el recorte caiga en el
píxel exacto no se logró copiando un offset al C (una constante más que se
puede desincronizar), sino por construcción:

- `gen_boot_logo.py` centra la **caja de tinta** del wordmark en los dos ejes
  del lienzo de 320×98.
- `show_logo_boot()` centra ese lienzo en los dos ejes de la pantalla. Luego
  el centro de la tinta cae en el centro de la pantalla.
- El recorte lleva la tinta más 4 px de margen, así que su centro es el mismo.
- Centrar el recorte en la pantalla lo pone, por construcción, en el mismo
  lugar.

Con una salvedad real: el bootloader centra con **división entera**, y con un
margen simétrico de 4 px la marca quedaba corrida **1 px en X** (el ancho de
tinta, 132, y el de pantalla, 320, no tienen la misma paridad). En vez de
pasarle un offset al C, el generador ensancha el margen **lejano** en 1 px
hasta que el centrado entero da el píxel exacto: cambia cuánto fondo negro
lleva de un lado, no dónde queda la marca. El recorte resultante mide
**141×45** y el bootloader lo pinta en **(89, 97)**.

El generador **comprueba esa igualdad antes de escribir nada** y aborta si no
se cumple: si alguien cambia el lienzo o el centrado de `show_logo_boot()`,
falla ahí en vez de producir una marca que salta al arrancar.

**Modo USB y errores.** `draw_boot_screen()` deja `line` justo debajo de la
marca (derivado de `BMPHEIGHT_bootwordmark`, no una constante), así que el
texto posterior del bootloader cae **debajo** y no encima. El encabezado
"Bootloader USB mode" pasa al gris de leyenda; "Plug USB cable" y las demás
líneas de acción **se quedan en blanco** a propósito: son instrucciones de
recuperación y el blanco sobre negro es lo más legible. `error()` /
`fatal_error()` limpian la pantalla por su cuenta y no cambian.

**El gris es un literal RGB, y es una excepción documentada.** El bootloader no
enlaza la paleta de Aura (`apps/aura/` no existe en ese build), así que
`#8E8E93` viaja como `LCD_RGBPACK(0x8e, 0x8e, 0x93)` en el C. Es el mismo
valor que `aura_ds.color.category.settings_gray_hex` de `tokens.json`, de
donde lo toma también la maqueta — así los dos no pueden divergir en silencio.

**Bug latente corregido de paso.** `gen_boot_logo.py` leía
`tokens["color"]["dark"]["background"]`, una clave que **ya no existe**
(se renombró a `shell_bg` en algún momento posterior): el script reventaba con
`KeyError` desde entonces y nadie lo notó porque el BMP ya estaba versionado.
El fondo pasa a ser negro literal con su razón escrita — `shell_bg` es
`#1C1C1E`, que no es lo que este bitmap necesita: se pinta antes de que exista
tema (D-051). Regenerado, el `rockboxlogo.320x98x16.bmp` sale **byte a byte
idéntico** al del árbol, así que la reproducibilidad del `rockbox.zip`
(contrato v11) no se toca. El script gana `--check` para poder comprobar eso
sin escribir.

**Cómo se verifica algo que el simulador no ejecuta.** El simulador **no**
corre el bootloader. Por eso el generador produce también
`docs/screenshots/ronda-estabilidad/bootloader-maqueta.png`: la pantalla
completa de 320×240 con la marca en su posición real y las leyendas dibujadas
con los **glifos reales** de `FONT_SYSFIXED`, leídos del mismo
`fonts/08-Schumacher-Clean.bdf` del que Rockbox genera su `sysfont.c` — no una
fuente monoespaciada parecida. Es la maqueta que el dueño aprueba **antes** de
que nadie flashee nada.

**Verificado.**
- Bootloader (`firmware/build-ipod6g-boot`, `--type=B`): **0 errores, 0
  warnings**. `bootloader-ipod6g.ipod` **95 592 → 108 648 B** (el bitmap de
  141×45×16 son 12 690 B), muy por debajo del tope de 150 KB.
- **IRAM medida**: `IRAM1_SIZE` = `0x20000` (131 072 B) en el S5L8702;
  `MOVE_AREA` = `IRAM1_SIZE − IM3HDR_SZ` = `0x1F800` (129 024 B). Ocupado
  108 640 B (`.text` 83 012 + `.rodata` 17 608 + `.data` 8 000) = **84.2 %**,
  quedan **20 384 B** libres.
- Target (`firmware/build-ipod6g`): **0 errores**; no recompiló nada — la
  entrada nueva de `apps/bitmaps/native/SOURCES` está bajo
  `#if defined(BOOTLOADER)`, así que el bitmap **no** entra al firmware.
- `gen_boot_logo.py --check` y `--bootloader-crop --check`: los dos bitmaps
  del árbol coinciden con lo que genera el script.
- `firmware/tools/stack_report.py`: verde, sin cambio.
- **No se flasheó nada desde esta sesión** y no se puede: el simulador no
  ejecuta el bootloader y flashear NOR es del dueño, con Studio, y solo
  después de que Studio tenga "Actualizar el arranque" (§B.5).

## D-348 — Un directorio de compilación viejo produce un binario distinto al de uno limpio: `make.dep` de Rockbox se congela

**Cómo apareció.** Aviso de la sesión de Metro (R7-3): allá un build limpio del
target llevaba semanas sin compilar y nadie lo vio porque `build-ipod6g/`
arrastraba un `make.dep` viejo. Se comprobó aquí el mismo patrón — y aunque la
causa concreta de Metro (un `-I` que solo estaba en `MPEGCFLAGS` y no llegaba
a `mkdepfile`) **no existe en Aura** (aquí nada fuera de `apps/aura/` necesita
un `-I` propio: `apps/main.c` y `apps/gui/usb_screen.c` incluyen
`"aura/aura_main.h"`, con el prefijo, y `apps/` ya está en el camino de
inclusión), el problema de fondo sí está, y es peor de lo que parecía.

**El defecto real, en Rockbox base.** `tools/root.make:209`:

```make
$(DEPFILE) dep:
	$(call mkdepfile,...)
```

La regla **no tiene prerrequisitos**. `make.dep` se genera la primera vez que
se compila un directorio y **nunca se vuelve a generar**. Cualquier `#include`
agregado después es invisible para `make`: el objeto que lo usa no se
recompila cuando esa cabecera cambia, para siempre.

**Medido en este árbol.** `firmware/build-ipod6g/make.dep` no sabía que
`aura_master_art.o` y `aura_movieflow.o` dependen de `apple2026_tokens.h` (la
cabecera **generada** por `design-system/generate.py`, que cambia cada vez que
se toca `tokens.json`). Resultado: el `rockbox.bin` de ese directorio difería
en **7 bytes** del que produce un directorio limpio **con el mismo commit** —
objetos compilados contra una versión anterior de la paleta, conviviendo con
el resto. Tras `make dep` + recompilar, los dos binarios son **byte a byte
idénticos**.

Siete bytes no suenan a nada, y esa es exactamente la razón por la que
importa: nadie lo habría notado nunca, y el contrato v11 (actualizaciones
selectivas por CRC32) descansa sobre que dos builds del mismo código den lo
mismo.

**Decisión.** `firmware/tools/package_dist.sh`:

- **Siempre**: `make dep` antes de `make`, para que la base de dependencias
  refleje los `#include` de hoy. Cuesta segundos.
- **Con `--release-tag`**: además, borra y reconfigura el directorio de
  compilación. Un release tiene que ser reproducible byte a byte y no se
  arriesga a ningún otro estado viejo que `make dep` no cubra. Cuesta un build
  completo, que para un release es el precio correcto.

No se toca `tools/root.make`: cambiar la regla de Rockbox afectaría a todos los
targets del fork por un problema que se resuelve entero desde el script propio
de Aura, y `MODIFICATIONS.md` no crece por algo que no hace falta.

**Metro y moonlit tienen el mismo `root.make`.** Lo suyo es más grave (allá el
build limpio directamente falla), pero la regla congelada es común a las tres
familias y su `package_dist.sh` merece el mismo par de líneas.

**De paso: `stack_report.py` gana `BIG_FRAMES`** (propuesta de moonlit,
aprobada por la supervisora, para que las tres familias compartan la misma
herramienta): una lista de funciones de `apps/<familia>/` que pueden superar el
tope de 1 024 B **con su motivo escrito**, porque el marco viene de un idioma
de Rockbox que no se puede quitar sin empeorar otra cosa (`struct
tagcache_search` en la pila cuando la función corre en más de un hilo — D-346
—, un `char[MAX_PATH]` que la API exige por valor). Cada entrada permitida se
imprime con su razón; una entrada que dejó de hacer falta genera un **aviso**
para borrarla; y cualquier función nueva sobre el tope **sigue fallando**.

**En Aura la lista queda vacía**: D-345 y D-346 bajaron las tres candidatas
(`run_search`, `count_unique_tag`, `build_playlist_from_songs`) por otros
medios. Se conserva la maquinaria porque la herramienta es compartida.

**Verificado.**
- Build limpio en `firmware/build-ipod6g-clean` (configurado desde cero,
  `--target=ipod6g --type=N`): **0 errores**. Aura no tiene el fallo de Metro.
- `cmp` de los dos `rockbox.bin` con el mismo commit: **7 bytes distintos**
  antes; **idénticos** después de `make dep` + recompilar en el directorio
  viejo.
- `BIG_FRAMES` probado con una entrada falsa y una obsoleta y el tope bajado a
  900 B: imprime el motivo de la permitida, avisa de la obsoleta, y **falla**
  con la función nueva que se pasa. El archivo se restauró tras la prueba.
- `firmware/tools/stack_report.py`: verde con la lista vacía.

## D-349 — Contrato v18: imágenes cuadradas de punta a punta; y una copia del contrato de biblioteca que había quedado atrás

**Registro, sin trabajo de firmware en esta decisión** (el trabajo va en D-350).
Fija el texto que las dos mitades de la ronda comparten: **Studio** garantiza
que lo que escribe es cuadrado (`cover.jpg` 320×320, foto de artista 128×128,
las dos recortadas al centro desde su copia local) y **los tres firmwares**
dejan de suponerlo sin verificarlo.

**`CONTRATO-firmware-studio.md` → v18.** Se **copió desde `../Aura-Studio`**
(solo lectura) y quedó **byte a byte idéntico** a la copia de allá — que es el
objetivo de que exista una sola versión del texto. Verificado contra el plan
maestro §A.2: cabecera "Versión 18 — 2026-09-03", párrafo de encabezado v18, la
nota de §D.3 (recorte cuadrado de la foto de artista), el bloque "Versión de
formato y purga" de §D.5 (`/.aura/art/format.txt`), la clave de álbum de §D.5
con el `mtime` de `cover.jpg`, la fila nueva de la tabla de §D, y
`--family moonlit` fuera de Pendientes. Única diferencia de forma con el
maestro: el bloque de formato/purga se escribió como viñeta dentro de la lista
de §D.5 (y el GC salió a párrafo aparte) en vez de un párrafo nuevo; el texto
literal se conserva entero.

**`docs/contracts/library-layout-v1.md` → v1.5, y por qué NO se copió.** La
copia de Aura Studio traía el párrafo nuevo correcto, pero sobre una **base
anterior a D-341**: revertía tres cosas que ya no son ciertas desde hace dos
semanas, y las publicaba con un número de versión que aquí ya estaba usado con
otro contenido ("v1.4"). Lo que se habría perdido al copiarla:

1. **§2, nota de `cfcache/`.** Su copia decía que se indexa por `album_seek` de
   tagcache y que *"el firmware la vacía él mismo al terminar una
   reconstrucción"*. Falso desde **D-338**: la clave es estable
   (`crc32(ruta) + mtime`) y el firmware conserva las entradas de álbum,
   recogiendo huérfanas con presupuesto.
2. **§4.2, fila `music`.** Volvía a "vacía `/.rockbox/aura/cfcache/`".
3. **§5.** Faltaba la referencia a la caché maestra compartida
   (`/.aura/art/{albums,artists,photos}/`, D-340/D-341).
4. **§6.** Faltaba la entrada de v1.4 del 2026-08-26.

No es que su texto contradiga al maestro: es que la base era vieja. Como
`Aura-Firmware` es la fuente canónica de **este** documento (lo dice su propia
cabecera), se resolvió al revés de la regla general de esta ronda: aquí se
escribe **v1.5 — 2026-09-04** = la v1.4 íntegra **más** el párrafo literal del
maestro (autodescrito "v1.5:"), y Aura Studio copia este archivo. Reconciliado
con la supervisora antes de escribir nada; la copia de Studio **no se tocó**.

La lección, que vale para las tres familias: cuando dos repos mantienen "copias
idénticas", copiar en la dirección equivocada no produce un conflicto — produce
una **reversión silenciosa**. Lo único que la delató fue hacer `diff` de los dos
archivos enteros en vez de solo mirar el bloque nuevo.

**Renumeración respecto al plan hijo.** El plan asignaba D-347 al contrato v18;
quedó en **D-349** porque en el camino entraron D-346 (la carrera de tagcache,
encargo de la supervisora) y D-348 (la reproducibilidad del build). El
bootloader es D-347.

## D-350 — Las carátulas no cuadradas se rompían en los tres caminos que no pasan por la caché maestra (contrato v18)

**El síntoma que reportó el dueño** ("las imágenes se ven mal") tenía una
causa exacta y reproducible. La caché maestra ya recortaba bien
(`aura_master_art_decode_fill()`: fill + center-crop desde v16). Lo que
seguía roto eran los **caminos que no pasan por ella**: "Ahora suena" (135 px)
y el decode "más grande que la maestra" (CoverDrift 320), más la portada de
playlist. Los tres hacían lo mismo:

```c
int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
bm.width = size; bm.height = size;
read_jpeg_file(path, &bm, ...);          /* -> 135 x 101 con una 4:3 */
...
mask_corners_buffer(buf, ART_SIZE, ...); /* lee 135 x 135 */
aura_art_transpose(decoded, dst, size);  /* idem */
```

`FORMAT_KEEP_ASPECT` **ajusta dentro** de la caja, no la llena: con una
portada 4:3 el decodificador devuelve 135×101, y el enmascarado de esquinas,
el reflejo y la transposición siguen leyendo con stride 135 y alto 135. Las
últimas 34 filas son memoria sin inicializar. **No es que la imagen quede
mal encuadrada: queda rota**, con basura de colores debajo.

Se reprodujo en el simulador con fixtures nuevos y quedó capturado antes y
después (`docs/screenshots/ronda-estabilidad/11-antes-ahora-suena-4x3.png`
vs `11-ahora-suena-4x3.png`, y el par `13-*` con una 1:4).

**Corrección: una sola primitiva para todos.** Los tres pasan a
`aura_master_art_decode_fill()`, la misma que ya usaba la maestra, que es
genérica en `size` desde D-341. Como dos de los tres corren con el candado de
decodificación ya tomado por su llamador, se separó en
`aura_master_art_decode_fill_locked()` (el núcleo) y un envoltorio que
bloquea — `mutex_lock()` de Rockbox **no es reentrante**, así que llamar a la
versión que bloquea desde dentro de una región ya bloqueada habría sido un
abrazo mortal, no un no-op.

`decode_album_at()` y `decode_playlist_art()` dejan de devolver un puntero al
scratch con dimensiones arbitrarias y ahora **llenan un cuadrado** en el
buffer del llamador (`out->cover_data`, que mide `size × size` por contrato),
desde donde se transpone. Un buffer menos y una suposición menos.

**Bug que introdujo la corrección y que el simulador atrapó**: al quitar el
`struct bitmap` de `load_album_art()` quedó `s_art_bm.data` sin asignar, y el
dibujo de la carátula inclinada seguía leyéndolo — **segfault** en el
simulador al abrir "Ahora suena". `s_art_bm` se eliminó por completo: ya solo
era un envoltorio de un puntero que ahora es directo. Vale la pena anotarlo:
la captura automática lo encontró en la primera corrida.

**Lo que NO se cambió, y por qué.** El tile de video/foto de las listas
(`aura_screens.c`) sigue con `KEEP_ASPECT` + banda centrada. Es deliberado:
un cartel de video es **3:4 por diseño** (contrato §D.1, "sin cambio") y
recortarlo al cuadrado le cortaría la cabeza. Además nunca tuvo el bug —
centra usando `bm.width`/`bm.height` reales, que es exactamente la diferencia.
Anotado en el código para que la próxima auditoría no lo "arregle".

**Qué pasa con una proporción extrema.** `aura_master_art_fill_box()` rechaza
más de 4:1 y `aura_master_art_decode_fill()` cae al "centrado sobre color
promedio". Verificado con la 1:4: sale la imagen completa centrada sobre gris,
sin recorte ni rotura. Es el comportamiento correcto — recortar 1:4 al
cuadrado tiraría el 75 % de la imagen.

**`/.aura/art/format.txt` (§D.5 del contrato v18).** El problema que cierra:
la clave de una entrada de caché describe su **fuente** (ruta + mtime), no el
código que la derivó. Corregido el decode, las miniaturas mal derivadas por la
versión anterior siguen teniendo la clave correcta y sobrevivirían **para
siempre**. `aura_sync_check_art_format()` lee el entero al arrancar y, si
falta o es menor que 2, purga `/.aura/art/{albums,artists,photos}` y las dos
L2 privadas de Aura (`cfcache`, `photocache`) y escribe su versión. Se llama
desde `apps/main.c` junto a `aura_sync_force_shared_db_path()` — **antes de
que nada lea arte**: si una miniatura rota se lee una sola vez, se dibuja.

**Medida de la purga** (el plan pedía moverla al hilo del constructor si pasaba
de 2 s): con **3 569 entradas** sembradas en el simdisk, **13 ticks = 0.13 s**.
Se queda en el hilo de UI. Límite honesto: es un SSD de Mac, no el disco de
2008 del iPod; si en hardware resultara lenta, la medición ya está
instrumentada (`DEBUGF` con ticks) para decidirlo con un número.

**Clave de álbum con el `mtime` de `cover.jpg`** (§D.5 del contrato v18):
`max(mtime de la pista, mtime del cover.jpg hermano)`. Cierra la hipótesis (a)
de D-338/M-096/D-055 — una carátula reescrita **sin tocar la pista** dejaba la
clave igual y la maestra vieja sobrevivía. La decisión se partió en dos piezas
puras y probadas en host (`aura_cache_keys_album_mtime()` y
`aura_cache_keys_sibling_cover()`, **13 comprobaciones nuevas** en
`test_cache_keys.c`), y el acceso a disco quedó en
`aura_fsutil_file_mtime()`. Rockbox no tiene `stat()`: la única forma de leer
la fecha de un archivo es recorrer su directorio con `readdir()`. Con dircache
listo (siempre en el 6G) eso se sirve de RAM; el costo es un recorrido del
directorio del álbum por resolución de clave.

**Token `tile_placeholder` (§F del plan maestro).** El relleno del tile de
respaldo y el recuadro de selección eran **el mismo color**
(`A26_SELECTION_FILL`), así que en una cuadrícula de tiles sin carátula la
fila activa desaparecía. Token nuevo en `tokens.json` (claro `#D1D1D6`, oscuro
`#3A3A3C`) → `A26_COLOR_{LIGHT,DARK}_TILE_PLACEHOLDER` vía `generate.py`, y
por tanto también `AuraPalette.swift` (Studio lo recibe al subir el pin).

*Desviación de forma respecto a §F*: se expone como `a26_tile_placeholder()`,
no como un miembro de `a26_token_t`. Ese enum es la paleta que un **tema**
puede sobrescribir (8 roles, `CONTRATO-formato-tema.md`): agregarle un rol
cambiaría el formato de tema, que esta ronda no toca. Es un color compilado
por tema, resuelto igual que `aura_accent()` — el precedente ya establecido en
el árbol para un color que no es rol de tema.

**Fixtures reproducibles**: `firmware/tools/gen_test_media.sh --aspect-fixtures`
crea cuatro álbumes con `cover.jpg` de 1:1, 4:3, 16:9 y 1:4, con barras de
color SMPTE — un recorte con el stride equivocado sale **rasgado**, no solo
mal encuadrado, así que el fallo se ve a simple vista.

**Verificado.**
- Target: **0 errores, 0 warnings**. `stack_report.py` **verde**, sin cambio
  (6 608 B, 53.8 %).
- `make -C firmware/rockbox/apps/aura/test test`: **16/16 suites verdes**,
  incluidas las 13 comprobaciones nuevas de la clave v18.
- Simulador (build limpio + `make install`), con los cuatro álbumes de
  proporciones y la base reconstruida:
  - **"Ahora suena" 4:3, 16:9, 1:1 y 1:4**: las cuatro salen cuadradas y
    limpias (`11`…`14`). El antes de la 4:3 y la 1:4 muestra la rotura
    original (`11-antes-*`, `13-antes-*`).
  - Lista de **Álbumes** con los cuatro tiles (`10`), **CoverDrift** en el
    panel derecho de Música (`15`).
  - **Purga de formato**: 3 569 entradas borradas, `format.txt` = 2, y el
    segundo arranque **no** vuelve a purgar.
- **Límite de la evidencia**: la captura de CoverDrift no es un antes/después
  concluyente — en el momento del volcado el carrusel mostraba un fixture
  cuadrado, no una de las portadas de proporción. Comparte el código
  corregido (`decode_album_at`) con "Ahora suena", que sí quedó probado
  punta a punta; la confirmación visual de CoverDrift con una portada 4:3 va
  a la lista de hardware.

## D-351 — El interruptor Hold pasa a ser el gesto de bloquear; "Bloqueo" gana su submenú; y los ajustes de Rockbox dejan de perderse

**Punto de partida.** El bloqueo de Aura solo se pedía **al encender**. La
razón no era de diseño: el interruptor Hold del 6G **no genera eventos** —
`pmu_holdswitch_locked()` es un estado que hay que **leer** — y el bucle
principal esperaba botones sin límite de tiempo, así que nadie lo leía. El
único que lo consultaba era la barra de estado al dibujarse, de modo que en
una pantalla quieta el candado podía tardar minutos en aparecer, o no
aparecer nunca.

**1. Sondeo en el bucle principal.** `next_button()` pasa a tener un tope de
`HZ/2`, aplicado **después** de todas las puertas de animación y solo hacia
abajo: una animación que pidió 20 fps sigue a 20 fps. No se gatea con
`lcd_active()` — con la pantalla dormida no hay nada que dibujar, pero el
flanco hay que verlo igual para saber **cuánto tiempo** estuvo puesto el Hold.

Los flancos se resuelven en el bucle, entre leer el botón y repartirlo a las
pantallas, **nunca dentro de una pantalla**: una pantalla no puede saber si
otra ya atendió el mismo flanco, y el estado tiene que sobrevivir a cambiar de
pantalla. La primera vuelta **adopta** el estado sin disparar flanco: arrancar
con el Hold ya puesto no es "acabas de bloquear".

**2. Pantalla de bloqueo en reposo.** Con Hold puesto y el bloqueo armado se
dibuja **en lugar de** la pantalla actual, sin tocar la navegación: al quitar
el Hold se vuelve exactamente a donde estaba. Candado grande en el **mismo
sitio** que la pantalla de desbloqueo, para que quitar el Hold no mueva nada —
solo aparecen las cajas de dígitos debajo. Sin entrada de código: con Hold
puesto la rueda está muerta por hardware, no hay nada que teclear.

**3. `screen_lock_require`** (`aura.cfg`): `hold` (por defecto), `1min`,
`5min`, `boot`. Un Hold accidental en el bolsillo no obliga a teclear;
`boot` conserva el comportamiento anterior a esta decisión.

**4. "Bloqueo" es ahora un submenú** (§D.6): Activar · Cambiar código · Pedir
código · Quitar bloqueo. Las cuatro filas **existen siempre y no se mueven**:
sin bloqueo armado solo "Activar" es elegible, con él las otras tres. Se
prefirió esto a una lista que cambia de largo porque una fila que aparece y
desaparece hace saltar la selección bajo el dedo, y el árbol ya usa filas
atenuadas para "presente pero no elegible" (Recopilaciones, Audiolibros).

Eso destapó un hueco real: `aura_screenlock.c` **infería** qué iba a hacer a
partir del estado (`enabled` sin `active` ⇒ desactivar), así que con el
bloqueo armado la única pantalla alcanzable era la de desactivar — **"Cambiar
código" no tenía forma de existir**. Ahora el submenú lo dice explícitamente
(`aura_screenlock_begin(SET | CHANGE | REMOVE)`).

De paso, el título de la pantalla de código pasa de "Bloqueo de pantalla" a
**"Bloqueo"**: no cabía en la barra y salía truncado ("Bloqueo de p").

**5. Los ajustes de Rockbox se perdían** (hallazgo portado de Metro R7-5, vía
la supervisora; verificado aquí en el código). `settings_save()`
(`apps/settings.c:738`) **no escribe nada**: solo registra
`flush_config_block_callback` en `DISK_EVENT_SPINUP`, y
`call_storage_idle_notifys()` se **auto-bloquea 30 s** entre corridas salvo
con `force` (`firmware/ata_idle_notify.c`). La escritura real llega en el
apagado limpio, por `system_flush()`. Consecuencia: un reinicio a mano o una
batería agotada pierde **minutos** de ajustes — brillo, retroiluminación,
límite de volumen, apagado automático, repetir, clicker.

`aura_settings_core_touched()` sustituye a `settings_save()` a secas en los
**14 sitios** de Aura que lo llamaban: hace lo mismo y además anota que hay
algo pendiente. `aura_settings_core_flush()` fuerza la escritura y se llama
**al salir de una pantalla** (MENU), no en cada clic: forzar un giro de disco
por cada paso de la rueda del brillo sería peor que el problema. Solo escribe
si de verdad cambió algo, así que navegar con MENU sin tocar nada no gira el
disco. El cambio de familia (`aura_firmware_switch.c`) fuerza el flush **de
inmediato**: lo que sigue es un reinicio, no una salida de pantalla.

**6. El simulador gana un token `HOLD`** en su inyector
(`uisimulator/common/sim_tasks.c`, registrado en `MODIFICATIONS.md`). Sin él
toda esta máquina de estados solo se podría probar a mano: el Hold no es un
botón que se pueda inyectar como pulsación. Es la diferencia entre "compila" y
"está verificado".

**Verificado.**
- Target: **0 errores, 0 warnings**. `stack_report.py` **verde**, sin cambio.
- `make -C firmware/rockbox/apps/aura/test test`: **16/16 suites verdes**.
- Simulador, recorrido real con el token `HOLD` (capturas en
  `docs/screenshots/ronda-estabilidad/`):
  - `16`: **candado en la barra** del menú raíz al poner Hold, **sin tocar
    ningún botón** — que es justo lo que antes no pasaba.
  - `18`: **pantalla de bloqueo en reposo** ("Bloqueado", candado, reloj,
    batería, sin cajas de dígitos).
  - `19`: ciclo Hold ON→OFF con `require = hold` → **pide el código**.
  - `20`: submenú de Bloqueo con las cuatro filas y "Activar" atenuada
    (el bloqueo ya estaba armado). `21`: la fila renombrada en Ajustes.
  - `22`: "Pedir código" con las cuatro opciones y "Al bloquear" marcada.
- **Límite documentado**: los umbrales de 1 y 5 minutos no se probaron
  esperando el tiempo real; lo que se verificó es la rama `hold` (umbral 0) y
  que `boot` no pide nada por Hold. La aritmética es un solo `TIME_AFTER`
  sobre `hold_since`. Va a la lista de hardware.

## D-352 — La barra de estado centraba el título por la altura de la fuente, no por la de las mayúsculas

**El defecto.** `text_y = (alto_barra − h) / 2`, con `h` de
`lcd_getstringsize()`, que es `font->height` e **incluye el descendente**. El
resto de la barra —reloj, iconos, batería— se centra por su **tinta real**.
Resultado: el título quedaba **1 px más arriba** que todo lo demás. Un píxel
es exactamente la clase de defecto que sobrevive a la revisión visual y
delata que algo no está calculado.

**La corrección.** `design-system/generate.py` mide la **caja de tinta de las
mayúsculas** de cada rol leyendo el glifo `H` del `.fnt` **ya rasterizado**
(no de la TTF: lo que importa es lo que el aparato va a dibujar) y emite
`A26_FONT_CAP_TOP_<ROL>` / `A26_FONT_CAP_H_<ROL>` en `apple2026_tokens.h`.
La barra centra con ellas:

```c
text_y = (alto_barra - A26_FONT_CAP_H_DS_BOLD_12) / 2 - A26_FONT_CAP_TOP_DS_BOLD_12;
```

Formato del `.fnt`, que hubo que deducir (documentado en el lector): tras la
cabecera van los píxeles **fila a fila sin relleno entre filas**, 4 bits cada
uno, **nibble bajo primero**, y con el valor **invertido** — `0` es tinta,
`15` es fondo (`convttf.c` hace `0xff - *tsrc` antes de empaquetar). El campo
`depth` de la cabecera **no** son bits por píxel: `1` significa 4 bpp
(`font.h`). Las pistas del formato vinieron de moonlit (D-068) vía la
supervisora; el `depth` y el umbral se verificaron aquí decodificando el
glifo y mirándolo.

De paso, `generate_fonts()` pasa a correr **antes** que `generate_header()`:
con el orden anterior un árbol limpio fallaba, porque los `.fnt` que ahora se
miden todavía no existían.

**Verificación mecánica: `aura_spec_check.py statusbar`.** Agrupa la banda en
columnas contiguas de tinta —cada glifo, icono y la batería caen en su propio
grupo— y exige que el centro vertical de cada uno coincida con el de la barra
±1 px. Tres criterios, los tres pagados con errores por moonlit (D-068):

1. **"Tinta" no es "píxel claro"**, es "píxel que se aparta del **fondo**", y
   el fondo se deduce de la propia captura. Un umbral absoluto de luminancia
   se invierte al cambiar de tema.
2. Cada elemento se mide por **su** caja de tinta, no por su celda: un símbolo
   de 16 px dibuja entre 8 y 15 px de tinta según cuál sea.
3. La tolerancia de 1 px **no es holgura para errores**: el firmware centra el
   título por las **mayúsculas** mientras la herramienta mide la **tinta
   real**, así que un título con acentos o con letras que bajan tiene su
   centro medio píxel corrido. Eso es correcto y esperado.

Se añadió un cuarto criterio propio: un grupo cuya tinta ocupa la **banda
entera** no es un elemento de la barra sino el contenido de fondo (en
`(split)`, la imagen del panel derecho llega hasta el borde superior). Medir
su "centro" no dice nada — por construcción cae en el centro — y contarlo
falsearía el resultado.

**Verificado.**
- `(full)`: 4 elementos, **peor desviación 0.0 px**.
- `(split)`: 3 elementos + el panel descartado, **0.0 px**.
- **Honestidad sobre el alcance de la herramienta**: la captura anterior al
  arreglo (`16-antes-barra-titulo-1px-arriba.png`) mide el título en
  `y 5..12` (centro 8.5) contra 9.5 de todo lo demás — es decir, la
  herramienta lo habría dado por BUENO, justo en el borde de su tolerancia de
  1 px. La herramienta prueba que ahora la alineación es exacta; no es la que
  encontró el defecto. Se deja escrito para que nadie la use como si
  detectara desviaciones de un píxel.

### D-351, addendum — los umbrales de 1 y 5 minutos, ejercitados de verdad

El límite que quedó anotado al cerrar D-351 ("no se probaron esperando el
tiempo real") se cierra sin esperar catorce minutos: bajo `#ifdef SIMULATOR`
la **unidad** de los umbrales pasa de un minuto a **2 segundos**. La
aritmética de hardware no cambia — es el mismo `TIME_AFTER` sobre el mismo
`hold_since`, solo con otra constante.

**Dos segundos y no uno**: el inyector separa tokens consecutivos por ~1 s
(`AURA_INJECT_WAIT_TICKS`), así que con la unidad en 1 s "soltar **antes** del
umbral" sería inexpresable — el mínimo escribible ya empataría con el umbral.
Con 2 s, `HOLD,HOLD` cae dentro y `HOLD,WAIT,WAIT,HOLD` cae fuera, sin
ambigüedad.

**Las cuatro ramas, verificadas** (capturas `25`…`28`):

| Ajuste | Hold puesto | Umbral | Resultado |
|---|---|---|---|
| Tras 1 minuto | ~1 s | 2 s | **vuelve al menú, sin código** ✓ |
| Tras 1 minuto | ~4 s | 2 s | **pide el código** ✓ |
| Tras 5 minutos | ~5 s | 10 s | **vuelve al menú, sin código** ✓ |
| Tras 5 minutos | ~14 s | 10 s | **pide el código** ✓ |

Lo que hace concluyente la tabla es el cruce: con ~4 s "Tras 1 minuto" **sí**
pide y con ~5 s "Tras 5 minutos" **no** — los dos umbrales son de verdad
distintos, no "cualquier espera lo dispara".
