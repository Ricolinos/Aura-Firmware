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
 * del tema, barra de estado/titulo, y las filas visibles alrededor de
 * `selected` (con scroll si no caben todas). En cualquier modo grafico
 * salvo Ultra la lista ocupa solo el panel izquierdo y el panel
 * derecho muestra el icono del item seleccionado, con un retardo de
 * ~1s (L2/L3, PLAN-UX.md, Fase 15) -- en Ultra ocupa toda la pantalla,
 * sin panel derecho. No llama a lcd_update(): el llamador decide
 * cuando presentar el frame. */
void aura_widgets_draw_list(const char *title, const aura_list_item_t *items,
                             int count, int selected);

/* True si el modo grafico activo usa pantalla dividida (todos menos
 * Ultra). Lo consultan las pantallas con dibujo propio (Coverflow,
 * Now Playing) para decidir su propio layout. */
/* Layout de la pantalla actual. No todas las pantallas del firmware
 * original son divididas ni todas son de ancho completo: la tabla que
 * decide cual es cual vive en aura_screens.c (screen_uses_split_layout)
 * y se comunica aca antes de dibujar, para que exista una sola fuente
 * de verdad en vez de que cada funcion de dibujo elija por su cuenta. */
typedef enum {
    AURA_LIST_SPLIT = 0, /* lista a la izquierda + panel de preview */
    AURA_LIST_FULL,      /* lista a ancho completo, sin panel */
} aura_list_layout_t;

void aura_widgets_set_list_layout(aura_list_layout_t layout);

/* True si la pantalla que se esta por dibujar termina realmente
 * dividida: hace falta que su layout declarado sea SPLIT *y* que el
 * ajuste de Graficos no este en Ninguno. */
int aura_widgets_split_active(void);

/* True mientras el panel derecho tiene un icono nuevo pendiente de
 * mostrar (retardo de 1s, L3) -- aura_main.c lo usa para decidir si
 * redibujar con un timeout corto en vez de bloquear indefinidamente
 * esperando el proximo boton. */
int aura_widgets_panel_pending(void);

/* El proximo draw_list adopta el icono pendiente del panel derecho de
 * inmediato, sin el retardo de ~1s -- para navegacion hacia atras,
 * donde el preview del padre se restaura al instante (D-068). */
void aura_widgets_panel_force_next(void);

/* Dibuja el panel derecho de la pantalla dividida: linea divisoria +
 * icono grande centrado (NULL = panel vacio). Sin retardo propio --
 * aura_widgets_draw_list() ya se encarga del debounce de 1s antes de
 * llamar esto; una pantalla con dibujo propio que quiera el mismo
 * panel sin contenido animado puede llamarlo directo. */
void aura_widgets_draw_right_panel_icon(const char *icon_name);

/* Numero de filas de lista que caben en pantalla con el layout actual;
 * usado por aura_screens.c para el scroll. */
int aura_widgets_visible_rows(void);

/* Dibuja el icono <name>-<size>.bmp del tema activo en (x, y) via
 * lcd_bitmap_transparent(). Devuelve su ancho (0 si no se encontro el
 * archivo). Compartido por aura_widgets_draw_list() y aura_statusbar.c
 * -- un unico punto que resuelve ICON_DIR/aura/<tema>/. */
int aura_widgets_draw_icon(const char *name, int size, int x, int y);

/* Misma firma, pero con la variante de color de contraste del icono --
 * para el contenido que va sobre la barra de seleccion. */
int aura_widgets_draw_icon_selected(const char *name, int size, int x, int y);

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

/* Aviso bloqueante con 2 opciones (Si/No), estilo el "aviso" de
 * PLAN-UX.md S3.8 -- SELECT confirma la opcion resaltada. El llamador
 * lleva su propio estado de cual esta seleccionada (`yes_selected`);
 * este widget solo dibuja. */
void aura_widgets_draw_confirm(const char *title, const char *body,
                                int yes_selected);

#endif /* AURA_WIDGETS_H */
