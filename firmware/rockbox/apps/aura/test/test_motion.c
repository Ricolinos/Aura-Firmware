/* Tests host-side de las tablas de easing (aura_motion.c). Sin
 * dependencias de Rockbox: compila y corre nativo en el Mac.
 * Ejecutar con `make -C apps/aura/test`. */
#include <stdio.h>
#include "../aura_motion.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void test_linear_bounds(void)
{
    CHECK(aura_motion_linear(-5, 100) == 0);
    CHECK(aura_motion_linear(0, 100) == 0);
    CHECK(aura_motion_linear(100, 100) == 256);
    CHECK(aura_motion_linear(200, 100) == 256);
    CHECK(aura_motion_linear(0, 0) == 0);   /* duracion invalida: no divide por cero */
}

static void test_linear_is_monotonic_and_midpoint_is_half(void)
{
    int prev = -1;
    long e;

    CHECK(aura_motion_linear(50, 100) == 128);

    for (e = 0; e <= 100; e += 5)
    {
        int v = aura_motion_linear(e, 100);
        CHECK(v >= prev);
        prev = v;
    }
}

static void test_spring_bounds(void)
{
    CHECK(aura_motion_spring(-5, 380) == 0);
    CHECK(aura_motion_spring(0, 380) == 0);
    CHECK(aura_motion_spring(380, 380) == 256);
    CHECK(aura_motion_spring(1000, 380) == 256);
    CHECK(aura_motion_spring(0, 0) == 0);
}

static void test_spring_overshoots_then_settles(void)
{
    long e;
    int max_v = 0;
    int last_v = 0;

    /* La curva de sobrepaso debe superar 256 en algun punto intermedio
     * (SS6/SS9.2: "resorte corto con sobrepaso") y volver a asentarse
     * exactamente en 256 al final -- si nunca pasa de 256 no hay
     * sobrepaso, y si no vuelve a 256 la fila quedaria mal alineada. */
    for (e = 0; e <= 380; e += 4)
    {
        int v = aura_motion_spring(e, 380);
        if (v > max_v)
            max_v = v;
        last_v = v;
    }

    CHECK(max_v > 256);
    CHECK(max_v < 320); /* sobrepaso perceptible, no "de juguete" (riesgo del plan) */
    CHECK(last_v == 256);
    CHECK(aura_motion_spring(380, 380) == 256);
}

static void test_spring_starts_at_zero_and_is_smooth(void)
{
    long e;
    int prev = -1000;

    CHECK(aura_motion_spring(1, 380) >= 0);

    /* No exige monotonia (la curva SI baja despues del pico), pero si
     * exige que no haya saltos bruscos entre muestras consecutivas --
     * cada paso de 4 ticks no deberia mover mas de ~40 unidades de 256,
     * o la interpolacion entre muestras de la tabla estaria rota. */
    for (e = 0; e <= 380; e += 4)
    {
        int v = aura_motion_spring(e, 380);
        if (prev > -1000)
            CHECK((v - prev > -40) && (v - prev < 40));
        prev = v;
    }
}

int main(void)
{
    test_linear_bounds();
    test_linear_is_monotonic_and_midpoint_is_half();
    test_spring_bounds();
    test_spring_overshoots_then_settles();
    test_spring_starts_at_zero_and_is_smooth();

    printf("%d/%d checks OK\n", checks - failures, checks);
    if (failures)
    {
        printf("%d FALLO(S)\n", failures);
        return 1;
    }
    printf("TODO OK\n");
    return 0;
}
