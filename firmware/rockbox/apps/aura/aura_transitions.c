#include "lcd.h"
#include "kernel.h"
#include "tick.h"
#include "gui/viewport.h"
#include "debug.h"

#include "aura_transitions.h"
#include "aura_screens.h"
#include "aura_settings.h"
#include "aura_tokens.h"

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

/* NOTA (reemplaza el enfoque anterior de viewport desplazado -- ver
 * DECISIONS.md): un viewport con x fuera de [0, AURA_SCREEN_WIDTH] o con
 * x+width > AURA_SCREEN_WIDTH es invalido. set_viewport_ex() en
 * SIMULATOR solo lo *reporta* por DEBUGF, pero lo instala igual como
 * viewport activo -- un blit grande (p.ej. una foto a pantalla
 * completa) dentro de ese viewport invalido corrompe memoria (bus
 * error). No hay forma segura de "desplazar" un viewport de
 * AURA_SCREEN_WIDTH completo sin salirse de esos limites, porque en
 * Rockbox vp.x cumple doble función (origen de traduccion de
 * coordenadas Y limite de recorte): no se puede recortar el ancho
 * visible sin romper la traduccion del contenido dibujado dentro.
 *
 * En su lugar, se usa un "wipe" de revelado: se dibuja la pantalla
 * nueva completa (coordenadas normales, sin desplazar) pero acotada a
 * una franja que crece cada cuadro, siempre dentro de
 * [0, AURA_SCREEN_WIDTH]. El framebuffer conserva la pantalla anterior
 * en la parte aun no cubierta, dando el mismo efecto visual de
 * "entra deslizando" sin nunca construir un viewport invalido. */
void aura_transition_slide(aura_nav_t *nav, int direction)
{
    int frames, frame_delay, i;
    struct viewport vp;
    struct viewport *saved;
    long start_tick = current_tick;

    if (aura_settings.graphics_mode == AURA_GFX_ULTRA || direction == 0)
        return;

    if (aura_settings.graphics_mode == AURA_GFX_FULL)
    {
        frames = 8;
        frame_delay = HZ / 60;
    }
    else /* AURA_GFX_MINIMAL */
    {
        frames = 4;
        frame_delay = HZ / 45;
    }

    for (i = 1; i <= frames; i++)
    {
        int revealed = (AURA_SCREEN_WIDTH * i) / frames;

        viewport_set_defaults(&vp, SCREEN_MAIN);
        vp.y = 0;
        vp.height = AURA_SCREEN_HEIGHT;

        if (direction > 0)
        {
            /* Adelante: revela desde el borde derecho hacia la
             * izquierda (x+width se mantiene siempre en
             * AURA_SCREEN_WIDTH, nunca lo excede). */
            vp.x = AURA_SCREEN_WIDTH - revealed;
            vp.width = revealed;
        }
        else
        {
            /* Atras: revela desde el borde izquierdo hacia la
             * derecha (x fijo en 0, width nunca excede
             * AURA_SCREEN_WIDTH). */
            vp.x = 0;
            vp.width = revealed;
        }

        saved = lcd_set_viewport(&vp);
        aura_screens_draw(nav);
        lcd_set_viewport(saved);
        lcd_update();

        if (i < frames)
            sleep(frame_delay);
    }

    TRANSITION_LOG("slide", frames, start_tick);
}

/* T4 (Coverflow, L4): mismo truco seguro de "wipe" que aura_transition_slide
 * (D-030) -- nunca se instala un viewport fuera de [0, AURA_SCREEN_WIDTH] --
 * pero revelando desde AMBOS bordes hacia el centro en vez de desde uno
 * solo, para que la entrada a Coverflow se sienta distinta de una
 * navegacion de lista comun ("el contenido emerge de detras", L4). */
void aura_transition_reveal(aura_nav_t *nav)
{
    int frames, frame_delay, i;
    struct viewport vp;
    struct viewport *saved;
    long start_tick = current_tick;

    if (aura_settings.graphics_mode == AURA_GFX_ULTRA)
        return;

    if (aura_settings.graphics_mode == AURA_GFX_FULL)
    {
        frames = 10;
        frame_delay = HZ / 60;
    }
    else /* AURA_GFX_MINIMAL */
    {
        frames = 4;
        frame_delay = HZ / 45;
    }

    for (i = 1; i <= frames; i++)
    {
        int half = (AURA_SCREEN_WIDTH / 2 * i) / frames;

        viewport_set_defaults(&vp, SCREEN_MAIN);
        vp.y = 0;
        vp.height = AURA_SCREEN_HEIGHT;

        /* Mitad izquierda: revela desde x=0 hacia el centro. */
        vp.x = 0;
        vp.width = half;
        saved = lcd_set_viewport(&vp);
        aura_screens_draw(nav);
        lcd_set_viewport(saved);

        /* Mitad derecha: revela desde el borde derecho hacia el
         * centro. x+width se mantiene siempre en AURA_SCREEN_WIDTH. */
        vp.x = AURA_SCREEN_WIDTH - half;
        vp.width = half;
        saved = lcd_set_viewport(&vp);
        aura_screens_draw(nav);
        lcd_set_viewport(saved);

        lcd_update();

        if (i < frames)
            sleep(frame_delay);
    }

    TRANSITION_LOG("reveal", frames, start_tick);
}
