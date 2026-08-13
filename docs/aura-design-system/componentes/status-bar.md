# StatusBar

Un solo componente, dos estados. No son dos componentes distintos que se
alternan — es el mismo `StatusBar` con geometría y contenido distintos según
el estado de pantalla.

## Capa

`--layer-chrome`. Ver `sistema/01-capas-y-jerarquia.md`, regla 1: cuando
`StatusBar` está montada, nada se dibuja nunca por encima de ella — pero su
montaje es condicional por pantalla, no universal. Falta inventariar qué
pantallas la omiten y por qué (hipótesis: pantallas de reproducción a
pantalla completa tipo cover art puro, a confirmar).

## Estados

### `(split)`

- Ocupa la mitad izquierda del ancho de pantalla.
- Se muestra cuando el layout general está en estado `split` (panel izquierdo
  + panel derecho visibles simultáneamente).
- Va pegada a la parte superior del panel izquierdo (`--layer-panel`).

### `(full)`

- Ocupa el ancho completo de la pantalla.
- Se muestra cuando el layout general está en estado `full` (sin panel
  izquierdo montado).
- Va pegada a la parte superior de toda la pantalla.

## Especificación de layout

Ambos estados comparten altura, padding y contenido — solo cambia el ancho.

| Propiedad | `(split)` | `(full)` |
|---|---|---|
| Alto | 20px | 20px |
| Ancho | 160px | 320px |
| Padding interno | 4px | 4px |

## Contenido y orden

Mismos 5 elementos en ambos estados, pero en **orden distinto**:

| Estado | Orden (izquierda → derecha) |
|---|---|
| `(split)` | `DynamicTitle` → `ClockIndicator` → Candado → Play/Pause → Batería |
| `(full)` | `ClockIndicator` → `DynamicTitle` → Candado → Play/Pause → Batería |

- **`DynamicTitle`** (nombre de menú o sección) — spec completo en
  `componentes/dynamic-title.md`.
- **`ClockIndicator`** (hora) — spec completo en
  `componentes/clock-indicator.md`. En `(full)`, su visibilidad determina
  la posición de `DynamicTitle` (ver ese archivo).
- **Íconos** (candado, play/pause, batería) — reglas abajo.

⚠️ **Nota de espacio en `(split)`:** el ancho útil tras padding es
160 − (4×2) = **152px**. Con `DynamicTitle` en máximo 60-80px y
`ClockIndicator` en máximo 40px, el espacio restante para los 3 íconos +
sus separaciones es ajustado pero parece viable — falta el tamaño exacto de
cada ícono para confirmarlo con certeza (ver Pendientes).

## Reglas de íconos (candado, play/pause, batería)

Siempre se renderizan a la derecha de la barra, con **4px de separación**
entre cada uno. **Alto fijo de 12px para los tres, independientemente de su
ancho** (el ancho varía según el ícono). Regla dura: los íconos **nunca
deben chocar con `ClockIndicator`** — el layout tiene que respetar esto
sobre cualquier otra cosa.

- **Batería es el único ícono persistente** — siempre visible, pegada al
  borde derecho de la barra.
- Candado y play/pause son condicionales y se renderizan **a la izquierda**
  de la batería, según el estado del dispositivo:

| Estado del dispositivo | Orden (izquierda → derecha) |
|---|---|
| Bloqueado, sin reproducción activa | Candado → Batería |
| Bloqueado, reproduciendo | Candado → Play/Pause → Batería |
| Desbloqueado, reproduciendo | Play/Pause → Batería |
| Desbloqueado, sin reproducción | Batería sola *(confirmado)* |

## Tipografía

**Corregido:** el 12px original era una referencia al máximo/cap-height, no
el tamaño de fuente real. El tamaño real es **8px**. Título y hora usan
pesos y opacidades distintas — dos tokens separados (ver
`fundamentos/02-tipografia.md`):

| Elemento | Token | Familia/Peso | Tamaño | Opacidad |
|---|---|---|---|---|
| `DynamicTitle` | `--font-statusbar-title` | SF Pro Bold | 8px (máx. 12px) | 60% |
| `ClockIndicator` | `--font-statusbar-time` | SF Pro Regular | 8px (máx. 12px) | 80% |

Los íconos no son texto, no usan estos tokens.

## Fondo

