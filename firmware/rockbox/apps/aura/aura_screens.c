#include <string.h>
#include <stdio.h>

#include "button.h"
#include "lcd.h"
#include "backlight.h"
#include "settings.h"
#include "version.h"

#include "aura_screens.h"
#include "aura_widgets.h"
#include "aura_statusbar.h"
#include "aura_theme.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "aura_tokens.h"
#include "aura_music.h"
#include "aura_nowplaying.h"
#include "aura_transitions.h"
#include "aura_coverflow.h"
#include "aura_home.h"
#include "aura_photos.h"
#include "aura_video.h"

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

static const nav_entry_t music_entries[] = {
    { AURA_STR_MUSIC_ARTISTS,   NULL, AURA_SCREEN_MUSIC_ARTISTS },
    { AURA_STR_MUSIC_ALBUMS,    NULL, AURA_SCREEN_MUSIC_ALBUMS },
    { AURA_STR_MUSIC_SONGS,     NULL, AURA_SCREEN_MUSIC_SONGS },
    { AURA_STR_MUSIC_PLAYLISTS, NULL, AURA_SCREEN_MUSIC_PLAYLISTS },
    { AURA_STR_MUSIC_GENRES,    NULL, AURA_SCREEN_MUSIC_GENRES },
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
    case AURA_SCREEN_MUSIC:
        *out = music_entries;
        return sizeof(music_entries) / sizeof(music_entries[0]);
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
    case AURA_SCREEN_MUSIC_ARTISTS:       return AURA_STR_MUSIC_ARTISTS;
    case AURA_SCREEN_MUSIC_ALBUMS:
    case AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST: return AURA_STR_MUSIC_ALBUMS;
    case AURA_SCREEN_MUSIC_SONGS:
    case AURA_SCREEN_MUSIC_SONGS_BY_ALBUM:
    case AURA_SCREEN_MUSIC_SONGS_BY_GENRE:   return AURA_STR_MUSIC_SONGS;
    case AURA_SCREEN_MUSIC_GENRES:        return AURA_STR_MUSIC_GENRES;
    case AURA_SCREEN_MUSIC_PLAYLISTS:     return AURA_STR_MUSIC_PLAYLISTS;
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
    int range = MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING;
    int fraction = range > 0
        ? (256 * (global_settings.brightness - MIN_BRIGHTNESS_SETTING)) / range
        : 0;

    snprintf(buf, sizeof(buf), "%d / %d", global_settings.brightness,
             MAX_BRIGHTNESS_SETTING);
    aura_widgets_draw_slider(aura_str(AURA_STR_SETTINGS_BRIGHTNESS), fraction, buf);
}

static void draw_about(void)
{
    aura_theme_clear_screen();
    aura_statusbar_draw(0, AURA_SCREEN_WIDTH, aura_str(AURA_STR_SETTINGS_ABOUT), 0);

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
    lcd_putsxy(AURA_SPACING_LG, AURA_LAYOUT_STATUSBAR_HEIGHT + AURA_SPACING_LG,
               (const unsigned char *)aura_str(AURA_STR_ABOUT_BUILT_ON));
    lcd_putsxy(AURA_SPACING_LG,
               AURA_LAYOUT_STATUSBAR_HEIGHT + AURA_SPACING_LG + AURA_TYPE_BODY + AURA_SPACING_SM,
               (const unsigned char *)rbversion);
}

static void draw_message_centered(aura_str_id_t msg_id)
{
    int w, h;
    int content_top = AURA_LAYOUT_STATUSBAR_HEIGHT;
    int content_h = AURA_SCREEN_HEIGHT - content_top;

    aura_theme_clear_screen();
    aura_statusbar_draw(0, AURA_SCREEN_WIDTH, NULL, 0);

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w, &h);
    lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2, content_top + (content_h - h) / 2,
               (const unsigned char *)aura_str(msg_id));
}

static void draw_empty_state(aura_screen_id_t screen)
{
    aura_str_id_t msg_id;

    switch (screen)
    {
    case AURA_SCREEN_MUSIC:  msg_id = AURA_STR_EMPTY_MUSIC; break;
    case AURA_SCREEN_VIDEOS: msg_id = AURA_STR_EMPTY_VIDEOS; break;
    case AURA_SCREEN_PHOTOS: msg_id = AURA_STR_EMPTY_PHOTOS; break;
    default:                 msg_id = AURA_STR_NOTHING_PLAYING; break;
    }

    draw_message_centered(msg_id);
}

