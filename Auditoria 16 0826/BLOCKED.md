# Trabajo bloqueado, incompleto o pendiente de confirmación

**Snapshot fechado: 2026-08-16.** Ver la nota de vida útil en `RESUMEN.md` — este archivo también es una fotografía, no documentación permanente.

---

## Bloqueos reales de arquitectura o hardware

Nada de esto se intentó y se revirtió — se identificó como no-implementable con la información/infraestructura disponible antes de escribir código, o quedó pendiente de un recurso que no está disponible ahora mismo.

- **Costo real en hardware de CoverFlow (T3.1)**: el render columna-por-columna del carrusel no está medido en ciclos ARM926EJ-S reales — no se sabe con certeza si compite bien con el presupuesto de audio del dispositivo. Pendiente de una sesión guiada con hardware físico.
- **Capacidad de batería del dispositivo real**: `battery_capacity` sigue en el default de `ipod6g.h` (550 mAh) — nunca se verificó contra la etiqueta física de la batería del iPod de pruebas (D-046 solo identificó el modelo del disco duro, no la batería). Pendiente de una sesión con el dispositivo físico abierto o su etiqueta a la vista.
- **Variante dinámica de `DateEditor` (hoja de calendario)**: **Corrección (resync 2026-08-16)** — la pantalla real Ajustes → Fecha y Hora SÍ existe (`draw_date_edit()`, D-170); lo que falta es solo el renderer del ícono dinámico de hoja de calendario para la fila (la variante de reloj analógico hermana ya existe desde D-108). Ya no es un bloqueo de dependencia — es un hueco de implementación normal (`RESYNC-GAPS.md` B6).

## Verificado en código y tests, pendiente de confirmación en hardware/dispositivo real

- **D-273 (Aura Studio, esta sesión)**: el bug de la ruta rápida "ya instalado" que confiaba en evidencia de disco obsoleta para saltarse DFU quedó corregido en código — `swift build` limpio, `swift test` sin regresiones nuevas. **Pendiente de que el dueño reintente la actualización con el iPod físico** y confirme que el botón de rescate ("No arrancó con Aura -- terminar por DFU") aparece y funciona cuando corresponde.
- **Gesto de mantener SELECT (revelar reloj / infraestructura de hold general, B-01/B-02)**: la arquitectura se implementó y cerró en D-108 (`AURA_BUTTON_HOLD`, reutilizando el umbral `REPEAT_START` del driver). Pero el propio D-108 reconoce una limitación real: el arnés de capturas scripted (`apple2026_sim_shot.sh`) no puede simular un botón sostenido el tiempo suficiente para generar `BUTTON_REPEAT` — el gesto de hold en sí **solo es verificable sosteniendo SELECT en el simulador interactivo en vivo**, por el dueño del diseño. No hay evidencia en `DECISIONS.md` de que esa verificación manual se haya hecho después de D-108. Es decir: la implementación probablemente funciona (la lógica está razonada y la navegación normal se verificó sin regresión), pero el gesto específico de hold nunca se confirmó visualmente.
- **Home dividida / pantalla transitoria de Fase 13**: no se pudo verificar visualmente en el simulador porque el fixture de prueba no trae una carátula reconocible por `aura_home_has_content()` — verificado solo por lectura de código y aritmética. La pantalla en sí es transitoria (reemplazada por trabajo de Fase 15), así que el impacto práctico es bajo.
- **`aura_nowplaying.c`, umbrales de `LyricsPanel`** (`LYR_SILENCE_MIN_MS=8000`, `LYR_LINE_READ_MS=3000`): el propio código los marca "valores provisionales" — consistente con que la doc tampoco los da como definitivos. Genuinamente abierto en ambos lados, no es una divergencia doc↔código, es una pregunta de diseño sin cerrar.

## Trabajo de implementación NO EMPEZADO (identificado, sin ambigüedad)

> **Corrección (resync 2026-08-16)**: cuatro puntos de esta sección estaban mal en la auditoría original — se corrigen en línea abajo. La lista canónica y priorizada de huecos de implementación ahora vive en `RESYNC-GAPS.md`.

