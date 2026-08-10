#include "lcd.h"
#include "kernel.h"
#include "tick.h"
#include "gui/viewport.h"

#include "aura_transitions.h"
#include "aura_screens.h"
#include "aura_settings.h"
#include "aura_tokens.h"

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
}
