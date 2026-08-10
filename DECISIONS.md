# DECISIONS.md — Registro de decisiones técnicas

Formato: cada decisión con contexto, alternativas consideradas y justificación. Orden cronológico.

---

## D-001 — Rockbox como capa de hardware, no firmware desde cero

**Decisión de partida (no negociable, del brief del proyecto).** Fork de Rockbox aportando kernel, drivers (ATA, LCD, clickwheel, audio, USB, batería), sistema de archivos, códecs y motor de reproducción. La UI de Rockbox (menús, WPS, temas, plugins de usuario) se elimina/desactiva y se reemplaza por la capa propia "Aura UI". El firmware resultante es GPL (obligación heredada de Rockbox).

## D-002 — Fork de Rockbox in-repo, no como submódulo

El código de Rockbox se clona dentro de `/firmware/rockbox` y se versiona como parte de este mismo repositorio (historial propio de Rockbox eliminado, `.git` interno descartado). Motivo: los cambios de Aura tocan el árbol de Rockbox en profundidad (apps/, firmware/, tools/) y un submódulo obligaría a mantener un fork remoto separado; in-repo simplifica clone-and-build.

- **Origen**: espejo oficial `https://github.com/Rockbox/rockbox` (espejo de git.rockbox.org).
- **Commit base**: `0726ec93517a61f602679ab052b083217ec9c96d` (2026-08-09, "FS#13978 - rbutil: Improve GUI responsiveness during voice/talk file generation"). Clonado con `--depth 1`; el `.git` interno se eliminó antes del primer commit para versionar el árbol dentro de este repositorio.

## D-007 — Parche de `tools/configure`: detección de GCC en macOS

El `configure` de Rockbox fuerza `CC=gcc-16` en Darwin (con un comentario `FIXME` sobre `sigaltstack` fallando en macOS ≥ 26.4, motivo por el cual el simulador usa hilos SDL en vez de ucontext). En este equipo Homebrew solo tiene `gcc-15` instalado. Se parcheó `tools/configure` (`firmware/rockbox/tools/configure`, bloque `Darwin)`) para probar `gcc-16`, `gcc-15`, `gcc-14`, `gcc-13` en orden y usar el primero disponible, en vez de fallar con una versión fija. Sigue siendo GCC real de Homebrew (no clang) porque el código de Rockbox depende de extensiones/builtins de GCC no soportadas por el clang de Xcode en este contexto.

## D-008 — Auto-screendump del simulador para evidencia headless

macOS moderno requiere permiso de "Grabación de Pantalla" para `screencapture`/`osascript System Events`, y esta sesión (agente en background, sin interacción del usuario) no puede concederlo interactivamente. Rockbox ya trae `screen_dump()` (`firmware/screendump.c`), que vuelca el framebuffer a un `.bmp` en el disco simulado, normalmente disparado con la tecla F5 dentro de la ventana SDL — inviable de automatizar sin inyección de eventos de teclado a nivel de SO.

**Alternativa implementada**: se añadió a `uisimulator/common/sim_tasks.c` un disparador por variables de entorno — `AURA_SIM_AUTODUMP_TICKS=<n>` programa un `screen_dump()` automático n ticks después del arranque, y `AURA_SIM_AUTODUMP_QUIT=1` cierra el proceso justo después. Envuelto en `firmware/tools/sim_screenshot.sh`. Este mecanismo es además la base de la matriz de capturas (pantalla × tema × modo) requerida en la Fase 7.

## D-003 — Simulador SDL como banco de pruebas principal de la UI

Toda la capa Aura UI se desarrolla y verifica primero contra el simulador SDL de Rockbox en macOS (Fase A del brief), con capturas automatizadas como evidencia. El target ARM real (ipod6g) se compila en la Fase 8. Motivo: ciclo de iteración de segundos vs. minutos, y verificación visual sin hardware.

## D-004 — Tipografía e íconos

- **Inter** (SIL Open Font License) rasterizada a fuentes bitmap en los tamaños exactos de uso — en un LCD 320×240 el render bitmap es más nítido y órdenes de magnitud más barato que el vectorial.
- **Lucide** (licencia ISC) como set de íconos, trazo fino, exportados a bitmap a tamaños exactos.
- **Prohibido**: SF Pro y SF Symbols en el firmware (la licencia de Apple prohíbe su redistribución fuera de plataformas Apple). En Aura Studio (app que corre en macOS) sí se usan SF Symbols, lo cual está permitido.

## D-005 — Aura Studio nativa en Swift + SwiftUI

Sin Electron. Motivos: coherencia estética con macOS, bajo consumo, acceso directo a IOKit/DiskArbitration para el instalador. Target: Apple Silicon.

