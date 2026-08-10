/* Tabla de cadenas ES/EN propia de Aura UI.
 *
 * Independiente del sistema de idiomas (.lang) de Rockbox: Aura no
 * reutiliza ninguna cadena de la UI que reemplaza, asi que mantiene su
 * propia tabla, pequena y completa, en vez de generar un .lang nuevo
 * para un puñado de textos. Ver D-013 en DECISIONS.md.
 */
#ifndef AURA_LANG_H
#define AURA_LANG_H

typedef enum {
    AURA_STR_MUSIC = 0,
    AURA_STR_VIDEOS,
    AURA_STR_PHOTOS,
    AURA_STR_NOWPLAYING,
    AURA_STR_SETTINGS,

    AURA_STR_SETTINGS_THEME,
    AURA_STR_SETTINGS_GRAPHICS,
    AURA_STR_SETTINGS_EQ,
    AURA_STR_SETTINGS_BRIGHTNESS,
    AURA_STR_SETTINGS_LANGUAGE,
    AURA_STR_SETTINGS_ABOUT,

    AURA_STR_THEME_LIGHT,
    AURA_STR_THEME_DARK,

    AURA_STR_GFX_ULTRA,
    AURA_STR_GFX_MINIMAL,
    AURA_STR_GFX_FULL,

    AURA_STR_EQ_FLAT,
    AURA_STR_EQ_BASS_BOOST,
    AURA_STR_EQ_VOCAL,
    AURA_STR_EQ_TREBLE_BOOST,

    AURA_STR_LANG_ES,
    AURA_STR_LANG_EN,

    AURA_STR_ABOUT_BUILT_ON,

    AURA_STR_EMPTY_MUSIC,
    AURA_STR_EMPTY_VIDEOS,
    AURA_STR_EMPTY_PHOTOS,
    AURA_STR_NOTHING_PLAYING,

    AURA_STR_MUSIC_ARTISTS,
    AURA_STR_MUSIC_ALBUMS,
    AURA_STR_MUSIC_SONGS,
    AURA_STR_MUSIC_PLAYLISTS,
    AURA_STR_MUSIC_GENRES,
    AURA_STR_DB_NOT_READY,
    AURA_STR_EMPTY_LIST,
    AURA_STR_UNSUPPORTED_FORMAT,

    AURA_STR_COUNT,
} aura_str_id_t;

/* Devuelve la cadena para el idioma activo (aura_settings.language). */
const char *aura_str(aura_str_id_t id);

#endif /* AURA_LANG_H */
