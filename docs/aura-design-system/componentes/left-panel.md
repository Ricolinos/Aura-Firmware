# LeftPanel


> **Nota (D-286):** las menciones a "SF Pro" en esta página describen el tamaño/peso con el que se midió el rol tipográfico, no la cara concreta que lo resuelve hoy -- el tema compilado por defecto de este repositorio usa **Inter** para todos los roles (ver `fundamentos/02-tipografia.md` y `PLAN-theme-system.md`).
## Capa

`--layer-panel`. Montado únicamente en estado `split`. Ver
`sistema/01-capas-y-jerarquia.md`, regla 2 — siempre por encima de
`--layer-base` y `--layer-content`, sin excepción.

## Persistencia entre pantallas

`LeftPanel` no se remonta al navegar entre menú y submenú — es el mismo
componente montado durante toda la navegación en `split`, solo cambia el
contenido de `MenuList` (ver abajo). Ejemplo: Menú principal → Música →
(submenú con Cover Flow, Genius, Listas de reproducción, Artista, Álbumes...)
es una sola instancia de `LeftPanel` actualizando su lista, no una nueva
pantalla con un `LeftPanel` nuevo.

## Capa

`--layer-panel`. Montado únicamente en estado `split`. Ver
`sistema/01-capas-y-jerarquia.md`, regla 2 — siempre por encima de
`--layer-base` y `--layer-content`, sin excepción.

## Qué renderiza

Únicamente listas de **menús** (listas de "puertas") — nunca listas de
**contenido**. Ver `sistema/02-navegacion-menus-contenido.md` para la
definición completa de esta distinción.

## Dimensiones

- **160px × 240px**, pegado al borde izquierdo de la pantalla.
- **Siempre vive debajo de `StatusBar` y ligado a ella**: dondequiera que
  se renderice `LeftPanel`, `StatusBar` se muestra también, en su estado
  `(split)`. No existe un estado donde `LeftPanel` esté montado sin
  `StatusBar`.
- Padding interno: **4px en los 4 lados** (izquierda, derecha, arriba,
  abajo) — con una excepción, ver abajo.

## Espacio útil

Descontando los 20px que ocupa `StatusBar`:

| Medida | Valor | Cómo se calcula |
|---|---|---|
| Alto útil (sin `StatusBar`) | 220px | 240 − 20 |
| Alto útil con padding | 212px | 220 − (4+4) |
| Ancho útil con padding | 152px | 160 − (4+4) |

**Importante:** la lista de `MenuList` y `Selector` usan los **220px
completos**, no los 212px — ver "Excepción de padding vertical" abajo. El
padding horizontal (152px) sí aplica normalmente al contenido de cada ítem.

### Excepción de padding vertical

`Selector` es el único elemento que puede rebasar el padding interno — pero
solo el margen de **arriba y abajo**, nunca el de los costados (su ancho
sigue siendo 152px, respetando el padding horizontal).

**D-195 (encargo del dueño, 2026-08-13):** filas más altas para que se lean
menos apretadas — de 10 ítems visibles a 7, cada uno de 31px de alto en vez
de 22px (≈140% del tamaño anterior). 7 × 31px = 217px de los 220px útiles:
ya no calza exacto como con 22×10 = 220px — quedan 3px sin usar al fondo
del panel, aceptado a cambio de legibilidad.

## Composición interna

`LeftPanel` no es un bloque monolítico — contiene tres elementos con
comportamiento propio, todos dentro de `--layer-panel` (no tienen layer
tokens propios, se ordenan internamente dentro del panel):

### MenuList

La lista de opciones en sí (ej. Música, Videos, Fotos, Podcasts, Extras,
Ajustes, Canciones aleatorias en el menú principal). Es el contenido que
cambia entre menú y submenú sin remontar `LeftPanel`.

**Capacidad:** máximo **7 ítems visibles** (D-195, antes 10), cada uno de
**31px de alto** (antes 22px), sin espaciado entre ellos por defecto
(7×31 = 217px de los 220px útiles del panel).

**Divisor de sección:** cuando hay un cambio de sección dentro de la lista
(ver `sistema/02-navegacion-menus-contenido.md` y el concepto de secciones
ya usado en `componentes/dynamic-title.md`), se muestra una línea de **1px
de alto, 152px de largo** (respeta el padding horizontal de 4px por lado).