## D-006 — Formato de video interno: MPEG-1/2 a 320×240

El S5L8702 no puede decodificar H.264/VP9 por software a framerate útil. Rockbox ya trae un decodificador MPEG-1/2 (libmpeg2) probado en este hardware. Aura Studio transcodifica cualquier formato de entrada a este formato interno con ffmpeg; el firmware solo reproduce ese formato.

## D-009 — Fuentes: `convttf` del propio fork, TTF → `.fnt` directo

Rockbox trae `tools/convttf.c`, que rasteriza un TTF a su formato binario nativo `.fnt` en un único paso (`-p <tamaño-px>`), sin pasar por BDF intermedio. Se usa directamente sobre los tres pesos de Inter (Regular/Medium/SemiBold) a los tamaños exactos de `type_scale` (20/14/11/9 px). Requiere `freetype2` (`brew install freetype`, ya presente). El binario compilado (`firmware/rockbox/tools/convttf`) no se versiona — ya está cubierto por el `.gitignore` propio de Rockbox.

## D-010 — Iconos: color pre-compuesto en tiempo de generación, no en runtime

Los SVG de Lucide usan `stroke="currentColor"`; el generador fija `color="<text_primary del tema>"` en el propio SVG antes de rasterizar con `rsvg-convert`, y compone el resultado sobre un fondo sólido al convertir a BMP top-down de 24 bits con `sips`. Se comprobó que el loader de Rockbox (`apps/recorder/bmp.c`) soporta BMP top-down (altura negativa). Se generan dos sets completos de bitmaps — uno por tema — en vez de un único set monocromo tintado en runtime, porque el loader de bitmaps de Rockbox no compone alpha en tiempo de dibujo.

**Actualizado en la Fase 3** (la revisión que este mismo texto anticipaba): el fondo de composición ya no es el `background` del tema sino `TRANSPARENT_COLOR` (magenta `#FF00FF`, la convención de Rockbox en `firmware/export/lcd.h`), y `aura_widgets.c` dibuja los iconos con `lcd_bitmap_transparent()` en vez de `lcd_bitmap()`. Motivo: los iconos de fila se ven tanto sobre el fondo normal como sobre la barra de selección con color de acento, y un fondo horneado fijo se notaba como un recuadro sólido sobre el acento. La marca de "elegido" en las listas de ajustes (Tema/Gráficos/EQ/Idioma) reutiliza el icono `check` por el mismo motivo.

Primer intento (`rsvg-convert -b "#FF00FF" ...` compuesto directo) dejó un halo magenta alrededor de cada icono: `rsvg-convert` antialiasea el trazo, así que los píxeles de borde quedan en un magenta *casi* puro pero no exacto, y `lcd_bitmap_transparent()` compara color exacto — no los trataba como transparentes. Solución: renderizar con canal alfa real (sin `-b`) y umbralizar en Python con Pillow — alfa ≥ 128 vuelca el color de trazo sólido, si no, magenta puro; sin colores intermedios, sin halo. Esto añadió una dependencia nueva: un venv en `design-system/.venv` con Pillow (no cubierto por `fetch_assets.sh`; se crea una vez con `python3 -m venv design-system/.venv && design-system/.venv/bin/pip install pillow`). `firmware/tools/build_sim.sh` usa ese intérprete si existe, si no cae a `python3` del sistema (y `generate.py` falla con un mensaje explícito si falta Pillow).

## D-011 — Salida generada (`design-system/out/`) no se versiona

`tokens.json`, `vendor/` (fuentes Inter y SVG de Lucide descargados, ~1.3 MB) y los scripts se versionan porque son la fuente de verdad. `out/` (headers, `.fnt`, `.bmp`) se excluye del repo vía `.gitignore`: es 100 % derivable de lo anterior con `python3 design-system/generate.py` (determinista, ~segundos), y `firmware/tools/build_sim.sh` lo regenera automáticamente antes de cada build del simulador.

## D-012 — Presets de EQ construidos sobre `eq_defaults` real de Rockbox

`apps/settings_list.c` define `eq_defaults[EQ_NUM_BANDS]` (tipo/frecuencia de corte/Q de cada una de las 10 bandas del ecualizador gráfico real de Rockbox), hasta ahora de uso interno a ese archivo. Se declaró `extern` en `apps/settings.h` (una línea, aditiva, sin cambiar su comportamiento) para que `apps/aura/aura_settings.c` la reutilice: los 4 presets de Aura (Plano/Graves/Voz/Agudos) solo definen la ganancia en dB de cada banda y copian tipo/cutoff/Q de `eq_defaults`, en vez de inventar valores de filtro propios. Al aplicar un preset se escribe `global_settings.eq_band_settings[]` y se llama a `sound_settings_apply()` (la misma función que usa el menú de EQ nativo de Rockbox), así el DSP real queda configurado — el brief pide expresamente reutilizar el backend de Rockbox para esto.

