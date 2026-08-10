#include "lcd.h"
#include "button.h"
#include "audio.h"
#include "powermgmt.h"
#include "power.h"

#include "aura_statusbar.h"
#include "aura_widgets.h"
#include "aura_theme.h"
#include "aura_tokens.h"

static const char *battery_icon_name(void)
{
#if CONFIG_CHARGING
    if (charging_state())
        return "battery-charging";
#endif

    int level = battery_level();
    if (level < 0)
        return "battery-medium"; /* desconocido: ni alarmar ni mentir con "full" */
    if (level >= 60)
        return "battery-full";
    if (level >= 20)
        return "battery-medium";
    return "battery-low";
}

void aura_statusbar_draw(int x, int width, const char *title, int centered)
{
    int right = x + width - AURA_SPACING_SM;
    int icon_y = (AURA_LAYOUT_STATUSBAR_HEIGHT - AURA_ICON_SIZE_STATUS) / 2;
    int status = audio_status();
    int is_playing = (status & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)) != 0;
    int is_paused = (status & AUDIO_STATUS_PAUSE) != 0;
    int is_hold = button_hold();

    lcd_set_foreground(aura_color(AURA_TOK_SURFACE));
    lcd_fillrect(x, 0, width, AURA_LAYOUT_STATUSBAR_HEIGHT);
    lcd_set_foreground(aura_color(AURA_TOK_BORDER));
    lcd_hline(x, x + width - 1, AURA_LAYOUT_STATUSBAR_HEIGHT - 1);

    /* Bateria: siempre visible, siempre en el extremo derecho (L5,
     * PLAN-UX.md). */
    right -= AURA_ICON_SIZE_STATUS;
    aura_widgets_draw_icon(battery_icon_name(), AURA_ICON_SIZE_STATUS, right, icon_y);

    /* Candado de Hold y play/pausa: el candado ocupa el lugar del
     * play/pausa si no suena nada; si suena, se coloca a la izquierda
     * del icono de reproduccion (L5). */
    if (is_playing)
    {
        right -= AURA_SPACING_XS + AURA_ICON_SIZE_STATUS;
        aura_widgets_draw_icon(is_paused ? "pause" : "play",
                                AURA_ICON_SIZE_STATUS, right, icon_y);
        if (is_hold)
        {
            right -= AURA_SPACING_XS + AURA_ICON_SIZE_STATUS;
            aura_widgets_draw_icon("lock", AURA_ICON_SIZE_STATUS, right, icon_y);
        }
    }
    else if (is_hold)
    {
        right -= AURA_SPACING_XS + AURA_ICON_SIZE_STATUS;
        aura_widgets_draw_icon("lock", AURA_ICON_SIZE_STATUS, right, icon_y);
    }

    if (title && title[0])
    {
        int w, h, text_x, text_y;

        lcd_setfont(aura_font(AURA_FONT_STYLE_CAPTION));
        lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)title, &w, &h);
        text_y = (AURA_LAYOUT_STATUSBAR_HEIGHT - h) / 2;
        text_x = centered ? x + (width - w) / 2 : x + AURA_SPACING_LG;
        lcd_putsxy(text_x, text_y, (const unsigned char *)title);
    }
}
