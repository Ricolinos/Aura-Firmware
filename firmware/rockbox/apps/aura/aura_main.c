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
#include "aura_statusbar.h"
#include "aura_menu_list.h"
#include "aura_selection_summary.h"
#include "aura_coverdrift.h"
#include "aura_coverflow.h"
#include "aura_status_bar_v2.h"

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

/* Botones que soportan el gesto de "mantener presionado"
 * (AURA_BUTTON_HOLD, aura_main.h -- B-02 en BLOCKED.md, pieza de
 * infraestructura general). Hoy solo SELECT (ClockIndicator, B-01):
 * agregar otro boton es sumarlo aca, next_button() no cambia. */
static bool is_hold_button(long raw)
{
    return raw == BUTTON_SELECT;
}

/* Boton crudo normalizado. timeout_ticks < 0 = bloquear
 * indefinidamente; si no, BUTTON_NONE en caso de timeout (para
 * refrescar la pantalla sin que el usuario haya tocado nada, D-022).
 *
 * Una pulsacion NUEVA siempre dispara de inmediato, sin esperar a ver
 * si se vuelve un hold -- esperar agregaria latencia perceptible a
 * TODOS los clicks de TODAS las pantallas, la inmensa mayoria de las
 * cuales no tiene ningun gesto de hold que distinguir (justo la
 * regresion que B-02 queria evitar: "siempre y cuando no interfiera
 * con otra funcion"). BUTTON_REL se sigue ignorando (el dispatch ya
 * ocurrio en la pulsacion, igual que siempre).
 *
 * BUTTON_REPEAT es donde cambia el comportamiento: para los botones
 * de is_hold_button(), el PRIMER repeat de una pulsacion (el driver
 * los espacia por REPEAT_START~300ms, firmware/drivers/button.c --
 * mismo umbral de "hold" que usa el resto de Rockbox via los mapas de
 * botones en apps/keymaps, nunca un temporizador propio de Aura) dispara
 * AURA_BUTTON_HOLD una sola vez; los repeats siguientes de la MISMA
 * pulsacion se ignoran (no hay evento de "soltar" un hold). Para
 * cualquier otro boton, un repeat se sigue tratando igual que una
 * pulsacion nueva, sin cambios -- el giro sostenido del scroll y
 * saltar pistas rapido manteniendo LEFT/RIGHT en Ahora suena dependen
 * de este comportamiento exacto. */
static long next_button(int timeout_ticks)
{
    static long s_hold_tracking = BUTTON_NONE;

    for (;;)
    {
        long b = (timeout_ticks < 0) ? button_get(true)
                                      : button_get_w_tmo(timeout_ticks);

        if (b & BUTTON_REL)
        {
            if ((b & ~BUTTON_REL) == s_hold_tracking)
                s_hold_tracking = BUTTON_NONE;
            continue;
        }

        if (b & BUTTON_REPEAT)
        {
            long raw = b & ~BUTTON_REPEAT;

            if (is_hold_button(raw))
            {
                if (raw == s_hold_tracking)
                    continue; /* ya disparado para esta pulsacion */
                s_hold_tracking = raw;
                s_wheel_velocity = 0;
                return raw | AURA_BUTTON_HOLD;
            }

            b = raw;
        }
        else if (b != BUTTON_NONE)
        {
            /* Pulsacion fresca de verdad (no timeout, no repeat):
             * cualquier hold que se estuviera rastreando ya termino. */
            s_hold_tracking = BUTTON_NONE;
        }

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

            /* MarqueeText del titulo de la barra (T2.1) -- mismo par
             * pending()/animating() que ya usa la barra de
             * deslizamiento (cadencia gruesa durante todo el ciclo
             * mientras el texto desborde, fina solo cuando los pixeles
             * se estan moviendo de verdad). Bug real encontrado en la
             * propia verificacion de esta tarea (no en teoria): una
             * primera version que solo pedia cuadros durante el tramo
             * de movimiento dejaba el marquee congelado para siempre
             * en cuanto el bucle se dormia durante el tramo estatico
             * -- nunca volvia a despertar para notar que tocaba
             * empezar a mover el texto. */
            if (aura_statusbar_title_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;
            else if (aura_statusbar_title_pending() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;

            /* ScrollIndicator de MenuList v2 (T2.4) -- mismo par
             * pending()/animating() que el resto de esta puerta. */
            if (aura_menu_list_scroll_indicator_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;
            else if (aura_menu_list_scroll_indicator_pending() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;

            /* MarqueeText de los dos slots de texto de SelectionSummary
             * (T2.8) -- mismo par pending()/animating(), escrito
             * correcto desde el primer intento (D-093 ya establecio
             * este aprendizaje, no se repite el bug de D-091). */
            if (aura_selection_summary_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;
            else if (aura_selection_summary_pending() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;

            /* CoverDrift (T2.9) -- movimiento continuo mientras este
             * montado, sin tramo estatico real (a diferencia del resto
             * de esta puerta): pending()/animating() coinciden. */
            if (aura_coverdrift_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;

            /* CoverFlow (T3.2(b)) -- idle/scrolling, mismo criterio de
             * movimiento continuo que CoverDrift. */
            if (aura_coverflow_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;

            /* ClockIndicator por atajo (B-01 en BLOCKED.md) --
             * animating() durante el Drop-and-Lift/Push-and-Pull real,
             * pending() (cadencia gruesa) mientras sigue visible
             * esperando los 10s de autoocultado. */
            if (aura_status_bar_v2_clock_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;
            else if (aura_status_bar_v2_clock_pending() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;
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

        /* Gesto de "mantener SELECT" (AURA_BUTTON_HOLD, aura_main.h,
         * B-02 en BLOCKED.md) -- interceptado aca de forma
         * centralizada, igual que la puerta de energia: StatusBar es
         * la unica duena de este gesto hoy (revela ClockIndicator, B-01),
         * ninguna pantalla necesita reconocerlo. Se consume aca mismo
         * (no llega a aura_screens_handle_button()) -- si en el futuro
         * una pantalla especifica necesita su PROPIO significado para
         * mantener SELECT, este es el lugar donde se decidiria cual de
         * las dos funciones gana, no algo que resolver hoy con un solo
         * consumidor real. */
        if (button == (BUTTON_SELECT | AURA_BUTTON_HOLD))
        {
            aura_status_bar_v2_reveal_clock();
            continue;
        }

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
