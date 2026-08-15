#include <stdio.h>
#include "../aura_patterns.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void test_lerp(void)
{
    CHECK(aura_pattern_lerp(0, 100, 0) == 0);
    CHECK(aura_pattern_lerp(0, 100, 256) == 100);
    CHECK(aura_pattern_lerp(0, 100, 128) == 50);
    CHECK(aura_pattern_lerp(10, 10, 128) == 10);
    CHECK(aura_pattern_lerp(100, 0, 64) == 75); /* interpola tambien decreciendo */
    /* fuera de rango se clampa a los extremos, no extrapola */
    CHECK(aura_pattern_lerp(0, 100, 300) == 100);
    CHECK(aura_pattern_lerp(0, 100, -50) == 0);
}

static void test_marquee_static_phase(void)
{
    /* Doc: 2s estatico, texto visible sin desplazar. */
    CHECK(aura_pattern_marquee_offset(0, 2000, 5000, 120) == 0);
    CHECK(aura_pattern_marquee_offset(1999, 2000, 5000, 120) == 0);
}

static void test_marquee_scroll_phase(void)
{
    /* Justo al empezar a mover: offset 0. A mitad del tramo de 5s:
     * offset ~mitad de la distancia. Al final del tramo: offset completo. */
    CHECK(aura_pattern_marquee_offset(2000, 2000, 5000, 100) == 0);
    CHECK(aura_pattern_marquee_offset(2000 + 2500, 2000, 5000, 100) == 50);
    CHECK(aura_pattern_marquee_offset(2000 + 4999, 2000, 5000, 100) == 99);
}

static void test_marquee_loops(void)
{
    /* Al completar el ciclo (2000+5000=7000ms) vuelve a la fase
     * estatica -- "el ciclo se repite indefinidamente". */
    CHECK(aura_pattern_marquee_offset(7000, 2000, 5000, 100) == 0);
    CHECK(aura_pattern_marquee_offset(7000 + 1000, 2000, 5000, 100) == 0);
    CHECK(aura_pattern_marquee_offset(7000 + 2000 + 2500, 2000, 5000, 100) == 50);
}

static void test_marquee_no_scroll_phase_is_noop(void)
{
    CHECK(aura_pattern_marquee_offset(5000, 2000, 0, 100) == 0);
}

static void test_fade_on_idle_shape(void)
{
    /* Fade in lineal 0..256 durante fade_in_ms. */
    CHECK(aura_pattern_fade_on_idle_alpha_256(0, 500, 1500, 500) == 0);
    CHECK(aura_pattern_fade_on_idle_alpha_256(250, 500, 1500, 500) == 128);
    /* Hold: alpha=256 fijo durante hold_ms despues del fade-in. */
    CHECK(aura_pattern_fade_on_idle_alpha_256(500, 500, 1500, 500) == 256);
    CHECK(aura_pattern_fade_on_idle_alpha_256(500 + 1499, 500, 1500, 500) == 256);
    /* Fade out lineal 256..0 tras fade_in+hold. */
    CHECK(aura_pattern_fade_on_idle_alpha_256(500 + 1500, 500, 1500, 500) == 256);
    CHECK(aura_pattern_fade_on_idle_alpha_256(500 + 1500 + 250, 500, 1500, 500) == 128);
    CHECK(aura_pattern_fade_on_idle_alpha_256(500 + 1500 + 500, 500, 1500, 500) == 0);
    /* Bien pasado el fade-out: se queda en 0, no se vuelve negativo ni
     * reinicia sola (a diferencia de Marquee, esto no es un loop). */
    CHECK(aura_pattern_fade_on_idle_alpha_256(500 + 1500 + 5000, 500, 1500, 500) == 0);
}

static void test_fade_on_idle_zero_fade_in(void)
{
    /* fade_in_ms=0 -- aparece de golpe en vez de dividir por cero. */
    CHECK(aura_pattern_fade_on_idle_alpha_256(0, 0, 1500, 500) == 256);
}

static void test_drift_reaches_center(void)
{
    aura_pattern_point_t p = aura_pattern_drift_pos(0, 50, 7000, 7000);
    CHECK(p.dx == 0 && p.dy == 0);

    p = aura_pattern_drift_pos(90, 50, 999999, 7000); /* mas alla de la duracion: sigue en el centro */
    CHECK(p.dx == 0 && p.dy == 0);
}

