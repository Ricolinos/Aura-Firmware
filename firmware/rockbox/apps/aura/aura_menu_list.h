/* MenuList v2 (PLAN.md T2.3, componentes/left-panel.md) -- lista de
 * MENUS (nunca de contenido, ver sistema/02-navegacion-menus-contenido.md)
 * dentro de LeftPanel. En paralelo a aura_widgets_draw_list() del
 * sistema viejo mientras dura la migracion -- ver DECISIONS.md para el
 * alcance real de esta tarea (geometria nueva construida y verificada
 * en un consumidor real; migrar las ~30 pantallas existentes que hoy
 * usan el widget viejo es trabajo de seguimiento, no de esta pasada).
 */
#ifndef AURA_MENU_LIST_H
#define AURA_MENU_LIST_H

typedef struct {
    const char *label;
    const char *icon_name; /* NULL = sin icono */
} aura_menu_item_v2_t;

/* Dibuja hasta AURA_DS_METRICS_MENU_LIST_MAX_VISIBLE_ROWS (10) filas
 * VISIBLES de `items` (de `count` totales) en el panel izquierdo
 * (siempre AURA_DS_METRICS_LEFT_PANEL_WIDTH de ancho, anclado en `x`),
 * empezando en `y`. `selected` es el indice resaltado con el Selector
 * nuevo (aura_selector.h) -- salto directo, sin animacion (decision
 * cerrada del documento). Si `count` > 10, la ventana visible se centra
 * en `selected` (mismo criterio de windowing que ya usa
 * aura_widgets_draw_list() del sistema viejo) y se dibuja
 * ScrollIndicator (T2.4) a la derecha. No dibuja StatusBar ni pagina
 * (el llamador ya la puso encima). */
void aura_menu_list_draw(int x, int y, const aura_menu_item_v2_t *items,
                          int count, int selected);

/* Par pending()/animating() de ScrollIndicator (T2.4) -- aura_main.c
 * las consulta para saber si hace falta seguir pidiendo cuadros aunque
 * el usuario no toque nada. Ver aura_widgets_scrollbar_pending()/
 * _animating() para el mismo patron ya establecido. */
int aura_menu_list_scroll_indicator_pending(void);
int aura_menu_list_scroll_indicator_animating(void);

#endif /* AURA_MENU_LIST_H */
