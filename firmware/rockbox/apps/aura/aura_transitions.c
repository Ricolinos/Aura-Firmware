#include <string.h>

#include "lcd.h"
#include "kernel.h"
#include "tick.h"
#include "gui/viewport.h"
#include "debug.h"
#include "button.h"

#include "aura_transitions.h"
#include "aura_screens.h"
#include "aura_widgets.h"
#include "aura_settings.h"
#include "apple2026_tokens.h"
#include "aura_flow.h"
#include "aura_patterns.h"
#include "aura_albumart.h"
#include "aura_art.h"
#include "aura_nowplaying.h"
#include "aura_coverflow.h"
#include "aura_statusbar.h"
#include "aura_status_bar_v2.h"
#include "aura_lang.h"
#include "apple2026_shell.h"

/* Instrumentacion de frames (Fase 16, PLAN-UX.md): DEBUGF es un no-op
 * fuera de builds DEBUG, asi que queda siempre presente en el codigo
 * sin costo en produccion -- sirve para decidir con datos reales (no a
 * ojo) si un modo grafico necesita degradar en hardware real. */
#define TRANSITION_LOG(name, frames, start_tick) \
    do { \
        DEBUGF("aura_transitions: %s %d frames en %ld ticks\n", \
               (name), (frames), (long)(current_tick - (start_tick))); \
        (void)(start_tick); /* evita -Wunused-variable en builds sin DEBUG */ \
    } while (0)

/* Bug real encontrado con el simulador interactivo, no con el arnes de
 * botones pautado (D-074): estos bucles de cuadros son sincronicos --
 * duermen entre cuadros sin leer el boton en ningun momento -- y con el
 * usuario sosteniendo o repitiendo un boton rapido mientras se encadenan
 * varias transiciones seguidas (navegar rapido = varias pantallas =
 * varios push/reveal uno atras del otro), los eventos de repeticion del
 * teclado se acumulan en la cola sin drenarse hasta desbordarla
 * (KERNEL_ASSERT "queue_post ovf"). No hace falta ATENDER el boton
 * durante la animacion (séria una complicacion real de estado a medio
 * dibujar); alcanza con no dejar que la cola crezca sin limite. */
static void drain_button_queue_if_full(void)
{
    if (button_queue_full())
        button_clear_queue();
}

/* -- Push real (Fase 26 / D-068) ---------------------------------------
 *
 * Analisis cuadro a cuadro del firmware original (video del usuario,
 * D-068): en un T1/T3 las DOS pantallas se mueven en bloque -- la vieja
 * sale por un borde mientras la nueva entra por el opuesto -- con
 * ease-out (el primer paso cubre ~la mitad del recorrido) y el titulo
 * de la barra de estado cambia AL INSTANTE, sin deslizar. El enfoque
 * anterior ("wipe" de revelado) no movia la pantalla vieja, y ademas
 * a26_shell_clear_screen() borraba el framebuffer entero en cada
 * cuadro (lcd_clear_display ignora el viewport activo), asi que en la
 * practica la nueva pantalla deslizaba sobre fondo vacio.
 *
 * Mecanica: la pantalla nueva se renderiza UNA vez a un framebuffer
 * offscreen propio (viewport_set_buffer, API estandar de Rockbox);
 * despues cada cuadro es solo (a) desplazar el remanente de la pantalla
 * vieja en el framebuffer real con memmove por fila (stride horizontal,
 * filas contiguas -- garantizado para este target, config.h:818) y (b)
 * blitear la franja entrante de la nueva con lcd_bitmap_part. Ningun
 * viewport sale jamas de [0, A26_SCREEN_WIDTH] (la restriccion real de
 * D-030 se mantiene). La banda de la barra de estado (y < statusbar_h)
 * queda fuera del deslizamiento: se blitea completa antes del primer
 * cuadro (titulo instantaneo, como el original). */

static fb_data s_push_fb[A26_SCREEN_WIDTH * A26_SCREEN_HEIGHT];

static void *push_fb_address(int x, int y)
{
    return &s_push_fb[y * A26_SCREEN_WIDTH + x];
}

static struct frame_buffer_t s_push_buffer = {
    { .fb_ptr = s_push_fb },
    .get_address_fn = &push_fb_address,
    .stride = STRIDE_MAIN(A26_SCREEN_WIDTH, A26_SCREEN_HEIGHT),
    .elems = A26_SCREEN_WIDTH * A26_SCREEN_HEIGHT,
};

/* Desplazamiento con ease-out cuadratico: d(t) = W * (1 - (1-t)^2).
 * Coincide con lo observado en el original: arranque rapido,
 * desaceleracion al asentarse. */
static int eased_offset(int width, int i, int frames)
{
    int remain = frames - i;
    return width - (width * remain * remain) / (frames * frames);
}

/* -- Revelado detras de paneles con CoverDrift (D-259, generalizado
 * D-261) -----------------------------------------------------------
 *
 * D-259 construyo esto SOLO dentro de aura_transition_coverflow_enter(),
 * para la entrada a Cover Flow con CoverDrift activo. D-261 lo extrae
 * a dos funciones compartidas para que aura_transition_slide() (el
 * push generico, usado por CUALQUIER otra navegacion split->full)
 * pueda usar la misma coreografia -- CERO cambio de comportamiento
 * para Cover Flow, que ahora es un llamador mas de estas mismas dos
 * funciones en vez de tener su propia copia del codigo. */
static fb_data s_outgoing_fb[A26_SCREEN_WIDTH * A26_SCREEN_HEIGHT];

/* Captura EXACTA de lo que hay en pantalla ahora mismo (LeftPanel +
 * panel derecho con CoverDrift) -- tiene que llamarse ANTES de
 * renderizar la pantalla destino al framebuffer real (mas abajo, en
 * cada llamador), o ese contenido se pierde. Mismo patron de lectura
 * directa del framebuffer que ya usa aura_transition_flip_and_flow()
 * (s_back_tex) para capturar "lo que esta ahora en pantalla". Nunca
 * hay dos transiciones activas a la vez (son sincronicas/bloqueantes),
 * asi que un solo buffer estatico compartido entre TODOS los
 * llamadores de esta funcion es seguro. */
static void capture_outgoing_split_frame(void)
{
    int y;

    for (y = 0; y < A26_SCREEN_HEIGHT; y++)
    {
        const fb_data *src = FBADDR(0, y);
        memcpy(&s_outgoing_fb[y * A26_SCREEN_WIDTH], src,
               A26_SCREEN_WIDTH * sizeof(fb_data));
    }
}

/* Ambos paneles (izquierdo y derecho, cada uno `panel_w` px, ya
 * capturados por capture_outgoing_split_frame()) salen hacia SU borde,
 * revelando la pantalla destino (ya renderizada completa en
 * s_push_fb, quieta) segun se abren -- "dando paso a que la pantalla
 * completa salga del centro" (encargo textual del dueno del
 * producto). Repinta la base completa cada cuadro (mas simple y
 * robusto que llevar la cuenta de la franja delta exacta recien
 * revelada) -- barato, son solo dos blits desde buffers estaticos,
 * nunca mas de `frames` iteraciones. La banda [0, bar_h) queda en
 * `bg` durante todo el revelado -- la StatusBar destino cae despues,
 * en la Fase 2 (Drop), igual que el resto de las transiciones
 * split->full de este archivo. Requiere que la pantalla destino ya
 * este prerrenderizada en s_push_fb (cada llamador lo hace antes de
 * llamar aca). */
