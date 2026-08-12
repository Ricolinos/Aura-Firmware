# Tipografía

🟡 Parcial — primer token definido (`StatusBar`), resto pendiente.

## Tokens definidos

| Token | Tamaño | Peso | Familia | Opacidad | Uso |
|---|---|---|---|---|---|
| `--font-statusbar-title` | 8px (máx. render 12px) | Bold | SF Pro | 60% | `DynamicTitle` en `StatusBar` |
| `--font-statusbar-time` | 8px (máx. render 12px) | Regular | SF Pro | 80% | `ClockIndicator` en `StatusBar` |

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

## Estructura sugerida (resto, a llenar)

| Token | Tamaño | Uso |
|---|---|---|
| `--font-display` | — | Título de canción en Now Playing |
| `--font-body` | — | Listas de menú |
| `--font-caption` | — | Metadata secundaria (artista, álbum) |
