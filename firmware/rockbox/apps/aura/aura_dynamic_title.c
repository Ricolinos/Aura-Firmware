#include "lcd.h"

#include "aura_marquee.h"
#include "aura_patterns.h"
#include "aura_dynamic_title.h"

/* Recorta a (x,y,w,h) y dibuja `text` desplazado (off_x, off_y) DENTRO
 * de ese recorte -- mismo mecanismo de viewport que aura_marquee.c
 * (hereda fuente/color/drawmode del contexto activo). */
static void draw_clipped(int x, int y, int w, int h, const char *text,
                          int off_x, int off_y)
{
    struct viewport vp = *lcd_current_viewport;
    struct viewport *saved;

    vp.x = x;
    vp.y = y;
    vp.width = w;
    vp.height = h;
    saved = lcd_set_viewport(&vp);

    lcd_putsxy(off_x, off_y, (const unsigned char *)text);

    lcd_set_viewport(saved);
}

void aura_dynamic_title_draw(int x, int y, int max_width,
                              const char *text, const char *prev_text,
                              aura_title_transition_t transition,
                              int progress_256, int forward,
                              long marquee_elapsed_ms)
{
    int w, h;

    if (transition == AURA_TITLE_TRANSITION_NONE || !prev_text)
    {
        aura_marquee_draw(x, y, max_width, text, marquee_elapsed_ms);
        return;
    }

    lcd_getstringsize((const unsigned char *)text, &w, &h);

    if (transition == AURA_TITLE_TRANSITION_FADE_SLIDE)
    {
        /* Horizontal -- doc confirmado: profundizar = nuevo desde la
         * derecha; regresar = nuevo desde la izquierda. */
        int new_from = forward ? max_width : -max_width;
        int old_to   = forward ? -max_width : max_width;

        draw_clipped(x, y, max_width, h, prev_text,
                     aura_pattern_lerp(0, old_to, progress_256), 0);
        draw_clipped(x, y, max_width, h, text,
                     aura_pattern_lerp(new_from, 0, progress_256), 0);
    }
    else /* AURA_TITLE_TRANSITION_SCROLL_SLIDE */
    {
        /* Vertical -- doc confirmado: scroll abajo = nuevo desde abajo,
         * viejo sale hacia arriba; scroll arriba, al reves. */
        int new_from = forward ? h : -h;
        int old_to   = forward ? -h : h;

        draw_clipped(x, y, max_width, h, prev_text,
                     0, aura_pattern_lerp(0, old_to, progress_256));
        draw_clipped(x, y, max_width, h, text,
                     0, aura_pattern_lerp(new_from, 0, progress_256));
    }
}