static void reveal_behind_panels(int panel_w, int bar_h, unsigned bg,
                                  int frames, int frame_delay)
{
    int i, y;

    for (i = 1; i <= frames; i++)
    {
        int d = eased_offset(panel_w, i, frames);
        int left_w = panel_w - d;
        int right_w = panel_w - d;
        int right_x = A26_SCREEN_WIDTH - right_w;

        lcd_bitmap_part(s_push_fb, 0, bar_h, A26_SCREEN_WIDTH,
                         0, bar_h, A26_SCREEN_WIDTH, A26_SCREEN_HEIGHT - bar_h);
        for (y = 0; y < bar_h; y++)
        {
            fb_data *row = FBADDR(0, y);
            int gx;
            for (gx = 0; gx < A26_SCREEN_WIDTH; gx++)
                row[gx] = bg;
        }

        /* LeftPanel remanente (columnas [d, panel_w) de lo capturado --
         * la mitad derecha de la imagen, ya que se desliza hacia la
         * izquierda y su mitad izquierda es la que ya salio de
         * encuadre), pegado al borde izquierdo. */
        if (left_w > 0)
            lcd_bitmap_part(s_outgoing_fb, d, 0, A26_SCREEN_WIDTH,
                             0, 0, left_w, A26_SCREEN_HEIGHT);

        /* Panel derecho remanente (columnas [panel_w, panel_w+right_w)
         * de lo capturado -- la mitad IZQUIERDA de esa imagen, ya que
         * se desliza hacia la derecha y su mitad derecha es la que ya
         * salio de encuadre), pegado al borde derecho segun avanza. */
        if (right_w > 0)
            lcd_bitmap_part(s_outgoing_fb, panel_w, 0, A26_SCREEN_WIDTH,
                             right_x, 0, right_w, A26_SCREEN_HEIGHT);

        lcd_update();
        drain_button_queue_if_full();
        if (i < frames)
            sleep(frame_delay);
    }
}

/* Espejo temporal EXACTO de reveal_behind_panels() (D-267, encargo del
 * dueno de producto: "la transicion... deberia ser exactamente la misma
 * transicion, pero invertida cuando retrocedamos, por ejemplo, al salir
 * del coverflow"). Roles de los dos buffers INTERCAMBIADOS respecto a la
 * version de entrada: aca `s_push_fb` ya tiene el DESTINO (split, la
 * pantalla a la que se vuelve -- lo pre-renderiza el paso 1 de
 * aura_transition_slide(), IGUAL que para cualquier otro llamador, sin
 * cambio) y `s_outgoing_fb` tiene lo SALIENTE (full, capturado en vivo
 * por capture_outgoing_split_frame() antes de pre-renderizar el
 * destino). En vez de que los paneles ENCOJAN revelando una base fija
 * (la version de entrada), aca CRECEN desde sus propios bordes de
 * pantalla, cubriendo progresivamente la base fija (lo saliente) hasta
 * formar el split completo -- por eso la lectura de origen NO se
 * desplaza con `d` (a diferencia de la version de entrada, que si
 * recorta una ventana movil del panel que se encoge): el destino ya
 * esta fijo en su posicion FINAL en `s_push_fb`, solo se revela mas
 * porcion de esa misma imagen fija segun avanza el cuadro. */
static void reveal_behind_panels_exit(int panel_w, int bar_h, unsigned bg,
                                       int frames, int frame_delay)
{
    int i, y;

    for (i = 1; i <= frames; i++)
    {
        int d = panel_w - eased_offset(panel_w, i, frames);
        int left_w = panel_w - d;
        int right_w = panel_w - d;

        /* Base: lo saliente (p.ej. Cover Flow), pantalla completa. */
        lcd_bitmap_part(s_outgoing_fb, 0, bar_h, A26_SCREEN_WIDTH,
                         0, bar_h, A26_SCREEN_WIDTH, A26_SCREEN_HEIGHT - bar_h);
        for (y = 0; y < bar_h; y++)
        {
            fb_data *row = FBADDR(0, y);
            int gx;
            for (gx = 0; gx < A26_SCREEN_WIDTH; gx++)
                row[gx] = bg;
        }

        /* Paneles del destino, ya fijos en s_push_fb -- revelados directo
         * desde sus propios bordes de pantalla, creciendo hacia el
         * centro (sin desplazamiento de origen: el destino no se mueve,
         * solo se revela mas de el). */
        if (left_w > 0)
            lcd_bitmap_part(s_push_fb, 0, 0, A26_SCREEN_WIDTH,
                             0, 0, left_w, A26_SCREEN_HEIGHT);
        if (right_w > 0)
            lcd_bitmap_part(s_push_fb, A26_SCREEN_WIDTH - right_w, 0, A26_SCREEN_WIDTH,
                             A26_SCREEN_WIDTH - right_w, 0, right_w, A26_SCREEN_HEIGHT);

        lcd_update();
        drain_button_queue_if_full();
        if (i < frames)
            sleep(frame_delay);
    }
}

/* -- Shift-and-Reveal (D-278, transiciones/00-vocabulario.md) --------
 *
 * Primer consumidor: "Acerca de" (D-279) -- ícono/tile de
 * SelectionSummary que se reposiciona a la izquierda mientras el resto
 * de la pantalla completa (FULL-CARRY) aparece en el hueco liberado a
 * la derecha, con LeftPanel saliendo empujado en paralelo. Diseñada
 * para servir tambien a DateEditor (componentes/date-editor.md) sin
 * reescribirse -- solo cambia el `carry` que le pasa el llamador.
 *
 * Sin callbacks de dibujo, misma convencion que el resto de este
 * archivo: el elemento persistente se CAPTURA como bitmap de un
 * framebuffer ya renderizado (s_outgoing_fb) en vez de pedirle al
 * llamador que sepa dibujarlo en (x,y) -- el llamador solo declara
 * DONDE vive el elemento en cada extremo (`aura_shift_rect_t`), nunca
 * COMO se ve. El destino completo se prerrenderiza una vez con
 * aura_screens_draw() al offscreen `s_push_fb`, igual que las demas
 * transiciones -- el cuadro final coincide exacto con esa imagen fija
 * (mismo "requisito de continuidad" que ya exige el vuelo de caratula),
 * porque el rect `to` del destino YA tiene ahi dibujado el mismo
 * elemento que el bitmap capturado reproduce encima durante el
 * recorrido.
 *
 * `direction` > 0: split -> full (entrar). `carry` siempre describe
 * {from = posicion en split, to = posicion en full} sin importar la
 * direccion -- la funcion decide sola hacia donde viaja el elemento y
 * que buffer usar como fuente segun el signo. */
