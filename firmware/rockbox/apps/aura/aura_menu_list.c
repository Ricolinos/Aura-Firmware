#include <string.h>

#include "lcd.h"
#include "string-extra.h"
#include "tick.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_settings.h"
#include "aura_selector.h"
#include "aura_scroll_indicator.h"
#include "aura_menu_list.h"

#define PADDING  AURA_DS_METRICS_LEFT_PANEL_PADDING
#define ROW_H    AURA_DS_METRICS_MENU_LIST_ROW_HEIGHT
#define ICON_X   AURA_DS_METRICS_MENU_LIST_ICON_X_FROM_PANEL_EDGE
#define ICON_SZ  A26_ICON_SIZE_LIST_V2
#define TEXT_GAP AURA_DS_METRICS_MENU_LIST_TEXT_GAP_AFTER_ICON

/* Trunca `label` a `out` (buffer de `outsz`) para que quepa en
 * `max_width` px con la fuente YA activa -- "..." al final, nunca corte
 * abrupto (componentes/left-panel.md: "truncar con '...'", distinto de
 * la estrategia de abreviacion manual que el documento tambien nombra
 * y que es contenido, no logica de render). */
static void truncate_to_fit(const char *label, char *out, size_t outsz, int max_width)
{
    int w, h;
    size_t len;

    lcd_getstringsize((const unsigned char *)label, &w, &h);
    if (w <= max_width)
    {
        strlcpy(out, label, outsz);
        return;
    }

    len = strlen(label);
    while (len > 0)
    {
        len--;
        if (len + 4 > outsz) /* +3 de "..." +1 del terminador */
            continue;
        memcpy(out, label, len);
        strlcpy(out + len, "...", outsz - len);
        lcd_getstringsize((const unsigned char *)out, &w, &h);
        if (w <= max_width)
            return;
    }
    strlcpy(out, "...", outsz);
}

/* Estado de actividad para el fundido de ScrollIndicator (Fade-on-Idle,
 * T1.1) -- se reinicia cuando la VENTANA visible (`first`) cambia, no
 * cuando cambia `selected` a secas: moverse dentro de la ventana ya
 * visible tambien cuenta como actividad segun el documento ("aparece
 * con CUALQUIER movimiento de seleccion... aunque ese movimiento no
 * cause un desplazamiento visual"), asi que en realidad se reinicia
 * con cualquier cambio de `selected`. */
static int s_last_selected = -1;
static long s_activity_since = 0;

/* Switch inline v2 (left-panel.md, "Switch -- valores booleanos"):
 * dimensiones derivadas del indicador del Selector (alto max 12px,
 * selector.md) con proporcion ~1.8:1 -- el documento difiere las
 * medidas exactas de los elementos derechos, TODO(pendiente-doc).
 * Mismos colores sobre fila en reposo y sobre el Selector (pastilla
 * GRIS, no de acento): pista de acento encendida / riel apagada,
 * perilla del fondo del shell -- solo cambia el color de las esquinas
 * redondeadas segun el fondo real debajo. */
/* Switch (referencia visual del dueno del diseno, 2026-08-13): pista en
 * capsula y perilla tambien en CAPSULA -- mas ancha que alta, no un
 * circulo -- que vive DENTRO de la pista con un margen minimo, no
 * desbordandola. Encendido: pista de acento y perilla en un tono muy
 * claro del propio acento. Apagado: pista gris y perilla casi blanca. */
#define SWITCH_W        28
#define SWITCH_H        14
#define SWITCH_MARGIN   2
#define SWITCH_THUMB_W  15

static void draw_switch_v2(int x, int y, int value, int on_selector)
{
    int thumb_h = SWITCH_H - 2 * SWITCH_MARGIN;
    int thumb_x = value ? (x + SWITCH_W - SWITCH_MARGIN - SWITCH_THUMB_W)
                         : (x + SWITCH_MARGIN);
    unsigned bg = on_selector ? a26_color(A26_SELECTION_FILL) : a26_color(A26_SHELL_BG);
    /* Pista apagada: el riel puro es casi invisible en tema claro, se
     * oscurece hacia la tinta primaria hasta un gris con contraste. */
    unsigned off_track = a26_shell_blend(a26_color(A26_SHELL_RAIL),
                                          a26_color(A26_TEXT_PRIMARY), 90);
    unsigned track = value ? aura_accent() : off_track;
    unsigned white = (unsigned)AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT;
    unsigned thumb = value ? a26_shell_blend(track, white, 205)
                            : a26_shell_blend(off_track, white, 245);

    /* Capsulas via fill_rounded_rect con radio = alto/2 (la primitiva
     * las acota ahi): misma forma, y es el camino con antialias ya
     * probado en todo el sistema. */
    a26_shell_fill_rounded_rect(x, y, SWITCH_W, SWITCH_H, SWITCH_H / 2, track, bg);
    a26_shell_fill_rounded_rect(thumb_x, y + SWITCH_MARGIN, SWITCH_THUMB_W, thumb_h,
                                 thumb_h / 2, thumb, track);
}

