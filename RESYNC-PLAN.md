# RESYNC-PLAN — Fase 1: plan de resincronización doc ↔ código

**Fecha: 2026-08-16. Estado: PENDIENTE DE APROBACIÓN DEL DUEÑO. Nada aplicado.**

Regla de esta pasada: el código manda, la doc se actualiza para reflejarlo — pero solo para divergencias **tipo A** (decisión deliberada con D-XXX o encargo registrado). Tipo B (código incompleto, la doc tiene razón) no toca la doc. Tipo C (sin rastro) se pregunta, no se decide. **No se cambia ni una línea de código en esta pasada** — lo que parece bug de código va a `RESYNC-GAPS.md`.

Método: tres auditorías paralelas leyeron cada `.md` de `docs/aura-design-system/` completo contra el código real (`firmware/rockbox/apps/aura/*.c/.h`, `design-system/tokens.json`, `apple2026_tokens.h`, `font.h`) y verificaron cada D-XXX citado abriendo la entrada en `DECISIONS.md`.

---

## 0. Correcciones a las premisas del encargo (leer primero)

La verificación a fondo contradijo cuatro cosas que el encargo (y `RESUMEN.md`/`BLOCKED.md`, que salieron de una auditoría más superficial) daban por hechas:

| Premisa del encargo | Lo que el código muestra en realidad | Efecto en el triaje |
|---|---|---|
| "El toggle 'desactivar sombras desde Ajustes' no existe en código → tipo B" | **SÍ existe**: `aura_settings.left_panel_shadow` (default true), fila **"Mostrar sombras"** en Ajustes, `aura_shadows_enabled()` (`apple2026_shell.c:171`), consultado por la sombra del LeftPanel, CoverDrift y NowPlaying Modo 4 (D-088 lo creó como "Sombra de panel", D-154 lo renombró y lo hizo global) | Pasa a **tipo A**: la doc debe citarlo. El hueco B real es otro: `draw_tile_shadow()` de SelectionSummary (`aura_selection_summary.c:462`) **no consulta el toggle** — es la única sombra que lo ignora |
| "El stagger de entrada de NowPlaying no tiene implementación" | **SÍ está implementado** (`aura_transitions.c:842-900`, D-113): fade de textos + modos entrando desde la derecha + barra desde abajo, en paralelo, 8 cuadros (4 en modo reducido); StatusBar cae después. La doc `now-playing.md:100-114` ya describe exactamente ese provisional | No es B. Es **tipo C**: doc y código coinciden, ambos lo marcan provisional — falta que el dueño lo ratifique |
| "`DateEditor`: cero código, stub puro" | **SÍ hay pantalla real**: `AURA_SCREEN_SETTINGS_DATE_EDIT` → `draw_date_edit()` (`aura_screens.c:2822`, D-170): rejilla del mes como selector, cabecera "Mes Año" con campo activo en acento, SELECT avanza día→mes→año, MENU cancela, persiste con `rtc_write_datetime()`. `grep date_editor` no lo encontraba porque el nombre interno es otro | Contenido de la pantalla → **tipo A** (documentarla citando D-170). Lo que sigue siendo B: la transición `Shift-and-Reveal` y el ícono dinámico de hoja de calendario |
| "`DynamicTitle` nunca dispara transición" | Confirmado (B), **y además** el título de StatusBar **tampoco hace marquee**: `aura_status_bar_v2.c:265-266` pasa `marquee_elapsed_ms = 0` siempre → fase estática perpetua, solo recorta | Un segundo hueco B, no anotado antes |

`RESUMEN.md` y `BLOCKED.md` deberían corregirse en Fase 2 con estos cuatro puntos (son reportes, no doc de diseño — no requieren aprobación aparte, pero lo señalo).

---

## 1. Tipo A — doc desactualizada, actualizar citando D-XXX

Agrupado por archivo de doc (= un commit atómico por archivo en Fase 2).

### `componentes/selection-summary.md` (el más desactualizado — reescritura de varias secciones)