static void shift_and_reveal_enter(aura_nav_t *nav, const aura_shift_rect_t *carry,
                                    int frames, int frame_delay, int bar_h)
{
    unsigned bg = a26_color(A26_SHELL_BG);
    const int panel_w = AURA_DS_METRICS_LEFT_PANEL_WIDTH;
    int i, x, y;
    (void)nav;

    for (i = 1; i <= frames; i++)
    {
        int prog = eased_offset(256, i, frames);
        int d = eased_offset(panel_w, i, frames);
        int left_w = panel_w - d;
        int ex, ey;

        /* 1. Base: el destino aparece con fade-in en el hueco que se va
         * liberando -- "el nuevo componente aparece en el espacio que
         * se libera a la derecha" (doc). Se pinta el ancho completo: la
         * franja que todavia cubre LeftPanel se sobreescribe en el paso
         * 2, es mas simple que recortar la fuente por columna. */
        for (y = bar_h; y < A26_SCREEN_HEIGHT; y++)
        {
            fb_data *dst = FBADDR(0, y);
            const fb_data *src = &s_push_fb[y * A26_SCREEN_WIDTH];

            for (x = 0; x < A26_SCREEN_WIDTH; x++)
                dst[x] = a26_shell_blend(bg, src[x], prog);
        }
        /* Hueco de barra durante el empuje (Push-and-Drop estandar,
         * mismo criterio que reveal_behind_panels()): la StatusBar(full)
         * no existe todavia, cae al final. */
        lcd_set_foreground(bg);
        for (y = 0; y < bar_h; y++)
            lcd_hline(0, A26_SCREEN_WIDTH - 1, y);

        /* 2. LeftPanel + su StatusBar(split) salen empujados -- mismo
         * remanente que reveal_behind_panels(), sin espejo del panel
         * derecho (aca no hay panel derecho que crecer, es un elemento
         * puntual, no CoverDrift). */
        if (left_w > 0)
            lcd_bitmap_part(s_outgoing_fb, d, 0, A26_SCREEN_WIDTH,
                             0, 0, left_w, A26_SCREEN_HEIGHT);

        /* 3. Elemento persistente: capturado de lo que YA estaba en
         * pantalla (s_outgoing_fb, rect `from`), viajando hacia `to`,
         * encima de todo -- los textos auxiliares que rodeaban al
         * elemento en split no estan en este rect, asi que desaparecen
         * desde el primer cuadro (quedan tapados por la base del
         * destino), cumpliendo la regla del documento. */
        ex = aura_pattern_lerp(carry->from_x, carry->to_x, prog);
        ey = aura_pattern_lerp(carry->from_y, carry->to_y, prog);
        lcd_bitmap_part(s_outgoing_fb, carry->from_x, carry->from_y, A26_SCREEN_WIDTH,
                         ex, ey, carry->from_w, carry->from_h);

        lcd_update();
        drain_button_queue_if_full();
        if (i < frames)
            sleep(frame_delay);
    }

    /* Drop de StatusBar(full) al final, como pide la doc viva (no
     * "simultaneo" como el original de 2008 -- date-editor.md ya lo
     * confirma para el otro consumidor previsto). */
    {
        int drop_frames = (aura_settings.animation_mode == AURA_ANIM_ALL)
            ? AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_ALL
            : AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_MINIMAL;

        for (i = 1; i <= drop_frames; i++)
        {
            int dd = eased_offset(bar_h, i, drop_frames);

            if (dd > 0)
                lcd_bitmap_part(s_push_fb, 0, bar_h - dd, A26_SCREEN_WIDTH,
                                0, 0, A26_SCREEN_WIDTH, dd);
            lcd_update_rect(0, 0, A26_SCREEN_WIDTH, bar_h);
            drain_button_queue_if_full();
            if (i < drop_frames)
                sleep(frame_delay);
        }
    }
}

/* Inversa exacta (Menu, direction < 0): full -> split. Lift de la
 * StatusBar(full) PRIMERO (fase 1 de Lift-and-Push, D-267), despues el
 * contenido saliente se desvanece mientras LeftPanel entra empujado
 * (crece desde su propio borde, mismo mecanismo de
 * reveal_behind_panels_exit()) y el elemento persistente viaja
 * to -> from -- semejante a como Lift-and-Push ya es la inversa
 * temporal exacta de Push-and-Drop en aura_transition_slide(). */
static void shift_and_reveal_exit(aura_nav_t *nav, const aura_shift_rect_t *carry,
                                   int frames, int frame_delay, int bar_h)
{
    unsigned bg = a26_color(A26_SHELL_BG);
    const int panel_w = AURA_DS_METRICS_LEFT_PANEL_WIDTH;
    int i, x, y;
    (void)nav;

    {
        int lift_frames = (aura_settings.animation_mode == AURA_ANIM_ALL)
            ? AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_ALL
            : AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_MINIMAL;

        for (i = 1; i <= lift_frames; i++)
        {
            int dd = eased_offset(bar_h, i, lift_frames);

            for (y = 0; y + dd < bar_h; y++)
                memcpy(FBADDR(0, y), FBADDR(0, y + dd),
                       A26_SCREEN_WIDTH * sizeof(fb_data));
            lcd_set_foreground(bg);
            for (y = bar_h - dd; y < bar_h; y++)
                lcd_hline(0, A26_SCREEN_WIDTH - 1, y);
            lcd_update_rect(0, 0, A26_SCREEN_WIDTH, bar_h);
            drain_button_queue_if_full();
            if (i < lift_frames)
                sleep(frame_delay);
        }
    }

    for (i = 1; i <= frames; i++)
    {
        int prog = eased_offset(256, i, frames);
        int left_w = eased_offset(panel_w, i, frames);
        int ex, ey;

        /* 1. Base: lo saliente (full, capturado en s_outgoing_fb antes
         * de prerrenderizar el destino) se desvanece hacia el fondo. */
        for (y = bar_h; y < A26_SCREEN_HEIGHT; y++)
        {
            fb_data *dst = FBADDR(0, y);
            const fb_data *src = &s_outgoing_fb[y * A26_SCREEN_WIDTH];

            for (x = 0; x < A26_SCREEN_WIDTH; x++)
                dst[x] = a26_shell_blend(bg, src[x], 256 - prog);
        }
        lcd_set_foreground(bg);
        for (y = 0; y < bar_h; y++)
            lcd_hline(0, A26_SCREEN_WIDTH - 1, y);

        /* 2. LeftPanel + su StatusBar(split), ya fijos en el destino
         * (s_push_fb), entran creciendo desde el borde izquierdo --
         * mismo criterio que reveal_behind_panels_exit(). */
        if (left_w > 0)
            lcd_bitmap_part(s_push_fb, 0, 0, A26_SCREEN_WIDTH,
                             0, 0, left_w, A26_SCREEN_HEIGHT);

        /* 3. Elemento persistente: capturado de lo saliente (rect `to`,
         * su posicion en full), viajando de vuelta hacia `from`. */
        ex = aura_pattern_lerp(carry->to_x, carry->from_x, prog);
        ey = aura_pattern_lerp(carry->to_y, carry->from_y, prog);
        lcd_bitmap_part(s_outgoing_fb, carry->to_x, carry->to_y, A26_SCREEN_WIDTH,
                         ex, ey, carry->to_w, carry->to_h);

        lcd_update();
        drain_button_queue_if_full();
        if (i < frames)
            sleep(frame_delay);
    }
    /* Sin Drop aparte para la StatusBar(split): viaja PEGADA a su panel
     * en el paso 2 (igual que reveal_behind_panels_exit()), no cae por
     * separado como la de pantalla completa. */
}

void aura_transition_shift_and_reveal(aura_nav_t *nav, int direction,
                                      const aura_shift_rect_t *carry)
{
    int frames, frame_delay;
    const int bar_h = A26_LAYOUT_STATUSBAR_HEIGHT;
    struct viewport vp;
    struct viewport *saved;
    long start_tick = current_tick;

    if (!lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE || direction == 0)
        return;

    /* Mismos tokens de Push-and-Drop (D-274) -- ratificado por el dueno
     * (PLAN-about-storage.md Q6): el empuje de LeftPanel que ocurre en
     * paralelo ya va a este ritmo, dos velocidades simultaneas se
     * verian desacopladas. Shift-and-Reveal no tiene timing propio en
     * la doc ("[ ] Timing/easing... sigue sin valores"). */
    if (aura_settings.animation_mode == AURA_ANIM_ALL)
    {
        frames = AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FRAMES_ALL;
        frame_delay = HZ / AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FPS_ALL;
    }
    else
    {
        frames = AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FRAMES_MINIMAL;
        frame_delay = HZ / AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FPS_MINIMAL;
    }

    /* Captura ANTES de prerrenderizar el destino -- mismo orden que el
     * resto del archivo, imprescindible aca ademas para el elemento
     * persistente (su bitmap fuente sale de este buffer en los dos
     * sentidos). */
    capture_outgoing_split_frame();

    viewport_set_defaults(&vp, SCREEN_MAIN);
    vp.x = 0;
    vp.y = 0;
    vp.width = A26_SCREEN_WIDTH;
    vp.height = A26_SCREEN_HEIGHT;
    viewport_set_buffer(&vp, &s_push_buffer, SCREEN_MAIN);
    saved = lcd_set_viewport(&vp);
    aura_screens_draw(nav);
    lcd_set_viewport(saved);

    if (direction > 0)
        shift_and_reveal_enter(nav, carry, frames, frame_delay, bar_h);
    else
        shift_and_reveal_exit(nav, carry, frames, frame_delay, bar_h);

    TRANSITION_LOG("shift-and-reveal", frames, start_tick);
}

