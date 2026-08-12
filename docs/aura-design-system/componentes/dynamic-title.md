# DynamicTitle

Nombre provisional. Elemento de texto en `StatusBar` que muestra el nombre
del contexto de navegación actual — puede ser un **menú** o una **sección**.

## Qué muestra

- **Menú**: nombre de una lista de opciones similares (ej. "Ajustes",
  "Música").
- **Sección**: subdivisión dentro de una lista — de menús (ej. dentro de
  Ajustes: Personalización, Visualización, Rendimiento, Sistema) o de
  elementos (ej. dentro de una lista de Canciones/Artistas/Álbumes/Géneros,
  secciones alfabéticas A, B, C...).

## Ancho máximo

Depende del estado de pantalla y de si `ClockIndicator` (la hora) está
visible en ese momento:

| Estado | Con hora visible | Sin hora visible |
|---|---|---|
| `(split)` | 60px | 80px |
| `(full)` | 120px | 120px (asumido igual — ver Pendientes) |

En `(full)`, la posición (no solo el ancho) también depende de
`ClockIndicator` — ver "Interacción con la hora" en
`componentes/status-bar.md` y el spec completo en
`componentes/clock-indicator.md`.

## Cuando el texto no cabe en el ancho máximo

Usa `MarqueeText` (`componentes/marquee-text.md`), con un refinamiento
confirmado aquí que aplica a `MarqueeText` en general: difuminado de 4px en
ambos extremos del área visible, para que el texto entre/salga de forma
sutil en vez de cortarse abruptamente.

## Transición al cambiar de valor

Dos comportamientos distintos según el tipo de cambio — esto no es
`MarqueeText` (que es para overflow), es la transición de un valor a otro:

### Cambio de nombre de menú

Patrón `Fade-Slide` horizontal (`transiciones/00-vocabulario.md`).
**Confirmado:** al entrar a un menú, el título renderiza de derecha a
izquierda. Al salir, de izquierda a derecha.

### Cambio de nombre de sección

Patrón `Scroll-Slide` vertical (`transiciones/00-vocabulario.md`).
**Confirmado:** al hacer scroll hacia abajo, el texto nuevo entra desde
abajo y el viejo sale hacia arriba. Al hacer scroll hacia arriba, el texto
nuevo entra desde arriba y empuja al viejo hacia abajo.

## Pendiente de definir

- [ ] Confirmar ancho máximo en `(full)` sin hora visible (asumido 120px)
- [ ] Espaciado entre `DynamicTitle` y sus elementos vecinos dentro de la barra
