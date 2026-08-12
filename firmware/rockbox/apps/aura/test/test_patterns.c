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

    printf("%d/%d checks OK\n", checks - failures, checks);
    return failures ? 1 : 0;
}
