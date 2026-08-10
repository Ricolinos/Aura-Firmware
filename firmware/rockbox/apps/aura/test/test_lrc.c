/* Tests host-side del parser de letras sincronizadas (aura_lrc.c).
 * Ejecutar con `make -C apps/aura/test`. */
#include <stdio.h>
#include <string.h>
#include "../aura_lrc.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void test_basic_single_line(void)
{
    aura_lrc_t lrc;
    int n = aura_lrc_parse("[00:12.34]Hola mundo\n", &lrc);

    CHECK(n == 1);
    CHECK(lrc.count == 1);
    CHECK(lrc.lines[0].timestamp_ms == 12340);
    CHECK(strcmp(lrc.lines[0].text, "Hola mundo") == 0);
}

static void test_metadata_lines_ignored(void)
{
    aura_lrc_t lrc;
    const char *src =
        "[ar:Artista de prueba]\n"
        "[ti:Titulo]\n"
        "[al:Album]\n"
        "[00:05.00]Primera linea\n";

    int n = aura_lrc_parse(src, &lrc);

    CHECK(n == 1);
    CHECK(lrc.lines[0].timestamp_ms == 5000);
    CHECK(strcmp(lrc.lines[0].text, "Primera linea") == 0);
}

static void test_multiple_timestamps_same_line(void)
{
    aura_lrc_t lrc;
    int n = aura_lrc_parse("[00:10.00][00:40.00]Estribillo\n", &lrc);

    CHECK(n == 2);
    CHECK(lrc.lines[0].timestamp_ms == 10000);
    CHECK(strcmp(lrc.lines[0].text, "Estribillo") == 0);
    CHECK(lrc.lines[1].timestamp_ms == 40000);
    CHECK(strcmp(lrc.lines[1].text, "Estribillo") == 0);
}

static void test_out_of_order_input_gets_sorted(void)
{
    aura_lrc_t lrc;
    const char *src =
        "[00:30.00]Tercera\n"
        "[00:05.00]Primera\n"
        "[00:15.00]Segunda\n";

    int n = aura_lrc_parse(src, &lrc);

    CHECK(n == 3);
    CHECK(lrc.lines[0].timestamp_ms == 5000);
    CHECK(strcmp(lrc.lines[0].text, "Primera") == 0);
    CHECK(lrc.lines[1].timestamp_ms == 15000);
    CHECK(strcmp(lrc.lines[1].text, "Segunda") == 0);
    CHECK(lrc.lines[2].timestamp_ms == 30000);
    CHECK(strcmp(lrc.lines[2].text, "Tercera") == 0);
}

static void test_fraction_digit_counts(void)
{
    aura_lrc_t lrc;
    const char *src =
        "[00:01.5]Un decimo\n"
        "[00:02.50]Un centesimo\n"
        "[00:03.500]Un milisegundo\n";

    int n = aura_lrc_parse(src, &lrc);

    CHECK(n == 3);
    CHECK(lrc.lines[0].timestamp_ms == 1500);
    CHECK(lrc.lines[1].timestamp_ms == 2500);
    CHECK(lrc.lines[2].timestamp_ms == 3500);
}

static void test_timestamp_without_fraction(void)
{
    aura_lrc_t lrc;
    int n = aura_lrc_parse("[01:02]Sin fraccion\n", &lrc);

    CHECK(n == 1);
    CHECK(lrc.lines[0].timestamp_ms == 62000);
}

static void test_empty_lyric_line_is_kept(void)
{
    /* Hueco instrumental: timestamp sin texto es valido y se conserva. */
    aura_lrc_t lrc;
    int n = aura_lrc_parse("[00:05.00]\n[00:10.00]Canta\n", &lrc);

    CHECK(n == 2);
    CHECK(strcmp(lrc.lines[0].text, "") == 0);
    CHECK(strcmp(lrc.lines[1].text, "Canta") == 0);
}

static void test_unclosed_bracket_does_not_crash(void)
{
    aura_lrc_t lrc;
    int n = aura_lrc_parse("[00:05.00 sin cerrar\n[00:10.00]Ok\n", &lrc);

    /* La primera linea es basura (corchete sin cerrar -> ningun tag
     * valido -> se descarta); la segunda debe parsear normalmente. */
    CHECK(n == 1);
    CHECK(strcmp(lrc.lines[0].text, "Ok") == 0);
}

static void test_crlf_line_endings(void)
{
    aura_lrc_t lrc;
    int n = aura_lrc_parse("[00:01.00]Linea uno\r\n[00:02.00]Linea dos\r\n", &lrc);

    CHECK(n == 2);
    CHECK(strcmp(lrc.lines[0].text, "Linea uno") == 0);
    CHECK(strcmp(lrc.lines[1].text, "Linea dos") == 0);
}

static void test_empty_input(void)
{
    aura_lrc_t lrc;
    int n = aura_lrc_parse("", &lrc);
    CHECK(n == 0);
    CHECK(lrc.count == 0);
}

static void test_find_active_line(void)
{
    aura_lrc_t lrc;
    aura_lrc_parse(
        "[00:10.00]A\n[00:20.00]B\n[00:30.00]C\n", &lrc);

    CHECK(aura_lrc_find_active_line(&lrc, 0) == -1);
    CHECK(aura_lrc_find_active_line(&lrc, 9999) == -1);
    CHECK(aura_lrc_find_active_line(&lrc, 10000) == 0);
    CHECK(aura_lrc_find_active_line(&lrc, 15000) == 0);
    CHECK(aura_lrc_find_active_line(&lrc, 20000) == 1);
    CHECK(aura_lrc_find_active_line(&lrc, 29999) == 1);
    CHECK(aura_lrc_find_active_line(&lrc, 30000) == 2);
    CHECK(aura_lrc_find_active_line(&lrc, 999999) == 2);
}

static void test_find_active_line_empty(void)
{
    aura_lrc_t lrc;
    aura_lrc_parse("[ar:Solo metadata]\n", &lrc);

    CHECK(lrc.count == 0);
    CHECK(aura_lrc_find_active_line(&lrc, 5000) == -1);
}

int main(void)
{
    test_basic_single_line();
    test_metadata_lines_ignored();
    test_multiple_timestamps_same_line();
    test_out_of_order_input_gets_sorted();
    test_fraction_digit_counts();
    test_timestamp_without_fraction();
    test_empty_lyric_line_is_kept();
    test_unclosed_bracket_does_not_crash();
    test_crlf_line_endings();
    test_empty_input();
    test_find_active_line();
    test_find_active_line_empty();

    printf("%d/%d checks OK\n", checks - failures, checks);
    if (failures)
    {
        printf("%d FALLO(S)\n", failures);
        return 1;
    }
    printf("TODO OK\n");
    return 0;
}
