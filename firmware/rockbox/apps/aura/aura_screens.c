#include <string.h>
#include <stdio.h>

#include "button.h"
#include "lcd.h"
#include "backlight.h"
#include "settings.h"
#include "version.h"

#include "aura_screens.h"
#include "aura_widgets.h"
#include "aura_theme.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "aura_tokens.h"

#define MAX_MENU_ENTRIES 8

typedef struct {
    aura_str_id_t label_id;
    const char *icon_name;
    aura_screen_id_t target;
} nav_entry_t;

static const nav_entry_t root_entries[] = {
    { AURA_STR_MUSIC,      "music",    AURA_SCREEN_MUSIC },
    { AURA_STR_VIDEOS,     "video",    AURA_SCREEN_VIDEOS },
    { AURA_STR_PHOTOS,     "image",    AURA_SCREEN_PHOTOS },
    { AURA_STR_NOWPLAYING, "play",     AURA_SCREEN_NOWPLAYING },
    { AURA_STR_SETTINGS,   "settings", AURA_SCREEN_SETTINGS },
};

static const nav_entry_t settings_entries[] = {
    { AURA_STR_SETTINGS_THEME,      NULL, AURA_SCREEN_SETTINGS_THEME },
    { AURA_STR_SETTINGS_GRAPHICS,   NULL, AURA_SCREEN_SETTINGS_GRAPHICS },
    { AURA_STR_SETTINGS_EQ,         NULL, AURA_SCREEN_SETTINGS_EQ },
    { AURA_STR_SETTINGS_BRIGHTNESS, NULL, AURA_SCREEN_SETTINGS_BRIGHTNESS },
    { AURA_STR_SETTINGS_LANGUAGE,   NULL, AURA_SCREEN_SETTINGS_LANGUAGE },
    { AURA_STR_SETTINGS_ABOUT,      NULL, AURA_SCREEN_SETTINGS_ABOUT },
};

static int get_nav_table(aura_screen_id_t screen, const nav_entry_t **out)
{
    switch (screen)
    {
    case AURA_SCREEN_ROOT:
        *out = root_entries;
        return sizeof(root_entries) / sizeof(root_entries[0]);
    case AURA_SCREEN_SETTINGS:
        *out = settings_entries;
        return sizeof(settings_entries) / sizeof(settings_entries[0]);
    default:
        *out = NULL;
        return 0;
    }
}

static aura_str_id_t screen_title_id(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_MUSIC:               return AURA_STR_MUSIC;
    case AURA_SCREEN_VIDEOS:              return AURA_STR_VIDEOS;
    case AURA_SCREEN_PHOTOS:              return AURA_STR_PHOTOS;
    case AURA_SCREEN_NOWPLAYING:          return AURA_STR_NOWPLAYING;
    case AURA_SCREEN_SETTINGS:            return AURA_STR_SETTINGS;
    case AURA_SCREEN_SETTINGS_THEME:      return AURA_STR_SETTINGS_THEME;
    case AURA_SCREEN_SETTINGS_GRAPHICS:   return AURA_STR_SETTINGS_GRAPHICS;
    case AURA_SCREEN_SETTINGS_EQ:         return AURA_STR_SETTINGS_EQ;
    case AURA_SCREEN_SETTINGS_BRIGHTNESS: return AURA_STR_SETTINGS_BRIGHTNESS;
    case AURA_SCREEN_SETTINGS_LANGUAGE:   return AURA_STR_SETTINGS_LANGUAGE;
    case AURA_SCREEN_SETTINGS_ABOUT:      return AURA_STR_SETTINGS_ABOUT;
    default:                              return AURA_STR_SETTINGS;
    }
}

/* -- Pantallas de eleccion (Tema / Graficos / EQ / Idioma) -------------- */

static const aura_str_id_t theme_choice_labels[] = {
    AURA_STR_THEME_LIGHT, AURA_STR_THEME_DARK,
};
static const aura_str_id_t graphics_choice_labels[] = {
    AURA_STR_GFX_ULTRA, AURA_STR_GFX_MINIMAL, AURA_STR_GFX_FULL,
};
static const aura_str_id_t eq_choice_labels[] = {
    AURA_STR_EQ_FLAT, AURA_STR_EQ_BASS_BOOST, AURA_STR_EQ_VOCAL, AURA_STR_EQ_TREBLE_BOOST,
};
static const aura_str_id_t language_choice_labels[] = {
    AURA_STR_LANG_ES, AURA_STR_LANG_EN,
};

