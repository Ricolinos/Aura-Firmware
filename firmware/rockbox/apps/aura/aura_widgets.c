#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "font.h"
#include "file.h"
#include "rbpaths.h"
#include "recorder/bmp.h"
#include "tick.h"

#include "aura_widgets.h"
#include "aura_theme.h"
#include "aura_settings.h"
#include "aura_tokens.h"
#include "aura_statusbar.h"
#include "aura_lang.h"

/* Layout: barra de estado arriba (Fase 13, PLAN-UX.md), filas de lista
 * debajo. Pantalla dividida izquierda/derecha (Fase 15, L2): en
 * cualquier modo grafico salvo Ultra, la lista ocupa solo el panel
 * izquierdo (168px) y el panel derecho muestra un preview contextual
 * del item seleccionado, con un retardo de ~1s (L3) antes de
 * actualizarse. En Ultra, sigue siendo la lista de ancho completo de
 * siempre -- "sin panel derecho", L4. */
#define LIST_TOP      (AURA_LAYOUT_STATUSBAR_HEIGHT + AURA_SPACING_SM)
#define ROW_HEIGHT    (AURA_TYPE_BODY + 2 * AURA_SPACING_SM)
#define ROW_PAD_X     AURA_SPACING_LG
#define ICON_TEXT_GAP AURA_SPACING_MD
#define PANEL_RIGHT_X (AURA_LAYOUT_PANEL_LEFT_WIDTH + 1)
#define PANEL_RIGHT_W (AURA_SCREEN_WIDTH - PANEL_RIGHT_X)
#define PANEL_RETARDO_TICKS HZ

static const char *theme_dir_name(void)
{
    return (aura_settings.theme == AURA_THEME_DARK) ? "dark" : "light";
}

int aura_widgets_draw_icon(const char *name, int size, int x, int y)
{
    /* Dimensionado para el icono mas grande que se pide hoy
     * (AURA_ICON_SIZE_PREVIEW, panel derecho de la pantalla dividida,
     * Fase 15) -- un buffer de sobra tambien sirve para los tamanos
     * mas chicos que se usan en listas/barra de estado. */
    static unsigned char icon_buf[AURA_ICON_SIZE_PREVIEW * AURA_ICON_SIZE_PREVIEW
                                   * 4 + 64];
    char path[MAX_PATH];
    struct bitmap bm;
    int ret;

    if (!name)
        return 0;

    snprintf(path, sizeof(path), "%s/aura/%s/%s-%d.bmp",
              ICON_DIR, theme_dir_name(), name, size);

    bm.data = (char *)icon_buf;
    ret = read_bmp_file(path, &bm, sizeof(icon_buf), FORMAT_NATIVE, NULL);
    if (ret <= 0)
        return 0;

    /* Los bitmaps se generan sobre TRANSPARENT_COLOR (D-010 en
     * DECISIONS.md), asi que se dibujan igual sobre fondo normal o
     * sobre la barra de seleccion resaltada. */
    lcd_bitmap_transparent((const fb_data *)bm.data, x, y, bm.width, bm.height);
    return bm.width;
}

int aura_widgets_split_active(void)
{
    return aura_settings.graphics_mode != AURA_GFX_ULTRA;
}

static int list_width(void)
{
    return aura_widgets_split_active() ? AURA_LAYOUT_PANEL_LEFT_WIDTH : AURA_SCREEN_WIDTH;
}

int aura_widgets_visible_rows(void)
{
    int rows = (AURA_SCREEN_HEIGHT - LIST_TOP) / ROW_HEIGHT;
    return rows > 0 ? rows : 1;
}

/* Retardo de ~1s antes de que el panel derecho refleje una nueva
 * seleccion (L3, PLAN-UX.md): mientras el usuario sigue moviendose por
 * la lista, el panel sigue mostrando el icono anterior. */
static const char *s_panel_pending_icon;
static const char *s_panel_shown_icon;
static long s_panel_pending_since = 0;

static void draw_right_panel_debounced(const char *icon_name)
{
    if (icon_name != s_panel_pending_icon)
    {
        s_panel_pending_icon = icon_name;
        s_panel_pending_since = current_tick;
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
    lcd_set_foreground(aura_color(AURA_TOK_BORDER));
    lcd_vline(AURA_LAYOUT_PANEL_LEFT_WIDTH, 0, AURA_SCREEN_HEIGHT - 1);

    if (!icon_name)
        return;

    aura_widgets_draw_icon(icon_name, AURA_ICON_SIZE_PREVIEW,
        PANEL_RIGHT_X + (PANEL_RIGHT_W - AURA_ICON_SIZE_PREVIEW) / 2,
        (AURA_SCREEN_HEIGHT - AURA_ICON_SIZE_PREVIEW) / 2);
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

    aura_theme_clear_screen();
    aura_statusbar_draw(0, width, title, 0);

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));

    for (i = first; i < count && i < first + visible; i++)
    {
        int row_y = LIST_TOP + (i - first) * ROW_HEIGHT;
        int is_selected = (i == selected);
        int text_x = ROW_PAD_X;

        if (is_selected)
        {
            lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
            lcd_fillrect(0, row_y, width, ROW_HEIGHT);
        }

        if (items[i].icon_name)
        {
            int w = aura_widgets_draw_icon(items[i].icon_name, AURA_ICON_SIZE_MENU,
                               text_x, row_y + (ROW_HEIGHT - AURA_ICON_SIZE_MENU) / 2);
            if (w > 0)
                text_x += w + ICON_TEXT_GAP;
        }

        lcd_set_foreground(is_selected ? aura_color(AURA_TOK_ACCENT_ON)
                                        : aura_color(AURA_TOK_TEXT_PRIMARY));
        lcd_putsxy(text_x, row_y + AURA_SPACING_SM,
                   (const unsigned char *)items[i].label);

        if (items[i].checked)
        {
            aura_widgets_draw_icon("check", AURA_ICON_SIZE_MENU,
                      width - ROW_PAD_X - AURA_ICON_SIZE_MENU,
                      row_y + (ROW_HEIGHT - AURA_ICON_SIZE_MENU) / 2);
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

    aura_theme_clear_screen();
    aura_statusbar_draw(0, AURA_SCREEN_WIDTH, title, 0);

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));
    lcd_putsxy(ROW_PAD_X, LIST_TOP + AURA_SPACING_SM,
               (const unsigned char *)label);

    lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
    lcd_getstringsize((const unsigned char *)value_text, &w, &h);
    lcd_putsxy(AURA_SCREEN_WIDTH - ROW_PAD_X - w, LIST_TOP + AURA_SPACING_SM,
               (const unsigned char *)value_text);
}

