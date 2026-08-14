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
claro sobre tile de color pleno. El conjunto va centrado horizontal y
verticalmente en el espacio disponible del lado derecho.

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

## Fondo del panel derecho (confirmado)

El fondo de `SelectionSummary` no es neutro — es un **degradado en
diagonal** compuesto por:

- **`--color-accent` al centro**
- "Contaminado" por dos colores derivados: uno **ligeramente más oscuro**
  y otro **ligeramente más claro** que el acento.

Como el acento es configurable por el usuario, los dos colores derivados
deben **calcularse** a partir del acento vigente (no ser valores fijos).

🔴 Pendiente: dirección exacta de la diagonal (¿de qué esquina a qué
esquina va el claro y el oscuro?) y el porcentaje de aclarado/oscurecido
de los colores derivados.

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

## Texto: `MarqueeText`

Cuando el texto no cabe en el espacio disponible, usa el componente
`MarqueeText` (`componentes/marquee-text.md`), que implementa el patrón
`Marquee Loop` — loop continuo de derecha a izquierda, sin transición de
entrada, 2s estático + 5s en movimiento por ciclo. Texto que sí cabe se
muestra estático, sin ningún comportamiento de `MarqueeText`.

## Variante dinámica

Existe al menos un caso (Ajustes → Fecha y Hora) donde en vez de un ícono
estático se muestra un ícono "trabajado"/dinámico (hoja de calendario que
renderiza mes y día actual dentro de sí mismo), más textos adicionales
alrededor (hora arriba, algo más abajo). Ver
`componentes/date-editor.md` — todavía sin resolver si esto es una variante
de `SelectionSummary` o un componente distinto.

## Comportamiento

**Ícono y texto (superior/inferior): ambos cambian de forma instantánea**
al cambiar de selección en `LeftPanel` — sin transición, sin `Fade-Slide`
ni `Scroll-Slide`. A diferencia de `DynamicTitle`, `SelectionSummary` no
anima sus cambios de valor.

## Transición con `CoverDrift`

Cuando el contenido rico (carátulas, fotos) termina de cargar y
`CoverDrift` (`componentes/cover-drift.md`) se monta en `--layer-content`,
la transición entre ambos es un **cross-fade** — `SelectionSummary` se
desvanece mientras `CoverDrift` aparece. Aplica igual en reversa (si el
contenido rico deja de estar disponible).

## Sombra de `LeftPanel`

Debe renderizarse una sombra que simule que `LeftPanel` está por encima de
este componente — spec completo en `efectos/01-sombras.md` (regla
actualizada, compartida con `CoverDrift`).

## Pendiente de definir

- [ ] Producción de un ícono único por cada ítem de todo el árbol de menús
      (regla de sistema ya cerrada — esto es trabajo de asset production)
- [ ] Relación exacta con la variante dinámica de Fecha y Hora
