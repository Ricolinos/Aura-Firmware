#include <stdio.h>
#include <string.h>
#include "../aura_artist_images_parse.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void test_basic_line(void)
{
    char file[128], artist[64];
    CHECK(aura_artist_images_parse_line("the-1975.jpg: The 1975", file, sizeof(file), artist, sizeof(artist)));
    CHECK(!strcmp(file, "the-1975.jpg"));
    CHECK(!strcmp(artist, "The 1975"));
}

static void test_artist_with_colon(void)
{
    char file[128], artist[64];
    CHECK(aura_artist_images_parse_line("panic.jpg: Panic! At The Disco: Live", file, sizeof(file), artist, sizeof(artist)));
    CHECK(!strcmp(file, "panic.jpg"));
    CHECK(!strcmp(artist, "Panic! At The Disco: Live"));
}

static void test_trims_whitespace_around_both_fields(void)
{
    char file[128], artist[64];
    CHECK(aura_artist_images_parse_line("  gorillaz.jpg  :   Gorillaz  ", file, sizeof(file), artist, sizeof(artist)));
    CHECK(!strcmp(file, "gorillaz.jpg"));
    CHECK(!strcmp(artist, "Gorillaz"));
}

static void test_comment_line_ignored(void)
{
    char file[128], artist[64];
    CHECK(!aura_artist_images_parse_line("# aura-artist-images v1", file, sizeof(file), artist, sizeof(artist)));
    CHECK(file[0] == '\0');
    CHECK(artist[0] == '\0');
}

static void test_comment_after_leading_whitespace_ignored(void)
{
    char file[128], artist[64];
    CHECK(!aura_artist_images_parse_line("   # comentario", file, sizeof(file), artist, sizeof(artist)));
}

static void test_empty_line_ignored(void)
{
    char file[128], artist[64];
    CHECK(!aura_artist_images_parse_line("", file, sizeof(file), artist, sizeof(artist)));
    CHECK(!aura_artist_images_parse_line("   ", file, sizeof(file), artist, sizeof(artist)));
}

static void test_line_without_colon_ignored(void)
{
    char file[128], artist[64];
    CHECK(!aura_artist_images_parse_line("sin dos puntos", file, sizeof(file), artist, sizeof(artist)));
}

static void test_empty_filename_ignored(void)
{
    char file[128], artist[64];
    CHECK(!aura_artist_images_parse_line(" : Artista", file, sizeof(file), artist, sizeof(artist)));
}

static void test_empty_artist_ignored(void)
{
    char file[128], artist[64];
    CHECK(!aura_artist_images_parse_line("archivo.jpg: ", file, sizeof(file), artist, sizeof(artist)));
}

static void test_null_line_is_safe(void)
{
    char file[128], artist[64];
    CHECK(!aura_artist_images_parse_line(NULL, file, sizeof(file), artist, sizeof(artist)));
    CHECK(file[0] == '\0');
    CHECK(artist[0] == '\0');
}

static void test_truncates_to_buffer_size(void)
{
    char file[6], artist[5]; /* "abcd\0" cabe, "abcde\0" no */
    CHECK(aura_artist_images_parse_line("abcdefgh.jpg: abcdefgh", file, sizeof(file), artist, sizeof(artist)));
    CHECK(strlen(file) == 5);
    CHECK(strlen(artist) == 4);
}

static void test_multiple_lines_same_file_both_parse(void)
{
    /* Contrato §D.3: varias lineas pueden apuntar al mismo archivo --
     * el parser de UNA linea no sabe ni le importa, cada llamada es
     * independiente; la deduplicacion/indice vive en aura_artist_images.c. */
    char file1[128], artist1[64], file2[128], artist2[64];
    CHECK(aura_artist_images_parse_line("the-1975.jpg: The 1975", file1, sizeof(file1), artist1, sizeof(artist1)));
    CHECK(aura_artist_images_parse_line("the-1975.jpg: The 1975 feat. Phoebe Bridgers", file2, sizeof(file2), artist2, sizeof(artist2)));
    CHECK(!strcmp(file1, file2));
    CHECK(strcmp(artist1, artist2) != 0);
}

int main(void)
{
    test_basic_line();
    test_artist_with_colon();
    test_trims_whitespace_around_both_fields();
    test_comment_line_ignored();
    test_comment_after_leading_whitespace_ignored();
    test_empty_line_ignored();
    test_line_without_colon_ignored();
    test_empty_filename_ignored();
    test_empty_artist_ignored();
    test_null_line_is_safe();
    test_truncates_to_buffer_size();
    test_multiple_lines_same_file_both_parse();

    printf("%d/%d checks OK\n", checks - failures, checks);
    return failures ? 1 : 0;
}
