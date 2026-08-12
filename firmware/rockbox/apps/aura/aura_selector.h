/* Selector (PLAN.md T2.2, componentes/selector.md): highlight del item
 * seleccionado en MenuList del sistema nuevo. Reemplaza la pastilla
 * SELECTION_FILL gris + texto en acento del sistema viejo -- aca la
 * PASTILLA MISMA es del color de acento (runtime, aura_accent()), sin
 * animacion al moverse entre filas (decision cerrada en el documento:
 * "ya se probo una version animada y no dio buen resultado").
 */
#ifndef AURA_SELECTOR_H
#define AURA_SELECTOR_H

/* Indicador dinamico opcional a la derecha del Selector -- TODO(pendiente-doc):
 * solo la flecha esta implementada por ahora. El icono de carga
 * (selector.md: "sustituye a la flecha cuando el destino esta
 * cargando") queda para cuando exista un consumidor real con estado de
 * carga async que lo necesite -- construirlo sin eso seria adivinar su
 * apariencia (¿spinner rotando? ¿cuantos cuadros?) sin ninguna spec. */
typedef enum {
    AURA_SELECTOR_INDICATOR_NONE = 0,
    AURA_SELECTOR_INDICATOR_CHEVRON,
} aura_selector_indicator_t;

/* Dibuja la pastilla en (x, y, w, h) -- normalmente
 * AURA_DS_METRICS_SELECTOR_WIDTH/HEIGHT (152x22) y radio
 * AURA_DS_METRICS_SELECTOR_CORNER_RADIUS (5), pero se dejan como
 * parametros para que el llamador pueda ajustar por contexto (p. ej.
 * un ancho de columna distinto) sin hardcodear la constante dos veces.
 * `indicator` se dibuja a la derecha si no es NONE. */
void aura_selector_draw(int x, int y, int w, int h, aura_selector_indicator_t indicator);

#endif /* AURA_SELECTOR_H */