## D-013 — Aura no reutiliza el sistema de idiomas (`.lang`) de Rockbox

Aura oculta por completo la UI de Rockbox, así que ninguna cadena de su interfaz (menús, ajustes, mensajes) coincide con las ~1500 cadenas ya traducidas de Rockbox — reutilizar `apps/language.c` + archivos `.lng` compilados por `genlang` habría sido mucho aparato para un puñado de textos propios. `apps/aura/aura_lang.c` mantiene su propia tabla estática ES/EN (`aura_str(id)`), indexada por `aura_settings.language`; cambiar el idioma en Ajustes es instantáneo, sin releer archivos.

## D-014 — Ajustes de Aura: archivo propio, reutilizando el parser de Rockbox

El tema, modo gráfico, preset de EQ e idioma se guardan en `ROCKBOX_DIR "/aura/aura.cfg"`, un archivo `clave: valor` propio parseado con `settings_parseline()` (la misma función que usa `apps/settings.c` para `config.cfg`) en vez de con el `configfile.c` de `apps/plugins/lib/`, que depende de la tabla de indirección `rb->` exclusiva de plugins cargables — Aura se compila dentro del binario principal, no como plugin. El brillo (`global_settings.brightness`) y el motor de EQ (D-012) sí reutilizan directamente el backend de ajustes existente de Rockbox, tal como pide el brief; solo las 4 preferencias que son enteramente nuevas de Aura necesitan su propio archivo.

## D-015 — `aura_main()` reemplaza a `root_menu()`, no lo borra

`apps/main.c` invocaba `root_menu()` (NORETURN) como última llamada de `main()`. Se cambió esa única línea a `aura_main()`; `apps/root_menu.c` sigue compilado (otros ajustes como el "start screen" aún lo referencian por callback) pero ya no se invoca nunca, así que la UI de Rockbox queda inalcanzable para el usuario sin tocar ese código. Coherente con "elimina/desactiva" del brief: desactivar es más seguro que borrar cuando hay referencias cruzadas dispersas por el árbol de Rockbox.

## D-016 — Backdrop del tema por defecto y modo de dibujo de texto

Dos correcciones descubiertas al verificar la primera pantalla en el simulador (ver `docs/screenshots/fase3-root-dark.png`):

- El backdrop del tema por defecto de Rockbox (`cabbiev2.bmp`) queda registrado vía `lcd_set_backdrop()` desde antes de que arranque Aura, y `lcd_clear_display()` lo respeta en vez de pintar un color plano. `aura_theme_init()` llama `lcd_set_backdrop(NULL)` para desactivarlo.
- `lcd_putsxy()`/`lcd_bitmap()` con el `DRMODE_SOLID` por defecto rellenan también el fondo de su caja delimitadora con el color de fondo *registrado* (no con lo que ya hay dibujado debajo). Como ese registro no se actualizaba fila a fila, el texto de la fila resaltada con el color de acento se dibujaba como un rectángulo opaco del color de fondo del tema, tapando la propia barra de selección. `aura_theme_clear_screen()` fija `lcd_set_drawmode(DRMODE_FG)` (pinta solo los píxeles de trazo, deja el resto intacto) para todas las pantallas de Aura.

## D-017 — Inyección de botones en el simulador para verificación headless

Verificar la navegación real (no solo que el binario compile) requiere simular pulsaciones del clickwheel, pero esta sesión no tiene permisos de Accesibilidad de macOS para enviar eventos de teclado a la ventana SDL. Se extendió el mismo mecanismo de auto-screendump (D-008) en `uisimulator/common/sim_tasks.c`: la variable de entorno `AURA_SIM_BUTTONS` (lista separada por comas: `SELECT,MENU,SCROLL_FWD,SCROLL_BACK,PLAY,LEFT,RIGHT`) inyecta cada botón directamente en la cola global `button_queue` vía `button_queue_post()` — la misma cola que llena el driver real de teclado/clickwheel — con un pequeño delay entre presión y suelta, y entre botones sucesivos. El screendump automático se reprograma para disparar cuando termina de inyectar toda la secuencia. `firmware/tools/sim_screenshot.sh` expone esto como un tercer argumento opcional. Usado para verificar en la Fase 3 navegación raíz→Ajustes→pantallas de elección→aplicar+volver, cambio de tema e idioma en vivo, y el botón MENU; será la base de la matriz de capturas de la Fase 7.

## D-018 — Brillo por defecto = máximo en el simulador (no es un bug)

