# SelectionSummary

Nombre provisional — corrígeme si tienes uno ya definido en tu investigación.

Es lo que se renderiza en `--layer-base` cuando **no hay contenido más rico
disponible** para mostrar del lado derecho (ej. sin carátulas de álbum
cargadas, sin fotos). Es el estado base/vacío del lado derecho: un ícono +
un texto que describen la selección actual de `LeftPanel`.

## Capa

`--layer-base`. Siempre presente cuando no hay nada montado en
`--layer-content` encima.

## Diseño

**Ícono sobre tile (confirmado 2026-08, según mockups):** el ícono no se
renderiza "suelto" — va sobre un **tile de 90×90px con esquinas muy
redondeadas, 28px de radio (~31% del lado)**, con el **símbolo interno de
~60×60px** (o ligeramente más grande). Estilo app icon de iOS: símbolo
claro sobre tile de color pleno.

**Posición (D-270, D-272):** el tile va al **centro exacto del panel**
(horizontal y vertical), independiente de si hay texto arriba, abajo, en
ambos slots o en ninguno — su posición es una constante, no depende del
contenido. El texto vive aparte, **no forma un grupo con el ícono**: el
texto superior se centra en el margen completo entre el borde superior
del panel y el borde superior del tile; el texto inferior (una o dos
líneas, como bloque) se centra en el margen completo entre el borde
inferior del tile y el borde inferior del panel. Antes de D-270 se
centraba el conjunto ícono+texto como una sola unidad, lo que movía el
tile según cuánto texto hubiera — corregido a pedido explícito del dueño.

**Por qué 28px y no el 22.37% literal de Apple (D-211 → corrección
2026-08-14):** el radio de esquina de este tile pasó por dos ajustes el
mismo día. D-211 lo subió de 8px (10%, el radio genérico de tarjeta) a
20px, igualando el 22.37% real que usan los íconos de iOS — pero
`a26_shell_round_bitmap_corners()` (`apple2026_shell.c`, función
`stamp_corner()`) dibuja un **cuarto de círculo simple** contra la máscara
del bitmap, no la curva de continuidad G2 real del squircle de Apple. Un
arco circular al mismo porcentaje que un squircle se **percibe menos
redondeado**: la curvatura del squircle es continua a lo largo de todo el
borde, mientras que un arco circular solo curva la región inmediata de la
esquina — igualar el porcentaje literal subestima el efecto visual en un
renderer circular. Confirmado con capturas reales del simulador
comparando 20/24/28/32px: 24px casi no se distingue de 20px; 32px empieza
a leerse como un blob/círculo (el tramo recto entre esquinas se vuelve
demasiado corto para leerse como "cuadrado"); **28px (~31% del tile)** es
el punto donde se lee como "claramente muy redondeado, estilo iOS" sin
perder la silueta de cuadrado. **Regla para el futuro:** en este
renderer (esquinas circulares, no squircle), el radio visualmente
equivalente a un squircle está por encima del porcentaje literal de
Apple — no "corregir" este valor de vuelta a ~22% asumiendo que ese es el
número correcto solo porque coincide con la cifra que usa iOS.

*(Nota: la medida original de 40×40px quedó obsoleta con este rediseño.)*

**Color del tile (confirmado 2026-08-14 — ya no es un pendiente):** ya
no es siempre `--color-accent`. Sigue la jerarquía de color por
categoría del Menú principal, en cascada a toda pantalla descendiente
sin importar la profundidad de navegación — Música usa `--color-accent`
(sin cambios), Ajustes/Video/Fotos tienen su propio color FIJO, y
Extras es un degradado de dos tonos (amarillo → acento). Mecanismo
completo, resolución de color y el mapeo pantalla→categoría:
`sistema/04-color-por-categoria.md` y `fundamentos/01-color.md`. El
glifo/símbolo sobre el tile sigue siendo blanco constante (variante
`-selector`) — no cambia, es lo que mantiene el símbolo legible sobre
cualquier color de tile.

## Fondo del panel derecho (D-267)