| Línea | Qué dice la doc | Qué hace el código | D-XXX | Cambio propuesto exacto |
|---|---|---|---|---|
| 60-74 "Fondo del panel derecho (confirmado)" | Fondo = degradado diagonal con acento al centro + 2 derivados; 🔴 dirección y % pendientes | Fondo = **imagen 160×240 por preset de acento**, opaca (`ensure_panel_background()`, `aura_selection_summary.c:132-163, 385-397`); único preset `pink`, cualquier acento cae ahí; sin archivo → `SHELL_BG`. El degradado diagonal **solo vive dentro del tile** (`draw_diagonal_gradient()` :100-130). Nunca hubo degradado de panel en código (pre-D-267 era `SHELL_BG` plano) | D-267 (imagen), D-097 (degradado del tile), D-086 G9 (25% claro/oscuro) | Dividir en dos secciones: **"Fondo del panel derecho (D-267)"** — imagen horneada por preset de acento (`design-system/assets/panel-backgrounds/<preset>-source.png` → `out/icons/aura/backgrounds/<preset>.bmp`); sigue el ACENTO, no la categoría (mockup: fila Fotos, tile naranja, fondo rosa); interino solo `pink`, los otros 5 pendientes de imágenes del dueño; fallback `SHELL_BG`. **"Degradado del tile (D-097/D-236)"** — 3 puntos claro→color→oscuro por categoría, 25% hacia blanco/negro (cierra el 🔴 del %). Mantener 🔴 solo para la dirección (ver C) |
| 17-22 "El conjunto va centrado horizontal y verticalmente" | Ícono+texto como grupo | Tile SIEMPRE al centro exacto, `tile_y=(240-90)/2` fijo (:369-381); texto aparte: superior centrado en `[0,tile_y)`, inferior en `[tile_y+90,240)` (:494-532) | D-270, D-272 | "El tile va al centro exacto del panel, independiente de si hay texto. El texto vive aparte: el superior se centra en el margen completo entre el borde superior y el tile; el inferior en el margen entre el tile y el borde inferior (D-270, D-272)." |
| (sin sección) tipografía/color de texto | Solo "símbolo claro sobre tile" | Superior **SF Pro Bold 18pt** 1 línea; inferior **Medium 16pt** hasta 2 líneas por palabra (`split_two_lines()`); ambos **blanco fijo** `SELECTOR_CONTENT_TINT_HEX_ON_ACCENT` | D-263, D-267, D-271 | Añadir "Tipografía y color del texto": tabla slot superior Bold 18pt 1 línea / inferior Medium 16pt ≤2 líneas (corte por palabra; MarqueeText por línea si aún desborda); blanco constante en ambos temas porque el fondo es siempre saturado. Citar D-263/D-267/D-271 |
| (sin sección) sombra del tile | Solo "sombra de LeftPanel" (:172-176) | Drop shadow: offset +4 Y, alpha 35%, caída lineal SDF 12px alrededor de todo el perímetro (`draw_tile_shadow()` :165-224); esquinas recortadas con compositing real (:442-476) | D-267, D-270 | Añadir "Sombra del tile (D-267/D-270)": offset 4px, 35% pico, barrido lineal 12px medido como distancia al rectángulo redondeado (equivalente práctico a blur gaussiano en este LCD); esquinas restauradas con píxeles reales del fondo |
| 117-125 "El ícono siempre cambia… símbolo claro sobre tile" | Ícono monocromo `-selector` blanco | "Acerca de" muestra el **badge de Aura a color** (`aura_badge.bmp`, pipeline `tile_icons`, `aura_screens.c:1579`); la fila conserva "info" | D-269 | Añadir excepción: "Acerca de usa un ícono a color (badge de Aura, `tile_icons` en tokens.json) SOLO en SelectionSummary; la fila mantiene su glifo. Única excepción hoy a 'símbolo blanco -selector'" |
| 135-142, 182 "Variante dinámica… sin resolver si es variante o componente distinto" | Arquitectura abierta | Es **variante del mismo componente**: `aura_selection_summary_draw_dynamic()` con `icon_renderer` + `bottom_renderer` (:552-560); consumidores: Fecha y hora (reloj analógico), Acerca de (badge + barras de almacenamiento) | D-108 (B-04), D-264 | Reemplazar por "Resuelto (D-108/B-04): modo del mismo componente. Slots: ícono estático o `renderer`; texto inferior o `bottom_renderer` gráfico." Añadir ejemplos reales D-264 (Fecha y hora; Acerca de con barras Música/Video/Fotos/Otros; Repetir/Aleatorio en línea; Música/Video/Fotos "No hay X"). Marcar :182 cerrado. **Conservar** los ejemplos Hora/Fecha de sub-filas (:90-96) como intención (dependen de DateEditor, B) |
| 144-159 "Comportamiento (revertido por D-262)": "fundido real de 600ms tras 2000ms" | Espera única 2s + fundido | Destino SelectionSummary: **1000ms + corte instantáneo**; 2s + fundido 600ms solo hacia CoverDrift (`aura_screens.c:1238-1250`) | D-266 | "…se actualiza tras **1000ms** de estabilidad con un **corte instantáneo** (D-266). El fundido de 600ms tras 2000ms queda reservado a cambios cuyo destino es CoverDrift." |
| 161-170 "Cross-fade con CoverDrift… aplica igual en reversa" | Simétrico | Solo al entrar a CoverDrift hay fundido; la reversa es 1s + corte | D-266 | Sustituir "Aplica igual en reversa" por "La reversa NO funde: al dejar de calificar, SelectionSummary aparece con corte tras 1000ms (D-266)" |

### `componentes/status-bar.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 101-111 "El tamaño real es 8px" (Bold 8 / Regular 8) | 8px | `DS_BOLD_12` (`aura_status_bar_v2.c:245`) y `DS_REG_12`; `fundamentos/02-tipografia.md:18-19` YA dice 12px | D-205 (8→10), D-207 (10→12, "el texto casi no se nota", 2026-08-14) | Tabla: `--font-statusbar-title` SF Pro Bold **12px** 60%; `--font-statusbar-time` Regular **12px** 80%. Nota: "8→10 (D-205) →12 (D-207)". Borrar "el tamaño real es 8px" |
| 57-61 "ClockIndicator en máximo 40px" | 40 | `CLOCK_INDICATOR_MAX_WIDTH=46`, `HEIGHT=12` (tokens) | D-207 | "máximo 46px (D-207)" |

### `componentes/clock-indicator.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 19-20 "Alto: 10px, Ancho máximo: 40px" | 10/40 | 12/46 (tokens `clock_indicator`) | D-207 | "Alto: 12px, Ancho máximo: 46px (D-207; antes 10/40)" |
| 5-15 "Tres modos: Persistente / Auto-oculta-por-atajo / Por atajo" + 55-57 "¿qué dispara la revelación en auto-oculta?" | 3 modos, disparador desconocido | `aura_settings.clock_visible` es **bool**: persistente, u oculto y revelado SOLO por mantener SELECT (`AURA_BUTTON_HOLD`), auto-oculto a 10s | D-108 (B-01) | Reducir a **dos modos**: Persistente / Oculto (se revela manteniendo SELECT ~300ms, se oculta solo a los 10s). Marcar el pendiente resuelto por D-108 |
| 14-15, 58 "Falta confirmar que mantener Select no choque con otras funciones" | Pendiente | Hold solo para SELECT en lista blanca; pulsación corta se despacha en PRESS antes de que el hold exista → sin doble disparo por construcción; StatusBar única dueña | D-108 (B-02) | Marcar resuelto: "D-108: infraestructura de hold general, sin choque por construcción (despacho en press)". Nota honesta: gesto no verificable por el arnés headless |

