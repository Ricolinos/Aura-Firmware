# PLAN — Niveles de Animaciones y Gráficos (niveles-fx)

> Fuente normativa: matriz de niveles entregada por el dueño (2026-08-18). Canon visual: `docs/aura-design-system/` — "Todas" referencia el design system, no se re-especifica aquí.

## Estado de ejecución (2026-08-18)

Aprobado con las recomendaciones del plan (Q1–Q6). **Ejecutado**: §7 (arquitectura, `aura_fx.h`), §3 (delays de Gráficos=Todos), §8.4 (CoverDrift en los 3 niveles de Gráficos + eje Animaciones), §5/§9 (5 imágenes residentes de Mínimos), §8.5 (SelectionSummary Gráficos=Ninguno, Q1/Q5), §8.1/§8.3 (sustitución de Flip-and-Flow/Flow-Return por push en Mínimas), §8.2 (Modo 4 → Fade-Slide en Mínimas), Q6 (debounce legado de listas de contenido). Decisiones registradas en `DECISIONS.md` D-310 a D-315. Build ARM + simulador limpios, `make -C firmware/rockbox/apps/aura/test test` 11/11 sin regresiones.

**Deliberadamente no ejecutado, pendiente:**
- §6.3 — optimización del morph del Modo 4 en hardware (D-315): el perfil estático identificó los sospechosos, pero cada paso de optimización exige medir con la instrumentación D-300 en un iPod real antes/después, algo que este entorno no puede hacer. El diagnóstico completo queda en `now-playing.md` § Niveles de reducción del Modo 4 y en §6 de este documento.
- §14.1/§14.3 — doble decodificación por ciclo de `CoverDrift` y `aura_coverdrift_animating()` que nunca vuelve a `false` (hallazgos laterales, D-311): riesgo de introducir un bug real sin poder compilar-y-medir superó el beneficio en esta pasada.
- Verificación **en hardware real** de todo lo demás (los dos delays de 500ms, el push sustituto Music Flow↔Reproductor, el Fade-Slide del Modo 4, las 9 combinaciones de niveles, cambio en caliente sin estados huérfanos) — este entorno no tiene acceso a un iPod físico ni a una sesión interactiva del simulador; solo se verificó que compila y que la lógica pura (tests de host) sigue pasando.

## 0. Resumen ejecutivo — premisas del encargo que hay que corregir antes de planear

El inventario (FASE 0) confirmó que la arquitectura pedida ya existe a medias: `animation_mode` tiene 3 niveles reales y todas las transiciones lo consultan con el mismo patrón; `graphics_mode` existe pero es binario de facto. Cuatro premisas del encargo no coinciden con el código/canon actual:

| # | Premisa del encargo | Realidad | Consecuencia |
|---|---|---|---|
| P1 | "CoverDrift no se monta con menos de 10 imágenes según su spec" | El umbral es **3** desde D-254 (`apple2026_tokens.h:223`, `cover-drift.md` §Activación) | El modo de 5 imágenes es compatible con el umbral vigente sin regla extra (ver §5) |
| P2 | SelectionSummary: "delay de actualización 2s (actual)" | El delay actual es **1000 ms** (D-266, cadencia asimétrica: 2000 ms solo hacia CoverDrift) | "Mínimas" = conservar 1000 ms, no 2000 (ver Q2) |
| P3 | "solo 5 imágenes aleatorias en loop **para limitar RAM**" | El loop actual es **solo de índices**: en RAM viven siempre 2 imágenes (2×200 KB), se recorra 5 o 300 | El claim de RAM es falso tal cual; lo que sí se puede ganar es **disco/batería** (ver §5 y §9) |
| P4 | La matriz define contenido del panel derecho con Gráficos=Ninguna (degradado de acento; solo texto) | Regla dura vigente (`status-bar.md`, `aura_widgets_split_active()`): con Gráficos=Ninguno **no existe el panel** — todo pasa a (full) | Conflicto frontal. Es la pregunta abierta principal (Q1) |

Nada de esto invalida el encargo; cambia detalles de tres celdas y exige una decisión explícita en Q1.

---

## 1. FASE 0.1 — Inventario del código

Base: `firmware/rockbox/apps/aura/`.

### 1.1 — Ajuste "Animaciones"

| Qué | Dónde |
|---|---|
| Enum `aura_anim_mode_t` (`AURA_ANIM_NONE/MINIMAL/ALL`) | `aura_settings.h:45-50` |
| Campo `animation_mode`, default `MINIMAL` | `aura_settings.h:97`, `aura_settings.c:47` |
| Persistencia `aura.cfg` clave `animation_mode` | lectura `aura_settings.c:264-268`, escritura `:347`; migración desde `graphics_mode` en cfg viejos `:328-329` |
| UI (Ajustes → Personalización → Animaciones) | `aura_screens.c:273`, elección `:448-450`, aplicación `:646-648`; strings ES "Ninguna/Mínimas/Todas" `aura_lang.c:71-73` |
| **Consumo: 24 lecturas en 2 archivos**, patrón uniforme | `aura_transitions.c` (gates en `:469, :531, :636, :890, :1094, :1373` + bifurcación ALL/MINIMAL de cuadros/fps en ~15 sitios) y `aura_nowplaying.c:1659,1662` (`mode4_morph`) |