static void test_drift_starts_offset(void)
{
    /* angulo 0 = derecha pura (cos=1,sin=0): arranca a distance_px a la
     * derecha del centro. */
    aura_pattern_point_t p = aura_pattern_drift_pos(0, 100, 0, 7000);
    CHECK(p.dx == 100 && p.dy == 0);

    /* angulo 90 = abajo pura (cos=0,sin=1 en screen-space y-abajo). */
    p = aura_pattern_drift_pos(90, 100, 0, 7000);
    CHECK(p.dx == 0 && p.dy == 100);

    /* angulo 180 = izquierda pura. */
    p = aura_pattern_drift_pos(180, 100, 0, 7000);
    CHECK(p.dx == -100 && p.dy == 0);
}

static void test_drift_midpoint_is_half_distance(void)
{
    aura_pattern_point_t p = aura_pattern_drift_pos(0, 100, 3500, 7000);
    CHECK(p.dx == 50 && p.dy == 0);
}

static void test_drift_all_8_documented_angles_are_valid(void)
{
    /* AURA_DS_METRICS_COVER_DRIFT_DIRECTIONS_DEG (tokens.json, D-086):
     * ninguno de los 8 debe caer en el fallback "angulo invalido". */
    static const int angles[] = { 0, 60, 90, 120, 180, 240, 270, 300 };
    size_t i;
    for (i = 0; i < sizeof(angles) / sizeof(angles[0]); i++)
    {
        aura_pattern_point_t p = aura_pattern_drift_pos(angles[i], 100, 0, 7000);
        CHECK(p.dx != 0 || p.dy != 0 || angles[i] == 0);
        /* angulo 0 arranca en (100,0) -- dx!=0 ya lo cubre; el resto
         * arrancan con alguna componente no nula siempre a distance=100. */
    }
}

static void test_drift_unknown_angle_is_noop(void)
{
    aura_pattern_point_t p = aura_pattern_drift_pos(45, 100, 0, 7000);
    CHECK(p.dx == 0 && p.dy == 0);
}

/* D-257: aura_pattern_drift_pos() es ahora un envoltorio de
 * aura_pattern_drift_pos_hp() (dx256/dy256 = pixeles*256, sin
 * truncar). Estas pruebas cubren la version de alta precision --
 * confirman que sigue de acuerdo con el envoltorio en los mismos
 * puntos exactos que ya prueban las 5 de arriba, y ademas demuestran
 * que retiene precision subpixel real que la version entera pierde. */
static void test_drift_hp_reaches_center(void)
{
    aura_pattern_point_hp_t p = aura_pattern_drift_pos_hp(0, 50, 7000, 7000);
    CHECK(p.dx256 == 0 && p.dy256 == 0);

    p = aura_pattern_drift_pos_hp(90, 50, 999999, 7000);
    CHECK(p.dx256 == 0 && p.dy256 == 0);
}

static void test_drift_hp_starts_offset(void)
{
    /* A t=0 no hay perdida de precision posible (remain_ms==duration_ms
     * divide exacto) -- dx256 debe ser distance_px*256 exacto. */
    aura_pattern_point_hp_t p = aura_pattern_drift_pos_hp(0, 100, 0, 7000);
    CHECK(p.dx256 == 100 * 256 && p.dy256 == 0);

    p = aura_pattern_drift_pos_hp(90, 100, 0, 7000);
    CHECK(p.dx256 == 0 && p.dy256 == 100 * 256);

    p = aura_pattern_drift_pos_hp(180, 100, 0, 7000);
    CHECK(p.dx256 == -100 * 256 && p.dy256 == 0);
}

static void test_drift_hp_matches_int_wrapper_at_tested_points(void)
{
    /* En los mismos puntos "redondos" que ya prueban las 5 pruebas
     * enteras de arriba, hp/256 debe coincidir exacto con el resultado
     * entero -- el envoltorio no cambio nada observable ahi. */
    int angles[] = { 0, 60, 90, 120, 180, 240, 270, 300 };
    size_t i;
    for (i = 0; i < sizeof(angles) / sizeof(angles[0]); i++)
    {
        aura_pattern_point_t pi = aura_pattern_drift_pos(angles[i], 100, 3500, 7000);
        aura_pattern_point_hp_t ph = aura_pattern_drift_pos_hp(angles[i], 100, 3500, 7000);
        CHECK(ph.dx256 / 256 == pi.dx);
        CHECK(ph.dy256 / 256 == pi.dy);
    }
}