### `componentes/cover-drift.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 21-27 "(c) ya pasó el retardo de activación de 3s" | 3s propio | Retirado; la propia doc lo dice en :113 (inconsistencia interna) | D-262 | "(c) se cumplió el debounce general del panel derecho (ver Activación)" |
| 29-33 "Menú raíz: solo cuando la fila resaltada es Música" | Solo Música | También **Canciones aleatorias** y **Ahora suena** (`music_row_wants_coverdrift()`, `aura_screens.c:715-727`) | D-266 | "Menú raíz: Música, Canciones aleatorias y Ahora suena (D-266 — las tres resuelven a `AURA_CATEGORY_MUSIC`). Videos y Fotos siguen fuera." |
| 113-134 "solo se actualiza (con fundido, no corte) tras 2000ms… sea CoverDrift, SelectionSummary o ícono"; 130-134 "dejar de calificar → 2000ms y fundido" | Uniforme | Asimétrico por DESTINO: hacia CoverDrift 2000ms + fundido; hacia SelectionSummary 1000ms + corte | D-266 | Reescribir Activación: "2000ms + fundido 600ms SOLO cuando el destino es CoverDrift; cuando el destino es SelectionSummary/ícono (incluye dejar de calificar) la espera baja a 1000ms y el cambio es un corte (D-266)" |
| 153-156 "Cross-fade al montarse/desmontarse" | Simétrico | Solo al montarse | D-266 | "Cross-fade al montarse; al desmontarse, corte tras 1000ms (D-266)" |

### `componentes/marquee-text.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 38-40 🔴 gap entre vueltas | Pendiente | `MARQUEE_LOOP_GAP=24` (`aura_marquee.c:33`), token "provisional G11" | D-086 (G11) | "24px de separación entre el final de una vuelta y el inicio de la siguiente (D-086, provisional — vetable por el dueño)". Cerrar el pendiente |

### `componentes/scroll-indicator.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 22-23, 44-45 🔴 alto exacto y hex del gris | Pendientes | Alto **24px**; gris = `A26_SHELL_RAIL` del tema (`#C6C6C8` claro / `#3A3A3C` oscuro); tokens "provisional G8" | D-086 (G8) | "Alto fijo 24px; gris = `SHELL_RAIL` del tema (`#C6C6C8`/`#3A3A3C`) (D-086, provisional — vetable)". Cerrar ambos |

### `componentes/selector.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 19-24 "Color: el ítem seleccionado usa `--color-accent`" (se lee como pastilla de acento) | Pastilla de acento | Pastilla **gris `A26_SELECTION_FILL`**; el ACENTO va en texto/ícono/chevron del ítem seleccionado (`aura_selector.c:9-25`, `aura_menu_list.c:258`); el propio comentario del código dice "el documento fuente queda por actualizar" | D-112 (corrección directa del dueño 2026-08-12) | Reescribir "Color": "La pastilla es gris (`SELECTION_FILL`, varía por tema); el `--color-accent` tiñe el TEXTO, el ÍCONO (variante `-on`) y la flecha del ítem seleccionado — no la pastilla (D-112)" |

### `componentes/cover-flow.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 81-82, 272-274 "snap con aceleración… umbral exacto del giro sostenido pendiente" | Curva sin formalizar | `aura_wheel.h:16-26`: paso=1 a velocidad baja, crece con v² hasta 420°/s, tope ×3 (mismo `aura_wheel_step()` que las listas); `aura_coverflow.c:146-153`: "giro sostenido" no existe en clickwheel (eventos discretos), `scrolling` = posición animada aún no alcanza el objetivo | D-077, D-103 | Reemplazar el pendiente por: "**Curva de aceleración (D-077/D-103)**: paso=1 hasta velocidad baja, crece con v² y tope ×3 antes del umbral de 420°/s. No existe 'giro sostenido' como gesto: `scrolling` = 'la posición animada aún no alcanza el álbum objetivo'." Quitar el checkbox |
| 106-109 cita 220ms `CF_SCROLL_ANIM_MS` | Ya resuelto en doc | Código sigue con `TODO(pendiente-doc)` D-103 | D-103 | Sin cambio en doc → RESYNC-GAPS: marcador de código obsoleto |
| 86-90 "sobre el álbum que ya suena, alterna pausa/reanudar" | Ya ratificado en doc | Código sigue con `TODO(pendiente-doc)` D-115 | D-115 | Sin cambio en doc → RESYNC-GAPS: marcador obsoleto |

### `componentes/left-panel.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 96 "Texto: SF Pro Regular, 12px (D-195)" | Regular 12 | `menu_item: ds_semibold_15` → **Semibold 15px** | D-207 (→Semibold 14), D-267 (+1 → 15) | "SF Pro **Semibold, 15px** (D-195: 12 Regular → D-207: Semibold 14 → D-267: 15)" |
| 97, 104, 110-112 "Switch — dimensiones pendientes" | Sin medidas | `aura_menu_list.c:90-93`: pista **28×14** cápsula, perilla **15×10** cápsula, margen 2; encendido pista acento + perilla tono claro del acento; apagado pista gris + perilla casi blanca. (El `TODO(pendiente-doc)` de `aura_menu_list.h:19` describe la versión vieja 22×12 de D-111 — obsoleto) | D-165 → D-167 (corrección del dueño con referencia visual 2026-08-13) | Añadir fila Switch con esas medidas y colores (D-165/D-167). Cambiar "dimensiones pendientes" → "switch confirmado; texto/carga siguen diferidos". → RESYNC-GAPS: TODO de `aura_menu_list.h:19` obsoleto |
| 105-108 checkmark | Sin medidas | Ícono de 14px, mismo lenguaje que íconos de fila | D-111 §3 | Añadir "ícono 14px (D-111)" |
| 138-139 "¿LeftPanel tiene entrada/salida propia en (full)→(split)?" | Abierto | `reveal_behind_panels_exit()` (D-267) / `Lift-and-Push`: entra empujado con su StatusBar (split); sin animación independiente | D-267 | "**Resuelto (D-267):** no. En `(full)→(split)` entra empujado desde el borde izquierdo junto con su StatusBar (split) — fase 3 de `Lift-and-Push`. Sin entrada/salida propia." |

