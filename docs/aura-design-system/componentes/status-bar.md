# StatusBar


> **Nota (D-286):** las menciones a "SF Pro" en esta página describen el tamaño/peso con el que se midió el rol tipográfico, no la cara concreta que lo resuelve hoy -- el tema compilado por defecto de este repositorio usa **Inter** para todos los roles (ver `fundamentos/02-tipografia.md` y `PLAN-theme-system.md`).
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
`ClockIndicator` en máximo 46px (D-207, subido de 40 al crecer la
tipografía a 12px), el espacio restante para los 3 íconos + sus
separaciones es ajustado pero viable — confirmado en la práctica: la
batería de 21px es exactamente el máximo que no choca contra el reloj
persistente en el peor caso (ver "Reglas de íconos").

## Reglas de íconos (candado, play/pause, batería)

Siempre se renderizan a la derecha de la barra, con **4px de separación**
entre cada uno. **Alto fijo de 12px para los tres, independientemente de su
ancho** (el ancho varía según el ícono). Regla dura: los íconos **nunca
deben chocar con `ClockIndicator`** — el layout tiene que respetar esto
sobre cualquier otra cosa.

**Corrección 2026-08-14 (auditoría del dueño del diseño, "los íconos deben
tener el mismo alto, esto implicaría agrandar la pila que se ve muy
pequeña"):** el "12px de alto" de arriba es el lienzo, no la tinta real —
`battery.100`/`.50`/`.25`/`.100.bolt` (SF Symbols) tienen un aspecto natural
~2:1 ancho:alto, así que forzados al mismo lienzo CUADRADO que candado/
play-pausa, `contain` quedaba limitado por el ANCHO y la tinta real de la
batería resultaba de solo 6px — la mitad de los 10px que candado y
play-pausa sí alcanzan en ese mismo lienzo cuadrado. La batería (las 4
variantes) ahora se renderiza en un lienzo de **21×12px** (ancho propio,
alto sin cambios) en vez de 12×12 — su tinta real sube a 10px, igual que
los otros dos, salvo `battery-charging` (el rayo superpuesto tiene un
aspecto distinto) que queda en 8px, variación menor y aceptable. 21 es a
la vez el mínimo que logra los 10px de tinta (el salto ocurre entre 20 y
21) y el máximo que en `(split)` no choca contra `ClockIndicator` en el
peor caso real (candado + play/pausa + reloj persistente) — a 22px ya la
cruza. Ver `icon.battery_icon` en `design-system/tokens.json` y
`AURA_DS_METRICS_STATUSBAR_ICON_WIDTH_BATTERY` en `aura_status_bar_v2.c`.

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

El tamaño real es **12px** (D-207). Historia: la spec original decía 8px;
D-205 lo subió a 10px y D-207 a 12px el mismo día (2026-08-14), por
encargo directo del dueño ("en la barra el texto casi no se nota, ayúdame
a hacerlo 2px más grande"). Los 12px reutilizan estilos ya cargados
(`ds_bold_12` / `ds_reg_12`, compartidos con el reproductor) sin consumir
presupuesto de fuentes. Título y hora usan pesos y opacidades distintas —
dos tokens separados (ver `fundamentos/02-tipografia.md`):

| Elemento | Token | Familia/Peso | Tamaño | Opacidad |
|---|---|---|---|---|
| `DynamicTitle` | `--font-statusbar-title` | SF Pro Bold | 12px | 60% |
| `ClockIndicator` | `--font-statusbar-time` | SF Pro Regular | 12px | 80% |

Los íconos no son texto, no usan estos tokens.

## Fondo

**Relleno sólido con `--color-bg-base`** (`SHELL_BG`, el mismo fondo del
shell) — decisión del dueño, D-274 (2026-08-16), ratificando el valor que
D-096 había puesto como provisional. No hay traslucencia ni blur.

Historia: la spec original recomendaba traslucencia simple (alpha blend
sin blur, barata en una franja de 320×20px) con blur real como mejora
opcional, y dejaba el color sólido solo como fallback "si ninguna
traslucencia resultaba viable". En `(split)` la barra vive sobre el
`LeftPanel` blanco, donde cualquier blend es indistinguible de un color
sólido; en `(full)` sí hay contenido debajo (`NowPlaying`, `CoverFlow`),
y aun así el dueño eligió el relleno sólido — queda cerrado.

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

Usa el patrón `Lift-and-Push` (confirmado 2026-08-13, spec completo en
`transiciones/00-vocabulario.md`): la `StatusBar (full)` se levanta y
sale por arriba, hay un hueco mientras el contenido se desplaza, y la
`StatusBar (split)` entra **empujada junto a su panel**, no animada por
su cuenta.

## Pendiente de definir

- [x] **Ancho en px del ícono de batería**: 21px (confirmado 2026-08-14,
      ver corrección arriba) — candado y play/pausa siguen pendientes
      (se asumen cuadrados = 12px por ahora, sin confirmar en el
      documento).
- [ ] Espaciado exacto entre las zonas `DynamicTitle`/`ClockIndicator`/
      íconos (el 4px confirmado es solo entre íconos; la regla de "nunca
      chocar con la hora" ya está confirmada como restricción dura, pero
      no como valor numérico de gap)
- [x] Color de fondo: relleno sólido `--color-bg-base`, sin traslucencia —
      ratificado por el dueño (D-274, 2026-08-16), ver "Fondo"
- [ ] Decisión final: ¿SF Display real, o bitmap propio a 12px inspirado en
      ella? (ver nota de licencia y rendering en `fundamentos/02-tipografia.md`)
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
