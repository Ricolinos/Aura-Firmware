# PLAN-themes-impl — Sistema de temas: firmware + Aura Studio (Fase 1: investigación y plan)

> **ESTADO: EJECUTADO PARCIALMENTE** — 2026-08-17. Histórico. No es trabajo pendiente.
> Decisiones en `DECISIONS.md` (D-289) y en Aura Studio (ST-003…ST-005).
> Sin ejecutar, a propósito: `accent_default`/`accent_presets` del manifiesto
> (el firmware los acepta pero no los lee — `CONTRATO-formato-tema.md` §H) y la
> Fase 2B de Aura Studio (rasterizador nativo de fuentes/íconos del sistema);
> ambos registrados en D-289 y en `CONTRATO-firmware-studio.md` (pendientes).

**Fecha: 2026-08-17. Estado: PENDIENTE DE APROBACIÓN DEL DUEÑO. Nada modificado en ningún repo** (este archivo es el único escrito, sin commit).

Continúa y aterriza `PLAN-theme-system.md` (especificación del 2026-08-16, hoy solo en el repo archivado `Aura-Proyect`; en Fase 2 se copia aquí como `docs/aura-design-system/sistema/05-temas.md` + este plan). **Toda afirmación de viabilidad de abajo fue re-verificada contra el código actual de `Aura-Firmware` (HEAD `6e73074`, post-D-286) y de `Aura-Studio` (HEAD `4082834`)** — donde la especificación y el código difieren, gana el código y lo señalo.

Principio rector vigente: **Aura Studio es un CONSTRUCTOR de temas, no un DISTRIBUIDOR.** El material de Apple sigue en `~/Aura-local/theme-apple-source/`, fuera de todo repo.

---

## 1.0 — VEREDICTO DE VIABILIDAD EN RUNTIME (bloqueante)

### ✅ VIABLE. Modelo híbrido confirmado: estructura en compilación, apariencia (paleta + fuentes + íconos + fondos) en tiempo de ejecución. Sin recompilar el firmware para instalar o cambiar un tema.

Evidencia, elemento por elemento:

| Elemento | Hoy en el código | ¿Runtime? | Coste RAM | Coste tiempo | Evidencia |
|---|---|---|---|---|---|
| **Fuentes** (14 roles) | Ya se cargan **en runtime desde disco** por ruta: `font_load(paths[i])` con `paths[] = FONT_DIR "/" A26_FONT_*` — la ruta es una constante de compilación, pero `font_load()` acepta cualquier `const char*` (`font.c:611` → `font_load_ex` → `open(path)`). | **Sí** — sustituir `paths[]` por rutas construidas con `snprintf` desde el directorio del tema es un cambio local en `apple2026_shell.c:34-58`. | **Igual que hoy**: cada `.fnt` de Aura (233–631 KB) supera `MAX_FONT_SIZE = 60000` (ipod6g: LCD 240 px, 64 MB) → todas van en modo *cached* (LRU de 256 glifos + `fd` abierto). ≤ 14 × ~60 KB ≈ **0.84 MB, ya pagados hoy, independientes de la cara**. Cambiar de tema no añade RAM. | Arranque: **+0** (mismas 14 cargas, otro directorio; el `.cfg` del tema son bytes). Cambio en caliente: 14 × `font_unload` (cada una guarda su `.gc` de glifos: 14 escrituras pequeñas) + 14 × `font_load` → **decenas de ms en flash, <0.5 s en HDD** (el disco ya gira). | `apple2026_shell.c:34-58`; `font.c:62-70, 417-586, 611, 616-651`; `font.h:64` |
| Límite duro de fuentes | `MAXUSERFONTS = 14`, **exactamente** los 14 estilos de Aura (`font.h:55-64`, subido 12→13→14 para Aura). `font_unload()` **no se llama en ningún sitio de `apps/aura` hoy**. | Un tema **sustituye la cara de cada rol**, nunca añade roles. El cambio en caliente es obligatoriamente **descargar-y-recargar** (no cargar al lado). | 14 de 31 `fd` abiertos (`MAX_OPEN_FILES = 31`), constante — descargar antes de cargar mantiene el conteo. | — | `font.h:64`, `fs_defines.h:41-45` |
| **Íconos** | **Ya se leen del disco en cada dibujo** (`read_bmp_file()` a `icon_buf`, 14 464 B compartidos, sin caché en RAM). El camino **primario** para todos los íconos es la **máscara de cobertura** (`draw_icon_mask_2`, `aura_widgets.c:167`) compuesta contra el framebuffer con la tinta del token vivo; los BMP horneados `light/`/`dark/` son **solo fallback** si falta la máscara (`aura_widgets.c:258`). | **Sí** — 5 sitios construyen rutas bajo `ICON_DIR "/aura/…"` (`aura_widgets.c:167, 258`; `aura_selection_summary.c:198`; `aura_screens.c:1780`; `aura_albumart.c:292`): basta un prefijo de directorio resuelto por tema. | **0** | **Igual que hoy** por cuadro (mismo `read_bmp_file` + mismo blend). | `aura_widgets.c:105-106, 160-274` |
| Peso de íconos por tema | Hoy en `rockbox.zip`: `masks/` 801 BMP = **5.2 MB**; `light/`+`dark/` 8010 BMP = **52 MB** (fallback puro). | Consecuencia de diseño: **un tema v1 solo necesita las 801 máscaras** (5.2 MB); los horneados son opcionales. Un tema completo pesa **~11.5 MB** (6.2 MB fuentes + 5.2 MB máscaras + fondos/tiles) en vez de 63 MB. | — | — | conteo real de `design-system/out/icons/` |
| **Paleta de color** | Compilada (`A26_COLOR_LIGHT_*`/`DARK_*` en `apple2026_tokens.h:79-100`) pero **el 100 % pasa por `a26_color()`** (`apple2026_shell.c:75-108`): **cero usos** de `A26_COLOR_*` fuera de la shell. `a26_color()` re-lee `aura_settings.theme` **en cada llamada** (no hay paleta cacheada que invalidar). El acento **ya es runtime** (`aura_accent()` lee `aura_settings.accent_rgb24`; `a26_color(A26_ACCENT)` lo devuelve — precedente completo "ajuste → paleta viva → siguiente cuadro"). | **Sí** — tabla `s_palette[2][A26_TOKEN_COUNT]` inicializada con los defines actuales (default compilado) y sobreescrita desde `theme.cfg`. Fuera del embudo quedan **~23 usos directos**: 9 del blanco constante del Selector (estructural, se queda), 2 de `CATEGORY_EXTRAS_YELLOW` (barra de almacenamiento), 1 tabla de presets de acento, 1 default de acento, y ~11 `LCD_RGBPACK` derivados de arte/sombras (no son paleta). Los de categoría y presets pasan a la tabla; el resto no cambia. | **<1 KB** | **0** por cuadro (una indirección de tabla en vez de un `switch`). El redibujo es automático: el bucle principal redibuja pantalla completa cada iteración (`aura_main.c:423, 443`). | `apple2026_shell.c:75-114`; grep de `A26_COLOR_`/`AURA_DS_COLOR_`/`LCD_RGBPACK` en `apps/aura` |
| Fondos de panel y tile-icons | Runtime desde disco, cacheados en RAM una vez (`s_bg_pixels` 160×240×2 B = 76.8 KB; `s_aura_badge_pixels` 90×90×2 = 16.2 KB). | **Sí**, solo cambia la ruta. **Dos cachés a invalidar** al cambiar de tema: `s_aura_badge_theme` (`aura_screens.c:1753`, keyed por claro/oscuro) y `s_bg_loaded_name` (`aura_selection_summary.c:194`, **compara por identidad de puntero** — con nombres construidos en runtime recargaría en cada llamada; hay que pasarlo a comparación por contenido o a un contador de generación de tema). | Igual | Igual | — |
| **Estructura** (radios, espaciado, layout, timings, tamaños de buffer) | 123 defines / 394 usos; dimensionan buffers estáticos (`s_bg_pixels[BG_W*BG_H]`, `s_aura_badge_pixels`, `icon_buf`, `s_slots[CF_CACHE_SLOTS]`…). | **No** (híbrido). Un tema no la toca. | — | — | `apple2026_tokens.h` |
| Manifiesto | — | Mismo parser que `aura.cfg`: `read_line()` + `settings_parseline()` (`aura_settings.c:217-220`); claves desconocidas se ignoran (compatibilidad hacia adelante ya probada). | bytes | despreciable | `aura_settings.c:207-321` |
| Orden de arranque | `aura_settings_load()` (paso 3) corre **antes** de `a26_shell_init()` (paso 7, las 14 `font_load`) — `aura_main.c:282, 318`. | El `theme_id` leído de `aura.cfg` ya está disponible cuando se cargan fuentes. Cero reordenamiento. | — | — | `aura_main.c:269-333` |