static void test_drift_hp_retains_subpixel_precision_lost_by_int_version(void)
{
    /* distance_px=1, a 100ms de arrancar un ciclo de 7000ms: la version
     * entera trunca dist_now a 0 (6900/7000 < 1), perdiendo el
     * desplazamiento entero -- la version hp retiene ~0.98px reales
     * (252/256) que la version entera descarta por completo. Prueba
     * exactamente el motivo de D-257: el truncamiento encadenado de la
     * version entera puede perder mas de un pixel de informacion real
     * en casos de distancia chica/angulo lento, la version hp no. */
    aura_pattern_point_t pi = aura_pattern_drift_pos(0, 1, 100, 7000);
    aura_pattern_point_hp_t ph = aura_pattern_drift_pos_hp(0, 1, 100, 7000);

    CHECK(pi.dx == 0); /* la version entera efectivamente pierde este pixel */
    CHECK(ph.dx256 == 252); /* la version hp lo retiene, en punto fijo */
}

static void test_drift_hp_unknown_angle_is_noop(void)
{
    aura_pattern_point_hp_t p = aura_pattern_drift_pos_hp(45, 100, 0, 7000);
    CHECK(p.dx256 == 0 && p.dy256 == 0);
}

static void test_push_and_drop_forward(void)
{
    /* reverse=0: Fase A (push, 100ms) primero, Fase B (drop, 50ms) despues. */
    aura_push_drop_state_t s = aura_pattern_push_and_drop(0, 100, 50, 0);
    CHECK(s.phase == AURA_PUSH_DROP_PHASE_A);
    CHECK(s.progress_256 == 0);

    s = aura_pattern_push_and_drop(50, 100, 50, 0);
    CHECK(s.phase == AURA_PUSH_DROP_PHASE_A);
    CHECK(s.progress_256 == 128);

    s = aura_pattern_push_and_drop(100, 100, 50, 0);
    CHECK(s.phase == AURA_PUSH_DROP_PHASE_B);
    CHECK(s.progress_256 == 0);

    s = aura_pattern_push_and_drop(100 + 25, 100, 50, 0);
    CHECK(s.phase == AURA_PUSH_DROP_PHASE_B);
    CHECK(s.progress_256 == 128);

    s = aura_pattern_push_and_drop(100 + 50, 100, 50, 0);
    CHECK(s.phase == AURA_PUSH_DROP_DONE);
    CHECK(s.progress_256 == 256);

    s = aura_pattern_push_and_drop(999999, 100, 50, 0);
    CHECK(s.phase == AURA_PUSH_DROP_DONE);
}

static void test_push_and_drop_reverse_swaps_order_and_duration(void)
{
    /* reverse=1 (G6, espejo provisional de (full)->(split)): Fase A
     * dura lo que antes duraba el drop (50ms), Fase B lo que antes
     * duraba el push (100ms) -- orden Y duracion invertidos. */
    aura_push_drop_state_t s = aura_pattern_push_and_drop(0, 100, 50, 1);
    CHECK(s.phase == AURA_PUSH_DROP_PHASE_A);

    s = aura_pattern_push_and_drop(49, 100, 50, 1);
    CHECK(s.phase == AURA_PUSH_DROP_PHASE_A);

    s = aura_pattern_push_and_drop(50, 100, 50, 1);
    CHECK(s.phase == AURA_PUSH_DROP_PHASE_B);
    CHECK(s.progress_256 == 0);

    s = aura_pattern_push_and_drop(50 + 100, 100, 50, 1);
    CHECK(s.phase == AURA_PUSH_DROP_DONE);
}

static void test_push_and_drop_zero_duration_phase_skips_instantly(void)
{
    aura_push_drop_state_t s = aura_pattern_push_and_drop(0, 0, 50, 0);
    CHECK(s.phase == AURA_PUSH_DROP_PHASE_B);
    CHECK(s.progress_256 == 0);
}

int main(void)
{
    test_lerp();
    test_marquee_static_phase();
    test_marquee_scroll_phase();
    test_marquee_loops();
    test_marquee_no_scroll_phase_is_noop();
    test_fade_on_idle_shape();
    test_fade_on_idle_zero_fade_in();
    test_drift_reaches_center();
    test_drift_starts_offset();
    test_drift_midpoint_is_half_distance();
    test_drift_all_8_documented_angles_are_valid();
    test_drift_unknown_angle_is_noop();
    test_drift_hp_reaches_center();
    test_drift_hp_starts_offset();
    test_drift_hp_matches_int_wrapper_at_tested_points();
    test_drift_hp_retains_subpixel_precision_lost_by_int_version();
    test_drift_hp_unknown_angle_is_noop();
    test_push_and_drop_forward();
    test_push_and_drop_reverse_swaps_order_and_duration();
    test_push_and_drop_zero_duration_phase_skips_instantly();

    printf("%d/%d checks OK\n", checks - failures, checks);
    return failures ? 1 : 0;
}
