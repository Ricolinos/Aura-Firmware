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
 * la izquierda). No hace nada en modo Ultra minimalista. */
void aura_transition_slide(aura_nav_t *nav, int direction);

#endif /* AURA_TRANSITIONS_H */
