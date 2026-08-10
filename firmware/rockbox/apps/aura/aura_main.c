#include "button.h"
#include "lcd.h"
#include "audio.h"
#include "tick.h"
#include "misc.h"
#include "usb.h"

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

#ifdef USB_ENABLE_HID
    /* Fase 12 (PLAN-UX.md) / D-051: global_settings.usb_hid=false (en
     * apps/main.c) solo apaga la UI del selector HID -- el driver real
     * lee su propia variable estatica en firmware/usb.c, que solo
     * cambia usb_set_hid() (el callback que dispara el menu de Ajustes
     * de Rockbox, que Aura no usa). Sin esta llamada el iPod segue
     * enumerandose como teclado/raton HID ante el host aunque la
     * pantalla ya no lo muestre. Se llama aca (no en apps/main.c) para
     * garantizar que el hilo usb ya termino usb_core_init() y su tabla
     * de drivers ya existe -- aura_main() arranca varios pasos de init()
     * despues de usb_init(). */
    usb_set_hid(false);
#endif

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

        /* SYS_USB_CONNECTED/SYS_POWEROFF/SYS_REBOOT son eventos de
         * sistema, no botones -- aura_screens_handle_button() no los
         * reconoce y los ignora en silencio (bug real encontrado en
         * hardware: el iPod nunca entraba en modo de almacenamiento
         * USB al conectar el cable estando ya arrancado en Aura,
         * porque este evento nunca se manejaba). default_event_handler()
         * es la misma funcion que usa el resto de Rockbox (menu.c,
         * tree.c, etc.): monta el disco/apaga limpio y devuelve el
         * propio evento si lo manejo, o 0 para un boton normal. */
        if (default_event_handler(button) != 0)
            continue;

        aura_screens_handle_button(&nav, button);
    }
}