Al verificar la pantalla de Brillo, el valor inicial mostrado fue 63/63 (máximo) en vez del `DEFAULT_BRIGHTNESS_SETTING` de `ipod6g.h` (0x20 = 32). Investigado con un `#error` temporal para volcar el valor efectivo del macro en la compilación real: `firmware/export/config/sim.h` redefine deliberadamente `DEFAULT_BRIGHTNESS_SETTING` a `MAX_BRIGHTNESS_SETTING` para *todo* build de simulador (sin backlight físico que atenuar, tiene sentido arrancar la ventana siempre visible). Es comportamiento correcto y preexistente de Rockbox, no un defecto de Aura; en el target real `ipod6g` (Fase 8) `sim.h` no aplica y el valor por defecto vuelve a ser 32.

## D-019 — Parser `.lrc` propio, puro y sin asignación dinámica

`apps/aura/aura_lrc.c` es C99 autocontenido (solo `<string.h>`), sin `malloc`: el llamador reserva `aura_lrc_t` de tamaño fijo (`AURA_LRC_MAX_LINES=600` líneas de hasta 127 caracteres). Igual que `aura_nav.c`, compila y se testea nativo en el host (`apps/aura/test/test_lrc.c`, 45 checks) sin tocar Rockbox. Soporta múltiples timestamps por línea (coros repetidos), tags de metadata (`[ar:]`, `[ti:]`, etc., ignorados), fracciones de 1/2/3 dígitos, entrada desordenada (se ordena al parsear) y corchetes sin cerrar (se descartan sin crashear). `aura_lrc_find_active_line()` hace búsqueda binaria por timestamp — barato de llamar en cada frame de la pantalla de letras.

## D-020 — Fixtures de audio de prueba generadas con ffmpeg, no versionadas

`firmware/tools/gen_test_media.sh` genera tonos sintéticos de 3s (440 Hz) en los 6 formatos nativos (FLAC, MP3, AAC/m4a, ALAC, WAV, AIFF) más un `.lrc` de muestra, en `firmware/test-media/` (gitignored, igual que `design-system/out/`: 100 % reproducible bajo demanda, y son binarios sin valor como diff). Se usan para probar la reproducción real en el simulador sin depender de música del usuario ni de assets con licencia ajena.

## D-021 — Música: envoltorio de dominio sobre tagcache/playlist/audio, no una reimplementación

`apps/aura/aura_music.c` es la única pieza de Aura que decide *qué* consultar y *cuándo*; toda la lógica real (índices, búsqueda, reproducción) es la de Rockbox. Decisiones puntuales:

- **Filtros de navegación fuera de `aura_nav`**: `aura_nav` es genérico (pila de pantallas + selección), no sabe qué es un artista. El artista/álbum/género elegido al entrar a una pantalla hija vive en variables propias de `aura_music.c` (`s_artist_seek`, etc.), con un contador de generación (`aura_music_filter_generation()`) que `aura_screens.c` usa como parte de la clave de caché de sus listas — sin él, re-entrar a "Álbumes por artista" con un artista *distinto* mostraría en caché los álbumes del artista anterior, porque el id de pantalla no cambia entre una visita y otra.
- **Orden de `#include`**: `tagcache.h` (y `recorder/albumart.h`) chequean `#ifdef HAVE_TAGCACHE`/`HAVE_ALBUMART` *antes* de incluir `config.h` ellos mismos (esa macro la define `config.h` vía `config/ipod6g.h`). Si son el primer header de un archivo, la macro todavía no existe y todo su contenido desaparece en silencio sin ningún error de compilación obvio — la falla se ve despues, como funciones "implicitamente declaradas". `aura_music.c` incluye `config.h` primero explícitamente por esto.
- **`tagcache_start_scan()` no sirve para el build inicial**: su manejador en `tagcache.c` hace `if (!tc_stat.ready) break;` antes de escanear nada — solo re-verifica una base *ya construida*. `aura_music_db_ready()` usa `tagcache_rebuild()` en su lugar, que no tiene esa condición, y solo la dispara una vez que `tagcache_is_fully_initialized()` confirma que Rockbox ya determinó (de forma asíncrona, tarda ~1s) si una base válida existe en disco — dispararla antes reconstruía (¡borraba!) una base perfectamente válida en *cada* arranque, porque la determinación de "¿ya existe?" corre en un hilo de fondo y no está lista todavía en el primer chequeo.
- **Aura dispara el escaneo, no un menú**: en Rockbox original esto lo hace el usuario a mano desde "Base de datos > Inicializar ahora" en el navegador de archivos, pantalla que Aura no tiene. `aura_music_db_ready()` lo dispara sola la primera vez que se llama — y `aura_main()` la llama en cada vuelta del bucle principal (no solo al entrar a Música), así el escaneo arranca solo al boot, como en un iPod real, y las pantallas de navegación se refrescan solas cuando termina (ver D-022) en vez de quedar mostrando "Preparando la biblioteca..." indefinidamente hasta el próximo botón.
- **Playlists son un catálogo de archivos, no tagcache**: `apps/playlist_catalog.c` las lista recorriendo un directorio (`.m3u`/`.m3u8`) con `opendir`/`readdir`, no con una búsqueda de tagcache — `aura_music_list_playlists()` hace lo mismo directamente en vez de usar `catalog_view_playlists()` (que abre la navegación de archivos propia de Rockbox).

