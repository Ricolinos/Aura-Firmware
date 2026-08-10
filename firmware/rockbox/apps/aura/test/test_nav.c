/* Tests host-side de la maquina de estados de navegacion (aura_nav.c).
 * Sin dependencias de Rockbox: compila y corre nativo en el Mac.
 * Ejecutar con `make -C apps/aura/test`. */
#include <stdio.h>
#include "../aura_nav.h"

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
    aura_nav_t nav;
    aura_nav_init(&nav, AURA_SCREEN_ROOT);

    CHECK(aura_nav_depth(&nav) == 1);
    CHECK(aura_nav_current(&nav) == AURA_SCREEN_ROOT);
    CHECK(aura_nav_is_root(&nav));
    CHECK(aura_nav_get_selection(&nav) == 0);
}

static void test_push_pop(void)
{
    aura_nav_t nav;
    aura_nav_init(&nav, AURA_SCREEN_ROOT);

    CHECK(aura_nav_push(&nav, AURA_SCREEN_MUSIC));
    CHECK(aura_nav_depth(&nav) == 2);
    CHECK(aura_nav_current(&nav) == AURA_SCREEN_MUSIC);
    CHECK(!aura_nav_is_root(&nav));

    CHECK(aura_nav_push(&nav, AURA_SCREEN_NOWPLAYING));
    CHECK(aura_nav_depth(&nav) == 3);
    CHECK(aura_nav_current(&nav) == AURA_SCREEN_NOWPLAYING);

    CHECK(aura_nav_pop(&nav));
    CHECK(aura_nav_depth(&nav) == 2);
    CHECK(aura_nav_current(&nav) == AURA_SCREEN_MUSIC);

    CHECK(aura_nav_pop(&nav));
    CHECK(aura_nav_depth(&nav) == 1);
    CHECK(aura_nav_current(&nav) == AURA_SCREEN_ROOT);
}

static void test_pop_at_root_is_noop(void)
{
    aura_nav_t nav;
    aura_nav_init(&nav, AURA_SCREEN_ROOT);

    CHECK(!aura_nav_pop(&nav));
    CHECK(aura_nav_depth(&nav) == 1);
    CHECK(aura_nav_is_root(&nav));
}

static void test_max_depth_is_respected(void)
{
    aura_nav_t nav;
    int i;
    aura_nav_init(&nav, AURA_SCREEN_ROOT);

    for (i = 1; i < AURA_NAV_MAX_DEPTH; i++)
        CHECK(aura_nav_push(&nav, AURA_SCREEN_MUSIC));

    CHECK(aura_nav_depth(&nav) == AURA_NAV_MAX_DEPTH);
    CHECK(!aura_nav_push(&nav, AURA_SCREEN_MUSIC));
    CHECK(aura_nav_depth(&nav) == AURA_NAV_MAX_DEPTH);
}

static void test_selection_is_remembered_per_level(void)
{
    aura_nav_t nav;
    aura_nav_init(&nav, AURA_SCREEN_ROOT);

    aura_nav_set_selection(&nav, 3);
    CHECK(aura_nav_get_selection(&nav) == 3);

    aura_nav_push(&nav, AURA_SCREEN_MUSIC);
    CHECK(aura_nav_get_selection(&nav) == 0);
    aura_nav_set_selection(&nav, 7);
    CHECK(aura_nav_get_selection(&nav) == 7);

    aura_nav_pop(&nav);
    CHECK(aura_nav_get_selection(&nav) == 3);
}

static void test_push_pop_restores_selection_on_reentry(void)
{
    aura_nav_t nav;
    aura_nav_init(&nav, AURA_SCREEN_ROOT);

    aura_nav_push(&nav, AURA_SCREEN_SETTINGS);
    aura_nav_set_selection(&nav, 2);
    aura_nav_pop(&nav);

    /* Reentrar a Settings: como el nuevo push reinicia la seleccion a 0,
     * el llamador (aura_screens.c) es responsable de restaurarla si lo
     * desea -- este test documenta ese contrato. */
    aura_nav_push(&nav, AURA_SCREEN_SETTINGS);
    CHECK(aura_nav_get_selection(&nav) == 0);
}

static void test_reset_to_root(void)
{
    aura_nav_t nav;
    aura_nav_init(&nav, AURA_SCREEN_ROOT);

    aura_nav_push(&nav, AURA_SCREEN_MUSIC);
    aura_nav_push(&nav, AURA_SCREEN_NOWPLAYING);
    aura_nav_set_selection(&nav, 5);

    aura_nav_reset_to_root(&nav);
    CHECK(aura_nav_depth(&nav) == 1);
    CHECK(aura_nav_current(&nav) == AURA_SCREEN_ROOT);
    CHECK(aura_nav_is_root(&nav));
}

static void test_reset_to_root_preserves_configured_root(void)
{
    aura_nav_t nav;
    aura_nav_init(&nav, AURA_SCREEN_SETTINGS);

    aura_nav_push(&nav, AURA_SCREEN_SETTINGS_THEME);
    aura_nav_reset_to_root(&nav);

    CHECK(aura_nav_current(&nav) == AURA_SCREEN_SETTINGS);
    CHECK(aura_nav_depth(&nav) == 1);
}

int main(void)
{
    test_init();
    test_push_pop();
    test_pop_at_root_is_noop();
    test_max_depth_is_respected();
    test_selection_is_remembered_per_level();
    test_push_pop_restores_selection_on_reentry();
    test_reset_to_root();
    test_reset_to_root_preserves_configured_root();

    printf("%d/%d checks OK\n", checks - failures, checks);
    if (failures)
    {
        printf("%d FALLO(S)\n", failures);
        return 1;
    }
    printf("TODO OK\n");
    return 0;
}