void aura_transition_slide(aura_nav_t *nav, int direction, int width,
                            bool cover_drift_was_active)
{
    int frames, frame_delay, i, y;
    int d_prev = 0;
    const int bar_h = A26_LAYOUT_STATUSBAR_HEIGHT;
    struct viewport vp;
    struct viewport *saved;
    long start_tick = current_tick;
    unsigned shell_bg = a26_color(A26_SHELL_BG);
    /* Modo de barra ANTES de renderizar la pantalla nueva: split_active()
     * refleja el layout de la ultima pantalla dibujada, asi que aca
     * todavia es el de la pantalla que se va (ver mas abajo el de la
     * que llega, ya con el offscreen renderizado). */
    int old_split = aura_widgets_split_active();
    int new_split, bar_changes, body_top;
    int use_reveal;

    /* AUDITORIA-01 A-08: "toda animacion se detiene con la pantalla
     * dormida" (doc SS6/CLAUDE.md) -- la puerta central de aura_main.c
     * gatea CUANDO se pide un redibujo por timeout, pero estas funciones
     * corren dentro del manejo directo de boton, fuera de ese camino;
     * sin este chequeo, una transicion podia arrancar y sostener varios
     * frames con la pantalla ya apagada. */
    if (!lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE || direction == 0)
        return;

    /* Fuera de rango -> ancho completo. */
    if (width <= 0 || width > A26_SCREEN_WIDTH)
        width = A26_SCREEN_WIDTH;

    if (aura_settings.animation_mode == AURA_ANIM_ALL)
    {
        frames = AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FRAMES_ALL;
        frame_delay = HZ / AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FPS_ALL;
    }
    else /* AURA_ANIM_MINIMAL */
    {
        frames = AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FRAMES_MINIMAL;
        frame_delay = HZ / AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FPS_MINIMAL;
    }

    /* D-261: la coreografia de "revelado detras" (D-259, generalizada
     * aqui para cualquier llamador de este push generico) solo tiene
     * sentido cuando el destino es pantalla completa -- si el llamador
     * pasa `cover_drift_was_active=true` con un `width` que no es
     * A26_SCREEN_WIDTH (por ejemplo split->split), se ignora con
     * seguridad y sigue el push clasico de siempre. El contrato real
     * (cuando es seguro pasar `true`) es responsabilidad del llamador
     * -- documentado en aura_transitions.h -- pero este chequeo no
     * confia ciegamente en eso. Captura ANTES del paso 1 (el prerender
     * de la pantalla destino, mas abajo): si se hiciera despues, el
     * contenido que hay que preservar para el revelado ya se habria
     * perdido (aunque el prerender escribe a un buffer offscreen
     * aparte, no al framebuffer real -- se mantiene el mismo orden que
     * D-259 por consistencia, ver capture_outgoing_split_frame()). */
    use_reveal = cover_drift_was_active && (width == A26_SCREEN_WIDTH);
    if (use_reveal)
        capture_outgoing_split_frame();

    /* 1. Renderizar la pantalla nueva completa, una sola vez, al
     * framebuffer offscreen. a26_shell_clear_screen() limpia solo el
     * viewport activo (ver aura_theme.c), asi que el framebuffer real
     * -- con la pantalla vieja -- queda intacto. */
    viewport_set_defaults(&vp, SCREEN_MAIN);
    vp.x = 0;
    vp.y = 0;
    vp.width = A26_SCREEN_WIDTH;
    vp.height = A26_SCREEN_HEIGHT;
    viewport_set_buffer(&vp, &s_push_buffer, SCREEN_MAIN);

    saved = lcd_set_viewport(&vp);
    aura_screens_draw(nav);
    lcd_set_viewport(saved);

    new_split = aura_widgets_split_active();
    bar_changes = (old_split != new_split);

    /* 2. La barra de estado (correccion 2026-08-13, "transiciones que
     * se ven raras al salir del reproductor"):
     *
     *   - Mismo modo de barra (split->split, full->full): el titulo
     *     cambia AL INSTANTE y la banda no desliza -- comportamiento
     *     del firmware original ya verificado cuadro a cuadro (D-068).
     *   - (split)->(full): `Push-and-Drop` REAL (status-bar.md): la
     *     barra vieja se va EMPUJADA con el panel, durante el empuje no
     *     hay ninguna barra (hueco intencional) y la barra nueva CAE
     *     desde arriba al final. Antes se blitteaba la barra nueva
     *     entera antes del primer cuadro: aparecia asentada mientras su
     *     propio contenido todavia venia entrando.
     *   - (full)->(split): `Lift-and-Push`, la inversa temporal exacta
     *     -- el documento la marcaba "no definida" (vocabulario y
     *     status-bar.md) y por eso caia en el caso instantaneo, que
     *     ademas revelaba de golpe la cabecera del panel derecho. Fase
     *     1: la barra full se LEVANTA y sale por arriba. Fase 2: hueco.
     *     Fase 3: el push, con la barra (split) del destino entrando
     *     EMPUJADA junto a su panel, igual que se va en la ida.
     *
     * En los dos casos con cambio de modo la banda superior participa
     * del empuje (body_top = 0). */
    if (!bar_changes)
    {
        lcd_bitmap_part(s_push_fb, 0, 0, A26_SCREEN_WIDTH, 0, 0, width, bar_h);
        body_top = bar_h;
    }
    else
    {
        body_top = 0;
        if (!old_split)
        {
            /* Fase 1 de Lift-and-Push: la barra (full) sube y se va. */
            int lift_frames = (aura_settings.animation_mode == AURA_ANIM_ALL)
                ? AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_ALL
                : AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_MINIMAL;

            for (i = 1; i <= lift_frames; i++)
            {
                int dd = eased_offset(bar_h, i, lift_frames);

                for (y = 0; y + dd < bar_h; y++)
                    memcpy(FBADDR(0, y), FBADDR(0, y + dd),
                           A26_SCREEN_WIDTH * sizeof(fb_data));
                lcd_set_foreground(shell_bg);
                for (y = bar_h - dd; y < bar_h; y++)
                    lcd_hline(0, A26_SCREEN_WIDTH - 1, y);
                lcd_update_rect(0, 0, A26_SCREEN_WIDTH, bar_h);
                drain_button_queue_if_full();
                if (i < lift_frames)
                    sleep(frame_delay);
            }
        }
    }

    /* 3. Push por cuadros -- o, si `use_reveal`, el revelado de dos
     * bordes (D-259/D-261): `body_top` ya es 0 y `bar_changes &&
     * old_split` ya son ciertos en este caso (use_reveal exige
     * width==A26_SCREEN_WIDTH, y el llamador solo pasa
     * cover_drift_was_active=true cuando el origen era SPLIT), asi que
     * reveal_behind_panels() reproduce exactamente el mismo hueco de
     * barra que el push clasico ya dejaba para este caso -- ningun
     * comportamiento de barra distinto entre ambos caminos.
     * D-267 (encargo del dueno, "la transicion deberia ser exactamente
     * la misma pero invertida al retroceder"): `direction` distingue
     * entrada (paneles ENCOGEN revelando el destino) de salida (paneles
     * CRECEN cubriendo lo saliente) -- mismo hueco de barra en ambos
     * casos (la Fase 1 de Lift-and-Push arriba ya corrio identica),
     * distinta unicamente la geometria del contenido. */
    if (use_reveal)
    {
        if (direction > 0)
            reveal_behind_panels(AURA_DS_METRICS_LEFT_PANEL_WIDTH, bar_h,
                                  shell_bg, frames, frame_delay);
        else
            reveal_behind_panels_exit(AURA_DS_METRICS_LEFT_PANEL_WIDTH, bar_h,
                                       shell_bg, frames, frame_delay);
    }
    else
    {
        for (i = 1; i <= frames; i++)
        {
            int d = eased_offset(width, i, frames);
            int delta = d - d_prev;

            if (delta > 0)
            {
                for (y = body_top; y < A26_SCREEN_HEIGHT; y++)
                {
                    fb_data *row = FBADDR(0, y);

                    if (direction > 0)
                        /* La vieja sale por la izquierda: el remanente
                         * [0, width-d) viene de lo que estaba `delta`
                         * pixeles a la derecha. */
                        memmove(row, row + delta,
                                (width - d) * sizeof(fb_data));
                    else
                        /* La vieja sale por la derecha: el remanente
                         * [d, width) viene de lo que estaba `delta`
                         * pixeles a la izquierda. */
                        memmove(row + d, row + d_prev,
                                (width - d) * sizeof(fb_data));
                }

                if (direction > 0)
                    /* La nueva entra desde el borde derecho: sus columnas
                     * [0, d) aparecen en x = [width-d, width). */
                    lcd_bitmap_part(s_push_fb, 0, body_top, A26_SCREEN_WIDTH,
                                    width - d, body_top, d,
                                    A26_SCREEN_HEIGHT - body_top);
                else
                    /* La nueva entra desde el borde izquierdo: sus columnas
                     * [width-d, width) aparecen en x = [0, d). */
                    lcd_bitmap_part(s_push_fb, width - d, body_top,
                                    A26_SCREEN_WIDTH, 0, body_top, d,
                                    A26_SCREEN_HEIGHT - body_top);

                /* Hueco intencional del Push-and-Drop: hacia (full), la
                 * banda de la barra queda VACIA durante todo el empuje --
                 * la barra nueva no existe todavia, cae al final. */
                if (bar_changes && old_split)
                {
                    lcd_set_foreground(shell_bg);
                    for (y = 0; y < bar_h; y++)
                        lcd_hline(direction > 0 ? width - d : 0,
                                  direction > 0 ? width - 1 : d - 1, y);
                }
            }

            lcd_update_rect(0, 0, width, A26_SCREEN_HEIGHT);
            drain_button_queue_if_full();

            d_prev = d;
            if (i < frames)
                sleep(frame_delay);
        }
    }

    /* Fase 3 del Push-and-Drop hacia (full): la barra nueva CAE desde
     * arriba una vez que el empuje termino por completo. */
    if (bar_changes && old_split)
    {
        int drop_frames = (aura_settings.animation_mode == AURA_ANIM_ALL)
                ? AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_ALL
                : AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_MINIMAL;

        for (i = 1; i <= drop_frames; i++)
        {
            int dd = eased_offset(bar_h, i, drop_frames);

            if (dd > 0)
                lcd_bitmap_part(s_push_fb, 0, bar_h - dd, A26_SCREEN_WIDTH,
                                0, 0, A26_SCREEN_WIDTH, dd);
            lcd_update_rect(0, 0, A26_SCREEN_WIDTH, bar_h);
            drain_button_queue_if_full();
            if (i < drop_frames)
                sleep(frame_delay);
        }
    }

    TRANSITION_LOG("push", frames, start_tick);
}