## D-022 — Redibujo periódico condicional en el bucle principal

`aura_main()` bloquea indefinidamente en `button_get(true)` salvo en dos casos, donde usa `button_get_w_tmo(HZ/2)` para refrescar la pantalla sin que el usuario haya tocado nada: (1) en Ahora Suena mientras algo esta sonando y no en pausa, para que la barra de progreso avance; (2) mientras `!aura_music_db_ready()`, para que las pantallas de música dejen de mostrar "Preparando la biblioteca..." en cuanto el escaneo (D-021) termina. Sin esto, el timeout de auto-screendump (D-008/D-017) capturaba sistemáticamente el primer frame -- estale -- dibujado justo al entrar a la pantalla, no el estado real varios segundos despues, dando la falsa impresión de que el escaneo nunca terminaba.

## D-023 — Parche a `tagcache.c`: buffer de commit sin dircache ni base previa

El primer commit de tagcache de la vida de una base de datos necesita un buffer temporal grande (`tempbuf`) para construir los índices. `commit()` intenta conseguirlo de dos formas: robándolo de dircache (`HAVE_DIRCACHE`, indisponible en `SIMULATOR`, ver `firmware/export/config.h`) o robándolo del ramcache ya asignado (`allocate_tagcache()`, que a su vez necesita un *master file ya existente en disco* para calcular cuánto reservar — no sirve para el primerísimo build). En el simulador, sin ninguna de las dos, `tempbuf_size` se queda en 0 y `commit()` pospone el commit "hasta el próximo arranque" — indefinidamente, porque ningún arranque futuro cambia esa situación. Diagnosticado con `fprintf` temporal en `commit()` (ver historial de esta sesión) tras descartar `write_lock` como causa.

**Fix**: en `commit()`, si ninguna de las dos vías anteriores aportó un buffer, se llama a `allocate_tempbuf()` directamente (la misma función que ya usan las otras dos vías por detrás) — es una función general que solo pide memoria del pool común, sin depender de dircache ni de una base previa. Parche de una función, general (no específico de Aura ni del simulador), documentado en el propio archivo. Complementado con `global_settings.tagcache_ram = 1` forzado en ambas variantes de `init()` de `apps/main.c` (D-001/D-014 ya tocan ese archivo): en el dispositivo real esto evita giros de disco repetidos al navegar la biblioteca (alineado con el objetivo de batería del proyecto), aunque ahí el bug de arriba no se manifiesta porque `HAVE_DIRCACHE` sí está disponible.

## D-024 — Transiciones centralizadas por profundidad de pila, no por punto de push/pop

`apps/aura/aura_transitions.c` anima la entrada a una pantalla nueva desplazándola desde fuera de encuadre, usando el sistema de `struct viewport` de Rockbox (`lcd_set_viewport()` desplazado en X, restaurado después de cada cuadro) — no manipula el framebuffer a mano. Sin transición en modo Ultra minimalista; en Minimalista y Completo, la única diferencia es cantidad de cuadros/duración (4 cuadros ~90ms vs. 8 cuadros ~130ms).

En vez de llamar a la transición desde cada `case BUTTON_SELECT`/`BUTTON_MENU` de cada pantalla (muchos puntos, fácil de olvidar uno), `aura_screens_handle_button()` mide `aura_nav_depth(nav)` antes y después de despachar el botón a su manejador normal, y dispara la transición una sola vez al final si la profundidad cambió — la dirección (entrar/volver) se deduce del signo del cambio. Un único punto de verdad para "cuándo hubo navegación", sin tocar cada pantalla individualmente.

## D-025 — Modo Completo: simplificaciones deliberadas de Coverflow y home dividida

El brief pide Coverflow "fluido... con reflejos... calculados una vez por carátula, no en tiempo real" y permite explícitamente simplificar antes que forzar framerate insostenible en el ARM926EJ-S real. Decisiones tomadas en esa dirección:

