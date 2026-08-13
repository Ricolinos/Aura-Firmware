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
    int toggle;            /* -1 = fila normal; 0/1 = dibuja un switch en ese
                             * estado en vez de checkmark (SELECT lo invierte
                             * in situ -- doc de comportamiento SS1: `[OPCION]`
                             * booleano "sigue la regla de profundidad por
                             * defecto, no tiene mecanica propia", es decir
                             * vive en la fila misma, nunca en pantalla propia) */
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

/* True mientras la barra de deslizamiento (doc SS5.3) sigue en su
 * ventana de aparecer/persistir/desvanecer -- aura_main.c la consulta
 * con la misma cadencia gruesa (HZ/4) que aura_widgets_panel_pending()
 * para no perderse el momento en que termina la persistencia y arranca
 * el desvanecido. */
int aura_widgets_scrollbar_pending(void);

/* True SOLO durante los tramos donde el alpha de la barra realmente
 * cambia cuadro a cuadro (entrada o salida) -- aura_main.c pide la
 * cadencia fina de 20fps unicamente aca, no durante toda la ventana de
 * pending() (D-074: pedirla durante la persistencia, que no cambia
 * visualmente, sobrecargaba el bucle principal sin necesidad). */
int aura_widgets_scrollbar_animating(void);

/* True mientras la pastilla de seleccion todavia esta en pleno resorte
 * (SS9.2, Fase 28) -- aura_main.c pide la cadencia fina de 20fps
 * mientras dure, a diferencia de la barra de deslizamiento, el resorte
 * entero (no solo sus extremos) es la parte visualmente relevante. */
int aura_widgets_pill_animating(void);

/* Dibuja el icono <name>-<size>.bmp del tema activo en (x, y) via
 * lcd_bitmap_transparent(). Devuelve su ancho (0 si no se encontro el
 * archivo). Compartido por aura_widgets_draw_list() y aura_statusbar.c
 * -- un unico punto que resuelve ICON_DIR/aura/<tema>/. */
int aura_widgets_draw_icon(const char *name, int size, int x, int y);

/* Misma firma, pero con la variante de color de contraste del icono --
 * para el contenido que va sobre la barra de seleccion. */
int aura_widgets_draw_icon_selected(const char *name, int size, int x, int y);

/* Misma firma, con opacidad simulada arbitraria (0-256) mezclando cada
 * pixel no transparente hacia el fondo del shell -- para estados
 * "visible pero no seleccionable" que ninguna variante de color fija
 * cubre (p. ej. el icono de Letras sin .lrc en Ahora suena, T3.1(b)). */
int aura_widgets_draw_icon_dimmed(const char *name, int size, int x, int y, int alpha_256);
int aura_widgets_draw_icon_tertiary_dimmed(const char *name, int size, int x, int y, int alpha_256);
int aura_widgets_draw_icon_ink(const char *name, int size, int x, int y,
                                unsigned ink, int alpha_256);

/* Misma firma, variante TEXT_TERTIARY -- iconos en reposo/inactivos
 * fuera de una lista (p. ej. los 4 modos no activos de Ahora suena,
 * AUDITORIA-01 A-16). */
int aura_widgets_draw_icon_tertiary(const char *name, int size, int x, int y);

/* Misma firma, variante SHELL_RAIL -- controles de cantidad vacios
 * (p. ej. estrellas sin llenar de Ahora suena, AUDITORIA-01 A-16). */
int aura_widgets_draw_icon_rail(const char *name, int size, int x, int y);

/* Misma firma, variante blanco constante (PLAN.md T2.2/G5) -- contenido
 * (iconos, indicador del Selector) sobre la pastilla de acento del
 * sistema nuevo, que a diferencia de SELECTION_FILL del sistema viejo
 * no es gris. */
int aura_widgets_draw_icon_variant_selector(const char *name, int size, int x, int y);

/* Fila booleana: label a la izquierda, "Si"/"No" (localizado) a la
 * derecha. SELECT la alterna in situ (L11, PLAN-UX.md) -- el llamador
 * decide que hacer con el nuevo valor, este widget solo dibuja. */
void aura_widgets_draw_bool_row(const char *title, const char *label,
                                 int value);

/* Switch iOS-like: pista capsula + circulo que se desliza. `bg` es el
 * color que rodea la pista -- SHELL_BG en una fila normal, o
 * SELECTION_FILL si la fila esta bajo la pastilla de seleccion (D-010:
 * el redondeo recorta contra lo que hay detras, igual que el resto de
 * los rectangulos redondeados del sistema). Dibuja, no decide estado --
 * mismo contrato que el resto de aura_widgets. */
void aura_widgets_draw_toggle(int x, int y, int value, unsigned bg);

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

/* Capsula flotante de espera (SS5.2): NO limpia pantalla, se dibuja
 * encima de lo que el llamador ya pinto en este cuadro. `text` opcional
 * (NULL = solo la capsula vacia). El patron de espera del sistema --
 * ninguna otra pantalla debe tapar su contenido con un mensaje centrado
 * mientras algo carga (Principio 3). */
void aura_widgets_draw_wait_capsule(const char *text);

/* Aviso bloqueante con 2 opciones (Si/No), estilo el "aviso" de
 * PLAN-UX.md S3.8 -- SELECT confirma la opcion resaltada. El llamador
 * lleva su propio estado de cual esta seleccionada (`yes_selected`);
 * este widget solo dibuja. */
void aura_widgets_draw_confirm(const char *title, const char *body,
                                int yes_selected);

#endif /* AURA_WIDGETS_H */
