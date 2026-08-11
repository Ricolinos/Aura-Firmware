#include <string.h>
#include <stdio.h>

#include "button.h"
#include "lcd.h"
#include "backlight.h"
#include "settings.h"
#include "version.h"
#include "sound.h"
#include "powermgmt.h"
#include "string-extra.h"
#include "misc.h"

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
#include "aura_photos.h"
#include "aura_video.h"
#include "aura_manifest.h"

#define MAX_MENU_ENTRIES 16

typedef struct {
    aura_str_id_t label_id;
    const char *icon_name;
    aura_screen_id_t target;
} nav_entry_t;

static const nav_entry_t root_entries_all[] = {
    { AURA_STR_MUSIC,      "music",    AURA_SCREEN_MUSIC },
    { AURA_STR_VIDEOS,     "video",    AURA_SCREEN_VIDEOS },
    { AURA_STR_PHOTOS,     "image",    AURA_SCREEN_PHOTOS },
    { AURA_STR_NOWPLAYING, "play",     AURA_SCREEN_NOWPLAYING },
    { AURA_STR_SETTINGS,   "settings", AURA_SCREEN_SETTINGS },
};
/* Musica y Ajustes son fijos; Videos/Fotos/Ahora suena son opcionales
 * (Menu principal configurable, L14, Fase 18) -- se filtran en tiempo
 * de dibujo/manejo de boton, no se recorta la tabla fuente. */
static nav_entry_t root_entries[sizeof(root_entries_all) / sizeof(root_entries_all[0])];
static int root_entries_count;

static void rebuild_root_entries(void)
{
    int i, n = 0;
    for (i = 0; i < (int)(sizeof(root_entries_all) / sizeof(root_entries_all[0])); i++)
    {
        const nav_entry_t *e = &root_entries_all[i];
        if (e->target == AURA_SCREEN_VIDEOS && !aura_settings.show_videos)
            continue;
        if (e->target == AURA_SCREEN_PHOTOS && !aura_settings.show_photos)
            continue;
        if (e->target == AURA_SCREEN_NOWPLAYING && !aura_settings.show_nowplaying)
            continue;
        root_entries[n++] = *e;
    }
    root_entries_count = n;
}

static const nav_entry_t music_entries[] = {
    { AURA_STR_MUSIC_ARTISTS,   NULL, AURA_SCREEN_MUSIC_ARTISTS },
    { AURA_STR_MUSIC_ALBUMS,    NULL, AURA_SCREEN_MUSIC_ALBUMS },
    { AURA_STR_MUSIC_SONGS,     NULL, AURA_SCREEN_MUSIC_SONGS },
    { AURA_STR_MUSIC_PLAYLISTS, NULL, AURA_SCREEN_MUSIC_PLAYLISTS },
    { AURA_STR_MUSIC_GENRES,    NULL, AURA_SCREEN_MUSIC_GENRES },
};

static const nav_entry_t settings_entries[] = {
    { AURA_STR_SETTINGS_THEME,      NULL, AURA_SCREEN_SETTINGS_THEME },
    { AURA_STR_SETTINGS_ANIMATIONS, NULL, AURA_SCREEN_SETTINGS_ANIMATIONS },
    { AURA_STR_SETTINGS_GRAPHICS,   NULL, AURA_SCREEN_SETTINGS_GRAPHICS },
    { AURA_STR_SETTINGS_EQ,         NULL, AURA_SCREEN_SETTINGS_EQ },
    { AURA_STR_SETTINGS_BRIGHTNESS, NULL, AURA_SCREEN_SETTINGS_BRIGHTNESS },
    { AURA_STR_SETTINGS_SHUFFLE,    NULL, AURA_SCREEN_SETTINGS_SHUFFLE },
    { AURA_STR_SETTINGS_REPEAT,     NULL, AURA_SCREEN_SETTINGS_REPEAT },
    { AURA_STR_SETTINGS_BACKLIGHT,     NULL, AURA_SCREEN_SETTINGS_BACKLIGHT },
    { AURA_STR_SETTINGS_SLEEPTIMER,    NULL, AURA_SCREEN_SETTINGS_SLEEPTIMER },
    { AURA_STR_SETTINGS_VOLUME_LIMIT,  NULL, AURA_SCREEN_SETTINGS_VOLUME_LIMIT },
    { AURA_STR_SETTINGS_CLICKER,       NULL, AURA_SCREEN_SETTINGS_CLICKER },
    { AURA_STR_SETTINGS_MAINMENU,      NULL, AURA_SCREEN_SETTINGS_MAINMENU },
    { AURA_STR_SETTINGS_LANGUAGE,   NULL, AURA_SCREEN_SETTINGS_LANGUAGE },
    { AURA_STR_SETTINGS_ABOUT,      NULL, AURA_SCREEN_SETTINGS_ABOUT },
    { AURA_STR_SETTINGS_RESET,         NULL, AURA_SCREEN_SETTINGS_RESET },
};

