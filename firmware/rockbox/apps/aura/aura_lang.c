#include "aura_lang.h"
#include "aura_settings.h"

static const char *const strings_es[AURA_STR_COUNT] = {
    [AURA_STR_MUSIC]              = "Música",
    [AURA_STR_VIDEOS]             = "Videos",
    [AURA_STR_PHOTOS]             = "Fotos",
    [AURA_STR_NOWPLAYING]         = "Ahora suena",
    [AURA_STR_SETTINGS]           = "Ajustes",

    [AURA_STR_SETTINGS_THEME]       = "Tema",
    [AURA_STR_SETTINGS_ANIMATIONS]  = "Animaciones",
    [AURA_STR_SETTINGS_GRAPHICS]    = "Gráficos",
    [AURA_STR_SETTINGS_EQ]          = "Ecualizador",
    [AURA_STR_SETTINGS_BRIGHTNESS]  = "Brillo",
    [AURA_STR_SETTINGS_LANGUAGE]    = "Idioma",
    [AURA_STR_SETTINGS_ABOUT]       = "Acerca de",
    [AURA_STR_SETTINGS_SHUFFLE]     = "Aleatorio",
    [AURA_STR_SETTINGS_REPEAT]      = "Repetir",

    [AURA_STR_REPEAT_OFF]           = "Desactivado",
    [AURA_STR_REPEAT_ALL]           = "Todo",
    [AURA_STR_REPEAT_ONE]           = "Uno",

    [AURA_STR_SETTINGS_BACKLIGHT]    = "Temporiz. luz",
    [AURA_STR_SETTINGS_SLEEPTIMER]   = "Temporiz. reposo",
    [AURA_STR_SETTINGS_VOLUME_LIMIT] = "Límite volumen",
    [AURA_STR_SETTINGS_CLICKER]      = "Sonido de clic",
    [AURA_STR_SETTINGS_MAINMENU]     = "Menú principal",
    [AURA_STR_SETTINGS_RESET]        = "Restablecer ajustes",

    [AURA_STR_TIMEOUT_OFF]          = "Desactivado",
    [AURA_STR_TIMEOUT_ALWAYS]       = "Siempre",

    [AURA_STR_MAINMENU_RESTORE]     = "Restaurar menú principal",

    [AURA_STR_RESET_CONFIRM_TITLE]  = "Restablecer ajustes",
    [AURA_STR_RESET_CONFIRM_BODY]   = "Esto vuelve todos los ajustes a su valor de fábrica.",
    [AURA_STR_RESET_DONE]           = "Ajustes restablecidos",

    [AURA_STR_THEME_LIGHT]        = "Claro",
    [AURA_STR_THEME_DARK]         = "Oscuro",

    [AURA_STR_ANIM_NONE]          = "Ninguna",
    [AURA_STR_ANIM_MINIMAL]       = "Mínimas",
    [AURA_STR_ANIM_ALL]           = "Todas",

    [AURA_STR_GFX_NONE]           = "Ninguno",
    [AURA_STR_GFX_MINIMAL]        = "Mínimos",
    [AURA_STR_GFX_ALL]            = "Todos",

    [AURA_STR_EQ_FLAT]            = "Plano",
    [AURA_STR_EQ_BASS_BOOST]      = "Realce de graves",
    [AURA_STR_EQ_VOCAL]           = "Voz",
    [AURA_STR_EQ_TREBLE_BOOST]    = "Realce de agudos",

    [AURA_STR_LANG_ES]            = "Español",
    [AURA_STR_LANG_EN]            = "Inglés",

    [AURA_STR_ABOUT_BUILT_ON]     = "Basado en Rockbox",
    [AURA_STR_ABOUT_MUSIC]        = "Música",
    [AURA_STR_ABOUT_VIDEOS]       = "Video",
    [AURA_STR_ABOUT_PHOTOS]       = "Fotos",
    [AURA_STR_ABOUT_PLAYLISTS]    = "Listas",
    [AURA_STR_ABOUT_NO_SYNC]      = "Aún no te has sincronizado con Aura Studio.",

    [AURA_STR_EMPTY_MUSIC]        = "Sin música todavía",
    [AURA_STR_EMPTY_VIDEOS]       = "Sin videos todavía",
    [AURA_STR_EMPTY_PHOTOS]       = "Sin fotos todavía",
    [AURA_STR_NOTHING_PLAYING]    = "Nada sonando",

    [AURA_STR_MUSIC_ARTISTS]      = "Artistas",
    [AURA_STR_MUSIC_ALBUMS]       = "Álbumes",
    [AURA_STR_MUSIC_SONGS]        = "Canciones",
    [AURA_STR_MUSIC_PLAYLISTS]    = "Listas",
    [AURA_STR_MUSIC_GENRES]       = "Géneros",
    [AURA_STR_DB_NOT_READY]       = "Preparando la biblioteca…",
    [AURA_STR_EMPTY_LIST]         = "Sin resultados",
    [AURA_STR_UNSUPPORTED_FORMAT] = "Formato no soportado",

    [AURA_STR_YES]                = "Sí",
    [AURA_STR_NO]                 = "No",

    [AURA_STR_ABOUT_STORAGE]      = "Almacenamiento",
    [AURA_STR_PLAYLIST_PICK]      = "Gira la rueda para elegir",

    [AURA_STR_SETTINGS_ACCENT]    = "Color de acento",
    [AURA_STR_ACCENT_PINK]        = "Rosa",
    [AURA_STR_ACCENT_RED]         = "Rojo",
    [AURA_STR_ACCENT_ORANGE]      = "Naranja",
    [AURA_STR_ACCENT_GREEN]       = "Verde",
    [AURA_STR_ACCENT_BLUE]        = "Azul",
    [AURA_STR_ACCENT_PURPLE]      = "Morado",
    [AURA_STR_SETTINGS_LEFT_PANEL_SHADOW] = "Mostrar sombras",
    [AURA_STR_MUSIC_COVERFLOW]    = "Cover Flow",
    [AURA_STR_UNKNOWN_ALBUM]      = "Álbum desconocido",
    [AURA_STR_UNKNOWN_ARTIST]     = "Artista desconocido",
    [AURA_STR_UNKNOWN_GENRE]      = "Género desconocido",
    [AURA_STR_UNKNOWN_TITLE]      = "Sin título",
    [AURA_STR_MUSIC_COMPOSERS]    = "Autores",
    [AURA_STR_MUSIC_COMPILATIONS] = "Recopilaciones",
    [AURA_STR_MUSIC_AUDIOBOOKS]   = "Audiolibros",
    [AURA_STR_MUSIC_SEARCH]       = "Búsqueda",
    [AURA_STR_ALL]                = "Todos",
    [AURA_STR_EXTRAS]             = "Extras",
    [AURA_STR_SHUFFLE_SONGS]      = "Canciones aleat.",
    [AURA_STR_EXTRAS_CLOCKS]      = "Reloj internacional",
    [AURA_STR_EXTRAS_CALENDAR]    = "Calendarios",
    [AURA_STR_EXTRAS_CONTACTS]    = "Agenda",
    [AURA_STR_EXTRAS_ALARMS]      = "Alarmas",
    [AURA_STR_EXTRAS_GAMES]       = "Juegos",
    [AURA_STR_EXTRAS_NOTES]       = "Notas",
    [AURA_STR_EXTRAS_SCREENLOCK]  = "Bloqueo pantalla",
    [AURA_STR_EXTRAS_STOPWATCH]   = "Cronómetro",
    [AURA_STR_EMPTY_GENERIC]      = "No hay nada aquí todavía",
    [AURA_STR_SETTINGS_SHOW_ICONS] = "Mostrar iconos",
};

