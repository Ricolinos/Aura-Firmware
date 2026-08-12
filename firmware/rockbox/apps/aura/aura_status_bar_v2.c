#include "lcd.h"
#include "tick.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_statusbar.h"
#include "aura_dynamic_title.h"
#include "aura_clock_indicator.h"
#include "aura_settings.h"
#include "aura_patterns.h"
#include "aura_motion.h"

#include "aura_status_bar_v2.h"

/* Separacion entre las zonas DynamicTitle/ClockIndicator/iconos:
 * componentes/status-bar.md y dynamic-title.md dejan pendiente el
 * espaciado exacto ("el 4px confirmado es solo entre iconos"). Se
 * reusa el mismo padding interno de la barra (4px) como valor
 * provisional razonable -- TODO(pendiente-doc), ver DECISIONS.md
 * D-096. */
#define AURA_SB2_ZONE_GAP AURA_DS_METRICS_STATUSBAR_PADDING

/* -- ClockIndicator por atajo (B-01 en BLOCKED.md, resuelto) ------------
 *
 * "Se revela manteniendo presionado SELECT" (doc) -- el gesto en si
 * (deteccion de mantener presionado) es AURA_BUTTON_HOLD (aura_main.h,
 * B-02): aura_main.c lo intercepta de forma centralizada y llama a
 * aura_status_bar_v2_reveal_clock(), StatusBar es la unica duena de
 * este estado.
 *
 * Progreso 0..256 animado con aura_pattern_lerp() (T1.1, cero
 * matematica nueva) -- mismo patron de redirigir-sin-salto ya usado en
 * CoverDrift/CoverFlow/morph de NowPlaying: `s_anim_from` guarda el
 * progreso real al momento en que arranca la animacion ACTUAL, para
 * que revelar de nuevo mientras todavia se esta ocultando (u ocultar
 * mientras todavia entra) nunca salte, solo cambie de direccion desde
 * donde ya estaba.
 *
 * (full): "entra de izquierda a derecha" (Push-and-Pull) -- el
 * progreso mueve la posicion X del reloj Y la de DynamicTitle a la
 * vez (empuja/jala), reutilizando el parametro `x` que
 * aura_clock_indicator_draw() ya expone (sin tocar ese componente:
 * su propio `enter_progress_256` es para el Drop-and-Lift VERTICAL de
 * (split), un eje distinto -- aca siempre se le pasa 256, "ya
 * asentado verticalmente", y el movimiento horizontal lo controla
 * este archivo). (split): Drop-and-Lift VERTICAL -- ese si usa
 * `enter_progress_256` tal como T2.5 ya lo diseño. Ningun consumidor
 * real de (split) existe todavia (mismo caso que el resto del
 * sistema), implementado por completitud/consistencia con el
 * documento, no verificado visualmente. */
#define AURA_SB2_CLOCK_ANIM_MS 220 /* TODO(pendiente-doc): timing de Drop-and-Lift/Push-and-Pull no confirmado, ver DECISIONS.md D-108 */
#define AURA_SB2_CLOCK_VISIBLE_MS AURA_DS_METRICS_CLOCK_INDICATOR_AUTO_HIDE_AFTER_MS /* 10000, ya confirmado */

static int s_clock_target = 0;      /* 0 = oculto, 256 = revelado */
static int s_clock_anim_from = 0;
static long s_clock_anim_since = 0;
static long s_clock_hide_deadline = 0;

static int clock_progress_now(void)
{
    long elapsed_ms = (current_tick - s_clock_anim_since) * 1000L / HZ;
    int t = aura_motion_linear(elapsed_ms, AURA_SB2_CLOCK_ANIM_MS);
    return aura_pattern_lerp(s_clock_anim_from, s_clock_target, t);
}

static void clock_retarget(int target)
{
    if (target == s_clock_target)
        return;
    s_clock_anim_from = clock_progress_now();
    s_clock_target = target;
    s_clock_anim_since = current_tick;
}