Patrón repetido a mano en cada sitio: gate `== AURA_ANIM_NONE` (return temprano, junto con `lcd_active()`) + calidad `== AURA_ANIM_ALL ? tokens_ALL : tokens_MINIMAL` (`apple2026_tokens.h:185-194`: 8/4 cuadros, 60/45 fps, drop 5/3). **No existe ningún helper central** (`aura_fx_level()` ni similar).

Granularidad actual: los 3 niveles son reales pero **globales** — ningún sitio distingue por componente; MINIMAL y ALL solo difieren en cuadros/fps, nunca en *qué* transición corre. Excepción relevante: **las animaciones internas de Music Flow (scroll, zoom, flip, marquee) no consultan `animation_mode` en absoluto** (`aura_musicflow.c`, state-driven contra `current_tick`) — casualmente es exactamente lo que la celda "Ninguna" pide conservar.

### 1.2 — Ajuste "Gráficos"

| Qué | Dónde |
|---|---|
| Enum `aura_gfx_mode_t` (`AURA_GFX_NONE/MINIMAL/ALL`) | `aura_settings.h:54-59` |
| Campo `graphics_mode`, default `MINIMAL` | `aura_settings.h:98`, `aura_settings.c:48` |
| Persistencia clave `graphics_mode` | `aura_settings.c:269-270`, `:348` |
| UI | `aura_screens.c:274`, `:451-453`, `:649-651`; strings "Ninguno/Mínimos/Todos" `aura_lang.c:75-77` |
| **Consumo: solo 2 sitios reales, ambos binarios** | `aura_widgets.c:345-349` (`aura_widgets_split_active()`: `!= AURA_GFX_NONE` apaga LeftPanel/panel derecho/barra split; 8 consumidores) y `aura_screens.c:5474` (ancho de transición T1 vs T3) |

**`AURA_GFX_MINIMAL` y `AURA_GFX_ALL` son hoy indistinguibles**: ningún sitio compara contra ellos. El nivel es un booleano con 3 etiquetas. Comentario obsoleto en `aura_musicflow.h:23-27` (describe un acoplamiento con `AURA_GFX_ALL` revertido en D-025).

### 1.3 — Los delays del panel derecho

Un solo mecanismo: `render_panel_debounced()` en `aura_screens.c:1217-1316`, decisión del plazo en `:1293-1298`:

| Ruta | Token | Valor | Al cumplirse |
|---|---|---|---|
| Hacia CoverDrift | `AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_MS` (`apple2026_tokens.h:150`) | **2000 ms** | fundido real de 600 ms (`start_panel_fade()`, `:1303-1304`; `COVER_DRIFT_CROSSFADE_MS`, `apple2026_tokens.h:224`) |
| Hacia SelectionSummary | `AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_FAST_MS` (`:151`) | **1000 ms** | corte instantáneo (`:1306-1310`) |

CoverDrift **no tiene delay propio** (el temporizador de 3 s de D-254 fue retirado en D-262); su único reloj es el ciclo de movimiento `CYCLE_MS = 7000` (`apple2026_tokens.h:222`). Existe además un debounce legado independiente para la ruta de listas de contenido: `PANEL_RETARDO_TICKS = HZ` (1 s) en `aura_widgets.c:66` — fuera del alcance de la matriz, pero hay que decidir si el nivel "Todas" también lo baja (Q6).

### 1.4 — Conclusión del inventario

- **Animaciones**: 3 niveles reales, globales, sin granularidad por componente, sin punto único de decisión.
- **Gráficos**: binario de facto (`!= NONE`); Mínimos y Todos idénticos.
- Los "dos delays de 2 s" son en realidad **2000 ms (CoverDrift) y 1000 ms (SelectionSummary)**.

---

## 2. FASE 0.2 — Regla de precedencia y matriz de interacción 3×3

**Regla confirmada contra cada celda:** *Gráficos decide qué existe; Animaciones decide cómo se mueve lo que existe. Cuando Gráficos elimina un elemento, Animaciones es irrelevante para ese elemento.* Ninguna celda de la matriz normativa la contradice. Se registra como decisión (§11, D-a).

Corolario que la matriz no dice pero se deriva de la regla: la **rotación de imágenes cada 7 s de CoverDrift es contenido, no animación** — sigue ocurriendo en todos los niveles de Animaciones; lo que Animaciones gobierna es el *cómo* del cambio (fade vs corte) y el movimiento. Igual el **marquee** de textos largos: es legibilidad funcional, no ornamento — se conserva en todos los niveles (hoy tampoco consulta `animation_mode`).

### 2.1 — CoverDrift (filas = Gráficos, columnas = Animaciones)

*Todas las celdas de la fila "Ninguno" dependen de Q1 (¿existe el panel?). Se documentan bajo la lectura recomendada: la matriz manda, el panel existe.*

| G \ A | Ninguna | Mínimas | Todas |
|---|---|---|---|
| **Ninguno** | Panel de **acento con degradado** (§4), estático. Entrada al estado por **corte** (gate de Animaciones apaga el fundido de 600 ms) | Igual de estático; la **aparición conserva el fundido** de 600 ms | Igual; aparición con fundido. Animaciones no tiene nada más que decidir: no hay imágenes |
| **Mínimos** | **5 imágenes** (§5), estáticas (sin drift); rotación cada 7 s por **corte instantáneo** | 5 imágenes, estáticas; rotación cada 7 s **con cross-fade** de 600 ms | 5 imágenes con **comportamiento canónico completo** (drift 7 s + cross-fade) |
| **Todos** | Todas las imágenes, estáticas, rotación por corte; delay de aparición **500 ms** | Todas las imágenes, estáticas, rotación con fade; delay **500 ms** | **Canon completo** (`cover-drift.md`); delay de aparición baja de 2000 a **500 ms** |

