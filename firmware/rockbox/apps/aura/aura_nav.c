#include "aura_nav.h"

void aura_nav_init(aura_nav_t *nav, aura_screen_id_t root)
{
    int i;
    for (i = 0; i < AURA_NAV_MAX_DEPTH; i++)
    {
        nav->screens[i] = AURA_SCREEN_ROOT;
        nav->selection[i] = 0;
    }
    nav->screens[0] = root;
    nav->depth = 1;
}

int aura_nav_push(aura_nav_t *nav, aura_screen_id_t screen)
{
    if (nav->depth >= AURA_NAV_MAX_DEPTH)
        return 0;

    nav->screens[nav->depth] = screen;
    nav->selection[nav->depth] = 0;
    nav->depth++;
    return 1;
}

int aura_nav_pop(aura_nav_t *nav)
{
    if (nav->depth <= 1)
        return 0;

    nav->depth--;
    return 1;
}

void aura_nav_reset_to_root(aura_nav_t *nav)
{
    aura_screen_id_t root = nav->screens[0];
    aura_nav_init(nav, root);
}

aura_screen_id_t aura_nav_current(const aura_nav_t *nav)
{
    return nav->screens[nav->depth - 1];
}

int aura_nav_depth(const aura_nav_t *nav)
{
    return nav->depth;
}

int aura_nav_is_root(const aura_nav_t *nav)
{
    return nav->depth == 1;
}

void aura_nav_set_selection(aura_nav_t *nav, int index)
{
    nav->selection[nav->depth - 1] = index;
}

int aura_nav_get_selection(const aura_nav_t *nav)
{
    return nav->selection[nav->depth - 1];
}