El fondo de `SelectionSummary` **no es un degradado calculado ni un color
plano** — es una **imagen de panel completo** (160×240px exactos, opaca)
horneada por el design system, una por **preset de acento**:

- Fuente versionada: `design-system/assets/panel-backgrounds/<preset>-source.png`
- Salida: `out/icons/aura/backgrounds/<preset>.bmp` (recorte + reescalado
  al tamaño exacto del panel derecho, hecho por `generate.py`)
- Token: `aura_ds.metrics.right_panel_background.presets`

**Sigue el ACENTO, no la categoría.** El tile encima sí toma el color de
la categoría (ver arriba), pero el fondo del panel es siempre el del
acento configurado por el usuario — confirmado por el mockup del dueño:
la fila Fotos muestra un tile naranja (color de categoría) sobre un fondo
rosa (color de acento).

**Estado interino:** solo existe el preset `pink`. Cualquier otro acento
cae a `pink` mientras el dueño comparte las imágenes de los otros cinco
presets. Si el archivo no está en el dispositivo (assets sin sincronizar
todavía), el fondo cae a `--color-bg-base` sólido en vez de dejar basura
en pantalla.

## Degradado del tile (D-097, D-236)

El **tile del ícono** (no el panel) sí lleva un **degradado en diagonal**
de tres puntos: un tono claro → el color pleno → un tono oscuro, todos
derivados del color de categoría vigente (`aura_category_gradient()`):

- Aclarado/oscurecido: **25% hacia blanco / 25% hacia negro** del color
  base (D-086, G9 — `accent_derived_lighten_pct`/`darken_pct` en
  `tokens.json`), calculado en tiempo real, nunca fijo.
- Dirección: claro arriba-izquierda → oscuro abajo-derecha (convención
  "luz desde arriba", D-097).

🔴 Pendiente: ratificar la dirección de la diagonal (arriba-izquierda
claro → abajo-derecha oscuro es la convención provisional de D-097, no
una decisión de diseño explícita).

## Tipografía y color del texto (D-263, D-267, D-271)

| Slot | Fuente | Líneas | Color |
|---|---|---|---|
| Texto superior | SF Pro **Bold 18pt** (`--font-selection-summary-top`) | 1, nunca se envuelve | Blanco constante |
| Texto inferior | SF Pro **Medium 16pt** (`--font-selection-summary-bottom`) | Hasta 2, cortadas por palabra (nunca a mitad de palabra) | Blanco constante |

Los tamaños se midieron en píxeles contra un mockup pixel-exacto del
dueño (D-271) — el 13/12pt provisional de D-267 resultó demasiado chico
frente al objetivo real. Ambos slots son **blanco fijo** en los dos temas
(D-267): el fondo del panel es siempre una imagen saturada, así que
`--color-text-primary` (que cambia por tema) no aplica aquí. Si el texto
inferior no cabe ni en dos líneas, cada línea usa `MarqueeText` por
separado (ver abajo).

## Sombra del tile (D-267, D-270)

El tile lleva una **sombra proyectada real** hacia abajo:

| Parámetro | Valor | Token |
|---|---|---|
| Desplazamiento vertical | 4px | `selection_summary.shadow_offset_y` |
| Opacidad pico | 35% | `selection_summary.shadow_alpha_pct` |
| Barrido | 12px | `selection_summary.shadow_blur_px` |
| Color | Negro | — |

El barrido es una **caída lineal medida como distancia al rectángulo
redondeado** (campo de distancia con signo, D-270) alrededor de todo el
perímetro del tile — no hay primitiva de blur gaussiano en este LCD, pero
el resultado se lee como un desenfoque suave, mismo principio que la
sombra de `LeftPanel` sobre el contenido. Las esquinas del tile se
recortan **restaurando los píxeles reales del fondo** que había debajo
(no rellenando con un color plano), porque el fondo ya no es un color
conocido sino una imagen (D-267).

**Modelo de texto — dos slots, no un solo texto:**

| Slot | Posición | Qué puede mostrar | ¿Siempre presente? |
|---|---|---|---|
| Texto superior | Arriba del ícono | **Título** *o* **Valor** (nunca ambos a la vez) | No — opcional, puede no mostrarse nada |
| Texto inferior | Abajo del ícono | **Valor** *o* **Descripción** (nunca ambos a la vez) | Sí — casi siempre muestra uno de los dos |

