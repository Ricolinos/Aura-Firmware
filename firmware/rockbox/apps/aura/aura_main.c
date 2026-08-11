#include "button.h"
#include "lcd.h"
#include "audio.h"
#include "tick.h"
#include "misc.h"
#include "usb.h"

#include "aura_main.h"
#include "aura_nav.h"
#include "aura_settings.h"
#include "apple2026_shell.h"
#include "aura_screens.h"
#include "aura_nowplaying.h"
#include "aura_music.h"
#include "aura_widgets.h"

/* Velocidad angular del ultimo SCROLL_FWD/BACK, en grados/seg -- ya
 * calculada y suavizada por el driver real del clickwheel
 * (firmware/target/arm/ipod/button-clickwheel.c, HAVE_SCROLLWHEEL) y
 * leida via button_get_data() antes de que otro button_get() la pise
 * (button_data es una variable global de button_queue.c, no una cola).
 * Los eventos sinteticos (arnes de botones pautado, sim_tasks.c) SIEMPRE
 * la reportan en 0 -- aura_wheel_step() ya trata 0 como "girar lento",
 * asi que degrada bien sin dato real. */
static long s_wheel_velocity;

long aura_main_wheel_velocity(void)
{
    return s_wheel_velocity;
}

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

        b &= ~BUTTON_REPEAT;
        s_wheel_velocity = (b == BUTTON_SCROLL_FWD || b == BUTTON_SCROLL_BACK)
            ? (button_get_data() & 0xFFFFFF)
            : 0;
        return b;
    }
}

void aura_main(void)
{
    aura_nav_t nav;
    bool first_boot = aura_settings_is_first_boot();

    /* Fase 18 (PLAN-UX.md) / D-06x: aplica los defaults opinados de
     * Aura sobre ajustes reales de Rockbox (backlight, volume_limit,
     * poweroff, sleeptimer) una sola vez, en el primer arranque -- ya
     * no se fuerzan en cada boot desde apps/main.c (eso pisaria en
     * silencio lo que el usuario elija y guarde desde las pantallas de
     * Ajustes que la Fase 18 agrega). */
    if (first_boot)
        aura_settings_apply_core_defaults();
    aura_settings_load();
    if (first_boot)
        /* aura_settings_is_first_boot() se basa en si aura.cfg ya
         * existe -- pero aura_settings_save() solo se llama hoy desde
         * las pantallas de Tema/Graficos/EQ/Idioma. Si el usuario
         * jamas toca esas 4 y solo cambia, por ejemplo, Temporiz. luz
         * (que persiste con el settings_save() de Rockbox, no con
         * este), aura.cfg nunca se crearia y el proximo arranque
         * volveria a pisar su eleccion pensando que sigue siendo el
         * primero. Se crea aca, de una vez, para que la marca de
         * "primer arranque ya hecho" no dependa de que pantallas
         * visito el usuario. */
        aura_settings_save();
    a26_shell_init();
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
        a26_shell_stamp_corners();
        lcd_update();

        /* aura_music_db_ready() dispara el escaneo inicial de la
         * biblioteca la primera vez que se llama (D-021) -- se llama
         * aca, no solo al entrar a Musica, para que empiece en cuanto
         * arranca Aura (como en un iPod real) y para que las pantallas
         * de navegacion musical se refresquen solas en cuanto termina,
         * en vez de quedar mostrando "Preparando la biblioteca..."
         * indefinidamente hasta el proximo boton. No es una animacion
         * (no hay nada que ver en pantalla mientras tanto) asi que NO
         * se gatea con lcd_active() -- el escaneo debe seguir avanzando
         * aunque la pantalla este dormida, igual que en un dispositivo
         * real. */
        if (!aura_music_db_ready() && timeout_ticks < 0)
            timeout_ticks = HZ / 2;

        /* Puerta de energia central (doc SS6/CLAUDE.md, Fase 28): toda
         * animacion visual se detiene con la pantalla dormida -- un
         * unico chequeo aca, no uno por cada *_pending() (evita repetir
         * la misma condicion en cada modulo y que alguno se olvide).
         * Redibujar a un ritmo fijo con la pantalla apagada no sirve
         * para nada (nadie lo ve) y solo gasta batería y CPU -- el mismo
         * problema de fondo que ya causo un desborde real de la cola de
         * botones una vez (D-074), asi que tambien es defensivo. */
        if (lcd_active())
        {
            if (aura_nav_current(&nav) == AURA_SCREEN_NOWPLAYING
                && aura_nowplaying_active()
                && (!(audio_status() & AUDIO_STATUS_PAUSE) || aura_nowplaying_needs_tick()))
            {
                timeout_ticks = HZ / 2;
            }

            /* Retardo de 1s del panel derecho en pantallas divididas (L3,
             * Fase 15): mientras el icono nuevo todavia no se mostro, hay
             * que seguir redibujando aunque el usuario no toque nada, si
             * no el cambio pendiente nunca llega a la pantalla hasta el
             * proximo boton. */
            if (aura_widgets_panel_pending() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;

            /* Fundido de la barra de deslizamiento (SS5.3): la cadencia
             * fina de 20fps (doc SS6) solo hace falta en los tramos
             * reales de entrada/salida -- pending() a secas cubre
             * tambien la persistencia (alpha fijo, nada que redibujar),
             * y pedir 20fps ahi de mas fue justamente lo que sobrecargo
             * el bucle principal y desbordo la cola de botones con el
             * simulador interactivo (D-074). Con solo pending() (sin
             * animating()) alcanza la misma cadencia gruesa que ya usa
             * el debounce del panel. */
            if (aura_widgets_scrollbar_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;
            else if (aura_widgets_scrollbar_pending() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;

            /* Resorte de la pastilla de seleccion (SS9.2, Fase 28): a
             * diferencia de la barra de deslizamiento, aca el tramo
             * entero (no solo la entrada/salida) es la parte que se ve
             * -- 20fps durante los ~380ms completos. */
            if (aura_widgets_pill_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;

            /* Resorte del icono de modo en Ahora suena (Fase 30, mismo
             * criterio que la pastilla de arriba). */
            if (aura_nowplaying_wheel_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;
        }

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

        /* Clicker (Fase 18, PLAN-UX.md): Aura no usa apps/action.c (D-022),
         * asi que keyclick_click() -- su unico llamador real -- nunca
         * corre; hay que pedir el beep directamente aca en cada boton
         * real. system_sound_play() ya no hace nada si
         * global_settings.keyclick esta en 0 (Desactivado). */
        if (button != BUTTON_NONE)
            system_sound_play(SOUND_KEYCLICK);

        aura_screens_handle_button(&nav, button);
    }
}