Delay de aparición: 2000 ms en filas Ninguno/Mínimos, 500 ms en Todos (los delays viven en Gráficos, §3). En la fila "Ninguno" el debounce y la mecánica de identidad quedan **intactos** (misma espera de 2 s antes de mostrar el panel de acento) — cambiar solo el contenido, no la coreografía, es lo que mantiene un único código con puntos de sustracción (Q4 lo ratifica).

### 2.2 — SelectionSummary (filas = Gráficos, columnas = Animaciones)

Hallazgo honesto: **SelectionSummary no tiene animaciones propias** — sus cambios de valor son cortes por diseño (D-266) y su única pieza móvil es el marquee (funcional, se conserva). Por lo tanto **las tres columnas de Animaciones son idénticas dentro de cada fila**; la matriz de SelectionSummary es efectivamente 3×1:

| Gráficos | Comportamiento |
|---|---|
| **Ninguno** | **Solo texto, sin íconos**: no se dibuja tile (ni degradado diagonal, ni sombra SDF, ni símbolo). Layout de texto: ver Q5. *(Depende de Q1: hoy con Ninguno el panel no existe.)* |
| **Mínimos** | Canon completo (`selection-summary.md`); delay de actualización **1000 ms** — el valor actual (P2) |
| **Todos** | Canon completo; delay de actualización baja de 1000 a **500 ms** |

Interacción con el toggle existente **"Mostrar iconos"** (Personalización): son ajustes ortogonales que se componen con AND — el ícono del tile se dibuja solo si `show_icons && graphics_mode != NONE`. Se documenta en la sección de niveles del componente.

### 2.3 — Celdas ambiguas detectadas

Las ambigüedades no se resuelven en silencio; van a §12 con recomendación: **Q1** (panel bajo Gráficos=Ninguno vs regla dura de `status-bar.md`), **Q2** (1000 ms como "actual" de SelectionSummary), **Q3** (semántica de RAM/disco de las 5 imágenes), **Q4** (debounce/fade del panel de acento), **Q5** (layout de texto sin tile), **Q6** (debounce legado de listas de contenido bajo "Todos").

---

## 3. FASE 0.3 — Decisión: los delays viven en Gráficos (D-b)

La matriz coloca el cambio 2 s → 0.5 s bajo **Gráficos**, no bajo Animaciones. Es literal y así se implementa. Justificación para el registro: *el delay no gobierna movimiento sino cuánto trabajo gráfico se dispara (decodificaciones de 200 KB desde disco, repintados del panel) — es una perilla de costo de dibujo, exactamente el dominio del ajuste de Gráficos ("todo lo que se DIBUJA de más", `aura_settings.h:52-53`); Animaciones solo decide si esa aparición se funde o se corta.* Un lector que lo busque en Animaciones encontrará la referencia cruzada en ambas secciones de niveles del design system.

Implementación: nuevo token `right_panel_debounce_short_ms = 500` en `design-system/tokens.json` → `AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_SHORT_MS` en `apple2026_tokens.h`; `aura_screens.c:1293-1296` elige el par (2000/1000) o (500/500) según `graphics_mode == AURA_GFX_ALL` — vía el helper central de §7.

---

## 4. FASE 0.4 — CoverDrift "Ninguno": el degradado de acento (D-c)

Verificado: **sí hay pieza reutilizable, pero no es la del tile.** En `aura_selection_summary.c` conviven dos degradados:

- `draw_diagonal_gradient()` (`:150-171`) — el diagonal de 3 puntos del **tile 90×90**. Descartado: su parámetro es un `size` cuadrado y su propio comentario (`:521-529`) advierte que a 160×240 el diagonal de 3 puntos **deja bandas visibles en RGB565**.
- `draw_accent_gradient_background()` (`:530-547`) — degradado **vertical** de 3 puntos `aura_accent_dark()` → `aura_accent()` → `aura_accent_light()`, ya validado exactamente a 160×240 (es el fallback documentado del fondo del panel cuando el acento no tiene BMP de preset).

**Resolución: reutilizar `draw_accent_gradient_background()`.** Responde al acento vigente en tiempo real (derivados ±25 % por `aura_accent_light()/dark()`, `apple2026_tokens.h:121-122`), cero RGB hardcodeado, cero pieza nueva. Único cambio estructural: hoy es `static`; se exporta en `aura_selection_summary.h` (o se muda a `apple2026_shell.c` junto a las primitivas compartidas — preferencia: **exportarla**, mudanza solo si un tercer consumidor aparece). "Ligero degradado" queda satisfecho por la spec ya ratificada del fallback (D-271/D-292).

---

## 5. FASE 0.5 — Las "5 imágenes aleatorias" de CoverDrift Mínimos (D-d)

Estado actual (`aura_screens.c:806-833`, `aura_coverdrift.c:77-84`): pool de hasta 300 seeks de álbum (solo índices, 1.2 KB), recorrido **secuencial alfabético** infinito; en RAM viven **2 imágenes** (activa + anterior, 2×200 KB); cada avance de ciclo (7 s) decodifica desde disco.

**Resolución propuesta (la más simple que hace verdadero el beneficio):**