/* -- Musica: navegacion por base de datos (tagcache) --------------------- */

static int is_music_browse_screen(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_MUSIC_ARTISTS:
    case AURA_SCREEN_MUSIC_ALBUMS:
    case AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST:
    case AURA_SCREEN_MUSIC_SONGS:
    case AURA_SCREEN_MUSIC_SONGS_BY_ALBUM:
    case AURA_SCREEN_MUSIC_SONGS_BY_GENRE:
    case AURA_SCREEN_MUSIC_GENRES:
        return 1;
    default:
        return 0;
    }
}

/* En modo grafico Completo, Albumes se navega con Coverflow en vez de
 * la lista plana (D-025); en Ultra/Minimalista sigue siendo una lista
 * como cualquier otra pantalla de is_music_browse_screen(). */
/* En modo grafico Completo, la raiz se dibuja como pantalla dividida
 * con caratulas si hay al menos un album con arte disponible (D-025);
 * si no hay ninguna caratula, la pantalla dividida no aportaria nada
 * y se usa la lista simple de siempre. */
static int is_split_home_screen(aura_screen_id_t screen)
{
    return screen == AURA_SCREEN_ROOT
        && aura_settings.graphics_mode == AURA_GFX_FULL
        && aura_home_has_content();
}

static int is_coverflow_screen(aura_screen_id_t screen)
{
    return aura_settings.graphics_mode == AURA_GFX_FULL
        && (screen == AURA_SCREEN_MUSIC_ALBUMS || screen == AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST);
}

static aura_screen_id_t s_music_cache_screen = AURA_SCREEN_COUNT;
static int s_music_cache_generation = -1;
static aura_music_item_t s_music_cache[AURA_MUSIC_MAX_ITEMS];
static int s_music_cache_count = 0;
static aura_list_item_t s_music_items_buf[AURA_MUSIC_MAX_ITEMS];

static void ensure_music_cache(aura_screen_id_t screen)
{
    int gen = aura_music_filter_generation();

    if (s_music_cache_screen == screen && s_music_cache_generation == gen)
        return;

    s_music_cache_screen = screen;
    s_music_cache_generation = gen;
    s_music_cache_count = aura_music_browse(screen, s_music_cache, AURA_MUSIC_MAX_ITEMS);
}

static void draw_music_browse(aura_nav_t *nav, aura_screen_id_t screen)
{
    int i;

    if (!aura_music_db_ready())
    {
        draw_message_centered(AURA_STR_DB_NOT_READY);
        return;
    }

    ensure_music_cache(screen);

    if (s_music_cache_count == 0)
    {
        draw_message_centered(AURA_STR_EMPTY_LIST);
        return;
    }

    for (i = 0; i < s_music_cache_count; i++)
    {
        s_music_items_buf[i].label = s_music_cache[i].label;
        s_music_items_buf[i].icon_name = NULL;
        s_music_items_buf[i].checked = 0;
    }

    aura_widgets_draw_list(aura_str(screen_title_id(screen)), s_music_items_buf,
                            s_music_cache_count, aura_nav_get_selection(nav));
}