static int is_choice_screen(aura_screen_id_t screen)
{
    return screen == AURA_SCREEN_SETTINGS_THEME
        || screen == AURA_SCREEN_SETTINGS_GRAPHICS
        || screen == AURA_SCREEN_SETTINGS_EQ
        || screen == AURA_SCREEN_SETTINGS_LANGUAGE;
}

static int get_choice_table(aura_screen_id_t screen, const aura_str_id_t **out)
{
    switch (screen)
    {
    case AURA_SCREEN_SETTINGS_THEME:
        *out = theme_choice_labels;
        return sizeof(theme_choice_labels) / sizeof(theme_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_GRAPHICS:
        *out = graphics_choice_labels;
        return sizeof(graphics_choice_labels) / sizeof(graphics_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_EQ:
        *out = eq_choice_labels;
        return sizeof(eq_choice_labels) / sizeof(eq_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_LANGUAGE:
        *out = language_choice_labels;
        return sizeof(language_choice_labels) / sizeof(language_choice_labels[0]);
    default:
        *out = NULL;
        return 0;
    }
}

static int get_choice_current(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_SETTINGS_THEME:    return (int)aura_settings.theme;
    case AURA_SCREEN_SETTINGS_GRAPHICS: return (int)aura_settings.graphics_mode;
    case AURA_SCREEN_SETTINGS_EQ:       return (int)aura_settings.eq_preset;
    case AURA_SCREEN_SETTINGS_LANGUAGE: return (int)aura_settings.language;
    default:                            return 0;
    }
}

static void apply_choice(aura_screen_id_t screen, int index)
{
    switch (screen)
    {
    case AURA_SCREEN_SETTINGS_THEME:
        aura_settings.theme = (aura_theme_id_t)index;
        break;
    case AURA_SCREEN_SETTINGS_GRAPHICS:
        aura_settings.graphics_mode = (aura_gfx_mode_t)index;
        break;
    case AURA_SCREEN_SETTINGS_EQ:
        aura_settings.eq_preset = (aura_eq_preset_t)index;
        aura_settings_apply_eq();
        break;
    case AURA_SCREEN_SETTINGS_LANGUAGE:
        aura_settings.language = (aura_lang_t)index;
        break;
    default:
        break;
    }
    aura_settings_save();
}

/* -- Dibujo -------------------------------------------------------------- */

static void draw_nav_list(aura_nav_t *nav, aura_screen_id_t screen)
{
    const nav_entry_t *entries;
    int count = get_nav_table(screen, &entries);
    aura_list_item_t items[MAX_MENU_ENTRIES];
    int i;

    for (i = 0; i < count; i++)
    {
        items[i].label = aura_str(entries[i].label_id);
        items[i].icon_name = entries[i].icon_name;
        items[i].checked = 0;
    }

    /* "Aura" es el nombre de la marca, igual en ambos idiomas: no va en
     * la tabla de traduccion, solo el menu raiz lo usa como titulo. */
    aura_widgets_draw_list(screen == AURA_SCREEN_ROOT ? "Aura"
                                                        : aura_str(screen_title_id(screen)),
                            items, count, aura_nav_get_selection(nav));
}

static void draw_choice_list(aura_nav_t *nav, aura_screen_id_t screen)
{
    const aura_str_id_t *labels;
    int count = get_choice_table(screen, &labels);
    int current = get_choice_current(screen);
    aura_list_item_t items[MAX_MENU_ENTRIES];
    int i;

    for (i = 0; i < count; i++)
    {
        items[i].label = aura_str(labels[i]);
        items[i].icon_name = NULL;
        items[i].checked = (i == current);
    }

    aura_widgets_draw_list(aura_str(screen_title_id(screen)), items, count,
                            aura_nav_get_selection(nav));
}

static void draw_brightness(void)
{
    char buf[16];
    int bar_x = AURA_SPACING_XXL;
    int bar_w = AURA_SCREEN_WIDTH - 2 * AURA_SPACING_XXL;
    int bar_y = AURA_SCREEN_HEIGHT / 2;
    int fill_w;
    int range = MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING;

    aura_theme_clear_screen();

    lcd_setfont(aura_font(AURA_FONT_STYLE_TITLE));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));
    lcd_putsxy(AURA_SPACING_LG, AURA_SPACING_LG,
               (const unsigned char *)aura_str(AURA_STR_SETTINGS_BRIGHTNESS));

    lcd_set_foreground(aura_color(AURA_TOK_BORDER));
    lcd_drawrect(bar_x, bar_y, bar_w, AURA_SPACING_XL);

    if (range > 0)
        fill_w = (bar_w * (global_settings.brightness - MIN_BRIGHTNESS_SETTING)) / range;
    else
        fill_w = 0;
    lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
    lcd_fillrect(bar_x, bar_y, fill_w, AURA_SPACING_XL);

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
    snprintf(buf, sizeof(buf), "%d / %d", global_settings.brightness,
             MAX_BRIGHTNESS_SETTING);
    lcd_putsxy(bar_x, bar_y + AURA_SPACING_XL + AURA_SPACING_SM,
               (const unsigned char *)buf);
}

