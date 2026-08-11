#include <string.h>
#include <stdio.h>

#include "button.h"
#include "audio.h"
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
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "apple2026_tokens.h"
#include "aura_music.h"
#include "aura_nowplaying.h"
#include "aura_transitions.h"
#include "aura_coverflow.h"
#include "aura_photos.h"
#include "aura_video.h"
#include "aura_manifest.h"
#include "aura_main.h"
#include "aura_wheel.h"

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

/* Iconos elegidos y verificados contra el catalogo real de SF Symbols
 * (D-075) -- ninguno se repite dentro de la lista (doc SS4, "hermanos se
 * distinguen"). */
static const nav_entry_t settings_entries[] = {
    { AURA_STR_SETTINGS_THEME,      "theme",             AURA_SCREEN_SETTINGS_THEME },
    { AURA_STR_SETTINGS_ANIMATIONS, "motion",            AURA_SCREEN_SETTINGS_ANIMATIONS },
    { AURA_STR_SETTINGS_GRAPHICS,   "graphics",          AURA_SCREEN_SETTINGS_GRAPHICS },
    { AURA_STR_SETTINGS_EQ,         "sliders-horizontal", AURA_SCREEN_SETTINGS_EQ },
    { AURA_STR_SETTINGS_BRIGHTNESS, "sun",               AURA_SCREEN_SETTINGS_BRIGHTNESS },
    { AURA_STR_SETTINGS_SHUFFLE,    "shuffle",           AURA_SCREEN_SETTINGS_SHUFFLE },
    { AURA_STR_SETTINGS_REPEAT,     "repeat",            AURA_SCREEN_SETTINGS_REPEAT },
    { AURA_STR_SETTINGS_BACKLIGHT,     "backlight",      AURA_SCREEN_SETTINGS_BACKLIGHT },
    { AURA_STR_SETTINGS_SLEEPTIMER,    "sleep",          AURA_SCREEN_SETTINGS_SLEEPTIMER },
    { AURA_STR_SETTINGS_VOLUME_LIMIT,  "volume-limit",   AURA_SCREEN_SETTINGS_VOLUME_LIMIT },
    { AURA_STR_SETTINGS_CLICKER,       "tap",            AURA_SCREEN_SETTINGS_CLICKER },
    { AURA_STR_SETTINGS_MAINMENU,      "menu-list",      AURA_SCREEN_SETTINGS_MAINMENU },
    { AURA_STR_SETTINGS_LANGUAGE,   "globe",             AURA_SCREEN_SETTINGS_LANGUAGE },
    { AURA_STR_SETTINGS_ABOUT,      "info",              AURA_SCREEN_SETTINGS_ABOUT },
    { AURA_STR_SETTINGS_RESET,         "reset",          AURA_SCREEN_SETTINGS_RESET },
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

/* -- Aleatorio y Clicker: booleanos reales de Rockbox (playlist_shuffle,
 * D-021; keyclick != 0, D-06x) -- viven inline en la fila de Ajustes con
 * un switch (D-075), no en pantalla propia: el doc de comportamiento ya
 * los describe como `[OPCION]` simple, "sigue la regla de profundidad
 * por defecto, no tiene mecanica propia". `settings_row_toggle_value()`
 * lee el valor actual para dibujar el switch; `toggle_settings_row()`
 * lo invierte in situ. Ambas viven aca porque son las dos unicas filas
 * de Ajustes con este comportamiento hoy -- si aparece una tercera, se
 * generaliza. */
static int settings_row_toggle_value(aura_screen_id_t target)
{
    if (target == AURA_SCREEN_SETTINGS_SHUFFLE)
        return global_settings.playlist_shuffle;
    if (target == AURA_SCREEN_SETTINGS_CLICKER)
        return global_settings.keyclick != 0;
    return -1;
}

static void toggle_settings_row(aura_screen_id_t target)
{
    if (target == AURA_SCREEN_SETTINGS_SHUFFLE)
        global_settings.playlist_shuffle = !global_settings.playlist_shuffle;
    else if (target == AURA_SCREEN_SETTINGS_CLICKER)
        global_settings.keyclick = global_settings.keyclick ? 0 : 2;
    else
        return;
    settings_save();
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
        items[i].toggle = settings_row_toggle_value(entries[i].target);
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
        /* Iconos canonicos del doc de diseno SS4 ("sun.max.circle /
         * moon.circle = temas") -- unica lista de eleccion con icono
         * propio por fila hoy: el resto (Graficos/EQ/Idioma/Repetir) son
         * niveles o presets abstractos sin un simbolo 1:1 natural, y
         * forzar uno repetido violaria "hermanos se distinguen" mas de
         * lo que ayudaria (D-075). */
        items[i].icon_name = (screen == AURA_SCREEN_SETTINGS_THEME)
            ? (i == 0 ? "theme-light" : "theme-dark")
            : NULL;
        items[i].checked = (i == current);
        items[i].toggle = -1;
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

/* Acerca de: FULL-COLD con 3 modos navegables (doc de comportamiento
 * SS4.6) -- el original real tiene barra de espacio por categoria,
 * contador de archivos, e info del dispositivo como TRES pantallas
 * separadas que se recorren con izquierda/derecha (Select = adelante),
 * no una sola lista larga. La primera version de esta pantalla
 * (Fase 24) las concatenaba todas en una -- vacio real contra el doc,
 * no una decision (nadie habia auditado esta pantalla contra el
 * inventario hasta la Fase 32). */
typedef enum {
    ABOUT_PAGE_STORAGE = 0,
    ABOUT_PAGE_COUNTS,
    ABOUT_PAGE_DEVICE,
    ABOUT_PAGE_COUNT,
} about_page_t;

static int s_about_page = ABOUT_PAGE_STORAGE;
static aura_screen_id_t s_about_last_screen = AURA_SCREEN_COUNT;

#define ABOUT_CONTENT_Y (A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_XXL)

static void draw_about_dots(void)
{
    /* Indicador de pagina (doc no lo especifica en detalle -- FULL-COLD
     * generico, Fase 32): puntos simples, la pagina activa en ACCENT,
     * el resto en SHELL_RAIL. Mismo lenguaje visual que cualquier
     * paginador. */
    int dot = 4, gap = A26_SPACING_MD;
    int total_w = ABOUT_PAGE_COUNT * dot + (ABOUT_PAGE_COUNT - 1) * gap;
    int x = (A26_SCREEN_WIDTH - total_w) / 2;
    int y = A26_SCREEN_HEIGHT - A26_SPACING_XXL;
    int i;

    for (i = 0; i < ABOUT_PAGE_COUNT; i++)
    {
        lcd_set_foreground(a26_color(i == s_about_page ? A26_ACCENT : A26_SHELL_RAIL));
        lcd_fillrect(x + i * (dot + gap), y, dot, dot);
    }
}

/* Pagina 1: barra de espacio usado por categoria (doc: "espacio de
 * almacenamiento usado, por categoria: Audio, Video, Fotos, Otros").
 * Sin "Otros": necesitaria consultar el espacio real de disco (fat_size)
 * ademas del manifiesto de Aura Studio, una API nueva sin precedente en
 * este modulo -- se muestra la proporcion entre las tres categorias
 * reales que Aura si conoce, simplificacion documentada (D-081). */
static void draw_about_storage(const aura_manifest_t *m)
{
    int bar_x = A26_SPACING_XXL;
    int bar_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    int bar_y = ABOUT_CONTENT_Y + A26_SPACING_XXL;
    long long total = m->music_bytes + m->video_bytes + m->photo_bytes;
    int seg_x = bar_x;
    int i;
    struct { long long bytes; aura_str_id_t label; } cats[3] = {
        { m->music_bytes, AURA_STR_ABOUT_MUSIC },
        { m->video_bytes, AURA_STR_ABOUT_VIDEOS },
        { m->photo_bytes, AURA_STR_ABOUT_PHOTOS },
    };
    int label_y = bar_y + A26_SPACING_LG + A26_SPACING_SM;

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_putsxy(bar_x, ABOUT_CONTENT_Y, (const unsigned char *)aura_str(AURA_STR_ABOUT_STORAGE));

    /* Radio = mitad del alto (SS5.4: extremos totalmente redondeados de
     * una barra fina, misma derivacion que el resto de las barras de
     * progreso del sistema -- no un radio suelto). */
    a26_shell_fill_rounded_rect(bar_x, bar_y, bar_w, A26_SPACING_LG,
                                 A26_SPACING_LG / 2, a26_color(A26_PROGRESS_TRACK),
                                 a26_color(A26_SHELL_BG));

    for (i = 0; i < 3 && total > 0; i++)
    {
        int seg_w = (int)(bar_w * cats[i].bytes / total);
        if (seg_w <= 0)
            continue;
        /* Segmentos separados por una linea de 1px (SHELL_RAIL) en vez
         * de color por categoria -- Aura no tiene paleta por categoria
         * (D-075: "el acento se gana, nunca decoracion de superficie"),
         * asi que la distincion es geometrica, no cromatica. */
        lcd_set_foreground(a26_color(A26_PROGRESS_FILL));
        lcd_fillrect(seg_x, bar_y, seg_w, A26_SPACING_LG);
        if (seg_x > bar_x)
        {
            lcd_set_foreground(a26_color(A26_SHELL_BG));
            lcd_vline(seg_x, bar_y, bar_y + A26_SPACING_LG - 1);
        }
        seg_x += seg_w;
    }

    for (i = 0; i < 3; i++)
    {
        char line[48], size_buf[16];
        output_dyn_value(size_buf, sizeof(size_buf), cats[i].bytes, byte_units, 4, true);
        snprintf(line, sizeof(line), "%s: %s", aura_str(cats[i].label), size_buf);
        lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
        lcd_putsxy(bar_x, label_y, (const unsigned char *)line);
        label_y += A26_TYPE_BODY + A26_SPACING_SM;
    }
}

/* Pagina 2: contador de archivos (doc: "Canciones, Videos, Podcast,
 * Fotos, Juegos, Contactos" -- Aura solo tiene Musica/Video/Fotos/
 * Listas reales, se muestran esas). */
static void draw_about_counts(const aura_manifest_t *m)
{
    const int line_h = A26_TYPE_BODY + A26_SPACING_SM;
    int y = ABOUT_CONTENT_Y;
    char line[48];

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));

    snprintf(line, sizeof(line), "%s: %d", aura_str(AURA_STR_ABOUT_MUSIC), m->music_count);
    lcd_putsxy(A26_SPACING_LG, y, (const unsigned char *)line);
    y += line_h;

    snprintf(line, sizeof(line), "%s: %d", aura_str(AURA_STR_ABOUT_VIDEOS), m->video_count);
    lcd_putsxy(A26_SPACING_LG, y, (const unsigned char *)line);
    y += line_h;

    snprintf(line, sizeof(line), "%s: %d", aura_str(AURA_STR_ABOUT_PHOTOS), m->photo_count);
    lcd_putsxy(A26_SPACING_LG, y, (const unsigned char *)line);
    y += line_h;

    snprintf(line, sizeof(line), "%s: %d", aura_str(AURA_STR_ABOUT_PLAYLISTS), m->playlist_count);
    lcd_putsxy(A26_SPACING_LG, y, (const unsigned char *)line);
}

