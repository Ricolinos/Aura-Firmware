# Tipografía

🟡 Parcial — primer token definido (`StatusBar`), resto pendiente.

## Tokens definidos

**Actualizado 2026-08-14** (encargo del dueño: tipografía de listas y barra de
estado más grande y legible, mismo alcance de D-195 pero sin tocar
CoverFlow/NowPlaying): `StatusBar` subió de 8px a 10px, mismo peso relativo
cada uno -- reutiliza estilos `ds_*` ya cargados (`DS_REG_10`/`DS_BOLD_10`,
compartidos con `np_counter`/alarmas), sin fuente nueva.

| Token | Tamaño | Peso | Familia | Opacidad | Uso |
|---|---|---|---|---|---|
| `--font-statusbar-title` | 10px (subido de 8px) | Bold | SF Pro | 60% | `DynamicTitle` en `StatusBar` |
| `--font-statusbar-time` | 10px (subido de 8px) | Regular | SF Pro | 80% | `ClockIndicator` en `StatusBar` |

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

| Token | Tamaño | Peso | Uso |
|---|---|---|---|
| `--font-menu-item` | 12px (D-195, subido de 10px) | Regular | Texto de ítems en `MenuList` (SF Pro) |

## Listas de contenido completo (Música/Video/Fotos/Alarmas, `aura_widgets_draw_list`)

**Nuevo 2026-08-14.** Sistema Apple2026 viejo (no `aura_ds`), sin doc propia
todavía -- `A26_TYPE_BODY` (13px Regular, compartido con CoverFlow/NowPlaying/
Fotos/Video) es el tamaño Regular más grande posible sin exceder
`MAXUSERFONTS=12` (ver `design-system/tokens.json`, `comment_ds`). Para subir
la legibilidad sin fuente nueva ni tocar lo compartido, el texto de cada fila
pasó a reutilizar `DS_BOLD_14` (14px Bold, el mismo estilo `ds_*` ya cargado
para `--font-lyrics-active`) -- 8% más grande y en negrita, sin agregar ningún
`.fnt`.

| Token | Tamaño | Peso | Uso |
|---|---|---|---|
| `--font-content-list-row` | 14px (subido de 13px, reusa `ds_bold_14`) | Bold | Texto de fila en listas de pantalla completa |

## Estructura sugerida (resto, a llenar)

| Token | Tamaño | Uso |
|---|---|---|
| `--font-body` | — | Otros contextos de lista aún no definidos |
| `--font-caption` | — | Metadata secundaria en otras pantallas |
