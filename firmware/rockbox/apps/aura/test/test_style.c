#include <stdio.h>
#include <string.h>
#include "../aura_style_manifest.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static void apply(aura_style_manifest_t *m, const char *name, const char *value)
{
    aura_style_manifest_apply_line(m, name, value);
}

static void test_init_is_all_absent(void)
{
    aura_style_manifest_t m;
    int i;

    aura_style_manifest_init(&m);
    CHECK(m.format == -1);
    CHECK(!m.has_id);
    CHECK(!m.has_name);
    for (i = 0; i < AURA_STYLE_ROLE_COUNT; i++)
    {
        CHECK(m.palette_light[i] == -1);
        CHECK(m.palette_dark[i] == -1);
    }
    CHECK(m.category_settings_gray == -1);
    CHECK(m.category_video == -1);
    CHECK(m.category_photos == -1);
    CHECK(m.category_extras_yellow == -1);
    CHECK(!aura_style_manifest_is_loadable(&m));
}

static void test_format_and_identity(void)
{
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "theme_format", "1");
    apply(&m, "theme_id", "apple-personal");
    apply(&m, "theme_name", "Apple (uso personal)");

    CHECK(m.format == 1);
    CHECK(m.has_id);
    CHECK(!strcmp(m.id, "apple-personal"));
    CHECK(m.has_name);
    CHECK(!strcmp(m.name, "Apple (uso personal)"));
    CHECK(aura_style_manifest_is_loadable(&m));
}

static void test_format_absent_not_loadable(void)
{
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "theme_id", "sin-formato");
    CHECK(!aura_style_manifest_is_loadable(&m));
}

static void test_format_newer_than_supported_not_loadable(void)
{
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "theme_format", "2");
    CHECK(m.format == 2);
    CHECK(!aura_style_manifest_is_loadable(&m));
}

static void test_format_older_is_loadable(void)
{
    /* v1 no tiene "menor" todavia (AURA_STYLE_FORMAT_SUPPORTED == 1),
     * pero el formato 0 debe seguir aceptandose (regla general del
     * contrato: menor o igual al soportado siempre carga). */
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "theme_format", "0");
    CHECK(aura_style_manifest_is_loadable(&m));
}

static void test_palette_hex_with_and_without_hash(void)
{
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "palette_light_shell_bg", "#FFFFFF");
    apply(&m, "palette_dark_shell_bg", "1C1C1E");

    CHECK(m.palette_light[AURA_STYLE_ROLE_SHELL_BG] == 0xFFFFFF);
    CHECK(m.palette_dark[AURA_STYLE_ROLE_SHELL_BG] == 0x1C1C1E);
}

static void test_palette_roles_do_not_cross_contaminate(void)
{
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "palette_light_text_primary", "#000000");

    CHECK(m.palette_light[AURA_STYLE_ROLE_TEXT_PRIMARY] == 0x000000);
    CHECK(m.palette_light[AURA_STYLE_ROLE_SHELL_BG] == -1);
    CHECK(m.palette_dark[AURA_STYLE_ROLE_TEXT_PRIMARY] == -1);
}

static void test_category_colors(void)
{
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "category_video", "#1E3A5F");
    apply(&m, "category_extras_yellow", "#FFCC00");

    CHECK(m.category_video == 0x1E3A5F);
    CHECK(m.category_extras_yellow == 0xFFCC00);
    CHECK(m.category_photos == -1);
}

static void test_malformed_hex_is_absent(void)
{
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "palette_light_shell_bg", "no-es-hex");
    CHECK(m.palette_light[AURA_STYLE_ROLE_SHELL_BG] == -1);
}

static void test_unknown_keys_are_ignored_silently(void)
{
    /* Campos de Aura Studio (theme_author/theme_license/
     * theme_redistributable/requires_firmware_min/accent_default/
     * accent_presets) o de un formato futuro -- no deben tocar nada
     * del manifiesto ni hacer que deje de ser cargable. */
    aura_style_manifest_t m;

    aura_style_manifest_init(&m);
    apply(&m, "theme_format", "1");
    apply(&m, "theme_author", "Alguien");
    apply(&m, "theme_license", "personal");
    apply(&m, "theme_redistributable", "no");
    apply(&m, "requires_firmware_min", "0.9.0");
    apply(&m, "accent_default", "#FF2D55");
    apply(&m, "un_campo_que_no_existe_todavia", "42");

    CHECK(aura_style_manifest_is_loadable(&m));
}

static void test_id_validation(void)
{
    CHECK(aura_style_id_is_valid("apple-personal"));
    CHECK(aura_style_id_is_valid("a"));
    CHECK(aura_style_id_is_valid("mi-tema-2"));

    CHECK(!aura_style_id_is_valid(""));
    CHECK(!aura_style_id_is_valid("default"));      /* reservado */
    CHECK(!aura_style_id_is_valid("Mi-Tema"));       /* mayusculas */
    CHECK(!aura_style_id_is_valid("mi tema"));       /* espacio */
    CHECK(!aura_style_id_is_valid("../etc"));        /* recorrido de ruta */
    CHECK(!aura_style_id_is_valid("tema.viejo"));    /* punto */
    CHECK(!aura_style_id_is_valid("tema/otro"));     /* separador */
    CHECK(!aura_style_id_is_valid(NULL));

    /* Exactamente 32 caracteres cabe (AURA_STYLE_ID_LEN = 33 = 32+NUL);
     * 33 ya no. */
    CHECK(aura_style_id_is_valid("12345678901234567890123456789012"));
    CHECK(!aura_style_id_is_valid("123456789012345678901234567890123"));
}

int main(void)
{
    test_init_is_all_absent();
    test_format_and_identity();
    test_format_absent_not_loadable();
    test_format_newer_than_supported_not_loadable();
    test_format_older_is_loadable();
    test_palette_hex_with_and_without_hash();
    test_palette_roles_do_not_cross_contaminate();
    test_category_colors();
    test_malformed_hex_is_absent();
    test_unknown_keys_are_ignored_silently();
    test_id_validation();

    printf("%d/%d checks OK\n", checks - failures, checks);
    return failures ? 1 : 0;
}
