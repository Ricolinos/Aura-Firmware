#include <stdio.h>
#include <string.h>
#include "../aura_shared_settings.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

/* Vector de prueba canonico -- CONTRATO-firmware-studio.md SS D.6 /
 * PLAN-ronda-ajustes-2-maestro.md SS A.3, LITERAL. Debe ser byte a byte
 * el mismo texto que usan los tests host de Metro-Aura y moonlit.aura:
 * si alguno de los tres firmwares interpreta este formato distinto, es
 * ahi donde tiene que fallar, no en produccion con el iPod del dueno. */
static const char *const VECTOR_A3 =
    "# aura-shared-settings v1\n"
    "rev: 7\n"
    "updated_by: metro\n"
    "screen_lock_enabled: 1\n"
    "screen_lock_pin: 0427\n"
    "screen_lock_require: 1min\n"
    "brightness: 32\n"
    "backlight_timeout: 10\n"
    "idle_poweroff: 20\n"
    "keyclick: 1\n"
    "volume_limit: -6\n"
    "replaygain: album\n"
    "language: fr\n"
    "appearance: light\n"
    "clave_futura: lo que sea\n";

static void test_init_is_all_absent(void)
{
    aura_shared_settings_t m;

    aura_shared_settings_init(&m);
    CHECK(m.rev == -1);
    CHECK(m.updated_by[0] == '\0');
    CHECK(m.screen_lock_enabled == -1);
    CHECK(m.screen_lock_pin[0] == '\0');
    CHECK(m.screen_lock_require == AURA_SHARED_LOCK_REQUIRE_COUNT);
    CHECK(m.brightness == -1);
    CHECK(m.backlight_timeout == -2);
    CHECK(m.idle_poweroff == -1);
    CHECK(m.keyclick == -1);
    CHECK(m.volume_limit == AURA_SHARED_SETTINGS_VOLUME_LIMIT_ABSENT);
    CHECK(m.replaygain == AURA_SHARED_REPLAYGAIN_COUNT);
    CHECK(m.language == AURA_SHARED_LANG_COUNT);
    CHECK(m.appearance == AURA_SHARED_APPEARANCE_COUNT);
    CHECK(m.unknown_count == 0);
}

/* Caso 1 del maestro SS A.3: "13 claves conocidas parseadas exactas,
 * clave_futura preservada al reescribir, rev 7". */
static void test_vector_a3_parses_all_13_known_keys(void)
{
    aura_shared_settings_t m;
    aura_shared_settings_status_t st;

    st = aura_shared_settings_parse(VECTOR_A3, &m);

    CHECK(st == AURA_SHARED_SETTINGS_OK);
    CHECK(m.rev == 7);
    CHECK(!strcmp(m.updated_by, "metro"));
    CHECK(m.screen_lock_enabled == 1);
    CHECK(!strcmp(m.screen_lock_pin, "0427"));
    CHECK(m.screen_lock_require == AURA_SHARED_LOCK_REQUIRE_1MIN);
    CHECK(m.brightness == 32);
    CHECK(m.backlight_timeout == 10);
    CHECK(m.idle_poweroff == 20);
    CHECK(m.keyclick == 1);
    CHECK(m.volume_limit == -6);
    CHECK(m.replaygain == AURA_SHARED_REPLAYGAIN_ALBUM);
    CHECK(m.language == AURA_SHARED_LANG_FR);
    CHECK(m.appearance == AURA_SHARED_APPEARANCE_LIGHT);
}

static void test_vector_a3_preserves_unknown_key_on_rewrite(void)
{
    aura_shared_settings_t m;
    char buf[1024];
    int n;

    aura_shared_settings_parse(VECTOR_A3, &m);
    CHECK(m.unknown_count == 1);
    CHECK(!strcmp(m.unknown[0].key, "clave_futura"));
    CHECK(!strcmp(m.unknown[0].value, "lo que sea"));

    n = aura_shared_settings_serialize(&m, buf, sizeof(buf));
    CHECK(n > 0);
    CHECK(strstr(buf, "clave_futura: lo que sea\n") != NULL);
    CHECK(strstr(buf, "rev: 7\n") != NULL);
}

/* Caso 2 del maestro SS A.3: "sin cabecera -> rechazado entero". */
static void test_missing_header_rejects_whole_file(void)
{
    aura_shared_settings_t m;
    aura_shared_settings_status_t st;
    static const char *const no_header =
        "rev: 7\n"
        "brightness: 32\n";

    st = aura_shared_settings_parse(no_header, &m);
    CHECK(st == AURA_SHARED_SETTINGS_NO_HEADER);
    /* El archivo entero se rechaza: nada de lo que traia debe haber
     * quedado aplicado, ni siquiera las claves que si eran validas. */
    CHECK(m.rev == -1);
    CHECK(m.brightness == -1);
}