/* Entrada a Cover Flow (coreografia del dueno del diseno, 2026-08-12,
 * caso SIN CoverDrift -- se mantiene identica, sin cambios, para
 * cualquier entrada a Coverflow donde CoverDrift no estaba activo):
 * LeftPanel Y su StatusBar (split) salen empujados hacia la izquierda
 * ("la barra de estado se va junto con el panel izquierdo"), mientras
 * la pantalla del Cover Flow entra desde el borde derecho POR ENCIMA
 * del SelectionSummary, que se queda quieto debajo hasta ser cubierto.
 * Cuando el contenido termino de entrar, la StatusBar (full) del Cover
 * Flow entra CAYENDO desde arriba -- el mismo Push-and-Drop que
 * status-bar.md describe para (split)->(full).
 *
 * Variante CON CoverDrift (D-259 -- documentada por el dueno del
 * diseno el 2026-08-12 para cuando CoverDrift existiera, implementada
 * ahora que existe, D-254 a D-258; PRUEBA ACOTADA a proposito SOLO a
 * esta entrada -- encargo explicito del dueno del producto, antes de
 * replicarla en cualquier otro punto de entrada a pantalla completa):
 * el panel derecho TAMBIEN sale, hacia la derecha, y el Cover Flow se
 * revela DETRAS de ambos paneles, renderizado desde el primer cuadro
 * (no entra desde un borde -- ya esta ahi, quieto, todo el tiempo).
 * Mismo Drop de StatusBar en la Fase 2, sin cambios, para ambos
 * casos. D-261: la captura/revelado con CoverDrift ahora vive en
 * capture_outgoing_split_frame()/reveal_behind_panels() (arriba en
 * este archivo), compartidas con aura_transition_slide() -- esta
 * funcion es un llamador mas, sin cambio de comportamiento propio. */