static aura_screen_id_t s_playlist_cache_screen = AURA_SCREEN_COUNT;
static char s_playlist_cache[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
static int s_playlist_cache_count = 0;

static void ensure_playlist_cache(aura_screen_id_t screen)
{
    if (s_playlist_cache_screen == screen)
        return;

    s_playlist_cache_screen = screen;
    s_playlist_cache_count = aura_music_list_playlists(s_playlist_cache, AURA_MUSIC_MAX_ITEMS);
}

static void draw_playlists(aura_nav_t *nav)
{
    int i;

    ensure_playlist_cache(AURA_SCREEN_MUSIC_PLAYLISTS);

    if (s_playlist_cache_count == 0)
    {
        draw_message_centered(AURA_STR_EMPTY_LIST);
        return;
    }

    for (i = 0; i < s_playlist_cache_count; i++)
    {
        s_music_items_buf[i].label = s_playlist_cache[i];
        s_music_items_buf[i].icon_name = NULL;
        s_music_items_buf[i].checked = 0;
    }

    aura_widgets_draw_list(aura_str(AURA_STR_MUSIC_PLAYLISTS), s_music_items_buf,
                            s_playlist_cache_count, aura_nav_get_selection(nav));
}

void aura_screens_draw(aura_nav_t *nav)
{
    aura_screen_id_t screen = aura_nav_current(nav);

    if (is_split_home_screen(screen))
        aura_home_draw(nav);
    else if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS || screen == AURA_SCREEN_MUSIC)
        draw_nav_list(nav, screen);
    else if (is_choice_screen(screen))
        draw_choice_list(nav, screen);
    else if (screen == AURA_SCREEN_SETTINGS_BRIGHTNESS)
        draw_brightness();
    else if (screen == AURA_SCREEN_SETTINGS_ABOUT)
        draw_about();
    else if (is_coverflow_screen(screen))
        aura_coverflow_draw(nav, screen);
    else if (is_music_browse_screen(screen))
        draw_music_browse(nav, screen);
    else if (screen == AURA_SCREEN_MUSIC_PLAYLISTS)
        draw_playlists(nav);
    else if (screen == AURA_SCREEN_PHOTOS)
        aura_photos_draw(nav);
    else if (screen == AURA_SCREEN_PHOTO_VIEWER)
        aura_photo_viewer_draw(nav);
    else if (screen == AURA_SCREEN_VIDEOS)
        aura_video_draw(nav);
    else if (screen == AURA_SCREEN_NOWPLAYING && aura_nowplaying_active())
        aura_nowplaying_draw();
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
        if (screen == AURA_SCREEN_MUSIC)
            aura_music_reset_filters();
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

static void handle_music_browse(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    int count = s_music_cache_count;
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
        if (count == 0)
            break;
        switch (screen)
        {
        case AURA_SCREEN_MUSIC_ARTISTS:
            aura_music_select_artist(s_music_cache[sel].seek);
            aura_nav_push(nav, AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST);
            break;
        case AURA_SCREEN_MUSIC_ALBUMS:
        case AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST:
            aura_music_select_album(s_music_cache[sel].seek);
            aura_nav_push(nav, AURA_SCREEN_MUSIC_SONGS_BY_ALBUM);
            break;
        case AURA_SCREEN_MUSIC_GENRES:
            aura_music_select_genre(s_music_cache[sel].seek);
            aura_nav_push(nav, AURA_SCREEN_MUSIC_SONGS_BY_GENRE);
            break;
        case AURA_SCREEN_MUSIC_SONGS:
        case AURA_SCREEN_MUSIC_SONGS_BY_ALBUM:
        case AURA_SCREEN_MUSIC_SONGS_BY_GENRE:
            if (aura_music_play_songs(screen, sel))
                aura_nav_push(nav, AURA_SCREEN_NOWPLAYING);
            break;
        default:
            break;
        }
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

static void handle_playlists(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < s_playlist_cache_count - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        if (s_playlist_cache_count > 0 && aura_music_play_playlist(sel))
            aura_nav_push(nav, AURA_SCREEN_NOWPLAYING);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

void aura_screens_handle_button(aura_nav_t *nav, long button)
{
    aura_screen_id_t screen = aura_nav_current(nav);
    int depth_before = aura_nav_depth(nav);

    if (is_split_home_screen(screen))
        aura_home_handle_button(nav, button);
    else if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS || screen == AURA_SCREEN_MUSIC)
        handle_nav_list(nav, screen, button);
    else if (is_choice_screen(screen))
        handle_choice_list(nav, screen, button);
    else if (screen == AURA_SCREEN_SETTINGS_BRIGHTNESS)
        handle_brightness(nav, button);
    else if (is_coverflow_screen(screen))
        aura_coverflow_handle_button(nav, screen, button);
    else if (is_music_browse_screen(screen))
        handle_music_browse(nav, screen, button);
    else if (screen == AURA_SCREEN_MUSIC_PLAYLISTS)
        handle_playlists(nav, button);
    else if (screen == AURA_SCREEN_PHOTOS)
        aura_photos_handle_button(nav, button);
    else if (screen == AURA_SCREEN_PHOTO_VIEWER)
        aura_photo_viewer_handle_button(nav, button);
    else if (screen == AURA_SCREEN_VIDEOS)
        aura_video_handle_button(nav, button);
    else if (screen == AURA_SCREEN_NOWPLAYING && aura_nowplaying_active())
        aura_nowplaying_handle_button(nav, button);
    else
        handle_dismiss_only(nav, button);

    /* Centralizado aca (no en cada handler) para no repetir la logica
     * en cada punto de push/pop: la profundidad de la pila es la unica
     * senal que hace falta para saber si hubo navegacion y en que
     * sentido. Ver D-024. */
    int depth_after = aura_nav_depth(nav);
    if (depth_after != depth_before)
        aura_transition_slide(nav, depth_after > depth_before ? 1 : -1);
}
