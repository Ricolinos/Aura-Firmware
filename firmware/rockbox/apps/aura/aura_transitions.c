#include "lcd.h"
#include "kernel.h"
#include "tick.h"
#include "gui/viewport.h"

#include "aura_transitions.h"
#include "aura_screens.h"
#include "aura_settings.h"
#include "aura_tokens.h"

void aura_transition_slide(aura_nav_t *nav, int direction)
{
    int frames, frame_delay, start_x, i;
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

    start_x = (direction > 0) ? AURA_SCREEN_WIDTH : -AURA_SCREEN_WIDTH;

    for (i = 1; i <= frames; i++)
    {
        int x = start_x - (start_x * i) / frames;

        viewport_set_defaults(&vp, SCREEN_MAIN);
        vp.x = x;
        vp.width = AURA_SCREEN_WIDTH;
        vp.height = AURA_SCREEN_HEIGHT;

        saved = lcd_set_viewport(&vp);
        aura_screens_draw(nav);
        lcd_set_viewport(saved);
        lcd_update();

        if (i < frames)
            sleep(frame_delay);
    }
}
