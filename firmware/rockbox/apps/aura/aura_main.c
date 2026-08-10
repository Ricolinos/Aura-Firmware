#include "button.h"
#include "lcd.h"
#include "audio.h"
#include "tick.h"

#include "aura_main.h"
#include "aura_nav.h"
#include "aura_settings.h"
#include "aura_theme.h"
#include "aura_screens.h"
#include "aura_nowplaying.h"
#include "aura_music.h"
#include "aura_home.h"

/* Boton crudo normalizado: se ignoran eventos de soltar (BUTTON_REL) y
 * se trata un repeat igual que una pulsacion nueva (mismo idioma que
 * usa el resto de Rockbox, ver apps/action.c). timeout_ticks < 0 =
 * bloquear indefinidamente; si no, BUTTON_NONE en caso de timeout (para
 * refrescar la pantalla sin que el usuario haya tocado nada, ver
 * D-022). */
static long next_button(int timeout_ticks)
{
    for (;;)
    {
        long b = (timeout_ticks < 0) ? button_get(true)
                                      : button_get_w_tmo(timeout_ticks);
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
        int timeout_ticks = -1;

        aura_screens_draw(&nav);
        lcd_update();

        if (aura_nav_current(&nav) == AURA_SCREEN_NOWPLAYING
            && aura_nowplaying_active()
            && !(audio_status() & AUDIO_STATUS_PAUSE))
        {
            timeout_ticks = HZ / 2;
        }

        /* aura_music_db_ready() dispara el escaneo inicial de la
         * biblioteca la primera vez que se llama (D-021) -- se llama
         * aca, no solo al entrar a Musica, para que empiece en cuanto
         * arranca Aura (como en un iPod real) y para que las pantallas
         * de navegacion musical se refresquen solas en cuanto termina,
         * en vez de quedar mostrando "Preparando la biblioteca..."
         * indefinidamente hasta el proximo boton. */
        if (!aura_music_db_ready() && timeout_ticks < 0)
            timeout_ticks = HZ / 2;

        /* Rotacion/crossfade de la pantalla de inicio dividida (modo
         * grafico Completo, D-025): necesita un tick mas fino que las
         * demas para que el crossfade (~1s) se vea con varios pasos
         * visibles en vez de un salto brusco. */
        if (aura_nav_current(&nav) == AURA_SCREEN_ROOT && aura_home_needs_tick())
            timeout_ticks = HZ / 4;

        button = next_button(timeout_ticks);
        aura_screens_handle_button(&nav, button);
    }
}
