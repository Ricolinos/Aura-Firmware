# CoverDrift

Nombre provisional — corrígeme si tienes uno ya definido en tu investigación.

## Capa

`--layer-content`. **Opcional** — solo se monta cuando hay contenido visual
rico disponible (carátulas de álbum cargadas, fotos). Cuando no lo hay, es
`SelectionSummary` en `--layer-base` quien se muestra en su lugar (ver
`componentes/selection-summary.md`). Siempre por debajo de `LeftPanel`
(`--layer-panel`) cuando este está montado.

## Comportamiento

Animación fluida donde las imágenes (carátulas de álbum, fotos) se mueven en
8 direcciones posibles: **0°, 60°, 90°, 120°, 180°, 240°, 270° y 300°**.

**Mecánica del movimiento (no es un recorrido simple):**

- Cada movimiento **termina en el centro** — no empieza ahí. La imagen
  arranca desde una posición descentrada (en alguna de las 8 direcciones) y
  se desplaza *hacia* el centro.
- **No siempre recorre el trayecto completo de extremo a extremo** — la
  distancia recorrida en cada movimiento puede variar, no es fija.
- **Duración: 7 segundos de movimiento constante** por desplazamiento. La
  velocidad exacta (px/s) no es un valor fijo — varía según la distancia a
  recorrer en cada instancia, pero siempre dura 7s.
- **Sensación deseada:** lento pero fluido — nunca entrecortado/trabado.

⚠️ **Interpretación:** leo "movimiento constante" como velocidad
uniforme/lineal durante esos 7s (sin easing de aceleración/desaceleración)
— avísame si en realidad querías algo de easing y solo te referías a que
no se sintiera entrecortado.

## Montaje

**Se necesitan al menos 10 imágenes disponibles** para que `CoverDrift` se
monte — con menos de eso, se queda `SelectionSummary`
(`componentes/selection-summary.md`) en su lugar.

## Transición con `SelectionSummary`

Cross-fade al montarse/desmontarse — spec completo en
`componentes/selection-summary.md`, sección "Transición con `CoverDrift`".

## Sombra de `LeftPanel`

Debe renderizarse una sombra que simule que `LeftPanel` está por encima de
este componente — spec completo en `efectos/01-sombras.md` (regla
actualizada, compartida con `SelectionSummary`).

## Reutilización

Este es el primer candidato real a **módulo** (no solo componente): el mismo
comportamiento de animación se usa tanto para carátulas de álbum (contexto
Música) como para fotos (contexto Fotos). Es un componente que se instancia
con distinta fuente de imágenes pero el mismo comportamiento de movimiento.

Cuando tengamos un tercer caso de reutilización confirmado, esto se mueve de
`componentes/` a `modulos/` formalmente — por ahora se queda aquí porque
solo hay dos contextos confirmados.

## Pendiente de definir

- [ ] ¿Cada imagen individual tiene su propio ciclo de 7s independiente
      (desfasadas entre sí), o todas se mueven sincronizadas al mismo tiempo?
- [ ] ¿Cómo se elige la posición/ángulo de arranque de cada movimiento — es
      aleatorio entre los 8, o sigue algún orden?
- [ ] ¿Qué pasa inmediatamente después de que una imagen llega al centro —
      pausa antes del siguiente movimiento, o arranca el siguiente de
      inmediato?
