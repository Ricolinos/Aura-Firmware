#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "font.h"
#include "file.h"
#include "rbpaths.h"
#include "recorder/bmp.h"
#include "tick.h"

#include "aura_widgets.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_lang.h"

/* Layout: barra de estado arriba (Fase 13, PLAN-UX.md), filas de lista
 * debajo. Pantalla dividida izquierda/derecha (Fase 15, L2): en
 * cualquier modo grafico salvo Ultra, la lista ocupa solo el panel
 * izquierdo (168px) y el panel derecho muestra un preview contextual
 * del item seleccionado, con un retardo de ~1s (L3) antes de
 * actualizarse. En Ultra, sigue siendo la lista de ancho completo de
 * siempre -- "sin panel derecho", L4. */
#define LIST_TOP      (A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_SM)
#define ROW_HEIGHT    (A26_TYPE_BODY + 2 * A26_SPACING_SM)
#define ROW_PAD_X     A26_LAYOUT_LIST_INSET
#define ICON_TEXT_GAP A26_SPACING_MD
#define PANEL_RIGHT_X (A26_LAYOUT_PANEL_LEFT_WIDTH + 1)
#define PANEL_RIGHT_W (A26_SCREEN_WIDTH - PANEL_RIGHT_X)
#define PANEL_RETARDO_TICKS HZ

/* Pastilla de seleccion de lista (doc SS5.1): rectangulo que no toca los
 * bordes -- margen 8px desde el borde de la columna (mas angosto que el
 * inset de 16px del texto, asi la pastilla queda mas ancha que el
 * contenido pero sin llegar al borde) y 2px arriba/abajo. */
#define PILL_MARGIN_X A26_SPACING_MD
#define PILL_MARGIN_Y 2
#define PILL_RADIUS   A26_LAYOUT_CORNER_RADIUS_PILL

static const char *theme_dir_name(void)
{
    return (aura_settings.theme == AURA_THEME_DARK) ? "dark" : "light";
}

/* `suffix` elige la variante de color del bitmap: "" es el color de
 * texto normal y "-on" el color de contraste, para el contenido que va
 * sobre la barra de seleccion. Las dos variantes las genera
 * design-system/generate.py; el color viene horneado en el bitmap
 * porque lcd_bitmap_transparent() no sabe recolorear (D-010). */
static int draw_icon_variant(const char *name, int size, int x, int y,
                              const char *suffix)
{
    /* Dimensionado para el icono mas grande que se pide hoy
     * (A26_ICON_SIZE_PREVIEW, panel derecho de la pantalla dividida,
     * Fase 15) -- un buffer de sobra tambien sirve para los tamanos
     * mas chicos que se usan en listas/barra de estado. */
    static unsigned char icon_buf[A26_ICON_SIZE_PREVIEW * A26_ICON_SIZE_PREVIEW
                                   * 4 + 64];
    char path[MAX_PATH];
    struct bitmap bm;
    int ret;

    if (!name)
        return 0;

    snprintf(path, sizeof(path), "%s/aura/%s/%s-%d%s.bmp",
              ICON_DIR, theme_dir_name(), name, size, suffix);

    bm.data = (char *)icon_buf;
    ret = read_bmp_file(path, &bm, sizeof(icon_buf), FORMAT_NATIVE, NULL);
    if (ret <= 0)
        return 0;

    /* Los bitmaps se generan sobre TRANSPARENT_COLOR (D-010 en
     * DECISIONS.md), asi que se dibujan igual sobre fondo normal o
     * sobre la barra de seleccion. */
    lcd_bitmap_transparent((const fb_data *)bm.data, x, y, bm.width, bm.height);
    return bm.width;
}

int aura_widgets_draw_icon(const char *name, int size, int x, int y)
{
    return draw_icon_variant(name, size, x, y, "");
}

int aura_widgets_draw_icon_selected(const char *name, int size, int x, int y)
{
    return draw_icon_variant(name, size, x, y, "-on");
}

/* Layout declarado por la pantalla actual (lo fija aura_screens_draw
 * desde su tabla). Por defecto SPLIT: es el layout de la mayoria de los
 * menus del firmware original. */
static aura_list_layout_t s_list_layout = AURA_LIST_SPLIT;

void aura_widgets_set_list_layout(aura_list_layout_t layout)
{
    s_list_layout = layout;
}