Dos opciones documentadas, con recomendación:

- **Traslucencia simple (alpha blend, sin blur) — recomendado por default.**
  Viable en este hardware: es una mezcla de color sobre lo ya dibujado en una
  franja de 320×20px (6,400px), operación barata incluso sin GPU.
- **Blur real (frosted glass)** — técnicamente posible por ser una franja
  chica, pero requiere recalcular el blur cada frame si algo se mueve debajo
  (ej. `CoverDrift` si llega a pasar por esa zona). Marcado como mejora
  opcional a evaluar después, no bloqueante para el primer prototipo.
- Si ninguna traslucencia es viable en la práctica al implementarla, el
  fallback es color sólido — valor exacto pendiente.

En ambos estados, `StatusBar` se ancla arriba — eso no cambia entre estados.

## Transición `(split) → (full)`

Usa el patrón `Push-and-Drop` (spec completo del patrón en
`transiciones/00-vocabulario.md`). Aplicado específicamente a `StatusBar`:

1. El contenido de `(full)` entra desde la derecha, empujando fuera de
   pantalla al panel izquierdo — y la `StatusBar (split)` se va con él,
   empujada, no se anima independientemente.
2. Mientras dura ese empuje, no hay ninguna `StatusBar` en pantalla.
3. Cuando el empuje termina, `StatusBar (full)` entra cayendo desde arriba
   hasta su posición anclada.

**Confirmado con un segundo caso:** el flujo de Ajustes → Fecha y Hora → Fecha
(ver `componentes/date-editor.md`) también dispara `(split) → (full)` con
`StatusBar (full)` entrando en Drop desde arriba — no es un comportamiento
exclusivo de la navegación por menú principal, es el comportamiento general
de cualquier transición a pantalla completa.

## Transición `(full) → (split)`

**No definida.** Ver estado abierto en `transiciones/00-vocabulario.md`.

## Pendiente de definir

- [ ] **Ancho en px de cada ícono** (el alto ya está confirmado en 12px
      para los tres — falta el ancho para calcular el espacio total)
- [ ] Espaciado exacto entre las zonas `DynamicTitle`/`ClockIndicator`/
      íconos (el 4px confirmado es solo entre íconos; la regla de "nunca
      chocar con la hora" ya está confirmada como restricción dura, pero
      no como valor numérico de gap)
- [ ] Valor exacto del color sólido de fondo (fallback si la traslucencia no
      resulta viable en la implementación real)
- [ ] Decisión final: ¿SF Display real, o bitmap propio a 12px inspirado en
      ella? (ver nota de licencia y rendering en `fundamentos/02-tipografia.md`)
- [ ] Transición inversa `(full) → (split)`
- [ ] Timing exacto de las 3 fases del `Push-and-Drop` para este componente
- [ ] ¿`StatusBar` reacciona a la sombra descrita en `efectos/01-sombras.md`,
      o al estar en `--layer-chrome` queda siempre por encima/exenta de ese
      efecto?


## Regla dura: (split) ⇔ LeftPanel (confirmada 2026-08-13)

La barra va en **`(split)` si y solo si el `LeftPanel` de 160px está en
pantalla**; en cualquier otro caso va en `(full)`. No es una decisión
por pantalla: **barra y panel se derivan del MISMO dato** (el layout
declarado por la pantalla, combinado con el ajuste de Gráficos, que
puede apagar el panel). Consecuencias:

- Con **Gráficos = Ninguno** no hay `LeftPanel`, así que tampoco hay
  barra `(split)` ni `SelectionSummary` — todo pasa a `(full)`.
- Los estados vacíos y de espera heredan el layout de su pantalla; no
  pueden dibujar barra `(full)` dentro de un contexto `(split)`.
- Una pantalla nueva no elige el ancho de su barra: declara su layout
  y la barra sale de ahí.

## Título en (full): centrado real (confirmado 2026-08-13)

En `(full)` el título va **centrado de verdad**: se centra el TEXTO
medido dentro de su caja de 120px, no solo la caja. (Antes se centraba
la caja y el texto se dibujaba pegado a su borde izquierdo, así que un
título corto quedaba visiblemente descentrado ~40px.) Si el texto no
cabe en la caja, manda el `MarqueeText`, que arranca desde la izquierda
como siempre. En `(split)` el título sigue alineado a la izquierda.
