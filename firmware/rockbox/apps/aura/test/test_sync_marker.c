#include <stdio.h>
#include <string.h>
#include "../aura_sync_marker.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void test_init(void)
{
    aura_sync_marker_t m;
    aura_sync_marker_init(&m);
    CHECK(m.version == -1);
    CHECK(m.timestamp[0] == '\0');
    CHECK(!m.music && !m.video && !m.images);
    CHECK(m.attempts == 0);
    CHECK(!aura_sync_marker_has_work(&m));
}

static void test_canonical_marker(void)
{
    aura_sync_marker_t m;
    const char *json =
        "{\n"
        "  \"version\": 1,\n"
        "  \"timestamp\": \"2026-08-17T20:15:00Z\",\n"
        "  \"changes\": { \"music\": true, \"video\": false, \"images\": true }\n"
        "}\n";
    CHECK(aura_sync_marker_parse(json, &m) == AURA_SYNC_MARKER_OK);
    CHECK(m.version == 1);
    CHECK(!strcmp(m.timestamp, "2026-08-17T20:15:00Z"));
    CHECK(m.music);
    CHECK(!m.video);
    CHECK(m.images);
    CHECK(m.attempts == 0);
    CHECK(aura_sync_marker_has_work(&m));
}

static void test_compact_and_reordered(void)
{
    aura_sync_marker_t m;
    /* Sin espacios, claves en otro orden, con attempts explicito. */
    const char *json = "{\"attempts\":2,\"changes\":{\"images\":false,\"video\":true,\"music\":false},\"version\":1,\"timestamp\":\"x\"}";
    CHECK(aura_sync_marker_parse(json, &m) == AURA_SYNC_MARKER_OK);
    CHECK(!m.music && m.video && !m.images);
    CHECK(m.attempts == 2);
    CHECK(!strcmp(m.timestamp, "x"));
}

static void test_unknown_keys_ignored(void)
{
    aura_sync_marker_t m;
    const char *json =
        "{ \"version\": 1, \"source\": \"Aura Studio 1.2\", \"studio_build\": 44,"
        "  \"changes\": { \"music\": true, \"themes\": true }, \"note\": \"music: false\" }";
    CHECK(aura_sync_marker_parse(json, &m) == AURA_SYNC_MARKER_OK);
    CHECK(m.music);
    CHECK(!m.video && !m.images);
}

static void test_key_inside_string_value_is_not_a_key(void)
{
    aura_sync_marker_t m;
    /* "video" aparece como VALOR de texto antes que como clave real. */
    const char *json =
        "{ \"version\": 1, \"timestamp\": \"video\", \"changes\": { \"video\": true } }";
    CHECK(aura_sync_marker_parse(json, &m) == AURA_SYNC_MARKER_OK);
    CHECK(!strcmp(m.timestamp, "video"));
    CHECK(m.video);
}

static void test_missing_version(void)
{
    aura_sync_marker_t m;
    CHECK(aura_sync_marker_parse("{ \"changes\": { \"music\": true } }", &m)
          == AURA_SYNC_MARKER_MISSING_VERSION);
    CHECK(m.version == -1);
    CHECK(!m.music); /* out queda como init(): no se reconstruye nada */
    CHECK(aura_sync_marker_parse("{ \"version\": \"uno\" }", &m)
          == AURA_SYNC_MARKER_MISSING_VERSION);
}

static void test_malformed(void)
{
    aura_sync_marker_t m;
    CHECK(aura_sync_marker_parse("", &m) == AURA_SYNC_MARKER_MALFORMED);
    CHECK(aura_sync_marker_parse("garbage", &m) == AURA_SYNC_MARKER_MALFORMED);
    CHECK(aura_sync_marker_parse(NULL, &m) == AURA_SYNC_MARKER_MALFORMED);
    CHECK(aura_sync_marker_parse("   \n[1,2]", &m) == AURA_SYNC_MARKER_MALFORMED);
    CHECK(!aura_sync_marker_has_work(&m));
}

static void test_newer_version_is_unsupported_but_readable(void)
{
    aura_sync_marker_t m;
    const char *json = "{ \"version\": 7, \"changes\": { \"music\": true } }";
    CHECK(aura_sync_marker_parse(json, &m) == AURA_SYNC_MARKER_UNSUPPORTED);
    CHECK(m.version == 7); /* para poder decir "version 7" en pantalla */
    CHECK(m.music);        /* leido, pero el llamador NO debe actuar */
}

static void test_bool_values_strict(void)
{
    aura_sync_marker_t m;
    /* 1/"yes"/null no son true: solo el literal JSON true. */
    const char *json = "{ \"version\": 1, \"changes\": { \"music\": 1, \"video\": \"yes\", \"images\": null } }";
    CHECK(aura_sync_marker_parse(json, &m) == AURA_SYNC_MARKER_OK);
    CHECK(!m.music && !m.video && !m.images);
    CHECK(!aura_sync_marker_has_work(&m));
}

static void test_serialize_roundtrip(void)
{
    aura_sync_marker_t m, back;
    char buf[256];
    int n;

    aura_sync_marker_init(&m);
    m.version = 1;
    strcpy(m.timestamp, "2026-08-17T21:00:00Z");
    m.music = true;
    m.images = true;
    m.attempts = 2;

    n = aura_sync_marker_serialize(&m, buf, sizeof(buf));
    CHECK(n > 0);
    CHECK((size_t)n == strlen(buf));
    CHECK(aura_sync_marker_parse(buf, &back) == AURA_SYNC_MARKER_OK);
    CHECK(back.version == 1);
    CHECK(!strcmp(back.timestamp, "2026-08-17T21:00:00Z"));
    CHECK(back.music && !back.video && back.images);
    CHECK(back.attempts == 2);
}

static void test_serialize_defaults_and_too_small(void)
{
    aura_sync_marker_t m, back;
    char big[256];
    char tiny[16];

    aura_sync_marker_init(&m); /* version -1 -> se serializa la soportada */
    CHECK(aura_sync_marker_serialize(&m, big, sizeof(big)) > 0);
    CHECK(aura_sync_marker_parse(big, &back) == AURA_SYNC_MARKER_OK);
    CHECK(back.version == AURA_SYNC_MARKER_VERSION_SUPPORTED);
    CHECK(aura_sync_marker_serialize(&m, tiny, sizeof(tiny)) == -1);
}

static void test_timestamp_truncates_safely(void)
{
    aura_sync_marker_t m;
    char json[256];
    char longts[80];
    memset(longts, 'a', sizeof(longts) - 1);
    longts[sizeof(longts) - 1] = '\0';
    snprintf(json, sizeof(json), "{ \"version\": 1, \"timestamp\": \"%s\" }", longts);
    CHECK(aura_sync_marker_parse(json, &m) == AURA_SYNC_MARKER_OK);
    CHECK(strlen(m.timestamp) == AURA_SYNC_MARKER_TIMESTAMP_LEN - 1);
}

int main(void)
{
    test_init();
    test_canonical_marker();
    test_compact_and_reordered();
    test_unknown_keys_ignored();
    test_key_inside_string_value_is_not_a_key();
    test_missing_version();
    test_malformed();
    test_newer_version_is_unsupported_but_readable();
    test_bool_values_strict();
    test_serialize_roundtrip();
    test_serialize_defaults_and_too_small();
    test_timestamp_truncates_safely();

    printf("%d/%d checks OK\n", checks - failures, checks);
    return failures ? 1 : 0;
}