void aura_transition_coverflow_enter(aura_nav_t *nav, bool cover_drift_was_active)
{
    int frames, frame_delay, i, y;
    int d_prev = 0;
    const int panel_w = AURA_DS_METRICS_LEFT_PANEL_WIDTH;
    const int bar_h = A26_LAYOUT_STATUSBAR_HEIGHT;
    struct viewport vp;
    struct viewport *saved;
    unsigned bg = a26_color(A26_SHELL_BG);
    long start_tick = current_tick;

    /* AUDITORIA-01 A-08, mismo criterio que aura_transition_slide(). */
    if (!lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE)
        return;

    if (aura_settings.animation_mode == AURA_ANIM_ALL)
    {
        frames = AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FRAMES_ALL;
        frame_delay = HZ / AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FPS_ALL;
    }
    else /* AURA_ANIM_MINIMAL */
    {
        frames = AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FRAMES_MINIMAL;
        frame_delay = HZ / AURA_DS_METRICS_PUSH_AND_DROP_PUSH_FPS_MINIMAL;
    }

    /* D-259/D-261: captura EXACTA de lo que hay en pantalla ahora mismo
     * (LeftPanel + panel derecho con CoverDrift) -- tiene que ser ANTES
     * de renderizar Cover Flow al framebuffer real (mas abajo), o ese
     * contenido real se pierde. Solo hace falta en el caso CON
     * CoverDrift -- el caso SIN el sigue leyendo el framebuffer en vivo
     * via memmove, sin cambios. */
    if (cover_drift_was_active)
        capture_outgoing_split_frame();

    /* Cover Flow completo (con su barra full incluida) renderizado UNA
     * vez al framebuffer offscreen -- misma mecanica que el push. La
     * franja de su barra (y < bar_h) NO se blitea en la fase de
     * deslizamiento: cae despues, en la fase Drop. */
    viewport_set_defaults(&vp, SCREEN_MAIN);
    vp.x = 0;
    vp.y = 0;
    vp.width = A26_SCREEN_WIDTH;
    vp.height = A26_SCREEN_HEIGHT;
    viewport_set_buffer(&vp, &s_push_buffer, SCREEN_MAIN);

    saved = lcd_set_viewport(&vp);
    aura_screens_draw(nav);
    lcd_set_viewport(saved);

    if (cover_drift_was_active)
    {
        /* D-259/D-261: ambos paneles salen hacia SU borde, revelando
         * Cover Flow (ya renderizado completo, quieto) segun se abren
         * -- ver reveal_behind_panels() arriba en este archivo. */
        reveal_behind_panels(panel_w, bar_h, bg, frames, frame_delay);
    }
    else
    {
        /* Fase 1 original, sin cambios: deslizamiento simultaneo. El
         * panel izquierdo (barra split incluida: TODAS las filas de
         * x < panel_w) recorre panel_w px; el Cover Flow recorre la
         * pantalla completa en el mismo numero de cuadros (entra "al
         * doble de velocidad", asi ambos terminan juntos). El hueco
         * entre el panel saliente y el contenido entrante muestra el
         * fondo del shell -- el SelectionSummary sigue visible debajo
         * hasta que el contenido lo cubre. */
        for (i = 1; i <= frames; i++)
        {
            int d = eased_offset(panel_w, i, frames);          /* salida del panel */
            int d_cf = eased_offset(A26_SCREEN_WIDTH, i, frames); /* entrada del CF */
            int delta = d - d_prev;
            int cf_left = A26_SCREEN_WIDTH - d_cf;

            for (y = 0; y < A26_SCREEN_HEIGHT; y++)
            {
                fb_data *row = FBADDR(0, y);

                /* Panel izquierdo saliendo (remanente [0, panel_w-d)). */
                if (delta > 0 && d < panel_w)
                    memmove(row, row + delta, (panel_w - d) * sizeof(fb_data));

                /* Hueco revelado tras el panel: fondo plano. */
                if (d > 0 && panel_w - d < panel_w && cf_left > panel_w - d)
                {
                    int gap_end = cf_left < panel_w ? cf_left : panel_w;
                    int gx;
                    for (gx = panel_w - d; gx < gap_end; gx++)
                        row[gx] = bg;
                }
            }

            /* Cover Flow entrando desde la derecha (solo el cuerpo, la
             * barra cae en la fase 2): sus columnas [0, d_cf) aparecen en
             * x = [320-d_cf, 320). La franja superior queda en fondo plano
             * mientras tanto. */
            if (d_cf > 0)
            {
                lcd_bitmap_part(s_push_fb, 0, bar_h, A26_SCREEN_WIDTH,
                                cf_left, bar_h, d_cf, A26_SCREEN_HEIGHT - bar_h);
                for (y = 0; y < bar_h; y++)
                {
                    fb_data *row = FBADDR(cf_left, y);
                    int gx;
                    for (gx = 0; gx < d_cf; gx++)
                        row[gx] = bg;
                }
            }

            lcd_update();
            drain_button_queue_if_full();
            d_prev = d;
            if (i < frames)
                sleep(frame_delay);
        }
    }

    /* Fase 2: Drop de la StatusBar (full) -- entra cayendo desde arriba
     * una vez que el contenido termino de entrar (status-bar.md,
     * Push-and-Drop, paso 3). En el cuadro `i`, las primeras `dd` filas
     * de pantalla muestran las ULTIMAS `dd` filas de la barra (la barra
     * asomando desde el borde superior). IDENTICA en ambos casos, sin
     * cambios. */
    {
        int drop_frames = (aura_settings.animation_mode == AURA_ANIM_ALL)
                ? AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_ALL
                : AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_MINIMAL;

        for (i = 1; i <= drop_frames; i++)
        {
            int dd = eased_offset(bar_h, i, drop_frames);

            if (dd > 0)
                lcd_bitmap_part(s_push_fb, 0, bar_h - dd, A26_SCREEN_WIDTH,
                                0, 0, A26_SCREEN_WIDTH, dd);

            lcd_update_rect(0, 0, A26_SCREEN_WIDTH, bar_h);
            drain_button_queue_if_full();
            if (i < drop_frames)
                sleep(frame_delay);
        }
    }

    TRANSITION_LOG("coverflow-enter", frames, start_tick);
}

/* -- Vuelo Cover Flow -> reproductor (rehecho 2026-08-12 por encargo
 * del dueno del diseno, reemplaza al Flip-and-Flow de T3.2(d)/D-113):
 *
 * Arranca desde el REVERSO CRECIDO (200px, la lista de canciones
 * visible) y hace MEDIA VUELTA continua en un solo sentido (decision
 * del dueno: media vuelta directa, ~500ms) mientras encoge a los
 * 135px del reproductor y viaja a su posicion exacta:
 *
 *   Fase A (primera mitad): el PANEL REAL de la lista -- capturado
 *   del framebuffer, con todo y sus canciones -- gira de frente a
 *   perfil.
 *   Fase B (segunda mitad): la CARATULA entra desde el perfil opuesto
 *   (misma direccion de giro, rotacion continua) y aterriza en el
 *   tilt/posicion del reproductor con su reflejo apareciendo en
 *   proporcion.
 *
 * Mientras tanto (primera mitad), las tapas laterales salen
 *   deslizandose hacia SU borde (aura_coverflow_draw_exit_frame).
 *
 * El tamano interpola de forma continua via la camara del proyector
 * (distance = CAM * fuente/visual - CAM), igual que el zoom del flip
 * (D-118). Al aterrizar corre el morph de entrada del reproductor
 * (now-playing.md, D-113) y el push de navegacion. Sincrono y
 * bloqueante como el resto de las transiciones. */
#define FLOW_MS 500 /* decision del dueno del diseno (antes 350 provisional) */

static fb_data s_back_tex[AURA_COVERFLOW_BACK_SIZE * AURA_COVERFLOW_BACK_SIZE];