- **Sin perspectiva 3D real**: las carátulas se dibujan planas, todas al mismo tamaño de decodificación (56 px), diferenciadas por atenuación de color hacia el fondo del tema (la central a brillo completo con marco de acento, las laterales atenuadas ~50%) en vez de escalar geométricamente — escalar exigiría decodificar cada carátula a *dos* tamaños distintos y volver a decodificar cada vez que cambia de posición central↔lateral, un costo que no se justifica frente al resultado visual.
- **"Velocidad" del clickwheel aproximada por cadencia, no por señal cruda**: Aura no expone el flag `BUTTON_REPEAT` fuera de `aura_main.c` (D-022 lo descarta para toda la UI), así que Coverflow mide el tiempo entre dos eventos de scroll consecutivos (`current_tick`); si llegan separados por menos de `HZ/6`, avanza de a dos álbumes en vez de uno. Aproximación barata de "más rápido si giras más rápido" sin necesitar la señal de repetición cruda del hardware.
- **Reflejo pre-renderizado una sola vez por carátula** (`aura_albumart.c`): al decodificar, se genera inmediatamente un segundo bitmap (mitad de alto, espejado verticalmente, con degradado lineal hacia el color de fondo del tema) que se cachea junto al original — el degradado nunca se recalcula por cuadro.
- **Home dividida solo si hay contenido**: si ningún álbum tiene carátula localizable, `aura_home_has_content()` devuelve `false` y la raíz usa la lista simple de siempre — una pantalla dividida con la mitad de arriba vacía no aporta nada (ver D-014, "ante la duda, lo más simple").
- **Carátula por álbum reutilizando `find_albumart()`**: esa función (la misma que usa Ahora Suena) exige un `struct mp3entry` con `.path`/`.album`/`.artist`; para un álbum elegido en Coverflow/home (no la pista en reproducción) se construye un `mp3entry` mínimo en memoria (`memset` a cero + esos tres campos apuntando a strings ya obtenidos de tagcache) tomando cualquier pista de ese álbum — se confirmó leyendo `search_albumart_files()` que no toca ningún otro campo del struct, así que es seguro.

## D-026 — `FORMAT_RESIZE` necesita un buffer de trabajo, no solo el tamaño final

Al decodificar la primera carátula de prueba, `read_bmp_file()`/`read_jpeg_file()` devolvían 0 (fallo) con un buffer dimensionado exactamente a `size*size*sizeof(fb_data)` — el tamaño *final* de la imagen. `FORMAT_RESIZE` necesita además espacio de trabajo para el remuestreo (ver `BM_SCALED_SIZE` en `apps/recorder/bmp.h`: contempla ~9× el ancho en `uint32_t` adicionales). `aura_albumart.c` decodifica en un buffer de escritorio genérico de 64 KB (con margen de sobra para cualquier tamaño de carátula que use Aura) y copia solo los `size*size` píxeles finales — que quedan al comienzo del buffer — al destino real.

## D-027 — Bug real de desincronización de selección entre el home dividido y la lista plana (no solo un artefacto de pruebas)

Al verificar la navegación en modo Completo con la inyección de botones (D-017), la selección visible se "adelantaba" de forma reproducible: cuatro pulsaciones de avance terminaban en el tercer ítem en vez del quinto. Causa raíz: `aura_home_has_content()` no puede confirmar que hay contenido hasta que `aura_music_db_ready()` lo confirma de forma asíncrona (D-021) — durante esa ventana (los primerísimos cuadros tras el arranque, típicamente menos de un segundo pero real en *todo* arranque, no solo en pruebas), `is_split_home_screen()` devuelve `false` y los botones se despachan al manejador de la lista plana, que llevaba su cuenta de selección en `aura_nav` (`aura_nav_get_selection`/`set_selection`) — mientras que `aura_home.c` llevaba la suya en una variable estática propia (`s_selected`). En cuanto la pantalla dividida se activaba, mostraba su contador (atrasado) en vez del de `aura_nav` (correcto), y los botones ya procesados por el otro camino parecían "perdidos".

No era un artefacto de la inyección de botones: la ventana de carrera ocurre en cualquier arranque real, con clickwheel físico o no; solo hacía falta pulsar durante esa ventana para reproducirlo. **Fix**: `aura_home.c` dejó de llevar su propio contador — lee y escribe la selección directamente en `aura_nav_get_selection(nav)`/`aura_nav_set_selection(nav)`, exactamente como la lista plana. Con un único contador de verdad, no importa cuál de los dos caminos despache un botón dado: el estado visible siempre coincide. Lección general para cualquier pantalla futura que tenga una variante "enriquecida" y una "simple" del mismo lugar en la navegación: comparten el contador de selección de `aura_nav`, nunca uno propio.

