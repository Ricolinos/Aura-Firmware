# Resumen de estado — Aura

**Snapshot fechado: 2026-08-16.** Este archivo describe el estado del proyecto en este momento, verificado auditando `docs/aura-design-system/` contra el código real (no solo contra los emojis 🟢/🟡/⚪ que trae el propio índice).

> **Nota de vida útil**: este proyecto ya tuvo antes `RESUMEN.md`/`BLOCKED.md`/`PLAN*.md` en la raíz — se retiraron el 2026-08-12 (ver `DECISIONS.md`, nota junto a D-094) por describir "solo estados pasados" y quedarse obsoletos frente al código. La fuente de verdad viva sigue siendo `docs/aura-design-system/` (actualizada continuamente) y `DECISIONS.md` (diario cronológico). Trata este archivo como una fotografía de hoy, no como documentación permanente — si vuelve a quedar desactualizado, la solución es borrarlo o regenerarlo, no mantenerlo a mano.

---

## Estado del build

| Build | Comando | Resultado |
|---|---|---|
| Firmware ARM real (`ipod6g`) | `make -j$(sysctl -n hw.ncpu)` en `firmware/build-ipod6g/` | ✅ Limpio, sin warnings ni errores |
| Simulador SDL | `firmware/tools/build_sim.sh` | ✅ Limpio (pipeline completo del design system + `rockboxui`). Único aviso no bloqueante: `Image.Image.getdata` deprecado en Pillow (`design-system/generate.py:505`), API que Pillow retira hasta 2027 |
| Test suite de host (C, lógica pura) | `make -C firmware/rockbox/apps/aura/test test` | ✅ 8/8 suites, 100% verde (881 aserciones: nav, lrc, splash_lang, motion, wheel, flow, color, patterns) |
| Aura Studio (Swift) — build | `swift build` en `studio/AuraStudio/` | ✅ Limpio |
| Aura Studio (Swift) — tests | `swift test` | 🟡 191 pruebas, 1 falla real (`LiveEnrichmentIntegrationTests.testCoverArtArchiveFetchesRealCover`) — prueba de integración contra una API externa (cover art), depende de red/disponibilidad del servicio, no del código propio |

`git status` queda limpio tras ambos builds.

---

## `docs/aura-design-system/` — estado por sección

Convención de esta auditoría: **✅ completo** (código cubre lo que la doc describe) · **🟡 parcial** (existe, con huecos concretos) · **⚪ no empezado** · **⚠️ doc desactualizada** (el código avanzó y la doc no se sincronizó de vuelta — ver `DECISIONS_AUDIT.md` para el detalle de cada caso).

### `fundamentos/`

| Archivo | Estado real | Nota |
|---|---|---|
| `01-color.md` | ✅ completo | Acento configurable + color por categoría (D-250), en `tokens.json` → `apple2026_tokens.h`. La tabla `--color-bg-*` sugerida en la doc no tiene tokens 1:1 (los roles existen en `a26_palette` con otros nombres) — gap de nomenclatura, no de función |
| `02-tipografia.md` | ✅ completo, ⚠️ desactualizada | Todos los tokens listados existen. Doc dice `MAXUSERFONTS=12`; código real tiene `14` desde D-267 |
| `03-espaciado.md` | ⚪ no empezado | Sin escala base (`--space-*`) ni equivalente en `tokens.json`; espaciado hardcodeado por componente |
| `04-bordes.md` | ✅ completo, ⚠️ desactualizada | Doc se marca 🟡 "pendiente definir radios" pero el sistema de radios YA existe completo (`corner_radius_screen/card/pill/capsule` + radios específicos por componente, ej. tile SelectionSummary 28px con justificación empírica D-211) |
| `05-breakpoints.md` | ✅ completo, informal | `split`/`full` son centrales en todo el código (`aura_widgets_split_active()`); falta solo formalizar como tabla de tokens |

### `sistema/`

| Archivo | Estado real |
|---|---|
| `01-capas-y-jerarquia.md` | ✅ completo — orden de dibujo real coincide con lo documentado. 2 preguntas "pendientes de definir" siguen abiertas en la doc |
| `02-navegacion-menus-contenido.md` | ✅ completo |
| `03-arbol-de-menus.md` | ✅ se autoaudita con su propio sistema ✅/◐/⬜/⛔, fechado 2026-08-13; 78 `AURA_SCREEN_*` consistentes con su tamaño |
| `04-color-por-categoria.md` | ✅ completo |

### `componentes/` (15)