**Presupuesto del dispositivo**: 64 MB; `PLUGIN_BUFFER_SIZE` 2 MiB, `CODEC_SIZE` 1 MiB (`ipod6g.h:153-154`), el resto es `audiobuf`. Un tema activo cuesta **~0 MB adicionales**: las fuentes ya están pagadas, los íconos no se cachean, y la paleta es <1 KB. Cambiar de tema no toca `audiobuf` (la reproducción sigue).

**Lo que la especificación decía y el código matiza** (sin invalidarla):
1. La spec asumía que los BMP horneados por modo eran parte obligatoria del tema (N.5 §4). El código muestra que la máscara es el camino primario y los horneados son fallback → **el formato v1 exige solo máscaras**; los horneados quedan opcionales (Q3). Esto además **elimina para el constructor** el problema heredado de N.5 (halo/dientes por clave magenta): una máscara es la cobertura misma, sin composición ni magenta.
2. La spec proponía `themes/default/` en disco además del default compilado. El código hoy tiene el default en las rutas legadas (`/.rockbox/fonts/`, `/.rockbox/icons/aura/`) y Studio usa `.rockbox/fonts/a26-title-20.fnt` como sentinela → **recomiendo que el tema por defecto siga en las rutas legadas** (id implícito `default`), sin mover nada ni cambiar `rockbox.zip` (Q2).
3. No existe ningún `AURA_VERSION`/`AURA_THEME_FORMAT` en el firmware ni versión visible en "Acerca de" (`grep` = 0 hits): el gating por formato hay que crearlo desde cero (§1.2 y §1.5).

**Riesgos reales, acotados**: (a) el cambio en caliente añade el primer uso de `font_unload` en Aura — hay que respetar el refcount (una fuente puede estar cargada dos veces si dos roles apuntan al mismo archivo: `pro_bold` y `pro_semibold` comparten `Inter-SemiBold` **como TTF de origen**, pero generan `.fnt` distintos, así que hoy son 14 archivos distintos; un tema podría legítimamente apuntar dos roles al mismo `.fnt` → `font_load` devuelve el mismo id con refcount 2, y `font_unload` ×2 lo libera correctamente — se documenta como caso soportado); (b) `MAX_PATH` en las rutas de íconos (`/.rockbox/aura/themes/<id>/icons/masks/<name>-<px>.bmp` con `<id>` ≤ 32 caracteres cabe con holgura en `MAX_PATH = 260`); (c) en HDD, un tema cuyas fuentes estén fragmentadas puede hacer el primer dibujo tras el cambio visiblemente más lento (LRU de glifos frío) — es el mismo comportamiento que el arranque hoy.

