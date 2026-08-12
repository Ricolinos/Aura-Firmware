#include <string.h>

#include "lcd.h"
#include "kernel.h"
#include "tick.h"
#include "gui/viewport.h"
#include "debug.h"
#include "button.h"

#include "aura_transitions.h"
#include "aura_screens.h"
#include "aura_settings.h"
#include "apple2026_tokens.h"
#include "aura_flow.h"
#include "aura_patterns.h"
#include "aura_albumart.h"
#include "aura_art.h"
#include "aura_nowplaying.h"
#include "aura_statusbar.h"
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

void aura_transition_slide(aura_nav_t *nav, int direction, int width)
{
    int frames, frame_delay, i, y;
    int d_prev = 0;
    const int bar_h = A26_LAYOUT_STATUSBAR_HEIGHT;
    struct viewport vp;
    struct viewport *saved;
    long start_tick = current_tick;

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
        frames = 8;
        frame_delay = HZ / 60;
    }
    else /* AURA_ANIM_MINIMAL */
    {
        frames = 4;
        frame_delay = HZ / 45;
    }

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

    /* 2. Titulo instantaneo: la banda de la barra de estado no
     * desliza -- se blitea entera antes del primer cuadro. */
    lcd_bitmap_part(s_push_fb, 0, 0, A26_SCREEN_WIDTH, 0, 0, width, bar_h);

    /* 3. Push por cuadros sobre la banda del cuerpo (y >= bar_h). */
    for (i = 1; i <= frames; i++)
    {
        int d = eased_offset(width, i, frames);
        int delta = d - d_prev;

        if (delta > 0)
        {
            for (y = bar_h; y < A26_SCREEN_HEIGHT; y++)
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
                lcd_bitmap_part(s_push_fb, 0, bar_h, A26_SCREEN_WIDTH,
                                width - d, bar_h, d,
                                A26_SCREEN_HEIGHT - bar_h);
            else
                /* La nueva entra desde el borde izquierdo: sus columnas
                 * [width-d, width) aparecen en x = [0, d). */
                lcd_bitmap_part(s_push_fb, width - d, bar_h,
                                A26_SCREEN_WIDTH, 0, bar_h, d,
                                A26_SCREEN_HEIGHT - bar_h);
        }

        lcd_update_rect(0, 0, width, A26_SCREEN_HEIGHT);
        drain_button_queue_if_full();

        d_prev = d;
        if (i < frames)
            sleep(frame_delay);
    }

    TRANSITION_LOG("push", frames, start_tick);
}

/* T4 (Coverflow, L4): mismo truco seguro de "wipe" que aura_transition_slide
 * (D-030) -- nunca se instala un viewport fuera de [0, A26_SCREEN_WIDTH] --
 * pero revelando desde AMBOS bordes hacia el centro en vez de desde uno
 * solo, para que la entrada a Coverflow se sienta distinta de una
 * navegacion de lista comun ("el contenido emerge de detras", L4). */
void aura_transition_reveal(aura_nav_t *nav)
{
    int frames, frame_delay, i;
    struct viewport vp;
    struct viewport *saved;
    long start_tick = current_tick;

    /* AUDITORIA-01 A-08, mismo criterio que aura_transition_slide(). */
    if (!lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE)
        return;

    if (aura_settings.animation_mode == AURA_ANIM_ALL)
    {
        frames = 10;
        frame_delay = HZ / 60;
    }
    else /* AURA_ANIM_MINIMAL */
    {
        frames = 4;
        frame_delay = HZ / 45;
    }

    for (i = 1; i <= frames; i++)
    {
        int half = (A26_SCREEN_WIDTH / 2 * i) / frames;

        viewport_set_defaults(&vp, SCREEN_MAIN);
        vp.y = 0;
        vp.height = A26_SCREEN_HEIGHT;

        /* Mitad izquierda: revela desde x=0 hacia el centro. */
        vp.x = 0;
        vp.width = half;
        saved = lcd_set_viewport(&vp);
        aura_screens_draw(nav);
        lcd_set_viewport(saved);

        /* Mitad derecha: revela desde el borde derecho hacia el
         * centro. x+width se mantiene siempre en A26_SCREEN_WIDTH. */
        vp.x = A26_SCREEN_WIDTH - half;
        vp.width = half;
        saved = lcd_set_viewport(&vp);
        aura_screens_draw(nav);
        lcd_set_viewport(saved);

        lcd_update();
        drain_button_queue_if_full();

        if (i < frames)
            sleep(frame_delay);
    }

    TRANSITION_LOG("reveal", frames, start_tick);
}

