# Selector

El highlight que indica cuál ítem de `MenuList` (`componentes/left-panel.md`)
está seleccionado actualmente.

## Dimensiones

- **152px × 22px** — mismo ancho útil que el resto del contenido del panel
  (respeta el padding horizontal de 4px), mismo alto que un ítem de
  `MenuList`.
- Esquinas redondeadas: **5px** de radio.
- Es el **único elemento de `LeftPanel` que puede rebasar el padding
  interno** — pero solo el margen de arriba y abajo, nunca los costados
  (ver `componentes/left-panel.md`, "Excepción de padding vertical").

## Color (precisado por el dueño — D-112, 2026-08-12)

**La pastilla es gris** (`SELECTION_FILL`, varía por tema — no es el
acento). El `--color-accent` (`fundamentos/01-color.md`, default
`#FF2D52`) tiñe el **contenido** del ítem seleccionado: su **texto**, su
**ícono** (variante `-on`) y la **flecha** del indicador dinámico — no el
fondo del `Selector`. Una primera lectura de este documento había pintado
la pastilla misma de acento; el dueño lo corrigió en vivo: "el color de
acento sobre el elemento seleccionado aplica al texto y al ícono, no al
seleccionador; el seleccionador debe ser de un color gris". Es el mismo
lenguaje de selección que ya usaban las listas de contenido — un solo
lenguaje en toda la app.

El acento sigue siendo **configurable por el usuario desde Ajustes** —
nunca tratar este valor como fijo/absoluto en la implementación, siempre
leerlo del ajuste actual.

## Movimiento entre ítems

**Confirmado: salta directo, sin animación.** Ya se probó una versión
animada y no dio buen resultado — decisión tomada, no es un pendiente.

## Indicador dinámico (flecha / ícono de carga)

Elemento opcional, configurable por el usuario, renderizado a la derecha
del `Selector`:

- **Flecha de selección** — indica que ese ítem lleva a otro nivel de
  navegación. Se muestra **solo** cuando el ítem no tiene ya otro elemento
  opcional del lado derecho (ver tabla en `componentes/left-panel.md`) **y**
  cuando lleva a un componente de pantalla completa.
- **Ícono de carga** — sustituye a la flecha cuando el destino está
  cargando. Mismo espacio, mismas reglas de aparición.

### Dimensiones y posición del indicador

- Alto máximo: **12px**, centrado verticalmente respecto al `Selector`
  (que mide 22px de alto).
- Posición: **4px de distancia del borde derecho del `Selector`**.

## Ítems no seleccionados

No tienen `Selector` renderizado — sin highlight, sin color especial, sin
indicador dinámico. Su layout interno (ícono, texto, elemento opcional) es
idéntico al de un ítem seleccionado; la única diferencia es la ausencia de
este componente detrás.

## Pendiente de definir

- [ ] Ancho exacto de la flecha/ícono de carga (solo se dio el alto máximo)


## Fila inerte (confirmada 2026-08-13)

Una fila que existe en el árbol del original pero **todavía no tiene
contenido propio** en Aura se dibuja **inerte**: mismo texto e icono, al
**50% de opacidad**, y `SELECT` no hace nada sobre ella. Es el mismo
lenguaje que los modos deshabilitados del reproductor
(`componentes/now-playing.md`). Se puede recorrer con la rueda — el
catálogo completo se ve — pero el firmware no finge soportar lo que no
tiene. Alternativa cuando la pantalla sí existe pero está vacía: entrar
y mostrar un **estado vacío honesto** con su título.
