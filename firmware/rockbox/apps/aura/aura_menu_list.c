#include <string.h>

#include "lcd.h"
#include "string-extra.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_selector.h"
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

void aura_menu_list_draw(int x, int y, const aura_menu_item_v2_t *items,
                          int count, int selected)
{
    int visible = AURA_DS_METRICS_MENU_LIST_MAX_VISIBLE_ROWS;
    int panel_w = AURA_DS_METRICS_LEFT_PANEL_WIDTH;
    int i;

    if (count > visible)
        count = visible; /* TODO(pendiente-doc): scroll >10 items, ver header */

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));

    /* Selector primero, como el sistema viejo ya establecio (D-081):
     * si en el futuro esto anima de nuevo, dibujarlo antes del texto
     * evita que un resorte en transito tape el contenido de otra fila. */
    if (selected >= 0 && selected < count)
    {
        int sel_y = y + selected * ROW_H;
        aura_selector_draw(x + PADDING, sel_y, panel_w - 2 * PADDING, ROW_H,
                            AURA_SELECTOR_INDICATOR_NONE);
    }

    for (i = 0; i < count; i++)
    {
        int row_y = y + i * ROW_H;
        int is_selected = (i == selected);
        int text_x = x + PADDING;
        char truncated[64];
        int max_text_w;

        if (items[i].icon_name)
        {
            int icon_x = x + ICON_X;
            int icon_y = row_y + (ROW_H - ICON_SZ) / 2;

            if (is_selected)
                aura_widgets_draw_icon_variant_selector(items[i].icon_name, ICON_SZ, icon_x, icon_y);
            else
                aura_widgets_draw_icon(items[i].icon_name, ICON_SZ, icon_x, icon_y);

            text_x = icon_x + ICON_SZ + TEXT_GAP;
        }

        max_text_w = x + panel_w - PADDING - text_x;
        truncate_to_fit(items[i].label, truncated, sizeof(truncated), max_text_w);

        /* Blanco constante (G5) sobre la pastilla de acento, en AMBOS
         * temas -- mismo tinte que ya hornea el icono "-selector".
         * AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT es el
         * mismo token que definio esa decision (D-086), reusado aca en
         * vez de inventar un segundo valor para lo mismo. */
        lcd_set_foreground(is_selected ? AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT
                                        : a26_color(A26_TEXT_PRIMARY));
        lcd_putsxy(text_x, row_y + (ROW_H - 10) / 2, (const unsigned char *)truncated);
    }
}
