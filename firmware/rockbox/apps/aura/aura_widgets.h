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

/* Dibuja el icono <name>-<size>.bmp del tema activo en (x, y) via
 * lcd_bitmap_transparent(). Devuelve su ancho (0 si no se encontro el
 * archivo). Compartido por aura_widgets_draw_list() y aura_statusbar.c
 * -- un unico punto que resuelve ICON_DIR/aura/<tema>/. */
int aura_widgets_draw_icon(const char *name, int size, int x, int y);

/* Fila booleana: label a la izquierda, "Si"/"No" (localizado) a la
 * derecha. SELECT la alterna in situ (L11, PLAN-UX.md) -- el llamador
 * decide que hacer con el nuevo valor, este widget solo dibuja. */
void aura_widgets_draw_bool_row(const char *title, const char *label,
                                 int value);

/* Slider horizontal generico: titulo arriba, pista+relleno al centro,
 * "value_text" (p. ej. "32 / 63") debajo. `fraction` en [0, 256]. */
void aura_widgets_draw_slider(const char *title, int fraction,
                               const char *value_text);

/* Selector de N digitos (0-9) estilo agenda del iPod original (L10):
 * un recuadro redondeado por digito, el que tiene el foco resaltado en
 * acento. `digits` son los valores actuales, `count` cuantos hay,
 * `focus` cual esta activo. */
void aura_widgets_draw_digits(const char *title, const int *digits,
                               int count, int focus);

/* Barra de progreso simple con texto arriba. `fraction` en [0, 256]. */
void aura_widgets_draw_progress(const char *text, int fraction);

#endif /* AURA_WIDGETS_H */
