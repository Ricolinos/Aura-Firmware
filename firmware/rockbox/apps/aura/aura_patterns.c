#include "aura_patterns.h"

int aura_pattern_lerp(int a, int b, int progress_256)
{
    if (progress_256 <= 0) return a;
    if (progress_256 >= 256) return b;
    return a + ((b - a) * progress_256) / 256;
}

int aura_pattern_marquee_offset(long elapsed_ms, long static_ms, long scroll_ms,
                                  int scroll_distance_px)
{
    long cycle_ms, t;

    if (static_ms < 0) static_ms = 0;
    if (scroll_ms <= 0) return 0; /* sin fase de movimiento, no hay nada que ofsetear */
    if (elapsed_ms < 0) elapsed_ms = 0;

    cycle_ms = static_ms + scroll_ms;
    t = elapsed_ms % cycle_ms;

    if (t < static_ms)
        return 0;

    t -= static_ms; /* [0, scroll_ms) */
    return (int)((long)scroll_distance_px * t / scroll_ms);
}

int aura_pattern_fade_on_idle_alpha_256(long elapsed_ms, long fade_in_ms,
                                          long hold_ms, long fade_out_ms)
{
    if (elapsed_ms < 0)
        elapsed_ms = 0;
    if (fade_in_ms < 0) fade_in_ms = 0;
    if (hold_ms < 0) hold_ms = 0;
    if (fade_out_ms < 0) fade_out_ms = 0;

    if (fade_in_ms > 0 && elapsed_ms < fade_in_ms)
        return (int)(elapsed_ms * 256 / fade_in_ms);

    elapsed_ms -= fade_in_ms;
    if (elapsed_ms < hold_ms)
        return 256;

    elapsed_ms -= hold_ms;
    if (fade_out_ms > 0 && elapsed_ms < fade_out_ms)
        return 256 - (int)(elapsed_ms * 256 / fade_out_ms);

    return 0;
}

/* cos(angulo)*256, sen(angulo)*256 -- unicamente para los 8 angulos
 * reales que CoverDrift usa (ver aura_patterns.h). Screen-space y-abajo
 * (sen positivo = hacia abajo), angulo 0 = derecha, sentido horario --
 * mismo convenio que aura_flow.c ya usa para su propia tabla. */
typedef struct { int angle, cos256, sin256; } drift_dir_t;

static const drift_dir_t DRIFT_DIRS[] = {
    {   0,  256,    0 },
    {  60,  128,  222 },
    {  90,    0,  256 },
    { 120, -128,  222 },
    { 180, -256,    0 },
    { 240, -128, -222 },
    { 270,    0, -256 },
    { 300,  128, -222 },
};
#define DRIFT_DIRS_N ((int)(sizeof(DRIFT_DIRS) / sizeof(DRIFT_DIRS[0])))

aura_pattern_point_t aura_pattern_drift_pos(int angle_deg, int distance_px,
                                              long elapsed_ms, long duration_ms)
{
    aura_pattern_point_t p = { 0, 0 };
    int i, cos256 = 0, sin256 = 0, found = 0;
    long remain_ms, dist_now;

    for (i = 0; i < DRIFT_DIRS_N; i++)
    {
        if (DRIFT_DIRS[i].angle == angle_deg)
        {
            cos256 = DRIFT_DIRS[i].cos256;
            sin256 = DRIFT_DIRS[i].sin256;
            found = 1;
            break;
        }
    }
    if (!found || duration_ms <= 0)
        return p;

    if (elapsed_ms < 0) elapsed_ms = 0;
    if (elapsed_ms > duration_ms) elapsed_ms = duration_ms;
    remain_ms = duration_ms - elapsed_ms; /* distancia restante al centro */

    dist_now = (long)distance_px * remain_ms / duration_ms;

    p.dx = (int)(dist_now * cos256 / 256);
    p.dy = (int)(dist_now * sin256 / 256);
    return p;
}

aura_push_drop_state_t aura_pattern_push_and_drop(long elapsed_ms, long push_ms,
                                                     long drop_ms, int reverse)
{
    aura_push_drop_state_t s;
    long first_ms = reverse ? drop_ms : push_ms;
    long second_ms = reverse ? push_ms : drop_ms;

    if (elapsed_ms < 0) elapsed_ms = 0;
    if (first_ms < 0) first_ms = 0;
    if (second_ms < 0) second_ms = 0;

    if (elapsed_ms < first_ms)
    {
        s.phase = AURA_PUSH_DROP_PHASE_A;
        s.progress_256 = (first_ms > 0) ? (int)(elapsed_ms * 256 / first_ms) : 256;
        return s;
    }

    elapsed_ms -= first_ms;
    if (elapsed_ms < second_ms)
    {
        s.phase = AURA_PUSH_DROP_PHASE_B;
        s.progress_256 = (second_ms > 0) ? (int)(elapsed_ms * 256 / second_ms) : 256;
        return s;
    }

    s.phase = AURA_PUSH_DROP_DONE;
    s.progress_256 = 256;
    return s;
}