/* -- Slider horizontal (migra el de Brillo, D-014) -------------------- */

void aura_widgets_draw_slider(const char *title, int fraction,
                               const char *value_text)
{
    int bar_x = AURA_SPACING_XXL;
    int bar_w = AURA_SCREEN_WIDTH - 2 * AURA_SPACING_XXL;
    int bar_y = AURA_SCREEN_HEIGHT / 2;
    int fill_w;

    aura_theme_clear_screen();
    aura_statusbar_draw(0, AURA_SCREEN_WIDTH, title, 0);

    if (fraction < 0)   fraction = 0;
    if (fraction > 256) fraction = 256;

    lcd_set_foreground(aura_color(AURA_TOK_BORDER));
    lcd_drawrect(bar_x, bar_y, bar_w, AURA_SPACING_XL);

    fill_w = (bar_w * fraction) / 256;
    lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
    lcd_fillrect(bar_x, bar_y, fill_w, AURA_SPACING_XL);

    if (value_text)
    {
        lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
        lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
        lcd_putsxy(bar_x, bar_y + AURA_SPACING_XL + AURA_SPACING_SM,
                   (const unsigned char *)value_text);
    }
}

/* -- Selector de digitos (L10, pantallas de fecha/hora/codigo) -------- */

#define DIGIT_BOX_W  24
#define DIGIT_BOX_H  32
#define DIGIT_GAP    AURA_SPACING_SM

void aura_widgets_draw_digits(const char *title, const int *digits,
                               int count, int focus)
{
    int total_w = count * DIGIT_BOX_W + (count - 1) * DIGIT_GAP;
    int start_x = (AURA_SCREEN_WIDTH - total_w) / 2;
    int box_y = AURA_SCREEN_HEIGHT / 2 - DIGIT_BOX_H / 2;
    int i;

    aura_theme_clear_screen();
    aura_statusbar_draw(0, AURA_SCREEN_WIDTH, title, 0);

    lcd_setfont(aura_font(AURA_FONT_STYLE_TITLE));

    for (i = 0; i < count; i++)
    {
        int box_x = start_x + i * (DIGIT_BOX_W + DIGIT_GAP);
        char digit_str[2] = { (char)('0' + digits[i]), '\0' };
        int w, h;
        int is_focus = (i == focus);

        lcd_set_foreground(is_focus ? aura_color(AURA_TOK_ACCENT)
                                     : aura_color(AURA_TOK_SURFACE));
        lcd_fillrect(box_x, box_y, DIGIT_BOX_W, DIGIT_BOX_H);

        lcd_getstringsize((const unsigned char *)digit_str, &w, &h);
        lcd_set_foreground(is_focus ? aura_color(AURA_TOK_ACCENT_ON)
                                     : aura_color(AURA_TOK_TEXT_PRIMARY));
        lcd_putsxy(box_x + (DIGIT_BOX_W - w) / 2, box_y + (DIGIT_BOX_H - h) / 2,
                   (const unsigned char *)digit_str);
    }
}

/* -- Progreso ----------------------------------------------------------- */

void aura_widgets_draw_progress(const char *text, int fraction)
{
    int bar_x = AURA_SPACING_XXL;
    int bar_w = AURA_SCREEN_WIDTH - 2 * AURA_SPACING_XXL;
    int bar_y = AURA_SCREEN_HEIGHT / 2;
    int fill_w;

    if (fraction < 0)   fraction = 0;
    if (fraction > 256) fraction = 256;

    if (text)
    {
        int w, h;
        lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
        lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)text, &w, &h);
        lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2, bar_y - AURA_SPACING_XL - h,
                   (const unsigned char *)text);
    }

    lcd_set_foreground(aura_color(AURA_TOK_BORDER));
    lcd_drawrect(bar_x, bar_y, bar_w, AURA_SPACING_SM);

    fill_w = (bar_w * fraction) / 256;
    lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
    lcd_fillrect(bar_x, bar_y, fill_w, AURA_SPACING_SM);
}
