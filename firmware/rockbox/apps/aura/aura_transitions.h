/* Transiciones entre pantallas, controladas por el ajuste Animaciones
 * (aura_settings.animation_mode) -- no por Graficos, que decide que se
 * dibuja, no que se mueve:
 *   Ninguna -- sin transicion (redibujo instantaneo).
 *   Minimas -- desplazamiento breve indicando adelante/atras.
 *   Todas   -- el mismo desplazamiento con mas cuadros, mas fluido.
 */
#ifndef AURA_TRANSITIONS_H
#define AURA_TRANSITIONS_H

#include "aura_nav.h"

/* Anima la entrada a la pantalla actual de `nav` desplazandola desde
 * fuera de encuadre. `direction` > 0 = se entro a una pantalla nueva
 * (entra desde la derecha), < 0 = se volvio a la anterior (entra desde
 * la izquierda). No hace nada en modo Ultra minimalista.
 *
 * `width` acota la region del empuje al rango [0, width): en un T1
 * entre dos menus divididos (L1/L2) el empuje solo debe verse en el
 * panel izquierdo (A26_LAYOUT_PANEL_LEFT_WIDTH) -- el panel derecho es
 * una capa aparte que no se mueve y se actualiza con su propio debounce
 * (L3). Para un T3 (cualquier extremo a pantalla completa) pasar
 * A26_SCREEN_WIDTH. Valores fuera de rango caen a ancho completo.
 *
 * Cubre dos de los cuatro patrones de PLAN-UX.md L4: T1 (menu->menu) y
 * T3 (push de pantalla completa) -- ambos son, en la practica, "revela
 * la pantalla nueva desde un borde", la misma animacion con distinto
 * origen segun la direccion. Ver D-058 en DECISIONS.md. */
void aura_transition_slide(aura_nav_t *nav, int direction, int width);

/* T4 (revelado de Coverflow, L4): el contenido nuevo emerge desde
 * ambos bordes hacia el centro en vez de deslizar desde uno solo --
 * distingue visualmente la entrada a Coverflow de una navegacion de
 * lista comun. No hace nada en modo Ultra (Coverflow no existe ahi). */
void aura_transition_reveal(aura_nav_t *nav);

#endif /* AURA_TRANSITIONS_H */
