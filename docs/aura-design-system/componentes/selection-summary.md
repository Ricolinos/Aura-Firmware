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
renderiza "suelto" — va sobre un **tile de 90×90px con esquinas redondeadas
visibles de 8px**, con el **símbolo interno de ~60×60px** (o ligeramente
más grande). Estilo app icon de iOS: símbolo claro sobre tile de color
pleno. El conjunto va centrado horizontal y verticalmente en el espacio
disponible del lado derecho.

*(Nota: la medida original de 40×40px quedó obsoleta con este rediseño.)*

**Color del tile:** por ahora, `--color-accent`
(`fundamentos/01-color.md`). Más adelante se definirá un mapeo de color por
selección (Música, Videos, Fotos, etc., como sugieren los mockups con rojo
y verde) — el dueño del diseño avisará cuándo; no es un pendiente activo.

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