/* Pagina 3: info del dispositivo (doc: numero de serie/modelo/version --
 * Aura no tiene un numero de serie que mostrar con sentido, asi que
 * queda solo la version real del firmware, que ya se mostraba en la
 * version de una sola pagina). */
static void draw_about_device(void)
{
    const int line_h = A26_TYPE_BODY + A26_SPACING_SM;
    int y = ABOUT_CONTENT_Y;

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_putsxy(A26_SPACING_LG, y, (const unsigned char *)aura_str(AURA_STR_ABOUT_BUILT_ON));
    y += line_h;
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_putsxy(A26_SPACING_LG, y, (const unsigned char *)rbversion);
}

static void draw_about(void)
{
    aura_manifest_t manifest;
    bool has_manifest;

    if (s_about_last_screen != AURA_SCREEN_SETTINGS_ABOUT)
        s_about_page = ABOUT_PAGE_STORAGE; /* reinicia solo al ENTRAR de nuevo */
    s_about_last_screen = AURA_SCREEN_SETTINGS_ABOUT;

    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_SETTINGS_ABOUT), 0);

    has_manifest = aura_manifest_load(&manifest);

    if (!has_manifest && s_about_page != ABOUT_PAGE_DEVICE)
    {
        lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_putsxy(A26_SPACING_LG, ABOUT_CONTENT_Y,
                   (const unsigned char *)aura_str(AURA_STR_ABOUT_NO_SYNC));
    }
    else switch (s_about_page)
    {
    case ABOUT_PAGE_STORAGE: draw_about_storage(&manifest); break;
    case ABOUT_PAGE_COUNTS:  draw_about_counts(&manifest); break;
    case ABOUT_PAGE_DEVICE:  draw_about_device(); break;
    default: break;
    }

    draw_about_dots();
}

