# Sistema de temas

🟢 Definido e implementado — D-289, 2026-08-17. Especificación original en
`PLAN-theme-system.md` (2026-08-16, repo archivado `Aura-Proyect`) y plan de
aterrizaje en `PLAN-themes-impl.md`; este documento describe el sistema **tal
como quedó implementado**, que en algunos puntos aterriza distinto de la
especificación original (señalado abajo). El contrato de formato completo,
compartido con Aura Studio, vive en `CONTRATO-formato-tema.md` (raíz del
repo) — este documento es la explicación de diseño; ese archivo es la fuente
de verdad exacta del formato (claves, tamaños, nombres).

**Principio rector, sin cambios**: Aura Studio es un CONSTRUCTOR de temas, no
un DISTRIBUIDOR. Ningún tema con material de licencia restringida (SF Pro,
SF Symbols) se commitea, se sube a un release ni se comparte — Studio lo
construye localmente desde lo que ya está en la Mac del usuario. El firmware
público sale con **Inter + Lucide/Phosphor** como tema por defecto, completo
y funcional por sí solo.

## Qué NO confundir: "Modo" vs. "Temas"

`aura_settings.theme` (fila "Modo" en Ajustes → Personalización, claro/
oscuro — renombrada de "Tema" en D-292, misma clave `theme:` en `aura.cfg`
sin cambio) es un ajuste **anterior y sin relación** con esto — sigue
existiendo exactamente igual. El sistema de temas de este documento se
llama **"Temas"** en la UI (fila hermana, renombrada de "Estilo" en D-292)
y `aura_style` en el código — nombres distintos a propósito, para que
nadie confunda un paquete de fuentes+íconos+paleta con el interruptor
claro/oscuro.

**Resolución explícita del conflicto (D-292, no dejado en silencio):**

- **Un tema respeta el Modo activo, nunca lo reemplaza.** El formato v1
  exige que todo tema traiga `palette_light_*` **y** `palette_dark_*`
  (§B) — no existe la posibilidad de "un tema con una sola paleta" en este
  formato, así que no hay ningún caso real de "el tema pisa el modo" que
  resolver en tiempo de ejecución.
- **Si un `theme_format` futuro llegara a admitir una sola paleta**
  (decisión abierta, no implementada): la fila "Modo" se **atenúa**
  (`dimmed`, mismo tratamiento que cualquier fila inerte) con su valor
  sustituido por "Fijado por el tema" — nunca se oculta ni se ignora en
  silencio. Ocultarla borraría la explicación; ignorarla dejaría a Modo
  mintiendo sobre lo que en verdad se está mostrando.
- **El Modo no afecta a `--color-accent`.** `aura_accent()` lee
  `accent_rgb24` directo, sin pasar por la paleta ni por el modo — el
  acento libre del usuario es independiente de si la pantalla está en
  claro u oscuro (ver `fundamentos/01-color.md`).
- **El tema no afecta al acento.** El slot `A26_ACCENT` de la tabla de
  paleta de un tema no se usa (D-289); `accent_default`/`accent_presets`
  del manifiesto se validan pero el firmware todavía no los lee
  (`CONTRATO-formato-tema.md` §H).
- **Cambiar de tema no cambia el Modo**, y viceversa — son ajustes
  independientes, cada uno con su propia fila y su propia clave en
  `aura.cfg` (`theme_id` para el tema, `theme` para el modo).

## Qué contiene un tema

| Componente | ¿En el tema? | Obligatorio |
|---|---|---|
| Paleta (8 roles × claro/oscuro) | Sí | Opcional por clave — lo que falte hereda del default compilado |
| Colores fijos de categoría (Ajustes/Video/Fotos/el amarillo de Extras) | Sí | Opcional |
| Tipografías (14 roles → `.fnt`) | Sí | **Obligatorio, los 14** |
| Íconos — máscaras de cobertura (89 × 9 tamaños) | Sí | **Obligatorio, las 801** |
| Íconos — BMP horneados por variante/modo (fallback) | Sí | Opcional (ver "Por qué solo máscaras" abajo) |
| Fondos de panel derecho, tile-icons | Sí | Opcional |
| Acento del usuario (color, no tema) | Sigue siendo 100% del usuario | — |
| `accent_default`/`accent_presets` del tema | El formato los acepta | **No cableados en v1** — ver "Qué queda pendiente" |
| Radios, espaciado, layout, timings, tamaños de buffer | No | Estructura, compilada, un tema no la toca |