void aura_menu_list_draw(int x, int y, const aura_menu_item_v2_t *items,
                          int count, int selected)
{
    int visible = AURA_DS_METRICS_MENU_LIST_MAX_VISIBLE_ROWS;
    int panel_w = AURA_DS_METRICS_LEFT_PANEL_WIDTH;
    /* Borde derecho del Selector -- los elementos derechos se anclan a
     * 4px de el, misma regla que el indicador dinamico (selector.md,
     * AURA_DS_METRICS_SELECTOR_INDICATOR_GAP_FROM_EDGE), esten o no
     * sobre la fila seleccionada (el layout de un item no seleccionado
     * es identico, left-panel.md). */
    int right_edge = x + PADDING + (panel_w - 2 * PADDING)
                    - AURA_DS_METRICS_SELECTOR_INDICATOR_GAP_FROM_EDGE;
    int first = 0;
    int i, has_any_icon = 0;
    int font_w, font_h;

    if (count > visible)
    {
        /* Ventana centrada en la seleccion -- mismo criterio de
         * windowing que aura_widgets_draw_list() del sistema viejo. */
        first = selected - visible / 2;
        if (first < 0)
            first = 0;
        if (first > count - visible)
            first = count - visible;
    }

    if (selected != s_last_selected)
    {
        s_last_selected = selected;
        s_activity_since = current_tick;
    }

    /* En una lista mixta (algunas filas con icono, otras sin -- p. ej.
     * Musica mientras la produccion de iconos 1:1 del arbol completo
     * sigue pendiente, selection-summary.md), el texto de TODAS las
     * filas se alinea a la columna post-icono: hermanos desalineados se
     * leen como jerarquia distinta, no como el mismo nivel de menu. */
    /* Ajuste "Mostrar iconos" (encargo 2026-08-13): apagarlo no solo
     * deja de dibujarlos -- tambien recupera su columna, para que el
     * texto no quede flotando con una sangria vacia. */
    for (i = 0; i < count; i++)
        if (items[i].icon_name && aura_settings.show_icons)
            has_any_icon = 1;

    /* D-195 (encargo del dueno, 2026-08-13): filas de MenuList ~140%
     * mas altas para que se lean menos apretadas -- DS_REG_12 en vez de
     * DS_REG_10 (fuente ya cargada por Now Playing, cero fuentes
     * nuevas: MAXUSERFONTS=12 ya esta en su limite exacto). */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    /* Alto real de la fuente activa para centrar verticalmente el texto
     * de cada fila -- ya no un "10" hardcodeado que asumia DS_REG_10
     * (D-195: el tamano de fuente ahora es una eleccion de diseno que
     * puede volver a cambiar, este calculo ya no depende de acordarse
     * de actualizar un numero suelto en otro lado del archivo). */
    lcd_getstringsize((const unsigned char *)"Ay", &font_w, &font_h);

    /* Selector primero (D-081): si algo lo anima en el futuro, dibujarlo
     * antes del texto evita que tape el contenido de otra fila mientras
     * esta en transito. Flecha de seleccion (selector.md): solo si el
     * destino es de pantalla completa Y el item no tiene ya otro
     * elemento derecho. */
    if (selected >= first && selected < first + visible)
    {
        int sel_y = y + (selected - first) * ROW_H;
        const aura_menu_item_v2_t *sel_item = &items[selected];
        aura_selector_indicator_t ind =
            (sel_item->full_screen_target && sel_item->toggle < 0 && !sel_item->checked)
                ? AURA_SELECTOR_INDICATOR_CHEVRON
                : AURA_SELECTOR_INDICATOR_NONE;

        aura_selector_draw(x + PADDING, sel_y, panel_w - 2 * PADDING, ROW_H, ind);
    }

    for (i = first; i < count && i < first + visible; i++)
    {
        int row_y = y + (i - first) * ROW_H;
        int is_selected = (i == selected);
        /* La sangria mueve TODA la fila (icono + texto), no solo el
         * texto -- con solo el texto desplazado, los iconos quedaban
         * alineados entre padre e hijo y la jerarquia no se leia
         * (verificado por pixel: icono de "Cover Flow" en el mismo x
         * que el de "Musica"). Referencia del dueno del diseno,
         * 2026-08-13 (Menu principal configurable). */
        int indent_px = items[i].indent * 12;
        int text_x = (has_any_icon ? (x + ICON_X + ICON_SZ + TEXT_GAP)
                                    : (x + PADDING)) + indent_px;
        int text_right = x + panel_w - PADDING;
        char truncated[64];
        int max_text_w;

        if (items[i].icon_name && aura_settings.show_icons)
        {
            int icon_x = x + ICON_X + indent_px;
            int icon_y = row_y + (ROW_H - ICON_SZ) / 2;

            if (items[i].dimmed)
                aura_widgets_draw_icon_tertiary_dimmed(items[i].icon_name, ICON_SZ,
                                                        icon_x, icon_y, 128);
            else if (is_selected)
                aura_widgets_draw_icon_selected(items[i].icon_name, ICON_SZ, icon_x, icon_y);
            else
                aura_widgets_draw_icon(items[i].icon_name, ICON_SZ, icon_x, icon_y);
        }

        /* Elementos opcionales del lado derecho (left-panel.md): switch
         * para booleanos, checkmark para confirmacion -- mutuamente
         * excluyentes por construccion de los llamadores. */
        if (items[i].toggle >= 0)
        {
            draw_switch_v2(right_edge - SWITCH_W,
                            row_y + (ROW_H - SWITCH_H) / 2,
                            items[i].toggle, is_selected);
            text_right = right_edge - SWITCH_W - TEXT_GAP;
        }
        else if (items[i].checked)
        {
            int check_y = row_y + (ROW_H - ICON_SZ) / 2;
            if (is_selected)
                aura_widgets_draw_icon_selected("check", ICON_SZ,
                                                 right_edge - ICON_SZ, check_y);
            else
                aura_widgets_draw_icon("check", ICON_SZ, right_edge - ICON_SZ, check_y);
            text_right = right_edge - ICON_SZ - TEXT_GAP;
        }

        max_text_w = text_right - text_x;
        truncate_to_fit(items[i].label, truncated, sizeof(truncated), max_text_w);

        /* Contenido del item seleccionado en ACENTO sobre la pastilla
         * gris (correccion del dueno del diseno 2026-08-12, ver
         * aura_selector.c) -- mismo trato que el texto de las listas de
         * contenido, un solo lenguaje de seleccion en toda la app. */
        /* Fila inerte: misma tinta del texto normal, al 50% -- mismo
         * lenguaje que los modos deshabilitados del reproductor. */
        lcd_set_foreground(items[i].dimmed
            ? a26_shell_blend(a26_color(A26_SHELL_BG), a26_color(A26_TEXT_PRIMARY), 128)
            : (is_selected ? aura_accent() : a26_color(A26_TEXT_PRIMARY)));
        lcd_putsxy(text_x, row_y + (ROW_H - font_h) / 2, (const unsigned char *)truncated);
    }

    {
        long idle_elapsed_ms = (current_tick - s_activity_since) * 1000L / HZ;
        aura_scroll_indicator_draw(x + panel_w, y, visible * ROW_H,
                                    first, count, visible, idle_elapsed_ms,
                                    a26_color(A26_SHELL_BG), a26_color(A26_SHELL_RAIL));
    }
}