### `componentes/search-keyboard.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 46 "Rueda: recorre la tira" | No dice si envuelve | `kb_advance()` **circular** — de 'A' hacia atrás cae en '_' (`aura_search.c:78-96`) | D-233 Fix 1 | "…recorre la tira **en loop**: ciclo cerrado, no lista con extremos (D-233)" |
| 52 "Menu: sale conservando lo escrito" | Una pulsación sale | Con texto, la **primera** MENU regresa el cursor a 'A' y arma la salida; solo la **segunda** consecutiva sale; sin texto sale de inmediato (`aura_search.c:65-70, 372-395`) | D-233 Fix 2 | "Menu: sin texto, sale. Con texto, la primera pulsación regresa el cursor al inicio de la tira y 'arma' la salida; una segunda Menu (sin otro botón en medio) sale conservando lo escrito (D-233)" |
| 56-59 "persisten… salir con Menu a cualquier profundidad y volver reanuda" | Persistencia total | `aura_search_reset()` al volver hasta el **menú raíz**; Búsqueda→Música→Búsqueda conserva | D-233 Fix 3 | "…persisten mientras la navegación no regrese al **menú principal**: salir al submenú Música y volver reanuda; llegar al menú principal vacía la búsqueda (D-233)" |
| 15 "y=92 en adelante: resultados" | y=92 | `KB_RESULTS_Y = 44+34+8 = 86`, filas 18px — nunca fue 92 (error de cálculo de la doc, mismo commit) | D-159 | "y=86 (caja + 8px), filas de 18px" |

### `componentes/world-clock.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 44-45 "alimentará Zona horaria de Ajustes **cuando exista**" | Futuro | Existe: `AURA_SCREEN_SETTINGS_TIMEZONE`, `draw_timezone()` (`aura_screens.c:3157`), misma tabla de 40 ciudades | D-164 | "Ese mismo dato alimenta Ajustes → Fecha y hora → **Zona horaria** (D-164): una sola tabla de husos/ciudades en todo el firmware" |

### `componentes/date-editor.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 1-5, 26 "(stub)… contenido y comportamiento no descrito todavía" | Sin contenido | **Pantalla real**: `draw_date_edit()` (`aura_screens.c:2822`): rejilla del mes (misma de Calendarios) como selector, cabecera "Mes Año" Bold 12 con campo activo en acento, fecha completa en español debajo; SELECT avanza día→mes→año y confirma; MENU cancela; persiste con `rtc_write_datetime()` | D-170 | Sustituir "Lo que sabemos"/pendiente de contenido por esa descripción citando D-170. Título como componente real, no stub. **Conservar** las secciones de Shift-and-Reveal e ícono dinámico como intención (B) |

### `fundamentos/02-tipografia.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 3 "🟡 Parcial — primer token (StatusBar), resto pendiente" | Parcial | 11 roles `type_scale_roles` con consumidor real | D-086, D-267, D-271 | Cabecera → "🟢 Todos los roles con consumidor real están definidos (11 roles, 9 estilos `ds_*`). Huecos: pantallas sin token propio siguen usando estilos compartidos" |
| 57-58, 73 `DS_SEMIBOLD_14`; "único hueco de `MAXUSERFONTS=12`" (×2) | 14 / =12 | `DS_SEMIBOLD_15` (`apple2026_shell.h:76`); `MAXUSERFONTS 14` (`font.h:64`; 12→13 D-263, 13→14 D-267) | D-263, D-267 | Renombrar; añadir bloque "Presupuesto de fuentes" (ver §4) |
| 66 `--font-menu-item` 14px Semibold | 14 | `menu_item = ds_semibold_15` | D-267 | "15px (D-267 +1pt; antes 14 D-207, 12 D-195, 10 original)" |
| (falta) fila para SelectionSummary | — | `selection_summary_text_top = ds_bold_18`, `_bottom = ds_medium_16`, blanco fijo | D-267, D-271 | Añadir sección "Tokens de SelectionSummary (D-271)" (ver §4) |
| 71-73 `A26_TYPE_BODY` "13px Regular… sin exceder MAXUSERFONTS=12" | =12 caduco | Reuso de `ds_bold_14` para filas (D-205) vigente, justificación caduca | D-205, D-263 | Corregir a "sin agregar `.fnt` nuevo" y quitar "=12" |

### `fundamentos/04-bordes.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| Todo (3 líneas) "🟡 Pendiente — definir radios… grosores" | Pendiente | Sistema completo: `corner_radius_screen=12/card=8/pill=8/capsule=6`, `selector.corner_radius=5`, `selection_summary.tile_corner_radius=28`, `cover_flow.corner_radius=8`, `search.pill_radius=11` (interior 6); primitivas `a26_shell_fill_rounded_rect()`/`stamp_corners()`/`round_bitmap_corners()` | D-072, D-083, D-097, D-116, D-211→D-236, D-268; search 11px = encargo 2026-08-13 sin D-XXX (citar el token, no inventar) | Reemplazo completo por tabla (ver §4). Marcar 🟢. Dejar 🔴 solo para el separador entre paneles (ver C) |