int aura_widgets_split_active(void)
{
    return s_list_layout == AURA_LIST_SPLIT
        && aura_settings.graphics_mode != AURA_GFX_NONE;
}

static int list_width(void)
{
    return aura_widgets_split_active() ? A26_LAYOUT_PANEL_LEFT_WIDTH : A26_SCREEN_WIDTH;
}

int aura_widgets_visible_rows(void)
{
    int rows = (A26_SCREEN_HEIGHT - LIST_TOP) / ROW_HEIGHT;
    return rows > 0 ? rows : 1;
}

/* Retardo de ~1s antes de que el panel derecho refleje una nueva
 * seleccion (L3, PLAN-UX.md): mientras el usuario sigue moviendose por
 * la lista, el panel sigue mostrando el icono anterior. */
static const char *s_panel_pending_icon;
static const char *s_panel_shown_icon;
static long s_panel_pending_since = 0;
static int s_panel_force_next = 0;

void aura_widgets_panel_force_next(void)
{
    s_panel_force_next = 1;
}

static void draw_right_panel_debounced(const char *icon_name)
{
    if (icon_name != s_panel_pending_icon)
    {
        s_panel_pending_icon = icon_name;
        s_panel_pending_since = current_tick;
    }

    /* Al volver atras el preview del menu padre se restaura al
     * instante, sin esperar el retardo -- es contenido que el usuario
     * ya habia visto, no una seleccion nueva (observado cuadro a cuadro
     * en el firmware original, D-068). El retardo de ~1s aplica solo a
     * selecciones nuevas mientras se navega. */
    if (s_panel_force_next)
    {
        s_panel_force_next = 0;
        s_panel_shown_icon = s_panel_pending_icon;
    }

    if (TIME_AFTER(current_tick, s_panel_pending_since + PANEL_RETARDO_TICKS))
        s_panel_shown_icon = s_panel_pending_icon;

    aura_widgets_draw_right_panel_icon(s_panel_shown_icon);
}

int aura_widgets_panel_pending(void)
{
    return s_panel_shown_icon != s_panel_pending_icon;
}

void aura_widgets_draw_right_panel_icon(const char *icon_name)
{
    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_vline(A26_LAYOUT_PANEL_LEFT_WIDTH, 0, A26_SCREEN_HEIGHT - 1);

    if (!icon_name)
        return;

    aura_widgets_draw_icon(icon_name, A26_ICON_SIZE_PREVIEW,
        PANEL_RIGHT_X + (PANEL_RIGHT_W - A26_ICON_SIZE_PREVIEW) / 2,
        (A26_SCREEN_HEIGHT - A26_ICON_SIZE_PREVIEW) / 2);
}

/* Barra de deslizamiento (doc SS5.3): aparece/persiste/desvanece segun
 * actividad de scroll, nunca fija en pantalla. Sin compositor alfa en
 * este LCD, el fundido se simula interpolando el color del trazo entre
 * el fondo y SHELL_RAIL con a26_shell_blend() -- visualmente equivalente
 * a variar la opacidad, calculado una vez por cuadro. Estado global
 * (una sola lista visible a la vez, mismo patron que el debounce del
 * panel derecho de arriba); se reinicia si cambia la lista mostrada. */
#define SCROLLBAR_W          3
#define SCROLLBAR_INSET      2
#define SCROLLBAR_MIN_H      24
#define SCROLLBAR_RADIUS     1
#define SCROLLBAR_FADE_IN_TICKS  (HZ * 150 / 1000)
#define SCROLLBAR_HOLD_TICKS     (HZ * 800 / 1000)
#define SCROLLBAR_FADE_OUT_TICKS (HZ * 330 / 1000)

static const aura_list_item_t *s_scrollbar_items;
static int s_scrollbar_selected = -1;
static long s_scrollbar_activity_since;

static int scrollbar_alpha_256(void)
{
    long elapsed = current_tick - s_scrollbar_activity_since;

    if (elapsed < 0)
        elapsed = 0;

    if (elapsed < SCROLLBAR_FADE_IN_TICKS)
        return (int)(elapsed * 256 / SCROLLBAR_FADE_IN_TICKS);

    if (elapsed < SCROLLBAR_FADE_IN_TICKS + SCROLLBAR_HOLD_TICKS)
        return 256;

    elapsed -= SCROLLBAR_FADE_IN_TICKS + SCROLLBAR_HOLD_TICKS;
    if (elapsed < SCROLLBAR_FADE_OUT_TICKS)
        return 256 - (int)(elapsed * 256 / SCROLLBAR_FADE_OUT_TICKS);

    return 0;
}

