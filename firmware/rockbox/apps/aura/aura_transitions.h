/* Transiciones entre pantallas, controladas por el modo grafico activo
 * (aura_settings.graphics_mode):
 *   Ultra minimalista -- sin transicion (redibujo instantaneo).
 *   Minimalista        -- desplazamiento breve indicando adelante/atras.
 *   Completo            -- el mismo desplazamiento con mas cuadros y
 *                          duracion, mas fluido.
 */
#ifndef AURA_TRANSITIONS_H
#define AURA_TRANSITIONS_H

#include "aura_nav.h"

/* Anima la entrada a la pantalla actual de `nav` desplazandola desde
 * fuera de encuadre. `direction` > 0 = se entro a una pantalla nueva
 * (entra desde la derecha), < 0 = se volvio a la anterior (entra desde
 * la izquierda). No hace nada en modo Ultra minimalista.
 *
 * Cubre dos de los cuatro patrones de PLAN-UX.md L4: T1 (menu->menu) y
 * T3 (push de pantalla completa) -- ambos son, en la practica, "revela
 * la pantalla nueva desde un borde", la misma animacion con distinto
 * origen segun la direccion. Ver D-058 en DECISIONS.md. */
void aura_transition_slide(aura_nav_t *nav, int direction);

/* T4 (revelado de Coverflow, L4): el contenido nuevo emerge desde
 * ambos bordes hacia el centro en vez de deslizar desde uno solo --
 * distingue visualmente la entrada a Coverflow de una navegacion de
 * lista comun. No hace nada en modo Ultra (Coverflow no existe ahi). */
void aura_transition_reveal(aura_nav_t *nav);

#endif /* AURA_TRANSITIONS_H */