### `fundamentos/01-color.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 3 "🟡 Pendiente — traer valores desde las guías Apple 2026" | Pendiente | Paleta completa en `tokens.json → color.light/dark` (9 tokens ×2 temas) + `aura_ds.color.*` | D-072 | Cabecera → 🟢; añadir tabla de la paleta base (shell_bg, text_primary/secondary/tertiary, accent, shell_rail, progress_fill/track, selection_fill, white_constant) con hex claro/oscuro |
| (falta) fondos de panel por acento | — | `right_panel_background.presets=["pink"]`; sigue el ACENTO; fallback pink | D-267 | Añadir párrafo "Fondo del panel derecho por acento (D-267)": imagen 160×240 por preset, `design-system/assets/panel-backgrounds/<nombre>-source.png`; solo `pink` disponible; fallback explícito |
| 52-60 tabla `--color-bg-base/-panel-left/-panel-right/-text-*` vacía | Vacía | Roles existen en `a26_palette` | D-072 | Rellenar: `--color-bg-base`→`A26_SHELL_BG`; `--color-bg-panel-left`→`A26_SHELL_BG`; `--color-bg-panel-right`→imagen `right_panel_background` (split de menú) / `SHELL_BG` (resto); `--color-text-primary/secondary`→`A26_TEXT_PRIMARY/SECONDARY` |

### `fundamentos/05-breakpoints.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| Todo "🟡 pendiente formalizar como tabla" | Informal | `split`: LeftPanel 160 + derecho 160, StatusBar 20 (`LEFT_PANEL_WIDTH=160`, `STATUSBAR_HEIGHT=20`); `full`: 320; `aura_widgets_split_active()`; política en `sistema/02` | D-072/D-086, política 2026-08-13 | Tabla: `split` = 160/160, StatusBar 20px sobre panel izq.; `full` = 320, StatusBar 20px ancho completo; regla de asignación → `sistema/02`. Marcar 🟢 |

### `efectos/01-sombras.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| 9-13 "el usuario puede desactivar este efecto desde Ajustes" | Genérico | **Existe**: Ajustes → "Mostrar sombras", `aura_shadows_enabled()`, apaga LeftPanel/CoverDrift/NP Modo 4 | D-088, D-154 | "…se desactiva con **Ajustes → Mostrar sombras** (`aura_shadows_enabled()`, D-088/D-154), que apaga TODAS las sombras del sistema" (nota: la del tile de SelectionSummary hoy lo ignora — hueco B, no se documenta como diseño) |
| 22-25 "[ ] Valores: offset, blur, opacidad, color" | Pendiente | **LeftPanel→contenido**: 8px, 20% máx., caída lineal, negro; **tile SelectionSummary**: offset_y 4, 35%, blur 12 SDF, negro | D-088 (8px/20% provisional), D-258, D-267/D-270 | Marcar resuelto con tabla de dos filas; anotar que 8px/20% siguen "provisional" en `tokens.json` |
| 19-21 "[ ] Qué significa 'máximo rendimiento'" | Pendiente | La propia nota histórica del doc dice que quedó reemplazado por el toggle | D-088 | Eliminar el ítem (obsoleto por la propia reformulación) |
| 26-31 "[ ] ¿animación de entrada/salida? [ ] ¿split y full?" | Pendiente | LeftPanel shadow: solo `split`, estática; NP Modo 4: fade propio ≈165ms | D-141, D-088 | Responder ambas; dejar abierta solo "otros componentes futuros" |

### `transiciones/00-vocabulario.md`

| Línea | Doc | Código | D-XXX | Cambio propuesto |
|---|---|---|---|---|
| `Flip-and-Flow` sin timing | — | `FLOW_MS 500` "decisión del dueño (antes 350 provisional)" (`aura_transitions.c:678`) | Sin D-XXX localizable (grep negativo); citar el comentario de código como fuente | "**Duración: 500ms** (dueño; antes 350 provisional). Vive en `FLOW_MS`, sin token en `tokens.json`" |
| Pendiente "[ ] `(full)→(split)` reversa" | Abierto | Resuelto en el mismo doc (`Lift-and-Push`) y en código (`reveal_behind_panels_exit()`) | D-267 | Borrar el checkbox (contradice la sección Lift-and-Push que ya existe) |

### `00-INDICE.md`

| Línea | Doc | Realidad | Cambio propuesto (celda exacta) |
|---|---|---|---|
| 31 "🟢 10 patrones documentados: …" | 10 | 11 (Lift-and-Push 2026-08-13) | "🟢 11 patrones documentados: `Morph Directo`, `Push-and-Drop`, `Lift-and-Push`, `Fade-on-Idle`, `Marquee Loop`, `Shift-and-Reveal` (sin implementar, depende de `DateEditor`), `Fade-Slide`, `Scroll-Slide`, `Drop-and-Lift`, `Push-and-Pull`, `Flip-and-Flow` (500ms). 🔴 Timings de Push/Drop sin token" |
| 29 "`NowPlaying` CERRADO 2026-08-12" | Cerrado | 2 valores provisionales en doc y código | "`NowPlaying` diseño cerrado 2026-08-12 con 2 valores provisionales (stagger de entrada D-113; umbrales de silencio del `LyricsPanel` 8s/3s)" |
| 27 fila `fundamentos/` | "🟡 Tipografía 9 tokens…; color con `--color-accent`; resto pendiente" | Tipografía 11 roles; color completo; bordes completos; breakpoints funcionales; espaciado sin empezar | "🟡 Tipografía 🟢 (11 roles, presupuesto 14/14 fuentes); color 🟢 (paleta 9 tokens ×2 temas, acento configurable, color por categoría, fondo de panel por acento D-267); bordes 🟢 (tabla completa de radios); breakpoints 🟢 (`split` 160/160, `full` 320); **espaciado ⚪** (sin escala base)" |
| 32 fila `efectos/` | "🟡 Regla capturada, faltan valores" | Valores + toggle existen | "🟡 Sombras con valores (LeftPanel 8px/20% lineal; tile SelectionSummary offset 4/35%/blur 12) y toggle global 'Mostrar sombras' (D-154). Sin otros efectos" |
| 29 fila `componentes/` — `SelectionSummary`, `DateEditor` | "SelectionSummary y CoverDrift con diseño y comportamiento definidos… DateEditor es solo stub" | SelectionSummary rediseñado D-267–D-272; DateEditor tiene pantalla real (D-170) pero sin Shift-and-Reveal ni ícono dinámico | "`SelectionSummary` (rediseño D-267–D-272: fondo por acento, tipografía medida, sombra SDF) y `CoverDrift` definidos… `DateEditor` con pantalla real (D-170) pero transición e ícono dinámico pendientes" |

