#include "button.h"
#include "lcd.h"

#include "aura_main.h"
#include "aura_nav.h"
#include "aura_settings.h"
#include "aura_theme.h"
#include "aura_screens.h"

/* Boton crudo normalizado: se ignoran eventos de soltar (BUTTON_REL) y
 * se trata un repeat igual que una pulsacion nueva (mismo idioma que
 * usa el resto de Rockbox, ver apps/action.c). */
static long next_button(void)
{
    for (;;)
    {
        long b = button_get(true);
        if (b & BUTTON_REL)
            continue;
        return b & ~BUTTON_REPEAT;
    }
}

void aura_main(void)
{
    aura_nav_t nav;

    aura_settings_load();
    aura_theme_init();
    aura_nav_init(&nav, AURA_SCREEN_ROOT);

    while (1)
    {
        long button;

        aura_screens_draw(&nav);
        lcd_update();

        button = next_button();
        aura_screens_handle_button(&nav, button);
    }
}
