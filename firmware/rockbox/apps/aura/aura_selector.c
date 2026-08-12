#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"

#include "aura_selector.h"

void aura_selector_draw(int x, int y, int w, int h, aura_selector_indicator_t indicator)
{
    unsigned bg = a26_color(A26_SHELL_BG);

    /* La pastilla misma es del color de acento -- no SELECTION_FILL
     * (gris) como el sistema viejo. aura_accent() lee el ajuste vigente
     * en runtime (T0.3), nunca el default compilado. */
    a26_shell_fill_rounded_rect(x, y, w, h, AURA_DS_METRICS_SELECTOR_CORNER_RADIUS,
                                 aura_accent(), bg);

    if (indicator == AURA_SELECTOR_INDICATOR_CHEVRON)
    {
        int icon_size = AURA_DS_METRICS_SELECTOR_INDICATOR_MAX_HEIGHT;
        int icon_x = x + w - AURA_DS_METRICS_SELECTOR_INDICATOR_GAP_FROM_EDGE - icon_size;
        int icon_y = y + (h - icon_size) / 2;

        /* Variante "-selector" (blanco constante, G5): el resto del
         * contenido sobre esta pastilla (texto, iconos de fila) usa el
         * mismo tinte -- ver el llamador real en aura_menu_list.c. */
        aura_widgets_draw_icon_variant_selector("chevron-right", icon_size, icon_x, icon_y);
    }
}