static int get_nav_table(aura_screen_id_t screen, const nav_entry_t **out)
{
    switch (screen)
    {
    case AURA_SCREEN_ROOT:
        rebuild_root_entries();
        *out = root_entries;
        return root_entries_count;
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
    case AURA_SCREEN_SETTINGS_ANIMATIONS: return AURA_STR_SETTINGS_ANIMATIONS;
    case AURA_SCREEN_SETTINGS_GRAPHICS:   return AURA_STR_SETTINGS_GRAPHICS;
    case AURA_SCREEN_SETTINGS_EQ:         return AURA_STR_SETTINGS_EQ;
    case AURA_SCREEN_SETTINGS_BRIGHTNESS: return AURA_STR_SETTINGS_BRIGHTNESS;
    case AURA_SCREEN_SETTINGS_LANGUAGE:   return AURA_STR_SETTINGS_LANGUAGE;
    case AURA_SCREEN_SETTINGS_ABOUT:      return AURA_STR_SETTINGS_ABOUT;
    case AURA_SCREEN_SETTINGS_SHUFFLE:    return AURA_STR_SETTINGS_SHUFFLE;
    case AURA_SCREEN_SETTINGS_REPEAT:     return AURA_STR_SETTINGS_REPEAT;
    case AURA_SCREEN_SETTINGS_BACKLIGHT:    return AURA_STR_SETTINGS_BACKLIGHT;
    case AURA_SCREEN_SETTINGS_SLEEPTIMER:   return AURA_STR_SETTINGS_SLEEPTIMER;
    case AURA_SCREEN_SETTINGS_VOLUME_LIMIT: return AURA_STR_SETTINGS_VOLUME_LIMIT;
    case AURA_SCREEN_SETTINGS_CLICKER:      return AURA_STR_SETTINGS_CLICKER;
    case AURA_SCREEN_SETTINGS_MAINMENU:     return AURA_STR_SETTINGS_MAINMENU;
    case AURA_SCREEN_SETTINGS_RESET:        return AURA_STR_SETTINGS_RESET;
    default:                              return AURA_STR_SETTINGS;
    }
}

/* -- Pantallas de eleccion (Tema / Graficos / EQ / Idioma) -------------- */

static const aura_str_id_t theme_choice_labels[] = {
    AURA_STR_THEME_LIGHT, AURA_STR_THEME_DARK,
};
static const aura_str_id_t animation_choice_labels[] = {
    AURA_STR_ANIM_NONE, AURA_STR_ANIM_MINIMAL, AURA_STR_ANIM_ALL,
};
static const aura_str_id_t graphics_choice_labels[] = {
    AURA_STR_GFX_NONE, AURA_STR_GFX_MINIMAL, AURA_STR_GFX_ALL,
};
static const aura_str_id_t eq_choice_labels[] = {
    AURA_STR_EQ_FLAT, AURA_STR_EQ_BASS_BOOST, AURA_STR_EQ_VOCAL, AURA_STR_EQ_TREBLE_BOOST,
};
static const aura_str_id_t language_choice_labels[] = {
    AURA_STR_LANG_ES, AURA_STR_LANG_EN,
};
/* Solo Desactivado/Todo/Uno -- REPEAT_SHUFFLE y REPEAT_AB quedan fuera
 * del modelo simplificado de Aura (el aleatorio ya es su propio
 * booleano independiente, D-014/Fase 17). El indice de esta lista
 * coincide 1:1 con REPEAT_OFF/REPEAT_ALL/REPEAT_ONE de Rockbox
 * (apps/settings.h), asi que no hace falta traducir indices. */
