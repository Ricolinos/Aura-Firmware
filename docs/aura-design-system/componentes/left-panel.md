# LeftPanel

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
sigue siendo 152px, respetando el padding horizontal). Esto es lo que
permite que 10 ítems de 22px de alto llenen exactamente los 220px sin
dejar espacio muerto arriba o abajo: 22px × 10 = 220px, sin gap entre
ítems por defecto.

## Composición interna

`LeftPanel` no es un bloque monolítico — contiene tres elementos con
comportamiento propio, todos dentro de `--layer-panel` (no tienen layer
tokens propios, se ordenan internamente dentro del panel):

### MenuList

La lista de opciones en sí (ej. Música, Videos, Fotos, Podcasts, Extras,
Ajustes, Canciones aleatorias en el menú principal). Es el contenido que
cambia entre menú y submenú sin remontar `LeftPanel`.

**Capacidad:** máximo **10 ítems visibles**, cada uno de **22px de alto**,
sin espaciado entre ellos por defecto (22×10 = 220px exactos).

**Divisor de sección:** cuando hay un cambio de sección dentro de la lista
(ver `sistema/02-navegacion-menus-contenido.md` y el concepto de secciones
ya usado en `componentes/dynamic-title.md`), se muestra una línea de **1px
de alto, 152px de largo** (respeta el padding horizontal de 4px por lado).

**Confirmado: se dibuja superpuesta, sin restar espacio** — los 10 ítems
visibles se mantienen sin importar cuántos divisores de sección haya.

#### Anatomía de un ítem

| Elemento | Posición | Tamaño |
|---|---|---|
| Ícono | 14px desde el borde izquierdo del panel (= 10px desde el borde del padding interno) | Máximo 14×14px (puede ser 12×14 o 14×12, nunca mayor a ese cuadro) |
| Texto | 4px después del ícono | SF Pro Regular, 10px, alineado a la izquierda |
| Elemento opcional derecho | Lado derecho del ítem | Ver tabla abajo — dimensiones individuales pendientes |

**Elementos opcionales del lado derecho** (según la naturaleza de cada
opción — no todos los ítems llevan uno):

| Elemento | Para qué sirve |
|---|---|
| Switch | Valores booleanos |
| Checkmark | Confirmación/selección múltiple |
| Ícono de tache (X) | — |
| Texto alineado a la derecha | Mostrar distintos parámetros/valores |
| Ícono de carga (animado) | Comunicar que la opción está cargando |

🟢 **Elementos opcionales del lado derecho:** catalogados arriba, sus
dimensiones individuales se definen más adelante cuando lleguemos a las
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

## Pendiente de definir

- [ ] ¿`LeftPanel` tiene su propia entrada/salida en `(full) → (split)`
      además de ser "empujado" en `(split) → (full)`?
