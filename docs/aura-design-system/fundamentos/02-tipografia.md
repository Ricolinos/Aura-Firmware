# Tipografía

🟢 Todos los roles con consumidor real están definidos (11 roles en
`type_scale_roles`, 9 estilos `ds_*`; resync 2026-08-16). Huecos: las
pantallas sin token propio (`--font-body`/`--font-caption` de la tabla
final) siguen usando estilos compartidos del sistema Apple2026 viejo.

**D-286 (2026-08-16, separación de repositorios) / D-289 (2026-08-17,
sistema de temas implementado):** las tablas de abajo describen cada
token por **rol** (tamaño en punto + peso), no por la cara tipográfica
concreta que lo resuelve — eso ahora depende del **estilo activo**
(`sistema/05-temas.md`, formato en `CONTRATO-formato-tema.md`; los 14
roles de esta página son EXACTAMENTE los 14 `fonts/<rol>.fnt` de un
paquete de tema). El tema COMPILADO por defecto de este
repositorio resuelve los 14 roles con **Inter** (SIL OFL, vendorizada en
`design-system/vendor/inter-ttf/`) en vez de SF Pro/SF Compact (Apple, no
redistribuible — ver `AUDIT-pre-split.md` 0.1). El tema opcional "Apple
(uso personal)", construido localmente en Aura Studio a partir de las
fuentes ya instaladas en la Mac del usuario (nunca redistribuidas), sigue
resolviendo estos mismos roles con SF Pro/SF Compact -- por eso las tablas
de abajo, escritas cuando SF Pro era el único tema, se dejan tal cual en
su columna "Familia": son la referencia histórica de CÓMO se midieron
estos tamaños (contra mockups reales, D-207/D-271), válida para cualquier
cara que los resuelva. Inter tiene otra altura de x que SF -- los tamaños
en punto se conservaron idénticos como simplificación pragmática; un
reajuste fino medido en píxel contra Inter queda como mejora futura, no
bloqueante (el resultado ya se verificó visualmente, sin texto cortado ni
glifos rotos, `docs/screenshots/theme-default-inter-lucide/`).

## Tokens definidos