void aura_transition_flip_and_flow(aura_nav_t *nav, int32_t album_seek)
{
    int size = AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE;
    int refl_h = aura_art_reflection_height(size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT);
    /* D-226 (encargo del dueno, 2026-08-14: freeze real al reproducir
     * desde Cover Flow, sospecha propia del dueno de que era la
     * transicion coverflow->reproductor -- confirmado). Estos dos
     * buffers eran locales de PILA: 135*135*2 (cover_buf) +
     * 135*33*2 (refl_buf) = 45360 bytes, ~44KB, en una sola llamada.
     * El hilo que corre esta funcion (el hilo "main" de Rockbox --
     * arranca en crt0.S y jamas pasa por create_thread() con un stack
     * propio, ver firmware/target/arm/s5l8702/app.lds) tiene una
     * region `.stack` de exactamente 0x2000 = 8192 bytes en IRAM,
     * INMEDIATAMENTE seguida por los stacks de IRQ y FIQ en el mismo
     * mapa de memoria. Una sola llamada a esta funcion desbordaba ese
     * stack casi 6 veces sobre su tamano, pisando IRQ/FIQ y lo que
     * sea que venga despues en IRAM -- corrupcion de memoria real, no
     * un bug logico, consistente con un freeze que exige apagar y
     * prender el iPod para recuperarse (nunca un panic legible: la
     * corrupcion pasa antes de que cualquier codigo pueda detectarla).
     * `static` (mismo patron que col_buf/rcol_buf, dos lineas mas
     * abajo, y que aura_screens.c:draw_album_list ya usaba a proposito
     * para esto mismo) los saca de la pila -- pasan a vivir en BSS,
     * ~44KB sobre los 64MB (MEMORYSIZE) del iPod 6G. Seguro reutilizar
     * la misma memoria en cada llamada: la funcion llena el buffer y
     * lo consume por completo antes de retornar, nunca reentrante (un
     * solo hilo de UI, disparada por botones, sin recursion). */
    static unsigned char cover_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * sizeof(fb_data)];
    static unsigned char refl_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE
                            * (AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT / 100)
                            * sizeof(fb_data)];
    static fb_data col_buf[AURA_COVERFLOW_BACK_SIZE + 8];
    static fb_data rcol_buf[AURA_COVERFLOW_BACK_SIZE / 2];
    aura_albumart_t art;
    int frames, frame_delay, i, row, col;
    long start_tick = current_tick;
    int back = AURA_COVERFLOW_BACK_SIZE;
    /* Centros de despegue (reverso) y aterrizaje (reproductor). */
    int y0 = AURA_COVERFLOW_BACK_Y + back / 2;
    int y1 = AURA_DS_METRICS_NOW_PLAYING_COVER_Y + size / 2;
    unsigned bg = a26_color(A26_SHELL_BG);

    if (!lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE)
    {
        aura_nav_push(nav, AURA_SCREEN_NOWPLAYING);
        return;
    }

    /* Textura del reverso: lo que este AHORA en pantalla (el panel con
     * su lista real), transpuesto para el walker de columnas. */
    for (row = 0; row < back; row++)
    {
        const fb_data *src = FBADDR(AURA_COVERFLOW_BACK_X, AURA_COVERFLOW_BACK_Y + row);
        for (col = 0; col < back; col++)
            s_back_tex[(size_t)col * back + row] = src[col];
    }

    art.size = size;
    art.radius = AURA_DS_METRICS_COVER_FLOW_CORNER_RADIUS;
    art.cover_data = cover_buf;
    art.reflection_data = refl_buf;
    if (!aura_albumart_load_for_album(album_seek, &art))
        aura_albumart_load_default(&art);

    if (aura_settings.animation_mode == AURA_ANIM_ALL)
    {
        frames = FLOW_MS * 60 / 1000;
        frame_delay = HZ / 60;
    }
    else /* AURA_ANIM_MINIMAL */
    {
        frames = FLOW_MS * 30 / 1000;
        frame_delay = HZ / 30;
    }

    for (i = 1; i <= frames; i++)
    {
        int t = eased_offset(256, i, frames);       /* 0..256, ease-out */
        int size_vis = back - ((back - size) * t) / 256; /* 200 -> 135 */
        int cy = aura_pattern_lerp(y0, y1, t);
        int cx = aura_pattern_lerp(0, AURA_NOWPLAYING_TILT_CX, t);
        aura_flow_slide_t slide;
        aura_flow_projection_t proj;
        int phase_b = (t >= 128);
        int tp = phase_b ? (t - 128) * 2 : t * 2;   /* progreso de la fase, 0..256 */
        int src_size = phase_b ? size : back;
        const fb_data *tex = phase_b ? (const fb_data *)art.cover_data : s_back_tex;
        int refl_vis = phase_b ? tp : 0;

        /* Media vuelta continua: fase A gira el panel 0->90 (perfil);
         * fase B trae la caratula del perfil opuesto -90->tilt final.
         * Mismo sentido de giro que la apertura del flip: toda la vida
         * del album gira hacia el mismo lado. */
        slide.angle = phase_b
            ? aura_pattern_lerp(-256, AURA_NOWPLAYING_TILT_IANGLE, tp)
            : aura_pattern_lerp(0, 256, tp);
        slide.distance = AURA_FLOW_CAM_DIST * src_size / size_vis - AURA_FLOW_CAM_DIST;
        slide.cx = cx;

        /* Fondo: laterales saliendo hacia sus bordes (primera mitad) y
         * la barra full del destino -- el morph de entrada de abajo se
         * encarga del resto del chrome del reproductor. */
        aura_coverflow_draw_exit_frame(phase_b ? 256 : tp);

        aura_flow_begin_projection(&proj, &slide, src_size);
        while (proj.screen_x < AURA_FLOW_SCREEN_W)
        {
            int scol = aura_flow_source_column(&proj);
            int dy = aura_flow_vertical_scale(&proj);
            int p = 0, dest_row, n_rows = 0;
            const fb_data *tex_col = tex + (size_t)scol * src_size;
            int cover_disp = (src_size << AURA_FLOW_SHIFT) / dy;
            int y_col = cy - cover_disp / 2;

            for (dest_row = 0; dest_row < (int)(sizeof(col_buf) / sizeof(col_buf[0])); dest_row++)
            {
                int source_row = p >> AURA_FLOW_SHIFT;

                if (source_row >= src_size)
                    break;
                col_buf[dest_row] = tex_col[source_row];
                p += dy;
                n_rows++;
            }
            if (n_rows > 0)
                lcd_bitmap(col_buf, proj.screen_x, y_col, 1, n_rows);

            if (refl_vis > 0)
            {
                const fb_data *refl_col = (const fb_data *)art.reflection_data
                                         + (size_t)scol * refl_h;
                int r_rows = 0;

                p = 0;
                for (dest_row = 0; dest_row < (int)(sizeof(rcol_buf) / sizeof(rcol_buf[0])); dest_row++)
                {
                    int source_row = p >> AURA_FLOW_SHIFT;

                    if (source_row >= refl_h)
                        break;
                    rcol_buf[dest_row] = a26_shell_blend(bg, refl_col[source_row], refl_vis);
                    p += dy;
                    r_rows++;
                }
                if (r_rows > 0)
                    lcd_bitmap(rcol_buf, proj.screen_x, y_col + n_rows, 1, r_rows);
            }

            if (!aura_flow_advance_column(&proj))
                break;
        }

        lcd_update();
        drain_button_queue_if_full();
        if (i < frames)
            sleep(frame_delay);
    }

    aura_nav_push(nav, AURA_SCREEN_NOWPLAYING);

    /* -- Morph de entrada (now-playing.md, "Entrada desde CoverFlow"):
     * la caratula es el unico elemento que ya esta en su lugar (llego
     * con el vuelo de arriba); el resto entra por grupos, cada uno con
     * su coreografia confirmada por el documento:
     *
     *   titulos/datos de la cancion  -> fade in
     *   iconos de modos              -> desde la derecha
     *   barra de progreso            -> desde abajo
     *   StatusBar                    -> desde arriba (su Drop estandar)
     *
     * Orden y duracion del stagger RATIFICADOS por el dueno
     * (2026-08-16, D-274; nacieron provisionales en D-113): los tres
     * grupos de contenido entran EN PARALELO (mismos cuadros) y la
     * barra cae AL FINAL, consistente con Push-and-Drop, donde la
     * barra siempre entra despues del contenido.
     *
     * Mecanica: NowPlaying completo se renderiza UNA vez al offscreen
     * (la caratula ahi es identica a la ya dibujada -- requisito duro
     * de continuidad del documento, mismas constantes de geometria);
     * cada grupo se compone por region desde ese buffer. El fade es
     * blend(bg -> destino) porque el estado inicial de esas regiones
     * tras el vuelo es fondo plano. */
    {
        struct viewport mvp;
        struct viewport *msaved;
        unsigned bg = a26_color(A26_SHELL_BG);
        const int bar_h = A26_LAYOUT_STATUSBAR_HEIGHT;
        /* Limites DERIVADOS del layout real (correccion 2026-08-12:
         * con 160/200 fijos, los primeros pixeles del titulo/album
         * (TEXT_X=157) quedaban tapados por el fondo hasta el redibujo
         * final -- notorio en tema oscuro -- y el tope de la barra
         * (y=196) se perdia el morph). */
        const int right_x = AURA_DS_METRICS_NOW_PLAYING_COVER_X
                            + AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE + 1;
        const int bottom_y = A26_SCREEN_HEIGHT
                             - AURA_DS_METRICS_NOW_PLAYING_PROGRESS_BOTTOM_OFFSET;
        const int mode_h = 22;            /* fila de modos: iconos de 20px + margen */
        int mode_y, morph_frames, x, yy;

        viewport_set_defaults(&mvp, SCREEN_MAIN);
        mvp.x = 0;
        mvp.y = 0;
        mvp.width = A26_SCREEN_WIDTH;
        mvp.height = A26_SCREEN_HEIGHT;
        viewport_set_buffer(&mvp, &s_push_buffer, SCREEN_MAIN);
        msaved = lcd_set_viewport(&mvp);
        aura_screens_draw(nav);
        lcd_set_viewport(msaved);

        mode_y = aura_nowplaying_last_mode_row_y();
        morph_frames = (aura_settings.animation_mode == AURA_ANIM_ALL) ? 8 : 4;

        for (i = 1; i <= morph_frames; i++)
        {
            int prog = eased_offset(256, i, morph_frames); /* 0..256 */
            int dx = (60 * (256 - prog)) / 256;   /* modos: desplazamiento restante */
            int dyo = ((A26_SCREEN_HEIGHT - bottom_y) * (256 - prog)) / 256;

            /* Fade in de textos/datos: columna derecha, salvo la fila
             * de modos (que entra deslizando, abajo). */
            for (yy = bar_h; yy < bottom_y; yy++)
            {
                fb_data *dst;
                const fb_data *src;

                if (yy >= mode_y && yy < mode_y + mode_h)
                    continue;
                dst = FBADDR(right_x, yy);
                src = &s_push_fb[yy * A26_SCREEN_WIDTH + right_x];
                for (x = 0; x < A26_SCREEN_WIDTH - right_x; x++)
                    dst[x] = a26_shell_blend(bg, src[x], prog);
            }

            /* Iconos de modos desde la derecha. */
            for (yy = mode_y; yy < mode_y + mode_h && yy < bottom_y; yy++)
            {
                fb_data *dst = FBADDR(right_x, yy);
                const fb_data *src = &s_push_fb[yy * A26_SCREEN_WIDTH + right_x];
                int span = A26_SCREEN_WIDTH - right_x;
                for (x = 0; x < span; x++)
                    dst[x] = (x >= dx) ? src[x - dx] : bg;
            }

            /* Barra de progreso (y transporte) desde abajo -- lo que
             * queda por entrar deja ver lo que ya habia (la cola del
             * reflejo), nunca un borrado. */
            if (dyo < A26_SCREEN_HEIGHT - bottom_y)
                lcd_bitmap_part(s_push_fb, 0, bottom_y, A26_SCREEN_WIDTH,
                                0, bottom_y + dyo, A26_SCREEN_WIDTH,
                                A26_SCREEN_HEIGHT - bottom_y - dyo);

            lcd_update();
            drain_button_queue_if_full();
            if (i < morph_frames)
                sleep(frame_delay);
        }

        /* StatusBar (full) cayendo desde arriba, al final -- mismo Drop
         * que la entrada a Cover Flow. */
        {
            int drop_frames = (aura_settings.animation_mode == AURA_ANIM_ALL)
                ? AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_ALL
                : AURA_DS_METRICS_PUSH_AND_DROP_DROP_FRAMES_MINIMAL;

            for (i = 1; i <= drop_frames; i++)
            {
                int dd = eased_offset(bar_h, i, drop_frames);

                if (dd > 0)
                    lcd_bitmap_part(s_push_fb, 0, bar_h - dd, A26_SCREEN_WIDTH,
                                    0, 0, A26_SCREEN_WIDTH, dd);
                lcd_update_rect(0, 0, A26_SCREEN_WIDTH, bar_h);
                drain_button_queue_if_full();
                if (i < drop_frames)
                    sleep(frame_delay);
            }
        }
    }

    TRANSITION_LOG("flow-to-player", frames, start_tick);
}

