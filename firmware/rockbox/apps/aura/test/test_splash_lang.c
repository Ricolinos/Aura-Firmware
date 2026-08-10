/* Tests host-side de la reescritura de mensajes de splash() al tono de
 * Aura (aura_splash_lang.c). Ejecutar con `make -C apps/aura/test`.
 *
 * Define su propia instancia minima de `aura_settings` (en vez de
 * enlazar aura_settings.c, que arrastra headers reales de Rockbox) --
 * aura_settings.h en si mismo no depende de nada del firmware, solo
 * declara el tipo. */
#include <stdio.h>
#include <string.h>
#include "../aura_settings.h"
#include "../aura_splash_lang.h"

aura_settings_t aura_settings;

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void translate(const char *input, char *out, size_t outsz)
{
    strncpy(out, input, outsz - 1);
    out[outsz - 1] = '\0';
    aura_splash_translate(out, outsz);
}

static void test_exact_match_es(void)
{
    char buf[64];
    aura_settings.language = AURA_LANG_ES;

    translate("Loading...", buf, sizeof(buf));
    CHECK(strcmp(buf, "Cargando...") == 0);

    translate("Scanning disk...", buf, sizeof(buf));
    CHECK(strcmp(buf, "Preparando el disco...") == 0);

    translate("Database is not ready", buf, sizeof(buf));
    CHECK(strcmp(buf, "Terminando de preparar la biblioteca...") == 0);
}

static void test_exact_match_en(void)
{
    char buf[64];
    aura_settings.language = AURA_LANG_EN;

    translate("Loading...", buf, sizeof(buf));
    CHECK(strcmp(buf, "Loading...") == 0);

    translate("Battery empty! RECHARGE! Shutting down...", buf, sizeof(buf));
    CHECK(strcmp(buf, "Battery empty. Plug in your charger.") == 0);
}

static void test_prefix_preserves_dynamic_suffix(void)
{
    char buf[64];
    aura_settings.language = AURA_LANG_ES;

    translate("Committing database [3/9]", buf, sizeof(buf));
    CHECK(strcmp(buf, "Preparando la biblioteca [3/9]") == 0);

    translate("Loading... (PLAY/PAUSE to abort)", buf, sizeof(buf));
    CHECK(strcmp(buf, "Cargando... (PLAY/PAUSE to abort)") == 0);
}

static void test_unmapped_text_passes_through(void)
{
    char buf[64];
    aura_settings.language = AURA_LANG_ES;

    translate("ATA error: -5", buf, sizeof(buf));
    CHECK(strcmp(buf, "ATA error: -5") == 0);

    translate("", buf, sizeof(buf));
    CHECK(strcmp(buf, "") == 0);
}

static void test_longer_prefix_checked_before_shorter(void)
{
    char buf[64];
    aura_settings.language = AURA_LANG_ES;

    /* "Loading..." (exacto) no debe "ganarle" por error a
     * "Loading... (" (prefijo) cuando el texto trae el parentesis. */
    translate("Loading... (STOP to abort)", buf, sizeof(buf));
    CHECK(strcmp(buf, "Cargando... (STOP to abort)") == 0);
}

int main(void)
{
    test_exact_match_es();
    test_exact_match_en();
    test_prefix_preserves_dynamic_suffix();
    test_unmapped_text_passes_through();
    test_longer_prefix_checked_before_shorter();

    printf("%d/%d checks OK\n", checks - failures, checks);
    return failures ? 1 : 0;
}
