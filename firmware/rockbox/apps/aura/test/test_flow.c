/* Tests host-side del nucleo matematico de Cover Flow (aura_flow.c).
 * Sin dependencias de Rockbox: compila y corre nativo en el Mac.
 * Ejecutar con `make -C apps/aura/test`.
 *
 * No hay una implementacion de referencia para comparar pixel a pixel
 * (el original vive dentro de un plugin que no corre en el host) -- los
 * tests verifican propiedades estructurales de la formula portada
 * (limites, terminacion, valores trigonometricos conocidos), no una
 * salida exacta contra pictureflow.c. */
#include <stdio.h>
#include "../aura_flow.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define ABS(x) ((x) < 0 ? -(x) : (x))

static void test_fixed_point_basics(void)
{
    CHECK(aura_flow_fmul(AURA_FLOW_ONE, AURA_FLOW_ONE) == AURA_FLOW_ONE);
    CHECK(aura_flow_fmul(AURA_FLOW_ONE, 0) == 0);
    CHECK(aura_flow_fmul(AURA_FLOW_ONE * 2, AURA_FLOW_HALF) == AURA_FLOW_ONE);

    CHECK(aura_flow_fdiv(AURA_FLOW_ONE, AURA_FLOW_ONE) == AURA_FLOW_ONE);
    CHECK(aura_flow_fdiv(0, AURA_FLOW_ONE) == 0);

    /* fmul(fdiv(a,b), b) ~= a, con tolerancia de redondeo de punto fijo. */
    {
        int a = AURA_FLOW_ONE * 7;
        int b = AURA_FLOW_ONE * 3;
        int roundtrip = aura_flow_fmul(aura_flow_fdiv(a, b), b);
        CHECK(ABS(roundtrip - a) < AURA_FLOW_ONE / 32);
    }
}

static void test_trig_known_values(void)
{
    const int TOL = 3; /* cuantizacion de la tabla de 33 muestras */

    CHECK(ABS(aura_flow_fsin(0) - 0) <= TOL);
    CHECK(ABS(aura_flow_fsin(256) - AURA_FLOW_ONE) <= TOL);   /* 90 grados */
    CHECK(ABS(aura_flow_fsin(512) - 0) <= TOL);                /* 180 grados */
    CHECK(ABS(aura_flow_fsin(768) - (-AURA_FLOW_ONE)) <= TOL); /* 270 grados */

    CHECK(ABS(aura_flow_fcos(0) - AURA_FLOW_ONE) <= TOL);
    CHECK(ABS(aura_flow_fcos(256) - 0) <= TOL);
    CHECK(ABS(aura_flow_fcos(512) - (-AURA_FLOW_ONE)) <= TOL);

    /* Identidad pitagorica aproximada en varios angulos (sin^2+cos^2 ~= 1). */
    {
        int angles[] = { 0, 100, 256, 400, 512, 700, 768, 900 };
        size_t i;
        for (i = 0; i < sizeof(angles) / sizeof(angles[0]); i++)
        {
            int s = aura_flow_fsin(angles[i]);
            int c = aura_flow_fcos(angles[i]);
            int sum = aura_flow_fmul(s, s) + aura_flow_fmul(c, c);
            CHECK(ABS(sum - AURA_FLOW_ONE) < AURA_FLOW_ONE / 16);
        }
    }
}

static void test_flat_centered_slide_is_visible_and_terminates(void)
{
    aura_flow_slide_t slide = { 0, 0, 0 };
    aura_flow_projection_t proj;
    int width = 100;
    int steps = 0;
    int last_column = -1;
    int monotonic = 1;

    aura_flow_begin_projection(&proj, &slide, width);
    CHECK(proj.screen_x < AURA_FLOW_SCREEN_W);

    do
    {
        int column = aura_flow_source_column(&proj);
        CHECK(column >= 0 && column < width);
        if (last_column >= 0 && column < last_column)
            monotonic = 0;
        last_column = column;
        steps++;
    }
    while (aura_flow_advance_column(&proj) && steps < AURA_FLOW_SCREEN_W + 10);

    CHECK(steps > 0);
    CHECK(steps <= AURA_FLOW_SCREEN_W); /* termina, no se cuelga */
    CHECK(monotonic); /* de frente y sin rotar, la fuente avanza siempre hacia adelante */
}

static void test_offscreen_slide_is_not_visible(void)
{
    /* "Muy a la derecha" pero dentro de un rango realista para punto
     * fijo de 32 bits -- CAM_DIST*cx no debe desbordar (cx en unidades
     * de pantalla, nunca cerca de INT_MAX/CAM_DIST en un flujo real). */
    aura_flow_slide_t slide = { 0, 0, AURA_FLOW_ONE * 1000 };
    aura_flow_projection_t proj;

    aura_flow_begin_projection(&proj, &slide, 100);
    CHECK(proj.screen_x >= AURA_FLOW_SCREEN_W);
}

static void test_rotated_slide_terminates(void)
{
    /* Un slide lateral (angulo ~45 grados, IANGLE_MAX/8=128) tambien
     * debe terminar en un numero acotado de columnas -- es el caso que
     * usa la recurrencia de Mobius (has_rotation), no el paso fijo. */
    aura_flow_slide_t slide = { 128, AURA_FLOW_ONE * 20, AURA_FLOW_ONE * 40 };
    aura_flow_projection_t proj;
    int width = 80;
    int steps = 0;

    aura_flow_begin_projection(&proj, &slide, width);
    if (proj.screen_x < AURA_FLOW_SCREEN_W)
    {
        do
        {
            int column = aura_flow_source_column(&proj);
            CHECK(column >= 0 && column < width);
            steps++;
        }
        while (aura_flow_advance_column(&proj) && steps < AURA_FLOW_SCREEN_W + 10);

        CHECK(steps > 0);
        CHECK(steps <= AURA_FLOW_SCREEN_W);
    }
}

static void test_vertical_scale_is_positive(void)
{
    aura_flow_slide_t slide = { 0, 0, 0 };
    aura_flow_projection_t proj;

    aura_flow_begin_projection(&proj, &slide, 100);
    CHECK(proj.screen_x < AURA_FLOW_SCREEN_W);
    CHECK(aura_flow_vertical_scale(&proj) > 0);
}

int main(void)
{
    test_fixed_point_basics();
    test_trig_known_values();
    test_flat_centered_slide_is_visible_and_terminates();
    test_offscreen_slide_is_not_visible();
    test_rotated_slide_terminates();
    test_vertical_scale_is_positive();

    printf("%d/%d checks OK\n", checks - failures, checks);
    if (failures)
    {
        printf("%d FALLO(S)\n", failures);
        return 1;
    }
    printf("TODO OK\n");
    return 0;
}