## D-028 — Visor de fotos: JPG/BMP decodificados, PNG/GIF listados pero no abribles

El brief pide "JPG/PNG/GIF" para el visor de imágenes. Los decoders de PNG y GIF de Rockbox (`apps/plugins/png.c`, `apps/plugins/gif.c` vía sus `.ovl`) están escritos exclusivamente contra la tabla de indirección `rb->` de plugins cargables (igual que `mpegplayer`, ver D-029) y no exponen una función tipo `read_png_file()`/`read_gif_file()` linkeable directamente al binario principal como sí lo hacen `read_bmp_file()` y `read_jpeg_file()` (ambos viven en `apps/recorder/`, pensados para uso desde el core — así los usa el propio Rockbox para carátulas). Portar esos dos decoders del modelo de plugin al core está fuera de alcance de esta fase.

**Alternativa implementada**: `aura_photos.c` lista `.jpg/.jpeg/.bmp/.png/.gif` en el navegador (para que el usuario vea todos sus archivos), pero solo marca `.jpg/.jpeg/.bmp` como `supported`. Al abrir un archivo no soportado, el visor muestra el mensaje "Formato no soportado" en vez de intentar decodificar o crashear. Aura Studio (Fase 10) debe convertir PNG/GIF a JPG o BMP al sincronizar, dejando el dispositivo con solo formatos que realmente puede mostrar — coherente con cómo ya se resuelve el video (ver D-029).

## D-029 — Reproducción de video delegada al plugin `mpegplayer` del propio fork

El brief limita el video a "un formato interno optimizado (MPEG-1/2 a 320×240) generado por la app de escritorio" — el dispositivo nunca decodifica H.264/VP9/contenedores de terceros. Escribir un reproductor de video propio dentro de Aura (en vez de reusar código) contradice el principio rector del proyecto (fork de Rockbox como capa de hardware, no reescribir lo que ya funciona). Rockbox ya trae `apps/plugins/mpegplayer/`, un reproductor MPEG-1/2 completo y optimizado para estos SoCs.

**Alternativa implementada**: `aura_video.c` lista `.mpg/.mpeg` en `/Videos` con la lista genérica de Aura, y al pulsar SELECT llama `plugin_load(VIEWERS_DIR "/mpegplayer.rock", path)`. `plugin_load()` es una llamada bloqueante que se ocupa por sí sola de guardar/restaurar pantalla, viewport y fuente antes y después — Aura no necesita ningún paso extra al volver, el siguiente ciclo de `aura_main()` redibuja la pantalla de Videos normalmente. El usuario nunca ve la UI de `mpegplayer` como algo ajeno a Aura porque no tiene menús propios visibles en este flujo (se invoca directo con la ruta del archivo, sin pasar por su selector de archivos). Verificado en el simulador con un fixture de 2s generado por `gen_test_media.sh`: `plugin_load()` retorna sin crash y Aura recupera el control normalmente.

## D-030 — Rediseño de `aura_transition_slide()`: de viewport desplazado a "wipe" de revelado, siempre dentro de límites

**Bug real encontrado en esta fase** (no solo en el visor de fotos — la ruta de código es compartida por *toda* navegación en modos Minimalista/Completo): la transición original desplazaba un `struct viewport` de `AURA_SCREEN_WIDTH` completo (320px) en X, cuadro a cuadro, para simular la pantalla entrando desde fuera de encuadre. Durante los cuadros intermedios eso produce necesariamente `vp.x < 0` o `vp.x + vp.width > LCD_WIDTH` (p. ej. `x=240, width=320` → `x+width=560`). `LCDFN(set_viewport_ex)` (`firmware/drivers/lcd-bitmap-common.c`) solo **reporta** ese caso por `DEBUGF` en builds `SIMULATOR` — no lo recorta ni lo rechaza; el viewport inválido igual se instala como `current_viewport` y se usa para dibujar. Un blit grande dentro de él (una foto a pantalla completa en el visor nuevo de esta fase) escribe fuera del framebuffer real → bus error (exit 138). El bug era preexistente desde la Fase 5 (D-024) pero no se había manifestado porque ningún dibujo anterior (listas, Coverflow de 56px, home de 96px) era lo bastante grande/ancho para pisar memoria fuera de rango durante la ventana de la animación.

En el modelo de viewports de Rockbox, `vp.x`/`vp.width` cumplen doble función: origen de traducción de coordenadas para todo lo dibujado dentro **y** límite de recorte. Eso significa que no se puede simplemente recortar el ancho visible de un viewport desplazado sin también romper la traducción de las coordenadas del contenido dibujado dentro (recortar `vp.width` sin tocar `vp.x` cambia qué se ve, pero cambiar `vp.x` para mantenerlo en rango cambia *dónde* aparece el contenido). No hay forma de "clampear" un viewport desplazado que preserve el efecto de deslizamiento sin ese problema.