**Confirmado: se dibuja superpuesta, sin restar espacio** — los 7 ítems
visibles (D-195) se mantienen sin importar cuántos divisores de sección
haya.

#### Anatomía de un ítem

| Elemento | Posición | Tamaño |
|---|---|---|
| Ícono | 14px desde el borde izquierdo del panel (= 10px desde el borde del padding interno) | Máximo 20×20px (D-195, antes 14×14px — escalado en la misma proporción que la fila) |
| Texto | 4px después del ícono | SF Pro **Semibold, 15px** (D-195: 12px Regular → D-207: Semibold 14px → D-267: 15px, +1pt), alineado a la izquierda |
| Elemento opcional derecho | Lado derecho del ítem | Ver tabla abajo — switch y checkmark confirmados; texto y carga siguen diferidos |

**Elementos opcionales del lado derecho** (según la naturaleza de cada
opción — no todos los ítems llevan uno):

| Elemento | Para qué sirve | Dimensiones |
|---|---|---|
| Switch | Valores booleanos | **Confirmado (D-165/D-167):** pista de 28×14px en cápsula; perilla de 15×10px, también en cápsula (más ancha que alta, no un círculo), con margen de 2px dentro de la pista — nunca la desborda. Encendido: pista del color de acento y perilla en un tono muy claro del propio acento. Apagado: pista gris con contraste y perilla casi blanca. Mismos colores sobre fila en reposo y sobre el `Selector`. |
| Checkmark | Confirmación/selección múltiple | Ícono de 14px, mismo lenguaje que los íconos de fila (D-111) |
| Ícono de tache (X) | — | Pendiente |
| Texto alineado a la derecha | Mostrar distintos parámetros/valores | Pendiente |
| Ícono de carga (animado) | Comunicar que la opción está cargando | Pendiente — sin consumidor real todavía |

**Fila inerte** (`dimmed`, no un elemento del lado derecho sino un estado de
toda la fila): el mismo tratamiento al 50% de opacidad que los modos
deshabilitados del reproductor — la fila se recorre con la rueda pero SELECT
no hace nada. Consumidores reales: idiomas sin traducir (D-013) y, desde
D-289, temas instalados con formato incompatible o manifiesto/fuentes
inválidas en el submenú "Estilo" (`sistema/05-temas.md`) — el catálogo
completo se muestra siempre, el firmware nunca finge soportar lo que no
tiene.

🟢 **Elementos opcionales del lado derecho:** switch (D-165/D-167) y
checkmark (D-111) ya confirmados; el resto se define cuando lleguemos a las
pantallas específicas que los usan — diferido a propósito, no bloqueante.

**Ítems no seleccionados:** siguen exactamente la misma lógica y
dimensiones que un ítem seleccionado — la única diferencia es la ausencia
de `Selector` detrás.

**Texto largo — confirmado:** no usa `MarqueeText`. La estrategia es
**abreviar el contenido** cuando sea posible (ej. "Temperatura de luz" →
"Temp. de luz" — texto más corto escrito a propósito, no un algoritmo
genérico) y, cuando no haya una abreviación razonable, **truncar con
"..."**. Esto es distinto al comportamiento de `DynamicTitle`, que sí usa
`MarqueeText` — son dos estrategias diferentes para dos contextos
distintos.

### Selector

Ver `componentes/selector.md` — spec completo.

### ScrollIndicator

Barra de desplazamiento que **aparece cuando el usuario se mueve dentro de
`MenuList`** y **se desvanece cuando el input está inactivo (idle)**. Usa un
patrón nuevo — ver `Fade-on-Idle` en `transiciones/00-vocabulario.md`.

## Entrada y salida

**Resuelto (D-267):** no, `LeftPanel` no tiene animación de entrada/salida
propia. En `(full) → (split)` entra empujado desde el borde izquierdo junto
con su `StatusBar (split)` — es la fase 3 de `Lift-and-Push` (o su variante
de revelado cuando la ida fue por revelado, ver
`transiciones/00-vocabulario.md`). En `(split) → (full)` sale empujado de la
misma forma, en sentido inverso.