void aura_status_bar_v2_reveal_clock(void)
{
    if (aura_settings.clock_visible)
        return; /* persistente: siempre visible, nada que revelar */

    clock_retarget(256);
    s_clock_hide_deadline = current_tick + (AURA_SB2_CLOCK_VISIBLE_MS * HZ / 1000);
}

int aura_status_bar_v2_clock_pending(void)
{
    return s_clock_target != 0 || clock_progress_now() != 0;
}

int aura_status_bar_v2_clock_animating(void)
{
    return clock_progress_now() != s_clock_target;
}

void aura_status_bar_v2_draw(int x, int width, const char *title,
                              bool is_playing, bool is_paused, bool is_hold)
{
    int is_full = (width == AURA_DS_METRICS_STATUSBAR_WIDTH_FULL);
    int padding = AURA_DS_METRICS_STATUSBAR_PADDING;
    int icon_h = AURA_DS_METRICS_STATUSBAR_ICON_HEIGHT;
    int icon_y = (AURA_DS_METRICS_STATUSBAR_HEIGHT - icon_h) / 2;
    int right = x + width - padding;
    int left = x + padding;
    int clock_w = AURA_DS_METRICS_CLOCK_INDICATOR_MAX_WIDTH;
    int title_max_w, title_x;
    int w, h, text_y;
    unsigned bg = a26_color(A26_SHELL_BG);
    unsigned ink;
    /* Persistente: siempre asentado (256), sin animacion -- mismo
     * comportamiento exacto de antes. No persistente: progreso real
     * de la revelacion por atajo (B-01), 0 si nunca se disparo. El
     * auto-ocultado a los 10s se resuelve aca, una vez por cuadro. */
    int clock_progress = 256;

    if (!aura_settings.clock_visible)
    {
        if (s_clock_target == 256 && current_tick > s_clock_hide_deadline)
            clock_retarget(0);
        clock_progress = clock_progress_now();
    }

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
     * despues, en su ancho maximo reservado; en (full) es al reves.
     *
     * `clock_progress` maneja los TRES casos con la misma formula:
     * persistente (256 fijo, layout identico al de antes), revelado
     * por atajo asentado (256, mismo layout), y a medio
     * entrar/salir (0<progreso<256, Push-and-Pull en (full) real vía
     * aura_pattern_lerp -- T1.1, cero matematica nueva). */
    if (is_full)
    {
        title_x = aura_pattern_lerp(left, left + clock_w + AURA_SB2_ZONE_GAP, clock_progress);
        title_max_w = AURA_DS_METRICS_DYNAMIC_TITLE_MAX_WIDTH_FULL;
        if (clock_progress > 0)
        {
            /* Horizontal, no vertical: enter_progress_256 se le pasa
             * SIEMPRE 256 a proposito (ver comentario junto al
             * estado de arriba) -- el eje que anima aca es X, propio
             * de este archivo, no el Drop-and-Lift vertical que ese
             * parametro controla. */
            int clock_x = aura_pattern_lerp(left - clock_w, left, clock_progress);
            aura_clock_indicator_draw(clock_x, clock_w, 0, 256);
        }
    }
    else
    {
        title_x = left;
        title_max_w = aura_pattern_lerp(AURA_DS_METRICS_DYNAMIC_TITLE_MAX_WIDTH_SPLIT_NO_CLOCK,
                                         AURA_DS_METRICS_DYNAMIC_TITLE_MAX_WIDTH_SPLIT_WITH_CLOCK,
                                         clock_progress);
        /* (split): Drop-and-Lift VERTICAL real -- este si es el eje
         * que enter_progress_256 ya sabe animar (T2.5), sin logica
         * nueva aca. Se autoprotege si progreso<=0 (no dibuja). */
        aura_clock_indicator_draw(left + title_max_w + AURA_SB2_ZONE_GAP, clock_w, 0, clock_progress);
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