- **Cuándo se eligen las 5**: al primer montaje de CoverDrift tras cada arranque, con `rand()` sobre el pool vigente (sin persistir en disco). "La selección persiste hasta cambiar la configuración" = se re-sortea (a) al cambiar el nivel de Gráficos, (b) al reiniciar el dispositivo, (c) cuando cambia la generación del pool (biblioteca resincronizada — el mecanismo de invalidación ya existe, `aura_screens.c:820-825`).
- **Menos de 5 disponibles**: el umbral de montaje sigue siendo **3** (P1 — el "10" del encargo ya no existe; D-254 lo bajó a 3 y esta matriz no lo toca). Con 3 o 4 imágenes en el pool, el modo usa todas: `cap = MIN(5, pool)`. Ninguna regla nueva.
- **Qué compra el modo** (corrige P3): las 5 imágenes se decodifican **una vez y quedan residentes** (~1.0 MB, §9). A partir de ahí CoverDrift corre **sin tocar el disco**: hoy el HDD del iPod despierta cada 7 s para leer ~400 KB (ineficiencia de doble decodificación incluida, §14). El claim honesto del ajuste no es "limitar RAM" sino **"limitar lecturas de disco (batería) acotando la memoria usada"** — la RAM acotada es el medio, el disco dormido es el beneficio. Ver Q3.

---

## 6. FASE 0.6 — El morph del Modo 4 está lento en hardware

Trabajo aparte de los niveles: el efecto se **optimiza conservando su definición** (~330 ms, fundido lineal, `now-playing.md`). Degradarlo violaría máxima fidelidad.

### 6.1 — Perfil estático (leído del código; el perfil en hardware lo confirma en FASE 2)

El morph (`mode4_morph()`, `aura_nowplaying.c:1646-1782`) es el **único** efecto del árbol que reconstruye la pantalla completa desde cero en cada cuadro: `a26_shell_clear_screen()` + `draw_player()` completo + `lcd_update()` de 320×240 con espera de DMA, ×19 cuadros (ALL) en un presupuesto de 330 ms. La transición hermana (Flip-and-Flow) ya usa la técnica correcta: destino prerrenderizado una vez a `s_push_fb` y composición por regiones (`aura_transitions.c:1256-1310`).

Sospechosos, en orden de gravedad estimada:

| # | Cuello de botella | Evidencia | Costo por morph |
|---|---|---|---|
| 1 | **Íconos leídos de DISCO en cada cuadro** — no existe caché de íconos en RAM (`aura_widgets.c:161-200` → `aura_style_read_icon_bmp()` → `read_bmp_file()`, `aura_style.c:428-444`) | ~13-15 BMP por cuadro (batería, play/pausa, 5 estrellas, 5 modos, 3 transporte) | **~250-285 aperturas+decodificaciones** sobre el HDD en 330 ms. Por sí solo explica la lentitud |
| 2 | Hoja de vidrio 190×240: `m4_grad_diag()` con **división entera por píxel** (ARM sin divisor HW) + 2 blends por píxel (`aura_nowplaying.c:660-666, 701-712`) | ~45 600 divisiones + ~91 000 blends por cuadro | ×19 |
| 3 | Carátula reproyectada por cuadro: el caché de perspectiva **nunca acierta durante el morph** (`:267-271`) + ~160 `lcd_bitmap()` de 1 px de ancho por cuadro (`:896`) | setup de viewport/clipping ×160 | ×19 |
| 4 | Sombra SDF del álbum recomputada por cuadro (`draw_art_soft_shadow`, `:737-782`, `isqrt256` por píxel) | ~8-9 K px | ×19 |
| 5 | Texto re-maquetado por cuadro: `wrap_lyric_line()` mide **cada prefijo acumulado** (O(palabras²) de `lcd_getstringsize`) + cabecera + contexto (`:1483-1607`) — invariante durante el morph | — | ×19 |
| 6 | StatusBar recompuesta entera por cuadro y desplazada con `memcpy` fila a fila (`:1688-1705`) | incluye íconos del punto 1 | ×19 |
| 7 | Cadencia nominal imposible: `HZ=100` → `frame_delay = HZ/60 = 1 tick`; el ritmo real lo fija el render. Interpolación **lineal** pura (sin las curvas de `aura_motion.c`) | `:1662-1675` | — |

### 6.2 — Protocolo de perfilado en hardware (FASE 2, antes de optimizar)

La instrumentación ya existe: D-300 dejó `DEBUGF` por fase en `mode4_morph` (`aura_nowplaying.c:1650-1657, 1714-1716, 1777-1779`) y existe `TRANSITION_LOG` (`aura_transitions.c:48-57`), creada justamente "para decidir con datos reales si un modo gráfico necesita degradar en hardware real". Protocolo: build DEBUG en el iPod, medir ms/cuadro del morph tal cual; re-medir tras cada optimización aplicada. **El simulador no cuenta** (no hay `#ifdef` de render — el mismo camino corre en ambos, pero el costo de disco/DMA solo existe en hardware).

### 6.3 — Plan de optimización (conserva el efecto; orden = impacto esperado)