int aura_widgets_scrollbar_pending(void)
{
    /* No alcanza con "alpha > 0": justo al reiniciar la actividad
     * (elapsed=0) el fundido de entrada arranca en alpha=0 -- si
     * pending() mirara solo el alpha actual, el bucle principal nunca
     * pediria un timeout corto para el SIGUIENTE cuadro y la barra se
     * quedaria congelada invisible hasta el proximo boton real. Lo que
     * importa es si falta seguir animando: toda la ventana
     * entrada+persistencia+salida, no el valor de un instante. Cadencia
     * gruesa (HZ/4, aura_main.c) -- ver aura_widgets_scrollbar_animating()
     * para cuando hace falta la cadencia fina de 20fps. */
    long elapsed = current_tick - s_scrollbar_activity_since;
    if (elapsed < 0)
        elapsed = 0;
    return elapsed < SCROLLBAR_FADE_IN_TICKS + SCROLLBAR_HOLD_TICKS + SCROLLBAR_FADE_OUT_TICKS;
}

int aura_widgets_scrollbar_animating(void)
{
    /* Distinto de pending(): true SOLO durante los dos tramos donde el
     * alpha realmente cambia cuadro a cuadro (entrada y salida) -- NO
     * durante la persistencia (alpha=256 fijo, nada que redibujar). Bug
     * real encontrado tras un crash del simulador interactivo
     * (queue_post ovf, D-074): la version anterior pedia la cadencia de
     * 20fps (HZ/20, 5 ticks) durante TODA la ventana de ~1.3s, incluida
     * la persistencia -- con el usuario hojeando listas largas rapido
     * (Ajustes navegado sin pausas), eso mantenia el bucle principal
     * redibujando a 20fps de forma casi continua, compitiendo por CPU
     * con el bombeo de eventos de teclado de SDL hasta desbordar la cola
     * de botones. Ahora pending() (cadencia HZ/4, la misma que ya prueba
     * segura el debounce del panel derecho) cubre el tramo de
     * persistencia solo para notar cuando termina, y esta funcion pide
     * la cadencia fina unicamente en los ~480ms reales de fundido. */
    long elapsed = current_tick - s_scrollbar_activity_since;
    if (elapsed < 0)
        elapsed = 0;

    if (elapsed < SCROLLBAR_FADE_IN_TICKS)
        return 1;

    elapsed -= SCROLLBAR_FADE_IN_TICKS + SCROLLBAR_HOLD_TICKS;
    return elapsed >= 0 && elapsed < SCROLLBAR_FADE_OUT_TICKS;
}

static void draw_scrollbar(int width, int visible, int count, int first)
{
    int alpha;
    int track_top = LIST_TOP;
    int track_h = A26_SCREEN_HEIGHT - LIST_TOP;
    int thumb_h, thumb_y, x;
    unsigned color;

    if (count <= visible)
        return;

    alpha = scrollbar_alpha_256();
    if (alpha <= 0)
        return;

    thumb_h = track_h * visible / count;
    if (thumb_h < SCROLLBAR_MIN_H)
        thumb_h = SCROLLBAR_MIN_H;
    if (thumb_h > track_h)
        thumb_h = track_h;

    thumb_y = (count > visible)
        ? track_top + (track_h - thumb_h) * first / (count - visible)
        : track_top;

    x = width - SCROLLBAR_INSET - SCROLLBAR_W;
    color = a26_shell_blend(a26_color(A26_SHELL_BG), a26_color(A26_SHELL_RAIL), alpha);

    a26_shell_fill_rounded_rect(x, thumb_y, SCROLLBAR_W, thumb_h,
                                 SCROLLBAR_RADIUS, color, a26_color(A26_SHELL_BG));
}

