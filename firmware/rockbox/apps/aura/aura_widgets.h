/* Widget de lista vertical con clickwheel: unico componente de
 * renderizado reutilizado por (casi) todas las pantallas de Aura --
 * menu raiz, Ajustes, y las listas de eleccion (Tema/Graficos/EQ/
 * Idioma). Minimalista a proposito: barra de titulo + filas de texto
 * con una barra de seleccion solida en el color de acento. */
#ifndef AURA_WIDGETS_H
#define AURA_WIDGETS_H

typedef struct {
    const char *label;
    const char *icon_name; /* NULL = sin icono */
    int checked;           /* 1 = dibuja una marca (usado en listas de eleccion) */
} aura_list_item_t;

/* Dibuja una pantalla de lista completa: limpia con el color de fondo
 * del tema, barra de titulo, y las filas visibles alrededor de
 * `selected` (con scroll si no caben todas). No llama a lcd_update():
 * el llamador decide cuando presentar el frame. */
void aura_widgets_draw_list(const char *title, const aura_list_item_t *items,
                             int count, int selected);

/* Numero de filas de lista que caben en pantalla con el layout actual;
 * usado por aura_screens.c para el scroll. */
int aura_widgets_visible_rows(void);

#endif /* AURA_WIDGETS_H */