static const char *const strings_en[AURA_STR_COUNT] = {
    [AURA_STR_MUSIC]              = "Music",
    [AURA_STR_VIDEOS]             = "Videos",
    [AURA_STR_PHOTOS]             = "Photos",
    [AURA_STR_NOWPLAYING]         = "Now Playing",
    [AURA_STR_SETTINGS]           = "Settings",

    [AURA_STR_SETTINGS_THEME]       = "Theme",
    [AURA_STR_SETTINGS_ANIMATIONS]  = "Animations",
    [AURA_STR_SETTINGS_GRAPHICS]    = "Graphics",
    [AURA_STR_SETTINGS_EQ]          = "Equalizer",
    [AURA_STR_SETTINGS_BRIGHTNESS]  = "Brightness",
    [AURA_STR_SETTINGS_LANGUAGE]    = "Language",
    [AURA_STR_SETTINGS_ABOUT]       = "About",
    [AURA_STR_SETTINGS_SHUFFLE]     = "Shuffle",
    [AURA_STR_SETTINGS_REPEAT]      = "Repeat",

    [AURA_STR_REPEAT_OFF]           = "Off",
    [AURA_STR_REPEAT_ALL]           = "All",
    [AURA_STR_REPEAT_ONE]           = "One",

    [AURA_STR_SETTINGS_BACKLIGHT]    = "Backlight timer",
    [AURA_STR_SETTINGS_SLEEPTIMER]   = "Sleep timer",
    [AURA_STR_SETTINGS_VOLUME_LIMIT] = "Volume limit",
    [AURA_STR_SETTINGS_CLICKER]      = "Clicker",
    [AURA_STR_SETTINGS_MAINMENU]     = "Main menu",
    [AURA_STR_SETTINGS_RESET]        = "Reset settings",

    [AURA_STR_TIMEOUT_OFF]          = "Off",
    [AURA_STR_TIMEOUT_ALWAYS]       = "Always",

    [AURA_STR_MAINMENU_RESTORE]     = "Restore main menu",

    [AURA_STR_RESET_CONFIRM_TITLE]  = "Reset settings",
    [AURA_STR_RESET_CONFIRM_BODY]   = "This resets every setting back to its default.",
    [AURA_STR_RESET_DONE]           = "Settings reset",

    [AURA_STR_THEME_LIGHT]        = "Light",
    [AURA_STR_THEME_DARK]         = "Dark",

    [AURA_STR_ANIM_NONE]          = "None",
    [AURA_STR_ANIM_MINIMAL]       = "Minimal",
    [AURA_STR_ANIM_ALL]           = "All",

    [AURA_STR_GFX_NONE]           = "None",
    [AURA_STR_GFX_MINIMAL]        = "Minimal",
    [AURA_STR_GFX_ALL]            = "All",

    [AURA_STR_EQ_FLAT]            = "Flat",
    [AURA_STR_EQ_BASS_BOOST]      = "Bass boost",
    [AURA_STR_EQ_VOCAL]           = "Vocal",
    [AURA_STR_EQ_TREBLE_BOOST]    = "Treble boost",

    [AURA_STR_LANG_ES]            = "Spanish",
    [AURA_STR_LANG_EN]            = "English",

    [AURA_STR_ABOUT_BUILT_ON]     = "Built on Rockbox",
    [AURA_STR_ABOUT_MUSIC]        = "Music",
    [AURA_STR_ABOUT_VIDEOS]       = "Video",
    [AURA_STR_ABOUT_PHOTOS]       = "Photos",
    [AURA_STR_ABOUT_PLAYLISTS]    = "Playlists",
    [AURA_STR_ABOUT_NO_SYNC]      = "You haven't synced with Aura Studio yet.",

    [AURA_STR_EMPTY_MUSIC]        = "No music yet",
    [AURA_STR_EMPTY_VIDEOS]       = "No videos yet",
    [AURA_STR_EMPTY_PHOTOS]       = "No photos yet",
    [AURA_STR_NOTHING_PLAYING]    = "Nothing playing",

    [AURA_STR_MUSIC_ARTISTS]      = "Artists",
    [AURA_STR_MUSIC_ALBUMS]       = "Albums",
    [AURA_STR_MUSIC_SONGS]        = "Songs",
    [AURA_STR_MUSIC_PLAYLISTS]    = "Playlists",
    [AURA_STR_MUSIC_GENRES]       = "Genres",
    [AURA_STR_DB_NOT_READY]       = "Preparing your library...",
    [AURA_STR_EMPTY_LIST]         = "No results",
    [AURA_STR_UNSUPPORTED_FORMAT] = "Unsupported format",

    [AURA_STR_YES]                = "Yes",
    [AURA_STR_NO]                 = "No",

    [AURA_STR_ABOUT_STORAGE]      = "Storage",
    [AURA_STR_PLAYLIST_PICK]      = "Turn the wheel to choose",

    [AURA_STR_SETTINGS_ACCENT]    = "Accent color",
    [AURA_STR_ACCENT_PINK]        = "Pink",
    [AURA_STR_ACCENT_RED]         = "Red",
    [AURA_STR_ACCENT_ORANGE]      = "Orange",
    [AURA_STR_ACCENT_GREEN]       = "Green",
    [AURA_STR_ACCENT_BLUE]        = "Blue",
    [AURA_STR_ACCENT_PURPLE]      = "Purple",
    [AURA_STR_SETTINGS_LEFT_PANEL_SHADOW] = "Show shadows",
    [AURA_STR_MUSIC_COVERFLOW]    = "Cover Flow",
    [AURA_STR_UNKNOWN_ALBUM]      = "Unknown album",
    [AURA_STR_UNKNOWN_ARTIST]     = "Unknown artist",
    [AURA_STR_UNKNOWN_GENRE]      = "Unknown genre",
    [AURA_STR_UNKNOWN_TITLE]      = "Untitled",
    [AURA_STR_MUSIC_COMPOSERS]    = "Composers",
    [AURA_STR_MUSIC_COMPILATIONS] = "Compilations",
    [AURA_STR_MUSIC_AUDIOBOOKS]   = "Audiobooks",
    [AURA_STR_MUSIC_SEARCH]       = "Search",
    [AURA_STR_ALL]                = "All",
    [AURA_STR_EXTRAS]             = "Extras",
    [AURA_STR_SHUFFLE_SONGS]      = "Shuffle Songs",
    [AURA_STR_EXTRAS_CLOCKS]      = "World Clock",
    [AURA_STR_EXTRAS_CALENDAR]    = "Calendars",
    [AURA_STR_EXTRAS_CONTACTS]    = "Contacts",
    [AURA_STR_EXTRAS_ALARMS]      = "Alarms",
    [AURA_STR_EXTRAS_GAMES]       = "Games",
    [AURA_STR_EXTRAS_NOTES]       = "Notes",
    [AURA_STR_EXTRAS_SCREENLOCK]  = "Screen Lock",
    [AURA_STR_EXTRAS_STOPWATCH]   = "Stopwatch",
    [AURA_STR_EMPTY_GENERIC]      = "Nothing here yet",
    [AURA_STR_SETTINGS_SHOW_ICONS] = "Show icons",
};

const char *aura_str(aura_str_id_t id)
{
    if (id < 0 || id >= AURA_STR_COUNT)
        return "";

    const char *s = (aura_settings.language == AURA_LANG_EN)
                        ? strings_en[id]
                        : strings_es[id];
    return s ? s : "";
}