static void handle_about(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SELECT:
    case BUTTON_RIGHT:
        if (s_about_page < ABOUT_PAGE_COUNT - 1)
            s_about_page++;
        break;
    case BUTTON_LEFT:
        if (s_about_page > 0)
            s_about_page--;
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
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
        items[i].toggle = -1;
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
        items[i].toggle = -1;
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

/* -- Menu principal configurable (L14) --------------------------------- */

#define MAINMENU_ROWS 4

static void draw_mainmenu(aura_nav_t *nav)
{
    aura_list_item_t items[MAINMENU_ROWS];

    items[0].label = aura_str(AURA_STR_VIDEOS);
    items[0].icon_name = "video";
    items[0].checked = aura_settings.show_videos;
    items[0].toggle = -1;
    items[1].label = aura_str(AURA_STR_PHOTOS);
    items[1].icon_name = "image";
    items[1].checked = aura_settings.show_photos;
    items[1].toggle = -1;
    items[2].label = aura_str(AURA_STR_NOWPLAYING);
    items[2].icon_name = "play";
    items[2].checked = aura_settings.show_nowplaying;
    items[2].toggle = -1;
    items[3].label = aura_str(AURA_STR_MAINMENU_RESTORE);
    items[3].icon_name = "reset";
    items[3].checked = 0;
    items[3].toggle = -1;

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
    int content_top = A26_LAYOUT_STATUSBAR_HEIGHT;
    int content_h = A26_SCREEN_HEIGHT - content_top;

    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, NULL, 0);

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, content_top + (content_h - h) / 2,
               (const unsigned char *)aura_str(msg_id));
}