**Bug preexistente encontrado en la investigación (no del sistema de temas, pero toca exactamente sus assets)**: `firmware/tools/package_dist.sh:77-80` busca `design-system/out/icons/{backgrounds,tile-icons}` pero el pipeline los escribe en `design-system/out/icons/aura/{backgrounds,tile-icons}` → **el `rockbox.zip` del release sale sin fondos de panel ni tile-icons** (el simulador sí los instala vía `build_sim.sh:64-68`, por eso no se notó). En el iPod real, el panel derecho de SelectionSummary no tiene fondo por preset y el badge de "Acerca de" cae al ícono `ipod`. Se corrige en Fase 2 (una línea).

---

## 1.1 — Firmware: submenú de Temas

### Ubicación en el árbol (`sistema/03-arbol-de-menus.md`)

El encargo dice **Extras**. Lo diseño ahí, pero **recomiendo Ajustes → Apariencia** y lo dejo como Q1, porque:

- El árbol documentado (`03-arbol-de-menus.md:109-120`) tiene un grupo **Apariencia (propios de Aura): Tema · Color de acento · Animaciones · Gráficos · Mostrar sombras · Mostrar iconos** — un tema es apariencia por definición, y ya vive ahí "Tema" (claro/oscuro) y "Color de acento" (el precedente runtime).
- Extras (`:68-81`) agrupa **utilidades**: Reloj internacional, Calendarios, Agenda, Alarmas, Juegos, Notas, Cronómetro — todas son "apps", ninguna es un ajuste. Meter ahí una lista de estilos rompe la taxonomía heredada del iPod original (Extras = apps).
- La propia `ExtrasView` de Studio ya le dice al usuario: *"Temas: se eligen en Ajustes > Tema, en el iPod"*.
- Coste de implementación **idéntico** en cualquiera de los dos sitios (una entrada más en `extras_entries[]` o en la tabla de Ajustes + una pantalla de elección).

Nombre de la fila y colisión con "Tema" (hoy = claro/oscuro): propongo **"Estilo"** para la lista de temas instalados y dejar "Tema: Claro/Oscuro" como está (así no se tocan `.lang` existentes ni docs). Alternativa: renombrar "Tema"→"Aspecto" y "Estilo"→"Tema" (más natural, pero toca 2 strings ES/EN + docs). Q1b.

### La pantalla

Reutiliza **exactamente** la maquinaria de pantallas de elección que ya usan Idioma (15 filas) y Ecualizador (23 presets): `is_choice_screen()` / `get_choice_table()` / `get_choice_current()` / `apply_choice()` / `draw_choice_list()` (`aura_screens.c:512-620, 2228-2280`) → `aura_menu_list_draw()` (`MenuList`, ícono + texto Semibold 15 + **checkmark** en la fila activa, `componentes/left-panel.md:74-116`) sobre `aura_selector_draw()` (`Selector`, `componentes/selector.md`). **Cero UI nueva.**

Diferencia con Idioma/EQ: la tabla de etiquetas se construye **desde disco** al entrar a la pantalla (no es `static const`): la primera fila fija es **"Aura"** (el default integrado, siempre presente aunque no haya directorio), seguida de un `theme_name` por cada `/.rockbox/aura/themes/<id>/theme.cfg` legible, hasta `MAX_MENU_ENTRIES` (32) — plantilla: `aura_music_list_playlists()` (`aura_music.c:974-1002`: `opendir`/`readdir`/filtro/`strlcpy` a un arreglo 2-D de tamaño fijo del llamador). Los `theme_name` se guardan en un buffer estático `char s_theme_labels[AURA_THEMES_MAX][AURA_THEME_NAME_LEN]` (p. ej. 16 × 32 = 512 B) porque `aura_menu_item_v2_t.label` es `const char*`. Un tema con `theme.cfg` ilegible o `theme_format` mayor al soportado **aparece atenuado** (`dimmed`, estado "fila inerte" ya documentado en `selector.md`) con SELECT sin efecto — así el usuario ve que existe y por qué no lo puede elegir (subtítulo no cabe: se muestra atenuado; el porqué lo explica Studio, ver 1.4).

### Qué pasa al seleccionar

**Aplica de inmediato, sin confirmación, sin reinicio** (T3 de la spec, confirmado viable arriba): `apply_choice()` llama a `aura_theme_activate(id)` que (1) valida el paquete (§1.2), (2) si es válido: 14 × `font_unload` de los ids actuales, relee la paleta, carga las 14 fuentes nuevas, invalida los dos cachés de imagen, guarda `theme_id` en `aura.cfg`; (3) si la validación falla o alguna `font_load` falla: **revierte** — recarga el tema anterior (o el default si el anterior tampoco carga) y **no** guarda. El siguiente cuadro ya se dibuja con el tema nuevo (redibujo total por iteración, `aura_main.c:423`). Si en hardware el cambio se sintiera lento, degradar a "aplica al reiniciar" es cambiar una línea (no llamar a `aura_theme_activate`, solo guardar) — no se prevé necesario.

Mientras carga (decenas de ms; en HDD hasta ~0.5 s) no se dibuja spinner: la pantalla ya está pintada y el siguiente cuadro sale con el tema nuevo. Si en hardware se nota, se puede usar el "loading icon" del Selector (variante ya documentada en `selector.md`).

### Vista previa antes de aplicar