**Regla clave:** "Valor" puede vivir en cualquiera de los dos slots según
el contexto — no es exclusivo de uno. "Título" solo aparece arriba,
"Descripción" solo aparece abajo.

**Ejemplos:**
- Música → sin selección rica: solo texto inferior con la Descripción
  "No hay música" (nada en el slot superior).
- Ajustes → Fecha y Hora → **Hora**: ícono de **reloj analógico** (muestra
  la hora actual con manecillas, dinámico), texto superior = hora en
  formato digital (Valor), texto inferior = el día (Descripción).
- Ajustes → Fecha y Hora → **Fecha**: ícono de **hoja de calendario**
  (muestra mes/día internamente, dinámico — ver
  `componentes/date-editor.md`), texto superior = hora (Valor), texto
  inferior = el día (Descripción).

Son dos casos hermanos dentro del mismo submenú — comparten la estructura
de texto (hora arriba, día abajo) pero cada uno usa un ícono dinámico
distinto y relevante a lo que se está por configurar.

## Comportamiento observado en el firmware original (referencia histórica)

Ejemplo (Menú principal → Música → submenú), documentado durante la
auditoría UX del firmware 2008 — **esto no es la regla que vamos a usar en
Aura**, se deja aquí solo como referencia de lo que hacía el original:

| Selección en `LeftPanel` | Ícono (original 2008) | Texto |
|---|---|---|
| Música | 🎵 | "No hay música" |
| Álbumes (submenú de Música) | 🎵 (mismo ícono) | "No hay álbumes" |
| Audiolibros (submenú de Música) | 📖 (ícono nuevo) | "No hay audiolibros" |

En el original, el ícono a veces se compartía entre selecciones relacionadas
y a veces cambiaba — sin una regla consistente aparente.

## Regla de diseño para Aura (decisión tomada)

**El ícono siempre cambia.** Cada ítem de menú tiene su propio ícono único,
sin excepción — es un mapeo 1:1 entre selección e ícono, no compartido entre
ítems relacionados como en el original. Esto simplifica la regla del
componente (ya no hay ambigüedad de "cuándo cambia") pero implica que hace
falta producir un ícono único por cada ítem de todo el árbol de menús
(Música, Videos, Fotos, Podcasts, Extras, Ajustes y todos sus submenús) —
eso es trabajo de producción de assets, no una pregunta de sistema pendiente.

**Única excepción hoy — Acerca de (D-269):** en `SelectionSummary` (y
solo ahí) la fila "Acerca de" muestra el **ícono real de Aura a color
completo** (badge circular, pipeline `tile_icons` en `tokens.json`,
horneado desde el bundle de Icon Composer del proyecto) en vez del
símbolo blanco `-selector`. La fila correspondiente en `LeftPanel`
conserva su glifo monocromo normal ("info"). Es la única fila que rompe
la regla "símbolo blanco sobre tile de color"; cualquier otra excepción
futura debe documentarse aquí.

## Texto: `MarqueeText`

Cuando el texto no cabe en el espacio disponible, usa el componente
`MarqueeText` (`componentes/marquee-text.md`), que implementa el patrón
`Marquee Loop` — loop continuo de derecha a izquierda, sin transición de
entrada, 2s estático + 5s en movimiento por ciclo. Texto que sí cabe se
muestra estático, sin ningún comportamiento de `MarqueeText`.

## Variante dinámica (resuelto — D-108/B-04, D-264)

**Es un modo del mismo componente, no un componente distinto** (decisión
del dueño, D-108 cerrando B-04). Comparte tile, degradado, sombra y los
dos slots de texto; lo que cambia es que cada slot puede recibir un
*renderer* en vez de un dato estático:

| Slot | Estático | Dinámico |
|---|---|---|
| Ícono | Nombre de ícono (variante `-selector`) | `renderer` que dibuja dentro del tile con datos en vivo |
| Texto inferior | Cadena | `bottom_renderer` gráfico que dibuja en el rectángulo del slot |