void aura_widgets_draw_list(const char *title, const aura_list_item_t *items,
                             int count, int selected)
{
    int split = aura_widgets_split_active();
    int width = list_width();
    int visible = aura_widgets_visible_rows();
    int first = 0;
    int i;

    if (count > visible)
    {
        first = selected - visible / 2;
        if (first < 0)
            first = 0;
        if (first > count - visible)
            first = count - visible;
    }

    if (items != s_scrollbar_items || selected != s_scrollbar_selected)
    {
        s_scrollbar_items = items;
        s_scrollbar_selected = selected;
        s_scrollbar_activity_since = current_tick;
    }

    a26_shell_clear_screen();
    aura_statusbar_draw(0, width, title, 0);

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));

    for (i = first; i < count && i < first + visible; i++)
    {
        int row_y = LIST_TOP + (i - first) * ROW_HEIGHT;
        int is_selected = (i == selected);
        int text_x = ROW_PAD_X;

        /* Fila activa: pastilla redondeada (doc SS5.1, no la barra de
         * ancho completo de 2007) con el contenido (icono, texto,
         * checkmark) en el color de contraste de la marca. */
        if (is_selected)
        {
            a26_shell_fill_rounded_rect(PILL_MARGIN_X, row_y + PILL_MARGIN_Y,
                                         width - 2 * PILL_MARGIN_X,
                                         ROW_HEIGHT - 2 * PILL_MARGIN_Y,
                                         PILL_RADIUS,
                                         a26_color(A26_SELECTION_FILL),
                                         a26_color(A26_SHELL_BG));
        }

        if (items[i].icon_name)
        {
            int icon_y = row_y + (ROW_HEIGHT - A26_ICON_SIZE_MENU) / 2;
            int w = is_selected
                ? aura_widgets_draw_icon_selected(items[i].icon_name,
                                                   A26_ICON_SIZE_MENU, text_x, icon_y)
                : aura_widgets_draw_icon(items[i].icon_name,
                                          A26_ICON_SIZE_MENU, text_x, icon_y);
            if (w > 0)
                text_x += w + ICON_TEXT_GAP;
        }

        lcd_set_foreground(is_selected ? a26_color(A26_ACCENT)
                                        : a26_color(A26_TEXT_PRIMARY));
        lcd_putsxy(text_x, row_y + A26_SPACING_SM,
                   (const unsigned char *)items[i].label);

        if (items[i].checked)
        {
            int check_x = width - ROW_PAD_X - A26_ICON_SIZE_MENU;
            int check_y = row_y + (ROW_HEIGHT - A26_ICON_SIZE_MENU) / 2;
            if (is_selected)
                aura_widgets_draw_icon_selected("check", A26_ICON_SIZE_MENU, check_x, check_y);
            else
                aura_widgets_draw_icon("check", A26_ICON_SIZE_MENU, check_x, check_y);
        }
    }

    if (split)
        draw_right_panel_debounced(count > 0 ? items[selected].icon_name : NULL);

    draw_scrollbar(width, visible, count, first);
}

/* -- Fila booleana (L11) --------------------------------------------- */

void aura_widgets_draw_bool_row(const char *title, const char *label,
                                 int value)
{
    const char *value_text = aura_str(value ? AURA_STR_YES : AURA_STR_NO);
    int w, h;

    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, title, 0);

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_putsxy(ROW_PAD_X, LIST_TOP + A26_SPACING_SM,
               (const unsigned char *)label);

    lcd_set_foreground(a26_color(A26_ACCENT));
    lcd_getstringsize((const unsigned char *)value_text, &w, &h);
    lcd_putsxy(A26_SCREEN_WIDTH - ROW_PAD_X - w, LIST_TOP + A26_SPACING_SM,
               (const unsigned char *)value_text);
}

/* -- Slider horizontal (migra el de Brillo, D-014) -------------------- */

void aura_widgets_draw_slider(const char *title, int fraction,
                               const char *value_text)
{
    int bar_x = A26_SPACING_XXL;
    int bar_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    int bar_y = A26_SCREEN_HEIGHT / 2;
    int fill_w;

    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, title, 0);

    if (fraction < 0)   fraction = 0;
    if (fraction > 256) fraction = 256;

    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_drawrect(bar_x, bar_y, bar_w, A26_SPACING_XL);

    fill_w = (bar_w * fraction) / 256;
    lcd_set_foreground(a26_color(A26_ACCENT));
    lcd_fillrect(bar_x, bar_y, fill_w, A26_SPACING_XL);

    if (value_text)
    {
        lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_putsxy(bar_x, bar_y + A26_SPACING_XL + A26_SPACING_SM,
                   (const unsigned char *)value_text);
    }
}

/* -- Selector de digitos (L10, pantallas de fecha/hora/codigo) -------- */

#define DIGIT_BOX_W  24
#define DIGIT_BOX_H  32
#define DIGIT_GAP    A26_SPACING_SM