- ~~**`DateEditor`**: cero código.~~ **FALSO** — SÍ hay pantalla real (`draw_date_edit()`, `aura_screens.c:2822`, D-170); el `grep` original falló por el nombre interno. Lo que sigue pendiente: transición `Shift-and-Reveal` e ícono dinámico de hoja de calendario (`RESYNC-GAPS.md` B5/B6).
- **`modulos/` (composiciones reutilizables entre pantallas)**: cero abstracción de "módulo" en el código — cada pantalla compone sus componentes directamente. Coincide con la doc (⚪ sin empezar).
- **Patrón de transición `Shift-and-Reveal`**: cero referencias en código, documentado sin implementar.
- ~~**Toggle "desactivar sombras desde Ajustes"** no existe~~ **FALSO** — SÍ existe: Ajustes → "Mostrar sombras", `aura_shadows_enabled()` (D-088/D-154). El hueco real es que la sombra del *tile* de SelectionSummary es la única que no lo consulta (`RESYNC-GAPS.md` B3).
- **`DynamicTitle`, transición nunca disparada en la práctica**: el motor de dibujo para `Fade-Slide`/`Scroll-Slide` está completo y correcto, pero el único llamador real (`aura_status_bar_v2.c:265`) siempre pasa `prev_text = NULL`, forzando la rama sin transición — en navegación real el título siempre se dibuja estático. Es trabajo de conexión pendiente. **Añadido en resync**: el título de StatusBar tampoco hace marquee nunca (`marquee_elapsed_ms = 0`) — `RESYNC-GAPS.md` B1/B2.
- ~~**Stagger de entrada de `NowPlaying`**: parece no empezado~~ **FALSO** — SÍ está implementado (`aura_transitions.c:842-900`, D-113) exactamente como la doc lo describe; solo falta ratificación del dueño (`RESYNC-PREGUNTAS.md` C5).

## Trabajo de DISEÑO (no de implementación) marcado sin empezar o con huecos

Según el texto literal de `docs/aura-design-system/00-INDICE.md` y los documentos individuales:

- `modulos/`: **⚪ Sin empezar** — "Composiciones de componentes reutilizables entre pantallas".
- `fundamentos/`: 🟡 — "Tipografía con 9 tokens definidos... resto pendiente" (nota: bordes y breakpoints ya están más completos de lo que este emoji sugiere, ver `RESUMEN.md`).
- `efectos/`: 🟡 — "Regla capturada, faltan valores" (nota: los valores SÍ existen en código, ver `DECISIONS_AUDIT.md` — la doc está desactualizada, no es que falten).
- `componentes/DateEditor`: el índice decía "es solo stub" — corregido en resync 2026-08-16 (pantalla real D-170; transición e ícono dinámico pendientes).
- **14 documentos con al menos un hueco `🔴` marcado explícitamente dentro del propio texto** (no solo en el índice general): `date-editor.md`, `status-bar.md`, `dynamic-title.md`, `selection-summary.md`, `marquee-text.md`, `now-playing.md`, `cover-flow.md`, `selector.md`, `left-panel.md`, `sistema/01-capas-y-jerarquia.md`, `scroll-indicator.md`, `transiciones/00-vocabulario.md`, `clock-indicator.md`, `efectos/01-sombras.md`.

## No encontrado (negativo confirmado, no ausencia de búsqueda)

- Sin marcadores `FIXME`/`XXX:` en `firmware/rockbox/apps/aura/` ni en `studio/AuraStudio/Sources/`.
- `git status` limpio, sin cambios sin commitear ni anomalías en los commits recientes.

## Nota sobre un posible falso bloqueo

Una de las auditorías de origen citó B-01/B-02 (disparador de auto-ocultado / infraestructura de hold) como "bloqueado" basándose en el texto de D-097. Verificado directamente contra `DECISIONS.md`: D-097 sí lo describe como bloqueo en su momento, pero **D-108 lo cierra explícitamente** ("Cierre de B-01/B-02/B-04"). Este archivo ya refleja el estado correcto (ver la entrada de "Gesto de mantener SELECT" arriba, en la sección de verificación pendiente, no en bloqueos reales) — se deja esta nota para que quede claro por qué no aparece en la primera sección pese a que una fuente lo mencionó ahí.