1. **Caché de íconos en RAM** (mata al sospechoso 1). Precedente en el propio repo: D-224 resolvió "musicflow muy lento" con exactamente este patrón (`aura_music.c:258-290`). Beneficia a todo el sistema, no solo al morph. Diseño: caché pequeño keyed por (nombre, tamaño, variante) sobre el buffer de máscaras, invalidado al cambiar de estilo/tema.
2. **Precalcular invariantes antes del bucle**: layout/wrap del texto de letras (sospechoso 5), colores del panel, `aura_shadows_enabled()` fuera de los bucles de píxel (`:715-717, :1762`).
3. **LUT del degradado del vidrio**: `m4_grad_diag()` depende solo de `(x_rel+y)` ∈ [0, 430] → LUT de 431 entradas por carátula (misma llave de invalidación que `s_panel_colors_valid`). Elimina las 45 600 divisiones/cuadro (sospechoso 2).
4. **StatusBar compuesta una vez** a un buffer de 320×20 y solo desplazada (sospechoso 6; generaliza la técnica `stripe_base` que D-300 ya usó para la franja de sombra).
5. **Prerrender de estados finales / composición por regiones** al estilo `s_push_fb` de Flip-and-Flow, con `lcd_update_rect()` acotado donde el cuadro lo permita (sospechosos 3 y parte del 7).
6. **Sombra SDF**: LUT de rampa por distancia o interpolación de alpha entre dos estados precalculados (sospechoso 4).
7. **Cadencia realista**: fijar cuadros a lo que el hardware entrega (medido en 6.2) y, si el dueño lo aprueba como parte del canon, easing `aura_motion_ease_out` en lugar de lerp puro — mejor percepción con menos cuadros. *Cambio de definición → solo con decisión explícita.*

**Solo si tras 1-6 el morph sigue inviable en hardware** se propone ajustar la definición del efecto — como decisión explícita al dueño, nunca silenciosa.

---

## 7. FASE 1 — Arquitectura de niveles: un punto único de decisión

Nuevo header **`firmware/rockbox/apps/aura/aura_fx.h`** (header-only, `static inline`, cabecera GPL v2): las 6 tablas de la matriz viven ahí como funciones de consulta con nombre, cada una con su comentario-celda. Ningún componente vuelve a comparar enums a mano.

```c
/* Animaciones */
static inline bool aura_fx_anim_off(void);        /* == AURA_ANIM_NONE          */
static inline bool aura_fx_anim_full(void);       /* == AURA_ANIM_ALL           */
static inline int  aura_fx_frames(int all, int m); /* patrón ALL?a:m centralizado */
static inline bool aura_fx_flip_and_flow(void);   /* solo ALL; MINIMAL → push    */
typedef enum { AURA_FX_M4_NONE, AURA_FX_M4_FADE_SLIDE, AURA_FX_M4_MORPH } aura_fx_m4_t;
static inline aura_fx_m4_t aura_fx_mode4(void);
/* Gráficos */
static inline bool aura_fx_coverdrift_images(void);   /* G != NONE               */
static inline int  aura_fx_coverdrift_pool_cap(void); /* MINIMAL→5, resto→0      */
static inline bool aura_fx_ss_icons(void);            /* G != NONE && show_icons */
static inline long aura_fx_panel_debounce_ms(bool to_coverdrift); /* §3          */
/* Cruces (regla de precedencia §2) */
static inline bool aura_fx_coverdrift_motion(void);   /* imágenes && anim ALL    */
static inline bool aura_fx_coverdrift_fade(void);     /* anim != NONE            */
```

- Los 24 sitios existentes de `animation_mode` migran mecánicamente a estos helpers en un commit **sin cambio de comportamiento** (verificable por diff de tokens usados).
- `aura_widgets_split_active()` queda como está (ya es el punto central de Gráficos para el layout); su destino depende de Q1.
- Tokens nuevos en `design-system/tokens.json` (regeneran `apple2026_tokens.h`): `right_panel_debounce_short_ms: 500`, `cover_drift_pool_cap_minimal: 5`. Lateral: `FLOW_MS 500` sigue siendo `#define` local sin token (deuda anotada en `00-vocabulario.md`) — se tokeniza de paso.

---

## 8. FASE 1 — Cambios por componente, celda por celda

### 8.1 — Music Flow

| Celda | Estado | Cambio |
|---|---|---|
| Ninguna: animaciones internas se conservan | **Ya correcto por construcción**: scroll/zoom/flip/marquee no consultan `animation_mode` (`aura_musicflow.c`) | Ninguno. Se documenta como intencional para que nadie lo "arregle" |
| Ninguna: cero transiciones al entrar/salir | **Ya correcto**: gates en `aura_transitions.c:890` (entrada), `:1094` (flip-and-flow hace `aura_nav_push` y retorna), `:1373` (flow-return, con `settle_idle` antes del gate), `:636` (salida genérica) | Ninguno |
| Mínimas: se conserva Music Flow↔lista | Hoy corre a 4 cuadros/45 fps | Ninguno (ya es la versión reducida del canon) |
| Mínimas: **Flip-and-Flow → empuje de pantalla completa, ambos sentidos** | Hoy MINIMAL = mismo vuelo a 30 fps | El empuje **ya existe y es simétrico**: `aura_transition_slide(nav, ±1, A26_SCREEN_WIDTH, false)` (`aura_transitions.c:612`) — Music Flow y NowPlaying son ambas FULL, `bar_changes == false`, push limpio de 4 cuadros. Es el patrón `Push-and-Drop` full↔full del vocabulario, no un patrón nuevo. Tres cambios de "quitar excepciones", condicionados a `!aura_fx_flip_and_flow()`: (1) `aura_musicflow.c:1289-1291` → `aura_nav_push()` directo; (2) `aura_screens.c:5413-5421` rama vacía → cae al push genérico; (3) `aura_screens.c:5397-5412` ramas de `flow_return` → push genérico con `direction = -1`. El regreso desde Modo 4 a Music Flow en Mínimas deja de encadenar dos morphs: sale de Letras según §8.2 y hace el push |
| Todas | Canon (`music-flow.md`, vuelo 500 ms) | Ninguno |