void aura_widgets_draw_digits(const char *title, const int *digits,
                               int count, int focus)
{
    int total_w = count * DIGIT_BOX_W + (count - 1) * DIGIT_GAP;
    int start_x = (A26_SCREEN_WIDTH - total_w) / 2;
    int box_y = A26_SCREEN_HEIGHT / 2 - DIGIT_BOX_H / 2;
    int i;

    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, title, 0);

    lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));

    for (i = 0; i < count; i++)
    {
        int box_x = start_x + i * (DIGIT_BOX_W + DIGIT_GAP);
        char digit_str[2] = { (char)('0' + digits[i]), '\0' };
        int w, h;
        int is_focus = (i == focus);

        lcd_set_foreground(is_focus ? a26_color(A26_SELECTION_FILL)
                                     : a26_color(A26_SHELL_RAIL));
        lcd_fillrect(box_x, box_y, DIGIT_BOX_W, DIGIT_BOX_H);

        lcd_getstringsize((const unsigned char *)digit_str, &w, &h);
        lcd_set_foreground(is_focus ? a26_color(A26_ACCENT)
                                     : a26_color(A26_TEXT_PRIMARY));
        lcd_putsxy(box_x + (DIGIT_BOX_W - w) / 2, box_y + (DIGIT_BOX_H - h) / 2,
                   (const unsigned char *)digit_str);
    }
}

/* -- Progreso ----------------------------------------------------------- */

/* -- Aviso bloqueante con 2 opciones (S3.8) ---------------------------- */

#define CONFIRM_MAX_LINES 4

/* Word-wrap simple y propio: no hace falta la generalidad de
 * apps/gui/splash.c (tabs, multi-pantalla, memoria de tamano maximo)
 * para un cuerpo corto de 2-3 lineas fijas. */
static int wrap_text(const char *text, int max_width, const char **lines, int *lens, int max_lines)
{
    int n = 0;
    const char *p = text;

    while (*p && n < max_lines)
    {
        const char *line_start = p;
        const char *last_space = NULL;
        const char *cursor = p;
        char buf[128];

        while (*cursor)
        {
            int len = (int)(cursor - line_start) + 1;
            int w, h;
            if (len >= (int)sizeof(buf))
                break;
            memcpy(buf, line_start, len);
            buf[len] = '\0';
            lcd_getstringsize((const unsigned char *)buf, &w, &h);
            if (w > max_width && last_space)
                break;
            if (*cursor == ' ')
                last_space = cursor;
            cursor++;
        }

        if (*cursor == '\0')
        {
            lines[n] = line_start;
            lens[n] = (int)(cursor - line_start);
            n++;
            break;
        }

        if (last_space)
        {
            lines[n] = line_start;
            lens[n] = (int)(last_space - line_start);
            p = last_space + 1;
        }
        else
        {
            lines[n] = line_start;
            lens[n] = (int)(cursor - line_start);
            p = cursor;
        }
        n++;
    }
    return n;
}

void aura_widgets_draw_confirm(const char *title, const char *body, int yes_selected)
{
    const char *lines[CONFIRM_MAX_LINES];
    int lens[CONFIRM_MAX_LINES];
    int box_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    int box_x = A26_SPACING_XXL;
    int text_y, i, n;
    int btn_y, btn_w, yes_x, no_x;
    const char *yes_label = aura_str(AURA_STR_YES);
    const char *no_label = aura_str(AURA_STR_NO);

    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, title, 0);

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));

    n = wrap_text(body, box_w, lines, lens, CONFIRM_MAX_LINES);
    text_y = A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_XXL;
    for (i = 0; i < n; i++)
    {
        /* lcd_putsxy no acepta longitud: se corta a un buffer chico con
         * terminador nulo antes de dibujar cada linea envuelta. */
        char buf[128];
        int len = lens[i];
        if (len >= (int)sizeof(buf))
            len = sizeof(buf) - 1;
        memcpy(buf, lines[i], len);
        buf[len] = '\0';
        lcd_putsxy(box_x, text_y, (const unsigned char *)buf);
        text_y += A26_TYPE_BODY + A26_SPACING_SM;
    }

    btn_w = 100;
    btn_y = A26_SCREEN_HEIGHT - A26_SPACING_XXL - 32;
    no_x = box_x;
    yes_x = box_x + box_w - btn_w;

    /* Chips redondeados, mismo radio de pastilla que la lista (SS5.4:
     * nunca un radio suelto por componente) -- antes eran rectangulos a
     * bordes vivos, el unico control del sistema que no seguia la
     * jerarquia concentrica de radios. */
    a26_shell_fill_rounded_rect(no_x, btn_y, btn_w, 32, PILL_RADIUS,
                                 a26_color(!yes_selected ? A26_SELECTION_FILL : A26_SHELL_RAIL),
                                 a26_color(A26_SHELL_BG));
    a26_shell_fill_rounded_rect(yes_x, btn_y, btn_w, 32, PILL_RADIUS,
                                 a26_color(yes_selected ? A26_SELECTION_FILL : A26_SHELL_RAIL),
                                 a26_color(A26_SHELL_BG));

    {
        int w, h;
        lcd_set_foreground(a26_color(!yes_selected ? A26_ACCENT : A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)no_label, &w, &h);
        lcd_putsxy(no_x + (btn_w - w) / 2, btn_y + (32 - h) / 2, (const unsigned char *)no_label);

        lcd_set_foreground(a26_color(yes_selected ? A26_ACCENT : A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)yes_label, &w, &h);
        lcd_putsxy(yes_x + (btn_w - w) / 2, btn_y + (32 - h) / 2, (const unsigned char *)yes_label);
    }
}