---

## 2. Tipo B — código incompleto, la doc tiene razón (→ `RESYNC-GAPS.md`, sin tocar doc)

Priorizado por impacto visible al usuario:

| # | Hueco | Dónde | Evidencia |
|---|---|---|---|
| B1 | **Título de StatusBar nunca hace marquee** — solo recorta | `aura_status_bar_v2.c:265-266` pasa `marquee_elapsed_ms=0` siempre | `status-bar.md:198`, `dynamic-title.md:27-31` prometen MarqueeText |
| B2 | **DynamicTitle nunca dispara Fade-Slide/Scroll-Slide** — motor completo, único llamador pasa `prev_text=NULL`; el concepto "sección alfabética" tampoco alimenta el título | `aura_status_bar_v2.c:262-266`, `aura_dynamic_title.c:35` | `dynamic-title.md:33-49` |
| B3 | **Sombra del tile de SelectionSummary ignora "Mostrar sombras"** — la única sombra que no consulta `aura_shadows_enabled()` | `aura_selection_summary.c:462` (`draw_tile_shadow()`) | D-154: el toggle gobierna "todas" |
| B4 | **Difuminado de 4px en extremos de MarqueeText no implementado** — el token `MARQUEE_EDGE_BLUR=4` existe con cero consumidores; `aura_marquee.c` hace recorte duro | `apple2026_tokens.h:177`, `aura_marquee.c` | `marquee-text.md:22-26`, `dynamic-title.md:27-31` |
| B5 | **Shift-and-Reveal sin implementar** — cero referencias en código; DateEditor entra por la transición estándar | — | `transiciones/00-vocabulario.md`, `date-editor.md:11-14` |
| B6 | **Ícono dinámico de hoja de calendario** para la fila Fecha — solo existe la variante reloj analógico; Fecha usa "calendar" estático | `aura_screens.c:242` | `date-editor.md:9-10`, `selection-summary.md:90-96` |
| B7 | **Ícono de carga (spinner) del Selector** — solo CHEVRON/NONE, sin consumidor async real | `aura_selector.h:11-19` | `selector.md:33-36, 55` |
| B8 | **ScrollIndicator se mueve a saltos** — `thumb_y` proporcional a `first` entero, sin interpolación | `aura_scroll_indicator.c:29-33` | `scroll-indicator.md:36-38` "animada/suave" |
| B9 | **Editores de Fecha/Hora sobre StatusBar vieja** — `draw_date_edit()`/`draw_time_edit()` usan `aura_widgets_draw_status_bar()`, no v2 | `aura_screens.c` | Deuda de código, no de doc |
| B10 | Escala de espaciado base (`03-espaciado.md`) — hueco de DISEÑO, no de código: el código no la decidió, la evitó | — | — |
| — | Marcadores `TODO(pendiente-doc)` **obsoletos** en código (la doc ya los resolvió): `aura_coverflow.c:163` (220ms, ya en doc), `aura_coverflow.c:1194` (PLAY alterna, ya en doc), `aura_menu_list.h:19` (switch 22×12, superado por 28×14 de D-167). Se limpian en una pasada de código posterior, no en esta | | |
| — | Comentarios de código obsoletos: `aura_selector.h:1-6` ("la pastilla misma es del color de acento", falso desde D-112); `tokens.json selector.comment_tint` describe un uso que ya no existe. Inconsistencia diario↔código: D-116 dice ScrollIndicator del reverso en `TEXT_SECONDARY`, `aura_coverflow.c:963` pasa `SHELL_RAIL` | | |

---

## 3. Tipo C — preguntas para el dueño (→ `RESYNC-PREGUNTAS.md`)