/* -- Regreso reproductor -> Cover Flow (encargo 2026-08-12): la
 * caratula YA esta de frente en el reproductor, asi que no hay giro ni
 * paso por el reverso -- es un MORPH fluido de posicion y geometria
 * (135px/tilt 7 -> 130px/frontal en el centro del carrusel), con el
 * reflejo visible todo el trayecto. Las tapas laterales entran desde
 * los bordes de la pantalla y el titulo/artista suben desde el borde
 * inferior (aura_coverflow_draw_return_frame). Al aterrizar, el
 * carrusel queda en reposo (settle_idle) mostrando la caratula. */
void aura_transition_flow_return(aura_nav_t *nav)
{
    int size = AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE;
    int carousel = AURA_DS_METRICS_COVER_FLOW_CENTER_SLIDE_SIZE;
    int refl_h = aura_art_reflection_height(size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT);
    /* D-226: mismo desborde de pila que aura_transition_flip_and_flow()
     * de arriba (~44KB de cover_buf/refl_buf contra un stack de hilo
     * "main" de solo 8KB) -- static por la misma razon. */
    static unsigned char cover_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * sizeof(fb_data)];
    static unsigned char refl_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE
                            * (AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT / 100)
                            * sizeof(fb_data)];
    static fb_data col_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE + 8];
    static fb_data rcol_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE / 2];
    aura_albumart_t art;
    int frames, frame_delay, i;
    long start_tick = current_tick;
    int32_t seek = aura_coverflow_current_album_seek();
    int y0 = AURA_DS_METRICS_NOW_PLAYING_COVER_Y + size / 2;
    int y1 = 30 + carousel / 2; /* CF_TOP_Y + centro -- misma linea media del carrusel */

    (void)nav; /* el pop ya ocurrio; nav queda en la firma por simetria
                * con el resto de las transiciones */
    aura_coverflow_settle_idle();

    if (!lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE || seek < 0)
        return;

    art.size = size;
    art.radius = AURA_DS_METRICS_COVER_FLOW_CORNER_RADIUS;
    art.cover_data = cover_buf;
    art.reflection_data = refl_buf;
    if (!aura_albumart_load_for_album(seek, &art))
        aura_albumart_load_default(&art);

    if (aura_settings.animation_mode == AURA_ANIM_ALL)
    {
        frames = FLOW_MS * 60 / 1000;
        frame_delay = HZ / 60;
    }
    else
    {
        frames = FLOW_MS * 30 / 1000;
        frame_delay = HZ / 30;
    }

    for (i = 1; i <= frames; i++)
    {
        int t = eased_offset(256, i, frames);
        int size_vis = size - ((size - carousel) * t) / 256; /* 135 -> 130 */
        int cy = aura_pattern_lerp(y0, y1, t);
        aura_flow_slide_t slide;
        aura_flow_projection_t proj;

        slide.angle = aura_pattern_lerp(AURA_NOWPLAYING_TILT_IANGLE, 0, t);
        slide.distance = AURA_FLOW_CAM_DIST * size / size_vis - AURA_FLOW_CAM_DIST;
        slide.cx = aura_pattern_lerp(AURA_NOWPLAYING_TILT_CX, 0, t);

        aura_coverflow_draw_return_frame(t);

        aura_flow_begin_projection(&proj, &slide, size);
        while (proj.screen_x < AURA_FLOW_SCREEN_W)
        {
            int scol = aura_flow_source_column(&proj);
            int dy = aura_flow_vertical_scale(&proj);
            int p = 0, dest_row, n_rows = 0;
            const fb_data *tex_col = (const fb_data *)art.cover_data + (size_t)scol * size;
            const fb_data *refl_col = (const fb_data *)art.reflection_data + (size_t)scol * refl_h;
            int cover_disp = (size << AURA_FLOW_SHIFT) / dy;
            int y_col = cy - cover_disp / 2;
            int r_rows = 0;

            for (dest_row = 0; dest_row < (int)(sizeof(col_buf) / sizeof(col_buf[0])); dest_row++)
            {
                int source_row = p >> AURA_FLOW_SHIFT;

                if (source_row >= size)
                    break;
                col_buf[dest_row] = tex_col[source_row];
                p += dy;
                n_rows++;
            }
            if (n_rows > 0)
                lcd_bitmap(col_buf, proj.screen_x, y_col, 1, n_rows);

            p = 0;
            for (dest_row = 0; dest_row < (int)(sizeof(rcol_buf) / sizeof(rcol_buf[0])); dest_row++)
            {
                int source_row = p >> AURA_FLOW_SHIFT;

                if (source_row >= refl_h)
                    break;
                rcol_buf[dest_row] = refl_col[source_row];
                p += dy;
                r_rows++;
            }
            if (r_rows > 0)
                lcd_bitmap(rcol_buf, proj.screen_x, y_col + n_rows, 1, r_rows);

            if (!aura_flow_advance_column(&proj))
                break;
        }

        /* Textos AL FINAL del cuadro: encima del reflejo, como en el
         * carrusel en reposo -- sin salto de capas al aterrizar. */
        aura_coverflow_draw_return_texts(t);

        lcd_update();
        drain_button_queue_if_full();
        if (i < frames)
            sleep(frame_delay);
    }

    TRANSITION_LOG("flow-return", frames, start_tick);
}