| Componente | Estado real | Detalle |
|---|---|---|
| StatusBar | 🟡 parcial | Casi todo implementado; fuente real 12px vs. 8px que dice la doc (⚠️ desactualizada tras encargo 2026-08-14); fondo de respaldo sin traslucencia real (`TODO(pendiente-doc)`, D-096) |
| ClockIndicator | ✅ completo | Los 3 pendientes que la doc dejaba abiertos se cerraron en D-108. Misma divergencia de tamaño de fuente que StatusBar |
| DynamicTitle | 🟡 parcial | Motor de dibujo completo, pero el único llamador real nunca pasa `prev_text` — nunca se dispara una transición en navegación real, siempre dibuja estático. **Corrección (resync 2026-08-16)**: además, el título de StatusBar tampoco hace marquee nunca (`marquee_elapsed_ms = 0` siempre) — ver `RESYNC-GAPS.md` B1/B2 |
| CoverDrift | ✅ completo (Música) | Fotos/Video explícitamente fuera de alcance por ahora. ⚠️ doc no menciona que "Ahora suena"/"Canciones aleatorias" también califican (extensión D-266 de esta sesión) |
| SelectionSummary | ✅ implementado, ⚠️ **doc significativamente desactualizada** | El fondo del panel cambió de degradado diagonal calculado a imagen de fondo completo (D-267) — la doc todavía describe el degradado como el fondo vigente. Tampoco refleja: tamaño de fuente real 18/16pt (D-271), centrado del ícono independiente del texto (D-270), centrado del texto en margen completo (D-272), sombra SDF de 12px (D-270) |
| Selector | ✅ completo | Pendiente menor de la doc (ancho de flecha/ícono de carga) sin resolver |
| ScrollIndicator | ✅ completo | Ambos pendientes de la doc resueltos silenciosamente en `tokens.json` |
| MarqueeText | 🟡 parcial | Loop y difuminado implementados; comportamiento al navegar fuera a media vuelta no se pudo verificar con certeza |
| LeftPanel | ✅ completo | Único de los 15 sin archivo `aura_*.c` dedicado (vive repartido en `apple2026_shell.c`/`aura_widgets.c`/`aura_transitions.c`) |
| CoverFlow | ✅ completo | 2 `TODO(pendiente-doc)` explícitos sin resolver (timing de snap y de Flip-and-Flow, ver `DECISIONS_AUDIT.md`) |
| NowPlaying | 🟡 parcial | Doc se declara CERRADO 2026-08-12, pero: umbrales de silencio de `LyricsPanel` siguen marcados "provisionales" en el propio código. **Corrección (resync 2026-08-16)**: el "stagger" de entrada SÍ está implementado (`aura_transitions.c:842-900`, D-113) exactamente como la doc lo describe — solo falta que el dueño lo ratifique (`RESYNC-PREGUNTAS.md` C5), no es un hueco de código |
| LyricsPanel | ✅ implementado | Verificación superficial por límite de tiempo de esta auditoría — recomendable revisar con más profundidad si hace falta certeza línea por línea |
| SearchKeyboard | ✅ implementado | Misma salvedad de verificación superficial |
| WorldClock | ✅ implementado | Misma salvedad de verificación superficial |
| DateEditor | 🟡 parcial | **Corrección (resync 2026-08-16)**: la auditoría original dijo "cero código" — era falso, el `grep` falló por el nombre interno. SÍ hay pantalla real: `draw_date_edit()` (`aura_screens.c:2822`, D-170), rejilla del mes como selector, SELECT avanza día→mes→año, persiste con `rtc_write_datetime()`. Lo que sigue pendiente es la transición `Shift-and-Reveal` y el ícono dinámico de hoja de calendario (`RESYNC-GAPS.md` B5/B6) |

**Conteo**: 9 completos, 5 parciales, 0 no empezados, 3 con verificación superficial únicamente (corregido en resync).

### `transiciones/00-vocabulario.md`

La doc nombra **11 patrones** (no 10 — incluye `Lift-and-Push`, añadido 2026-08-13).

| Patrón | Estado |
|---|---|
| Morph Directo | Implícito/default, sin función propia |
| Push-and-Drop | ✅ `reveal_behind_panels()` |
| Lift-and-Push | ✅ `reveal_behind_panels_exit()` (D-267/D-268, esta sesión) |
| Fade-on-Idle | ✅ implementado |
| Marquee Loop | ✅ implementado |
| **Shift-and-Reveal** | ⚪ **cero referencias en código** — `DateEditor` sí existe (D-170) pero entra por la transición estándar, no por este patrón |
| Fade-Slide | ✅ implementado |
| Scroll-Slide | ✅ implementado |
| Drop-and-Lift | ✅ implementado |
| Push-and-Pull | ✅ implementado |
| Flip-and-Flow | ✅ `aura_transition_flip_and_flow()` |

### `efectos/01-sombras.md`

🟡 parcial. Valores de offset/blur/alpha SÍ implementados (⚠️ doc dice "pendiente de definir" — desactualizada). **Corrección (resync 2026-08-16)**: la auditoría original dijo que el toggle "desactivar desde Ajustes" no existía — era falso. SÍ existe: Ajustes → **"Mostrar sombras"** (`aura_shadows_enabled()`, `apple2026_shell.c:171`, D-088/D-154), consultado por la sombra del LeftPanel, CoverDrift y NowPlaying Modo 4. El hueco real es más pequeño: la sombra del *tile* de SelectionSummary (`draw_tile_shadow()`) es la única que no lo consulta (`RESYNC-GAPS.md` B3).

### `modulos/00-README.md`

⚪ no empezado — coincide exactamente entre doc y código. No existe abstracción de "módulo" reutilizable; cada pantalla compone sus componentes directamente.

---

## Resumen numérico

- **Fundamentos**: 3 completos (1 con doc desactualizada), 1 no empezado, 1 informal-pero-funcional.
- **Sistema**: 4/4 completos.
- **Componentes**: 9 completos, 5 parciales, 0 no empezados, 3 sin verificar a fondo (corregido en resync: DateEditor sí tiene pantalla).
- **Transiciones**: 9/11 implementados, 1 sin implementar, 1 implícito.
- **Efectos**: 1/1 parcial (el toggle SÍ existe — "Mostrar sombras"; solo la sombra del tile de SelectionSummary lo ignora).
- **Módulos**: 0/1.

Para el detalle de *por qué* cada divergencia y qué se decidió en el código donde la doc no alcanzaba a cubrir, ver `DECISIONS_AUDIT.md`. Para lo que sigue genuinamente bloqueado o sin verificar, ver `BLOCKED.md`.
