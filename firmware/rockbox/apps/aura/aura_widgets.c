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
#define ROW_PAD_X     A26_SPACING_LG
#define ICON_TEXT_GAP A26_SPACING_MD
#define PANEL_RIGHT_X (A26_LAYOUT_PANEL_LEFT_WIDTH + 1)
#define PANEL_RIGHT_W (A26_SCREEN_WIDTH - PANEL_RIGHT_X)
#define PANEL_RETARDO_TICKS HZ

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

    a26_shell_clear_screen();
    aura_statusbar_draw(0, width, title, 0);

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));

    for (i = first; i < count && i < first + visible; i++)
    {
        int row_y = LIST_TOP + (i - first) * ROW_HEIGHT;
        int is_selected = (i == selected);
        int text_x = ROW_PAD_X;

        /* Fila activa: barra gris neutra con el contenido (icono, texto,
         * checkmark) en el color de contraste de la marca. */
        if (is_selected)
        {
            lcd_set_foreground(a26_color(A26_SELECTION_FILL));
            lcd_fillrect(0, row_y, width, ROW_HEIGHT);
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

    lcd_set_foreground(a26_color(!yes_selected ? A26_SELECTION_FILL : A26_SHELL_RAIL));
    lcd_fillrect(no_x, btn_y, btn_w, 32);
    lcd_set_foreground(a26_color(yes_selected ? A26_SELECTION_FILL : A26_SHELL_RAIL));
    lcd_fillrect(yes_x, btn_y, btn_w, 32);

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

void aura_widgets_draw_progress(const char *text, int fraction)
{
    int bar_x = A26_SPACING_XXL;
    int bar_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    int bar_y = A26_SCREEN_HEIGHT / 2;
    int fill_w;

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

    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_drawrect(bar_x, bar_y, bar_w, A26_SPACING_SM);

    fill_w = (bar_w * fraction) / 256;
    lcd_set_foreground(a26_color(A26_ACCENT));
    lcd_fillrect(bar_x, bar_y, fill_w, A26_SPACING_SM);
}