/* Par pending()/animating() -- mismo criterio que
 * aura_widgets_scrollbar_*()/aura_statusbar_title_*() (D-074/D-091):
 * pending() cubre la ventana ENTERA (cadencia gruesa, asegura cruzar
 * las fronteras de fase a tiempo); animating() solo los dos tramos de
 * fundido real (cadencia fina, evita gastar CPU redibujando a 20fps
 * durante la persistencia de alpha=256 fijo). */
int aura_menu_list_scroll_indicator_pending(void)
{
    long idle_elapsed_ms = (current_tick - s_activity_since) * 1000L / HZ;
    long window_ms = AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS
                    + AURA_DS_METRICS_SCROLL_INDICATOR_IDLE_BEFORE_FADE_MS
                    + AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS;
    return s_last_selected >= 0 && idle_elapsed_ms < window_ms;
}

int aura_menu_list_scroll_indicator_animating(void)
{
    long idle_elapsed_ms = (current_tick - s_activity_since) * 1000L / HZ;

    if (s_last_selected < 0)
        return 0;
    if (idle_elapsed_ms < AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS)
        return 1; /* apareciendo */

    idle_elapsed_ms -= AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS
                      + AURA_DS_METRICS_SCROLL_INDICATOR_IDLE_BEFORE_FADE_MS;
    return idle_elapsed_ms >= 0
        && idle_elapsed_ms < AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS; /* desvaneciendo */
}