**No como paso separado.** Una "vista previa" real de un tema exige cargar sus 14 fuentes — es decir, exactamente lo mismo que aplicarlo — y con `MAXUSERFONTS = 14` no hay hueco para tener las dos familias a la vez (habría que descargar el actual para previsualizar el otro). Como aplicar es instantáneo y **reversible con un SELECT en otra fila**, la aplicación **es** la vista previa. Un mock estático (una tarjeta con la paleta y una muestra de fuente renderizada) sería UI nueva sin equivalente en el sistema de diseño y con poco valor: descartado para v1.

---

## 1.2 — Firmware: carga y seguridad

### Formato del paquete (resumen; el contrato completo va en §1.5)

```
/.rockbox/aura/themes/<id>/            id: [a-z0-9-]{1,32}
  theme.cfg                            obligatorio
  fonts/<rol>.fnt                      14 obligatorios, nombre = rol
  icons/masks/<icon_key>-<px>.bmp      801 obligatorios (89 nombres × 9 tamaños)
  icons/light/, icons/dark/            OPCIONALES (BMP horneados por variante; fallback)
  backgrounds/<preset>.bmp             opcional (hoy: pink)
  tile-icons/aura_badge-{light,dark}.bmp   opcional
```

Tema por defecto = **rutas legadas actuales** (`/.rockbox/fonts/a26-*.fnt`, `/.rockbox/icons/aura/{masks,light,dark,backgrounds,tile-icons}`) con id reservado `default` y `theme_name` "Aura". No se mueve nada; `rockbox.zip` no cambia de layout; el sentinela de Studio sigue válido. (Q2)

### Resolución en el arranque y fallback obligatorio (seguridad)

Orden en `a26_shell_init()` (nuevo módulo `aura_theme.c` que la shell invoca):

1. `theme_id` de `aura.cfg` (clave nueva; ausente → `default`).
2. Si `theme_id == default` → rutas legadas (comportamiento de hoy, byte a byte).
3. Si no: **validar** el paquete: (a) `themes/<id>/theme.cfg` existe y parsea; (b) `theme_format` presente y `≤ AURA_THEME_FORMAT_SUPPORTED`; (c) los **14** `fonts/<rol>.fnt` existen (`stat`, no se cargan aún) — íconos y fondos **no** se validan aquí (se resuelven por archivo en cada dibujo, como hoy: si falta un BMP, se salta o cae al default por archivo). Si (a)-(c) pasan → paleta desde el manifiesto (lo que falte hereda del default compilado), 14 `font_load` desde `themes/<id>/fonts/`; si **cualquier** `font_load` devuelve `< 0` (archivo corrupto) → descargar lo cargado y **caer al default entero** (no mezclar caras).
4. Default: 14 `font_load` de las rutas legadas; si alguna falla → `FONT_SYSFIXED` para ese rol (lo que ya hace hoy `apple2026_shell.c:57`) y paleta compilada. **Nunca sin UI legible.**

Al caer al default en el arranque **no se reescribe `aura.cfg`**: el ajuste queda como estaba (si el tema vuelve — p. ej. Studio lo estaba copiando — se cura solo al siguiente arranque); en la pantalla "Estilo" el checkmark aparece en "Aura" (el activo real) y la fila del tema fallido, si su directorio existe pero es inválido, atenuada. Elegir explícitamente "Aura" sí escribe `theme_id: default`.

Resolución de íconos por dibujo (los 5 sitios de ruta): prefijo del tema activo → si el archivo no existe **y el tema no es el default**, se intenta el mismo archivo en el prefijo del default (fallback por archivo, cubre íconos/fondos/tiles opcionales o faltantes) → si tampoco, comportamiento actual (ícono omitido / badge → `ipod`). Coste: un `read_bmp_file` fallido extra solo cuando falta el archivo — despreciable, y sin coste en el default.

Ruta de escape garantizada: un tema no puede tocar el bootloader, ni `rockbox.ipod`, ni el default en rutas legadas; SELECT+MENU/reset y el arranque en `default` siempre existen. Restablecer ajustes (ya en Ajustes → Sistema) vuelve a `default`.

### Versionado del formato

- El firmware define `AURA_THEME_FORMAT_SUPPORTED 1` (`aura_theme.h`) y **lo escribe en `aura.cfg`** como `theme_format_supported: 1` en cada `aura_settings_save()` — así Studio puede leerlo del dispositivo montado sin adivinar la versión del firmware (hoy no existe ninguna versión visible en el iPod).
- Reglas: formato **igual** → carga; formato **menor** (tema viejo con firmware nuevo) → carga lo que entienda, hereda el resto del default (v1 no tiene "menor"; se define para el futuro); formato **mayor** → no carga, fallback, fila atenuada.
- Añadir un rol de fuente, un `icon_key` o un tamaño = subir `theme_format`.

### Validación: ambos lados, ninguno confía en el otro

Studio valida al construir/instalar (manifiesto completo, 14 `.fnt` con cabecera válida, 801 máscaras presentes con dimensiones correctas, `theme_format ≤` el soportado por el firmware del iPod) y **rechaza** paquetes incompletos antes de copiar. El firmware valida al cargar lo mínimo para no romperse (parseo, formato, 14 fuentes existen, `font_load` sin error). El firmware **no** confía en que Studio validó.

---

## 1.3 — Studio: construcción del tema

### Alcance realista y por fases (Q4)

Un constructor completo tiene dos piezas grandes que hoy **no existen** en Studio (verificado: cero usos de `NSFontManager`/`NSFont`, cero de `NSImage(systemSymbolName:)`, `PlaylistArtGenerator` es CoreGraphics puro): la rasterización de fuentes y la de íconos. Propongo dos entregas dentro del proyecto de temas, ambas necesarias, la segunda separable:

**2A — Empaquetar, validar, instalar, gestionar (+ tema Apple real, de punta a punta)**
Studio gana la pestaña "Temas" (en `ExtrasView`, que ya la anuncia) con: lista de temas en el iPod, activar, eliminar, y **"Construir tema desde carpeta de assets generados"**: el usuario señala una carpeta con el layout de `design-system/out/` (`fonts/a26-*.fnt`, `icons/masks/`, opcionalmente `icons/{light,dark}/`, `icons/aura/{backgrounds,tile-icons}/`), Studio la **reempaqueta** al formato de tema (renombra `a26-<rol>-<px>.fnt` → `<rol>.fnt` según el manifiesto de roles del contrato, copia máscaras, escribe `theme.cfg` con la paleta por defecto editable y los campos de licencia), **valida** y lo instala. Con esto el **tema Apple se construye hoy mismo** desde `~/Aura-local/theme-apple-source/design-system-out/` (que ya contiene los 14 `.fnt` de SF Pro/Compact + las 8010 BMP + las 801 máscaras horneadas por el pipeline el 2026-08-16 en la Mac del dueño) — es el primer caso de uso real y valida el contrato, el cargador del firmware y el fallback **sin escribir un rasterizador nuevo**. Cumple el principio rector: los assets están en la Mac del usuario; Studio no los incluye ni los descarga.

**2B — Rasterizar en Studio desde fuentes/símbolos del sistema** (el "constructor" pleno de la spec, N.6):
- **Fuentes → `.fnt`**: Studio embebe `convttf` (herramienta de Rockbox, GPL v2, con oferta de fuente igual que `mks5lboot`, T5) + `libfreetype` (FreeType License, redistribuible). El usuario elige familia/pesos con `NSFontPanel`/`CTFontManager` (Studio sugiere "una familia para todo" y asigna Regular/Medium/Semibold/Bold a los 14 roles); Studio invoca `convttf -p <px> -o <rol>.fnt <ttf>` con los **mismos px de `type_scale`** que el contrato publica (los tamaños son del sistema de diseño, el tema no los cambia). Los TTF del sistema pueden ser `.ttc`/`.otf`: `convttf` acepta lo que FreeType abra; SF Pro está en `/System/Library/Fonts/SFNS.ttf` (o `SF-Pro.ttf` si el usuario instaló el paquete de Apple) — Studio resuelve por familia vía CoreText y le pasa la ruta.
- **Íconos → máscaras**: puerto a Swift de **la mitad simple** del pipeline: por cada (`icon_key`, `px`) renderiza la forma **a 16×** (SF Symbols vía `NSImage(systemSymbolName:)` + `SymbolConfiguration(pointSize: px·16·0.82, weight: por tamaño)`, o SVG vía `NSImage(contentsOfFile:)`) en negro sobre alfa 0, reduce **con filtro de caja** al tamaño final, y escribe la **cobertura** (canal alfa) como BMP 24-bit R=G=B → `icons/masks/<key>-<px>.bmp`. Verifica la rampa (≥ 4 tonos, misma regla `MIN_INK_TONES` del pipeline). **No** genera horneados por variante (opcionales en v1) → **no hay composición contra fondos, ni clave magenta, ni umbral binario**: la restricción heredada de N.5 queda satisfecha por construcción, porque una máscara *es* la cobertura antialiasada que `draw_icon_mask_2` compone en el dispositivo. La lógica de `apple2026_sf_render.swift` (7.5 KB) es la referencia directa del render.
- **Mapa de nombres**: los 89 `icon_key` son nombres lógicos (hoy coinciden con nombres SF: `music`, `gearshape`, `chevron.right`…). Para SF Symbols el mapa es identidad (`icon.names` de `tokens.json`); para Lucide/Phosphor Studio embebe el mapa `icon-name-map.json` (ISC/MIT — sí puede ir dentro de Studio) y los SVG; para "carpeta de SVG propia" el usuario aporta un JSON con el mismo esquema.
- Casos especiales del pipeline que el puerto debe respetar: `dynamic_speaker` (5 estados, `pt` fijado a `px·16·0.82`), `battery_icon` (lienzo no cuadrado 21×12 por unidad), y `music`/`ipod` (artwork propio de Aura, SVG en el repo del firmware — Studio los toma del asset del Release, ver §1.5).

Recomendación: **2A en esta Fase 2; 2B como la siguiente unidad de trabajo, con su propio plan corto** — 2A ya entrega el sistema completo y verificable en dispositivo (incluido el tema Apple real), y 2B es un rasterizador que merece su propia verificación visual contra `docs/screenshots/`.

### Paleta en el constructor

v1: parte de la paleta del default (que Studio recibe compilada en `AuraPalette.swift` del Release, más el asset `theme-format-v1.json`, §1.5), con edición hex por rol × modo (claro/oscuro) y por color de categoría; el acento del tema es solo el *default* y sus 6 presets — el usuario sigue mandando en el iPod (T4). Fondos de panel y tile-icons: opcionales; si el usuario no aporta, el firmware cae por archivo al default.

---

## 1.4 — Studio: instalación y gestión

