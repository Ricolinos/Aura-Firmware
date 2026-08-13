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


## Política de listas: menú vs elementos (confirmada 2026-08-13)

El criterio que decide el layout de CUALQUIER lista del sistema:

| Tipo de lista | Qué es | Layout |
|---|---|---|
| **Lista de MENÚ** | Opciones del aparato: raíz, Música, Videos, Fotos, Ajustes, listas de elección (Tema, EQ, Idioma…), Menú pral. | `LeftPanel` **(split)** + `SelectionSummary` |
| **Lista de ELEMENTOS** | Contenido del usuario: canciones, álbumes, artistas, géneros, autores, recopilaciones, listas de reproducción y **todos sus derivados**, a cualquier profundidad | **Pantalla completa (full)** |

La profundidad **no** decide nada: una lista de canciones es pantalla
completa lo mismo si se llegó desde Música que desde Artistas → Álbumes.
Lo que decide es **qué contiene la lista**.

Esta tabla es la **única fuente del layout**: de ella salen el
`LeftPanel`, el ancho de la `StatusBar` (ver la regla dura en
`componentes/status-bar.md`) y el ancho del push T1/T3 de las
transiciones. Una pantalla nueva declara su layout ahí, no lo dibuja
por su cuenta.

**Corolario de transición:** menú → lista de elementos es siempre un
**T3** (push de ancho completo), nunca un T1 de panel.


### Template de lista de ELEMENTOS a pantalla completa (2026-08-13)

- Filas a ancho completo con el `Selector` estándar; el texto se recorta
  antes de invadir la columna del riel.
- **Riel A-Z indexado** pegado al borde derecho (columna de 10px): las
  **iniciales presentes** en la lista (los números y símbolos se agrupan
  bajo `#`), la del elemento seleccionado en **acento** y el resto en
  tinta terciaria. Se dibuja solo desde **12 elementos** — en una lista
  corta sería decoración — y nunca si hay una sola inicial.
- `ScrollIndicator` (Fade-on-Idle) como en cualquier lista larga.
- **Orden alfabético** siempre (sin distinguir mayúsculas, números
  primero), salvo las canciones de un álbum, que van en el orden del
  disco. El playlist de reproducción usa **el mismo criterio** que la
  lista visible: elegir la fila N reproduce siempre la fila N.