### 8.2 — Reproductor musical (cambio de modos / Modo 4)

| Celda | Estado | Cambio |
|---|---|---|
| Ninguna: cambio instantáneo | **Ya correcto**: gate en `aura_nowplaying.c:1659` | Ninguno |
| Mínimas: **lo viejo se desvanece; lo nuevo entra desde fuera** | Hoy MINIMAL = mismo morph a 9 cuadros/30 fps | Nueva rama `AURA_FX_M4_FADE_SLIDE` en el disparo (`aura_nowplaying.c:2016-2021`): fade-out del layout saliente (fundido lineal ~330 ms de contenido — cadencia canónica ya definida) y entrada del layout nuevo con las piezas **ya existentes** del vocabulario: hoja de letras deslizando desde la derecha (ya es su entrada canónica, `lyrics-panel.md`), íconos desde la derecha y barra desde abajo (mismo stagger de `aura_transitions.c:1273-1310`), StatusBar con su Lift/Drop de siempre. Cero curvas nuevas, cero patrones nuevos. Sentido inverso espejado al salir |
| Todas: morph canónico | `now-playing.md` (~330 ms, proyección por columnas) | Ninguno en definición; **optimización §6** |

### 8.3 — Transiciones generales

| Celda | Estado | Cambio |
|---|---|---|
| Ninguna: todo instantáneo | **Ya correcto**: los 6 gates de `aura_transitions.c` + `mode4_morph` | Ninguno; la migración a `aura_fx_anim_off()` no cambia comportamiento |
| Mínimas: solo transiciones simples; sin morphing ni fades complejos | Hoy MINIMAL conserva TODO en versión reducida, morphs incluidos | Los dos morphs se sustituyen (§8.1, §8.2). Interpretación registrada: `Push-and-Drop`/`Lift-and-Push`/`Shift-and-Reveal`/`Fade-Slide`/`Scroll-Slide`/`Drop-and-Lift`/`Push-and-Pull`/`Fade-on-Idle` son "simples" y se conservan en modo reducido (4/3 cuadros, 45 Hz — ya implementado); "morphing y fades complejos" = `Flip-and-Flow`, `Flow-Return` y el morph del Modo 4 |
| Todas | Canon completo, 11 patrones | Ninguno |

### 8.4 — CoverDrift (Gráficos)

| Celda | Cambio |
|---|---|
| Ninguno: acento con degradado | Sujeto a Q1. En `draw_panel_identity()` (`aura_screens.c:1081-1108`), la rama CoverDrift con `!aura_fx_coverdrift_images()` pinta `draw_accent_gradient_background()` exportada (§4) en vez de decodificar/dibujar imágenes. La mecánica de identidad/debounce/fade queda intacta (Q4); con Animaciones=Ninguna el fade se corta por su gate. Sin imágenes montadas no se toca el disco ni los buffers de 200 KB |
| Mínimos: 5 aleatorias residentes, delay 2000 ms | §5: sorteo al primer montaje por arranque; `aura_fx_coverdrift_pool_cap()` limita el pool; decodificación única a buffers residentes (§9); re-sorteo al cambiar nivel/reiniciar/regenerarse el pool |
| Todos: todas las imágenes, delay 500 ms | `aura_fx_panel_debounce_ms()` devuelve el par corto (§3). Loop completo = comportamiento actual |
| (Animaciones × imágenes) | `aura_fx_coverdrift_motion()`: con Animaciones ≠ Todas, `aura_coverdrift_draw()` pinta la imagen **centrada y quieta** (distancia 0 — el ciclo de 7 s sigue avanzando contenido); `aura_fx_coverdrift_fade()`: con Animaciones = Ninguna, el cross-fade de `:316-324` se salta y el cambio es corte |

**Cambio en caliente sin estados huérfanos** (requisito de FASE 2): bajar Gráficos con CoverDrift montado no puede dejar una imagen congelada — al volver de la pantalla de ajustes, `panel_identity` se recalcula y la rama degradada repinta; se verifica explícitamente en las 9 combinaciones.

### 8.5 — SelectionSummary (Gráficos)

| Celda | Cambio |
|---|---|
| Ninguno: solo texto | Sujeto a Q1. `draw_summary()` (`aura_selection_summary.c:549-725`) con `!aura_fx_ss_icons()`: se salta tile completo (degradado diagonal + sombra SDF + símbolo + recorte de esquinas + `memcpy` de 16 KB) — es la parte cara del componente. Layout de texto: Q5. El contrato "`icon_name` nunca es NULL" (`aura_selection_summary.h:87-91`) no se toca: la omisión es del dibujo, parametrizada como ya se hizo con `aura_ss_background_t` |
| Mínimos: canon, delay 1000 ms | Ninguno (es el estado actual; P2/Q2) |
| Todos: canon, delay 500 ms | `aura_fx_panel_debounce_ms()` (§3) |

---

## 9. Presupuesto de RAM del modo 5 imágenes (para que el claim sea verdad)

