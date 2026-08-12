# MarqueeText

Nombre provisional — corrígeme si tienes uno ya definido.

Componente de texto reutilizable que implementa el patrón `Marquee Loop`
(spec completo en `transiciones/00-vocabulario.md`). No es exclusivo de
`SelectionSummary` — cualquier texto que no quepa en su contenedor es
candidato a usar este componente.

## Condición de activación

Solo se activa cuando el texto no cabe completo en el espacio disponible.
Texto que sí cabe se muestra estático, sin ningún comportamiento de
`MarqueeText`.

## Ciclo (resumen — spec completo en el vocabulario de transiciones)

1. 2s estático, mostrando el texto visiblemente incompleto
2. 5s de movimiento continuo de derecha a izquierda; el mismo texto reentra
   por la derecha simultáneamente a que sale por la izquierda (loop, sin
   fase de entrada separada)
3. Se repite indefinidamente mientras el texto siga en pantalla

## Difuminado en los bordes

El área visible tiene un difuminado de **4px** en cada extremo (izquierdo y
derecho) para suavizar la entrada/salida del texto en vez de un corte
abrupto.

## Usado por

- `SelectionSummary` (`componentes/selection-summary.md`)
- `DynamicTitle` (`componentes/dynamic-title.md`)

## Candidatos a reutilización (sin confirmar)

- Títulos de canción largos en Now Playing
- Cualquier otro texto largo en contextos de pantalla completa

## Pendiente de definir

- [ ] Espaciado/separador entre el final de una vuelta del texto y el
      inicio de la siguiente (¿hay un gap o el texto se repite pegado?)
- [ ] Comportamiento si el usuario navega away a media vuelta — ¿se
      interrumpe el loop instantáneo o termina el ciclo?