Consumidores reales hoy (D-264):

- **Ajustes → Fecha y hora**: reloj analógico con manecillas a la hora
  real (`renderer`), hora digital real arriba (respeta 12/24h y sigue
  corriendo aunque el panel esté congelado por el debounce), etiqueta abajo.
- **Ajustes → Acerca de**: badge de Aura (D-269) como ícono, "Mi iPod"
  arriba, y un gráfico de barras de almacenamiento abajo
  (`bottom_renderer`: Música / Video / Fotos / Otros con porcentaje real
  del disco, colores de categoría existentes).
- **Repetir** (fila en línea, sin pantalla propia): ícono `repeat` /
  `repeat-1` + "Desactivado" / "Repetir todo" / "Repetir una", cambia in
  situ al pulsar SELECT.
- **Aleatorio**: "Activado" / "Desactivado", ligado al switch de la fila.
- **Música / Video / Fotos** (dentro de cada submenú): título de la
  sección arriba + "No hay X" abajo cuando esa fila está vacía.

Los ejemplos de Hora/Fecha como sub-filas (sección "Diseño", arriba)
siguen siendo la intención para cuando exista la variante de hoja de
calendario — ver `componentes/date-editor.md`.

## Comportamiento (D-262, ajustado por D-266 — ya NO es instantáneo)

**El panel izquierdo (`MenuList`/`Selector`) sigue actualizándose al
instante con cada movimiento, como siempre — pero `SelectionSummary`
YA NO.** Hasta D-261 este componente cambiaba de forma instantánea con la
selección; D-262 revierte esa regla a pedido explícito del dueño del
producto ("darle chance al ipod de procesar y renderizar correctamente"):
ahora se congela mientras se recorre `LeftPanel` y solo se actualiza tras
un tiempo de estabilidad sobre la misma fila.

**La espera y la forma del cambio dependen del DESTINO (D-266):**

| Destino del cambio | Espera | Cómo cambia |
|---|---|---|
| Otro `SelectionSummary` (ícono/texto distinto) | **1000ms** | **Corte instantáneo**, sin fundido |
| `CoverDrift` | 2000ms | Fundido real de 600ms |

Es el mismo mecanismo general que gobierna `CoverDrift` (ver
`componentes/cover-drift.md`, sección Activación): un solo debounce para
todo el panel derecho, con dos cadencias según a qué se va a llegar. A
diferencia de `DynamicTitle`, `SelectionSummary` sigue sin animar sus
cambios de VALOR en sí (el texto no hace `Fade-Slide` ni `Scroll-Slide`
interno) — lo que cambió es únicamente CUÁNDO se aplica ese cambio.

## Transición con `CoverDrift`

Cuando el contenido rico (carátulas, fotos) termina de cargar y
`CoverDrift` (`componentes/cover-drift.md`) se monta en `--layer-content`,
la transición entre ambos es un **cross-fade** de 600ms — `SelectionSummary`
se desvanece mientras `CoverDrift` aparece — tras 2000ms de estabilidad
(D-262). **La reversa NO funde (D-266):** cuando la fila deja de calificar
para `CoverDrift`, `SelectionSummary` aparece con un **corte** tras
1000ms, sin cross-fade. Es un caso más del mecanismo general de
debounce del panel derecho, no una transición dedicada aparte.

## Sombra de `LeftPanel`

Debe renderizarse una sombra que simule que `LeftPanel` está por encima de
este componente — spec completo en `efectos/01-sombras.md` (regla
actualizada, compartida con `CoverDrift`).

## Pendiente de definir

- [ ] Producción de un ícono único por cada ítem de todo el árbol de menús
      (regla de sistema ya cerrada — esto es trabajo de asset production)
- [x] Relación exacta con la variante dinámica de Fecha y Hora — resuelto
      en D-108 (B-04): es un modo del mismo componente, ver "Variante
      dinámica"
- [ ] Imágenes de fondo para los cinco presets de acento restantes (hoy
      solo `pink`, D-267)
- [ ] Ratificar la dirección del degradado del tile (D-097, provisional)