| Configuración | Buffers de imagen residentes | Δ vs hoy | Disco durante CoverDrift |
|---|---|---|---|
| Hoy (loop completo, 2 residentes) | 2×200 KB = 400 KB (+200 KB `cover_buf` + 50 KB `refl_buf` de paso) | — | Despierta cada 7 s: 2 lecturas de ~200 KB (doble decodificación, §14) + 2 reflejos descartados |
| Mínimos (5 residentes) | 5×200 KB = 1000 KB | **+~600 KB** (~0.9 % de los 64 MB) | **Cero** tras la carga inicial |
| "Cargar todo el loop" (lo que el claim original imagina) | 300×200 KB ≈ 61 MB | inviable | — |

Conclusión honesta: el modo **no ahorra RAM** respecto a hoy — gasta ~600 KB más a cambio de que el HDD no despierte nunca durante CoverDrift (batería y silencio, que en un iPod Classic con disco mecánico es la ganancia real). El texto del registro de decisión y del design system dirá exactamente eso (Q3). Los scratches de decodificación compartidos (800 KB, `aura_albumart.c:70-73`) no cambian en ningún nivel.

---

## 10. Actualización del design system

El canon sigue siendo "Todas"; los niveles se documentan como **sustracciones**, conforme al principio de máxima fidelidad (`00-INDICE.md` §"Principio de documentación").

- Sección **"Niveles de reducción"** (tabla normativa de la matriz correspondiente) en: `componentes/cover-drift.md`, `componentes/selection-summary.md`, `componentes/music-flow.md`, `componentes/now-playing.md`, y la tabla de "Transiciones generales" en `transiciones/00-vocabulario.md` (que ya tiene la tabla completo/reducido de cuadros — se completa con la columna "Ninguna" y la sustitución de los morphs en Mínimas).
- Página nueva **`sistema/06-niveles-de-fx.md`**: el hueco es real (hoy los niveles solo existen dispersos); contiene la regla de precedencia, las 6 tablas completas y referencias a las secciones por componente. Las páginas de componente mandan sobre el detalle; esta es el índice transversal.
- `sistema/03-arbol-de-menus.md`: las filas Animaciones/Gráficos enlazan a la página nueva.
- De paso: corregir el comentario obsoleto `aura_musicflow.h:23-27` y la referencia colgada a `PLAN-theme-system.md` en `selection-summary.md`/`now-playing.md`.

---

## 11. Decisiones a registrar en `DECISIONS.md` (numeración final al ejecutar; siguiente libre: D-310)

| Ref | Decisión |
|---|---|
| D-a | Regla de precedencia: Gráficos decide qué existe; Animaciones cómo se mueve lo que existe (§2). Corolarios: rotación de 7 s y marquee son contenido, no animación |
| D-b | Los delays de aparición/actualización del panel viven en **Gráficos** (§3), con su porqué |
| D-c | El fondo de CoverDrift en Gráficos=Ninguno reutiliza el degradado vertical de acento del fallback del panel (§4) |
| D-d | Semántica de las 5 imágenes: sorteo por arranque, residentes, re-sorteo al cambiar configuración; claim del ajuste = disco/batería, no RAM (§5, §9) |
| D-e | Sustituciones de Mínimas: Flip-and-Flow/Flow-Return → `Push-and-Drop` full↔full; morph del Modo 4 → fade + entrada desde fuera con piezas del vocabulario (§8) |
| D-f | Lo que Q1 resuelva sobre Gráficos=Ninguno y el layout split |
| D-g | Resultado del perfilado/optimización del Modo 4; si hiciera falta tocar la definición del efecto, decisión aparte y explícita |

---

## 12. Preguntas abiertas (con recomendación)

| # | Pregunta | Recomendación |
|---|---|---|
| **Q1** | La matriz define contenido del panel derecho con **Gráficos=Ninguno** (degradado de acento; solo texto), pero la regla dura vigente (`status-bar.md`, `aura_widgets_split_active()`) dice que con Ninguno **no hay LeftPanel ni panel derecho: todo pasa a (full)**. ¿Cuál manda? | **Que mande la matriz: es la especificación normativa del dueño y sus celdas carecen de sentido si el panel no existe.** Gráficos=Ninguno deja de colapsar a (full); el split se conserva con contenido degradado (panel de acento / solo texto), que además sigue siendo más barato que hoy en Mínimos (cero imágenes, cero íconos, cero disco). Se actualizan `status-bar.md` y `aura_widgets_split_active()` (que pasaría a depender solo del layout). *Alternativa si el dueño prefiere conservar el colapso a (full):* las dos celdas de la fila "Ninguno" se marcan "no aplica — el panel no existe" y el cambio de código en §8.4/§8.5 fila Ninguno desaparece. Es la decisión de mayor alcance del plan; nada de la fila "Ninguno" se implementa sin resolverla |
| **Q2** | El encargo dice que el delay "actual" de SelectionSummary es 2 s, pero desde D-266 es **1000 ms**. ¿"Mínimas" conserva 1000 ms o fuerza 2000? | **Conservar 1000 ms.** La intención de la celda es "comportamiento actual", y el actual-actual es la cadencia asimétrica de D-266 (2000 solo hacia CoverDrift). "Todas" baja ambos a 500 |
| **Q3** | Las 5 imágenes: ¿residentes (+600 KB, disco dormido) o solo tope de pool con la arquitectura actual de 2 residentes (Δ RAM 0, pero también Δ beneficio 0: mismas lecturas de disco cada 7 s, solo menos variedad)? | **Residentes.** Es la única implementación con la que el ajuste compra algo real (§9); +600 KB es el 0.9 % de la RAM. El texto del ajuste/registro se reformula al claim verdadero (disco/batería) |
| **Q4** | Con Gráficos=Ninguno, ¿el panel de acento conserva el debounce de 2 s y el fundido de 600 ms de la ruta CoverDrift? | **Sí.** Se cambia solo el contenido dibujado, no la mecánica de identidad — un solo código con puntos de sustracción, exactamente el principio rector. (Con Animaciones=Ninguna el fundido cae por su propio gate.) |
| **Q5** | SelectionSummary solo-texto: sin tile, ¿los dos slots de texto se quedan en sus posiciones actuales (calculadas alrededor del tile) o se recentran verticalmente como grupo? | **Recentrar como grupo** (superior Bold 18 + inferior Medium 16, mismo interletrado que hoy, centrados en los 240 px): sin tile, las posiciones actuales dejan un hueco muerto de 90 px en el centro exacto del panel. Es un cálculo de centrado, no geometría nueva. Verificación visual en hardware en FASE 2 |
| **Q6** | El debounce legado de 1 s de las listas de contenido (`PANEL_RETARDO_TICKS`, `aura_widgets.c:66`) está fuera de la matriz. ¿"Gráficos=Todos" también lo baja a 500 ms? | **Sí, por coherencia** — mismo trato al mismo concepto (delay de actualización del panel derecho), vía el mismo helper. Si el dueño prefiere no tocarlo, se excluye sin costo |

