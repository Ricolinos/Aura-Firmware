#include <stdio.h>
#include "../aura_color.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void test_rgb24_roundtrip(void)
{
    aura_rgb_t c = aura_color_from_rgb24(0xFF2D55);
    CHECK(c.r == 0xFF);
    CHECK(c.g == 0x2D);
    CHECK(c.b == 0x55);
    CHECK(aura_color_to_rgb24(c) == 0xFF2D55u);

    c = aura_color_from_rgb24(0x000000);
    CHECK(c.r == 0 && c.g == 0 && c.b == 0);

    c = aura_color_from_rgb24(0xFFFFFF);
    CHECK(c.r == 255 && c.g == 255 && c.b == 255);
}

static void test_blend_extremes(void)
{
    aura_rgb_t accent = aura_color_from_rgb24(0xFF2D55);
    aura_rgb_t white = aura_color_from_rgb24(0xFFFFFF);
    aura_rgb_t black = aura_color_from_rgb24(0x000000);

    aura_rgb_t at0 = aura_color_blend_pct(accent, white, 0);
    CHECK(at0.r == accent.r && at0.g == accent.g && at0.b == accent.b);

    aura_rgb_t at100 = aura_color_blend_pct(accent, white, 100);
    CHECK(at100.r == white.r && at100.g == white.g && at100.b == white.b);

    aura_rgb_t dark100 = aura_color_blend_pct(accent, black, 100);
    CHECK(dark100.r == 0 && dark100.g == 0 && dark100.b == 0);
}

static void test_blend_midpoint(void)
{
    /* Rojo puro (255,0,0) hacia blanco al 50% -> ~(255,128,128). */
    aura_rgb_t red = { 255, 0, 0 };
    aura_rgb_t white = { 255, 255, 255 };
    aura_rgb_t mid = aura_color_blend_pct(red, white, 50);

    CHECK(mid.r == 255);
    CHECK(mid.g >= 126 && mid.g <= 130);
    CHECK(mid.b >= 126 && mid.b <= 130);
}

static void test_blend_darken_lighten_are_symmetric_in_pct(void)
{
    /* AURA_DS_COLOR_ACCENT_DERIVED_LIGHTEN_PCT y _DARKEN_PCT son ambos
     * 25 (D-086) -- la distancia recorrida desde el acento hacia blanco
     * y hacia negro debe ser la misma magnitud de porcentaje. */
    aura_rgb_t accent = aura_color_from_rgb24(0xFF2D55);
    aura_rgb_t white = aura_color_from_rgb24(0xFFFFFF);
    aura_rgb_t black = aura_color_from_rgb24(0x000000);

    aura_rgb_t lighter = aura_color_blend_pct(accent, white, 25);
    aura_rgb_t darker = aura_color_blend_pct(accent, black, 25);

    CHECK(lighter.r > accent.r || lighter.r == 255);
    CHECK(lighter.g > accent.g);
    CHECK(lighter.b > accent.b);

    CHECK(darker.r < accent.r);
    CHECK(darker.g < accent.g || accent.g == 0);
    CHECK(darker.b < accent.b);
}

static void test_pct_clamped(void)
{
    aura_rgb_t accent = aura_color_from_rgb24(0xFF2D55);
    aura_rgb_t white = aura_color_from_rgb24(0xFFFFFF);

    aura_rgb_t over = aura_color_blend_pct(accent, white, 150);
    aura_rgb_t at100 = aura_color_blend_pct(accent, white, 100);
    CHECK(over.r == at100.r && over.g == at100.g && over.b == at100.b);

    aura_rgb_t under = aura_color_blend_pct(accent, white, -30);
    aura_rgb_t at0 = aura_color_blend_pct(accent, white, 0);
    CHECK(under.r == at0.r && under.g == at0.g && under.b == at0.b);
}

int main(void)
{
    test_rgb24_roundtrip();
    test_blend_extremes();
    test_blend_midpoint();
    test_blend_darken_lighten_are_symmetric_in_pct();
    test_pct_clamped();

    printf("%d/%d checks OK\n", checks - failures, checks);
    return failures ? 1 : 0;
}
