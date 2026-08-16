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

## Separación entre vueltas (D-086 G11, ratificado D-274)

Entre el final de una vuelta del texto y el inicio de la siguiente hay un
**gap de 24px** (`marquee.loop_gap` en `tokens.json`) — el texto no se
repite pegado. Propuesto bajo mandato autónomo en D-086 y **ratificado por
el dueño el 2026-08-16 (D-274)**: ya no es provisional.

## Interrupción al navegar (ratificado D-274)

Si el usuario cambia de fila (o sale de la pantalla) a media vuelta, el
loop **se interrumpe al instante** — el texto que se iba no termina su
ciclo. Al volver a la misma fila, el `MarqueeText` **reinicia desde la
fase estática de 2s**, no retoma donde iba. Confirmado por el dueño el
2026-08-16 (D-274) como comportamiento definitivo.

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

- [x] Espaciado/separador entre vueltas — resuelto: gap de 24px (D-086
      G11, ratificado por el dueño en D-274 — ver "Separación entre
      vueltas")
- [x] Comportamiento al navegar a media vuelta — resuelto (D-274): se
      interrumpe al instante y reinicia desde la fase estática de 2s al
      volver (ver "Interrupción al navegar"). Sin pendientes en este
      componente.
