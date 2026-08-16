# IndexRail

🟢 Definido (D-276, 2026-08-16). Riel vertical de índice alfabético `#`+A–Z
pegado al borde derecho de las **listas de elementos a pantalla completa**
(canciones, artistas, álbumes por nombre, géneros… — todo lo que dibuja
`aura_widgets_draw_list()` en estado `(full)`). Nombre interno en código:
"riel A-Z" (`draw_index_rail()`, `aura_widgets.c`).

## Dónde vive

- Solo en `(full)`. En `(split)` ese espacio es el panel derecho
  (`SelectionSummary`/`CoverDrift`) y el riel no se monta.
- Misma capa que el contenido de la lista (`--layer-base`,
  `sistema/01-capas-y-jerarquia.md`): no flota sobre nada, es parte del
  dibujo de la lista, siempre por debajo de la `StatusBar`.
- El texto de cada fila se recorta antes de invadir su columna (12px:
  ancho del riel + `--space-xs`).

## Cuándo aparece

Solo si la lista tiene **12 elementos o más** (`index_rail.min_items`) — en
una lista de 5 filas sería decoración. Debajo de ese umbral la lista no
tiene riel, y el `ScrollIndicator` se comporta como en cualquier otra lista
(ver "Relación con ScrollIndicator").

## Anatomía

| Parámetro | Valor | Token / origen |
|---|---|---|
| Columna | **10px** de ancho, pegada al borde derecho (x = 310…319) | `index_rail.width` |
| Rango vertical | Desde el tope de la lista (StatusBar + 4px = y 24) hasta el borde inferior (y 240): **216px** | — |
| Posiciones | **27 fijas: `#` + A–Z**, siempre las 27 dibujadas, en ese orden. `#` agrupa dígitos, acentuadas y símbolos (mismo criterio que el orden alfabético de la lista, que pone los números antes que las letras) | D-276 |
| Paso vertical | 216 ⁄ 27 = **8px** por letra | derivado |
| Fuente | **SF Pro Regular 7px** (`type_scale.micro`, `a26-micro-7.fnt`, glifo de 8px de alto — el paso exacto). Es la fuente que la base histórica siempre reservó para el riel ("7 px SF Pro: riel A-Z") y ya estaba cargada; con `ds_reg_8` (glifo 9px) las últimas tres letras no cabían | `type_scale_roles.index_rail` |
| Centrado | Cada glifo centrado horizontalmente en la columna | — |
| Fondo | Ninguno: letras sueltas sobre `--color-bg-base` | — |

Antes de D-276 el riel dibujaba **solo las iniciales presentes** en la lista,
repartidas para llenar los 216px — una lista con A, C, M, S mostraba
exactamente `A C M S` con 54px entre letra y letra, y la barra cambiaba de
forma con cada lista. El dueño pidió que todas las letras estén siempre y
que las vacías se lean como deshabilitadas.

## Estados por letra

| Estado | Cuándo | Tinta |
|---|---|---|
| **Seleccionada** | Es la inicial del elemento actualmente seleccionado — sigue el scroll en tiempo real | `--color-accent` (`A26_ACCENT` del tema) |
| **Presente** | Al menos un elemento de la lista empieza con esa letra (o cae en `#`) | `A26_TEXT_TERTIARY` |
| **Ausente (deshabilitada)** | Ninguna entrada empieza con esa letra | `A26_SHELL_RAIL` — el gris de rieles/separadores ya existente (`#C6C6C8` claro / `#3A3A3C` oscuro); cero tokens nuevos (D-276, Q5) |

Una letra deshabilitada **no es seleccionable**. Hoy eso es trivialmente
cierto porque nada del riel es seleccionable (ver Comportamiento); si algún
día existe salto por letra, debe saltar solo entre presentes.

## Comportamiento

- **Indicador pasivo**: la letra en acento es la inicial del elemento
  seleccionado y se mueve con la rueda; el riel comunica posición
  aproximada de forma continua.
- **Sin salto por letra** (hojeo rápido con la rueda). El gancho existe
  desde D-077 (`aura_wheel_should_hop_letters()`, umbral 420°/s) sin
  consumidor — construirlo es un encargo aparte con su propia coreografía
  (¿lupa? ¿la letra crece?), decidido así por el dueño en D-276 (Q4).
- Sin transición propia: aparece y desaparece con la lista.

## Relación con ScrollIndicator (D-275/D-276, Q2)

**Son excluyentes**: cuando el riel está montado (lista de ≥ 12 elementos
en `(full)`), el `ScrollIndicator` **no se dibuja**. Decisión del dueño
(Q2, opción a): la letra en acento del riel ya comunica la posición en
tiempo real — y con las 27 posiciones fijas lo hace de forma continua — y
ambos vivían en la misma columna (el pulgar de 4px pasaba exactamente por
el centro de las letras y les borraba píxeles al pintar sus esquinas). No
había dónde reposicionar el pulgar sin robarle ancho al texto de las filas.

| Elementos en la lista `(full)` | Qué se ve en la columna derecha |
|---|---|
| ≤ 10 | Nada |
| 11 | `ScrollIndicator` (su umbral es > 10; el riel todavía no monta) |
| ≥ 12 | `IndexRail` solo |

En `(split)` no hay riel nunca; el `ScrollIndicator` sigue sus reglas
normales. La condición vieja "y al menos dos iniciales distintas" (D-155)
desapareció con las 27 posiciones fijas: el riel siempre tiene forma
completa.

## Tokens

- `aura_ds.metrics.index_rail.width` = 10
- `aura_ds.metrics.index_rail.min_items` = 12
- `aura_ds.type_scale_roles.index_rail` = `micro` (7px SF Pro Regular)
- Colores: `A26_ACCENT` / `A26_TEXT_TERTIARY` / `A26_SHELL_RAIL` de la paleta

## Origen

- D-073 (Fase 27): especificado en la base histórica ("riel A-Z con lupa"),
  diferido por falta de una lista indexada.
- D-155 (2026-08-13): construido como indicador pasivo con las iniciales
  presentes, junto con el orden alfabético real de las listas.
- D-276 (2026-08-16): 27 posiciones fijas, estado deshabilitado, fuente de
  7px, geometría a tokens, y este archivo (antes solo existían cinco líneas
  en `sistema/02-navegacion-menus-contenido.md`).

## Pendiente de definir

- [ ] Salto por letra con la rueda rápida (`Fade`/lupa/crecimiento de la
      letra activa) — encargo aparte, gancho listo (D-077).
- [ ] La base histórica pedía **Semibold** para la letra activa y una
      **lupa**; hoy la activa es Regular en acento y no hay lupa. Confirmar
      si se conserva así o se recupera.