/* -- Flip-and-Flow (PLAN.md T3.2(d), componentes/cover-flow.md) --------
 *
 * "El album gira de nuevo, se posiciona en su lugar, y desde ahi
 * transicion fluida y continua hacia Now Playing -- con todo y
 * reflejo durante el trayecto." La caratula se decodifica UNA vez a
 * su tamano FINAL (135px, el de NowPlaying -- misma cache .pfraw que
 * el carrusel, T3.2(a), solo que con clave de tamano distinta) y se
 * anima con angulo/posicion CONTINUOS (aura_pattern_lerp, T1.1) desde
 * la geometria de reposo del carrusel (angulo 0, centrada) hasta la
 * geometria EXACTA de NowPlaying -- `AURA_NOWPLAYING_TILT_IANGLE`/
 * `_CX` son publicas en aura_nowplaying.h precisamente porque el
 * documento acopla ambas geometrias ("si se cambia una, se cambia la
 * otra"): esta funcion nunca inventa su propio destino, lee el mismo
 * numero que ya usa aura_nowplaying.c.
 *
 * Corte de alcance real, mismo criterio que D-101 (morph de Modo 4 en
 * NowPlaying): el TAMANO no interpola -- 135px fijo durante todo el
 * vuelo, un solo salto de tamano en el primer cuadro (la caratula de
 * 100px del carrusel es un archivo de cache DISTINTO, mismo album,
 * distinto pre-escalado). Este pipeline no tiene una primitiva de
 * reescalado de bitmaps en tiempo real -- interpolar el tamano de
 * verdad la exigiria. Angulo y posicion SI son continuos y reales.
 *
 * Sincrono y bloqueante, mismo estilo que aura_transition_slide()/
 * aura_transition_reveal() de arriba -- una coreografia de un solo
 * disparo que nunca se interrumpe ni se redirige a mitad de camino,
 * mas simple de razonar que integrarla al bucle de redibujo por
 * eventos que usa el resto de las animaciones de esta sesion. */
#define CF_FLOW_MS 350 /* TODO(pendiente-doc): "Timing de cada fase de Flip-and-Flow" no definido, ver DECISIONS.md D-105 */

void aura_transition_flip_and_flow(aura_nav_t *nav, int32_t album_seek, int from_y)
{
    int size = AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE;
    int refl_h = aura_art_reflection_height(size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT);
    unsigned char cover_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * sizeof(fb_data)];
    unsigned char refl_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE
                            * (AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT / 100)
                            * sizeof(fb_data)];
    fb_data col_buf[AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE
                     + AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT / 100];
    aura_albumart_t art;
    int frames, frame_delay, i;
    long start_tick = current_tick;

    art.size = size;
    art.radius = AURA_DS_METRICS_COVER_FLOW_CORNER_RADIUS;
    art.cover_data = cover_buf;
    art.reflection_data = refl_buf;

    if (!lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE
        || !aura_albumart_load_for_album(album_seek, &art))
    {
        /* Sin animacion (ajuste del usuario) o sin caratula real --
         * nada que volar, el llamador navega directo. */
        aura_nav_push(nav, AURA_SCREEN_NOWPLAYING);
        return;
    }

    frames = (aura_settings.animation_mode == AURA_ANIM_ALL) ? 14 : 6;
    frame_delay = HZ / 60;

    for (i = 1; i <= frames; i++)
    {
        int t = i * 256 / frames;
        int total_h = size + refl_h;
        int y = aura_pattern_lerp(from_y, AURA_DS_METRICS_NOW_PLAYING_COVER_Y, t);
        aura_flow_slide_t slide;
        aura_flow_projection_t proj;

        /* Signo positivo, no negativo -- mismo signo que
         * draw_cover_tilted() (aura_nowplaying.c) usa ahora, ver el
         * comentario ahi: con el signo negativo original el lado
         * izquierdo quedaba corto y el derecho alto, al reves de lo
         * pedido. Esta funcion nunca inventa su propio destino (lee
         * los mismos AURA_NOWPLAYING_TILT_IANGLE/_CX), asi que tiene
         * que seguir el mismo cambio de signo o la caratula "saltaria"
         * de direccion en el ultimo cuadro del vuelo. */
        slide.angle = aura_pattern_lerp(0, AURA_NOWPLAYING_TILT_IANGLE, t);
        slide.distance = 0;
        slide.cx = aura_pattern_lerp(0, AURA_NOWPLAYING_TILT_CX, t);

        a26_shell_clear_screen();
        /* Titulo instantaneo, mismo criterio que el push de arriba --
         * la banda de StatusBar no forma parte del vuelo. */
        aura_statusbar_draw(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_NOWPLAYING), 1);

        aura_flow_begin_projection(&proj, &slide, size);
        while (proj.screen_x < AURA_FLOW_SCREEN_W)
        {
            int col = aura_flow_source_column(&proj);
            int dy = aura_flow_vertical_scale(&proj);
            int p = 0, dest_row, n_rows = 0;
            const fb_data *cover_col = (const fb_data *)art.cover_data + (size_t)col * size;
            const fb_data *refl_col = (const fb_data *)art.reflection_data + (size_t)col * refl_h;

            for (dest_row = 0; dest_row < total_h; dest_row++)
            {
                int source_row = p >> AURA_FLOW_SHIFT;

                if (source_row >= total_h)
                    break;
                col_buf[dest_row] = (source_row < size)
                    ? cover_col[source_row]
                    : refl_col[source_row - size];
                p += dy;
                n_rows++;
            }
            if (n_rows > 0)
                lcd_bitmap(col_buf, proj.screen_x, y, 1, n_rows);

            if (!aura_flow_advance_column(&proj))
                break;
        }

        lcd_update();
        drain_button_queue_if_full();

        if (i < frames)
            sleep(frame_delay);
    }

    TRANSITION_LOG("flip_and_flow", frames, start_tick);

    aura_nav_push(nav, AURA_SCREEN_NOWPLAYING);
}