- **Dónde en la UI**: `ExtrasView` → fila "Temas" pasa de texto informativo a navegar a `ThemesView`: lista de temas instalados en el iPod (lee `themes/*/theme.cfg` del volumen montado, más "Aura (integrado)"), el activo marcado (lee `theme_id` de `aura.cfg`), botones Activar / Eliminar (nunca el default; confirmación en español) / Construir nuevo / Exportar (solo si `theme_redistributable: yes`; si no, **deshabilitado con explicación**, no oculto).
- **Instalar**: copia del directorio del tema a `/.rockbox/aura/themes/<id>/` con el mismo mecanismo que el instalador (`ditto`, sin privilegios: `InstallerViewModel.extractZip`/`copyFirmwareFiles` son proceso de usuario sobre el volumen montado), tomando el candado global `InstallerFlowRegistry.shared.beginWriting()` (D-185) para no cruzarse con un sync o una instalación; verificación por checksums del paquete tras copiar (Studio genera un `checksums.txt` dentro del tema al construirlo y lo verifica tras `ditto`). Antes de copiar, Studio comprueba `theme_format_supported` en `aura.cfg` del iPod (o, si el firmware instalado es anterior a este cambio y no escribe la clave, asume "sin soporte" y lo dice: "Actualiza Aura en el iPod para usar temas").
- **Activar desde Studio**: escribe `theme_id: <id>` en `aura.cfg` **preservando las demás líneas** (reescritura línea a línea, atómica como `sync_summary.cfg`). Nota de contrato: `aura_settings_save()` del firmware reescribe el archivo entero con sus claves conocidas — como `theme_id` es clave conocida del firmware, sobrevive; Studio **no** debe añadir claves ajenas al firmware (se perderían). Studio hoy nunca escribe `aura.cfg` (verificado); esta es la primera vez y queda documentada en el contrato §D.
- **Eliminar**: `removeItem` del directorio del tema **solo si** el path resuelto está bajo `<mountPath>/.rockbox/aura/themes/` y el `<id>` no es `default` ni contiene `/`/`..` (misma disciplina de rutas que `AuraDeviceProbe` — mountPath absoluto y no vacío, D-070). Si era el activo, primero escribe `theme_id: default`.
- **Seguridad de escritura**: no hay operación destructiva de disco aquí (nada de formatear/flashear), pero se aplican las reglas: identificación del volumen por `IPodMonitor.state == .diskMode(info)` con `mountPath` verificado, re-lectura de `mountPath` inmediatamente antes de cada escritura (nunca una captura vieja), candado global, y confirmación explícita al eliminar mostrando nombre del tema, id y nombre del volumen.
- **Advertencia de licencia (obligatoria, español, Principio 7)** — antes de construir/instalar un tema con assets restringidos (familias `SF Pro*`, `SF Compact*`, `SF Mono*`, `.SF NS*`, SF Symbols, o cualquiera que el usuario marque), y en la ficha del tema:
  > **Este tema usa tipografías o símbolos con licencia restringida que ya están en tu Mac.** Aura Studio lo construye solo para **tu propio iPod**: no lo compartas ni lo distribuyas — la licencia de esos assets no lo permite fuera de tu dispositivo. Aura Studio nunca incluye ni descarga estos archivos; solo usa los que tu Mac ya tiene.
  Se escribe en el manifiesto (`theme_license: personal`, `theme_redistributable: no`); Exportar queda deshabilitado con ese texto. En 2A, al empaquetar desde una carpeta, Studio pregunta explícitamente el origen de los assets (SF/otro restringido vs. libre) y no asume "libre" por defecto.
- **Strings**: `AppStrings.swift` (`enum S`, ES/EN) cubre solo Ajustes + barra lateral; el resto está en español hardcodeado. Los textos de Temas siguen el patrón que ya use la vista donde viven (ExtrasView está en español directo) — no se abre una migración de i18n aquí.

---

## 1.5 — Contrato entre repos: formato de tema v1

Documento nuevo **`CONTRATO-formato-tema.md`**, **idéntico en ambos repos** (canónico en `Aura-Firmware`, como `CONTRATO-firmware-studio.md`), versionado como "formato de tema v1"; y `CONTRATO-firmware-studio.md` sube a **v2** con: referencia a este contrato, la clave `theme_id`/`theme_format_supported` en la tabla del contrato en disco (§D), y **dos assets nuevos del Release**: `theme-format-v1.json` (generado por `generate.py` desde `tokens.json`: los 14 roles con su px, la lista de 89 `icon_key`, los 9 tamaños px, las 5 variantes, los nombres de presets de fondo y tile-icons, la paleta por defecto en hex) y `aura-theme-default.zip` (opcional, el default reempaquetado en formato de tema, para que Studio pueda mostrarlo/exportarlo como tema libre; Q5). Studio **no lee `tokens.json` ni el árbol del firmware**: consume `theme-format-v1.json` vía `fetch-firmware.sh` (ya existe) — es la fuente de verdad de roles/nombres/tamaños para construir y validar.

Contenido del contrato v1 (lo que Fase 2 escribe):

1. **Layout** del paquete (§1.2) y reglas del `<id>`.
2. **`theme.cfg`** — claves, formato `clave: valor` (mismo parser que `aura.cfg`), líneas ≤ 127 caracteres (el lector de `theme.cfg` usa su propio buffer de 128 — el de `aura.cfg` es `char line[64]` y la línea `accent_presets` con 6 hex mide exactamente 63, demasiado justo; `theme_name` ≤ 32):
   `theme_format: 1` (obligatoria) · `theme_id` · `theme_name` (≤32, lo que se muestra en el iPod) · `theme_author` · `theme_license: personal|open|<texto>` · `theme_redistributable: yes|no` · `requires_firmware_min` (informativa; el firmware no la usa en v1, Studio sí) · `palette_{light,dark}_{shell_bg,text_primary,text_secondary,text_tertiary,shell_rail,progress_fill,progress_track,selection_fill}: #RRGGBB` (8 roles × 2 modos; `accent` y `white_constant` no son del tema: el acento lo pone el usuario, el blanco del Selector es estructural) · `accent_default: #RRGGBB` · `accent_presets: #…,#…,#…,#…,#…,#…` (6) · `category_video`, `category_photos`, `category_extras_yellow`, `category_settings_gray: #RRGGBB`. Todo lo ausente hereda del default compilado.
