#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "button.h"

#include "aura_screenlock.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_lang.h"
#include "aura_screens.h"

#define SL_DIGITS   4
#define SL_BOX_W    34
#define SL_BOX_H    44
#define SL_GAP      A26_SPACING_MD

/* Dos pasadas: escribir la clave y confirmarla. Si la confirmacion no
 * coincide, se vuelve a la primera -- sin mensajes de error tecnicos,
 * el propio reinicio de los digitos es la senal. */
static int  s_digits[SL_DIGITS];
static int  s_first[SL_DIGITS];
static int  s_focus = 0;
static bool s_confirming = false;

static void reset_all(void)
{
    memset(s_digits, 0, sizeof(s_digits));
    memset(s_first, 0, sizeof(s_first));
    s_focus = 0;
    s_confirming = false;
}

void aura_screenlock_draw(void)
{
    int total_w = SL_DIGITS * SL_BOX_W + (SL_DIGITS - 1) * SL_GAP;
    int x0 = (A26_SCREEN_WIDTH - total_w) / 2;
    int y = A26_SCREEN_HEIGHT / 2 - SL_BOX_H / 2 + 10;
    const char *hint = aura_str(s_confirming ? AURA_STR_LOCK_CONFIRM
                                              : AURA_STR_LOCK_SET);
    int i, w, h;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_EXTRAS_SCREENLOCK));

    /* Candado grande arriba, el mismo simbolo del sistema. */
    aura_widgets_draw_icon("lock", A26_ICON_SIZE_PREVIEW,
                            (A26_SCREEN_WIDTH - A26_ICON_SIZE_PREVIEW) / 2,
                            A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_LG);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)hint, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y - A26_SPACING_XXL,
               (const unsigned char *)hint);

    for (i = 0; i < SL_DIGITS; i++)
    {
        int x = x0 + i * (SL_BOX_W + SL_GAP);
        char d[2] = { (char)('0' + s_digits[i]), '\0' };
        bool focus = (i == s_focus);

        a26_shell_fill_rounded_rect(x, y, SL_BOX_W, SL_BOX_H,
                                     A26_LAYOUT_CORNER_RADIUS_CARD,
                                     a26_color(A26_SELECTION_FILL),
                                     a26_color(A26_SHELL_BG));
        lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));
        lcd_set_foreground(focus ? aura_accent() : a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)d, &w, &h);
        lcd_putsxy(x + (SL_BOX_W - w) / 2, y + (SL_BOX_H - h) / 2,
                   (const unsigned char *)d);
    }
}

void aura_screenlock_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    /* La rueda cambia el DIGITO enfocado (0-9, con vuelta). */
    case BUTTON_SCROLL_FWD:
        s_digits[s_focus] = (s_digits[s_focus] + 1) % 10;
        break;
    case BUTTON_SCROLL_BACK:
        s_digits[s_focus] = (s_digits[s_focus] + 9) % 10;
        break;

    case BUTTON_SELECT:
        if (s_focus < SL_DIGITS - 1)
        {
            s_focus++;
            break;
        }
        if (!s_confirming)
        {
            /* Primera pasada completa: pedir confirmacion. */
            memcpy(s_first, s_digits, sizeof(s_first));
            memset(s_digits, 0, sizeof(s_digits));
            s_focus = 0;
            s_confirming = true;
        }
        else if (memcmp(s_first, s_digits, sizeof(s_first)) == 0)
        {
            /* Coincide: queda establecida y se sale. */
            reset_all();
            aura_nav_pop(nav);
        }
        else
        {
            /* No coincide: vuelve a la primera pasada. */
            reset_all();
        }
        break;

    case BUTTON_MENU:
        /* Restablece y NO configura nada (regla del original). */
        reset_all();
        aura_nav_pop(nav);
        break;

    default:
        break;
    }
}
