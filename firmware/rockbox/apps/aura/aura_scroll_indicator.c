#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_patterns.h"

#include "aura_scroll_indicator.h"

void aura_scroll_indicator_draw(int x, int track_y, int track_h,
                                 int first, int count, int visible,
                                 long idle_elapsed_ms)
{
    int thumb_h = AURA_DS_METRICS_SCROLL_INDICATOR_HEIGHT;
    int thumb_w = AURA_DS_METRICS_SCROLL_INDICATOR_THICKNESS;
    int max_first, thumb_y, alpha_256;
    unsigned gray, bg, color;

    if (count <= AURA_DS_METRICS_SCROLL_INDICATOR_MIN_ITEMS_TO_SHOW)
        return;

    alpha_256 = aura_pattern_fade_on_idle_alpha_256(idle_elapsed_ms,
        AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS,
        AURA_DS_METRICS_SCROLL_INDICATOR_IDLE_BEFORE_FADE_MS,
        AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS);
    if (alpha_256 <= 0)
        return;

    if (thumb_h > track_h)
        thumb_h = track_h;

    max_first = count - visible;
    thumb_y = (max_first > 0)
        ? track_y + ((track_h - thumb_h) * first) / max_first
        : track_y;

    /* Gris neutro (G8): reutiliza SHELL_RAIL del tema, ya definido --
     * no un segundo gris inventado. Sin alfa real: se simula
     * fundiendose contra SHELL_BG, mismo mecanismo que ya prueba el
     * scrollbar del sistema viejo (D-073). */
    gray = a26_color(A26_SHELL_RAIL);
    bg = a26_color(A26_SHELL_BG);
    color = a26_shell_blend(bg, gray, alpha_256);

    a26_shell_fill_rounded_rect(x - thumb_w, thumb_y, thumb_w, thumb_h,
                                 thumb_w / 2, color, bg);
}