| # | Pregunta concreta | Contexto |
|---|---|---|
| C1 | ¿Ratificas **claro arriba-izquierda → oscuro abajo-derecha** para el degradado del tile de SelectionSummary? | `TODO(pendiente-doc)` D-097 vigente; `selection-summary.md:72-74` |
| C2 | ¿Aceptas `SHELL_BG` **sólido** como fondo definitivo de la StatusBar, o quieres alpha-blend real ahora que en `(full)` sí hay contenido detrás (NP/CoverFlow)? | D-096 explícitamente provisional; `status-bar.md:115-127` |
| C3 | ¿Confirmas que Drop-and-Lift **vertical** en `(split)` y Push-and-Pull **horizontal** en `(full)` para el ClockIndicator es intencional? | Implementado así (D-108), nunca ratificado; `clock-indicator.md:46-50` |
| C4 | ¿**220ms** es el valor final de Drop-and-Lift/Push-and-Pull del reloj (`AURA_SB2_CLOCK_ANIM_MS`)? | D-108 explícitamente provisional |
| C5 | ¿Ratificas el stagger de entrada a NowPlaying: **tres grupos en paralelo + StatusBar al final, 8 cuadros (4 en modo reducido)**? | D-113, doc y código coinciden como provisional |
| C6 | ¿Ratificas **8s de hueco mínimo / 3s de lectura** para silencios del LyricsPanel? | `LYR_SILENCE_MIN_MS/LINE_READ_MS`, provisionales en ambos lados |
| C7 | ¿Se aceptan **260ms por fase** para el giro SELECT→reverso / reverso→carrusel (`CF_FLIP_MS`)? Es distinto del vuelo de 500ms ya confirmado | D-104; la sección original de la doc fue reemplazada y el pendiente se perdió |
| C8 | Vuelo CoverFlow→reproductor: gira en el mismo sentido que la apertura del flip — ¿lo confirmas o quieres invertirlo? | `cover-flow.md:275-276`, sin verificación en vivo registrada |
| C9 | ¿La lista de **6 presets de acento con nombre** (Rosa/Rojo/Naranja/Verde/Azul/Morado) es el diseño final, o sigue pendiente un selector visual de swatches? | D-087 provisional; `01-color.md:5-8` |
| C10 | `A26_ACCENT` (del tema, `#FF2D55`/`#FF456C`) y `aura_accent()` (usuario, `#FF2D52` default) coexisten como dos "acentos" — ¿documentar la separación y quién gana dónde, o está previsto fusionarlos? | D-086 los mantiene aparte deliberadamente |
| C11 | Entre LeftPanel y el panel derecho no hay línea: la separación la da solo la sombra. ¿Es diseño (se documenta así) o falta un separador de 1px `SHELL_RAIL`? | `04-bordes.md` pide "grosores de separadores" |
| C12 | Push-and-Drop: ¿los ≈133ms de push (8 cuadros@60Hz) y ≈83ms de drop (5 cuadros) actuales son el diseño (→ tokenizar y documentar) o provisionales? | `aura_transitions.c:281-287, 359, 467`, hardcodeados sin token ni D-XXX |
| C13 | ¿24px de gap entre vueltas de MarqueeText es final? (Si sí, el A correspondiente ya lo documenta como "provisional vetable") | G11 |
| C14 | MarqueeText al navegar fuera a media vuelta: hoy se interrumpe al instante y reinicia desde 2s estático al volver — ¿definitivo? | `marquee-text.md:41-42`; `aura_selection_summary.c:242-247` |
| C15 | `sistema/01-capas` (44-47): (1) ¿`--layer-content` puede tener más de un elemento apilado, o estrictamente 0 o 1? (2) ¿Existen casos donde `--layer-base` cambia de contenido sin transición de capa? — el debounce del panel derecho (D-262/D-266) es evidencia de que sí; ¿eso responde la (2)? | Sin resolución en código |

---

## 4. Bloques de texto propuestos completos

### `fundamentos/02-tipografia.md` — sección nueva tras "Tokens de LeftPanel"

```
## Tokens de SelectionSummary (D-271, 2026-08-15)

Medidos en píxeles contra un mockup pixel-exacto del dueño (161×240 ≈ el
panel real de 160×240), no tomados de un número de punto sin verificar
(D-267 había fijado 13/12pt, que resultaba más chico que el objetivo).
Ambos en **blanco fijo** sobre la imagen de fondo del panel (D-267), no
`A26_TEXT_PRIMARY`.

| Token | Tamaño | Peso | Uso |
|---|---|---|---|
| `--font-selection-summary-top` | 18px (`ds_bold_18`) | Bold | Texto superior (título de la fila), 1 línea |
| `--font-selection-summary-bottom` | 16px (`ds_medium_16`) | Medium | Texto inferior (descripción, hasta 2 líneas por palabra) — único consumidor de la cara Medium |

## Presupuesto de fuentes (estado real)

`MAXUSERFONTS` (`firmware/export/font.h`) **ya no es 12**: subió a 13
(D-263, decisión del dueño para no comprometer un tamaño pedido) y a 14
(D-267, neto -2/+3 estilos). Hoy: 5 fuentes Apple2026 + 9 estilos `ds_*`
(`ds_reg_8`, `ds_reg_10`, `ds_bold_10`, `ds_reg_12`, `ds_bold_12`,
`ds_bold_14`, `ds_semibold_15`, `ds_bold_18`, `ds_medium_16`) = 14/14, sin
hueco libre. Cualquier estilo nuevo exige retirar uno o subir el límite.
```

### `fundamentos/04-bordes.md` — reemplazo completo

```
# Bordes y Radios

🟢 Definido — un solo algoritmo de esquina por corte de distancia
(`a26_shell_fill_rounded_rect()`/`a26_shell_stamp_corners()`/
`a26_shell_round_bitmap_corners()`, `apple2026_shell.c`), sin antialias
salvo donde se indica. Todos los radios viven en `design-system/tokens.json`.

| Token | Valor | Uso | Origen |
|---|---|---|---|
| `layout.corner_radius_screen` | 12px | Las 4 esquinas físicas de pantalla (en `split` solo las 2 izquierdas: D-268) | D-072 |
| `layout.corner_radius_card` | 8px | Tarjetas | D-072 |
| `layout.corner_radius_pill` | 8px | Pastillas genéricas | D-072 |
| `layout.corner_radius_capsule` | 6px | Cápsulas (flotante de espera, etc.) | D-072 |
| `aura_ds.metrics.selector.corner_radius` | 5px | Pastilla del `Selector` | D-097 |
| `aura_ds.metrics.selection_summary.tile_corner_radius` | 28px (~31% de 90px) | Tile de `SelectionSummary`. Arco circular, no squircle G2: al mismo % se percibe menos redondo, por eso 28 y no el 22% literal de Apple | D-211 (8→20), D-236 (20→28) |
| `aura_ds.metrics.cover_flow.corner_radius` | 8px | Carátula en carrusel, reverso, reproductor y vuelo | D-083, D-116 |
| `aura_ds.metrics.search.pill_radius` | 11px (campo interior concéntrico: 6px) | Caja de búsqueda de 34px | encargo 2026-08-13 (`tokens.json`, `comment_radius`) |

Grosores: `scroll_indicator.thickness` = 4px. Entre `LeftPanel` y el panel
derecho no hay línea de borde: la separación la da la sombra
(`efectos/01-sombras.md`). 🔴 Pendiente confirmar con el dueño si eso es
diseño o falta un separador de 1px.
```

