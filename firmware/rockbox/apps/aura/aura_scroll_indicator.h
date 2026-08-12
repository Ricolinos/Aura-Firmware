/* ScrollIndicator (PLAN.md T2.4, componentes/scroll-indicator.md):
 * barra de desplazamiento dentro de LeftPanel/MenuList, en el espacio
 * del padding derecho del panel (no resta espacio del area de
 * contenido). Tamano FIJO (no proporcional al contenido, a diferencia
 * de un scrollbar estandar), capsula, solo aparece si la lista tiene
 * mas de AURA_DS_METRICS_SCROLL_INDICATOR_MIN_ITEMS_TO_SHOW items.
 */
#ifndef AURA_SCROLL_INDICATOR_H
#define AURA_SCROLL_INDICATOR_H

/* Dibuja el indicador si corresponde (no-op si count <= mostrar_desde).
 * `x` es el borde derecho del panel donde vive el carril (el indicador
 * ocupa AURA_DS_METRICS_SCROLL_INDICATOR_THICKNESS px terminando en
 * `x`). `track_y`/`track_h` acotan el carril completo (normalmente los
 * 220px utiles de LeftPanel). `first`/`count`/`visible` describen la
 * ventana actual de la lista (mismos parametros que ya calcula
 * aura_menu_list_draw()). `idle_elapsed_ms` es el tiempo transcurrido
 * desde el ULTIMO movimiento de seleccion -- el llamador lo reinicia a
 * 0 cada vez que `first` (o la seleccion) cambia; controla el fundido
 * de aparicion/desvanecido (patron Fade-on-Idle, T1.1).
 *
 * Simplificacion documentada: el indicador se dibuja directo en su
 * posicion objetivo, sin el "seguimiento suave/animado en tiempo real"
 * que pide el documento -- eso exige que el llamador lleve un estado
 * de posicion ANTERIOR entre cuadros para interpolar hacia el nuevo
 * objetivo (mismo tipo de estado que ya necesito la pastilla animada
 * del sistema viejo antes de que D-075 la retirara por decision del
 * documento) -- diferido, no descartado; la posicion final SI es
 * exacta, solo la transicion entre posiciones es un salto en vez de un
 * deslizamiento. */
void aura_scroll_indicator_draw(int x, int track_y, int track_h,
                                 int first, int count, int visible,
                                 long idle_elapsed_ms);

#endif /* AURA_SCROLL_INDICATOR_H */