static void draw_about(void)
{
    aura_theme_clear_screen();

    lcd_setfont(aura_font(AURA_FONT_STYLE_TITLE));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));
    lcd_putsxy(AURA_SPACING_LG, AURA_SPACING_LG, (const unsigned char *)"Aura");

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
    lcd_putsxy(AURA_SPACING_LG, AURA_SPACING_LG + AURA_TYPE_TITLE + AURA_SPACING_MD,
               (const unsigned char *)aura_str(AURA_STR_ABOUT_BUILT_ON));
    lcd_putsxy(AURA_SPACING_LG,
               AURA_SPACING_LG + AURA_TYPE_TITLE + AURA_SPACING_MD + AURA_TYPE_BODY + AURA_SPACING_SM,
               (const unsigned char *)rbversion);
}

static void draw_empty_state(aura_screen_id_t screen)
{
    aura_str_id_t msg_id;
    int w, h;

    switch (screen)
    {
    case AURA_SCREEN_MUSIC:  msg_id = AURA_STR_EMPTY_MUSIC; break;
    case AURA_SCREEN_VIDEOS: msg_id = AURA_STR_EMPTY_VIDEOS; break;
    case AURA_SCREEN_PHOTOS: msg_id = AURA_STR_EMPTY_PHOTOS; break;
    default:                 msg_id = AURA_STR_NOTHING_PLAYING; break;
    }

    aura_theme_clear_screen();

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w, &h);
    lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2, (AURA_SCREEN_HEIGHT - h) / 2,
               (const unsigned char *)aura_str(msg_id));
}

void aura_screens_draw(aura_nav_t *nav)
{
    aura_screen_id_t screen = aura_nav_current(nav);

    if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS)
        draw_nav_list(nav, screen);
    else if (is_choice_screen(screen))
        draw_choice_list(nav, screen);
    else if (screen == AURA_SCREEN_SETTINGS_BRIGHTNESS)
        draw_brightness();
    else if (screen == AURA_SCREEN_SETTINGS_ABOUT)
        draw_about();
    else
        draw_empty_state(screen);
}

/* -- Entrada --------------------------------------------------------------- */

static void handle_nav_list(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    const nav_entry_t *entries;
    int count = get_nav_table(screen, &entries);
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < count - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        aura_nav_push(nav, entries[sel].target);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

static void handle_choice_list(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    const aura_str_id_t *labels;
    int count = get_choice_table(screen, &labels);
    int sel = aura_nav_get_selection(nav);
    (void)labels;

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < count - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        apply_choice(screen, sel);
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

static void handle_brightness(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (global_settings.brightness < MAX_BRIGHTNESS_SETTING)
        {
            global_settings.brightness++;
            backlight_set_brightness(global_settings.brightness);
        }
        break;
    case BUTTON_SCROLL_BACK:
        if (global_settings.brightness > MIN_BRIGHTNESS_SETTING)
        {
            global_settings.brightness--;
            backlight_set_brightness(global_settings.brightness);
        }
        break;
    case BUTTON_SELECT:
    case BUTTON_MENU:
        settings_save();
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

static void handle_dismiss_only(aura_nav_t *nav, long button)
{
    if (button == BUTTON_MENU || button == BUTTON_SELECT)
        aura_nav_pop(nav);
}

void aura_screens_handle_button(aura_nav_t *nav, long button)
{
    aura_screen_id_t screen = aura_nav_current(nav);

    if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS)
        handle_nav_list(nav, screen, button);
    else if (is_choice_screen(screen))
        handle_choice_list(nav, screen, button);
    else if (screen == AURA_SCREEN_SETTINGS_BRIGHTNESS)
        handle_brightness(nav, button);
    else
        handle_dismiss_only(nav, button);
}
