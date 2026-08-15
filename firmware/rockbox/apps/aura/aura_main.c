#include "button.h"
#include "lcd.h"
#include "audio.h"
#include "tick.h"
#include "misc.h"
#include "usb.h"
#include "dir.h"
#include "settings.h"
#include "powermgmt.h"

#include "aura_main.h"
#include "aura_nav.h"
#include "aura_settings.h"
#include "apple2026_shell.h"
#include "aura_screens.h"
#include "aura_nowplaying.h"
#include "aura_stopwatch.h"
#include "aura_search.h"
#include "aura_worldclock.h"
#include "aura_music.h"
#include "aura_widgets.h"
#include "aura_statusbar.h"
#include "aura_menu_list.h"
#include "aura_selection_summary.h"
#include "aura_coverdrift.h"
#include "aura_coverflow.h"
#include "aura_status_bar_v2.h"
#include "aura_screenlock.h"
#include "aura_shutdown_screen.h"
#ifdef HAVE_HARDWARE_CLICK
#include "piezo.h"
#endif

/* Velocidad angular del ultimo SCROLL_FWD/BACK, en grados/seg -- ya
 * calculada y suavizada por el driver real del clickwheel
 * (firmware/target/arm/ipod/button-clickwheel.c, HAVE_SCROLLWHEEL) y
 * leida via button_get_data() antes de que otro button_get() la pise
 * (button_data es una variable global de button_queue.c, no una cola).
 * Los eventos sinteticos (arnes de botones pautado, sim_tasks.c) SIEMPRE
 * la reportan en 0 -- aura_wheel_step() ya trata 0 como "girar lento",
 * asi que degrada bien sin dato real. */
static long s_wheel_velocity;

/* D-238 (encargo del dueno: "que se bloquee tambien el usb"): mientras
 * el candado esta activo, una conexion USB se DIFIERE en vez de
 * atenderse -- ver el bloque `screen_lock_active` mas abajo. `seqnum`
 * hay que capturarlo con button_get_data() en el momento MISMO en que
 * SYS_USB_CONNECTED se recibe (es una variable global de "el ultimo
 * dato crudo", no una cola -- se pisa con cada button_get() que venga
 * despues, y la pantalla de PIN va a pedir varios mientras el usuario
 * escribe) para poder reproducir la conexion real, intacta, si el
 * usuario efectivamente desbloquea. */
static bool s_usb_pending;
static intptr_t s_usb_pending_seqnum;

long aura_main_wheel_velocity(void)
{
    return s_wheel_velocity;
}

/* Botones que soportan el gesto de "mantener presionado"
 * (AURA_BUTTON_HOLD, aura_main.h -- B-02 en BLOCKED.md, pieza de
 * infraestructura general). SELECT (ClockIndicator, B-01) y PLAY
 * (apagado manual desde el menu principal, Task A, ver mas abajo):
 * agregar otro boton es sumarlo aca, next_button() no cambia. */
