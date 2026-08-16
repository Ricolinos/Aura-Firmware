# Sistema de Diseño Aura

Documentación de referencia para el diseño de Aura (firmware custom sobre Rockbox
para iPod Classic 6G) y su app complementaria Aura Studio.

Está organizada como la documentación de un design system web (inspirada en la
estructura de Once UI: `Fundamentos → Sistema → Componentes → Transiciones →
Efectos → Módulos`), pero adaptada a lo que realmente somos: **C sobre un
dispositivo de 2008 con una pantalla de 320×240, sin GPU, sin flexbox, sin
compositor de capas real** — así que "capa" aquí no es un `z-index` de CSS,
es una regla de **orden de dibujo (draw order)** que el renderer tiene que
respetar en cada frame.

## Cómo usar esto con Claude Code

Cada componente y patrón tiene un nombre propio (ej. `Push-and-Drop`,
`--layer-chrome`). La idea es que puedas decirle a Claude Code "usa el patrón
Push-and-Drop para esta transición" o "este componente vive en `--layer-panel`"
y que eso sea suficiente contexto — sin tener que re-explicar el comportamiento
cada vez. Cuando el comportamiento de un componente cambie, se actualiza aquí
primero, no solo en el código.

## Estructura

| Carpeta | Contenido | Estado |
|---|---|---|
| `fundamentos/` | Color, tipografía, espaciado, bordes, breakpoints/tamaños de pantalla | 🟡 Tipografía 🟢 (11 roles, presupuesto 14/14 fuentes); color 🟢 (paleta 9 tokens ×2 temas, acento configurable, color por categoría, fondo de panel por acento D-267); bordes 🟢 (tabla completa de radios); breakpoints 🟢 (`split` 160/160, `full` 320); **espaciado ⚪** (sin escala base) |
| `sistema/` | El sistema de capas y jerarquía de dibujo, y la analogía de navegación menú/contenido | 🟢 Definido |
| `componentes/` | Componentes individuales con sus estados | 🟡 `StatusBar` casi cerrado; `LeftPanel`, `Selector`, `ScrollIndicator` con medidas exactas; `SelectionSummary` (rediseño D-267–D-272: fondo por acento, tipografía medida, sombra SDF) y `CoverDrift` con diseño y comportamiento definidos; `CoverFlow` con investigación técnica y decisiones estructurales; `NowPlaying` CERRADO (2026-08-12; los dos valores que quedaban provisionales — stagger de entrada y umbrales de silencio del `LyricsPanel` — ratificados por el dueño en D-274); `LyricsPanel` (Modo 4) con spec propia completa; `SearchKeyboard` completo; `MarqueeText`, `DynamicTitle`, `ClockIndicator` definidos; `DateEditor` con pantalla real (D-170) pero transición e ícono dinámico pendientes |
| `transiciones/` | Vocabulario de patrones de animación reutilizables | 🟢 11 patrones documentados: `Morph Directo`, `Push-and-Drop`, `Lift-and-Push`, `Fade-on-Idle`, `Marquee Loop`, `Shift-and-Reveal` (sin implementar, depende de `DateEditor`), `Fade-Slide`, `Scroll-Slide`, `Drop-and-Lift`, `Push-and-Pull` (300ms, D-274), `Flip-and-Flow` (500ms). Timings de Push/Drop ratificados y tokenizados (D-274) |
| `efectos/` | Sombras, elevación, otros efectos visuales | 🟡 Sombras con valores (LeftPanel 8px/20% lineal; tile SelectionSummary offset 4/35%/blur 12) y toggle global "Mostrar sombras" (D-154). Sin otros efectos |
| `sistema/03-arbol-de-menus.md` | Árbol de navegación completo (original + añadidos de Aura) con el estado real de cada rama | 🟢 Al día 2026-08-13 |
| `sistema/04-color-por-categoria.md` | Jerarquía de color por sección del Menú principal (Música/Ajustes/Video/Fotos/Extras), en cascada a toda pantalla descendiente | 🟢 Definido 2026-08-14 |
| `modulos/` | Composiciones de componentes reutilizables entre pantallas | ⚪ Sin empezar |

🟢 Definido y completo · 🟡 Parcial / con huecos marcados explícitamente · ⚪ Placeholder

## Principio rector

Un componente no es "una pantalla" — es un conjunto de **estados** con una
**capa fija o condicional**, y las transiciones entre estados son **patrones
con nombre**, no animaciones ad-hoc. Si dos componentes distintos necesitan el
mismo tipo de transición, usan el mismo patrón documentado en `transiciones/`,
no una reinvención parecida-pero-distinta.

## Principio de documentación: máxima fidelidad primero

Cuando un efecto o comportamiento tiene un costo de rendimiento relevante
para el hardware de 2008 (blur, sombras, animaciones), esta documentación
describe primero la **versión de máxima fidelidad** — como si el
rendimiento no fuera un problema. Las reducciones por rendimiento (ej. que
el usuario pueda desactivar un efecto desde Ajustes) se documentan como
**configuraciones que restan** sobre esa base, no como condiciones que
alteran la definición del componente en sí. Esto evita tener que
redefinir cada componente dos veces (versión completa vs. versión
optimizada) — hay una sola definición, y las opciones de rendimiento son
casos de "apagar" partes de ella.

## Convención de nombres

- Componentes: `PascalCase` (ej. `StatusBar`, `LeftPanel`)
- Capas (layer tokens): `--layer-nombre`, ordenadas de fondo (0) a frente (más alto)
- Patrones de transición: Nombre propio en `Title Case` (ej. `Push-and-Drop`, `Morph Directo`)
- Estados de un componente: `snake_case` entre paréntesis (ej. `(split)`, `(full)`)
