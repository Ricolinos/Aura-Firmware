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

## Color

El ítem seleccionado es el único con color distinto: usa `--color-accent`
(`fundamentos/01-color.md`), default `#FF2D52`. **Configurable por el
usuario desde Ajustes** — nunca tratar este valor como fijo/absoluto en la
implementación, siempre leerlo del ajuste actual.

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