## Por qué solo las máscaras son obligatorias (ajuste sobre la spec original)

La especificación original (`PLAN-theme-system.md` N.5) asumía que los BMP
horneados por modo (`icons/{light,dark}/`) eran parte obligatoria del
formato. Al aterrizar la implementación se verificó en el código
(`aura_widgets.c`) que la **máscara de cobertura ya es el camino primario**
para dibujar cualquier ícono — `draw_icon_mask_2()` compone la cobertura
contra el framebuffer real con la tinta del token vivo; los BMP horneados
`light/`/`dark/` son **solo el fallback** si falta la máscara (D-010,
heredado). Consecuencia: **un tema v1 solo necesita las 801 máscaras**
(~5.2 MB) — los horneados (~52 MB) son opcionales. Esto además resuelve por
construcción la restricción heredada de antialias (halo por clave magenta,
dientes de sierra por umbral binario, N.5 de la spec original): una máscara
**es** la cobertura antialiasada misma, sin composición previa contra un
fondo ni clave de transparencia — el constructor no puede repetir esos bugs
porque no hay paso de composición que los introduzca.

## Dónde vive en el disco

```
/.rockbox/aura/themes/<id>/          <id>: [a-z0-9-]{1,32}, nunca "default"
  theme.cfg                          manifiesto, obligatorio
  fonts/<rol>.fnt                    14 obligatorios (title, body, caption,
                                      header, micro, ds_reg_8, ds_semibold_15,
                                      ds_reg_10, ds_bold_10, ds_reg_12,
                                      ds_bold_12, ds_bold_14, ds_bold_18,
                                      ds_medium_16 — mismos 14 roles que
                                      A26_FONT_STYLE_*)
  icons/masks/<icon_key>-<px>.bmp    801 obligatorios (89 nombres × 9 tamaños)
  icons/light/, icons/dark/          opcionales (BMP horneados, fallback)
  backgrounds/<preset>.bmp           opcional
  tile-icons/aura_badge-{light,dark}.bmp  opcional
```

El tema por defecto ("Aura") **no vive aquí** — sigue en las rutas legadas
de siempre (`/.rockbox/fonts/`, `/.rockbox/icons/aura/{masks,light,dark,
backgrounds,tile-icons}/`), sin id de directorio propio (ajuste sobre la
spec original, que proponía moverlo a `themes/default/`: mover el default
habría tocado `rockbox.zip`, el sentinela de "árbol instalado" que usa Aura
Studio (`.rockbox/fonts/a26-title-20.fnt`), y `package_dist.sh` — sin ganar
nada, porque el fallback por archivo hacia esas rutas ya es gratis).

## El submenú "Temas"

Ajustes → Personalización → **Temas** (D-292: submenú y nombre nuevos;
antes Ajustes → Apariencia → "Estilo"), justo debajo de "Modo". Misma
maquinaria
visual que las pantallas de elección existentes (Idioma, Ecualizador):
`MenuList` + `Selector`, checkmark en la fila activa — **sin UI nueva**. La
diferencia es que la tabla de filas se lee del disco en cada entrada a la
pantalla (`aura_style_scan()`, mismo patrón que `aura_music_list_playlists()`
para listar playlists desde `/Playlists`), no de una tabla compilada:

- Fila 0, fija: **"Aura"** (el default). Siempre presente, siempre elegible.
- Una fila por cada `/.rockbox/aura/themes/<id>/theme.cfg` legible.
- Un tema con formato incompatible, manifiesto inválido, o al que le falte
  cualquiera de las 14 fuentes aparece **atenuado** (fila inerte, mismo
  tratamiento que los idiomas sin traducir): se puede recorrer con la rueda,
  SELECT no hace nada.

Elegir un estilo **aplica de inmediato**, sin confirmación ni reinicio — 14
`font_unload` + 14 `font_load` + recarga de paleta, del orden de decenas de
milisegundos. No hay vista previa como paso separado: con `MAXUSERFONTS`
exactamente al límite (14/14, sin hueco para tener dos familias cargadas a
la vez), aplicar **es** la vista previa — y es instantáneamente reversible
eligiendo otra fila.

