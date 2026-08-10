#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "font.h"
#include "file.h"
#include "rbpaths.h"
#include "recorder/bmp.h"

#include "aura_widgets.h"
#include "aura_theme.h"
#include "aura_settings.h"
#include "aura_tokens.h"

/* Layout: barra de titulo arriba, filas de lista debajo. */
#define TITLE_TOP     AURA_SPACING_LG
#define TITLE_HEIGHT  (AURA_TYPE_TITLE + AURA_SPACING_MD)
#define LIST_TOP      (TITLE_TOP + TITLE_HEIGHT + AURA_SPACING_SM)
#define ROW_HEIGHT    (AURA_TYPE_BODY + 2 * AURA_SPACING_SM)
#define ROW_PAD_X     AURA_SPACING_LG
#define ICON_TEXT_GAP AURA_SPACING_MD

static const char *theme_dir_name(void)
{
    return (aura_settings.theme == AURA_THEME_DARK) ? "dark" : "light";
}

static int draw_icon(const char *name, int size, int x, int y)
{
    static unsigned char icon_buf[AURA_ICON_SIZE_MENU * AURA_ICON_SIZE_MENU
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

int aura_widgets_visible_rows(void)
{
    int rows = (AURA_SCREEN_HEIGHT - LIST_TOP) / ROW_HEIGHT;
    return rows > 0 ? rows : 1;
}

void aura_widgets_draw_list(const char *title, const aura_list_item_t *items,
                             int count, int selected)
{
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

    lcd_setfont(aura_font(AURA_FONT_STYLE_TITLE));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));
    lcd_putsxy(ROW_PAD_X, TITLE_TOP, (const unsigned char *)title);

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));

    for (i = first; i < count && i < first + visible; i++)
    {
        int row_y = LIST_TOP + (i - first) * ROW_HEIGHT;
        int is_selected = (i == selected);
        int text_x = ROW_PAD_X;

        if (is_selected)
        {
            lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
            lcd_fillrect(0, row_y, AURA_SCREEN_WIDTH, ROW_HEIGHT);
        }

        if (items[i].icon_name)
        {
            int w = draw_icon(items[i].icon_name, AURA_ICON_SIZE_MENU,
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
            draw_icon("check", AURA_ICON_SIZE_MENU,
                      AURA_SCREEN_WIDTH - ROW_PAD_X - AURA_ICON_SIZE_MENU,
                      row_y + (ROW_HEIGHT - AURA_ICON_SIZE_MENU) / 2);
        }
    }
}
