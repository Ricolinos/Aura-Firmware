/* Tests host-side de la dinamica de rueda (aura_wheel.c). Sin
 * dependencias de Rockbox: compila y corre nativo en el Mac.
 * Ejecutar con `make -C apps/aura/test`. */
#include <stdio.h>
#include "../aura_wheel.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void test_slow_or_zero_is_always_one_step(void)
{
    CHECK(aura_wheel_step(0) == 1);
    CHECK(aura_wheel_step(-50) == 1);
    CHECK(aura_wheel_step(1) == 1);
    CHECK(aura_wheel_step(30) == 1);
}

static void test_step_is_monotonic_and_capped_at_three(void)
{
    int prev = 0;
    int v;

    for (v = 0; v <= 1000; v += 10)
    {
        int step = aura_wheel_step(v);
        CHECK(step >= prev);
        CHECK(step >= 1 && step <= 3);
        prev = step;
    }
}

static void test_step_at_threshold_is_three(void)
{
    CHECK(aura_wheel_step(AURA_WHEEL_LETTER_HOP_THRESHOLD_DEG_S) == 3);
    CHECK(aura_wheel_step(AURA_WHEEL_LETTER_HOP_THRESHOLD_DEG_S * 2) == 3);
    CHECK(aura_wheel_step(100000) == 3); /* nunca revienta con velocidades absurdas */
}

static void test_step_at_half_threshold_is_two(void)
{
    /* v^2 a mitad de umbral: 1 + round(2*0.25) = 1 + round(0.5) = 2 */
    CHECK(aura_wheel_step(AURA_WHEEL_LETTER_HOP_THRESHOLD_DEG_S / 2) == 2);
}

static void test_letter_hop_threshold(void)
{
    CHECK(!aura_wheel_should_hop_letters(0));
    CHECK(!aura_wheel_should_hop_letters(419));
    CHECK(!aura_wheel_should_hop_letters(AURA_WHEEL_LETTER_HOP_THRESHOLD_DEG_S));
    CHECK(aura_wheel_should_hop_letters(AURA_WHEEL_LETTER_HOP_THRESHOLD_DEG_S + 1));
    CHECK(aura_wheel_should_hop_letters(1000));
}

int main(void)
{
    test_slow_or_zero_is_always_one_step();
    test_step_is_monotonic_and_capped_at_three();
    test_step_at_threshold_is_three();
    test_step_at_half_threshold_is_two();
    test_letter_hop_threshold();

    printf("%d/%d checks OK\n", checks - failures, checks);
    if (failures)
    {
        printf("%d FALLO(S)\n", failures);
        return 1;
    }
    printf("TODO OK\n");
    return 0;
}