---

## 5. Secciones verificadas SIN divergencia (constancia de cobertura)

- **selection-summary.md**: tile 90×90 r28 símbolo 60; color de tile por categoría (D-236/D-250); MarqueeText 2s+5s; regla "ícono siempre cambia" (salvo excepción D-269); sombra de LeftPanel con compositing real.
- **status-bar.md**: alto 20 / ancho 160-320 / padding 4 / gap 4 / íconos 12 / batería 21; orden por estado; jerarquía candado/play-pausa/batería; título centrado real en (full); regla split⇔LeftPanel.
- **clock-indicator.md**: auto-hide 10 000ms; HH:MM 12/24h; (split) centrada + Drop-and-Lift; (full) izquierda + Push-and-Pull.
- **dynamic-title.md**: anchos 60/80/120 = tokens; comportamiento del MOTOR coincide (solo no cableado, B).
- **cover-drift.md**: 8 direcciones sin repetir; 7 000ms; velocidad constante; subpixel; 320×320; fundido 600ms; ≥3 imágenes; comparación por categoría; sombra D-258.
- **marquee-text.md**: activación solo por desborde; 2s+5s; dos copias.
- **scroll-indicator.md**: grosor 4; pill; >10 ítems; Fade-on-Idle 1500+500ms.
- **selector.md**: 152×22 r5; salta sin animación (D-212/D-214); indicador 12px a 4px; fila inerte 50%.
- **now-playing.md**: geometría de carátula (x=10, y=43, 135px, 7°), barra 300×7/298×5 a 44px, thumb 15×11, transporte gap 20, panel Modo 4 130px, barra 122px, tipografía — todo en tokens.
- **cover-flow.md**: geometría 130px/y=30/3 laterales/tilt/spacing/fade/reflejo/radio; PLAY in-place; salto ±10; sin loop; zoom 216/256 con 150/220ms; reverso 200×200 con todos sus valores; entrada con/sin CoverDrift (D-259); vuelo 500ms.
- **left-panel.md**: 160×240, padding 4, 220/212/152, filas 31×7, ícono 20 a 14px, gap 4, divisor 1×152.
- **lyrics-panel.md**: paneles 130/190, barra 122, hoja desde la derecha, alpha 216, color promedio ±15%, tinta por luminancia, jerarquía 256/176/150/76, sombra 8px con fade ≈165ms, tipografías, 3 puntos de 6px.
- **search-keyboard.md**: tira A-Z/0-9/_, y=26 Reg 8, caja y=44 h34 r11, campo 84×24 r6, ventana con 2 de contexto, activo Bold 14 blanco vs Reg 12 al 59%, 22 chars + "…", cursor HZ/2, Backward mantener borra, Play → resultados, subcadena.
- **world-clock.md**: filas 52, esfera r=20, Bold 12/Reg 12, esfera clara/oscura, horaria con LUT, 8 regiones, cuartos de hora, hasta 4 husos, menú flotante.
- **sistema/02, 03 (se autoaudita), 04**: coinciden. **modulos/**: ⚪ ambos lados. **transiciones/** `Lift-and-Push`: ya presente.
- **fundamentos/01-color.md** "Color por categoría": hex y regla coinciden.

---

## 6. Plan de commits para Fase 2 (solo tras aprobación)

Un commit por archivo, en este orden (de más a menos desactualizado), sin push:

1. `docs: resync selection-summary.md con D-267/D-269/D-270/D-271/D-272/D-266/D-108/D-264`
2. `docs: resync cover-drift.md con D-262/D-266`
3. `docs: resync status-bar.md con D-205/D-207`
4. `docs: resync clock-indicator.md con D-207/D-108`
5. `docs: resync selector.md con D-112`
6. `docs: resync left-panel.md con D-207/D-267/D-165/D-167/D-111`
7. `docs: resync search-keyboard.md con D-233/D-159`
8. `docs: resync cover-flow.md con D-077/D-103`
9. `docs: resync world-clock.md con D-164`
10. `docs: resync date-editor.md con D-170 (pantalla real; transición e ícono siguen pendientes)`
11. `docs: resync marquee-text.md con D-086 (G11)`
12. `docs: resync scroll-indicator.md con D-086 (G8)`
13. `docs: resync fundamentos/02-tipografia.md con D-263/D-267/D-271/D-205`
14. `docs: resync fundamentos/04-bordes.md (tabla completa de radios)`
15. `docs: resync fundamentos/01-color.md con D-072/D-267`
16. `docs: resync fundamentos/05-breakpoints.md (tabla split/full)`
17. `docs: resync efectos/01-sombras.md con D-088/D-154/D-258/D-267/D-270/D-141`
18. `docs: resync transiciones/00-vocabulario.md (FLOW_MS 500, quitar pendiente resuelto)`
19. `docs: resync 00-INDICE.md (11 patrones, estados reales de fundamentos/efectos/componentes/NowPlaying)`
20. `docs: RESYNC-GAPS.md + RESYNC-PREGUNTAS.md` (y corrección de las 4 premisas en `RESUMEN.md`/`BLOCKED.md`)

Verificación final: `git diff --stat` debe tocar únicamente `docs/aura-design-system/` y los reportes de la raíz — nada en `firmware/`, `studio/`, `design-system/`.

---

**BARRERA: no se aplica nada de lo anterior hasta que el dueño apruebe este plan** (completo, o indicando qué filas A se aceptan, cuáles se reclasifican, y qué respuestas C quiere dar ahora).