static void test_header_from_a_future_version_also_rejects(void)
{
    aura_shared_settings_t m;
    aura_shared_settings_status_t st;
    static const char *const future_header =
        "# aura-shared-settings v2\n"
        "rev: 7\n";

    st = aura_shared_settings_parse(future_header, &m);
    CHECK(st == AURA_SHARED_SETTINGS_NO_HEADER);
}

/* Caso 3 del maestro SS A.3: "brightness: 999 -> solo esa clave se
 * ignora", el resto del archivo se sigue aplicando. */
static void test_out_of_range_value_ignores_only_that_key(void)
{
    aura_shared_settings_t m;
    aura_shared_settings_status_t st;
    static const char *const vector =
        "# aura-shared-settings v1\n"
        "rev: 7\n"
        "brightness: 999\n"
        "keyclick: 1\n";

    st = aura_shared_settings_parse(vector, &m);
    CHECK(st == AURA_SHARED_SETTINGS_OK);
    CHECK(m.rev == 7);
    CHECK(m.brightness == -1); /* fuera de rango: queda "ausente" */
    CHECK(m.keyclick == 1);    /* la clave siguiente SI se aplico */
}

static void test_unknown_enum_value_ignores_only_that_key(void)
{
    aura_shared_settings_t m;
    static const char *const vector =
        "# aura-shared-settings v1\n"
        "screen_lock_require: never_heard_of_it\n"
        "appearance: light\n";

    aura_shared_settings_parse(vector, &m);
    CHECK(m.screen_lock_require == AURA_SHARED_LOCK_REQUIRE_COUNT);
    CHECK(m.appearance == AURA_SHARED_APPEARANCE_LIGHT);
}

static void test_screen_lock_pin_rejects_non_4_digit(void)
{
    aura_shared_settings_t m;
    static const char *const vector =
        "# aura-shared-settings v1\n"
        "screen_lock_pin: 42\n";

    aura_shared_settings_parse(vector, &m);
    CHECK(m.screen_lock_pin[0] == '\0');
}

static void test_backlight_timeout_never_is_valid_minus_one(void)
{
    aura_shared_settings_t m;
    static const char *const vector =
        "# aura-shared-settings v1\n"
        "backlight_timeout: -1\n";

    aura_shared_settings_parse(vector, &m);
    /* -1 es "nunca", un valor VALIDO -- no debe confundirse con el
     * centinela de ausente (-2). */
    CHECK(m.backlight_timeout == -1);
}

static void test_unknown_key_budget_is_bounded(void)
{
    aura_shared_settings_t m;
    char vector[2048];
    char *p = vector;
    int i;

    p += sprintf(p, "%s\n", AURA_SHARED_SETTINGS_HEADER);
    for (i = 0; i < AURA_SHARED_SETTINGS_MAX_UNKNOWN + 3; i++)
        p += sprintf(p, "unknown_%d: x\n", i);

    aura_shared_settings_parse(vector, &m);
    CHECK(m.unknown_count == AURA_SHARED_SETTINGS_MAX_UNKNOWN);
}

static void test_round_trip_matches_field_by_field(void)
{
    aura_shared_settings_t m, reparsed;
    char buf[1024];

    aura_shared_settings_parse(VECTOR_A3, &m);
    aura_shared_settings_serialize(&m, buf, sizeof(buf));
    aura_shared_settings_parse(buf, &reparsed);

    CHECK(reparsed.rev == m.rev);
    CHECK(!strcmp(reparsed.updated_by, m.updated_by));
    CHECK(reparsed.screen_lock_enabled == m.screen_lock_enabled);
    CHECK(!strcmp(reparsed.screen_lock_pin, m.screen_lock_pin));
    CHECK(reparsed.screen_lock_require == m.screen_lock_require);
    CHECK(reparsed.brightness == m.brightness);
    CHECK(reparsed.backlight_timeout == m.backlight_timeout);
    CHECK(reparsed.idle_poweroff == m.idle_poweroff);
    CHECK(reparsed.keyclick == m.keyclick);
    CHECK(reparsed.volume_limit == m.volume_limit);
    CHECK(reparsed.replaygain == m.replaygain);
    CHECK(reparsed.language == m.language);
    CHECK(reparsed.appearance == m.appearance);
    CHECK(reparsed.unknown_count == m.unknown_count);
}

int main(void)
{
    test_init_is_all_absent();
    test_vector_a3_parses_all_13_known_keys();
    test_vector_a3_preserves_unknown_key_on_rewrite();
    test_missing_header_rejects_whole_file();
    test_header_from_a_future_version_also_rejects();
    test_out_of_range_value_ignores_only_that_key();
    test_unknown_enum_value_ignores_only_that_key();
    test_screen_lock_pin_rejects_non_4_digit();
    test_backlight_timeout_never_is_valid_minus_one();
    test_unknown_key_budget_is_bounded();
    test_round_trip_matches_field_by_field();

    if (failures)
    {
        printf("%d/%d checks failed\n", failures, checks);
        return 1;
    }
    printf("%d/%d checks OK\n", checks, checks);
    return 0;
}