**Fix implementado**: se reemplazó el desplazamiento por un "wipe" de revelado. Se dibuja la pantalla nueva en su posición normal (coordenadas sin desplazar, siempre dentro de límites), pero acotada cada cuadro a una franja que crece desde 0 hasta el ancho completo — desde el borde derecho hacia la izquierda si la navegación es hacia adelante (`vp.x = ANCHO - revelado, vp.width = revelado`), o desde el borde izquierdo hacia la derecha si es hacia atrás (`vp.x = 0, vp.width = revelado`). En ambos casos `vp.x` y `vp.x + vp.width` quedan siempre dentro de `[0, AURA_SCREEN_WIDTH]` por construcción — no hace falta clampear nada porque el algoritmo nunca genera un rango inválido. El framebuffer conserva la pantalla anterior en la parte todavía no cubierta (no se redibuja ni se limpia), dando el mismo efecto visual de "entra deslizando" sin jamás instalar un viewport fuera de rango. Verificado: la captura repro original (`SCROLL_FWD,SCROLL_FWD,SELECT,SELECT` → abrir una foto) ya no crashea, y una revisión rápida de humo confirmó que la navegación en Ajustes, Música y modo Completo (home dividida/Coverflow) sigue funcionando sin regresiones.

## D-031 — El decoder JPEG de Rockbox no soporta el muestreo de croma no estándar que emite `ffmpeg` por defecto

Al verificar visualmente el visor de fotos (no solo "no crashea", sino "¿la imagen se ve bien?"), una imagen de prueba generada con `ffmpeg -f lavfi -i testsrc ... foto.jpg` (sin más flags) decodificaba "con éxito" (`read_jpeg_file()` devuelve `ret>0`, con las dimensiones correctas) pero el resultado era ruido disperso, no la imagen real. Se descartaron por evidencia directa, en este orden: el buffer de trabajo (D-026, de sobra: 240 KB reservados contra 165 KB necesarios), un bug en `lcd_bitmap()`/viewports (se volcó el buffer decodificado crudo a un BMP y se abrió *sin pasar por Rockbox* — el mismo ruido ya estaba ahí, en el buffer que devuelve el decoder) y un bug de mi propio código llamador (mismo patrón exacto que `aura_albumart.c`, que sí funciona con carátulas reales).

Comparando los marcadores JPEG de la imagen que falla contra una generada con `sips` (que sí decodifica bien): la de `ffmpeg` sin flags usa el mismo factor de muestreo `(1,2)` para los 3 componentes (Y, Cb, Cr) compartiendo una sola tabla de cuantización — una disposición atípica (sin submuestreo de croma relativo, pero tampoco 4:4:4 con tablas separadas como cualquier codificador convencional). El decoder de Rockbox (`apps/recorder/jpeg_load.c`, pensado para hardware embebido, no para cubrir cualquier combinación de muestreo posible) no lo interpreta bien. Agregar `-pix_fmt yuvj420p` al comando de `ffmpeg` fuerza submuestreo 4:2:0 estándar con tablas separadas (lo que produce cualquier codificador JPEG convencional) y el mismo archivo decodifica perfectamente. Confirmado también que la carátula de prueba de Música (`cover.jpg`, generada con `ffmpeg` sin este flag desde la Fase 4) probablemente tenía el mismo defecto — al ser un color sólido, el "ruido" resultante también sale prácticamente plano y pasó inadvertido a simple vista.

**No es un bug de Aura ni de mi código de llamada** — es una incompatibilidad real, específica del decoder JPEG minimalista de Rockbox frente a una salida no estándar (aunque técnicamente válida) del encoder MJPEG de `ffmpeg` en ciertas condiciones. Arreglar el decoder para soportar cualquier combinación de factores de muestreo está fuera de alcance (cambio profundo a un decoder maduro y usado en producción, para un caso límite). **Alternativa implementada**: `firmware/tools/gen_test_media.sh` agrega `-pix_fmt yuvj420p` a *todo* comando `ffmpeg` que genera un JPEG (carátula y fotos de prueba). **Implicación para Aura Studio (Fase 10)**: cuando la app de escritorio use `ffmpeg` para generar/optimizar carátulas o fotos JPEG, debe invocarlo con `-pix_fmt yuvj420p` (o equivalente) para garantizar compatibilidad con el dispositivo — queda anotado para no repetir este hallazgo en esa fase.

---

*(Las siguientes decisiones se añaden conforme avanza la ejecución.)*