---

## 13. FASE 2 — Ejecución (Sonnet, solo tras aprobación explícita)

Commits atómicos, en este orden (cada uno: build ARM + simulador SDL limpios, `make -C firmware/rockbox/apps/aura/test test` sin regresiones nuevas):

1. `aura_fx.h` + migración mecánica de los 24 sitios de `animation_mode` y los 2 de `graphics_mode` — **cero cambio de comportamiento**.
2. Tokens nuevos en `tokens.json` → regenerar `apple2026_tokens.h` (+ tokenizar `FLOW_MS` de paso).
3. Delays de Gráficos=Todos (500 ms) — CoverDrift, SelectionSummary (+ Q6 si se aprueba).
4. CoverDrift × Animaciones: quieto en ≠Todas, corte en Ninguna.
5. CoverDrift Gráficos=Mínimos: pool de 5 residentes (incluye arreglar de paso la doble decodificación, §14.1, prerrequisito natural).
6. Fila Gráficos=Ninguno según Q1: degradado exportado + SelectionSummary solo-texto.
7. Mínimas: Flip-and-Flow/Flow-Return → push genérico (3 sitios, §8.1).
8. Mínimas: Modo 4 → fade + entrada desde fuera (§8.2).
9. Perfilado D-300 en hardware (§6.2) → commits de optimización del morph en el orden §6.3, midiendo tras cada uno.
10. Design system (§10) + `DECISIONS.md` (§11).

Verificación obligatoria **en hardware** (el simulador no es evidencia de fluidez): los dos delays de 500 ms, el push sustituto Music Flow↔Reproductor en ambos sentidos, el fade+slide del Modo 4 en Mínimas, y el morph optimizado en Todas. Prueba de las **9 combinaciones** de niveles contra las matrices de §2 en CoverDrift y SelectionSummary, incluido el **cambio en caliente** (sin reinicio, sin estados huérfanos: imagen congelada al bajar Gráficos, panel sin repintar al subirlo). Ajustes existentes del usuario sobreviven: no cambia el formato de `aura.cfg` (mismas claves/enums; la migración vieja de `:328-329` sigue funcionando).

Reglas de siempre: `lcd_active()` en toda animación; sin RGB hardcodeado (degradado por `aura_accent*()`/tokens); textos de UI en es-MX añadidos al final de ambos `.lang`; GPL v2 en archivos nuevos de `apps/aura/`; sin cambios fuera de `apps/aura/` (si alguno apareciera → `MODIFICATIONS.md` en la misma pasada). Sin push.

---

## 14. Hallazgos laterales (fuera del alcance de la matriz; se reportan, no se ejecutan sin acuerdo)

1. **Doble decodificación por ciclo de CoverDrift** (`aura_screens.c:899-931`): cada avance decodifica 2 imágenes (2×200 KB del disco + 2 reflejos) cuando bastaría 1 intercambiando los roles de los buffers A/B. Propuesto arreglarlo dentro del commit 5 (le da sentido al modo residente); beneficia a todos los niveles.
2. **El reflejo de CoverDrift se genera y se tira** en cada carga (`refl_buf` 50 KB + cómputo, `aura_albumart.c:318-321, 339-341`): CoverDrift nunca lo usa. Candidato a eliminarse de esa ruta.
3. **`aura_coverdrift_animating()` nunca vuelve a `false`** tras el primer montaje (`aura_coverdrift.c:96-103`; `s_index` no se resetea) → `aura_main.c:591-592` pide cuadros a 20 fps permanentemente con la pantalla encendida, aunque CoverDrift ya no esté visible. CPU/batería desperdiciadas; arreglo pequeño (resetear al desmontar identidad).
4. **Comentario obsoleto** `aura_musicflow.h:23-27` (acoplamiento con `AURA_GFX_ALL` revertido en D-025) — se corrige en §10.
5. **La sombra del tile de SelectionSummary ignora el toggle "Mostrar sombras"** (única del sistema, ya anotado en `efectos/01-sombras.md`) — ajuste de una línea si se quiere cerrar de paso.
6. La **caché de íconos** del §6.3-1 acelera todo el shell (listas, barra de estado), no solo el morph — vale la pena aunque el perfilado señalara otro cuello primero.