**Actualizado 2026-08-14, dos pasadas el mismo día** (D-205: tipografía de
listas y barra de estado más grande y legible, mismo alcance de D-195 pero
sin tocar Music Flow/NowPlaying → `StatusBar` 8px a 10px. D-207: "el texto
casi no se nota, 2px más grande" → `StatusBar` 10px a 12px): reutiliza
estilos `ds_*` ya cargados en cada pasada (D-205: `DS_REG_10`/`DS_BOLD_10`,
compartidos con `np_counter`/alarmas; D-207: `DS_REG_12`/`DS_BOLD_12`,
compartidos con `np_album`/`np_title` sin alterar su apariencia ahí), cero
fuentes nuevas en ninguna de las dos.

| Token | Tamaño | Peso | Familia | Opacidad | Uso |
|---|---|---|---|---|---|
| `--font-statusbar-title` | 12px (D-207, subido de 10px, antes 8px) | Bold | SF Pro | 60% | `DynamicTitle` en `StatusBar` |
| `--font-statusbar-time` | 12px (D-207, subido de 10px, antes 8px) | Regular | SF Pro | 80% | `ClockIndicator` en `StatusBar` |

**Nota:** originalmente esto vivía como un solo token (`--font-statusbar`)
con el título y la hora compartiendo estilo — se separó porque tienen peso
y opacidad distintos. También se corrigió la familia de "SF Display" a
"SF Pro" según tu aclaración más reciente — avísame si esto último fue un
error mío al documentarlo la primera vez, no una corrección tuya
intencional.

### Dos notas prácticas antes de dar esto por cerrado

1. **Licencia de SF Display:** la fuente San Francisco de Apple está
   licenciada para apps del ecosistema Apple, no pensada para redistribuirse
   embebida en firmware de terceros. No bloquea el uso personal, pero si
   Aura se comparte/publica vale la pena resolver esto — o generar una
   fuente propia inspirada en su estilo, no la fuente original.
2. **Rendering a 12px / 163ppi:** Rockbox tradicionalmente usa fuentes
   *bitmap* (`.fnt`) pixel-hinted por tamaño, no vectoriales escaladas en
   tiempo real — se ven más nítidas y son más baratas en CPU sin GPU. Si el
   punto 1 nos lleva a una fuente propia de todas formas, esto se resuelve
   solo: se genera directamente como bitmap a 12px.

## Tokens de NowPlaying (confirmados 2026-08)

Familia SF Pro en todos:

| Token | Tamaño | Peso | Uso |
|---|---|---|---|
| `--font-np-title` | 12px | Bold | Título de canción en NowPlaying |
| `--font-np-album` | 12px | Regular | Álbum |
| `--font-np-artist` | 12px | Regular | Artista |
| `--font-np-counter` | 10px | Bold | Tiempos de progreso y contador "n of m" |
| `--font-lyrics` | 12px | Regular | Letras (Modo 4) |
| `--font-lyrics-active` | 14px | Bold | Línea activa de letras (Modo 4) |

## Tokens de LeftPanel (confirmados)

**D-207 (2026-08-14):** `menu_item` subió de Regular a **Semibold** además de
+2px -- único estilo Semibold del sistema (`SF-Pro-Text-Semibold.otf`),
ocupando en su momento el hueco de `MAXUSERFONTS` que quedaba libre
(`A26_FONT_STYLE_DS_BOLD_8`, sin consumidor real, confirmado por grep).
**D-267 (2026-08-15):** +1pt más, ahora `A26_FONT_STYLE_DS_SEMIBOLD_15`
(renombrado desde `DS_SEMIBOLD_14`, mismo único consumidor). Estilo
EXCLUSIVO de `MenuList` -- no comparte slot con `np_album`/`np_artist`/
`lyrics` (que se quedan en `DS_REG_12`), así que NowPlaying no cambia con
esto (el reproductor no se toca por encargo explícito del dueño).

| Token | Tamaño | Peso | Uso |
|---|---|---|---|
| `--font-menu-item` | 15px (D-267 +1pt; antes 14px D-207, 12px D-195, 10px original) | **Semibold** (D-207, antes Regular) | Texto de ítems en `MenuList` (SF Pro), incluye Ajustes y todos los submenús |

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

## Listas de contenido completo (Música/Video/Fotos/Alarmas, `aura_widgets_draw_list`)

**Nuevo 2026-08-14.** Sistema Apple2026 viejo (no `aura_ds`), sin doc propia
todavía -- `A26_TYPE_BODY` (13px Regular, compartido con Music Flow/NowPlaying/
Fotos/Video) era el tamaño Regular más grande posible sin agregar un `.fnt`
nuevo (ver `design-system/tokens.json`, `comment_ds`; el presupuesto de
fuentes exacto está arriba). Para subir la legibilidad sin fuente nueva ni
tocar lo compartido, el texto de cada fila pasó a reutilizar `DS_BOLD_14`
(14px Bold, el mismo estilo `ds_*` ya cargado para `--font-lyrics-active`,
D-205) -- 8% más grande y en negrita, sin agregar ningún `.fnt`.

| Token | Tamaño | Peso | Uso |
|---|---|---|---|
| `--font-content-list-row` | 14px (subido de 13px, reusa `ds_bold_14`) | Bold | Texto de fila en listas de pantalla completa |
| `--font-index-rail` | **7px** (`type_scale.micro`, `a26-micro-7.fnt` — SF Pro Regular, glifo de 8px de alto) | Regular | Letras del `IndexRail` (`componentes/index-rail.md`, D-276). Es la fuente que la base histórica reservó desde el principio para el riel ("7 px SF Pro") y **ya estaba cargada** en la escala Apple2026: cerrar "todas las letras visibles" (27 × 8 = 216px exactos) **no costó ningún slot** — `MAXUSERFONTS` sigue en 14/14. Antes el riel usaba `ds_reg_8` (glifo 9px), con el que 27 letras no caben |

## Estructura sugerida (resto, a llenar)

| Token | Tamaño | Uso |
|---|---|---|
| `--font-body` | — | Otros contextos de lista aún no definidos |
| `--font-caption` | — | Metadata secundaria en otras pantallas |