static const aura_str_id_t repeat_choice_labels[] = {
    AURA_STR_REPEAT_OFF, AURA_STR_REPEAT_ALL, AURA_STR_REPEAT_ONE,
};

static int is_choice_screen(aura_screen_id_t screen)
{
    return screen == AURA_SCREEN_SETTINGS_THEME
        || screen == AURA_SCREEN_SETTINGS_ANIMATIONS
        || screen == AURA_SCREEN_SETTINGS_GRAPHICS
        || screen == AURA_SCREEN_SETTINGS_EQ
        || screen == AURA_SCREEN_SETTINGS_LANGUAGE
        || screen == AURA_SCREEN_SETTINGS_REPEAT;
}

static int get_choice_table(aura_screen_id_t screen, const aura_str_id_t **out)
{
    switch (screen)
    {
    case AURA_SCREEN_SETTINGS_THEME:
        *out = theme_choice_labels;
        return sizeof(theme_choice_labels) / sizeof(theme_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_ANIMATIONS:
        *out = animation_choice_labels;
        return sizeof(animation_choice_labels) / sizeof(animation_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_GRAPHICS:
        *out = graphics_choice_labels;
        return sizeof(graphics_choice_labels) / sizeof(graphics_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_EQ:
        *out = eq_choice_labels;
        return sizeof(eq_choice_labels) / sizeof(eq_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_LANGUAGE:
        *out = language_choice_labels;
        return sizeof(language_choice_labels) / sizeof(language_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_REPEAT:
        *out = repeat_choice_labels;
        return sizeof(repeat_choice_labels) / sizeof(repeat_choice_labels[0]);
    default:
        *out = NULL;
        return 0;
    }
}

static int get_choice_current(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_SETTINGS_THEME:      return (int)aura_settings.theme;
    case AURA_SCREEN_SETTINGS_ANIMATIONS: return (int)aura_settings.animation_mode;
    case AURA_SCREEN_SETTINGS_GRAPHICS:   return (int)aura_settings.graphics_mode;
    case AURA_SCREEN_SETTINGS_EQ:       return (int)aura_settings.eq_preset;
    case AURA_SCREEN_SETTINGS_LANGUAGE: return (int)aura_settings.language;
    case AURA_SCREEN_SETTINGS_REPEAT:   return global_settings.repeat_mode;
    default:                            return 0;
    }
}

static void apply_choice(aura_screen_id_t screen, int index)
{
    /* AURA_SCREEN_SETTINGS_REPEAT es un ajuste real de Rockbox
     * (global_settings.repeat_mode, motor de playlist -- D-021), no
     * uno propio de Aura: se persiste con settings_save(), no
     * aura_settings_save(). */
    if (screen == AURA_SCREEN_SETTINGS_REPEAT)
    {
        global_settings.repeat_mode = index;
        settings_save();
        return;
    }

    switch (screen)
    {
    case AURA_SCREEN_SETTINGS_THEME:
        aura_settings.theme = (aura_theme_id_t)index;
        break;
    case AURA_SCREEN_SETTINGS_ANIMATIONS:
        aura_settings.animation_mode = (aura_anim_mode_t)index;
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

/* -- Aleatorio: booleano real de Rockbox (playlist_start() ya lo lee
 * solo, D-021) -- pantalla propia con el widget de fila booleana. */
static void draw_shuffle(void)
{
    aura_widgets_draw_bool_row(aura_str(AURA_STR_SETTINGS_SHUFFLE),
                                aura_str(AURA_STR_SETTINGS_SHUFFLE),
                                global_settings.playlist_shuffle);
}

static void handle_shuffle(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SELECT:
        global_settings.playlist_shuffle = !global_settings.playlist_shuffle;
        settings_save();
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
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

/* Fase 24: contadores/bytes reales que dejo Aura Studio en el ultimo
 * sync (aura_manifest_load()), no solo la version del firmware -- si
 * el dispositivo nunca se sincronizo desde Studio (aura_manifest_load
 * devuelve false, p. ej. instalacion recien flasheada) se muestra un
 * aviso en vez de contadores en cero que podrian confundirse con "la
 * biblioteca esta vacia". */
static void draw_about(void)
{
    const int line_h = AURA_TYPE_BODY + AURA_SPACING_SM;
    int y = AURA_LAYOUT_STATUSBAR_HEIGHT + AURA_SPACING_LG;
    aura_manifest_t manifest;
    char size_buf[16];
    char line_buf[48];

    aura_theme_clear_screen();
    aura_statusbar_draw(0, AURA_SCREEN_WIDTH, aura_str(AURA_STR_SETTINGS_ABOUT), 0);

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));

    lcd_putsxy(AURA_SPACING_LG, y, (const unsigned char *)aura_str(AURA_STR_ABOUT_BUILT_ON));
    y += line_h;
    lcd_putsxy(AURA_SPACING_LG, y, (const unsigned char *)rbversion);
    y += line_h + AURA_SPACING_LG;

    if (!aura_manifest_load(&manifest))
    {
        lcd_putsxy(AURA_SPACING_LG, y, (const unsigned char *)aura_str(AURA_STR_ABOUT_NO_SYNC));
        return;
    }

    output_dyn_value(size_buf, sizeof(size_buf), manifest.music_bytes, byte_units, 4, true);
    snprintf(line_buf, sizeof(line_buf), "%s: %d (%s)",
             aura_str(AURA_STR_ABOUT_MUSIC), manifest.music_count, size_buf);
    lcd_putsxy(AURA_SPACING_LG, y, (const unsigned char *)line_buf);
    y += line_h;

    output_dyn_value(size_buf, sizeof(size_buf), manifest.video_bytes, byte_units, 4, true);
    snprintf(line_buf, sizeof(line_buf), "%s: %d (%s)",
             aura_str(AURA_STR_ABOUT_VIDEOS), manifest.video_count, size_buf);
    lcd_putsxy(AURA_SPACING_LG, y, (const unsigned char *)line_buf);
    y += line_h;

    output_dyn_value(size_buf, sizeof(size_buf), manifest.photo_bytes, byte_units, 4, true);
    snprintf(line_buf, sizeof(line_buf), "%s: %d (%s)",
             aura_str(AURA_STR_ABOUT_PHOTOS), manifest.photo_count, size_buf);
    lcd_putsxy(AURA_SPACING_LG, y, (const unsigned char *)line_buf);
    y += line_h;

    snprintf(line_buf, sizeof(line_buf), "%s: %d",
             aura_str(AURA_STR_ABOUT_PLAYLISTS), manifest.playlist_count);
    lcd_putsxy(AURA_SPACING_LG, y, (const unsigned char *)line_buf);
}

/* -- Temporiz. luz / Temporiz. reposo: listas de opciones numericas,
 * no de cadenas fijas -- formateadas al vuelo (Fase 18). ------------- */

#define NUMERIC_CHOICE_MAX 8
static char s_numeric_labels[NUMERIC_CHOICE_MAX][16];

static const int backlight_values[] = { 0, 2, 5, 10, 20, 30, -1 };
#define BACKLIGHT_VALUES_N ((int)(sizeof(backlight_values) / sizeof(backlight_values[0])))

static const int sleeptimer_values[] = { 0, 15, 30, 60, 90, 120 };
#define SLEEPTIMER_VALUES_N ((int)(sizeof(sleeptimer_values) / sizeof(sleeptimer_values[0])))

static void draw_backlight(aura_nav_t *nav)
{
    aura_list_item_t items[BACKLIGHT_VALUES_N];
    int i;

    for (i = 0; i < BACKLIGHT_VALUES_N; i++)
    {
        int v = backlight_values[i];
        if (v == 0)
            strlcpy(s_numeric_labels[i], aura_str(AURA_STR_TIMEOUT_OFF), sizeof(s_numeric_labels[i]));
        else if (v < 0)
            strlcpy(s_numeric_labels[i], aura_str(AURA_STR_TIMEOUT_ALWAYS), sizeof(s_numeric_labels[i]));
        else
            snprintf(s_numeric_labels[i], sizeof(s_numeric_labels[i]), "%d s", v);

        items[i].label = s_numeric_labels[i];
        items[i].icon_name = NULL;
        items[i].checked = (v == global_settings.backlight_timeout);
    }
    aura_widgets_draw_list(aura_str(AURA_STR_SETTINGS_BACKLIGHT), items, BACKLIGHT_VALUES_N,
                            aura_nav_get_selection(nav));
}

static void handle_backlight(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < BACKLIGHT_VALUES_N - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        /* Aura simplifica a un solo ajuste (en vez de separar
         * desenchufado/enchufado, D-051): aplica el mismo valor a
         * ambos backends reales de Rockbox. */
        global_settings.backlight_timeout = backlight_values[sel];
        global_settings.backlight_timeout_plugged = backlight_values[sel];
        backlight_set_timeout(global_settings.backlight_timeout);
        backlight_set_timeout_plugged(global_settings.backlight_timeout_plugged);
        settings_save();
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

static void draw_sleeptimer(aura_nav_t *nav)
{
    aura_list_item_t items[SLEEPTIMER_VALUES_N];
    int i;

    for (i = 0; i < SLEEPTIMER_VALUES_N; i++)
    {
        int v = sleeptimer_values[i];
        if (v == 0)
            strlcpy(s_numeric_labels[i], aura_str(AURA_STR_TIMEOUT_OFF), sizeof(s_numeric_labels[i]));
        else
            snprintf(s_numeric_labels[i], sizeof(s_numeric_labels[i]), "%d min", v);

        items[i].label = s_numeric_labels[i];
        items[i].icon_name = NULL;
        items[i].checked = (v == (int)global_settings.sleeptimer_duration);
    }
    aura_widgets_draw_list(aura_str(AURA_STR_SETTINGS_SLEEPTIMER), items, SLEEPTIMER_VALUES_N,
                            aura_nav_get_selection(nav));
}

static void handle_sleeptimer(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < SLEEPTIMER_VALUES_N - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        global_settings.sleeptimer_duration = sleeptimer_values[sel];
        set_sleeptimer_duration(sleeptimer_values[sel]);
        settings_save();
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

/* -- Limite de volumen: slider sobre el rango real del DAC ------------ */

static void draw_volume_limit(void)
{
    int vol_min = sound_min(SOUND_VOLUME);
    int vol_max = sound_max(SOUND_VOLUME);
    int fraction = (vol_max > vol_min)
        ? (256 * (global_settings.volume_limit - vol_min)) / (vol_max - vol_min)
        : 0;
    char buf[16];

    snprintf(buf, sizeof(buf), "%d dB", global_settings.volume_limit);
    aura_widgets_draw_slider(aura_str(AURA_STR_SETTINGS_VOLUME_LIMIT), fraction, buf);
}

static void handle_volume_limit(aura_nav_t *nav, long button)
{
    int vol_min = sound_min(SOUND_VOLUME);
    int vol_max = sound_max(SOUND_VOLUME);
    int step = sound_steps(SOUND_VOLUME);

    if (step <= 0)
        step = 1;

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (global_settings.volume_limit + step <= vol_max)
            global_settings.volume_limit += step;
        break;
    case BUTTON_SCROLL_BACK:
        if (global_settings.volume_limit - step >= vol_min)
            global_settings.volume_limit -= step;
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

/* -- Clicker: booleano real de Rockbox (global_settings.keyclick, 0..3
 * off/weak/moderate/strong) -- Aura lo simplifica a Si/No, aplicando
 * "moderate" (2) al activar. Ver D-06x en DECISIONS.md: system_sound_play()
 * solo suena si algo lo llama -- aura_main.c lo hace en cada boton real. */
static void draw_clicker(void)
{
    aura_widgets_draw_bool_row(aura_str(AURA_STR_SETTINGS_CLICKER),
                                aura_str(AURA_STR_SETTINGS_CLICKER),
                                global_settings.keyclick != 0);
}

static void handle_clicker(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SELECT:
        global_settings.keyclick = global_settings.keyclick ? 0 : 2;
        settings_save();
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

/* -- Menu principal configurable (L14) --------------------------------- */

#define MAINMENU_ROWS 4

static void draw_mainmenu(aura_nav_t *nav)
{
    aura_list_item_t items[MAINMENU_ROWS];

    items[0].label = aura_str(AURA_STR_VIDEOS);
    items[0].icon_name = NULL;
    items[0].checked = aura_settings.show_videos;
    items[1].label = aura_str(AURA_STR_PHOTOS);
    items[1].icon_name = NULL;
    items[1].checked = aura_settings.show_photos;
    items[2].label = aura_str(AURA_STR_NOWPLAYING);
    items[2].icon_name = NULL;
    items[2].checked = aura_settings.show_nowplaying;
    items[3].label = aura_str(AURA_STR_MAINMENU_RESTORE);
    items[3].icon_name = NULL;
    items[3].checked = 0;

    aura_widgets_draw_list(aura_str(AURA_STR_SETTINGS_MAINMENU), items, MAINMENU_ROWS,
                            aura_nav_get_selection(nav));
}

static void handle_mainmenu(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < MAINMENU_ROWS - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        switch (sel)
        {
        case 0: aura_settings.show_videos = !aura_settings.show_videos; break;
        case 1: aura_settings.show_photos = !aura_settings.show_photos; break;
        case 2: aura_settings.show_nowplaying = !aura_settings.show_nowplaying; break;
        case 3:
            aura_settings.show_videos = true;
            aura_settings.show_photos = true;
            aura_settings.show_nowplaying = true;
            break;
        }
        aura_settings_save();
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

/* -- Restablecer ajustes: aviso bloqueante con confirmacion (S3.8) ----- */

static bool s_reset_confirm_yes = false;

static void draw_reset_confirm(void)
{
    aura_widgets_draw_confirm(aura_str(AURA_STR_RESET_CONFIRM_TITLE),
                               aura_str(AURA_STR_RESET_CONFIRM_BODY),
                               s_reset_confirm_yes);
}

static void handle_reset_confirm(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
    case BUTTON_SCROLL_BACK:
        s_reset_confirm_yes = !s_reset_confirm_yes;
        break;
    case BUTTON_SELECT:
        if (s_reset_confirm_yes)
        {
            /* settings_reset() vuelve TODO ajuste real de Rockbox a su
             * default de fabrica -- incluidos los que la Fase 18 opina
             * distinto (backlight/volume_limit/poweroff/sleeptimer);
             * aura_settings_apply_core_defaults() los vuelve a poner
             * en los valores de Aura justo despues, igual que en el
             * primer arranque. Los ajustes de higiene "siempre en cada
             * boot" (D-051/D-055: statusbar, colores, usb_hid, etc.)
             * tambien vuelven al default de Rockbox hasta el proximo
             * reinicio -- apps/main.c los reaplica en cada arranque de
             * cualquier forma, asi que se autocorrigen solos. */
            settings_reset();
            settings_save();
            aura_settings_reset_to_defaults();
            aura_settings_apply_core_defaults();
        }
        s_reset_confirm_yes = false;
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU:
        s_reset_confirm_yes = false;
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
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

/* En modo grafico Pro, Albumes se navega con Coverflow en vez de la
 * lista plana (D-025); en Ultra/Minimalista sigue siendo una lista
 * como cualquier otra pantalla de is_music_browse_screen(). La raiz ya
 * no tiene una pantalla especial propia (D-025 la retiro en la Fase
 * 15): aura_widgets_draw_list() dibuja la pantalla dividida
 * izquierda/derecha para *cualquier* lista en modo no-Ultra, root
 * incluida -- ver D-057 en DECISIONS.md. */
static int is_coverflow_screen(aura_screen_id_t screen)
{
    return aura_settings.graphics_mode == AURA_GFX_ALL
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

/* Tabla de layout: UNICA fuente de verdad de que pantalla se dibuja
 * dividida (lista izquierda + panel de preview) y cual a ancho completo.
 * El firmware original tiene de las dos, asi que Aura tampoco puede
 * asumir una sola: los menus de navegacion y las listas de contenido son
 * divididos; las pantallas con maquetacion propia (sliders de Brillo y
 * Limite de volumen, filas booleanas, Acerca de, aviso de reset,
 * Coverflow, visor de fotos, Ahora suena) son de ancho completo y ni
 * siquiera pasan por aura_widgets_draw_list().
 *
 * La consumen dos lugares: aura_screens_draw() (para el dibujo) y
 * aura_screens_handle_button() (para decidir si el empuje de la
 * transicion abarca solo el panel izquierdo o toda la pantalla). */
static int screen_uses_split_layout(aura_screen_id_t screen)
{
    return screen == AURA_SCREEN_ROOT
        || screen == AURA_SCREEN_SETTINGS
        || screen == AURA_SCREEN_MUSIC
        || is_choice_screen(screen)
        || screen == AURA_SCREEN_SETTINGS_BACKLIGHT
        || screen == AURA_SCREEN_SETTINGS_SLEEPTIMER
        || screen == AURA_SCREEN_SETTINGS_MAINMENU
        || is_music_browse_screen(screen)
        || screen == AURA_SCREEN_MUSIC_PLAYLISTS
        || screen == AURA_SCREEN_VIDEOS
        || screen == AURA_SCREEN_PHOTOS;
}

void aura_screens_draw(aura_nav_t *nav)
{
    aura_screen_id_t screen = aura_nav_current(nav);

    aura_widgets_set_list_layout(screen_uses_split_layout(screen)
                                  ? AURA_LIST_SPLIT : AURA_LIST_FULL);

    if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS || screen == AURA_SCREEN_MUSIC)
        draw_nav_list(nav, screen);
    else if (is_choice_screen(screen))
        draw_choice_list(nav, screen);
    else if (screen == AURA_SCREEN_SETTINGS_BRIGHTNESS)
        draw_brightness();
    else if (screen == AURA_SCREEN_SETTINGS_ABOUT)
        draw_about();
    else if (screen == AURA_SCREEN_SETTINGS_SHUFFLE)
        draw_shuffle();
    else if (screen == AURA_SCREEN_SETTINGS_BACKLIGHT)
        draw_backlight(nav);
    else if (screen == AURA_SCREEN_SETTINGS_SLEEPTIMER)
        draw_sleeptimer(nav);
    else if (screen == AURA_SCREEN_SETTINGS_VOLUME_LIMIT)
        draw_volume_limit();
    else if (screen == AURA_SCREEN_SETTINGS_CLICKER)
        draw_clicker();
    else if (screen == AURA_SCREEN_SETTINGS_MAINMENU)
        draw_mainmenu(nav);
    else if (screen == AURA_SCREEN_SETTINGS_RESET)
        draw_reset_confirm();
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

    if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS || screen == AURA_SCREEN_MUSIC)
        handle_nav_list(nav, screen, button);
    else if (is_choice_screen(screen))
        handle_choice_list(nav, screen, button);
    else if (screen == AURA_SCREEN_SETTINGS_BRIGHTNESS)
        handle_brightness(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_SHUFFLE)
        handle_shuffle(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_BACKLIGHT)
        handle_backlight(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_SLEEPTIMER)
        handle_sleeptimer(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_VOLUME_LIMIT)
        handle_volume_limit(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_CLICKER)
        handle_clicker(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_MAINMENU)
        handle_mainmenu(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_RESET)
        handle_reset_confirm(nav, button);
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
     * sentido. Ver D-024. Entrar a Coverflow usa T4 (revelado desde
     * ambos bordes, D-058/Fase 16) en vez del wipe T1/T3 comun, para
     * que se sienta distinto de una navegacion de lista. */
    int depth_after = aura_nav_depth(nav);
    if (depth_after != depth_before)
    {
        aura_screen_id_t to = aura_nav_current(nav);

        if (depth_after > depth_before && is_coverflow_screen(to))
            aura_transition_reveal(nav);
        else
        {
            /* T1 vs T3 segun los DOS extremos de la navegacion (L4):
             * si origen y destino son menus divididos, el empuje solo
             * se ve en el panel izquierdo -- el panel derecho es una
             * capa que no se mueve (L1) y se actualiza despues con su
             * debounce de ~1s (L3). Si cualquiera de los dos es de
             * pantalla completa, es un T3: push de ancho completo.
             * `screen` conserva la pantalla activa al entrar a esta
             * funcion (el origen); `to` es a donde se navego. */
            /* Se consulta la tabla directamente y no
             * aura_widgets_split_active(), que refleja el layout de la
             * ultima pantalla dibujada -- aca hacen falta los dos
             * extremos de la navegacion, no el estado del renderer. */
            int width = (aura_settings.graphics_mode != AURA_GFX_NONE
                         && screen_uses_split_layout(screen)
                         && screen_uses_split_layout(to))
                        ? AURA_LAYOUT_PANEL_LEFT_WIDTH
                        : AURA_SCREEN_WIDTH;

            /* Al volver, el preview del padre se restaura al instante
             * (sin el retardo de ~1s de seleccion nueva) -- observado
             * cuadro a cuadro en el original, D-068. Antes de la
             * transicion, para que el render offscreen ya lo incluya. */
            if (depth_after < depth_before)
                aura_widgets_panel_force_next();

            aura_transition_slide(nav, depth_after > depth_before ? 1 : -1,
                                  width);
        }
    }
}