## Fallback de seguridad (requisito, no comodidad)

Un iPod con la interfaz rota necesitaría Mac y cable para recuperarse — por
eso el sistema nunca puede quedar sin UI legible:

1. Al arrancar, `aura_style_boot()` intenta el `theme_id` guardado en
   `aura.cfg`. Si falla por cualquier motivo (falta el directorio, el
   manifiesto no parsea, `theme_format` es mayor al soportado, o **cualquiera**
   de las 14 fuentes no carga a mitad de camino), cae al default — que
   **siempre** "tiene éxito": las fuentes que falten se degradan
   individualmente a la fuente fija del sistema (`FONT_SYSFIXED`), nunca un
   fallo duro.
2. Al cambiar de estilo en caliente (`aura_style_activate()`), si el
   candidato falla, se **revierte exactamente** al estilo que estaba activo
   antes de la llamada (no al default a secas, salvo que ese también falle).
3. `aura.cfg` **no se reescribe** cuando el arranque tiene que caer al
   default por un estilo roto — el ajuste queda como estaba; si el estilo
   vuelve a estar disponible (p. ej. Studio lo estaba reinstalando), se cura
   solo en el siguiente arranque. Elegir "Aura" a mano desde el submenú sí
   persiste `theme_id: default`.
4. Íconos, fondos y tiles se resuelven **por archivo**: si el estilo activo
   no trae un BMP puntual, ese archivo cae al del default sin afectar al
   resto del tema.

Verificado en el simulador con dos temas rotos a propósito: uno con
`theme_format: 99` (mayor al soportado) y otro con `theme_format: 1` pero
solo 1 de las 14 fuentes presente — ambos aparecen como fila inerte en
"Estilo", y forzar el arranque con cualquiera de los dos como `theme_id`
activo cae al default limpiamente, sin corrupción visual ni cuelgue. Capturas
en `docs/screenshots/themes/`.

## Compatibilidad de formato

El firmware declara `AURA_STYLE_FORMAT_SUPPORTED = 1` (`aura_style_manifest.h`)
y lo escribe en `aura.cfg` como `theme_format_supported` (clave de solo
escritura — Aura Studio la lee del iPod montado; el firmware nunca la relee).
Reglas: `theme_format` igual → carga; mayor → no carga, fallback, fila
inerte; menor (sin caso real todavía, v1 es el primero) → cargaría lo que
entienda, heredando el resto del default. Añadir un rol de fuente, un
`icon_key`, o un tamaño nuevo sube el formato.

## Qué queda pendiente (documentado, no a medias)

- **`accent_default`/`accent_presets` del manifiesto**: el formato los
  acepta y Aura Studio puede escribirlos, pero **el firmware no los lee
  todavía** — el selector de acento (Ajustes → Color de acento) sigue
  usando siempre los 6 presets compilados. Cablearlo es trabajo de
  seguimiento, no una implementación a medias: hoy simplemente no existe,
  documentado así en `CONTRATO-formato-tema.md`.
- **`contract_version` explícito** en el contrato de datos del disco
  (`sync_summary.cfg`/`aura.cfg`) — no implementado, ver
  `CONTRATO-firmware-studio.md` §D.
- El lado constructor pleno de Aura Studio (rasterizar fuentes/íconos del
  sistema del usuario, con `convttf` embebido) es la fase **2B**, posterior
  — ver `PLAN-themes-impl.md` Q4. La Fase 2 actual entrega **2A**: empaquetar
  desde una carpeta de assets ya generados, instalar, listar, activar,
  eliminar — suficiente para construir y probar el tema Apple real desde
  `~/Aura-local/theme-apple-source/` sin escribir un rasterizador nuevo.

## Referencias

- `CONTRATO-formato-tema.md` (raíz del repo) — formato exacto, claves de
  `theme.cfg`, versión.
- `CONTRATO-firmware-studio.md` — cómo Aura Studio consume `theme-format-v1.json`
  y `aura-theme-default.zip` del Release, y el contrato de `theme_id`/
  `theme_format_supported` en `aura.cfg`.
- `fundamentos/01-color.md`, `fundamentos/02-tipografia.md` — los 8 roles de
  paleta y los 14 roles de tipografía que un tema puede sustituir.
- `componentes/left-panel.md` (MenuList, fila inerte), `componentes/selector.md`.
