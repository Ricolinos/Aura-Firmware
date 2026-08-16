#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_patterns.h"
#include "kernel.h"

#include "aura_scroll_indicator.h"

/* Estado del deslizamiento (D-275). Una sola lista visible a la vez,
 * asi que un solo juego de variables alcanza -- el "carril" (x, track_y,
 * track_h, count) identifica la lista: si cambia, es otra pantalla u
 * otra lista y el pulgar se planta en su sitio sin animar. */
static int s_key_x = -1, s_key_track_y, s_key_track_h, s_key_count;
static int s_from_y, s_to_y;
static long s_slide_since;
static int s_was_visible;

static long slide_elapsed_ms(void)
{
    return (current_tick - s_slide_since) * 1000L / HZ;
}

static int sliding(void)
{
    return s_key_x >= 0 && s_from_y != s_to_y
        && slide_elapsed_ms() < AURA_DS_METRICS_SCROLL_INDICATOR_SLIDE_MS;
}

static int current_thumb_y(void)
{
    long t = slide_elapsed_ms();
    int progress_256;

    if (s_from_y == s_to_y || t >= AURA_DS_METRICS_SCROLL_INDICATOR_SLIDE_MS)
        return s_to_y;
    progress_256 = (int)(t * 256 / AURA_DS_METRICS_SCROLL_INDICATOR_SLIDE_MS);
    return aura_pattern_lerp(s_from_y, s_to_y, progress_256);
}

void aura_scroll_indicator_draw(int x, int track_y, int track_h,
                                 int selected, int count,
                                 long idle_elapsed_ms,
                                 unsigned bg, unsigned ink)
{
    int thumb_h = AURA_DS_METRICS_SCROLL_INDICATOR_HEIGHT;
    int thumb_w = AURA_DS_METRICS_SCROLL_INDICATOR_THICKNESS;
    int target_y, thumb_y, alpha_256;
    unsigned color;

    if (count <= AURA_DS_METRICS_SCROLL_INDICATOR_MIN_ITEMS_TO_SHOW)
    {
        s_was_visible = 0;
        return;
    }

    /* Fundido asimetrico (D-275): entrada rapida (150ms), persistencia
     * mientras haya actividad, salida lenta (500ms) -- como el iPod
     * Classic. Antes el mismo valor servia de entrada y salida y la
     * barra tardaba medio segundo en verse tras cada movimiento. */
    alpha_256 = aura_pattern_fade_on_idle_alpha_256(idle_elapsed_ms,
        AURA_DS_METRICS_SCROLL_INDICATOR_FADE_IN_MS,
        AURA_DS_METRICS_SCROLL_INDICATOR_IDLE_BEFORE_FADE_MS,
        AURA_DS_METRICS_SCROLL_INDICATOR_FADE_OUT_MS);

    /* "Invisible" es solo cuando el desvanecido ya TERMINO (idle mas
     * alla de la ventana entera), no cuando el alpha es 0 en el primer
     * cuadro de la entrada: cada boton reinicia idle a 0 y ese cuadro
     * tiene alpha 0 -- si eso contara como "no visible", el siguiente
     * cuadro plantaria el pulgar sin deslizar (bug real, visto midiendo
     * capturas al verificar D-275). */
    if (idle_elapsed_ms >= AURA_DS_METRICS_SCROLL_INDICATOR_FADE_IN_MS
                          + AURA_DS_METRICS_SCROLL_INDICATOR_IDLE_BEFORE_FADE_MS
                          + AURA_DS_METRICS_SCROLL_INDICATOR_FADE_OUT_MS)
    {
        s_was_visible = 0;
        return;
    }

    if (thumb_h > track_h)
        thumb_h = track_h;

    /* Posicion por ITEM seleccionado (D-275), no por ventana visible:
     * el pulgar avanza con cada paso de la rueda, de arriba a abajo del
     * carril, sin los tramos muertos que producia `first`. */
    if (selected < 0) selected = 0;
    if (selected > count - 1) selected = count - 1;
    target_y = (count > 1)
        ? track_y + ((track_h - thumb_h) * selected) / (count - 1)
        : track_y;

    /* Deslizamiento "redirigir sin salto": si el carril cambio o el
     * indicador acaba de aparecer, plantar sin animar; si el objetivo
     * cambio, arrancar desde la posicion interpolada actual. */
    if (s_key_x != x || s_key_track_y != track_y || s_key_track_h != track_h
        || s_key_count != count || !s_was_visible)
    {
        s_key_x = x; s_key_track_y = track_y;
        s_key_track_h = track_h; s_key_count = count;
        s_from_y = s_to_y = target_y;
    }
    else if (target_y != s_to_y)
    {
        s_from_y = current_thumb_y();
        s_to_y = target_y;
        s_slide_since = current_tick;
    }
    s_was_visible = 1;
    thumb_y = current_thumb_y();

    if (alpha_256 <= 0)
        return; /* primer cuadro de la entrada: estado ya actualizado, nada que pintar */

    /* Sin alfa real: el fundido se simula mezclando la tinta hacia el
     * fondo REAL del contexto (parametros del llamador), mismo
     * mecanismo que ya probo el scrollbar del sistema viejo (D-073). */
    color = a26_shell_blend(bg, ink, alpha_256);

    a26_shell_fill_rounded_rect(x - thumb_w, thumb_y, thumb_w, thumb_h,
                                 thumb_w / 2, color, bg);
}

int aura_scroll_indicator_pending(long idle_elapsed_ms)
{
    long window_ms = AURA_DS_METRICS_SCROLL_INDICATOR_FADE_IN_MS
                    + AURA_DS_METRICS_SCROLL_INDICATOR_IDLE_BEFORE_FADE_MS
                    + AURA_DS_METRICS_SCROLL_INDICATOR_FADE_OUT_MS;
    return sliding() || (idle_elapsed_ms >= 0 && idle_elapsed_ms < window_ms);
}

int aura_scroll_indicator_animating(long idle_elapsed_ms)
{
    if (sliding())
        return 1;
    if (idle_elapsed_ms < 0)
        return 0;
    if (idle_elapsed_ms < AURA_DS_METRICS_SCROLL_INDICATOR_FADE_IN_MS)
        return 1; /* apareciendo */
    idle_elapsed_ms -= AURA_DS_METRICS_SCROLL_INDICATOR_FADE_IN_MS
                      + AURA_DS_METRICS_SCROLL_INDICATOR_IDLE_BEFORE_FADE_MS;
    return idle_elapsed_ms >= 0
        && idle_elapsed_ms < AURA_DS_METRICS_SCROLL_INDICATOR_FADE_OUT_MS; /* desvaneciendo */
}
