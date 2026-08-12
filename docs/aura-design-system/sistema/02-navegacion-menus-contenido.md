# Navegación: Menús vs. Contenido

Concepto de sistema, no exclusivo de un componente — afecta qué puede vivir
en `LeftPanel` y qué no, y establece el vocabulario que se usa en el resto
de la documentación.

## La analogía

Los **menús** son como **puertas** que llevan a **habitaciones**, donde
puede haber más puertas, o no. Cuando ya no hay más puertas, solo queda la
habitación en sí.

- **Menú** = una lista de puertas (submenús). Navegar un menú es abrir
  puertas que llevan a más puertas, hasta llegar a una habitación.
- **Contenido** = lo que compone una habitación — no es la habitación
  misma, es lo que hay dentro (muebles, pósters, artefactos). En términos
  del sistema: canciones, fotos, videos — los elementos "finales" de la
  navegación.

## Regla dura

**`LeftPanel` únicamente renderiza listas de menús (listas de puertas) —
nunca listas de contenido.** Si una lista muestra canciones, fotos, videos
u otros elementos finales, no vive en `LeftPanel`, sin excepción.

## Matiz importante

No toda lista de menú vive necesariamente en `LeftPanel`. Hay listas que
pueden mostrarse a pantalla completa — esas se detallan en su propio
componente cuando lleguemos a ellas, no aquí.

## Por qué esto importa para el resto del sistema

Esta distinción es la que determina, por ejemplo, cuándo algo debe
implementarse como un ítem de `MenuList` dentro de `LeftPanel`
(`componentes/left-panel.md`) versus cuándo debe ser un componente de
pantalla completa o vivir en `--layer-content`/`--layer-base` del lado
derecho (`componentes/selection-summary.md`, `componentes/cover-drift.md`).