/* Pantalla en espera de un recurso que todavia esta cargando en segundo
 * plano (hoy: la base de datos musical durante el escaneo inicial) --
 * a diferencia de draw_message_centered(), NO es un estado terminal:
 * usa la capsula flotante (SS5.2, Principio 3), no un texto centrado en
 * pagina completa (ese patron queda solo para los estados propios del
 * aparato, no para esperas). */
static void draw_waiting_state(aura_str_id_t msg_id)
{
    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, NULL, 0);
    aura_widgets_draw_wait_capsule(aura_str(msg_id));
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
        draw_waiting_state(AURA_STR_DB_NOT_READY);
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
        s_music_items_buf[i].toggle = -1;
    }

    aura_widgets_draw_list(aura_str(screen_title_id(screen)), s_music_items_buf,
                            s_music_cache_count, aura_nav_get_selection(nav));
}

static aura_screen_id_t s_playlist_cache_screen = AURA_SCREEN_COUNT;
static char s_playlist_cache[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
static int s_playlist_cache_count = 0;

static void ensure_playlist_cache(aura_screen_id_t screen)
{
    int i;

    if (s_playlist_cache_screen == screen)
        return;

    s_playlist_cache_screen = screen;
    s_playlist_cache_count = aura_music_list_playlists(s_playlist_cache, AURA_MUSIC_MAX_ITEMS);

    /* Este cache es solo para mostrar (aura_music_play_playlist() vuelve
     * a resolver el archivo real por su cuenta) -- se puede pelar la
     * extension in situ sin romper nada mas (Fase 32, D-081: ".m3u8" a
     * la vista del usuario es jerga tecnica de archivo, doc Principio 7). */
    for (i = 0; i < s_playlist_cache_count; i++)
        aura_music_playlist_display_name(s_playlist_cache[i], s_playlist_cache[i],
                                          AURA_MUSIC_ITEM_LEN);
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
        s_music_items_buf[i].toggle = -1;
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
    else if (screen == AURA_SCREEN_SETTINGS_BACKLIGHT)
        draw_backlight(nav);
    else if (screen == AURA_SCREEN_SETTINGS_SLEEPTIMER)
        draw_sleeptimer(nav);
    else if (screen == AURA_SCREEN_SETTINGS_VOLUME_LIMIT)
        draw_volume_limit();
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

/* Dinamica de rueda (doc SS7, Fase 29): cuantos items avanza un
 * SCROLL_FWD/BACK depende de que tan rapido se esta girando de verdad
 * -- aura_wheel_step() traduce la velocidad angular del ultimo evento
 * (aura_main_wheel_velocity(), leida del driver real del clickwheel) a
 * 1-3 items. Con velocidad 0 (arnes de botones pautado, eventos
 * sinteticos) siempre da 1 -- degrada al comportamiento de antes. */
static int wheel_advance(int sel, int count, int direction)
{
    int step, next;

    if (count <= 0)
        return sel; /* lista vacia: nada que recorrer (mismo comportamiento que antes) */

    step = aura_wheel_step((int)aura_main_wheel_velocity());
    next = sel + direction * step;

    if (next < 0)
        next = 0;
    if (next > count - 1)
        next = count - 1;
    return next;
}

static void handle_nav_list(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    const nav_entry_t *entries;
    int count = get_nav_table(screen, &entries);
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        aura_nav_set_selection(nav, wheel_advance(sel, count, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, wheel_advance(sel, count, -1));
        break;
    case BUTTON_SELECT:
        /* Filas booleanas (Aleatorio, Clicker): SELECT invierte el
         * switch in situ y NO navega -- doc de comportamiento SS1,
         * `[OPCION]` "no tiene mecanica propia" (D-075). Distinto de
         * cualquier otra fila de Ajustes, que si empuja su pantalla. */
        if (settings_row_toggle_value(entries[sel].target) >= 0)
        {
            toggle_settings_row(entries[sel].target);
            break;
        }
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
        aura_nav_set_selection(nav, wheel_advance(sel, count, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, wheel_advance(sel, count, -1));
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
        aura_nav_set_selection(nav, wheel_advance(sel, count, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, wheel_advance(sel, count, -1));
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

    /* PLAY global (doc de comportamiento SS7, Fase 29): reproducir/
     * pausar funciona desde CUALQUIER pantalla, no solo Ahora suena --
     * en el despachador central, no repetido por pantalla. Antes SOLO
     * funcionaba dentro de Ahora suena (aura_nowplaying_handle_button);
     * en el resto de la app BUTTON_PLAY no hacia nada, un vacio real
     * contra el doc, no una decision. Mismo guard que ya usaba
     * aura_nowplaying_active(): sin nada cargado (audio_status()==0)
     * no hace nada, no fuerza una pausa sin sentido. */
    if (button == BUTTON_PLAY)
    {
        int status = audio_status();
        if (status & AUDIO_STATUS_PAUSE)
            audio_resume();
        else if (status & AUDIO_STATUS_PLAY)
            audio_pause();
        return;
    }

    if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS || screen == AURA_SCREEN_MUSIC)
        handle_nav_list(nav, screen, button);
    else if (is_choice_screen(screen))
        handle_choice_list(nav, screen, button);
    else if (screen == AURA_SCREEN_SETTINGS_BRIGHTNESS)
        handle_brightness(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_BACKLIGHT)
        handle_backlight(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_SLEEPTIMER)
        handle_sleeptimer(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_VOLUME_LIMIT)
        handle_volume_limit(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_MAINMENU)
        handle_mainmenu(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_RESET)
        handle_reset_confirm(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_ABOUT)
        handle_about(nav, button);
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
                        ? A26_LAYOUT_PANEL_LEFT_WIDTH
                        : A26_SCREEN_WIDTH;

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
