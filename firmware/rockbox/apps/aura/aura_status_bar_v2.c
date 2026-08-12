#include "lcd.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_statusbar.h"
#include "aura_dynamic_title.h"
#include "aura_clock_indicator.h"
#include "aura_settings.h"

#include "aura_status_bar_v2.h"

/* Separacion entre las zonas DynamicTitle/ClockIndicator/iconos:
 * componentes/status-bar.md y dynamic-title.md dejan pendiente el
 * espaciado exacto ("el 4px confirmado es solo entre iconos"). Se
 * reusa el mismo padding interno de la barra (4px) como valor
 * provisional razonable -- TODO(pendiente-doc), ver DECISIONS.md
 * D-096. */
#define AURA_SB2_ZONE_GAP AURA_DS_METRICS_STATUSBAR_PADDING

void aura_status_bar_v2_draw(int x, int width, const char *title,
                              bool is_playing, bool is_paused, bool is_hold)
{
    int is_full = (width == AURA_DS_METRICS_STATUSBAR_WIDTH_FULL);
    int padding = AURA_DS_METRICS_STATUSBAR_PADDING;
    int icon_h = AURA_DS_METRICS_STATUSBAR_ICON_HEIGHT;
    int icon_y = (AURA_DS_METRICS_STATUSBAR_HEIGHT - icon_h) / 2;
    int right = x + width - padding;
    int left = x + padding;
    bool show_clock = aura_settings.clock_visible;
    int clock_w = AURA_DS_METRICS_CLOCK_INDICATOR_MAX_WIDTH;
    int title_max_w, title_x;
    int w, h, text_y;
    unsigned bg = a26_color(A26_SHELL_BG);
    unsigned ink;

    /* Fondo: traslucencia simple sobre lo ya dibujado es la opcion
     * recomendada por el documento, pero sin contenido detras de la
     * barra todavia (SelectionSummary/CoverDrift, T2.8/T2.9 aun no
     * existen) un blend real seria indistinguible de un color solido.
     * "Valor exacto del color solido de fondo" tambien esta marcado
     * pendiente -- se usa SHELL_BG (el mismo fondo del shell) como
     * fallback provisional. TODO(pendiente-doc), ver DECISIONS.md
     * D-096. */
    lcd_set_foreground(bg);
    lcd_fillrect(x, 0, width, AURA_DS_METRICS_STATUSBAR_HEIGHT);

    /* Iconos: bateria siempre en el extremo derecho; candado/play-pausa
     * condicionales a su izquierda, misma regla en (split) y (full)
     * (doc no distingue por estado para esta seccion). Ancho asumido
     * cuadrado (= icon_h) para el espaciado, mismo criterio ya usado
     * por el sistema viejo (aura_statusbar.c) -- el ancho exacto por
     * icono tambien esta pendiente en el documento. */
    right -= icon_h;
    aura_widgets_draw_icon(aura_battery_icon_name(), icon_h, right, icon_y);

    if (is_playing)
    {
        right -= AURA_DS_METRICS_STATUSBAR_ICON_GAP + icon_h;
        aura_widgets_draw_icon(is_paused ? "pause" : "play", icon_h, right, icon_y);
    }
    if (is_hold)
    {
        right -= AURA_DS_METRICS_STATUSBAR_ICON_GAP + icon_h;
        aura_widgets_draw_icon("lock", icon_h, right, icon_y);
    }

    /* ClockIndicator + DynamicTitle: orden y posicion segun estado
     * (componentes/status-bar.md tabla "Contenido y orden";
     * dynamic-title.md "en (full), la posicion tambien depende de
     * ClockIndicator"). En (split), el titulo va primero y el reloj
     * despues, en su ancho maximo reservado; en (full) es al reves. */
    if (is_full)
    {
        title_x = left;
        if (show_clock)
        {
            aura_clock_indicator_draw(left, clock_w, 0, 256);
            title_x = left + clock_w + AURA_SB2_ZONE_GAP;
        }
        title_max_w = AURA_DS_METRICS_DYNAMIC_TITLE_MAX_WIDTH_FULL;
    }
    else
    {
        title_x = left;
        title_max_w = show_clock
            ? AURA_DS_METRICS_DYNAMIC_TITLE_MAX_WIDTH_SPLIT_WITH_CLOCK
            : AURA_DS_METRICS_DYNAMIC_TITLE_MAX_WIDTH_SPLIT_NO_CLOCK;
        if (show_clock)
            aura_clock_indicator_draw(left + title_max_w + AURA_SB2_ZONE_GAP, clock_w, 0, 256);
    }

    /* Tipografia con opacidad simulada (fundamentos/02-tipografia.md,
     * status-bar.md): DynamicTitle Bold 8px 60%, ClockIndicator Regular
     * 8px 80% -- mismo mecanismo de a26_shell_blend() que ya usan
     * sombras/scrollbars, aplicado aca al color de tinta antes de
     * dibujar el texto en vez de a un rectangulo. aura_clock_indicator_draw()
     * fija su propia fuente/color (T2.5, componente independiente); se
     * corrige aca a la opacidad real que exige status-bar.md (T2.5 solo
     * la verifico en modo persistente aislado, sin esta regla todavia
     * cableada). */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_8));
    ink = a26_shell_blend(bg, a26_color(A26_TEXT_PRIMARY),
                           AURA_DS_OPACITY_STATUSBAR_TITLE_PCT * 256 / 100);
    lcd_set_foreground(ink);
    lcd_getstringsize((const unsigned char *)title, &w, &h);
    text_y = (AURA_DS_METRICS_STATUSBAR_HEIGHT - h) / 2;

    /* DynamicTitle estatico por ahora (transition=NONE): disparar
     * Fade-Slide/Scroll-Slide reales en cada navegacion es follow-up
     * anotado en el header de este modulo, no bloqueado. */
    aura_dynamic_title_draw(title_x, text_y, title_max_w, title, NULL,
                             AURA_TITLE_TRANSITION_NONE, 256, 1, 0);
}