3. **Roles de fuente** (14, con su px fijo): `title 20, body 13, caption 13, header 13, micro 7, ds_reg_8 8, ds_reg_10 10, ds_bold_10 10, ds_reg_12 12, ds_bold_12 12, ds_bold_14 14, ds_semibold_15 15, ds_bold_18 18, ds_medium_16 16` — archivo `fonts/<rol>.fnt`, formato `.fnt` de Rockbox (el que produce `convttf`).
4. **Íconos**: 89 `icon_key` × 9 px (`12, 16, 20, 24, 28, 36, 48, 60, 64`) → 801 máscaras obligatorias `icons/masks/<key>-<px>.bmp` (BMP 24-bit, R=G=B=cobertura 0–255, sin transparencia); horneados opcionales `icons/{light,dark}/<key>-<px>[-on|-tertiary|-rail|-selector].bmp` (BMP con clave magenta `#FF00FF`, como hoy). Los casos especiales de lienzo (`battery_icon` 21×12·n, `dynamic_speaker`) se listan.
5. **Fondos y tiles opcionales**: `backgrounds/<preset>.bmp` (160×240, 24-bit), `tile-icons/aura_badge-{light,dark}.bmp` (90×90, magenta donde alfa 0).
6. **Versionado y compatibilidad**: tabla `theme_format` ↔ firmware (v1 ↔ el primer release que incluya `aura_theme.c`); regla de qué cambios suben el formato; qué hace cada lado ante mayor/menor.
7. **Claves de `aura.cfg`** que cruzan la frontera: `theme_id` (firmware lee/escribe; Studio puede escribir preservando el resto), `theme_format_supported` (firmware escribe; Studio lee).
8. **Licencia**: `theme_license`/`theme_redistributable` son declaraciones del constructor; el firmware las ignora; Studio las respeta al exportar/compartir; el default es `open`/`yes` (Inter OFL, Lucide ISC, Phosphor MIT).

Ninguno de los dos repos depende de rutas del otro: el firmware solo conoce `/.rockbox/aura/themes/`; Studio solo conoce el Release (`theme-format-v1.json`) y el volumen montado.

---

## Preguntas abiertas (con recomendación)

**Q1 — Ubicación del submenú**: ¿Extras (como dice el encargo) o Ajustes → Apariencia (como dice la spec y el árbol documentado)? Recomendación: **Ajustes → Apariencia**, fila nueva **"Estilo"** justo después de "Tema" (razones en §1.1). Coste idéntico. **Q1b**: ¿"Estilo" + "Tema (claro/oscuro)" sin tocar strings, o renombrar "Tema"→"Aspecto" y llamar "Tema" a la lista? Recomendación: **"Estilo"** (sin renombrar nada existente).

**Q2 — Tema por defecto en disco**: ¿se queda en las rutas legadas (`/.rockbox/fonts`, `/.rockbox/icons/aura`) con id implícito `default`, o se mueve a `themes/default/` como decía la spec? Recomendación: **rutas legadas** — cero migración, `rockbox.zip` intacto, sentinela de Studio intacto, y el fallback por archivo hacia el default es una concatenación de prefijo.

**Q3 — Horneados `light/`/`dark/` en el formato**: ¿opcionales (solo máscaras obligatorias, tema ≈ 11.5 MB) o obligatorios (≈ 63 MB, como el default hoy)? Recomendación: **opcionales**. Verificado en código que la máscara es el camino primario para todos los íconos. En Fase 2 se toma una captura del simulador con `light/`/`dark/` retirados del disco para confirmar que no hay ningún consumidor escondido antes de declararlo en el contrato. (Segunda consecuencia, fuera de este alcance: el propio `rockbox.zip` podría dejar de llevar 52 MB de horneados — no se propone tocarlo ahora.)

**Q4 — Alcance de Studio en Fase 2**: ¿2A (empaquetar desde carpeta de assets + instalar/gestionar + tema Apple real de punta a punta) ahora y 2B (rasterizador nativo: `convttf` embebido + puerto Swift del render de máscaras) como siguiente unidad, o todo junto? Recomendación: **2A ahora, 2B después** con plan propio. Si prefieres todo junto, la Fase 2 crece ~2× del lado Studio y la verificación visual del rasterizador necesita comparación contra las máscaras del pipeline actual.

**Q5 — Asset `aura-theme-default.zip` en el Release** (el default reempaquetado como tema libre): ¿sí o no en v1? Recomendación: **sí, pero generado por `package_dist.sh` sin subirlo aún** (el dueño decide qué sube al Release) — sirve para probar el instalador de Studio con un tema libre sin tocar el material de Apple, y como ejemplo canónico del formato.

**Q6 — Manifiesto de formato como asset del Release (`theme-format-v1.json`)** para que Studio no lea `tokens.json`: ¿de acuerdo? Recomendación: **sí** — es la única forma de que Studio valide 89 nombres × 9 tamaños sin depender del árbol del firmware ni duplicar la lista a mano en Swift.

**Q7 — `theme_format_supported` escrito por el firmware en `aura.cfg`** (nueva clave que Studio lee del iPod para saber si puede instalar temas): ¿de acuerdo? Recomendación: **sí** — hoy no hay versión visible en el dispositivo y es la señal más simple y sin red.

**Q8 — Al fallar el tema en el arranque, ¿se reescribe `aura.cfg` a `default` o se deja el ajuste?** Recomendación: **se deja** (se cura solo si el tema vuelve; elegir "Aura" a mano sí lo escribe).

**Q9 — Nombre visible del default**: "Aura". ¿OK? (T2 de la spec: sí; el de SF, "Apple (uso personal)", lo pone Studio en el manifiesto de ese tema, sin logo.)

---

## Fase 2 (solo tras aprobación) — resumen de lo que haría

