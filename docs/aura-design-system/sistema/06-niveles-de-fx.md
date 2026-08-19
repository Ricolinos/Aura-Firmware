# Niveles de reducción: Animaciones y Gráficos

Fuente normativa: matriz entregada por el dueño del producto (2026-08-18),
plan de implementación en `PLAN-niveles-fx.md` (raíz del repo, histórico
una vez ejecutado). Esta página es el **índice transversal** de las 6
tablas por componente — cada componente manda sobre su propio detalle; acá
solo vive la regla que los cruza a todos y los enlaces.

## Principio rector

El canon de este sistema de diseño es la **versión de máxima fidelidad**
(`00-INDICE.md` § "Principio de documentación"): Animaciones = Todas,
Gráficos = Todos. Los niveles de abajo son **sustracciones** sobre ese
canon, nunca redefiniciones — un solo código con puntos de sustracción,
no tres implementaciones paralelas por nivel.

## Regla de precedencia (D-a)

**Gráficos decide QUÉ existe; Animaciones decide CÓMO se mueve lo que
existe.** Cuando Gráficos elimina un elemento (p. ej. el tile de
`SelectionSummary` en Ninguno), el ajuste de Animaciones ya no tiene nada
que decidir sobre ese elemento.

Corolario: la **rotación de contenido** (el ciclo de 7s de `CoverDrift`
que cambia de imagen) es contenido, no animación — sigue ocurriendo en
todos los niveles de Animaciones; lo que Animaciones gobierna es el
*cómo* del cambio (drift + fade vs. quieto + corte), nunca el *cuándo*.
Igual el marquee de textos largos: es legibilidad funcional, no ornamento
— se conserva siempre.

## Dónde viven los delays (D-b)

El cambio de delay del panel derecho (2000/1000ms → 500ms con Gráficos =
Todos) vive bajo **Gráficos**, no bajo Animaciones, aunque un lector
razonable lo buscaría ahí. Por qué: el delay no gobierna movimiento sino
cuánto trabajo de dibujo se dispara (decodificar una carátula de 200KB,
repintar el panel) — es una perilla de costo de dibujo, el dominio de
Gráficos ("todo lo que se DIBUJA de más", `aura_settings.h`). Animaciones
solo decide si esa aparición se funde o se corta.

## Tablas por componente

| Componente | Sección | Eje(s) que aplican |
|---|---|---|
| `CoverDrift` | `componentes/cover-drift.md` § Niveles de reducción | Ambos (matriz 3×3) |
| `SelectionSummary` | `componentes/selection-summary.md` § Niveles de reducción | Solo Gráficos (sin animaciones propias) |
| `Music Flow` ↔ Reproductor | `componentes/music-flow.md` § Niveles de reducción | Solo Animaciones |
| Reproductor, Modo 4 (Letras) | `componentes/now-playing.md` § Niveles de reducción del Modo 4 | Solo Animaciones |
| Transiciones generales | `transiciones/00-vocabulario.md` (tabla completo/reducido de cada patrón + nota de `Flip-and-Flow`) | Solo Animaciones |

## Implementación

Punto único de las decisiones que **cruzan** ambos ajustes (para que las
tablas de arriba vivan en un solo lugar del código, no repetidas a mano en
cada componente): `firmware/rockbox/apps/aura/aura_fx.h`. Los gates
binarios preexistentes (Animaciones = Ninguna apaga una transición) siguen
viviendo en cada sitio de `aura_transitions.c`/`aura_nowplaying.c` tal
como estaban — son triviales y no cruzan ejes, forzarlos por `aura_fx.h`
habría sido un refactor sin cambio de conducta.

## Corrección de una regla previa (Q1)

Antes de esta matriz, Gráficos = Ninguno **colapsaba** el panel derecho
completo — sin `LeftPanel`, sin `SelectionSummary`, todo pasaba a
`(full)` (regla documentada en `componentes/status-bar.md`). La matriz
normativa del dueño define contenido PROPIO para ese nivel (degradado de
acento, solo texto), lo que exige que el panel **siga existiendo** — la
regla de `status-bar.md` se corrigió en la misma pasada (2026-08-18):
Gráficos ya no decide layout, solo decide qué se dibuja dentro del panel
una vez que el layout ya lo puso ahí.
