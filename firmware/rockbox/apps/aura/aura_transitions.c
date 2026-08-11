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