/* Pastilla de progreso canonica (doc SS5.2): 4px de alto, extremos
 * redondeados, carril PROGRESS_TRACK + relleno PROGRESS_FILL -- NUNCA
 * SHELL_RAIL/ACCENT (esos son para separadores y estado activo, no
 * progreso; el doc reserva un par de tokens dedicado). El redondeo de
 * los extremos usa la misma primitiva de corte por distancia que la
 * pastilla de seleccion en vez de la tabla de retranqueos manual del
 * documento -- mismo resultado visual (un radio real, no aproximado a
 * mano), una sola implementacion de "rectangulo con puntas redondas" en
 * todo el sistema. */
void aura_widgets_draw_progress(const char *text, int fraction)
{
    int bar_x = A26_SPACING_XXL;
    int bar_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    int bar_y = A26_SCREEN_HEIGHT / 2;
    int bar_h = A26_SPACING_SM;
    int fill_w;
    unsigned bg = a26_color(A26_SHELL_BG);

    if (fraction < 0)   fraction = 0;
    if (fraction > 256) fraction = 256;

    if (text)
    {
        int w, h;
        lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)text, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, bar_y - A26_SPACING_XL - h,
                   (const unsigned char *)text);
    }

    a26_shell_fill_rounded_rect(bar_x, bar_y, bar_w, bar_h, bar_h / 2,
                                 a26_color(A26_PROGRESS_TRACK), bg);

    fill_w = (bar_w * fraction) / 256;
    if (fill_w > 0)
        a26_shell_fill_rounded_rect(bar_x, bar_y, fill_w, bar_h, bar_h / 2,
                                     a26_color(A26_PROGRESS_FILL), bg);
}

/* Capsula flotante de espera (doc SS5.2, Principio 3: "la carga
 * convive, no interrumpe"): NO limpia pantalla ni ocupa una pagina
 * completa -- se dibuja encima de lo que ya esta a la vista, como
 * ultimo paso del dibujo de la pantalla que la necesita. Geometria fija
 * del documento: x=40, ancho pantalla-80, y=alto-14, capsula de 12px con
 * radio 6 (A26_LAYOUT_CORNER_RADIUS_CAPSULE), fondo SHELL_BG y borde
 * SHELL_RAIL de 1px -- unica superficie de esperas del sistema; unica
 * pagina completa que le queda al aparato es para sus propios estados
 * (apagado, USB, base de datos vacia), no para un progreso. */
void aura_widgets_draw_wait_capsule(const char *text)
{
    int cap_h = 12;
    int cap_y = A26_SCREEN_HEIGHT - 14;
    int cap_x = 40;
    int cap_w = A26_SCREEN_WIDTH - 80;
    int w, h;

    a26_shell_outline_rounded_rect(cap_x, cap_y, cap_w, cap_h,
                                    A26_LAYOUT_CORNER_RADIUS_CAPSULE,
                                    a26_color(A26_SHELL_BG),
                                    a26_color(A26_SHELL_RAIL),
                                    a26_color(A26_SHELL_BG));

    if (!text)
        return;

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)text, &w, &h);
    lcd_putsxy(cap_x + (cap_w - w) / 2, cap_y + (cap_h - h) / 2,
               (const unsigned char *)text);
}