**`Aura-Firmware`** (commits atómicos, sin push):
1. Corregir `package_dist.sh:77-80` (rutas de `backgrounds`/`tile-icons`) — bug preexistente.
2. `aura_theme.{c,h}` nuevo: constantes (`AURA_THEME_FORMAT_SUPPORTED 1`, `AURA_THEMES_DIR`, límites), lectura de `theme.cfg` (parser existente), tabla de paleta runtime con default compilado, resolución de rutas de fuentes/íconos/fondos/tiles por tema con fallback por archivo al default, `aura_theme_scan()` (lista de instalados), `aura_theme_validate()`, `aura_theme_activate()` (con reversión) y `aura_theme_boot()`; **lógica pura** (parseo de manifiesto, resolución de rutas, política de fallback/formato) en un leaf `aura_theme_manifest.c` sin cabeceras de Rockbox → **test host nuevo `test_theme.c`** (el arnés admite un `.c` por test; los tests pasan a **9/9**, con los 8 actuales verdes).
3. `apple2026_shell.c`: `a26_color()` lee la tabla runtime; `a26_shell_init()` delega la carga de fuentes en `aura_theme_boot()`; nuevo `a26_shell_reload_fonts()` (unload ×14 + load ×14). Los 2 usos de `CATEGORY_EXTRAS_YELLOW`, la tabla de presets de acento y el default de acento pasan por la tabla. Los 5 sitios de ruta de íconos usan el prefijo del tema. Invalidación de `s_aura_badge_theme` y `s_bg_loaded_name` (a comparación por contenido/generación).
4. `aura_settings.c`: claves `theme_id` (string, `AURA_THEME_ID_LEN` 33) y `theme_format_supported` (solo escritura). `aura_screens.c`: pantalla de elección "Estilo" (según Q1) con tabla dinámica desde disco, fila "Aura" fija, filas inertes para temas inválidos; strings ES/EN al final de ambos `.lang`.
5. `generate.py`: emite `theme-format-v1.json`; `package_dist.sh`: lo copia a `dist/` y (Q5) arma `aura-theme-default.zip` desde `out/`.
6. Docs: `docs/aura-design-system/sistema/05-temas.md` (sistema, formato, fallback, submenú; con la spec `PLAN-theme-system.md` incorporada), fila en `00-INDICE.md`, `03-arbol-de-menus.md` (fila nueva), `fundamentos/01-color.md`/`02-tipografia.md` (nota "el tema activo decide la cara/los hex"), `componentes/left-panel.md` (fila inerte en lista dinámica si aplica). `CONTRATO-formato-tema.md` nuevo + `CONTRATO-firmware-studio.md` v2. `README.md`: párrafo de temas. `CLAUDE.md`: una regla ("nada de rutas de assets fuera de `aura_theme_path()`"). `DECISIONS.md`: D-288 (bug de `package_dist.sh`), D-289 (sistema de temas), D-290 (pantalla Estilo).
7. Verificación: `build_sim.sh` limpio, build ARM limpio, tests 9/9. **Capturas** con `apple2026_sim_shot.sh` en `docs/screenshots/themes/`: (a) submenú Estilo, (b) tema por defecto aplicado (raíz + Ajustes), (c) tema alternativo aplicado — para el simulador se construye un tema de prueba libre `themes/aura-inverse/` (Inter con paleta invertida y máscaras copiadas) porque el material de Apple no entra al repo ni al simdisk versionado; el tema Apple real se verifica en el iPod del dueño vía Studio, (d) fallback: `theme_id: roto` con `theme.cfg` corrupto y con fuentes faltantes → arranca en Aura sin cambios visibles; (e) Q3: raíz con `light/`/`dark/` retirados del simdisk.

**`Aura-Studio`** (2A; commits atómicos, sin push):
1. `Models/Theme*.swift`: modelo del paquete, parser/escritor de `theme.cfg`, lector de `theme-format-v1.json` (del Release, vía `Vendor/firmware-dist/`), validador (14 fuentes con cabecera `.fnt`, 801 máscaras, dimensiones, `theme_format`), detección de licencia restringida por nombre de familia/origen declarado.
2. `Services/ThemeInstaller.swift`: listar/activar/eliminar/instalar sobre el volumen montado (`ditto`, candado global, re-lectura de `mountPath`, edición línea a línea de `aura.cfg`, checksums post-copia).
3. `Services/ThemePackager.swift`: reempaquetado desde carpeta con layout `design-system/out/` (renombrado de fuentes por rol, copia de máscaras/horneados/fondos/tiles, `theme.cfg`, `checksums.txt`).
4. `Views/ThemesView.swift` desde `ExtrasView` (lista, activo, Activar/Eliminar/Construir/Exportar deshabilitado con explicación, advertencia de licencia).
5. Tests: parser/escritor de manifiesto, validador (paquete completo/incompleto/formato mayor), reempaquetado (renombres), edición de `aura.cfg` preservando líneas, rutas seguras al eliminar. `swift build` + `swift test` (excepción conocida de `LiveEnrichmentIntegrationTests`).
6. `CONTRATO-formato-tema.md` (copia idéntica), `CONTRATO-firmware-studio.md` v2 (copia idéntica), `CLAUDE.md` (regla: los temas se validan antes de copiar; nunca se exporta un tema `redistributable: no`), `README.md`, `DECISIONS.md`: ST-003 (temas: alcance 2A y contrato), ST-004 (instalador/gestor), ST-005 (empaquetado + tema Apple construido y probado en el iPod del dueño — la verificación en hardware queda como paso del dueño, documentado).

**Fuera de git**: nada. El material de Apple no se mueve.

**BARRERA** — detenido aquí hasta tu aprobación.
