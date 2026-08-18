#include <stdio.h>
#include <string.h>
#include "../aura_device_name.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

int main(void)
{
    char out[AURA_DEVICE_NAME_BUF];

    CHECK(aura_device_name_sanitize("iPod de Ricardo", out, sizeof(out)) == 15);
    CHECK(!strcmp(out, "iPod de Ricardo"));

    /* espacios en los extremos y colapsados */
    CHECK(aura_device_name_sanitize("   iPod   de  Ana \t ", out, sizeof(out)) == 11);
    CHECK(!strcmp(out, "iPod de Ana"));

    /* control chars fuera */
    aura_device_name_sanitize("iPod\x01 de\r\n Ana", out, sizeof(out));
    CHECK(!strcmp(out, "iPod de Ana"));

    /* UTF-8 (2 bytes) se conserva */
    aura_device_name_sanitize("iPod de Ñoño", out, sizeof(out));
    CHECK(!strcmp(out, "iPod de Ñoño"));

    /* truncado a 48 bytes SIN partir secuencia: 46 'a' + "ñ" (2 bytes) = 48 -> cabe */
    {
        char in[128];
        memset(in, 'a', 46); in[46] = '\0'; strcat(in, "\xC3\xB1" "zzz");
        CHECK(aura_device_name_sanitize(in, out, sizeof(out)) == 48);
        CHECK(out[46] == (char)0xC3 && out[47] == (char)0xB1 && out[48] == '\0');
        /* 47 'a' + "ñ" no cabe entera: se corta ANTES de la ñ */
        memset(in, 'a', 47); in[47] = '\0'; strcat(in, "\xC3\xB1");
        CHECK(aura_device_name_sanitize(in, out, sizeof(out)) == 47);
        CHECK(out[47] == '\0');
    }

    /* vacio / NULL / solo espacios */
    CHECK(aura_device_name_sanitize("", out, sizeof(out)) == 0 && out[0] == '\0');
    CHECK(aura_device_name_sanitize(NULL, out, sizeof(out)) == 0);
    CHECK(aura_device_name_sanitize("   ", out, sizeof(out)) == 0);

    /* buffer chico: respeta outsz */
    {
        char small[6];
        CHECK(aura_device_name_sanitize("abcdefgh", small, sizeof(small)) == 5);
        CHECK(!strcmp(small, "abcde"));
    }

    printf("%d/%d checks OK\n", checks - failures, checks);
    return failures ? 1 : 0;
}