static bool is_hold_button(long raw)
{
    return raw == BUTTON_SELECT || raw == BUTTON_PLAY;
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
static long s_swallow_btn = BUTTON_NONE;
static bool s_last_was_repeat = false;

bool aura_main_last_was_repeat(void)
{
    return s_last_was_repeat;
}

void aura_main_swallow_repeats(long raw)
{
    s_swallow_btn = raw & ~(BUTTON_REPEAT | BUTTON_REL);
}

/* D-201: reporte del dueno -- "el clicker que suena [en el iPod
 * original] no viene de la bocina, es un efecto realizado con el
 * hardware". Tenia razon: el iPod Classic no tiene bocina (no hay
 * HAVE_SPEAKER en ipod6g.h) -- el clic del original SIEMPRE fue el
 * piezo electrico interno (GPIO/timer por PWM, piezo-6g.c), NUNCA
 * paso por el DAC/audifonos. `system_sound_play(SOUND_KEYCLICK)`
 * (D-196) SI necesita audifonos porque suena por el mismo camino que
 * la musica -- suficiente para el simulador (audio del Mac via SDL,
 * sin concepto de audifonos) pero inaudible en el aparato real sin
 * nada conectado al jack, que es como el dueno lo estaba probando.
 *
 * `keyclick_click()` (apps/misc.c) SI dispara el piezo via
 * `piezo_button_beep()` si `global_settings.keyclick_hardware` esta
 * prendido -- pero Aura nunca llama a esa funcion (D-022, tiene su
 * propio manejo de botones) y ese segundo ajuste nunca se expuso en
 * Ajustes. En vez de agregar un toggle mas para algo que en un iPod
 * real siempre sonaba solo, el piezo suena SIEMPRE que el Clicker de
 * Ajustes este encendido -- mismo criterio de "un solo interruptor,
 * como el original" que ya uso D-196 para el default del beep de
 * software. */
static void aura_main_play_keyclick(void)
{
    system_sound_play(SOUND_KEYCLICK);
#ifdef HAVE_HARDWARE_CLICK
    /* El driver del piezo (piezo-6g.c) es especifico del target ARM
     * real -- no se compila para el simulador (mismo guard anidado que
     * ya usa apps/misc.c:keyclick_click() para esto exacto), aunque
     * HAVE_HARDWARE_CLICK si este definido ahi. */
#if !defined(SIMULATOR)
    if (global_settings.keyclick)
    {
        /* D-214 (temporal, diagnostico del freeze): confirma si el
         * piezo se dispara justo antes de un cuelgue -- ver el
         * comentario grande de aura_music_debug_mark() en
         * aura_music.c. */
        aura_music_debug_mark("K1 piezo...");
        piezo_button_beep(false, false);
        aura_music_debug_mark("K2 piezo OK");
    }
#endif
#endif
}

static long next_button(int timeout_ticks)
{
    static long s_hold_tracking = BUTTON_NONE;

    for (;;)
    {
        long b = (timeout_ticks < 0) ? button_get(true)
                                      : button_get_w_tmo(timeout_ticks);

        if (b & BUTTON_REL)
        {
            long raw = b & ~BUTTON_REL;

            if (raw == s_hold_tracking)
                s_hold_tracking = BUTTON_NONE;
            if (raw == s_swallow_btn)
                s_swallow_btn = BUTTON_NONE; /* soltado: fin del trago */

            /* LEFT/RIGHT: el release SI se entrega (unico caso) -- el
             * reproductor decide al soltar (tap = cambiar pista al
             * instante; hold = aplicar el salto final de la busqueda
             * SIN esperar el timeout de la ventana, correccion
             * 2026-08-12 "el audio sigue quedandose atras"). Ninguna
             * otra pantalla matchea un valor con BUTTON_REL, asi que
             * cae en sus `default` sin efecto. */
            if (raw == BUTTON_LEFT || raw == BUTTON_RIGHT)
            {
                s_last_was_repeat = false;
                s_wheel_velocity = 0;
                return b;
            }
            continue;
        }

        if (b & BUTTON_REPEAT)
        {
            long raw = b & ~BUTTON_REPEAT;

            if (raw == s_swallow_btn)
                continue; /* repeat del boton que disparo la ultima
                           * navegacion con transicion -- se ignora
                           * hasta su REL (aura_main_swallow_repeats) */

            if (is_hold_button(raw))
            {
                if (raw == s_hold_tracking)
                    continue; /* ya disparado para esta pulsacion */
                s_hold_tracking = raw;
                s_wheel_velocity = 0;
                s_last_was_repeat = true;
                return raw | AURA_BUTTON_HOLD;
            }

            b = raw;
            s_last_was_repeat = true;
        }
        else if (b != BUTTON_NONE)
        {
            /* Pulsacion fresca de verdad (no timeout, no repeat):
             * cualquier hold o trago que se estuviera rastreando ya
             * termino. */
            s_hold_tracking = BUTTON_NONE;
            s_swallow_btn = BUTTON_NONE;
            s_last_was_repeat = false;
        }

        s_wheel_velocity = (b == BUTTON_SCROLL_FWD || b == BUTTON_SCROLL_BACK)
            ? (button_get_data() & 0xFFFFFF)
            : 0;
        return b;
    }
}

/* Reporte del dueño en hardware real (D-194): podia copiar musica,
 * fotos y video al iPod en modo disco (Finder los ve bien), pero el
 * firmware no los reconocia. La carpeta correcta ya era una convencion
 * real -- PHOTOS_DIR "/Photos" y VIDEOS_DIR "/Videos" en
 * aura_photos.c/aura_video.c hacen opendir() sobre esos nombres
 * exactos, y "/Music" ya es donde tagcache_rebuild() escanea, y
 * LibrarySync (Aura Studio) ya escribe estos cuatro nombres al
 * sincronizar -- pero NINGUN lado los creaba de antemano. Si el
 * usuario arrastraba archivos sueltos a la raiz del disco (o adivinaba
 * mal el nombre de una carpeta que ni existia), el firmware nunca los
 * encontraba. Se crean aca, en cada arranque (mkdir es barato y
 * idempotente -- dir_exists() evita el intento si ya estan), para que
 * el destino sea obvio en Finder desde la primera conexion, sin
 * depender de que Aura Studio haya sincronizado ya. */
static void aura_main_ensure_media_dirs(void)
{
    static const char* const dirs[] = { "/Music", "/Photos", "/Videos", "/Playlists" };
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    {
        if (!dir_exists(dirs[i]))
            mkdir(dirs[i]);
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

    /* Bloqueo de pantalla GLOBAL (Task B, encargo del dueno): arma el
     * candado EN VIVO (screen_lock_active) en CADA arranque si el
     * usuario lo dejo armado (screen_lock_enabled) -- este es el UNICO
     * lugar que pone screen_lock_active en true; aura_screenlock.c ya
     * no lo hace al terminar de configurar una clave (eso solo arma
     * screen_lock_enabled, ver D-2xx).
     *
     * Por que aca y no "al apagarse" o "al dormir": este firmware no
     * tiene un estado de suspension del que se despierte solo -- TODO
     * apagado real (SYS_POWEROFF, sea por Apagado del iPod/Task A,
     * mantener Play/Pause, bateria critica en firmware/powermgmt.c, o
     * cualquier otro disparador que exista o se agregue despues) termina
     * en power_off() sin retorno (firmware/powermgmt.c); la UNICA forma
     * de volver a correr este codigo es un arranque nuevo desde cero.
     * Forzar la condicion ACA, incondicionalmente, cierra todo camino de
     * apagado de una vez -- no hace falta enumerar ni interceptar cada
     * uno por separado (y no hay riesgo de que un camino nuevo se
     * olvide de rearmar el candado). */
    if (aura_settings.screen_lock_enabled)
        aura_settings.screen_lock_active = true;

    aura_main_ensure_media_dirs();
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

        /* D-197: candado EN VIVO -- se intercepta el loop entero ANTES
         * de tocar aura_screens_draw()/aura_screens_handle_button() o
         * cualquiera de la logica de animacion/tagcache de abajo, para
         * que bloquear de verdad signifique que NADA de la app es
         * alcanzable salvo la propia pantalla de desbloqueo. Reporte
         * del dueno en hardware real: se podia configurar una clave,
         * pero nada la aplicaba de verdad -- el aparato seguia usandose
         * sin problema. */
        if (aura_settings.screen_lock_active)
        {
            aura_screenlock_draw();
            a26_shell_stamp_corners();
            lcd_update();

            button = next_button(-1);

            /* D-238: SYS_USB_CONNECTED es justo lo que
             * default_event_handler() usaba para montar el disco sin
             * pedir PIN (gui_usb_screen_run() dentro de esa rama, ver
             * misc.c) -- el reporte original de esta funcion decia
             * "NADA de la app es alcanzable salvo la propia pantalla
             * de desbloqueo", pero el USB conectado se colaba por este
             * costado. Ahora se difiere: NO se llama a
             * default_event_handler() para este evento en particular
             * (el resto -- SYS_POWEROFF, SYS_BATTERY_UPDATE, etc. --
             * se siguen atendiendo igual que siempre, cargar sigue
             * funcionando: eso lo decide el driver de energia por su
             * cuenta, INDEPENDIENTE de este ack de mas alto nivel). Se
             * guarda el seqnum y se sigue mostrando el candado; recien
             * se atiende de verdad si el usuario desbloquea (mas
             * abajo). Si el usuario nunca desbloquea, el iPod se queda
             * cargando sin exponer el disco -- nunca "colgado" para el
             * host, simplemente el volumen de almacenamiento no
             * aparece hasta entonces, igual que un telefono bloqueado. */
            if (button == SYS_USB_CONNECTED)
            {
                s_usb_pending = true;
                s_usb_pending_seqnum = button_get_data();
                continue;
            }

            if (default_event_handler(button) != 0)
                continue;

            if (button != BUTTON_NONE
                && !(button & BUTTON_REL)
                && (button == BUTTON_SCROLL_FWD || button == BUTTON_SCROLL_BACK
                    || !aura_main_last_was_repeat()))
                aura_main_play_keyclick();

            aura_screenlock_handle_button(&nav, button);

            /* Se acaba de desbloquear (screen_lock_active paso a false
             * dentro de aura_screenlock_handle_button()) y habia una
             * conexion USB en espera: atenderla de verdad recien
             * ahora, con el seqnum real que se guardo en el momento de
             * la conexion -- default_event_handler_deferred_usb()
             * hace exactamente lo mismo que hubiera hecho
             * default_event_handler() en ese momento (misc.c, D-238),
             * solo que con el seqnum correcto en vez de pedirselo de
             * nuevo a button_get_data() (que ya esta pisado por los
             * digitos del PIN que el usuario acaba de escribir).
             *
             * usb_inserted() (estado EN VIVO del driver, no un evento)
             * se revisa antes de reproducir la conexion diferida: el
             * cable pudo haberse desconectado mientras el candado
             * seguia activo -- SYS_USB_DISCONNECTED no pasa por
             * default_event_handler_ex()/esta cola de botones (lo
             * espera gui_usb_screen_run() por su cuenta, que en este
             * escenario nunca llego a correr), asi que este es el
             * unico chequeo real disponible. Sin el, desbloquear mucho
             * despues de desconectar el cable dejaria la pantalla de
             * USB esperando un desenchufe que ya paso -- Aura
             * congelada ahi hasta el proximo enchufado real. */
            if (s_usb_pending && !aura_settings.screen_lock_active)
            {
                s_usb_pending = false;
                if (usb_inserted())
                    default_event_handler_deferred_usb(s_usb_pending_seqnum);
            }
            continue;
        }

        aura_screens_draw(&nav);
        /* La hoja de vidrio del Modo 4 es CUADRADA: sobre ella no se
         * estampan las esquinas derechas de pantalla (se leian como
         * esquinas redondeadas de la hoja, correccion 2026-08-12). */
        if (aura_nav_current(&nav) == AURA_SCREEN_NOWPLAYING
            && aura_nowplaying_sheet_active())
            a26_shell_stamp_corners_left_only();
        else
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

            /* Cursor parpadeante del teclado de Busqueda: medio
             * segundo encendido, medio apagado -- misma puerta de
             * lcd_active() que el resto. */
            if (aura_nav_current(&nav) == AURA_SCREEN_MUSIC_SEARCH
                && aura_search_needs_tick() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;

            /* Reloj internacional: el minutero avanza solo. Un
             * refresco por segundo basta (la manecilla se mueve por
             * minuto), con la misma puerta de lcd_active(). */
            if (aura_nav_current(&nav) == AURA_SCREEN_EXTRAS_CLOCKS
                && aura_worldclock_needs_tick() && timeout_ticks < 0)
                timeout_ticks = HZ;

            /* Cronometro en marcha: las centesimas exigen refresco
             * continuo, con la misma puerta de lcd_active() del resto. */
            if (aura_stopwatch_running() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;

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

            /* D-262: debounce/fundido general del panel derecho de la
             * Ruta A (reemplaza el temporizador de 3s especifico de
             * CoverDrift, D-254) -- cadencia FINA mientras el fundido
             * de 600ms esta realmente corriendo (se vea fluido, igual
             * que aura_coverdrift_animating()); cadencia gruesa durante
             * el resto del tramo pendiente (solo hace falta que el
             * reloj avance para cumplir el plazo de 2s aunque el
             * usuario no toque botones, nada que animar en pantalla
             * todavia). */
            if (aura_screens_right_panel_fading() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;
            else if (aura_screens_right_panel_pending() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;

            /* CoverFlow (T3.2(b)) -- idle/scrolling, mismo criterio de
             * movimiento continuo que CoverDrift. pending() cubre las
             * ventanas de cadencia gruesa del reverso (tramo estatico
             * del marquee de cabecera, persistencia del
             * ScrollIndicator) igual que el resto de esta puerta. */
            if (aura_coverflow_animating() && timeout_ticks < 0)
                timeout_ticks = HZ / 20;
            else if (aura_coverflow_pending() && timeout_ticks < 0)
                timeout_ticks = HZ / 4;

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

        /* D-209 (encargo del dueno, 2026-08-14): pantalla propia de
         * apagado -- se dibuja UNA vez, antes de dejar que
         * default_event_handler() siga con clean_shutdown() (que ya no
         * dibuja su splash de texto de fabrica, ver
         * aura_settings_apply_core_defaults()). Sin esto la pantalla se
         * hubiera quedado congelada en lo ultimo dibujado por Aura (o en
         * negro sin icono) durante los ~1-2s que tarda el apagado real. */
        if (button == SYS_POWEROFF)
            aura_shutdown_screen_draw();

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

        /* Mantener Play/Pause en el menu principal = apagado manual
         * (Task A, encargo del dueno): "si Apagado del iPod es
         * Desactivado, el iPod se puede seguir apagando, pero solo
         * manteniendo Play/Pause desde el menu principal". No existia
         * ningun camino de apagado manual en este codigo (Aura no usa
         * apps/action.c/apps/keymaps -- D-022 -- asi que el
         * ACTION_STD_POWEROFF que otros targets de Rockbox atan a este
         * mismo gesto nunca corria aca), asi que se agrega aca con el
         * mismo mecanismo de hold que SELECT arriba. Gateado a
         * AURA_SCREEN_ROOT (el propio "menu principal") -- en cualquier
         * otra pantalla, PLAY sostenido simplemente no hace nada nuevo
         * (antes tampoco tenia un significado de "hold" propio en
         * ningun lado: cada BUTTON_REPEAT volvia a alternar play/pausa
         * en bucle mientras se sostuviera, un efecto no documentado que
         * este gesto reemplaza).
         *
         * sys_poweroff() (firmware/powermgmt.c) solo ENCOLA el evento
         * SYS_POWEROFF -- el apagado real ocurre en la vuelta siguiente
         * del bucle, cuando next_button() lo entrega y el bloque de
         * arriba (`if (button == SYS_POWEROFF) aura_shutdown_screen_draw();`
         * + default_event_handler()) corre exactamente igual que para
         * el apagado por inactividad (Task A) o cualquier otro camino
         * de apagado -- ninguna logica nueva que duplique esa pantalla. */
        if (button == (BUTTON_PLAY | AURA_BUTTON_HOLD))
        {
            if (aura_nav_current(&nav) == AURA_SCREEN_ROOT)
                sys_poweroff();
            continue;
        }

        /* Clicker (Fase 18, PLAN-UX.md): Aura no usa apps/action.c (D-022),
         * asi que keyclick_click() -- su unico llamador real -- nunca
         * corre; hay que pedir el beep directamente aca via
         * aura_main_play_keyclick() (D-201: beep de software por el DAC
         * -- necesita audifonos, D-196 -- MAS el piezo interno del
         * aparato, el mismo mecanismo del clicker del iPod original,
         * audible sin nada conectado).
         *
         * Suena SOLO en pulsaciones frescas y en cada paso de rueda
         * (correccion 2026-08-12): los repeats de un boton sostenido
         * (conmutadores de mantener backward/forward) sonaban como si
         * se hiciera scroll, y los REL de LEFT/RIGHT que ahora se
         * entregan duplicarian el click del tap. */
        if (button != BUTTON_NONE
            && !(button & BUTTON_REL)
            && (button == BUTTON_SCROLL_FWD || button == BUTTON_SCROLL_BACK
                || !aura_main_last_was_repeat()))
            aura_main_play_keyclick();

        aura_screens_handle_button(&nav, button);
    }
}
