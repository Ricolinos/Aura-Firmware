/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Ricardo Gómez
 *
 * Aura UI -- capa de interfaz sobre este fork de Rockbox (ver
 * MODIFICATIONS.md, DECISIONS.md D-001/D-002 en la raíz del repositorio).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "button.h"
#include "audio.h"
#include "lcd.h"
#include "gui/viewport.h"
#include "backlight.h"
#include "settings.h"
#include "version.h"
#include "sound.h"
#include "powermgmt.h"
#include "string-extra.h"
#include "misc.h"
#include "mv.h"
#include "storage.h" /* D-284: storage_get_identify() para la identidad de la unidad */
#include "fs_defines.h"
#include "rbpaths.h"
#include "recorder/bmp.h"
#include "file.h"
#include "dir.h"
#include "dircache.h"

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
#include "aura_albumart.h"
#include "aura_art.h"
#include "aura_coverdrift.h"
#include "aura_scroll_indicator.h"
#include "aura_flow.h"
#include "rtc.h"
#include "aura_stopwatch.h"
#include "aura_search.h"
#include "aura_worldclock.h"
#include "aura_calendar.h"
#include "aura_screenlock.h"
#include "aura_alarms.h"
#include "plugin.h"
#include "aura_video.h"
#include "aura_manifest.h"
#include "aura_main.h"
#include "aura_wheel.h"
#include "aura_menu_list.h"
#include "aura_status_bar_v2.h"
#include "aura_selection_summary.h"
#include "aura_category.h"
#include "aura_style.h"
#include "aura_color.h"

/* Cota de los buffers locales de items de menu. El arbol del original
 * (2026-08-13) llevo Ajustes a 18 filas: con la cota vieja de 16, los
 * dos `aura_menu_item_v2_t items[MAX_MENU_ENTRIES]` de draw_nav_list()
 * y draw_choice_list() se desbordaban en la pila (crash real al abrir
 * Ajustes). Ademas de subirla, get_nav_table() y get_choice_table()
 * acotan lo que devuelven, para que agregar filas nunca vuelva a poder
 * pisar la pila. */
#define MAX_MENU_ENTRIES 32

typedef struct {
    aura_str_id_t label_id;
    const char *icon_name;
    aura_screen_id_t target;
} nav_entry_t;

/* Orden del menu de inicio del firmware original (2026-08-13):
 * Musica, Videos, Fotos, Extras, Ajustes, Canciones aleat. Podcasts se
 * omite hasta que exista soporte real. "Ahora suena" no es una entrada
 * del original: aparece SOLO cuando hay reproduccion activa (y el
 * ajuste de Menu principal la sigue pudiendo ocultar). */
static const nav_entry_t root_entries_all[] = {
    { AURA_STR_MUSIC,         "music",    AURA_SCREEN_MUSIC },
    { AURA_STR_VIDEOS,        "video",    AURA_SCREEN_VIDEOS },
    { AURA_STR_PHOTOS,        "image",    AURA_SCREEN_PHOTOS },
    { AURA_STR_EXTRAS,        "extras",   AURA_SCREEN_EXTRAS },
    { AURA_STR_SETTINGS,      "settings", AURA_SCREEN_SETTINGS },
    { AURA_STR_NOWPLAYING,    "play",     AURA_SCREEN_NOWPLAYING },
    { AURA_STR_SHUFFLE_SONGS, "shuffle",  AURA_SCREEN_SHUFFLE_SONGS },
};
/* Musica y Ajustes son fijos; Videos/Fotos/Ahora suena son opcionales
 * (Menu principal configurable, L14, Fase 18) -- se filtran en tiempo
 * de dibujo/manejo de boton, no se recorta la tabla fuente. */
static nav_entry_t root_entries[sizeof(root_entries_all) / sizeof(root_entries_all[0])];
static int root_entries_count;


/* Orden de "Cover Flow" primero: unico orden que da un documento fuente
 * (componentes/left-panel.md, ejemplo de LeftPanel persistente) --
 * "Menu principal -> Musica -> (submenu con Cover Flow, Genius, Listas
 * de reproduccion, Artista, Albumes...)". "Genius" no existe como
 * funcion real en Aura, se omite -- el resto de items conserva el orden
 * ya existente de este arreglo. Icono "square-on-square" (cuadrados
 * superpuestos): eleccion provisional, no hay un icono dedicado de
 * "pila de caratulas" todavia producido -- mismo tipo de pendiente de
 * produccion de assets que selection-summary.md ya documenta para otros
 * iconos 1:1. */
/* Orden del submenu Musica del firmware original (2026-08-13). Genius
 * se omite (no existe la funcion y el dueno pidio no simular la
 * pantalla). Audiolibros es una fila presente e inerte, como en el
 * original cuando no hay material del tipo. */
static const nav_entry_t music_entries[] = {
    { AURA_STR_MUSIC_COVERFLOW,    "square-on-square", AURA_SCREEN_MUSIC_COVERFLOW },
    { AURA_STR_MUSIC_PLAYLISTS,    "playlist",   AURA_SCREEN_MUSIC_PLAYLISTS },
    { AURA_STR_MUSIC_ARTISTS,      "artist",     AURA_SCREEN_MUSIC_ARTISTS },
    { AURA_STR_MUSIC_ALBUMS,       "album",      AURA_SCREEN_MUSIC_ALBUMS },
    { AURA_STR_MUSIC_COMPILATIONS, "compilation", AURA_SCREEN_MUSIC_COMPILATIONS },
    { AURA_STR_MUSIC_SONGS,        "song",       AURA_SCREEN_MUSIC_SONGS },
    { AURA_STR_MUSIC_GENRES,       "genre",      AURA_SCREEN_MUSIC_GENRES },
    { AURA_STR_MUSIC_COMPOSERS,    "composer",   AURA_SCREEN_MUSIC_COMPOSERS },
    { AURA_STR_MUSIC_AUDIOBOOKS,   "audiobook",  AURA_SCREEN_MUSIC_AUDIOBOOKS },
    { AURA_STR_MUSIC_SEARCH,       "search",     AURA_SCREEN_MUSIC_SEARCH },
};

/* Atajos de Musica que pueden vivir tambien en el menu de inicio: cada
 * uno con su bit en aura_settings.root_shortcuts. */
static const aura_screen_id_t ROOT_SHORTCUTS[] = {
    AURA_SCREEN_MUSIC_COVERFLOW, AURA_SCREEN_MUSIC_PLAYLISTS,
    AURA_SCREEN_MUSIC_ARTISTS,   AURA_SCREEN_MUSIC_ALBUMS,
    AURA_SCREEN_MUSIC_SONGS,     AURA_SCREEN_MUSIC_GENRES,
};
#define ROOT_SHORTCUT_N ((int)(sizeof(ROOT_SHORTCUTS) / sizeof(ROOT_SHORTCUTS[0])))

int aura_screens_root_shortcut_bit(aura_screen_id_t target)
{
    int i;
    for (i = 0; i < ROOT_SHORTCUT_N; i++)
        if (ROOT_SHORTCUTS[i] == target)
            return i;
    return -1;
}

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
        /* Como el original: solo con reproduccion en curso (ademas del
         * ajuste de Menu principal, que puede ocultarla igual). */
        if (e->target == AURA_SCREEN_NOWPLAYING
            && (!aura_settings.show_nowplaying || !aura_nowplaying_active()))
            continue;
        root_entries[n++] = *e;

        /* Atajos de Musica marcados en el Menu principal: van JUSTO
         * despues de su padre, como en el original. */
        if (e->target == AURA_SCREEN_MUSIC)
        {
            int k;
            for (k = 0; k < ROOT_SHORTCUT_N && n < (int)(sizeof(root_entries)
                                                    / sizeof(root_entries[0])); k++)
            {
                const nav_entry_t *m;
                int j2, mcount;

                if (!(aura_settings.root_shortcuts & (1u << k)))
                    continue;
                mcount = (int)(sizeof(music_entries) / sizeof(music_entries[0]));
                m = music_entries;
                for (j2 = 0; j2 < mcount; j2++)
                    if (m[j2].target == ROOT_SHORTCUTS[k])
                    {
                        root_entries[n++] = m[j2];
                        break;
                    }
            }
        }
    }
    root_entries_count = n;
}


/* Iconos elegidos y verificados contra el catalogo real de SF Symbols
 * (D-075) -- ninguno se repite dentro de la lista (doc SS4, "hermanos se
 * distinguen"). */
static const nav_entry_t settings_entries[] = {
    /* Orden del firmware original con los ajustes propios de Aura
     * intercalados por SECCION (encargo 2026-08-13): informacion,
     * reproduccion, apariencia (los de Aura, juntos), pantalla,
     * sonido, sistema. Sin separadores visibles -- el orden ES la
     * agrupacion, como en el original. */
    { AURA_STR_SETTINGS_ABOUT,      "info",              AURA_SCREEN_SETTINGS_ABOUT },
    /* -- reproduccion -- */
    { AURA_STR_SETTINGS_SHUFFLE,    "shuffle",           AURA_SCREEN_SETTINGS_SHUFFLE },
    { AURA_STR_SETTINGS_REPEAT,     "repeat",            AURA_SCREEN_SETTINGS_REPEAT },
    { AURA_STR_SETTINGS_MAINMENU,   "menu-list",         AURA_SCREEN_SETTINGS_MAINMENU },
    /* -- apariencia (propios de Aura) -- */
    { AURA_STR_SETTINGS_THEME,      "theme",             AURA_SCREEN_SETTINGS_THEME },
    /* D-289: "Estilo" (fuentes+iconos+paleta instalables) justo
     * despues de "Tema" (claro/oscuro) -- ambas son "apariencia", sin
     * relacion entre si. Icono "sync" reusado (D-286/D-004: reuso
     * documentado en vez de un SVG nuevo cuando ninguno de los 89
     * existentes encaja mejor) -- "theme"/"paintpalette"/"graphics"/
     * "square-on-square"/"sliders-horizontal" ya son de las 5 filas
     * hermanas de este mismo grupo. */
    { AURA_STR_SETTINGS_STYLE,      "sync",              AURA_SCREEN_SETTINGS_STYLE },
    { AURA_STR_SETTINGS_ACCENT,     "paintpalette",      AURA_SCREEN_SETTINGS_ACCENT },
    { AURA_STR_SETTINGS_ANIMATIONS, "motion",            AURA_SCREEN_SETTINGS_ANIMATIONS },
    { AURA_STR_SETTINGS_GRAPHICS,   "graphics",          AURA_SCREEN_SETTINGS_GRAPHICS },
    { AURA_STR_SETTINGS_LEFT_PANEL_SHADOW, "square-on-square", AURA_SCREEN_SETTINGS_LEFT_PANEL_SHADOW },
    { AURA_STR_SETTINGS_SHOW_ICONS, "sliders-horizontal", AURA_SCREEN_SETTINGS_SHOW_ICONS },
    /* -- pantalla -- */
    { AURA_STR_SETTINGS_BRIGHTNESS, "sun",               AURA_SCREEN_SETTINGS_BRIGHTNESS },
    { AURA_STR_SETTINGS_BACKLIGHT,  "backlight",         AURA_SCREEN_SETTINGS_BACKLIGHT },
    /* -- sonido -- */
    { AURA_STR_SETTINGS_EQ,         "equalizer",         AURA_SCREEN_SETTINGS_EQ },
    { AURA_STR_SETTINGS_VOLUME_LIMIT, "volume-limit",    AURA_SCREEN_SETTINGS_VOLUME_LIMIT },
    { AURA_STR_SETTINGS_VOLUME_NORM, "volume-2",         AURA_SCREEN_SETTINGS_VOLUME_NORM },
    { AURA_STR_SETTINGS_AUDIOBOOKS, "audiobook",         AURA_SCREEN_SETTINGS_AUDIOBOOKS },
    { AURA_STR_SETTINGS_CLICKER,    "tap",               AURA_SCREEN_SETTINGS_CLICKER },
    /* -- sistema -- */
    { AURA_STR_SETTINGS_SLEEPTIMER, "sleep",             AURA_SCREEN_SETTINGS_SLEEPTIMER },
    /* Apagado del iPod (Task A, encargo del dueno): antes esto "solo
     * pasaba solo", sin ninguna pantalla que lo explicara ni permitiera
     * cambiarlo -- envuelve global_settings.poweroff (nucleo de
     * Rockbox, timeout de apagado por inactividad). */
    { AURA_STR_SETTINGS_POWEROFF,   "poweroff",          AURA_SCREEN_SETTINGS_POWEROFF },
    /* Bloqueo de pantalla (Task B, encargo del dueno): reubicado de
     * Extras -- es un ajuste GLOBAL (se arma aca, se activa solo con el
     * aparato apagado/prendido), no una utilidad de Extras. */
    { AURA_STR_SETTINGS_SCREENLOCK, "lock",              AURA_SCREEN_SETTINGS_SCREENLOCK },
    { AURA_STR_SETTINGS_DATETIME,   "calendar",          AURA_SCREEN_SETTINGS_DATETIME },
    { AURA_STR_SETTINGS_SORT_BY,    "sort",              AURA_SCREEN_SETTINGS_SORT_BY },
    { AURA_STR_SETTINGS_LANGUAGE,   "globe",             AURA_SCREEN_SETTINGS_LANGUAGE },
    { AURA_STR_SETTINGS_COPYRIGHT,  "legal",             AURA_SCREEN_SETTINGS_COPYRIGHT },
    { AURA_STR_SETTINGS_RESET,      "reset",             AURA_SCREEN_SETTINGS_RESET },
};

/* Extras del firmware original (2026-08-13), en su orden. */
/* Juegos (2026-08-13): los originales del iPod son inviables (sus
 * ejecutables llevan DRM FairPlay con llave por dispositivo y corren
 * sobre RetailOS, indocumentada -- ver sistema/03-arbol-de-menus.md).
 * Klondike SI existe: es exactamente el plugin `solitaire` de Rockbox.
 * iQuiz y Vortex no tienen equivalente y van inertes hasta que se
 * construyan nativos. La lista se arma en draw_games(), no aca: su
 * fila 0 LANZA un plugin en vez de navegar a una pantalla. */
/* Fecha y hora del original (2026-08-13). Los editores de Fecha y Hora
 * quedan pendientes; lo que si existe es la Zona horaria (que fija el
 * huso local que ya consume el Reloj internacional) y los dos
 * booleanos, cableados a ajustes REALES: formato de 12/24 h del nucleo
 * y visibilidad del ClockIndicator de la StatusBar. */
static const nav_entry_t datetime_entries[] = {
    { AURA_STR_SETTINGS_DATE,        "calendar", AURA_SCREEN_SETTINGS_DATE_EDIT },
    { AURA_STR_SETTINGS_TIME,        "clock",    AURA_SCREEN_SETTINGS_TIME_EDIT },
    { AURA_STR_SETTINGS_TIMEZONE,    "clock", AURA_SCREEN_SETTINGS_TIMEZONE },
    { AURA_STR_SETTINGS_CLOCK24,     NULL,    AURA_SCREEN_SETTINGS_CLOCK24 },
    { AURA_STR_SETTINGS_CLOCK_TITLE, NULL,    AURA_SCREEN_SETTINGS_CLOCK_TITLE },
};

static const nav_entry_t extras_entries[] = {
    { AURA_STR_EXTRAS_CLOCKS,     "clock",      AURA_SCREEN_EXTRAS_CLOCKS },
    { AURA_STR_EXTRAS_CALENDAR,   "calendar",   AURA_SCREEN_EXTRAS_CALENDAR },
    { AURA_STR_EXTRAS_CONTACTS,   "contacts",   AURA_SCREEN_EXTRAS_CONTACTS },
    { AURA_STR_EXTRAS_ALARMS,     "alarm",      AURA_SCREEN_EXTRAS_ALARMS },
    { AURA_STR_EXTRAS_GAMES,      "games",      AURA_SCREEN_EXTRAS_GAMES },
    { AURA_STR_EXTRAS_NOTES,      "notes",      AURA_SCREEN_EXTRAS_NOTES },
    { AURA_STR_EXTRAS_STOPWATCH,  "stopwatch",  AURA_SCREEN_EXTRAS_STOPWATCH },
};

static int clamp_menu_count(int n)
{
    return (n > MAX_MENU_ENTRIES) ? MAX_MENU_ENTRIES : n;
}

/* Videos y Fotos del original (2026-08-13). Las filas que no tienen
 * contenido propio en Aura van INERTES (el reproductor de video real
 * vive en "Todos los videos", que si funciona). */
static const nav_entry_t videos_entries[] = {
    { AURA_STR_VIDEOS_ALL,     "video",  AURA_SCREEN_VIDEOS_ALL },
    { AURA_STR_VIDEOS_MOVIES,  "movie",  AURA_SCREEN_VIDEOS_MOVIES },
    { AURA_STR_VIDEOS_TVSHOWS, "tv",     AURA_SCREEN_VIDEOS_TVSHOWS },
    { AURA_STR_VIDEOS_CLIPS,   "clip",   AURA_SCREEN_VIDEOS_CLIPS },
};

static const nav_entry_t photos_entries[] = {
    { AURA_STR_PHOTOS_ALL, "image", AURA_SCREEN_PHOTOS_ALL },
};

static int get_nav_table(aura_screen_id_t screen, const nav_entry_t **out)
{
    switch (screen)
    {
    case AURA_SCREEN_VIDEOS:
        *out = videos_entries;
        return clamp_menu_count((int)(sizeof(videos_entries) / sizeof(videos_entries[0])));
    case AURA_SCREEN_PHOTOS:
        *out = photos_entries;
        return clamp_menu_count((int)(sizeof(photos_entries) / sizeof(photos_entries[0])));
    case AURA_SCREEN_SETTINGS_DATETIME:
        *out = datetime_entries;
        return clamp_menu_count((int)(sizeof(datetime_entries) / sizeof(datetime_entries[0])));
    case AURA_SCREEN_EXTRAS:
        *out = extras_entries;
        return clamp_menu_count((int)(sizeof(extras_entries) / sizeof(extras_entries[0])));
    case AURA_SCREEN_ROOT:
        rebuild_root_entries();
        *out = root_entries;
        return clamp_menu_count(root_entries_count);
    case AURA_SCREEN_MUSIC:
        *out = music_entries;
        return clamp_menu_count(sizeof(music_entries) / sizeof(music_entries[0]));
    case AURA_SCREEN_SETTINGS:
        *out = settings_entries;
        return clamp_menu_count(sizeof(settings_entries) / sizeof(settings_entries[0]));
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
    case AURA_SCREEN_MUSIC_COMPOSERS:     return AURA_STR_MUSIC_COMPOSERS;
    case AURA_SCREEN_MUSIC_COMPILATIONS:  return AURA_STR_MUSIC_COMPILATIONS;
    case AURA_SCREEN_MUSIC_AUDIOBOOKS:    return AURA_STR_MUSIC_AUDIOBOOKS;
    case AURA_SCREEN_MUSIC_SEARCH:
    case AURA_SCREEN_MUSIC_SEARCH_RESULTS: return AURA_STR_MUSIC_SEARCH;
    case AURA_SCREEN_EXTRAS:              return AURA_STR_EXTRAS;
    case AURA_SCREEN_EXTRAS_CLOCKS:
    case AURA_SCREEN_EXTRAS_CLOCK_REGIONS:
    case AURA_SCREEN_EXTRAS_CLOCK_CITIES: return AURA_STR_EXTRAS_CLOCKS;
    case AURA_SCREEN_EXTRAS_CALENDAR:
    case AURA_SCREEN_EXTRAS_CALENDAR_DAY: return AURA_STR_EXTRAS_CALENDAR;
    case AURA_SCREEN_EXTRAS_CONTACTS:     return AURA_STR_EXTRAS_CONTACTS;
    case AURA_SCREEN_EXTRAS_ALARMS:
    case AURA_SCREEN_EXTRAS_ALARM_EDIT:
    case AURA_SCREEN_EXTRAS_ALARM_TIME:
    case AURA_SCREEN_EXTRAS_ALARM_CHOICE: return AURA_STR_EXTRAS_ALARMS;
    case AURA_SCREEN_EXTRAS_GAMES:        return AURA_STR_EXTRAS_GAMES;
    case AURA_SCREEN_EXTRAS_NOTES:        return AURA_STR_EXTRAS_NOTES;
    case AURA_SCREEN_EXTRAS_STOPWATCH:    return AURA_STR_EXTRAS_STOPWATCH;
    case AURA_SCREEN_VIDEOS_MOVIES:       return AURA_STR_VIDEOS_MOVIES;
    case AURA_SCREEN_VIDEOS_TVSHOWS:      return AURA_STR_VIDEOS_TVSHOWS;
    case AURA_SCREEN_VIDEOS_CLIPS:        return AURA_STR_VIDEOS_CLIPS;
    case AURA_SCREEN_VIDEOS_ALL:          return AURA_STR_VIDEOS;
    case AURA_SCREEN_PHOTOS_ALL:          return AURA_STR_PHOTOS;
    case AURA_SCREEN_SETTINGS_COPYRIGHT:  return AURA_STR_SETTINGS_COPYRIGHT;
    case AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER: return AURA_STR_MUSIC_ALBUMS;
    case AURA_SCREEN_MUSIC_SONGS_BY_ARTIST:
    case AURA_SCREEN_MUSIC_SONGS_BY_COMPOSER:  return AURA_STR_MUSIC_SONGS;
    case AURA_SCREEN_MUSIC_ARTISTS_BY_GENRE:   return AURA_STR_MUSIC_ARTISTS;
    case AURA_SCREEN_MUSIC_ALBUMS:
    case AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST: return AURA_STR_MUSIC_ALBUMS;
    case AURA_SCREEN_MUSIC_SONGS:
    case AURA_SCREEN_MUSIC_SONGS_BY_ALBUM:
    case AURA_SCREEN_MUSIC_SONGS_BY_GENRE:   return AURA_STR_MUSIC_SONGS;
    case AURA_SCREEN_MUSIC_GENRES:        return AURA_STR_MUSIC_GENRES;
    case AURA_SCREEN_MUSIC_PLAYLISTS:     return AURA_STR_MUSIC_PLAYLISTS;
    case AURA_SCREEN_MUSIC_COVERFLOW:     return AURA_STR_MUSIC_COVERFLOW;
    case AURA_SCREEN_VIDEOS:              return AURA_STR_VIDEOS;
    case AURA_SCREEN_PHOTOS:              return AURA_STR_PHOTOS;
    case AURA_SCREEN_NOWPLAYING:          return AURA_STR_NOWPLAYING;
    case AURA_SCREEN_SETTINGS:            return AURA_STR_SETTINGS;
    case AURA_SCREEN_SETTINGS_THEME:      return AURA_STR_SETTINGS_THEME;
    case AURA_SCREEN_SETTINGS_STYLE:      return AURA_STR_SETTINGS_STYLE;
    case AURA_SCREEN_SETTINGS_ANIMATIONS: return AURA_STR_SETTINGS_ANIMATIONS;
    case AURA_SCREEN_SETTINGS_GRAPHICS:   return AURA_STR_SETTINGS_GRAPHICS;
    case AURA_SCREEN_SETTINGS_EQ:         return AURA_STR_SETTINGS_EQ;
    case AURA_SCREEN_SETTINGS_BRIGHTNESS: return AURA_STR_SETTINGS_BRIGHTNESS;
    case AURA_SCREEN_SETTINGS_LANGUAGE:   return AURA_STR_SETTINGS_LANGUAGE;
    case AURA_SCREEN_SETTINGS_SORT_BY:    return AURA_STR_SETTINGS_SORT_BY;
    case AURA_SCREEN_SETTINGS_DATETIME:   return AURA_STR_SETTINGS_DATETIME;
    case AURA_SCREEN_SETTINGS_DATE_EDIT:  return AURA_STR_SETTINGS_DATE;
    case AURA_SCREEN_SETTINGS_TIME_EDIT:  return AURA_STR_SETTINGS_TIME;
    case AURA_SCREEN_SETTINGS_TIMEZONE:   return AURA_STR_SETTINGS_TIMEZONE;
    case AURA_SCREEN_SETTINGS_AUDIOBOOKS: return AURA_STR_SETTINGS_AUDIOBOOKS;
    case AURA_SCREEN_SETTINGS_ACCENT:     return AURA_STR_SETTINGS_ACCENT;
    case AURA_SCREEN_SETTINGS_ABOUT:      return AURA_STR_SETTINGS_ABOUT;
    case AURA_SCREEN_SETTINGS_SHUFFLE:    return AURA_STR_SETTINGS_SHUFFLE;
    case AURA_SCREEN_SETTINGS_REPEAT:     return AURA_STR_SETTINGS_REPEAT;
    case AURA_SCREEN_SETTINGS_BACKLIGHT:    return AURA_STR_SETTINGS_BACKLIGHT;
    case AURA_SCREEN_SETTINGS_SLEEPTIMER:   return AURA_STR_SETTINGS_SLEEPTIMER;
    case AURA_SCREEN_SETTINGS_POWEROFF:     return AURA_STR_SETTINGS_POWEROFF;
    case AURA_SCREEN_SETTINGS_SCREENLOCK:   return AURA_STR_SETTINGS_SCREENLOCK;
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
/* Los 23 presets del firmware original, en su orden. */
static const aura_str_id_t eq_choice_labels[] = {
    AURA_STR_EQ_OFF,
    AURA_STR_EQ_ACOUSTIC,
    AURA_STR_EQ_BASS_BOOST,
    AURA_STR_EQ_BASS_RED,
    AURA_STR_EQ_CLASSICAL,
    AURA_STR_EQ_DANCE,
    AURA_STR_EQ_DEEP,
    AURA_STR_EQ_ELECTRONIC,
    AURA_STR_EQ_FLAT,
    AURA_STR_EQ_HIPHOP,
    AURA_STR_EQ_JAZZ,
    AURA_STR_EQ_LATIN,
    AURA_STR_EQ_LOUDNESS,
    AURA_STR_EQ_LOUNGE,
    AURA_STR_EQ_PIANO,
    AURA_STR_EQ_POP,
    AURA_STR_EQ_RNB,
    AURA_STR_EQ_ROCK,
    AURA_STR_EQ_SMALLSPK,
    AURA_STR_EQ_SPOKEN,
    AURA_STR_EQ_TREBLE_BOOST,
    AURA_STR_EQ_TREBLE_RED,
    AURA_STR_EQ_VOCAL_BOOST,
};
/* Lista completa del original (2026-08-13, P15b). Solo Español e
 * Ingles estan traducidos hoy: el resto se muestra INERTE (fila
 * atenuada que no se puede elegir) hasta que exista su traduccion --
 * el usuario ve el catalogo completo sin que el firmware finja
 * soportar un idioma que no tiene. */
static const aura_str_id_t sort_choice_labels[] = {
    AURA_STR_SORT_FIRSTNAME, AURA_STR_SORT_LASTNAME,
};

static const aura_str_id_t language_choice_labels[] = {
    AURA_STR_LANG_ES, AURA_STR_LANG_EN,
    AURA_STR_LANG_DA, AURA_STR_LANG_DE, AURA_STR_LANG_FR, AURA_STR_LANG_IT,
    AURA_STR_LANG_NL, AURA_STR_LANG_NO, AURA_STR_LANG_PT, AURA_STR_LANG_FI,
    AURA_STR_LANG_SV, AURA_STR_LANG_JA, AURA_STR_LANG_ZH, AURA_STR_LANG_KO,
    AURA_STR_LANG_RU,
};
/* Cuantos de esa lista estan realmente disponibles (los primeros N). */
#define LANGUAGE_AVAILABLE_N 2
/* Repetir (D-021: solo Desactivado/Todo/Uno -- REPEAT_SHUFFLE y
 * REPEAT_AB quedan fuera del modelo simplificado de Aura, el aleatorio
 * ya es su propio booleano independiente, D-014/Fase 17) indices 0/1/2,
 * coinciden 1:1 con REPEAT_OFF/REPEAT_ALL/REPEAT_ONE de Rockbox (apps/
 * settings.h) -- reusado por el ciclo en linea de aura_screens_handle_button()
 * (D-264, ya no es una lista de eleccion navegable, ver is_choice_screen()
 * arriba). */

/* Apagado del iPod (Task A, encargo del dueno): mismo patron que
 * REPEAT arriba -- ajuste REAL de Rockbox (global_settings.poweroff,
 * apps/settings_list.c: INT_SETTING en minutos, 0-60, 0 = "Desactivado"
 * via formatter_time_unit_0_is_off), envuelto con 4 frases fijas en vez
 * de las etiquetas numericas genericas que ya usan Temporiz. luz/reposo
 * (draw_backlight/draw_sleeptimer) porque el dueno pidio texto exacto
 * ("Desactivado, despues de 10min, despues de 20min, despues de 1hra"),
 * no "10 min" a secas. AURA_STR_TIMEOUT_OFF ya existe (lo usan Temporiz.
 * luz/reposo) -- se reutiliza en vez de duplicar "Desactivado". Mismo
 * orden que poweroff_choice_minutes[] (get_choice_current/apply_choice,
 * mas abajo). */
static const aura_str_id_t poweroff_choice_labels[] = {
    AURA_STR_TIMEOUT_OFF, AURA_STR_POWEROFF_10MIN,
    AURA_STR_POWEROFF_20MIN, AURA_STR_POWEROFF_1HOUR,
};
static const int poweroff_choice_minutes[] = { 0, 10, 20, 60 };

/* Acento configurable (PLAN.md T0.3, fundamentos/01-color.md): la doc
 * confirma "configurable por el usuario" pero no dice COMO -- lista
 * provisional de 6 presets con nombre (// TODO(pendiente-doc): un
 * selector de swatches de color seria mas fiel al espiritu del sistema
 * nuevo que una lista de texto, pero draw_choice_list() no tiene forma
 * de pintar un color arbitrario por fila hoy; se resuelve con la misma
 * mecanica de eleccion por texto que Tema/Idioma mientras tanto).
 * Indice 0 = default de fabrica (AURA_DS_COLOR_ACCENT_DEFAULT_RGB24). */
static const aura_str_id_t accent_choice_labels[] = {
    AURA_STR_ACCENT_PINK, AURA_STR_ACCENT_RED, AURA_STR_ACCENT_ORANGE,
    AURA_STR_ACCENT_GREEN, AURA_STR_ACCENT_BLUE, AURA_STR_ACCENT_PURPLE,
};
/* T4.1: movido a tokens.json (aura_ds.color.accent_presets_hex) --
 * ningun color RGB hardcodeado en C (regla del proyecto). Mismo orden
 * que accent_choice_labels arriba. */
static const unsigned accent_choice_rgb24[] = AURA_DS_COLOR_ACCENT_PRESETS_HEX_RGB24_VALUES;

/* AURA_SCREEN_SETTINGS_REPEAT (D-264): dejo de ser una pantalla de
 * eleccion navegable -- ahora es una fila en linea (SELECT cicla el
 * valor, ver aura_screens_handle_button()), igual que Aleatorio/
 * Clicker. Retirada de esta funcion y de get_choice_table()/
 * get_choice_current()/apply_choice() abajo -- codigo confirmado sin
 * otro consumidor real (grep completo contra el arbol, D-264) antes de
 * borrarlo, no dejado como ruta muerta. */
static int is_choice_screen(aura_screen_id_t screen)
{
    return screen == AURA_SCREEN_SETTINGS_THEME
        || screen == AURA_SCREEN_SETTINGS_ANIMATIONS
        || screen == AURA_SCREEN_SETTINGS_GRAPHICS
        || screen == AURA_SCREEN_SETTINGS_EQ
        || screen == AURA_SCREEN_SETTINGS_LANGUAGE
        || screen == AURA_SCREEN_SETTINGS_SORT_BY
        || screen == AURA_SCREEN_SETTINGS_ACCENT
        || screen == AURA_SCREEN_SETTINGS_POWEROFF;
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
    case AURA_SCREEN_SETTINGS_SORT_BY:
        *out = sort_choice_labels;
        return clamp_menu_count((int)(sizeof(sort_choice_labels) / sizeof(sort_choice_labels[0])));
    case AURA_SCREEN_SETTINGS_LANGUAGE:
        *out = language_choice_labels;
        return sizeof(language_choice_labels) / sizeof(language_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_ACCENT:
        *out = accent_choice_labels;
        return sizeof(accent_choice_labels) / sizeof(accent_choice_labels[0]);
    case AURA_SCREEN_SETTINGS_POWEROFF:
        *out = poweroff_choice_labels;
        return sizeof(poweroff_choice_labels) / sizeof(poweroff_choice_labels[0]);
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
    case AURA_SCREEN_SETTINGS_SORT_BY:  return aura_settings.sort_by_lastname;
    case AURA_SCREEN_SETTINGS_POWEROFF:
    {
        size_t i, n = sizeof(poweroff_choice_minutes) / sizeof(poweroff_choice_minutes[0]);
        for (i = 0; i < n; i++)
            if (poweroff_choice_minutes[i] == global_settings.poweroff)
                return (int)i;
        return 0;
    }
    case AURA_SCREEN_SETTINGS_ACCENT:
    {
        /* El acento vigente puede no coincidir con ningun preset (el
         * usuario podria haberlo llegado a fijar por otra via en el
         * futuro) -- en ese caso no hay fila "actual" real, se cae al
         * primer preset (Rosa) sin marcar un checkmark erroneo en otra
         * fila con un valor distinto. */
        size_t i, n = sizeof(accent_choice_rgb24) / sizeof(accent_choice_rgb24[0]);
        for (i = 0; i < n; i++)
            if (accent_choice_rgb24[i] == aura_settings.accent_rgb24)
                return (int)i;
        return 0;
    }
    default:                            return 0;
    }
}

static void apply_choice(aura_screen_id_t screen, int index)
{

    /* Apagado del iPod (Task A): igual que REPEAT arriba, un ajuste
     * REAL de Rockbox -- set_poweroff_timeout() (firmware/powermgmt.c)
     * es el mismo setter que usa la propia settings_list.c de Rockbox,
     * necesario para que el cambio surta efecto sin reiniciar (no basta
     * con escribir global_settings.poweroff a secas). */
    if (screen == AURA_SCREEN_SETTINGS_POWEROFF)
    {
        int n = (int)(sizeof(poweroff_choice_minutes) / sizeof(poweroff_choice_minutes[0]));
        if (index < 0 || index >= n)
            index = 0;
        global_settings.poweroff = poweroff_choice_minutes[index];
        set_poweroff_timeout(global_settings.poweroff);
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
    case AURA_SCREEN_SETTINGS_SORT_BY:
        aura_settings.sort_by_lastname = index;
        break;
    case AURA_SCREEN_SETTINGS_ACCENT:
        if (index >= 0 && (size_t)index < sizeof(accent_choice_rgb24) / sizeof(accent_choice_rgb24[0]))
            aura_settings.accent_rgb24 = accent_choice_rgb24[index];
        break;
    default:
        break;
    }
    aura_settings_save();
}

/* -- Aleatorio, Clicker y Sombra de panel: booleanos que viven inline en
 * la fila de Ajustes con un switch (D-075), no en pantalla propia --
 * Aleatorio/Clicker son ajustes reales de Rockbox (playlist_shuffle,
 * D-021; keyclick != 0, D-06x) que el doc de comportamiento ya describe
 * como `[OPCION]` simple ("sigue la regla de profundidad por defecto,
 * no tiene mecanica propia"); Sombra de panel es propio de Aura
 * (aura_settings.left_panel_shadow, T0.4, efectos/01-sombras.md: "el
 * usuario puede desactivarlo desde Ajustes"). `settings_row_toggle_value()`
 * lee el valor actual para dibujar el switch; `toggle_settings_row()`
 * lo invierte in situ. Generalizado (comentario viejo prometia esto al
 * aparecer una tercera fila): un `if` por target en vez de una tabla,
 * porque cada uno persiste distinto (settings_save() de Rockbox vs
 * aura_settings_save() propio). */
static int settings_row_toggle_value(aura_screen_id_t target)
{
    if (target == AURA_SCREEN_SETTINGS_SHUFFLE)
        return global_settings.playlist_shuffle;
    if (target == AURA_SCREEN_SETTINGS_CLICKER)
        return global_settings.keyclick != 0;
    if (target == AURA_SCREEN_SETTINGS_LEFT_PANEL_SHADOW)
        return aura_settings.left_panel_shadow;
    if (target == AURA_SCREEN_SETTINGS_SHOW_ICONS)
        return aura_settings.show_icons;
    /* "Ajuste volumen" del original = normalizacion de volumen; se
     * cablea al replaygain REAL del nucleo, no a un ajuste cosmetico. */
    if (target == AURA_SCREEN_SETTINGS_CLOCK24)
        return global_settings.timeformat == 0;
    if (target == AURA_SCREEN_SETTINGS_CLOCK_TITLE)
        return aura_settings.clock_visible;
    if (target == AURA_SCREEN_SETTINGS_VOLUME_NORM)
        return global_settings.replaygain_settings.noclip
               || global_settings.replaygain_settings.type != REPLAYGAIN_OFF;
    return -1;
}

static void toggle_settings_row(aura_screen_id_t target)
{
    if (target == AURA_SCREEN_SETTINGS_SHUFFLE)
    {
        global_settings.playlist_shuffle = !global_settings.playlist_shuffle;
        settings_save();
    }
    else if (target == AURA_SCREEN_SETTINGS_CLICKER)
    {
        global_settings.keyclick = global_settings.keyclick ? 0 : 2;
        settings_save();
    }
    else if (target == AURA_SCREEN_SETTINGS_LEFT_PANEL_SHADOW)
    {
        aura_settings.left_panel_shadow = !aura_settings.left_panel_shadow;
        aura_settings_save();
    }
    else if (target == AURA_SCREEN_SETTINGS_SHOW_ICONS)
    {
        aura_settings.show_icons = !aura_settings.show_icons;
        aura_settings_save();
    }
    else if (target == AURA_SCREEN_SETTINGS_CLOCK24)
    {
        global_settings.timeformat = global_settings.timeformat ? 0 : 1;
        settings_save();
    }
    else if (target == AURA_SCREEN_SETTINGS_CLOCK_TITLE)
    {
        aura_settings.clock_visible = !aura_settings.clock_visible;
        aura_settings_save();
    }
    else if (target == AURA_SCREEN_SETTINGS_VOLUME_NORM)
    {
        bool on = global_settings.replaygain_settings.type != REPLAYGAIN_OFF;
        global_settings.replaygain_settings.type = on ? REPLAYGAIN_OFF
                                                       : REPLAYGAIN_TRACK;
        settings_save();
    }
}

/* -- Dibujo -------------------------------------------------------------- */

/* Definida mas abajo (tabla completa de layouts) -- los constructores
 * de items v2 la consultan para decidir la flecha del Selector
 * (selector.md: "cuando lleva a un componente de pantalla completa"). */
static int screen_uses_split_layout(aura_screen_id_t screen);

/* -- CoverDrift en Musica (D-254) -----------------------------------
 *
 * Reemplaza aura_selection_summary_draw() SOLO para ciertas filas de
 * ciertas pantallas de la Ruta A (draw_menu_screen_v2(), esta seccion)
 * -- NUNCA en la Ruta B (listas de contenido, aura_widgets_draw_list()),
 * y NUNCA convierte una pantalla FULL en SPLIT (screen_uses_split_layout()
 * no se toca, ver DECISIONS.md D-253/D-254). Encargo textual del dueno
 * del producto, 2026-08-15: en el menu raiz, solo la fila Musica; dentro
 * del submenu de Musica, todas las filas EXCEPTO Audiolibros -- Video y
 * Fotos quedan fuera de esta pasada a proposito (el dueno los pidio
 * despues, uno a la vez, para validar comportamiento en un solo lugar
 * primero). */
static bool music_row_wants_coverdrift(aura_screen_id_t container, aura_screen_id_t target)
{
    if (container == AURA_SCREEN_ROOT)
        /* D-266 (encargo del dueno de producto): "Canciones aleat." y
         * "Ahora suena" tambien son filas de Musica en el menu raiz
         * (mismo criterio que ya usa aura_category_for_screen() -- las
         * tres resuelven a AURA_CATEGORY_MUSIC), asi que tambien
         * califican para CoverDrift en el panel derecho -- antes solo
         * Musica lo hacia. Etapa 2 del plan original de D-259/D-261,
         * pendiente desde entonces. */
        return target == AURA_SCREEN_MUSIC
            || target == AURA_SCREEN_SHUFFLE_SONGS
            || target == AURA_SCREEN_NOWPLAYING;

    if (container == AURA_SCREEN_MUSIC)
        return target == AURA_SCREEN_MUSIC_COVERFLOW
            || target == AURA_SCREEN_MUSIC_PLAYLISTS
            || target == AURA_SCREEN_MUSIC_ARTISTS
            || target == AURA_SCREEN_MUSIC_ALBUMS
            || target == AURA_SCREEN_MUSIC_COMPILATIONS
            || target == AURA_SCREEN_MUSIC_SONGS
            || target == AURA_SCREEN_MUSIC_GENRES
            || target == AURA_SCREEN_MUSIC_COMPOSERS
            || target == AURA_SCREEN_MUSIC_SEARCH;
            /* AURA_SCREEN_MUSIC_AUDIOBOOKS excluida a proposito -- fila
             * inerte, el dueno la nombro explicitamente como excepcion. */

    return false;
}

/* Pool GENERAL de todas las caratulas de album de la biblioteca (no el
 * album exacto de la fila resaltada -- D-242, esta misma sesion, ya
 * encontro que resolver eso no tiene API publica, el mapeo vive en
 * funciones static internas de tagcache.c). Mismo patron de cache por
 * generacion que ensure_music_cache() (arriba en este archivo); solo
 * conserva el seek de cada album, no el label. */
static int32_t s_drift_album_pool_seeks[AURA_MUSIC_MAX_ITEMS];
static int s_drift_album_pool_count = 0;
static int s_drift_album_pool_generation = -1;
static bool s_drift_album_pool_was_ready = false;

/* aura_music_db_ready() (aura_music.c) no es solo una consulta -- es lo
 * que DISPARA tagcache_rebuild() la primera vez (Aura no tiene pantalla
 * de "Base de datos > Inicializar", ver el comentario grande junto a su
 * definicion), no bloqueante e idempotente por sus propias banderas
 * static. Sin llamarla, la base de datos se queda vacia para siempre en
 * un primer arranque -- CoverDrift necesita dispararla igual que
 * draw_music_browse() ya lo hace, aunque el usuario nunca haya entrado
 * a Musica todavia (puede quedarse resaltando la fila Musica en el
 * menu raiz sin entrar). `s_drift_album_pool_was_ready` evita el mismo
 * bug al reves: aura_music_filter_generation() NO cambia cuando la base
 * pasa de no-lista a lista (solo cambia con filtros de artista/album/
 * genero), asi que el cache por generacion por si solo dejaria el pool
 * vacio para siempre si el primer intento cayo antes de que la base
 * estuviera lista -- se reintenta en cada cuadro hasta que
 * aura_music_db_ready() de verdad devuelva true por primera vez. */
static void ensure_drift_album_pool(void)
{
    static aura_music_item_t s_pool_scratch[AURA_MUSIC_MAX_ITEMS];
    int gen;
    int n, i;

    if (!aura_music_db_ready())
        return;

    gen = aura_music_filter_generation();
    if (s_drift_album_pool_was_ready && s_drift_album_pool_generation == gen)
        return;

    s_drift_album_pool_was_ready = true;
    s_drift_album_pool_generation = gen;
    n = aura_music_browse(AURA_SCREEN_MUSIC_ALBUMS, s_pool_scratch, AURA_MUSIC_MAX_ITEMS);

    s_drift_album_pool_count = 0;
    for (i = 0; i < n; i++)
    {
        if (s_pool_scratch[i].seek < 0) /* fila sintetica "Canciones", no es un album real */
            continue;
        s_drift_album_pool_seeks[s_drift_album_pool_count++] = s_pool_scratch[i].seek;
    }
}

/* Decodificacion bajo demanda de solo la imagen activa + la anterior
 * (nunca el pool completo -- con hasta AURA_MUSIC_MAX_ITEMS=300 albumes
 * y ~168KB por imagen al tamano de CoverDrift D-254, decodificarlas
 * todas de una vez es inviable). aura_albumart_load_for_album() (mismo
 * cache .pfraw en disco que ya usa Cover Flow/la lista de Albumes, pide
 * el tamano nuevo directo -- genera su propio archivo de cache aparte,
 * sin invalidar el de 48px/130px que usan otras pantallas) da el
 * bitmap TRANSPUESTO (columna-mayor, mismo formato que
 * draw_album_thumb() de mas abajo); se transpone a fila-mayor (lo que
 * espera aura_coverdrift_draw() via lcd_bitmap_part()) una sola vez por
 * cambio de indice activo, nunca por cuadro. */
static aura_coverdrift_image_t s_drift_album_images[AURA_MUSIC_MAX_ITEMS];
static int s_drift_album_buf_a_idx = -1;
static int s_drift_album_buf_b_idx = -1;
static struct bitmap s_drift_album_bmp_a;
static struct bitmap s_drift_album_bmp_b;
static fb_data s_drift_album_pixels_a[AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE
                                       * AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE];
static fb_data s_drift_album_pixels_b[AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE
                                       * AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE];

static bool decode_album_drift_tile(int pool_idx, fb_data *dst)
{
    enum { SZ = AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE };
    static unsigned char cover_buf[SZ * SZ * sizeof(fb_data)];
    static unsigned char refl_buf[SZ *
        (SZ * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT / 100 + 1)
        * sizeof(fb_data)];
    aura_albumart_t art;
    const fb_data *cover;
    int col, row;

    if (pool_idx < 0 || pool_idx >= s_drift_album_pool_count)
        return false;

    art.size = SZ;
    art.radius = A26_LAYOUT_CORNER_RADIUS_CARD;
    art.cover_data = cover_buf;
    art.reflection_data = refl_buf;

    if (!aura_albumart_load_for_album(s_drift_album_pool_seeks[pool_idx], &art))
        return false;

    cover = (const fb_data *)art.cover_data;
    for (col = 0; col < SZ; col++)
        for (row = 0; row < SZ; row++)
            dst[row * SZ + col] = cover[col * SZ + row];
    return true;
}

static void ensure_drift_albums_decoded(void)
{
    enum { SZ = AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE };
    int active = aura_coverdrift_active_index();
    int prev = aura_coverdrift_prev_index();

    if (active >= 0 && active < s_drift_album_pool_count && active != s_drift_album_buf_a_idx)
    {
        s_drift_album_buf_a_idx = active;
        s_drift_album_images[active].bmp = NULL;
        if (decode_album_drift_tile(active, s_drift_album_pixels_a))
        {
            s_drift_album_bmp_a.width = SZ;
            s_drift_album_bmp_a.height = SZ;
            s_drift_album_bmp_a.data = (char *)s_drift_album_pixels_a;
            s_drift_album_images[active].bmp = &s_drift_album_bmp_a;
        }
    }

    if (prev >= 0 && prev < s_drift_album_pool_count && prev != s_drift_album_buf_b_idx)
    {
        s_drift_album_buf_b_idx = prev;
        s_drift_album_images[prev].bmp = NULL;
        if (decode_album_drift_tile(prev, s_drift_album_pixels_b))
        {
            s_drift_album_bmp_b.width = SZ;
            s_drift_album_bmp_b.height = SZ;
            s_drift_album_bmp_b.data = (char *)s_drift_album_pixels_b;
            s_drift_album_images[prev].bmp = &s_drift_album_bmp_b;
        }
    }
}

/* -- Debounce/fundido general del panel derecho (D-262) -----------------
 *
 * Encargo textual del dueno del producto, 2026-08-15: "aplicar un delay
 * de 2seg al panel derecho respecto a lo que se elija en el panel
 * izquierdo... mientras nos desplacemos por el panel izquierdo, el
 * panel derecho no cambiara, manteniendo lo renderizado. sin embargo,
 * una vez dejemos de mover el scroll... despues de 2 seg. de estar en
 * la nueva opcion, el panel derecho se renderizara adecuadamente, si
 * cae a una opcion sin coverdrift, el coverdrift que este se
 * desvanecera, si cae a una opcion con otro tipo de coverdrift, igual
 * habra una transicion... lo mismo con fotos, o con el selection
 * summary". Generaliza y REEMPLAZA el temporizador de 3s especifico de
 * CoverDrift (coverdrift_armed_and_ready(), s_drift_arm_category/
 * _since, retirados) -- confirmado explicitamente por el dueno al
 * preguntarle la relacion entre ambos plazos ("El de 2s reemplaza al
 * de 3s -- CoverDrift ya no necesita su propia espera de 3s aparte, se
 * unifica en esta regla general mas simple"). Aplica a CUALQUIER
 * llamador de draw_menu_screen_v2() con panel activo, no solo a
 * Musica: el icono/descripcion normal del panel tambien se congela y
 * funde igual que CoverDrift, en vez de cambiar al instante como antes
 * (2026-08-12).
 *
 * Identidad de lo que se ve en el panel derecho en un cuadro dado.
 * `coverdrift`+`coverdrift_category` colapsan container_screen/
 * selected_target/panel_icon/panel_desc en dos campos para la
 * COMPARACION (panel_identity_equal() mas abajo): dos filas cualquiera
 * que ambas califiquen CoverDrift de la MISMA categoria son la MISMA
 * identidad visual (el fondo ambiental de esa categoria no cambia por
 * fila exacta -- misma nocion de "sesion por categoria" que ya uso
 * D-260), asi que moverse entre ellas NUNCA dispara un fundido
 * innecesario. Solo cuando se ENTRA o se SALE de CoverDrift, cuando
 * CAMBIA la categoria de CoverDrift (Musica/Video/Fotos -- ningun
 * llamador califica todavia mas de Musica, pero la comparacion tiene
 * que ser correcta desde ya para cuando D-263/D-264 conecten Fotos/
 * Video, sin necesitar tocar esta funcion otra vez), o cuando cambia
 * el icono/descripcion de una fila sin CoverDrift, cuenta como un
 * cambio real de identidad. */
/* D-264: contenido real por pantalla del panel derecho -- `panel_top`
 * (SF Pro Bold 16pt, D-263) y las dos variantes DINAMICAS (icono/pie),
 * usadas por Reloj (icono: reloj analogico) y Acerca de (pie: grafico
 * de almacenamiento) respectivamente. `panel_icon`/`panel_top`/
 * `panel_desc` se ignoran cuando el renderer correspondiente no es NULL
 * -- mismo contrato que aura_selection_summary_draw_dynamic() (ver ese
 * archivo). Todos comparados por PUNTERO en panel_identity_equal(),
 * mismo criterio que el resto de esta identidad -- los textos/renderers
 * de este archivo son siempre literales/funciones estables, nunca
 * generados dinamicamente por instancia. */
typedef struct
{
    const char *title;
    const char *panel_icon;
    const char *panel_top;
    const char *panel_desc;
    aura_selection_summary_icon_renderer_t icon_renderer;
    aura_selection_summary_bottom_renderer_t bottom_renderer;
    aura_screen_id_t container_screen;
    aura_screen_id_t selected_target;
    bool coverdrift;
    aura_category_t coverdrift_category;
    aura_ss_background_t background; /* D-281 */
} panel_identity_t;

/* Categoria CONGELADA de una identidad de panel -- reproduce, sobre
 * container_screen/selected_target ya comprometidos, la MISMA regla que
 * update_active_category() aplica en vivo cada cuadro (mas arriba en
 * este archivo): en la raiz del Menu principal la categoria real es la
 * del item resaltado, fuera de la raiz es la de la pantalla contenedora.
 * D-263: pasada explicita a aura_selection_summary_draw()/_draw_dynamic()
 * en vez de que ese componente consulte aura_category_current() en
 * vivo -- ver el comentario grande en aura_selection_summary.c para el
 * bug real que esto corrige (el color del tile saltaba antes que el
 * icono durante el debounce de D-262). */
static aura_category_t panel_identity_category(const panel_identity_t *id)
{
    return (id->container_screen == AURA_SCREEN_ROOT)
        ? aura_category_for_screen(id->selected_target)
        : aura_category_for_screen(id->container_screen);
}

static bool panel_identity_equal(const panel_identity_t *a, const panel_identity_t *b)
{
    if (a->coverdrift != b->coverdrift)
        return false;
    if (a->coverdrift)
        return a->coverdrift_category == b->coverdrift_category;
    return a->title == b->title
        && a->panel_icon == b->panel_icon
        && a->panel_top == b->panel_top
        && a->panel_desc == b->panel_desc
        && a->icon_renderer == b->icon_renderer
        && a->bottom_renderer == b->bottom_renderer
        && a->container_screen == b->container_screen
        && a->selected_target == b->selected_target
        && a->background == b->background;
}

/* Lo que esta REALMENTE dibujado en el panel derecho ahora mismo (tras
 * cualquier fundido en curso). `s_panel_pending_target` es la identidad
 * que se esta cronometrando para reemplazarla -- puede ser igual a la
 * comprometida (nada pendiente) o distinta (contando los 2s, o ya en
 * pleno fundido hacia ella). */
static bool s_panel_has_committed = false;
static panel_identity_t s_panel_committed;
static panel_identity_t s_panel_pending_target;
static long s_panel_pending_since = 0;
static bool s_panel_fading = false;
static long s_panel_fade_since = 0;

/* Snapshots de 160x240 (ancho real del panel derecho) para el fundido:
 * "vieja" = lo comprometido, leido directo del framebuffer real justo
 * despues de dibujarlo (ya esta ahi, congelado, este mismo cuadro);
 * "nueva" = lo pendiente, renderizado UNA sola vez a un framebuffer
 * offscreen (ver start_panel_fade()) y leido de vuelta -- ambas quedan
 * fijas durante los AURA_DS_METRICS_COVER_DRIFT_CROSSFADE_MS del
 * fundido (nunca se re-renderiza CoverDrift en vivo a mitad de un
 * fundido -- el movimiento ambiental es lento, 7s de borde a borde, una
 * imagen congelada 600ms es imperceptible, y evita re-disparar
 * aura_coverdrift_advance_if_due() dos veces en el mismo cuadro). */
enum { PANEL_FADE_W = A26_SCREEN_WIDTH - AURA_DS_METRICS_LEFT_PANEL_WIDTH };
static fb_data s_panel_old_snapshot[PANEL_FADE_W * A26_SCREEN_HEIGHT];
static fb_data s_panel_new_snapshot[PANEL_FADE_W * A26_SCREEN_HEIGHT];

/* Framebuffer offscreen para renderizar la identidad PENDIENTE antes de
 * que sea comprometida -- mismo patron que s_push_fb de
 * aura_transitions.c (viewport_set_buffer(), API estandar de Rockbox).
 * Tiene que cubrir el ANCHO COMPLETO de pantalla, no solo el panel
 * derecho: aura_selection_summary_draw()/aura_coverdrift_draw() reciben
 * `x` ABSOLUTO (AURA_DS_METRICS_LEFT_PANEL_WIDTH = 160), asi que un
 * buffer de 160px de ancho dejaria ese x fuera de rango. */
static fb_data s_panel_render_fb[A26_SCREEN_WIDTH * A26_SCREEN_HEIGHT];

static void *panel_render_fb_address(int x, int y)
{
    return &s_panel_render_fb[y * A26_SCREEN_WIDTH + x];
}

static struct frame_buffer_t s_panel_render_buffer = {
    { .fb_ptr = s_panel_render_fb },
    .get_address_fn = &panel_render_fb_address,
    .stride = STRIDE_MAIN(A26_SCREEN_WIDTH, A26_SCREEN_HEIGHT),
    .elems = A26_SCREEN_WIDTH * A26_SCREEN_HEIGHT,
};

/* Dibuja una identidad de panel de forma "en vivo" (CoverDrift sigue
 * avanzando su ciclo si le toca, SelectionSummary/icono normal si no)
 * -- usada tanto para redibujar lo comprometido cuadro a cuadro (sea
 * que este congelado esperando el plazo, o normal) como para
 * renderizar la identidad pendiente UNA vez al arrancar un fundido
 * (ver start_panel_fade()). */
static void draw_panel_identity(int panel_x, int panel_w, const panel_identity_t *id)
{
    if (id->coverdrift)
    {
        aura_coverdrift_advance_if_due(panel_w, s_drift_album_pool_count);
        ensure_drift_albums_decoded();
        aura_coverdrift_draw(panel_x, panel_w,
                              s_drift_album_images, s_drift_album_pool_count);
    }
    else if (id->icon_renderer || id->bottom_renderer)
    {
        /* D-264: variante dinamica -- Reloj (icono: reloj analogico,
         * bottom_renderer NULL) o Acerca de (icono ESTATICO "ipod"
         * envuelto en un renderer trivial, ver draw_about_icon_renderer()
         * mas abajo -- pie: grafico de almacenamiento). INVARIANTE: quien
         * arma este panel_identity_t nunca deja bottom_renderer sin
         * icon_renderer -- aura_selection_summary_draw_dynamic() no
         * recibe `icon_name`, asi que un icon_renderer NULL aca
         * dibujaria el tile sin ningun simbolo encima. */
        aura_selection_summary_draw_dynamic(panel_x, panel_w, id->icon_renderer,
                                             panel_identity_category(id),
                                             id->panel_top, id->panel_desc,
                                             id->bottom_renderer, id->background);
    }
    else
    {
        aura_selection_summary_draw(panel_x, panel_w, id->panel_icon,
                                     panel_identity_category(id),
                                     id->panel_top, id->panel_desc);
    }
}

/* Arranca el fundido hacia s_panel_pending_target: captura lo viejo del
 * framebuffer real (el llamador ya dibujo lo comprometido este mismo
 * cuadro, congelado, justo antes de esta llamada) y renderiza lo nuevo
 * una vez a un viewport offscreen para capturarlo tambien. Despues de
 * esta funcion, cada cuadro subsiguiente hasta que el fundido termine
 * solo hace blend de los dos snapshots (draw_panel_fade_frame()) --
 * ningun redibujo en vivo de ninguno de los dos lados a mitad del
 * fundido. */
static void start_panel_fade(int panel_x, int panel_w)
{
    struct viewport vp;
    struct viewport *saved;
    int x, y;

    for (y = 0; y < A26_SCREEN_HEIGHT; y++)
    {
        const fb_data *src = FBADDR(panel_x, y);
        memcpy(&s_panel_old_snapshot[y * panel_w], src, panel_w * sizeof(fb_data));
    }

    viewport_set_defaults(&vp, SCREEN_MAIN);
    viewport_set_buffer(&vp, &s_panel_render_buffer, SCREEN_MAIN);
    saved = lcd_set_viewport(&vp);
    /* Bug real (encontrado por el dueno, "parpadeo" de una imagen de
     * CoverDrift al cambiar de fila incluso sin CoverDrift de por medio):
     * s_panel_render_fb es un buffer ESTATICO, nunca se limpia entre usos
     * -- SelectionSummary (draw_summary(), aura_selection_summary.c) NUNCA
     * llena el rectangulo completo del panel, solo el tile+texto (deja
     * margenes sin tocar, confiando en que el llamador YA limpio la
     * pantalla -- cierto en el camino en vivo, a26_shell_clear_screen() al
     * inicio de draw_menu_screen_v2(), pero FALSO aca: este buffer offscreen
     * nunca pasa por ahi). Si una renderizacion anterior a este mismo buffer
     * fue CoverDrift (llena el panel completo, borde a borde), sus pixeles
     * sobrevivian en los margenes de cualquier fundido posterior hacia
     * SelectionSummary -- visibles durante los 600ms del fundido, hasta
     * que el commit final vuelve al framebuffer real (ya limpio cada
     * cuadro). Limpiar el viewport aca (mismo par bg/fg que
     * a26_shell_clear_screen(), pero contra el viewport YA canjeado a este
     * buffer offscreen, no al framebuffer real) elimina cualquier pixel
     * viejo antes de dibujar la identidad pendiente encima. */
    lcd_set_background(a26_color(A26_SHELL_BG));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_clear_viewport();
    draw_panel_identity(panel_x, panel_w, &s_panel_pending_target);
    lcd_set_viewport(saved);

    for (y = 0; y < A26_SCREEN_HEIGHT; y++)
        for (x = 0; x < panel_w; x++)
            s_panel_new_snapshot[y * panel_w + x] =
                s_panel_render_fb[y * A26_SCREEN_WIDTH + panel_x + x];

    s_panel_fading = true;
    s_panel_fade_since = current_tick;
}

/* Un cuadro del fundido en curso: blend por pixel de los dos snapshots
 * congelados, escrito directo al framebuffer real (mismo
 * a26_shell_blend() que ya usa la sombra paralela de D-258 y el propio
 * fundido interno de CoverDrift, D-256). */
static void draw_panel_fade_frame(int panel_x, int panel_w, long elapsed_ms)
{
    int alpha_256 = elapsed_ms * 256 / AURA_DS_METRICS_COVER_DRIFT_CROSSFADE_MS;
    int x, y;

    if (alpha_256 > 256) alpha_256 = 256;
    if (alpha_256 < 0) alpha_256 = 0;

    for (y = 0; y < A26_SCREEN_HEIGHT; y++)
    {
        fb_data *dst = FBADDR(panel_x, y);
        const fb_data *from = &s_panel_old_snapshot[y * panel_w];
        const fb_data *to = &s_panel_new_snapshot[y * panel_w];
        for (x = 0; x < panel_w; x++)
            dst[x] = a26_shell_blend(from[x], to[x], alpha_256);
    }
}

/* Punto de entrada unico para el panel derecho de la Ruta A, llamado
 * desde draw_menu_screen_v2() en vez del dibujo instantaneo de antes.
 * Reglas, en orden:
 *  1. Entrada a una pantalla distinta (title cambia de puntero -- ver
 *     nota de aura_str() en root_selection_description()): SIEMPRE
 *     instantaneo, sin fundido -- nunca debe verse contenido de la
 *     pantalla ANTERIOR congelado al entrar a una nueva.
 *  2. Fundido en curso (SOLO ocurre camino a CoverDrift, ver regla 3):
 *     si la identidad pendiente sigue siendo la misma, avanza el fundido
 *     (o lo cierra si ya se cumplieron los CROSSFADE_MS); si cambio a
 *     mitad del fundido, lo aborta (vuelve a mostrar lo comprometido
 *     solido) y reinicia el conteo de estabilidad para la nueva
 *     pendiente.
 *  3. Sin fundido: si la fila resaltada ya coincide con lo comprometido,
 *     redibuja en vivo sin mas. Si difiere, cronometra (reinicia el
 *     conteo si la pendiente cambio) -- D-266 (correccion del dueno tras
 *     probar D-262 en el simulador, 2026-08-15): la espera y la accion
 *     final YA NO son uniformes, dependen de hacia DONDE se dirige la
 *     pendiente. Si la pendiente es CoverDrift: espera
 *     RIGHT_PANEL_DEBOUNCE_MS (2s) y, al cumplirse, arranca el fundido
 *     real (start_panel_fade(), CROSSFADE_MS). Si la pendiente es
 *     SelectionSummary/icono normal (nunca CoverDrift): espera
 *     RIGHT_PANEL_DEBOUNCE_FAST_MS (1s, mas corta) y, al cumplirse,
 *     compromete DIRECTO -- corte instantaneo, sin fundido -- sin
 *     importar si lo comprometido saliente era CoverDrift o no. Mientras
 *     se cuenta cualquiera de las dos esperas, se sigue mostrando lo
 *     comprometido, congelado. */
static void render_panel_debounced(int panel_x, int panel_w, const char *title,
                                    const char *panel_icon, const char *panel_top,
                                    const char *panel_desc,
                                    aura_selection_summary_icon_renderer_t icon_renderer,
                                    aura_selection_summary_bottom_renderer_t bottom_renderer,
                                    aura_screen_id_t container_screen,
                                    aura_screen_id_t selected_target,
                                    aura_ss_background_t background)
{
    panel_identity_t pending;

    ensure_drift_album_pool();
    pending.title = title;
    pending.panel_icon = panel_icon;
    pending.panel_top = panel_top;
    pending.panel_desc = panel_desc;
    pending.icon_renderer = icon_renderer;
    pending.bottom_renderer = bottom_renderer;
    pending.container_screen = container_screen;
    pending.selected_target = selected_target;
    pending.background = background;
    pending.coverdrift = music_row_wants_coverdrift(container_screen, selected_target)
        && aura_coverdrift_should_mount(s_drift_album_pool_count);
    pending.coverdrift_category = pending.coverdrift
        ? aura_category_for_screen(selected_target) : AURA_CATEGORY_NONE;

    if (!s_panel_has_committed || pending.title != s_panel_committed.title)
    {
        s_panel_fading = false;
        s_panel_committed = pending;
        s_panel_pending_target = pending;
        s_panel_pending_since = current_tick;
        s_panel_has_committed = true;
        draw_panel_identity(panel_x, panel_w, &s_panel_committed);
        return;
    }

    if (s_panel_fading)
    {
        long elapsed_ms;

        if (!panel_identity_equal(&pending, &s_panel_pending_target))
        {
            s_panel_fading = false;
            s_panel_pending_target = pending;
            s_panel_pending_since = current_tick;
            draw_panel_identity(panel_x, panel_w, &s_panel_committed);
            return;
        }

        elapsed_ms = (current_tick - s_panel_fade_since) * 1000L / HZ;
        if (elapsed_ms >= AURA_DS_METRICS_COVER_DRIFT_CROSSFADE_MS)
        {
            s_panel_fading = false;
            s_panel_committed = s_panel_pending_target;
            s_panel_pending_since = current_tick;
            draw_panel_identity(panel_x, panel_w, &s_panel_committed);
            return;
        }

        draw_panel_fade_frame(panel_x, panel_w, elapsed_ms);
        return;
    }

    if (!panel_identity_equal(&pending, &s_panel_pending_target))
    {
        s_panel_pending_target = pending;
        s_panel_pending_since = current_tick;
    }

    if (panel_identity_equal(&pending, &s_panel_committed))
    {
        draw_panel_identity(panel_x, panel_w, &s_panel_committed);
        return;
    }

    {
        long debounce_ms = s_panel_pending_target.coverdrift
            ? AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_MS
            : AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_FAST_MS;

        if ((current_tick - s_panel_pending_since) * 1000L / HZ >= debounce_ms)
        {
            if (s_panel_pending_target.coverdrift)
            {
                draw_panel_identity(panel_x, panel_w, &s_panel_committed);
                start_panel_fade(panel_x, panel_w);
                draw_panel_fade_frame(panel_x, panel_w, 0);
            }
            else
            {
                s_panel_committed = s_panel_pending_target;
                draw_panel_identity(panel_x, panel_w, &s_panel_committed);
            }
            return;
        }
    }

    draw_panel_identity(panel_x, panel_w, &s_panel_committed);
}

/* Publica (aura_screens.h) para que aura_main.c pida cuadros a cadencia
 * fina/media mientras el panel derecho tiene algo pendiente sin
 * confirmar todavia (contando los 2s, o corriendo el fundido) --
 * aura_coverdrift_animating() sigue cubriendo el movimiento ambiental
 * en vivo una vez comprometido, esto cubre el tramo de espera/fundido
 * que antes solo existia para CoverDrift (D-254) y ahora aplica a
 * cualquier panel derecho de la Ruta A. */
bool aura_screens_right_panel_pending(void)
{
    return s_panel_has_committed
        && !panel_identity_equal(&s_panel_pending_target, &s_panel_committed);
}

/* Cadencia FINA (HZ/20, igual que aura_coverdrift_animating()) mientras
 * el fundido de 600ms esta realmente corriendo -- aura_screens_right_
 * panel_pending() por si sola solo pide HZ/4 (misma cadencia que antes
 * usaba el "armado" de 3s), insuficiente para que el fundido se vea
 * fluido. */
bool aura_screens_right_panel_fading(void)
{
    return s_panel_fading;
}

/* D-259/D-260/D-262: publica (aura_screens.h) para que el manejador de
 * SELECT (aura_screens_handle_button(), mas abajo en este archivo)
 * sepa si CoverDrift estaba realmente MONTADO (comprometido, no solo
 * pendiente) para `target` en el ultimo cuadro dibujado -- para elegir
 * la coreografia de transicion de entrada. Consulta SIN efectos
 * secundarios. Compara por CATEGORIA (aura_category_for_screen()), no
 * por destino exacto -- misma nocion de "sesion" que panel_identity_
 * equal() ya usa internamente para CoverDrift. */
bool aura_screens_coverdrift_active_for(aura_screen_id_t target)
{
    if (!s_panel_has_committed || !s_panel_committed.coverdrift)
        return false;

    return aura_category_for_screen(target)
        == aura_category_for_screen(s_panel_committed.selected_target);
}

/* D-264: equivalente de aura_screens_coverdrift_active_for() pero para
 * la fila "Acerca de" -- el dueno confirmo explicitamente que el morph
 * al entrar a esa pantalla REUSA el revelado detras de paneles de
 * CoverDrift (D-259/D-261), no una animacion nueva. Como Acerca de no
 * es CoverDrift, no puede usar esa funcion (que exige
 * s_panel_committed.coverdrift) -- compara por DESTINO exacto, no por
 * categoria (a diferencia de CoverDrift, esta fila es la unica en su
 * categoria que califica, no hace falta la nocion de "sesion"). Misma
 * disciplina: solo lee s_panel_committed, sin efectos secundarios. */
bool aura_screens_about_reveal_active(void)
{
    return s_panel_has_committed
        && s_panel_committed.container_screen == AURA_SCREEN_SETTINGS
        && s_panel_committed.selected_target == AURA_SCREEN_SETTINGS_ABOUT;
}

/* Pantalla de menu completa del sistema nuevo (auditoria 2026-08-12,
 * cierre de la migracion parcial de T2.2/T2.3): StatusBar v2 en
 * (split) + MenuList v2/Selector en LeftPanel + SelectionSummary en el
 * panel derecho -- TODA lista de menus pasa por aca, ya no por
 * aura_widgets_draw_list() (ese queda solo para listas de CONTENIDO,
 * cuyo componente propio el design system difiere explicitamente:
 * sistema/02-navegacion-menus-contenido.md, "esas se detallan en su
 * propio componente cuando lleguemos a ellas").
 *
 * El panel izquierdo (MenuList/Selector) sigue actualizandose al
 * INSTANTE con cada movimiento, como siempre. El panel derecho YA NO
 * (D-262, revierte la regla "ambos cambian de forma instantanea" de
 * selection-summary.md, documentada aca desde la auditoria 2026-08-12
 * -- encargo textual del dueno: "darle chance al ipod de procesar y
 * renderizar correctamente") -- se congela mientras se recorre el
 * LeftPanel y solo se actualiza, con un fundido real, tras
 * AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_MS de estabilidad sobre la
 * misma fila; ver render_panel_debounced() arriba en este archivo para
 * el mecanismo completo (aplica por igual a CoverDrift y a SelectionSummary/
 * icono normal). */
/* `container_screen`/`selected_target` (D-254): identidad de la
 * pantalla y del destino de la fila resaltada, SOLO para decidir si
 * CoverDrift reemplaza a SelectionSummary en este cuadro -- pasa
 * AURA_SCREEN_COUNT en ambos desde cualquier llamador que no sea el
 * menu raiz/submenu de Musica (nunca califica, ver
 * music_row_wants_coverdrift(), asi que esos llamadores no cambian de
 * comportamiento). */
/* `panel_top`/`icon_renderer`/`bottom_renderer` (D-264): contenido real
 * del panel derecho, vacio (NULL) para casi todos los llamadores --
 * solo draw_nav_list() (Musica/Video/Fotos/Ajustes) los puebla de
 * verdad, el resto (listas de eleccion, deslizadores) sigue exactamente
 * igual que antes. */
static void draw_menu_screen_v2(const char *title,
                                 const aura_menu_item_v2_t *items, int count,
                                 int selected, const char *panel_icon,
                                 const char *panel_top,
                                 const char *panel_desc,
                                 aura_selection_summary_icon_renderer_t icon_renderer,
                                 aura_selection_summary_bottom_renderer_t bottom_renderer,
                                 aura_screen_id_t container_screen,
                                 aura_screen_id_t selected_target,
                                 aura_ss_background_t background)
{
    a26_shell_clear_screen();
    /* Barra y panel del MISMO dato (regla dura 2026-08-13): con
     * Graficos=Ninguno no hay LeftPanel, asi que tampoco hay barra
     * (split) ni SelectionSummary -- antes la barra se forzaba split
     * en ese caso y quedaba una barra de 160px sobre una lista de 320. */
    aura_widgets_draw_status_bar(title);
    aura_menu_list_draw(0, A26_LAYOUT_STATUSBAR_HEIGHT, items, count, selected);
    if (panel_icon && aura_widgets_split_active())
    {
        int panel_x = AURA_DS_METRICS_LEFT_PANEL_WIDTH;
        int panel_w = A26_SCREEN_WIDTH - AURA_DS_METRICS_LEFT_PANEL_WIDTH;

        /* D-262: el panel derecho ya no cambia al instante con la
         * seleccion (ver el comentario grande junto a
         * render_panel_debounced() mas arriba) -- se congela mientras
         * el usuario recorre el LeftPanel y solo se actualiza (con
         * fundido real) tras AURA_DS_METRICS_RIGHT_PANEL_DEBOUNCE_MS de
         * estabilidad. Decide TAMBIEN si la identidad resultante es
         * CoverDrift o SelectionSummary/icono normal -- reemplaza el
         * bloque instantaneo que vivia aca antes (2026-08-12). */
        render_panel_debounced(panel_x, panel_w, title, panel_icon, panel_top, panel_desc,
                                icon_renderer, bottom_renderer,
                                container_screen, selected_target, background);
    }
}

/* Icono del item de Ajustes que abre `screen` -- el arbol de menus
 * todavia no tiene un icono 1:1 producido por cada OPCION de las listas
 * de eleccion (pendiente de produccion de assets, selection-summary.md),
 * asi que el panel derecho de esas listas muestra el simbolo del menu
 * padre, que si existe y describe el contexto real. */
static const char *parent_settings_icon(aura_screen_id_t screen)
{
    unsigned i;
    for (i = 0; i < sizeof(settings_entries) / sizeof(settings_entries[0]); i++)
        if (settings_entries[i].target == screen)
            return settings_entries[i].icon_name;
    return "settings";
}

/* Descripcion de "sin seleccion rica" por item del menu raiz
 * (componentes/selection-summary.md: "Musica -> sin seleccion rica:
 * solo texto inferior con la Descripcion 'No hay musica'"). Reusa las
 * cadenas AURA_STR_EMPTY_MUSIC/VIDEOS/PHOTOS y AURA_STR_NOTHING_PLAYING
 * ya existentes en la tabla de idioma -- Ajustes no tiene un equivalente
 * natural de "vacio", se deja
 * sin texto inferior, el caso limite que el propio documento contempla
 * ("bottom_text... tambien acepta NULL"). */
static const char *root_selection_description(aura_screen_id_t target)
{
    switch (target)
    {
    case AURA_SCREEN_MUSIC:      return aura_str(AURA_STR_EMPTY_MUSIC);
    case AURA_SCREEN_VIDEOS:     return aura_str(AURA_STR_EMPTY_VIDEOS);
    case AURA_SCREEN_PHOTOS:     return aura_str(AURA_STR_EMPTY_PHOTOS);
    case AURA_SCREEN_NOWPLAYING: return aura_str(AURA_STR_NOTHING_PLAYING);
    default:                     return NULL;
    }
}

/* -- Contenido real del panel derecho, D-264 -----------------------------
 *
 * Encargo textual del dueno del producto, 2026-08-15: generaliza
 * root_selection_description() (arriba, solo cubria el menu raiz) a
 * TAMBIEN las filas de DENTRO de cada submenu de biblioteca (Musica/
 * Video/Fotos: titulo de seccion arriba + "No hay X" abajo cuando esa
 * seccion esta vacia) y a un puñado de filas especiales de Ajustes
 * (Aleatorio, Repetir, Fecha y hora, Acerca de). */

/* Manifiesto de Aura Studio (aura_manifest.h, ya usado por la pantalla
 * completa de Acerca de) cacheado en memoria -- aura_manifest_load() abre
 * y lee un archivo cada vez que se llama, y este archivo lo consulta
 * potencialmente TODOS los cuadros mientras se recorre Video/Fotos o
 * Acerca de (draw_nav_list() corre cada cuadro, con o sin debounce del
 * panel derecho -- el debounce solo retrasa el DIBUJO, no el computo de
 * `pending`). Cargado una sola vez por sesion (arranque perezoso, no se
 * refresca) -- si el usuario sincroniza de nuevo desde Aura Studio a
 * mitad de sesion, el resumen queda desactualizado hasta reiniciar; el
 * mismo compromiso que ya acepta silenciosamente la pantalla completa
 * de Acerca de hoy (recarga en cada draw_about(), pero esa pantalla no
 * se dibuja continuamente como un menu). */
static aura_manifest_t s_manifest_cache;
static bool s_manifest_loaded = false;
static bool s_manifest_has_data = false;

static const aura_manifest_t *cached_manifest(void)
{
    if (!s_manifest_loaded)
    {
        s_manifest_loaded = true;
        s_manifest_has_data = aura_manifest_load(&s_manifest_cache);
    }
    return s_manifest_has_data ? &s_manifest_cache : NULL;
}

/* D-279: suma real de bytes de una carpeta PLANA (sin subcarpetas) --
 * mismo costo que aura_video_draw()/aura_photos_draw() ya pagan al
 * listar /Videos y /Photos (dir_get_info() lee el tamano de la entrada
 * FAT, sin stat() extra). Nunca recursivo -- ver PLAN-about-storage.md
 * S0: un recorrido recursivo de /Music (miles de archivos) es el unico
 * costo real identificado, por eso Musica sigue viniendo del manifiesto
 * de Aura Studio, no de aca. */
static long long sum_dir_bytes(const char *path)
{
    DIR *d = opendir(path);
    struct DIRENT *entry;
    long long total = 0;

    if (!d)
        return 0;
    while ((entry = readdir(d)) != NULL)
    {
        struct dirinfo info = dir_get_info(d, entry);
        if (info.attribute & ATTR_DIRECTORY)
            continue;
        total += info.size;
    }
    closedir(d);
    return total;
}

/* D-280 (Q2): dircache listo == readdir se sirve de RAM (dir_get_info()
 * ya trae el tamano de la entrada, sin tocar disco) -- condicion real que
 * hace seguro un recorrido recursivo de /Music (miles de archivos, PLAN-
 * about-fixes.md C2). En el simulador HAVE_DIRCACHE no existe
 * (config.h: excluido con #ifndef SIMULATOR) -- ahi esta funcion siempre
 * devuelve false y el llamador cae al manifiesto, igual que antes de esta
 * pasada. */
static bool dircache_ready(void)
{
#ifdef HAVE_DIRCACHE
    struct dircache_info info;
    dircache_get_info(&info);
    return info.status == DIRCACHE_READY;
#else
    return false;
#endif
}

/* Version recursiva de sum_dir_bytes() -- SOLO se llama cuando
 * dircache_ready() ya confirmo que es barata (RAM, no disco). `/Music`
 * tiene subcarpetas Artista/Album que la version plana original (D-279,
 * para /Videos y /Photos, que SI son planas) no recorre. */
static long long sum_dir_bytes_recursive(const char *path)
{
    DIR *d = opendir(path);
    struct DIRENT *entry;
    long long total = 0;

    if (!d)
        return 0;
    while ((entry = readdir(d)) != NULL)
    {
        struct dirinfo info = dir_get_info(d, entry);
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        if (info.attribute & ATTR_DIRECTORY)
        {
            char child[MAX_PATH];
            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            total += sum_dir_bytes_recursive(child);
        }
        else
        {
            total += info.size;
        }
    }
    closedir(d);
    return total;
}

/* Recoleccion unica del dato de almacenamiento para la barra de "Acerca
 * de" (D-279), compartida entre el bottom_renderer de SelectionSummary
 * (split) y la pantalla expandida (full) -- reemplaza el calculo
 * duplicado que antes vivia suelto dentro de draw_about_storage_bars().
 * Video y Fotos NO salen del manifiesto: se prefiere la suma real de su
 * carpeta (arriba) para reflejar copias hechas a mano por Finder, que el
 * manifiesto (solo se actualiza en cada sync desde Aura Studio) no
 * vería hasta la siguiente sincronizacion -- Musica si depende del
 * manifiesto, ver el comentario de sum_dir_bytes(). `force_reload`
 * relee sync_summary.cfg del disco en vez de usar el cache de una vez
 * por sesion (cached_manifest()) -- solo hace falta al ENTRAR al estado
 * expandido (Q1: "recargar el manifest al expandir, no solo una vez por
 * sesion"); el bottom_renderer de split, que corre cada cuadro, sigue
 * con el cache barato. */
/* D-280 (Q2): cache del recorrido de /Music -- costoso incluso en RAM
 * (miles de entradas), asi que solo se recalcula cuando `force_reload`
 * es cierto (una vez por ENTRADA al estado expandido, ver
 * draw_about_storage_expanded()/handle_about() mas abajo), nunca por
 * cuadro. -1 = "no disponible" (dircache no listo, o SIMULATOR) -> usa
 * el manifiesto, comportamiento identico al de antes de esta pasada. */
static long long s_music_scan_bytes = -1;

/* D-282 (C3/Q5): bytes de /.rockbox/ (el firmware de Aura en si, no la
 * particion de firmware de Apple -- esa ya no existe en el iPod del
 * dueno, D-185/D-186, y de todos modos no es espacio de Aura). Cache de
 * SESION, no por entrada: /.rockbox/ practicamente no cambia mientras el
 * dispositivo esta encendido (solo un reinstalo desde Aura Studio lo
 * toca, y eso implica reiniciar), asi que una sola lectura real (la
 * primera vez que se pide, con dircache listo) basta -- distinto del
 * cache de musica de arriba, que si se refresca cada entrada porque el
 * usuario puede copiar canciones sin reiniciar. -2 = "todavia no
 * calculado", -1 = "no disponible" (dircache no listo/SIMULATOR). */
static long long s_system_scan_bytes = -2;

static void about_storage_collect(bool force_reload,
                                   long long *music_b, long long *video_b,
                                   long long *photo_b, long long *system_b,
                                   long long *other_b,
                                   long long *free_b, long long *total_b)
{
    aura_manifest_t fresh;
    const aura_manifest_t *m = force_reload
        ? (aura_manifest_load(&fresh) ? &fresh : NULL)
        : cached_manifest();
    sector_t vol_size = 0, vol_free = 0;

    if (force_reload)
        s_music_scan_bytes = dircache_ready() ? sum_dir_bytes_recursive("/Music") : -1;
    *music_b = (s_music_scan_bytes >= 0) ? s_music_scan_bytes : (m ? m->music_bytes : 0);
    *video_b = sum_dir_bytes("/Videos");
    *photo_b = sum_dir_bytes("/Photos");

    if (s_system_scan_bytes == -2)
        s_system_scan_bytes = dircache_ready() ? sum_dir_bytes_recursive("/.rockbox") : -1;
    *system_b = (s_system_scan_bytes >= 0) ? s_system_scan_bytes : 0;

    /* D-280: bug real -- volume_size()/fat_size() (firmware/common/fat.c)
     * devuelve KiB, no sectores (Rockbox mismo lo formatea con
     * kibyte_units, apps/menus/main_menu.c), pero este calculo llevaba
     * desde D-264 multiplicando por SECTOR_SIZE (512) en vez de 1024 --
     * total_b/free_b salian a la MITAD de lo real, duplicando todos los
     * porcentajes y el ancho de cada segmento. */
    volume_size(IF_MV(0,) &vol_size, &vol_free);
    *total_b = (long long)vol_size * 1024;
    *free_b  = (long long)vol_free * 1024;
    /* D-282: "Otros" ya no incluye lo que ahora se distingue como
     * "Sistema" (/.rockbox/) -- residual real: playlists, cache de
     * caratulas, archivos sueltos, holgura de clusters. */
    *other_b = *total_b - *free_b - *music_b - *video_b - *photo_b - *system_b;
    if (*other_b < 0)
        *other_b = 0;
}

/* Cuenta real (no cacheada -- ver ensure_drift_album_pool() arriba en
 * este archivo para el patron de cache por generacion que si haria
 * falta si esto se volviera un costo real medido, no solo teorico) de
 * si `target` tiene contenido navegable -- pedir 1 solo item basta para
 * saber "vacio" vs "no vacio", no hace falta la lista completa. */
static bool music_target_has_content(aura_screen_id_t target)
{
    static aura_music_item_t scratch[1];

    if (!aura_music_db_ready())
        return false;
    return aura_music_browse(target, scratch, 1) > 0;
}

/* "No hay artistas"/"No hay álbumes"/etc, solo cuando la seccion esta
 * genuinamente vacia -- si tiene contenido, NULL (sin texto inferior
 * extra; el dueno no describio que mostrar en el caso "con contenido",
 * se deja igual que hoy en vez de inventar un conteo no pedido).
 * Cover Flow consulta Albumes (mismo contenido real que dibuja, D-254) --
 * Busqueda no tiene un "vacio" natural antes de escribir nada, se deja
 * sin texto (juicio propio, el encargo no lo cubre). */
static const char *music_row_empty_description(aura_screen_id_t target)
{
    aura_screen_id_t query = (target == AURA_SCREEN_MUSIC_COVERFLOW)
        ? AURA_SCREEN_MUSIC_ALBUMS : target;
    aura_str_id_t empty_id;

    switch (target)
    {
    case AURA_SCREEN_MUSIC_ARTISTS:    empty_id = AURA_STR_MUSIC_EMPTY_ARTISTS; break;
    case AURA_SCREEN_MUSIC_ALBUMS:
    case AURA_SCREEN_MUSIC_COVERFLOW:  empty_id = AURA_STR_MUSIC_EMPTY_ALBUMS; break;
    case AURA_SCREEN_MUSIC_SONGS:      empty_id = AURA_STR_MUSIC_EMPTY_SONGS; break;
    case AURA_SCREEN_MUSIC_PLAYLISTS:  empty_id = AURA_STR_MUSIC_EMPTY_PLAYLISTS; break;
    case AURA_SCREEN_MUSIC_GENRES:     empty_id = AURA_STR_MUSIC_EMPTY_GENRES; break;
    case AURA_SCREEN_MUSIC_COMPOSERS:  empty_id = AURA_STR_MUSIC_EMPTY_COMPOSERS; break;
    case AURA_SCREEN_MUSIC_AUDIOBOOKS: empty_id = AURA_STR_MUSIC_EMPTY_AUDIOBOOKS; break;
    default: return NULL;
    }

    if (music_target_has_content(query))
        return NULL;
    return aura_str(empty_id);
}

/* Peliculas/Series/Videoclips (D-264): filas INERTES/atenuadas hoy (ver
 * `dimmed` en draw_nav_list()) -- todavia sin desglose real por
 * subcarpeta (esa clasificacion es trabajo aparte, no incluido en esta
 * pasada), asi que su texto "vacio" se muestra SIEMPRE mientras sigan
 * inertes, no condicionado a un conteo que este archivo todavia no
 * puede calcular. "Todos" (VIDEOS_ALL) si tiene un conteo real via el
 * manifiesto -- mismo dato que ya usa la pantalla completa de Acerca
 * de. */
static const char *video_row_empty_description(aura_screen_id_t target)
{
    switch (target)
    {
    case AURA_SCREEN_VIDEOS_MOVIES:  return aura_str(AURA_STR_VIDEOS_EMPTY_MOVIES);
    case AURA_SCREEN_VIDEOS_TVSHOWS: return aura_str(AURA_STR_VIDEOS_EMPTY_TVSHOWS);
    case AURA_SCREEN_VIDEOS_CLIPS:   return aura_str(AURA_STR_VIDEOS_EMPTY_CLIPS);
    case AURA_SCREEN_VIDEOS_ALL:
        /* D-291: cuenta real de /Videos, no sync_summary.cfg -- ese
         * manifiesto solo se reescribe en cada sync de Studio, asi que
         * podia desincronizarse de lo que en verdad hay en el disco
         * (copia manual, sync interrumpido). */
        return (aura_video_count() > 0) ? NULL : aura_str(AURA_STR_EMPTY_VIDEOS);
    default: return NULL;
    }
}

static const char *photos_row_empty_description(aura_screen_id_t target)
{
    if (target != AURA_SCREEN_PHOTOS_ALL)
        return NULL;
    /* D-291: idem video_row_empty_description() arriba -- cuenta real
     * de /Photos. */
    return (aura_photos_count() > 0) ? NULL : aura_str(AURA_STR_EMPTY_PHOTOS);
}

/* HH:MM real (D-264, fila "Fecha y hora"): reusa aura_format_clock()
 * (aura_statusbar.c/.h), YA respeta global_settings.timeformat -- sin
 * formato propio inventado. Buffer estatico reescrito en el lugar,
 * MISMO puntero en cada llamada (patron por-puntero que ya usa todo
 * este componente para detectar cambio de VALOR, aura_selection_summary.c) --
 * el contenido se actualiza cada cuadro sin que panel_identity_equal()
 * lo vea como "cambio de fila" (deliberado: el reloj debe seguir
 * corriendo en vivo mientras el panel esta "congelado" en esta fila,
 * mismo criterio que la deriva ambiental de CoverDrift durante el
 * debounce de D-262 -- ver esa nota en DECISIONS.md D-262). Buffer de 12
 * bytes -- bug real encontrado en revision: el original (8 bytes) le
 * quedaba corto al formato de 12 horas ("12:59 PM" son 8 caracteres + el
 * nulo, 9 bytes -- snprintf() no desborda, pero SI trunca en silencio,
 * cortando la "M" final). El ancho del texto varia entre 1 y 2 digitos
 * de hora en formato de 12 horas (9:05 AM vs 12:05 PM) -- MarqueeText
 * nunca lo nota (texto corto de sobra para no desbordar el panel en
 * ningun caso), asi que no hace falta reiniciar su fase por esto. */
static const char *clock_row_top_text(void)
{
    static char buf[12];
    aura_format_clock(buf, sizeof(buf));
    return buf;
}

/* Icono real de Aura (D-269, encargo del dueno de producto): a color
 * completo, NO un glifo monocromo para tenir -- reemplaza el "ipod"
 * blanco de D-263/D-264 SOLO en el icono de SelectionSummary de "Acerca
 * de" (el icono de la FILA en la lista, settings_entries[]="info", NO
 * cambia -- el dueno lo pidio explicito: "el icono que aparece en el
 * left panel no debe cambiar"). Horneado por design-system/generate.py
 * (generate_tile_icons()). D-269 lo aplano a mano de un bundle .icon de
 * Icon Composer como badge circular de 60px; D-285 lo reemplaza por las
 * dos imagenes PNG que el dueno entrego (una por tema) a tile completo.
 * Mismo patron read_bmp_file() que ensure_panel_background() (D-267),
 * con lcd_bitmap_transparent() (clave magenta, D-010): las esquinas
 * transparentes del icono dejan ver el fondo restaurado. */
static fb_data s_aura_badge_pixels[AURA_DS_METRICS_TILE_ICONS_ITEMS_AURA_BADGE_SIZE
                                    * AURA_DS_METRICS_TILE_ICONS_ITEMS_AURA_BADGE_SIZE];
static int  s_aura_badge_theme = -1;   /* tema con el que se cargo el buffer */
static unsigned s_aura_badge_generation = 0; /* D-289: generacion del estilo activo */
static bool s_aura_badge_ok = false;

/* D-285 (encargo del dueno, dos imagenes del icono de Aura): el badge deja
 * de ser un circulo de 60px sobre el degradado gris y pasa a ser la IMAGEN
 * COMPLETA del icono cubriendo el tile de 90px -- una version por tema
 * (aura_badge-light.bmp para el claro, aura_badge-dark.bmp para el oscuro,
 * decision de asignacion: el icono de fondo oscuro va con el tema oscuro).
 * El icono trae su propio cuadrado redondeado con radio del 31% (medido
 * en la fuente: 317/1024), el mismo 31% (28px) del tile de D-236 -- por
 * eso el recorte de esquinas del tile (que ahora corre DESPUES del
 * renderer, ver aura_selection_summary_draw_tile) no deja crecientes y la
 * silueta coincide con los demas tiles. Se recarga si el usuario cambia
 * de tema en la sesion. La fila de la lista sigue con su icono "info"
 * (regla de D-269: solo SelectionSummary). */
static void draw_about_icon_renderer(int cx, int cy, int size)
{
    enum { SZ = AURA_DS_METRICS_TILE_ICONS_ITEMS_AURA_BADGE_SIZE };
    int theme = (aura_settings.theme == AURA_THEME_DARK) ? 1 : 0;
    unsigned generation = aura_style_generation();

    /* D-289: la generacion del estilo activo entra en la clave de
     * cache, ademas del tema claro/oscuro -- sin esto, cambiar de
     * estilo (o revertir uno invalido) no invalidaria este buffer si
     * el tema claro/oscuro no cambio a la vez. */
    if (s_aura_badge_theme != theme || s_aura_badge_generation != generation)
    {
        char rel[MAX_PATH];
        struct bitmap bm;
        int ret;

        s_aura_badge_theme = theme;
        s_aura_badge_generation = generation;
        snprintf(rel, sizeof(rel), "tile-icons/aura_badge-%s.bmp",
                 theme ? "dark" : "light");
        bm.data = (char *)s_aura_badge_pixels;
        ret = aura_style_read_icon_bmp(rel, &bm, sizeof(s_aura_badge_pixels));
        s_aura_badge_ok = (ret > 0 && bm.width == SZ && bm.height == SZ);
    }

    if (s_aura_badge_ok)
        lcd_bitmap_transparent(s_aura_badge_pixels, cx - SZ / 2, cy - SZ / 2, SZ, SZ);
    else
        aura_widgets_draw_icon_variant_selector("ipod", size,
                                                 cx - size / 2, cy - size / 2);
}

/* Color solido representativo de una categoria -- el centro del
 * degradado de 3 puntos que ya usa aura_category_gradient() (mismo tono
 * base que el resto del sistema, ninguna paleta nueva). */
static unsigned category_flat_color(aura_category_t cat)
{
    unsigned a, center, b;
    aura_category_gradient(cat, &a, &center, &b);
    return center;
}

/* D-289: el amarillo plano de Extras para las dos barras de
 * almacenamiento de abajo -- Extras NO pasa por category_flat_color()
 * aca porque su degradado de dos tonos (amarillo -> acento) no tiene
 * un "centro" real (aura_category_gradient() no lo calcula para esa
 * categoria, ver apple2026_shell.c). Resuelto contra el estilo activo
 * en vez del AURA_DS_COLOR_CATEGORY_EXTRAS_YELLOW compilado. */
static unsigned category_extras_yellow_flat(void)
{
    aura_rgb_t c = aura_color_from_rgb24(aura_style_category_extras_yellow_rgb24());
    return LCD_RGBPACK(c.r, c.g, c.b);
}

/* Grafico de almacenamiento por color (D-264, extendido D-279/D-282 --
 * encargo textual original: "dividir la barra en 4 segmentos por color:
 * rosa = musica, azul = videos, verde = imagenes, amarillo = extras").
 * Verde para Fotos hubiera roto la jerarquia de color por categoria ya
 * fija por encargo del dueno (D-250, `photos_hex` naranja, 2026-08-14) --
 * el dueno confirmo mantener el color de categoria vigente en toda la
 * barra (PLAN-about-storage.md Q4) en vez de introducir un verde que
 * seria la unica excepcion del sistema. "Extras" no tiene bytes medibles
 * (ver about_storage_collect()): el segmento amarillo representa el
 * residual "Otros" con el extremo amarillo de la categoria Extras
 * (`extras_yellow_hex`). D-282 (encargo del dueno, separar "Sistema" de
 * "Otros"): sexto segmento gris de Ajustes (`settings_gray`, D-250 --
 * Sistema=Ajustes semanticamente, y ese gris ya era el color de "Otros"
 * antes de D-279) para /.rockbox/. Colores: acento configurable para
 * Musica, color fijo de categoria para Video/Fotos, gris de Ajustes para
 * Sistema, amarillo fijo de Extras para "Otros" -- ninguno es un RGB
 * nuevo. */
/* Carril + segmentos, sin fondo/capsula -- el llamador decide como tapar
 * los extremos segun lo que haya DETRAS (D-279): imagen del panel
 * derecho en split (restore, ver abajo) o SHELL_BG plano en la pantalla
 * expandida (over_content, mas simple, sin snapshot). Compartido para no
 * duplicar el calculo de anchos/colores/minimo visible en dos sitios. */
static void draw_storage_segments(int x, int y, int width, int height,
                                   long long music_b, long long video_b,
                                   long long photo_b, long long system_b,
                                   long long other_b,
                                   long long free_b, long long total_b)
{
    int seg_x = x;
    int i;
    /* D-282/Q4: contorno de 1px hacia adentro de cada segmento (y de los
     * puntos de color de las filas del expandido, ver
     * draw_about_storage_row()) -- amarillo (1.5:1) y naranja (2.2:1)
     * fallan el umbral WCAG de 3:1 para elementos graficos sobre el fondo
     * casi-blanco nuevo de D-281, y ningun tono "amarillo mas oscuro" pasa
     * 3:1 sin dejar de leerse como amarillo (PLAN-about-fixes.md
     * seccion 4). Blend hacia negro en tema claro / hacia blanco en
     * oscuro -- misma regla para los 6 colores, no solo los que fallan,
     * para que la barra se lea como una sola pieza. */
    bool dark = (aura_settings.theme == AURA_THEME_DARK);
    unsigned outline_toward = dark ? LCD_RGBPACK(255, 255, 255) : LCD_RGBPACK(0, 0, 0);
    int outline_pct256 = (AURA_DS_METRICS_ABOUT_SEGMENT_OUTLINE_PCT * 256) / 100;
    struct { long long bytes; unsigned color; } segs[6];

    segs[0].bytes = music_b;  segs[0].color = aura_accent();
    segs[1].bytes = video_b;  segs[1].color = category_flat_color(AURA_CATEGORY_VIDEO);
    segs[2].bytes = photo_b;  segs[2].color = category_flat_color(AURA_CATEGORY_PHOTOS);
    segs[3].bytes = system_b; segs[3].color = category_flat_color(AURA_CATEGORY_SETTINGS);
    segs[4].bytes = other_b;  segs[4].color = category_extras_yellow_flat();
    segs[5].bytes = free_b;   segs[5].color = a26_color(A26_PROGRESS_TRACK);

    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(a26_color(A26_PROGRESS_TRACK));
    lcd_fillrect(x, y, width, height);
    lcd_set_drawmode(DRMODE_FG);

    if (total_b <= 0)
        return;

    for (i = 0; i < 6; i++)
    {
        int seg_w = (int)((long long)width * segs[i].bytes / total_b);
        unsigned outline;
        if (segs[i].bytes <= 0)
            continue;
        /* D-279/Q9: bytes>0 pero proporcion diminuta no debe desaparecer
         * -- ancho minimo visible (about.segment_min_px). Recortado
         * contra lo que quede del ancho real de la barra para no pintar
         * fuera de [x, x+width). */
        if (seg_w < AURA_DS_METRICS_ABOUT_SEGMENT_MIN_PX)
            seg_w = AURA_DS_METRICS_ABOUT_SEGMENT_MIN_PX;
        if (seg_x + seg_w > x + width)
            seg_w = (x + width) - seg_x;
        if (seg_w <= 0)
            continue;
        lcd_set_foreground(segs[i].color);
        lcd_fillrect(seg_x, y, seg_w, height);
        /* Contorno superior/inferior de 1px hacia adentro (D-282/Q4) --
         * filas horizontales, no verticales: no interfieren con el
         * separador SHELL_BG entre segmentos de abajo (que si es vertical,
         * en la misma columna que un contorno lateral se pisarian). Sube
         * el contraste contra el fondo casi-blanco que rodea la barra por
         * arriba y por abajo, para los 6 colores por igual. */
        outline = a26_shell_blend(segs[i].color, outline_toward, outline_pct256);
        lcd_set_foreground(outline);
        lcd_hline(seg_x, seg_x + seg_w - 1, y);
        lcd_hline(seg_x, seg_x + seg_w - 1, y + height - 1);
        if (seg_x > x)
        {
            lcd_set_foreground(a26_color(A26_SHELL_BG));
            lcd_vline(seg_x, y, y + height - 1);
        }
        seg_x += seg_w;
    }
}

static void draw_about_storage_bars(int x, int y, int width, int height)
{
    long long music_b, video_b, photo_b, system_b, other_b, free_b, total_b;

    /* D-277: esta barra vive SOBRE la imagen de fondo del panel derecho
     * (D-267), no sobre SHELL_BG -- redondear contra un color plano
     * pintaba esquinas blancas encima de la imagen (defecto latente desde
     * D-267). Se captura el fondo real ANTES de pintar (mismo contrato
     * que el tile, a26_shell_round_bitmap_corners_over_content) y al
     * final la mascara de capsula restaura esos pixeles en los
     * casquetes. El buffer cubre el ancho maximo del panel derecho por
     * el alto maximo razonable de la barra -- NO sirve para la barra
     * expandida de la pantalla completa (D-279, mas ancha que el panel
     * derecho): esa usa draw_about_storage_bar_expanded() mas abajo,
     * contra SHELL_BG plano, sin snapshot. */
    static fb_data saved_bar[(A26_SCREEN_WIDTH - AURA_DS_METRICS_LEFT_PANEL_WIDTH) * 24];
    bool restore = width > 0 && height > 0
                && width <= (A26_SCREEN_WIDTH - AURA_DS_METRICS_LEFT_PANEL_WIDTH)
                && height <= 24;

    about_storage_collect(false, &music_b, &video_b, &photo_b, &system_b,
                           &other_b, &free_b, &total_b);

    if (restore)
    {
        int sy;
        for (sy = 0; sy < height; sy++)
            memcpy(&saved_bar[sy * width], FBADDR(x, y + sy), width * sizeof(fb_data));
    }

    draw_storage_segments(x, y, width, height, music_b, video_b, photo_b,
                          system_b, other_b, free_b, total_b);

    /* D-277: los segmentos rectangulares tapaban las esquinas que el
     * carril traia redondeadas -- la barra terminaba cuadrada. La
     * mascara de capsula recorta los DOS extremos del compuesto
     * (multicolor) con semicirculos exactos, restaurando la imagen de
     * fondo real en los casquetes. */
    if (restore)
        a26_shell_capsule_ends_restore(x, y, width, height, saved_bar, width);
}

/* Punto de entrada unico (D-264): llamado por draw_nav_list() para
 * CUALQUIER pantalla -- filas sin caso especial simplemente no tocan
 * ninguna salida (se quedan en lo que el llamador ya puso por defecto:
 * `*panel_icon` sin cambios, el resto en NULL). */
static void compute_panel_content(aura_screen_id_t screen, aura_screen_id_t target,
                                   const char **panel_icon,
                                   const char **panel_top,
                                   const char **panel_desc,
                                   aura_selection_summary_icon_renderer_t *icon_renderer,
                                   aura_selection_summary_bottom_renderer_t *bottom_renderer,
                                   aura_ss_background_t *background)
{
    if (screen == AURA_SCREEN_ROOT)
    {
        *panel_desc = root_selection_description(target);
        return;
    }

    if (screen == AURA_SCREEN_MUSIC)
    {
        *panel_top = aura_str(AURA_STR_MUSIC);
        *panel_desc = music_row_empty_description(target);
        return;
    }

    if (screen == AURA_SCREEN_VIDEOS)
    {
        *panel_top = aura_str(AURA_STR_VIDEOS);
        *panel_desc = video_row_empty_description(target);
        return;
    }

    if (screen == AURA_SCREEN_PHOTOS)
    {
        *panel_top = aura_str(AURA_STR_PHOTOS);
        *panel_desc = photos_row_empty_description(target);
        return;
    }

    if (screen == AURA_SCREEN_SETTINGS)
    {
        if (target == AURA_SCREEN_SETTINGS_SHUFFLE)
        {
            *panel_desc = settings_row_toggle_value(AURA_SCREEN_SETTINGS_SHUFFLE)
                ? aura_str(AURA_STR_TOGGLE_ON) : aura_str(AURA_STR_TOGGLE_OFF);
            return;
        }
        if (target == AURA_SCREEN_SETTINGS_REPEAT)
        {
            int mode = global_settings.repeat_mode;
            *panel_icon = (mode == 2) ? "repeat-1" : "repeat";
            *panel_top = aura_str(AURA_STR_REPEAT_ROW_TOP);
            *panel_desc = (mode == 1) ? aura_str(AURA_STR_REPEAT_SUMMARY_ALL)
                        : (mode == 2) ? aura_str(AURA_STR_REPEAT_SUMMARY_ONE)
                        : aura_str(AURA_STR_TOGGLE_OFF);
            return;
        }
        if (target == AURA_SCREEN_SETTINGS_DATETIME)
        {
            *icon_renderer = aura_selection_summary_render_analog_clock;
            *panel_top = clock_row_top_text();
            *panel_desc = aura_str(AURA_STR_SETTINGS_DATETIME);
            return;
        }
        if (target == AURA_SCREEN_SETTINGS_ABOUT)
        {
            *icon_renderer = draw_about_icon_renderer;
            *panel_top = aura_str(AURA_STR_ABOUT_MY_IPOD);
            *bottom_renderer = draw_about_storage_bars;
            /* D-281 (C1): fondo neutro gris->blanco solo para esta fila --
             * transicion mas fluida hacia el estado expandido (SHELL_BG
             * plano), pedido explicito del dueno solo para "Acerca de". */
            *background = AURA_SS_BG_NEUTRAL_FADE;
            return;
        }
    }
}

static void draw_nav_list(aura_nav_t *nav, aura_screen_id_t screen)
{
    const nav_entry_t *entries;
    int count = get_nav_table(screen, &entries);
    aura_menu_item_v2_t items[MAX_MENU_ENTRIES];
    int selected = aura_nav_get_selection(nav);
    const char *panel_icon = NULL;
    int i;

    for (i = 0; i < count; i++)
    {
        items[i].label = aura_str(entries[i].label_id);
        items[i].icon_name = entries[i].icon_name;
        items[i].checked = 0;
        items[i].toggle = settings_row_toggle_value(entries[i].target);
        /* Flecha del Selector (selector.md) solo para destinos
         * navegables de pantalla completa -- un toggle inline no navega
         * a ningun lado, la regla de exclusion del documento ya lo
         * cubre pero se evita marcarlo siquiera. */
        items[i].indent = 0;
        /* Repetir (D-264): fila en linea igual que Aleatorio/Clicker
         * (SELECT cicla el valor, nunca navega) -- pero NO es un
         * toggle booleano (`toggle` se queda en -1, 3 estados, sin
         * pastilla de switch propia), asi que necesita su propia
         * exclusion aca ademas de la que ya cubre `toggle < 0`. */
        items[i].full_screen_target = (items[i].toggle < 0)
            && entries[i].target != AURA_SCREEN_SETTINGS_REPEAT
            && !screen_uses_split_layout(entries[i].target);
        /* Filas del arbol del original sin contenido propio todavia. */
        items[i].dimmed = (entries[i].target == AURA_SCREEN_MUSIC_COMPILATIONS
                           || entries[i].target == AURA_SCREEN_VIDEOS_MOVIES
                           || entries[i].target == AURA_SCREEN_VIDEOS_TVSHOWS
                           || entries[i].target == AURA_SCREEN_VIDEOS_CLIPS
                           || entries[i].target == AURA_SCREEN_MUSIC_AUDIOBOOKS
                           || entries[i].target == AURA_SCREEN_EXTRAS_CONTACTS);
    }

    {
        const char *panel_top = NULL;
        const char *panel_desc = NULL;
        aura_selection_summary_icon_renderer_t icon_renderer = NULL;
        aura_selection_summary_bottom_renderer_t bottom_renderer = NULL;
        aura_ss_background_t background = AURA_SS_BG_ACCENT_IMAGE;
        aura_screen_id_t target = (selected >= 0 && selected < count)
            ? entries[selected].target : AURA_SCREEN_COUNT;

        if (selected >= 0 && selected < count)
        {
            if (entries[selected].icon_name)
                panel_icon = entries[selected].icon_name;
            else if (screen == AURA_SCREEN_MUSIC)
                panel_icon = "music"; /* icono del item padre en el raiz --
                                       * los items de biblioteca no tienen
                                       * icono 1:1 producido todavia */
            else
                panel_icon = parent_settings_icon(screen);

            /* D-264: contenido real por fila -- generaliza
             * root_selection_description() (arriba) al resto de
             * pantallas. Puede sobreescribir `panel_icon` (Repetir,
             * icono segun estado) ademas de poblar top/desc/renderers.
             * D-281: tambien decide la variante de fondo (solo "Acerca
             * de" pide NEUTRAL_FADE hoy). */
            compute_panel_content(screen, target, &panel_icon, &panel_top,
                                   &panel_desc, &icon_renderer, &bottom_renderer,
                                   &background);
        }

        draw_menu_screen_v2(screen == AURA_SCREEN_ROOT ? "Aura"
                                                        : aura_str(screen_title_id(screen)),
                             items, count, selected, panel_icon,
                             panel_top, panel_desc,
                             icon_renderer, bottom_renderer,
                             screen, target, background);
    }
}

/* -- Vista grafica del Ecualizador (encargo 2026-08-13) -------------------
 *
 * Reemplaza el panel derecho generico (icono + descripcion) SOLO para
 * la lista de presets del EQ: en vez de un icono estatico, dibuja la
 * curva de respuesta real del preset resaltado, en vivo mientras se
 * recorre la lista. Aura no tiene edicion de banda por banda (el
 * modelo son 23 presets fijos, D-156) asi que esta es una vista de
 * PREVISUALIZACION, no el editor interactivo completo de tres campos
 * (frecuencia/Q/ganancia) que el firmware de referencia investigado
 * construye -- alcance honesto, coherente con el modelo real de Aura.
 *
 * Matematica identica a la investigada (aproximacion gaussiana de los
 * filtros de pico/escalon, eje de frecuencia logaritmico en punto fijo
 * 1/256 de octava) -- sin coma flotante, sin tabla nueva de por medio
 * mas alla de la LUT gaussiana de 33 entradas. */
static int eqg_log2(int hz)
{
    int e = 0, m;
    if (hz < 1) hz = 1;
    m = hz;
    while (m > 1) { m >>= 1; e++; }
    return (e << 8) + (int)(((long)(hz - (1 << e)) << 8) >> e);
}

static const unsigned short EQG_GAUSS[33] = {
    256, 252, 240, 222, 199, 173, 146, 119,  94,  72,  54,  39,
     27,  18,  12,   8,   5,   3,   2,   1,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0
};

static int eqg_bell(int x)
{
    int i, f;
    if (x < 0) x = -x;
    x >>= 1;
    i = x >> 5;
    if (i >= 32) return 0;
    f = x & 31;
    return (EQG_GAUSS[i] * (32 - f) + EQG_GAUSS[i + 1] * f) >> 5;
}

typedef struct { int u0, w, gain, shelf; } eqg_band_t;

static int eqg_prepare(eqg_band_t *out, const int *gains)
{
    int n = 0, i;
    for (i = 0; i < EQ_NUM_BANDS; i++)
    {
        if (gains[i] == 0)
            continue;
        out[n].u0 = eqg_log2(eq_defaults[i].cutoff);
        out[n].w = (7 * 256) / (eq_defaults[i].q > 0 ? eq_defaults[i].q : 1);
        if (out[n].w < 24) out[n].w = 24;
        out[n].gain = gains[i];
        out[n].shelf = (i == 0) ? -1 : (i == EQ_NUM_BANDS - 1) ? 1 : 0;
        n++;
    }
    return n;
}

/* Respuesta total en decimas de dB en la posicion `u` (1/256 de
 * octava). El escalon se construye con la MISMA campana que el pico:
 * vale 1-bell/2 antes del corte y bell/2 despues (o al reves para el
 * de agudos), cruzando por 0.5 exactamente en el corte -- una
 * sigmoide gratis, sin funcion nueva. */
static int eqg_response(const eqg_band_t *bands, int n, int u)
{
    int total = 0, i;
    for (i = 0; i < n; i++)
    {
        int d = u - bands[i].u0;
        int k = eqg_bell((d << 8) / bands[i].w);
        if (bands[i].shelf < 0)
            k = (d <= 0) ? 256 - (k >> 1) : (k >> 1);
        else if (bands[i].shelf > 0)
            k = (d >= 0) ? 256 - (k >> 1) : (k >> 1);
        total += (bands[i].gain * k) >> 8;
    }
    return total;
}

#define EQG_U_MIN 1106 /* log2(20 Hz) x 256 */
#define EQG_U_MAX 3658 /* log2(20000 Hz) x 256 */

static void draw_eq_curve_panel(int x, int width, int preset)
{
    const int *gains = aura_settings_eq_preset_gains(preset);
    eqg_band_t bands[EQ_NUM_BANDS];
    int n = eqg_prepare(bands, gains);
    int plot_x = x + A26_SPACING_MD;
    int plot_w = width - 2 * A26_SPACING_MD;
    int plot_y = A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_XXL + A26_SPACING_MD;
    int plot_h = 100;
    int zero_y = plot_y + plot_h / 2;
    int px_per_db = 2; /* +-24 dB caben en +-48px, holgado en 100px de alto */
    unsigned fill = a26_shell_blend(aura_accent(), a26_color(A26_SHELL_BG), 38);
    int px, prev_y = zero_y;

    /* Icono/etiqueta del preset arriba del plot (reemplaza el icono
     * generico del panel comun). */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_putsxy(plot_x, A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_MD,
               (const unsigned char *)aura_str(AURA_STR_SETTINGS_EQ));

    /* Linea de 0 dB, ANTES que la curva -- si no, un preset plano
     * (todas las bandas en 0) quedaria invisible bajo ella. */
    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_hline(plot_x, plot_x + plot_w - 1, zero_y);

    for (px = 0; px < plot_w; px++)
    {
        int u = EQG_U_MIN + (px * (EQG_U_MAX - EQG_U_MIN)) / (plot_w - 1);
        int db = eqg_response(bands, n, u);
        int y = zero_y - (db * px_per_db) / 10;
        int sx = plot_x + px;

        if (y < plot_y) y = plot_y;
        if (y > plot_y + plot_h - 1) y = plot_y + plot_h - 1;

        lcd_set_foreground(fill);
        if (y < zero_y)      lcd_vline(sx, y, zero_y - 1);
        else if (y > zero_y) lcd_vline(sx, zero_y + 1, y);

        lcd_set_foreground(aura_accent());
        if (px == 0) prev_y = y;
        lcd_vline(sx, prev_y < y ? prev_y : y, prev_y > y ? prev_y : y);
        prev_y = y;
    }
}

static void draw_choice_list(aura_nav_t *nav, aura_screen_id_t screen)
{
    const aura_str_id_t *labels;
    int count = get_choice_table(screen, &labels);
    int current = get_choice_current(screen);
    aura_menu_item_v2_t items[MAX_MENU_ENTRIES];
    int selected = aura_nav_get_selection(nav);
    const char *panel_icon;
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
        items[i].indent = 0;
        items[i].dimmed = 0;
        items[i].full_screen_target = 0;
        /* Idiomas sin traduccion: fila presente pero inerte. */
        items[i].dimmed = (screen == AURA_SCREEN_SETTINGS_LANGUAGE
                           && i >= LANGUAGE_AVAILABLE_N);
    }

    panel_icon = (selected >= 0 && selected < count && items[selected].icon_name)
        ? items[selected].icon_name
        : parent_settings_icon(screen);

    /* Ecualizador: el panel derecho muestra la CURVA del preset
     * resaltado (encargo 2026-08-13) en vez del icono generico -- la
     * pieza que el dueno del diseno investigo y pidio replicar. */
    if (screen == AURA_SCREEN_SETTINGS_EQ && aura_widgets_split_active())
    {
        a26_shell_clear_screen();
        aura_widgets_draw_status_bar(aura_str(screen_title_id(screen)));
        aura_menu_list_draw(0, A26_LAYOUT_STATUSBAR_HEIGHT, items, count, selected);
        if (selected >= 0 && selected < count)
            draw_eq_curve_panel(AURA_DS_METRICS_LEFT_PANEL_WIDTH,
                                 A26_SCREEN_WIDTH - AURA_DS_METRICS_LEFT_PANEL_WIDTH,
                                 selected);
        return;
    }

    draw_menu_screen_v2(aura_str(screen_title_id(screen)), items, count,
                         selected, panel_icon, NULL, NULL, NULL, NULL,
                         AURA_SCREEN_COUNT, AURA_SCREEN_COUNT, AURA_SS_BG_ACCENT_IMAGE);
}

/* D-289: pantalla "Estilo" -- misma maquinaria visual que
 * draw_choice_list() (MenuList + Selector, checkmark en la fila
 * activa, filas inertes atenuadas) pero con una tabla DINAMICA leida
 * del disco en vez de una tabla de aura_str_id_t compilada -- por eso
 * no vive dentro de is_choice_screen()/get_choice_table(), que asumen
 * texto compilado (aura_str()). aura_style_scan() ya deja los nombres
 * en un buffer propio (aura_style_entry_t.name) que sobrevive el resto
 * de esta funcion, asi que items[i].label puede apuntar directo ahi
 * sin copiar de nuevo. */
static void draw_style_list(aura_nav_t *nav)
{
    static aura_style_entry_t entries[AURA_STYLES_MAX];
    int count = aura_style_scan(entries, AURA_STYLES_MAX);
    aura_menu_item_v2_t items[MAX_MENU_ENTRIES];
    int selected = aura_nav_get_selection(nav);
    const char *active_id = aura_style_active_id();
    int i;

    if (count > MAX_MENU_ENTRIES)
        count = MAX_MENU_ENTRIES;

    for (i = 0; i < count; i++)
    {
        items[i].label = entries[i].name;
        items[i].icon_name = NULL;
        items[i].checked = !strcmp(entries[i].id, active_id);
        items[i].toggle = -1;
        items[i].indent = 0;
        items[i].full_screen_target = 0;
        items[i].dimmed = !entries[i].loadable;
    }

    draw_menu_screen_v2(aura_str(AURA_STR_SETTINGS_STYLE), items, count,
                         selected, "sync", NULL, NULL, NULL, NULL,
                         AURA_SCREEN_COUNT, AURA_SCREEN_COUNT, AURA_SS_BG_ACCENT_IMAGE);
}

/* Fila inerte (formato incompatible / manifiesto invalido / fuentes
 * faltantes): se puede recorrer, no elegir -- mismo criterio que los
 * idiomas sin traducir en handle_choice_list(). SELECT en la fila
 * "Aura" o en cualquier estilo cargable llama a
 * aura_style_activate(): solo si devuelve true (aplico de verdad, sin
 * revertir) se persiste en aura.cfg -- si el estilo fallara a mitad de
 * carga (disco removido, etc.) el ajuste guardado no miente sobre lo
 * que quedo activo (PLAN-themes-impl.md SS1.1/SS1.2). */
static void handle_style_list(aura_nav_t *nav, long button)
{
    static aura_style_entry_t entries[AURA_STYLES_MAX];
    int count = aura_style_scan(entries, AURA_STYLES_MAX);
    int sel = aura_nav_get_selection(nav);

    if (count > AURA_STYLES_MAX)
        count = AURA_STYLES_MAX;

    if (button == BUTTON_SELECT && (sel < 0 || sel >= count || !entries[sel].loadable))
        return;

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, count, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, count, -1));
        break;
    case BUTTON_SELECT:
        if (aura_style_activate(entries[sel].id))
        {
            strlcpy(aura_settings.style_id, entries[sel].id, sizeof(aura_settings.style_id));
            aura_settings_save();
        }
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
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
/* D-280: true = la proxima vez que se dibuje la pagina de almacenamiento
 * debe releer manifiesto + recorrido de /Music (recien se entro a la
 * pantalla o se acaba de re-entrar tras salir con Menu); false = ya se
 * recargo para esta visita, los cuadros siguientes usan el dato en cache. */
static bool s_about_needs_reload = true;

#define ABOUT_CONTENT_Y (A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_XXL)

/* Region animada por Fade-Slide entre las 3 paginas de "Acerca de"
 * (D-283 4/4, PLAN-about-fixes.md Q9): toda el area de contenido bajo
 * la StatusBar y sobre los puntos de paginacion -- mismo ancho angosto
 * que usan las 3 paginas (texto/barra en Almacenamiento y Conteos,
 * texto con scroll en Creditos), consistente para que la region nunca
 * cambie de forma al cambiar de pagina. */
#define ABOUT_PAGE_REGION_X AURA_DS_METRICS_ABOUT_EXPANDED_BAR_X
#define ABOUT_PAGE_REGION_W (AURA_DS_METRICS_ABOUT_EXPANDED_BAR_RIGHT - ABOUT_PAGE_REGION_X)
#define ABOUT_PAGE_REGION_Y A26_LAYOUT_STATUSBAR_HEIGHT
#define ABOUT_PAGE_REGION_H (A26_SCREEN_HEIGHT - A26_SPACING_XXL - ABOUT_PAGE_REGION_Y)

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
        /* Circulo, no cuadrado (AUDITORIA-01 A-22): radio = mitad del
         * lado, misma primitiva de corte por distancia que el resto del
         * sistema -- nada de cajas vivas (doc SS5.4/anti-patron SS8). */
        unsigned color = a26_color(i == s_about_page ? A26_ACCENT : A26_SHELL_RAIL);
        a26_shell_fill_rounded_rect(x + i * (dot + gap), y, dot, dot,
                                     dot / 2, color, a26_color(A26_SHELL_BG));
    }
}

/* Una fila de la pantalla expandida (D-279): punto de color + etiqueta
 * (DS_REG_12, secundaria) a la izquierda, cifra + porcentaje del disco
 * (DS_BOLD_12, primaria) alineada al borde derecho de `w` -- Q12: mismo
 * formato que ya usaba esta pantalla (output_dyn_value con byte_units,
 * MiB/GiB binarios), con el porcentaje que componentes/selection-summary.md
 * ya prometia ("con porcentaje real del disco", D-264) y que la version
 * anterior de esta pagina nunca calculaba. */
static void draw_about_storage_row(int x, int y, int w, unsigned dot_color,
                                    aura_str_id_t label_id,
                                    long long bytes, long long total_b)
{
    char size_buf[16], line[32], pct_buf[8];
    /* D-280 (2/2): un decimal bajo 10.0% -- entero truncado mostraba "0%"
     * para cualquier categoria por debajo del 1% del disco (el caso comun
     * con discos grandes: 119 MiB de musica es 0.0X% de un volumen de
     * cientos de GB). pct_x10 en decimas de punto porcentual, sin float. */
    long long pct_x10 = total_b > 0 ? (bytes * 1000) / total_b : 0;
    const int dot = A26_SPACING_SM + A26_SPACING_XS;
    int tw, th;
    /* D-282/Q4: mismo contorno de 1px que los segmentos de la barra
     * (draw_storage_segments()) -- el punto de color tiene el mismo
     * problema de contraste sobre SHELL_BG casi-blanco. */
    bool dark = (aura_settings.theme == AURA_THEME_DARK);
    unsigned outline_toward = dark ? LCD_RGBPACK(255, 255, 255) : LCD_RGBPACK(0, 0, 0);
    int outline_pct256 = (AURA_DS_METRICS_ABOUT_SEGMENT_OUTLINE_PCT * 256) / 100;
    unsigned dot_border = a26_shell_blend(dot_color, outline_toward, outline_pct256);

    a26_shell_outline_rounded_rect(x, y + (A26_TYPE_BODY - dot) / 2, dot, dot,
                                    dot / 2, dot_color, dot_border, a26_color(A26_SHELL_BG));

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_putsxy(x + dot + A26_SPACING_SM, y, (const unsigned char *)aura_str(label_id));

    output_dyn_value(size_buf, sizeof(size_buf), bytes, byte_units, 4, true);
    if (pct_x10 < 100)
        snprintf(pct_buf, sizeof(pct_buf), "%d.%d%%", (int)(pct_x10 / 10), (int)(pct_x10 % 10));
    else
        snprintf(pct_buf, sizeof(pct_buf), "%d%%", (int)(pct_x10 / 10));
    snprintf(line, sizeof(line), "%s \xC2\xB7 %s", size_buf, pct_buf);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
    lcd_getstringsize((const unsigned char *)line, &tw, &th);
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_putsxy(x + w - tw, y, (const unsigned char *)line);
}

/* Barra expandida sobre SHELL_BG plano (a diferencia de
 * draw_about_storage_bars(), que vive sobre la imagen del panel derecho
 * y necesita el snapshot/restore de D-277) -- mas simple, sin buffer de
 * respaldo, misma mascara de capsula que ya usaba esta pagina antes del
 * rediseño. */
static void draw_about_storage_bar_expanded(int x, int y, int width, int height,
                                             long long music_b, long long video_b,
                                             long long photo_b, long long system_b,
                                             long long other_b,
                                             long long free_b, long long total_b)
{
    draw_storage_segments(x, y, width, height, music_b, video_b, photo_b,
                          system_b, other_b, free_b, total_b);
    a26_shell_capsule_ends_over_content(x, y, width, height, a26_color(A26_SHELL_BG));
}

/* Pagina 1, FULL-CARRY (D-278/D-279 -- encargo del dueno: "el
 * SelectionSummary pasa a pantalla completa, con el icono de Aura
 * desplazandose a la izquierda y la barra expandiendose... en estado
 * expandido aparecen textos mostrando el espacio que ocupa cada tipo de
 * archivo"). Reemplaza la version anterior (D-081, barra de 3 colores
 * uniformes sin "Otros" -- limitacion resuelta ahora por
 * about_storage_collect(), D-279). El tile es el MISMO que viaja con
 * aura_transition_shift_and_reveal() (D-278): misma funcion de dibujo
 * (aura_selection_summary_draw_tile()) y mismo eje Y que en split
 * (aura_selection_summary_tile_rect_split()), solo cambia X -- el cuadro
 * final de la transicion tiene que coincidir pixel a pixel con esto. */
/* D-284: identidad de la unidad de almacenamiento, leida del ATA IDENTIFY
 * DEVICE real (storage_get_identify(): las mismas palabras que el menu de
 * depuracion de Rockbox ya usa -- modelo en 27..46, byte-swapped) mas la
 * palabra 217 "nominal media rotation rate": 0x0001 = medio no rotatorio
 * (SSD, iFlash, CF), 0x0401..0xFFFE = rpm de un disco duro, 0 = no
 * reportado. Encargo del dueno: "que detecte si tiene SSD o HDD y de ser
 * posible la marca". Si la unidad no reporta la palabra 217, se infiere
 * por el modelo solo cuando el nombre lo dice de forma inequivoca
 * ("SSD"/"FLASH"/"IFLASH"/"CF"), y si no, se muestra el tipo neutro --
 * nunca se adivina. En el simulador no hay ATA (storage_get_identify()
 * es NULL): se dice "Disco simulado", jamas un modelo inventado. */
static void about_drive_identity(char *model, size_t model_sz, aura_str_id_t *kind)
{
    unsigned short *id = storage_get_identify();
    int i;

    model[0] = '\0';
#ifdef SIMULATOR
    (void)id; (void)i; (void)model_sz;
    *kind = AURA_STR_ABOUT_DRIVE_SIMULATED;
    return;
#else
    if (!id)
    {
        *kind = AURA_STR_ABOUT_DRIVE_UNKNOWN;
        return;
    }
    for (i = 0; i < 20 && (size_t)(i * 2 + 2) < model_sz; i++)
    {
        model[i * 2]     = (char)(id[27 + i] >> 8);
        model[i * 2 + 1] = (char)(id[27 + i] & 0xFF);
    }
    model[i * 2 < (int)model_sz ? i * 2 : (int)model_sz - 1] = '\0';
    /* recorta espacios de relleno ATA al final y al inicio */
    {
        int len = (int)strlen(model);
        while (len > 0 && model[len - 1] == ' ') model[--len] = '\0';
        i = 0;
        while (model[i] == ' ') i++;
        if (i) memmove(model, model + i, strlen(model + i) + 1);
    }
    if (id[217] == 0x0001)
        *kind = AURA_STR_ABOUT_DRIVE_SSD;
    else if (id[217] >= 0x0401 && id[217] <= 0xFFFE)
        *kind = AURA_STR_ABOUT_DRIVE_HDD;
    else
    {
        char up[48];
        int n = 0;
        while (model[n] && n < (int)sizeof(up) - 1) { up[n] = toupper((unsigned char)model[n]); n++; }
        up[n] = '\0';
        if (strstr(up, "SSD") || strstr(up, "FLASH") || strstr(up, "IFLASH") || strstr(up, "CF "))
            *kind = AURA_STR_ABOUT_DRIVE_SSD;
        else
            *kind = AURA_STR_ABOUT_DRIVE_UNKNOWN;
    }
#endif
}

/* Linea de identidad en la parte superior de la pagina de almacenamiento:
 * "<tipo> · <modelo> · <capacidad>". El modelo se recorta con "…" para
 * caber en el ancho de la region si hace falta (los strings ATA llegan a
 * 40 caracteres); tipo y capacidad nunca se recortan. */
static void draw_about_drive_line(int x, int y, int w, long long total_b)
{
    char model[48], cap[16], line[96];
    aura_str_id_t kind;
    int tw, th;

    about_drive_identity(model, sizeof(model), &kind);
    output_dyn_value(cap, sizeof(cap), total_b, byte_units, 4, true);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));

    if (model[0])
    {
        int model_len = (int)strlen(model);
        for (;;)
        {
            snprintf(line, sizeof(line), "%s · %s%s · %s", aura_str(kind), model,
                     model_len < (int)strlen(model) ? "…" : "", cap);
            lcd_getstringsize((const unsigned char *)line, &tw, &th);
            if (tw <= w || model_len <= 3)
                break;
            model[--model_len] = '\0';
        }
    }
    else
        snprintf(line, sizeof(line), "%s · %s", aura_str(kind), cap);

    lcd_putsxy(x, y, (const unsigned char *)line);
}

static void draw_about_storage_expanded(void)
{
    long long music_b, video_b, photo_b, system_b, other_b, free_b, total_b;
    aura_category_t cat = aura_category_for_screen(AURA_SCREEN_SETTINGS_ABOUT);
    int split_x, tile_y, tile_w, tile_h;
    int text_x = AURA_DS_METRICS_ABOUT_EXPANDED_BAR_X;
    int text_w = AURA_DS_METRICS_ABOUT_EXPANDED_BAR_RIGHT - text_x;
    int bar_x = AURA_DS_METRICS_ABOUT_EXPANDED_BAR_X;
    int bar_w = AURA_DS_METRICS_ABOUT_EXPANDED_BAR_RIGHT - bar_x;
    int bar_h = AURA_DS_METRICS_ABOUT_BAR_H;
    int row_h, y, bar_y, region_top;
    int i;
    /* D-282: 5 filas -- Libre no lleva fila de texto propia (es el
     * "resto" implicito de la barra, igual que antes), pero Sistema si,
     * separada de Otros. */
    struct { long long bytes; unsigned color; aura_str_id_t label; } rows[5];

    /* Q1/D-280: recarga fresca del manifiesto (no el cache de una vez por
     * sesion que usa el bottom_renderer de split) -- pero SOLO al entrar
     * a este estado, no en cada cuadro (bug real encontrado en revision:
     * esta pantalla se redibuja ~20 veces por segundo mientras esta
     * visible, y antes de este arreglo relenia sync_summary.cfg Y
     * recorria /Music, /Videos, /Photos en cada uno de esos cuadros --
     * barato en el simulador, pero exactamente el patron que D-280/C2
     * identifico como el unico costo real si se ejecuta sin control).
     * s_about_needs_reload la pone en true handle_about() al SALIR
     * (BUTTON_MENU) para que la proxima entrada si relea. */
    about_storage_collect(s_about_needs_reload, &music_b, &video_b, &photo_b, &system_b,
                          &other_b, &free_b, &total_b);
    s_about_needs_reload = false;

    aura_selection_summary_tile_rect_split(&split_x, &tile_y, &tile_w, &tile_h);
    aura_selection_summary_draw_tile(AURA_DS_METRICS_ABOUT_EXPANDED_TILE_X, tile_y,
                                     cat, NULL, draw_about_icon_renderer);

    rows[0].bytes = music_b; rows[0].color = aura_accent();
    rows[0].label = AURA_STR_ABOUT_MUSIC;
    rows[1].bytes = video_b; rows[1].color = category_flat_color(AURA_CATEGORY_VIDEO);
    rows[1].label = AURA_STR_ABOUT_VIDEOS;
    rows[2].bytes = photo_b; rows[2].color = category_flat_color(AURA_CATEGORY_PHOTOS);
    rows[2].label = AURA_STR_ABOUT_PHOTOS;
    rows[3].bytes = system_b; rows[3].color = category_flat_color(AURA_CATEGORY_SETTINGS);
    rows[3].label = AURA_STR_ABOUT_SYSTEM;
    rows[4].bytes = other_b; rows[4].color = category_extras_yellow_flat();
    rows[4].label = AURA_STR_ABOUT_OTHER;

    /* D-284: identidad de la unidad arriba de las filas. */
    draw_about_drive_line(text_x, AURA_DS_METRICS_ABOUT_EXPANDED_DRIVE_LINE_Y, text_w, total_b);

    row_h = (AURA_DS_METRICS_ABOUT_EXPANDED_TEXT_BOTTOM_Y
             - AURA_DS_METRICS_ABOUT_EXPANDED_TEXT_TOP_Y) / 5;
    y = AURA_DS_METRICS_ABOUT_EXPANDED_TEXT_TOP_Y;
    for (i = 0; i < 5; i++)
    {
        draw_about_storage_row(text_x, y, text_w, rows[i].color, rows[i].label,
                               rows[i].bytes, total_b);
        y += row_h;
    }

    /* Mismo eje vertical que en split (Q11): centrada en el margen
     * inferior del tile, misma formula que D-272/D-279 usan para el
     * texto/barra en el panel derecho. */
    region_top = tile_y + tile_h;
    bar_y = region_top + (A26_SCREEN_HEIGHT - region_top - bar_h) / 2;
    draw_about_storage_bar_expanded(bar_x, bar_y, bar_w, bar_h,
                                    music_b, video_b, photo_b, system_b,
                                    other_b, free_b, total_b);
}

/* Q8 (PLAN-about-fixes.md): el tile de Aura persiste en las 3 paginas de
 * "Acerca de", no solo en Almacenamiento -- continuidad del viaje del
 * Shift-and-Reveal (D-278). Mismo x/y que ahi; cada pagina define su
 * propia region de texto a la derecha con las mismas constantes
 * about.expanded_bar_x/expanded_text_top_y/expanded_text_bottom_y. */
static void draw_about_persistent_tile(void)
{
    aura_category_t cat = aura_category_for_screen(AURA_SCREEN_SETTINGS_ABOUT);
    int split_x, tile_y, tile_w, tile_h;

    aura_selection_summary_tile_rect_split(&split_x, &tile_y, &tile_w, &tile_h);
    aura_selection_summary_draw_tile(AURA_DS_METRICS_ABOUT_EXPANDED_TILE_X, tile_y,
                                     cat, NULL, draw_about_icon_renderer);
}

/* Pagina 2, conteos detallados (D-283, encargo del dueno): Musica ya
 * era viable con lo que Aura ya tenia -- canciones/listas del
 * manifiesto (mismo dato de siempre), artistas contados EN VIVO de la
 * base de datos de Rockbox (aura_music_count_artists()) porque el
 * manifiesto nunca tuvo ese campo y tagcache ya lo sabe exacto, mas
 * fresco que cualquier sync. Video/Fotos necesitaban un cambio en Aura
 * Studio primero (Rockbox no tiene DB de video ni parser EXIF, PLAN-
 * about-fixes.md E2) -- Studio ya escribe los conteos por categoria
 * (D-283, LibrarySync.swift); si el manifiesto es de un sync ANTERIOR a
 * esa sesion (has_video_categories/has_photo_categories en false), se
 * muestra el aviso de sincronizar en vez de un "0" enganoso que se leeria
 * como "no tienes nada" en vez de "no lo sabemos todavia". */
/* Pagina 2 (Estado 2, D-283) rediseñada en D-284 (encargo del dueno:
 * "primero la seccion de musica, con el mismo esquema de colores y los
 * mismos iconos de los menus, luego videos, despues fotos"): tres
 * SECCIONES, cada una con el icono de 14px del menu principal ("music"/
 * "video"/"image", los mismos assets que dibuja LeftPanel) tenido con el
 * color plano de su categoria (D-250: Musica = acento, Video = navy,
 * Fotos = naranja) y el titulo en ese mismo color, y debajo las lineas
 * de conteo en la tinta normal. Musica gana "Albumes" (tagcache, mismo
 * mecanismo que Artistas). El orden ya era Musica/Videos/Fotos, pero sin
 * encabezados se leia como una sola lista sin estructura. */
static int draw_about_counts_header(int x, int y, const char *icon,
                                    aura_category_t cat, aura_str_id_t title)
{
    unsigned color = (cat == AURA_CATEGORY_MUSIC) ? aura_accent() : category_flat_color(cat);
    int isz = AURA_DS_METRICS_ABOUT_COUNTS_ICON_SIZE;
    int tw, text_h;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
    lcd_getstringsize((const unsigned char *)"Ag", &tw, &text_h);
    /* icono centrado verticalmente respecto a la linea de titulo */
    aura_widgets_draw_icon_ink(icon, isz, x, y + (text_h - isz) / 2, color, 256);
    lcd_set_foreground(color);
    lcd_putsxy(x + isz + A26_SPACING_SM, y, (const unsigned char *)aura_str(title));
    (void)tw;
    return text_h;
}

static int draw_about_counts_line(int x, int y, aura_str_id_t label, int value)
{
    char line[48];
    int tw, th;
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    snprintf(line, sizeof(line), "%s: %d", aura_str(label), value);
    lcd_putsxy(x, y, (const unsigned char *)line);
    lcd_getstringsize((const unsigned char *)"Ag", &tw, &th);
    return th + 1;
}

static int draw_about_counts_note(int x, int y)
{
    int tw, th;
    (void)tw;
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_putsxy(x, y, (const unsigned char *)aura_str(AURA_STR_ABOUT_SYNC_FOR_DETAIL));
    lcd_getstringsize((const unsigned char *)"Ag", &tw, &th);
    return th + 1;
}

static void draw_about_counts(const aura_manifest_t *m)
{
    int x = AURA_DS_METRICS_ABOUT_EXPANDED_BAR_X;
    /* Arranca en la misma altura que la linea de identidad de la pagina 1
     * (no en expanded_text_top_y): tres secciones con encabezado necesitan
     * todo el alto disponible entre la StatusBar y los puntos de pagina. */
    int y = AURA_DS_METRICS_ABOUT_EXPANDED_DRIVE_LINE_Y;
    int gap = AURA_DS_METRICS_ABOUT_COUNTS_GROUP_GAP;
    /* Titulos con sangria del icono para que las lineas alineen con el
     * texto del encabezado, no con el icono. */
    int lx = x + AURA_DS_METRICS_ABOUT_COUNTS_ICON_SIZE + A26_SPACING_SM;

    draw_about_persistent_tile();

    /* -- Musica -- */
    y += draw_about_counts_header(x, y, "music", AURA_CATEGORY_MUSIC, AURA_STR_MUSIC);
    y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_SONGS, m->music_count);
    y += draw_about_counts_line(lx, y, AURA_STR_MUSIC_ALBUMS, aura_music_count_albums());
    y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_ARTISTS, aura_music_count_artists());
    y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_PLAYLISTS, m->playlist_count);
    y += gap;

    /* -- Videos -- */
    y += draw_about_counts_header(x, y, "video", AURA_CATEGORY_VIDEO, AURA_STR_VIDEOS);
    if (m->has_video_categories)
    {
        y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_MOVIES, m->video_movies_count);
        y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_SERIES, m->video_series_count);
        y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_CLIPS, m->video_clips_count);
    }
    else
        y += draw_about_counts_note(lx, y);
    y += gap;

    /* -- Fotos -- */
    y += draw_about_counts_header(x, y, "image", AURA_CATEGORY_PHOTOS, AURA_STR_PHOTOS);
    if (m->has_photo_categories)
    {
        y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_IMAGES, m->photo_images_count);
        y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_PHOTOS_TAKEN, m->photo_photos_count);
        y += draw_about_counts_line(lx, y, AURA_STR_ABOUT_AI, m->photo_ai_count);
    }
    else
        y += draw_about_counts_note(lx, y);
}

/* Pagina 3: Estado 3 "Creditos" (D-283, PLAN-about-fixes.md Q7/Q8/Q9) --
 * reemplaza el resumen de una sola linea ("Basado en Rockbox") por el
 * texto completo que la GPL v2 exige (atribucion + URL del codigo
 * fuente, GPL v2 SS3) mas la nota de marca de Apple sin afiliacion.
 * AUDITORIA-01 A-10/A-c ya habia retirado `rbversion` crudo de aqui
 * (jerga de build, no version real de Aura) -- eso se conserva, este
 * cambio es aparte.
 *
 * El tile persiste (Q8, draw_about_persistent_tile(), igual que en la
 * pagina de Conteos) asi que el texto vive en la columna angosta de la
 * region derecha (about.expanded_bar_x..bar_right, 182px) -- a ese
 * ancho el texto NO cabe completo (mas de 18 lineas con DS_REG_12
 * contra los ~13 que caben entre expanded_text_top_y y bottom_y), asi
 * que se reutiliza el mismo patron de scroll por rueda que ya usa
 * Avisos legales (draw_long_text()/handle_legal_text()) en vez de
 * inventar uno nuevo, adaptado a la region angosta con tile en vez de
 * pantalla completa. */
static int s_credits_scroll = 0;

static void draw_about_credits(void)
{
    const char *body = aura_str(AURA_STR_ABOUT_CREDITS_BODY);
    const char *lines[48];
    int lens[48];
    int text_x = AURA_DS_METRICS_ABOUT_EXPANDED_BAR_X;
    int box_w = AURA_DS_METRICS_ABOUT_EXPANDED_BAR_RIGHT - text_x;
    int top_y = AURA_DS_METRICS_ABOUT_EXPANDED_TEXT_TOP_Y;
    int bottom_y = AURA_DS_METRICS_ABOUT_EXPANDED_TEXT_BOTTOM_Y;
    int line_h, tw, th, n, i, y, visible;

    draw_about_persistent_tile();

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)"Ag", &tw, &th);
    line_h = th + A26_SPACING_XS;
    visible = (bottom_y - top_y) / line_h;

    n = aura_widgets_wrap_text(body, box_w, lines, lens, 48);
    if (s_credits_scroll > n - visible)
        s_credits_scroll = (n > visible) ? n - visible : 0;
    if (s_credits_scroll < 0)
        s_credits_scroll = 0;

    y = top_y;
    for (i = s_credits_scroll; i < n && i < s_credits_scroll + visible; i++)
    {
        char buf[64];
        int len = lens[i];

        if (len > (int)sizeof(buf) - 1)
            len = (int)sizeof(buf) - 1;
        memcpy(buf, lines[i], len);
        buf[len] = '\0';
        lcd_putsxy(text_x, y, (const unsigned char *)buf);
        y += line_h;
    }
}

static void draw_about(void)
{
    aura_manifest_t manifest;
    bool has_manifest;

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_SETTINGS_ABOUT));

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
    case ABOUT_PAGE_STORAGE: draw_about_storage_expanded(); break;
    case ABOUT_PAGE_COUNTS:  draw_about_counts(&manifest); break;
    case ABOUT_PAGE_DEVICE:  draw_about_credits(); break;
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
        {
            s_about_page++;
            s_credits_scroll = 0;
            /* Fade-Slide DESPUES de actualizar s_about_page -- la
             * transicion prerrenderiza el DESTINO llamando
             * aura_screens_draw(nav), que lee s_about_page para decidir
             * que pagina dibujar (mismo contrato que shift-and-reveal). */
            aura_transition_fade_slide_region(nav, ABOUT_PAGE_REGION_X, ABOUT_PAGE_REGION_Y,
                                              ABOUT_PAGE_REGION_W, ABOUT_PAGE_REGION_H, 1);
        }
        break;
    case BUTTON_LEFT:
        if (s_about_page > 0)
        {
            s_about_page--;
            s_credits_scroll = 0;
            aura_transition_fade_slide_region(nav, ABOUT_PAGE_REGION_X, ABOUT_PAGE_REGION_Y,
                                              ABOUT_PAGE_REGION_W, ABOUT_PAGE_REGION_H, -1);
        }
        break;
    /* Scroll por rueda dentro de la pagina de Creditos (Q8): mismo
     * patron que handle_legal_text(). No interfiere con SELECT/RIGHT/
     * LEFT, que siguen cambiando de pagina -- la rueda solo desplaza
     * texto DENTRO de la pagina 3. */
    case BUTTON_SCROLL_FWD:
        if (s_about_page == ABOUT_PAGE_DEVICE)
            s_credits_scroll++;
        break;
    case BUTTON_SCROLL_BACK:
        if (s_about_page == ABOUT_PAGE_DEVICE)
            s_credits_scroll--;
        break;
    case BUTTON_MENU:
        /* AUDITORIA-01 A-09: FULL-COLD "sin memoria" (doc de
         * comportamiento SS1) -- reiniciar al SALIR, no intentar
         * detectar "se volvio a entrar" con una variable centinela que
         * nunca cambiaba de valor una vez puesta (bug real: solo
         * funcionaba la primera vez que se dibujaba en todo el proceso). */
        s_about_page = ABOUT_PAGE_STORAGE;
        s_about_needs_reload = true;
        s_credits_scroll = 0;
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
    aura_menu_item_v2_t items[BACKLIGHT_VALUES_N];
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
        items[i].indent = 0;
        items[i].dimmed = 0;
        items[i].full_screen_target = 0;
    }
    draw_menu_screen_v2(aura_str(AURA_STR_SETTINGS_BACKLIGHT), items, BACKLIGHT_VALUES_N,
                         aura_nav_get_selection(nav),
                         parent_settings_icon(AURA_SCREEN_SETTINGS_BACKLIGHT), NULL, NULL,
                         NULL, NULL,
                         AURA_SCREEN_COUNT, AURA_SCREEN_COUNT, AURA_SS_BG_ACCENT_IMAGE);
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
    aura_menu_item_v2_t items[SLEEPTIMER_VALUES_N];
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
        items[i].indent = 0;
        items[i].dimmed = 0;
        items[i].full_screen_target = 0;
    }
    draw_menu_screen_v2(aura_str(AURA_STR_SETTINGS_SLEEPTIMER), items, SLEEPTIMER_VALUES_N,
                         aura_nav_get_selection(nav),
                         parent_settings_icon(AURA_SCREEN_SETTINGS_SLEEPTIMER), NULL, NULL,
                         NULL, NULL,
                         AURA_SCREEN_COUNT, AURA_SCREEN_COUNT, AURA_SS_BG_ACCENT_IMAGE);
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

/* -- Menu principal configurable (jerarquia del original) --------------
 *
 * No es una lista plana: los PADRES van al margen normal y sus HIJOS
 * indentados debajo (referencia del aparato real, 2026-08-13). Un hijo
 * marcado aparece ADEMAS en el menu de inicio sin dejar de vivir dentro
 * de su padre -- la sangria es lo que comunica esa pertenencia y lo que
 * divide visualmente el listado. */
typedef struct {
    aura_str_id_t label;
    const char *icon;
    int indent;
    aura_screen_id_t target;   /* AURA_SCREEN_COUNT = fila de accion */
} mainmenu_row_t;


static const mainmenu_row_t mainmenu_rows[] = {
    { AURA_STR_MUSIC,           "music",           0, AURA_SCREEN_MUSIC },
    { AURA_STR_MUSIC_COVERFLOW, "square-on-square", 1, AURA_SCREEN_MUSIC_COVERFLOW },
    { AURA_STR_MUSIC_PLAYLISTS, "playlist",        1, AURA_SCREEN_MUSIC_PLAYLISTS },
    { AURA_STR_MUSIC_ARTISTS,   "artist",          1, AURA_SCREEN_MUSIC_ARTISTS },
    { AURA_STR_MUSIC_ALBUMS,    "album",           1, AURA_SCREEN_MUSIC_ALBUMS },
    { AURA_STR_MUSIC_SONGS,     "song",            1, AURA_SCREEN_MUSIC_SONGS },
    { AURA_STR_MUSIC_GENRES,    "genre",           1, AURA_SCREEN_MUSIC_GENRES },
    { AURA_STR_VIDEOS,          "video",           0, AURA_SCREEN_VIDEOS },
    { AURA_STR_PHOTOS,          "image",           0, AURA_SCREEN_PHOTOS },
    { AURA_STR_EXTRAS,          "extras",          0, AURA_SCREEN_EXTRAS },
    { AURA_STR_NOWPLAYING,      "play",            0, AURA_SCREEN_NOWPLAYING },
    { AURA_STR_MAINMENU_RESTORE, "reset",          0, AURA_SCREEN_COUNT },
};
#define MAINMENU_ROWS ((int)(sizeof(mainmenu_rows) / sizeof(mainmenu_rows[0])))

/* Marcado actual de cada fila. Musica y Extras son fijos (siempre en el
 * menu de inicio, como en el original); el resto es configurable. */
static int mainmenu_row_checked(const mainmenu_row_t *r)
{
    int bit;

    switch (r->target)
    {
    case AURA_SCREEN_MUSIC:
    case AURA_SCREEN_EXTRAS:      return 1;
    case AURA_SCREEN_VIDEOS:      return aura_settings.show_videos;
    case AURA_SCREEN_PHOTOS:      return aura_settings.show_photos;
    case AURA_SCREEN_NOWPLAYING:  return aura_settings.show_nowplaying;
    case AURA_SCREEN_COUNT:       return 0;
    default:
        bit = aura_screens_root_shortcut_bit(r->target);
        return (bit >= 0)
            && (aura_settings.root_shortcuts & (1u << bit)) != 0;
    }
}

static void draw_mainmenu(aura_nav_t *nav)
{
    aura_menu_item_v2_t items[MAINMENU_ROWS];
    int sel = aura_nav_get_selection(nav);
    int i;

    for (i = 0; i < MAINMENU_ROWS; i++)
    {
        items[i].label = aura_str(mainmenu_rows[i].label);
        items[i].icon_name = mainmenu_rows[i].icon;
        items[i].indent = mainmenu_rows[i].indent;
        items[i].checked = mainmenu_row_checked(&mainmenu_rows[i]);
        items[i].toggle = -1;
        items[i].full_screen_target = 0;
        /* Musica y Extras no se pueden quitar del menu de inicio. */
        items[i].dimmed = (mainmenu_rows[i].target == AURA_SCREEN_MUSIC
                           || mainmenu_rows[i].target == AURA_SCREEN_EXTRAS);
    }

    draw_menu_screen_v2(aura_str(AURA_STR_SETTINGS_MAINMENU), items, MAINMENU_ROWS,
                         sel,
                         (sel >= 0 && sel < MAINMENU_ROWS) ? items[sel].icon_name
                                                            : "menu-list", NULL, NULL,
                         NULL, NULL,
                         AURA_SCREEN_COUNT, AURA_SCREEN_COUNT, AURA_SS_BG_ACCENT_IMAGE);
}

static void handle_mainmenu(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);
    const mainmenu_row_t *r;
    int bit;

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, MAINMENU_ROWS, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, MAINMENU_ROWS, -1));
        break;
    case BUTTON_SELECT:
        r = &mainmenu_rows[sel];
        switch (r->target)
        {
        case AURA_SCREEN_MUSIC:
        case AURA_SCREEN_EXTRAS:
            break; /* fijos */
        case AURA_SCREEN_VIDEOS:
            aura_settings.show_videos = !aura_settings.show_videos; break;
        case AURA_SCREEN_PHOTOS:
            aura_settings.show_photos = !aura_settings.show_photos; break;
        case AURA_SCREEN_NOWPLAYING:
            aura_settings.show_nowplaying = !aura_settings.show_nowplaying; break;
        case AURA_SCREEN_COUNT:
            /* Restaurar menu principal: el estado de fabrica. */
            aura_settings.show_videos = true;
            aura_settings.show_photos = true;
            aura_settings.show_nowplaying = true;
            aura_settings.root_shortcuts = 0;
            break;
        default:
            bit = aura_screens_root_shortcut_bit(r->target);
            if (bit >= 0)
                aura_settings.root_shortcuts ^= (1u << bit);
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

/* Pantalla de texto largo desplazable (Avisos legales, 2026-08-13):
 * pantalla completa, envuelve por palabras al ancho util y la rueda
 * desplaza por lineas. Sin dependencias nuevas -- reusa el mismo
 * wrap_text() de los confirmadores. */
#define LEGAL_LINE_H 12
static int s_legal_scroll = 0;

static void draw_long_text(aura_str_id_t title_id, aura_str_id_t body_id)
{
    const char *body = aura_str(body_id);
    const char *lines[64];
    int lens[64];
    int box_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_LG;
    int visible = (A26_SCREEN_HEIGHT - A26_LAYOUT_STATUSBAR_HEIGHT
                   - A26_SPACING_MD) / LEGAL_LINE_H;
    int n, i, y;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(title_id));

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));

    n = aura_widgets_wrap_text(body, box_w, lines, lens, 64);
    if (s_legal_scroll > n - visible)
        s_legal_scroll = (n > visible) ? n - visible : 0;
    if (s_legal_scroll < 0)
        s_legal_scroll = 0;

    y = A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_SM;
    for (i = s_legal_scroll; i < n && i < s_legal_scroll + visible; i++)
    {
        char buf[128];
        int len = lens[i];

        if (len > (int)sizeof(buf) - 1)
            len = (int)sizeof(buf) - 1;
        memcpy(buf, lines[i], len);
        buf[len] = '\0';
        lcd_putsxy(A26_SPACING_LG, y, (const unsigned char *)buf);
        y += LEGAL_LINE_H;
    }
}

static void handle_legal_text(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:  s_legal_scroll++; break;
    case BUTTON_SCROLL_BACK: s_legal_scroll--; break;
    case BUTTON_MENU:
        s_legal_scroll = 0;
        aura_nav_pop(nav);
        break;
    default: break;
    }
}

/* Audiolibros (original): la pregunta y, debajo, las tres velocidades
 * en una fila -- la elegida en acento. */
static void draw_audiobooks(aura_nav_t *nav)
{
    static const aura_str_id_t opts[3] = {
        AURA_STR_SPEED_SLOW, AURA_STR_SPEED_NORMAL, AURA_STR_SPEED_FAST,
    };
    const char *lines[16];
    int lens[16];
    int box_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    int sel = aura_nav_get_selection(nav);
    int n, i, y, total_w = 0, x;
    int widths[3];

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_SETTINGS_AUDIOBOOKS));

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    n = aura_widgets_wrap_text(aura_str(AURA_STR_AUDIOBOOKS_BODY), box_w,
                                lines, lens, 16);
    y = A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_XXL;
    for (i = 0; i < n; i++)
    {
        char buf[128];
        int len = lens[i] < (int)sizeof(buf) - 1 ? lens[i] : (int)sizeof(buf) - 1;
        memcpy(buf, lines[i], len);
        buf[len] = '\0';
        lcd_putsxy(A26_SPACING_XXL, y, (const unsigned char *)buf);
        y += 15;
    }

    /* Fila de opciones, centrada. */
    y += A26_SPACING_XXL;
    for (i = 0; i < 3; i++)
    {
        int w, h;
        lcd_getstringsize((const unsigned char *)aura_str(opts[i]), &w, &h);
        widths[i] = w;
        total_w += w + (i < 2 ? A26_SPACING_XXL : 0);
    }
    x = (A26_SCREEN_WIDTH - total_w) / 2;
    for (i = 0; i < 3; i++)
    {
        lcd_set_foreground(i == sel ? aura_accent()
                                     : a26_color(A26_TEXT_PRIMARY));
        lcd_putsxy(x, y, (const unsigned char *)aura_str(opts[i]));
        x += widths[i] + A26_SPACING_XXL;
    }
}

static void handle_audiobooks(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:  aura_nav_set_selection(nav, aura_wheel_advance(sel, 3, 1)); break;
    case BUTTON_SCROLL_BACK: aura_nav_set_selection(nav, aura_wheel_advance(sel, 3, -1)); break;
    case BUTTON_SELECT:
        aura_settings.audiobook_speed = sel;
        aura_settings_save();
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU: aura_nav_pop(nav); break;
    default: break;
    }
}

/* Juegos: lista propia (no nav_entry_t generico) porque la fila 0
 * LANZA un plugin en vez de navegar. */
static void draw_games(aura_nav_t *nav)
{
    static aura_list_item_t items[3];
    static const aura_str_id_t labels[3] = {
        AURA_STR_GAME_KLONDIKE, AURA_STR_GAME_IQUIZ, AURA_STR_GAME_VORTEX,
    };
    int i;

    for (i = 0; i < 3; i++)
    {
        items[i].label = aura_str(labels[i]);
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
        items[i].dimmed = 0;
        /* Solo Klondike (fila 0) existe: es el plugin solitaire de
         * Rockbox. iQuiz y Vortex quedan INERTES -- los originales son
         * inviables (DRM FairPlay por dispositivo + RetailOS) y no hay
         * equivalente todavia. */
        items[i].dimmed = (i != 0);
    }
    aura_widgets_draw_list(aura_str(AURA_STR_EXTRAS_GAMES), items, 3,
                            aura_nav_get_selection(nav));
}

static void handle_games(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:  aura_nav_set_selection(nav, aura_wheel_advance(sel, 3, 1)); break;
    case BUTTON_SCROLL_BACK: aura_nav_set_selection(nav, aura_wheel_advance(sel, 3, -1)); break;
    case BUTTON_SELECT:
        /* Solo Klondike existe hoy: es el plugin solitaire de Rockbox.
         * plugin_load() restaura pantalla/viewport/fuente al volver, el
         * siguiente ciclo redibuja esta pantalla sin pasos extra (mismo
         * camino que el reproductor de video). */
        if (sel == 0)
            plugin_load(PLUGIN_GAMES_DIR "/solitaire.rock", NULL);
        break;
    case BUTTON_MENU: aura_nav_pop(nav); break;
    default: break;
    }
}

/* -- Editores de Fecha y Hora del sistema (encargo 2026-08-13) -----------
 *
 * "Fecha: calendario que muestra la configuracion natural y se modifica
 * junto con la configuracion numerica seleccionada" / "Hora: reloj
 * analogico que mueve las manecillas junto con la configuracion" --
 * mismo lenguaje visual que ya construimos para Calendarios (rejilla
 * del mes) y Alarmas (reloj analogico con la LUT de senos de
 * aura_flow), reaplicado aqui para AJUSTAR el reloj real del sistema en
 * vez de solo mostrarlo. Persisten via rtc_write_datetime() bajo
 * `#if CONFIG_RTC` -- real en hardware (ipod6g), 0 en el simulador (que
 * refleja el reloj del host y no tiene RTC propio que escribir: el
 * editor funciona igual, solo que "Aplicar" no tiene adonde persistir
 * ahi, comportamiento honesto, no un bug). */

static int s_de_day = 1, s_de_month = 0, s_de_year = 2026; /* month 0-11 */
static int s_de_field = 0; /* 0 dia, 1 mes, 2 anio */
static bool s_de_inited = false;

static const char *const DATEEDIT_MONTHS_ES[12] = {
    "Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
    "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre",
};
static const char *const DATEEDIT_DOW_ES[7] = { "L", "M", "M", "J", "V", "S", "D" };

static int dateedit_days_in_month(int year, int month)
{
    static const int base[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    return (month == 1 && leap) ? 29 : base[month];
}

/* Zeller adaptado a semana-desde-lunes (mismo algoritmo que Calendarios,
 * duplicado aqui a proposito -- 10 lineas, no vale la pena acoplar dos
 * pantallas independientes por eso). */
static int dateedit_first_dow(int year, int month)
{
    int m = month + 1, y = year, k, j, h;
    if (m < 3) { m += 12; y--; }
    k = y % 100; j = y / 100;
    h = (1 + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
    return (h + 5) % 7;
}

static void dateedit_ensure_init(void)
{
    struct tm *now;
    if (s_de_inited)
        return;
    now = get_time();
    if (now)
    {
        s_de_year = now->tm_year + 1900;
        s_de_month = now->tm_mon;
        s_de_day = now->tm_mday;
    }
    s_de_field = 0;
    s_de_inited = true;
}

static void draw_date_edit(void)
{
    char title[32], natural[64];
    int total = dateedit_days_in_month(s_de_year, s_de_month);
    int start = dateedit_first_dow(s_de_year, s_de_month);
    int cal_top = A26_LAYOUT_STATUSBAR_HEIGHT + 34;
    int cell_w = A26_SCREEN_WIDTH / 7;
    int cell_h = 24;
    int i, w, h;

    dateedit_ensure_init();
    if (s_de_day > total)
        s_de_day = total;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_SETTINGS_DATE));

    snprintf(title, sizeof(title), "%s %d", DATEEDIT_MONTHS_ES[s_de_month], s_de_year);
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
    lcd_set_foreground(s_de_field == 1 || s_de_field == 2 ? aura_accent()
                                                            : a26_color(A26_TEXT_PRIMARY));
    lcd_getstringsize((const unsigned char *)title, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, A26_LAYOUT_STATUSBAR_HEIGHT + 2,
               (const unsigned char *)title);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_8));
    lcd_set_foreground(a26_color(A26_TEXT_TERTIARY));
    for (i = 0; i < 7; i++)
    {
        lcd_getstringsize((const unsigned char *)DATEEDIT_DOW_ES[i], &w, &h);
        lcd_putsxy(i * cell_w + (cell_w - w) / 2, cal_top - 12,
                   (const unsigned char *)DATEEDIT_DOW_ES[i]);
    }

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    for (i = 0; i < total; i++)
    {
        int cell = start + i;
        int col = cell % 7, row = cell / 7;
        int x = col * cell_w, y = cal_top + row * cell_h;
        char num[4];
        bool sel = (i + 1 == s_de_day);

        if (row >= 6)
            break;
        if (sel)
            a26_shell_fill_rounded_rect(x + 3, y, cell_w - 6, cell_h - 4,
                                         A26_LAYOUT_CORNER_RADIUS_PILL,
                                         a26_color(A26_SELECTION_FILL),
                                         a26_color(A26_SHELL_BG));
        snprintf(num, sizeof(num), "%d", i + 1);
        lcd_set_foreground((sel && s_de_field == 0) ? aura_accent()
                            : a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)num, &w, &h);
        lcd_putsxy(x + (cell_w - w) / 2, y + (cell_h - 4 - h) / 2,
                   (const unsigned char *)num);
    }

    /* Fecha en espanol natural, crece/cambia en vivo con el campo que
     * se este ajustando. */
    snprintf(natural, sizeof(natural), "%d de %s de %d",
             s_de_day, DATEEDIT_MONTHS_ES[s_de_month], s_de_year);
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)natural, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, cal_top + 6 * cell_h + A26_SPACING_SM,
               (const unsigned char *)natural);
}

static void date_edit_commit(void)
{
#if CONFIG_RTC
    struct tm tm;
    struct tm *now = get_time();

    if (now)
        tm = *now;
    else
        memset(&tm, 0, sizeof(tm));
    tm.tm_year = s_de_year - 1900;
    tm.tm_mon  = s_de_month;
    tm.tm_mday = s_de_day;
    rtc_write_datetime(&tm);
#endif
}

static void handle_date_edit(aura_nav_t *nav, long button)
{
    int total;

    dateedit_ensure_init();
    total = dateedit_days_in_month(s_de_year, s_de_month);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (s_de_field == 0)
            s_de_day = s_de_day % total + 1;
        else if (s_de_field == 1)
        {
            s_de_month = (s_de_month + 1) % 12;
            total = dateedit_days_in_month(s_de_year, s_de_month);
            if (s_de_day > total) s_de_day = total;
        }
        else
            s_de_year++;
        break;
    case BUTTON_SCROLL_BACK:
        if (s_de_field == 0)
            s_de_day = (s_de_day - 2 + total) % total + 1;
        else if (s_de_field == 1)
        {
            s_de_month = (s_de_month + 11) % 12;
            total = dateedit_days_in_month(s_de_year, s_de_month);
            if (s_de_day > total) s_de_day = total;
        }
        else if (s_de_year > 2000)
            s_de_year--;
        break;
    case BUTTON_SELECT:
        if (s_de_field < 2)
            s_de_field++;
        else
        {
            date_edit_commit();
            s_de_inited = false;
            aura_nav_pop(nav);
        }
        break;
    case BUTTON_MENU:
        s_de_inited = false;
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

/* -- Editor de Hora, con reloj analogico en vivo (mismo patron del
 * editor de hora de Alarmas, D-163) -------------------------------- */
#define DE_DIAL_R 42

static int s_te_hour = 0, s_te_minute = 0, s_te_field = 0;
static bool s_te_inited = false;

static void timeedit_ensure_init(void)
{
    struct tm *now;
    if (s_te_inited)
        return;
    now = get_time();
    if (now)
    {
        s_te_hour = now->tm_hour;
        s_te_minute = now->tm_min;
    }
    s_te_field = 0;
    s_te_inited = true;
}

static void draw_time_edit(void)
{
    int cx = A26_SCREEN_WIDTH / 2;
    int cy = A26_LAYOUT_STATUSBAR_HEIGHT + 14 + DE_DIAL_R;
    char buf[16];
    int h12, i, w, h, dx, dy;

    timeedit_ensure_init();
    h12 = s_te_hour % 12;
    if (h12 == 0) h12 = 12;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_SETTINGS_TIME));

    for (dy = -DE_DIAL_R; dy <= DE_DIAL_R; dy++)
    {
        int half = (int)a26_shell_isqrt256(
            (unsigned)(DE_DIAL_R * DE_DIAL_R - dy * dy)) / 256;
        lcd_set_foreground(a26_color(A26_SELECTION_FILL));
        lcd_hline(cx - half, cx + half, cy + dy);
    }
    lcd_set_foreground(a26_color(A26_TEXT_TERTIARY));
    for (i = 0; i < AURA_FLOW_IANGLE_MAX; i += 8)
    {
        dx = aura_flow_fsin(i) * DE_DIAL_R / AURA_FLOW_ONE;
        dy = -aura_flow_fcos(i) * DE_DIAL_R / AURA_FLOW_ONE;
        lcd_drawpixel(cx + dx, cy + dy);
    }
    lcd_set_foreground(s_te_field == 0 ? aura_accent() : a26_color(A26_TEXT_PRIMARY));
    dx = aura_flow_fsin(((s_te_hour % 12) * 60 + s_te_minute)
                        * AURA_FLOW_IANGLE_MAX / 720) * (DE_DIAL_R - 18) / AURA_FLOW_ONE;
    dy = -aura_flow_fcos(((s_te_hour % 12) * 60 + s_te_minute)
                         * AURA_FLOW_IANGLE_MAX / 720) * (DE_DIAL_R - 18) / AURA_FLOW_ONE;
    lcd_drawline(cx, cy, cx + dx, cy + dy);
    lcd_set_foreground(s_te_field == 1 ? aura_accent() : a26_color(A26_TEXT_SECONDARY));
    dx = aura_flow_fsin(s_te_minute * AURA_FLOW_IANGLE_MAX / 60)
         * (DE_DIAL_R - 9) / AURA_FLOW_ONE;
    dy = -aura_flow_fcos(s_te_minute * AURA_FLOW_IANGLE_MAX / 60)
         * (DE_DIAL_R - 9) / AURA_FLOW_ONE;
    lcd_drawline(cx, cy, cx + dx, cy + dy);

    snprintf(buf, sizeof(buf), "%d:%02d %s", h12, s_te_minute,
             s_te_hour < 12 ? "AM" : "PM");
    lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));
    lcd_set_foreground(aura_accent());
    lcd_getstringsize((const unsigned char *)buf, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, cy + DE_DIAL_R + A26_SPACING_LG,
               (const unsigned char *)buf);
}

static void time_edit_commit(void)
{
#if CONFIG_RTC
    struct tm tm;
    struct tm *now = get_time();

    if (now)
        tm = *now;
    else
        memset(&tm, 0, sizeof(tm));
    tm.tm_hour = s_te_hour;
    tm.tm_min  = s_te_minute;
    tm.tm_sec  = 0;
    rtc_write_datetime(&tm);
#endif
}

static void handle_time_edit(aura_nav_t *nav, long button)
{
    timeedit_ensure_init();

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (s_te_field == 0) s_te_hour = (s_te_hour + 1) % 24;
        else                 s_te_minute = (s_te_minute + 1) % 60;
        break;
    case BUTTON_SCROLL_BACK:
        if (s_te_field == 0) s_te_hour = (s_te_hour + 23) % 24;
        else                 s_te_minute = (s_te_minute + 59) % 60;
        break;
    case BUTTON_SELECT:
        if (s_te_field == 0)
            s_te_field = 1;
        else
        {
            time_edit_commit();
            s_te_inited = false;
            aura_nav_pop(nav);
        }
        break;
    case BUTTON_MENU:
        s_te_inited = false;
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

/* -- Zona horaria: mapa con un solo pin (encargo 2026-08-13) --------------
 *
 * "Un mapa que tiene un solo pin, al scrollear con el click wheel, se
 * va moviendo en orden de LATITUD... abajo aparece la confirmacion:
 * ciudad, GMT, hora" -- misma tabla de ciudades del Reloj
 * internacional (una sola fuente de husos en todo el firmware),
 * recorrida en el orden por latitud que expone
 * aura_worldclock_city_by_lat_order(). Mapa esquematico (continentes
 * como manchas redondeadas via proyeccion equirectangular) en vez de
 * un bitmap real -- coherente con el resto del sistema, sin assets de
 * imagen, todo generado desde tokens. */
#define TZM_X0  4
#define TZM_Y0  22
#define TZM_W   312
#define TZM_H   118

typedef struct { int x, y, w, h; } tzm_rect_t;

/* Cajas de continentes precalculadas con la misma proyeccion que
 * `tzm_project()` (equirectangular: x=(lon+180)*W/360, y=(90-lat)*H/180)
 * sobre los limites aproximados de cada masa continental -- solo
 * decoracion de fondo, no coordenadas de precision cartografica. */
static const tzm_rect_t TZM_CONTINENTS[] = {
    { 13,  35, 104, 36 },  /* America del Norte */
    { 89,  73,  41, 44 },  /* America del Sur */
    { 151, 35,  44, 22 },  /* Europa */
    { 144, 57,  59, 47 },  /* Africa */
    { 195, 35,  91, 43 },  /* Asia */
    { 257, 88,  37, 19 },  /* Oceania (Australia) */
};
#define TZM_CONTINENT_N ((int)(sizeof(TZM_CONTINENTS) / sizeof(TZM_CONTINENTS[0])))

static int s_tzm_pos = 0;      /* posicion en el orden por latitud */
static bool s_tzm_inited = false;

/* Ubica la posicion (en el orden por latitud) de la ciudad cuyo huso
 * coincide con el ajuste vigente -- punto de partida al entrar, para
 * que el pin arranque donde ya esta configurado, no en el extremo
 * norte del mapa. */
static int tzm_pos_for_current(void)
{
    int n = aura_worldclock_city_count();
    int i;
    for (i = 0; i < n; i++)
        if (aura_worldclock_city_utc_quarters(aura_worldclock_city_by_lat_order(i))
            == aura_settings.tz_local_quarters)
            return i;
    return 0;
}

static void tzm_project(int lat10, int lon10, int *px, int *py)
{
    *px = TZM_X0 + (lon10 + 1800) * TZM_W / 3600;
    *py = TZM_Y0 + (900 - lat10) * TZM_H / 1800;
}

static void draw_timezone(void)
{
    int n = aura_worldclock_city_count();
    int idx, i, px, py, w, h;
    int qtr, gmt_h, gmt_q;
    char gmt_buf[24], time_buf[16], sign;
    int hour24, minute;

    if (n <= 0)
        return;
    if (!s_tzm_inited)
    {
        s_tzm_pos = tzm_pos_for_current();
        s_tzm_inited = true;
    }
    if (s_tzm_pos >= n) s_tzm_pos = n - 1;
    idx = aura_worldclock_city_by_lat_order(s_tzm_pos);

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_SETTINGS_TIMEZONE));

    /* Continentes: mancha suave (SHELL_RAIL), esquinas redondeadas --
     * el mismo lenguaje de "tarjeta" que el resto del sistema, solo
     * que sin borde. */
    for (i = 0; i < TZM_CONTINENT_N; i++)
    {
        const tzm_rect_t *r = &TZM_CONTINENTS[i];
        a26_shell_fill_rounded_rect(r->x, r->y, r->w, r->h, 6,
                                     a26_color(A26_SHELL_RAIL),
                                     a26_color(A26_SHELL_BG));
    }

    /* Puntos tenues del resto de ciudades, para dar contexto al mapa. */
    for (i = 0; i < n; i++)
    {
        if (i == idx)
            continue;
        tzm_project(aura_worldclock_city_lat10(i), aura_worldclock_city_lon10(i),
                    &px, &py);
        lcd_set_foreground(a26_color(A26_TEXT_TERTIARY));
        lcd_drawpixel(px, py);
    }

    /* El PIN: unico, en acento, con una sombra sutil para que se lea
     * sobre cualquier continente. */
    tzm_project(aura_worldclock_city_lat10(idx), aura_worldclock_city_lon10(idx),
                &px, &py);
    a26_shell_fill_capsule_over(px - 3, py - 2, 6, 6, LCD_RGBPACK(0, 0, 0), 60);
    a26_shell_fill_rounded_rect(px - 4, py - 4, 8, 8, 4,
                                 aura_accent(), a26_color(A26_SHELL_BG));

    /* Confirmacion: ciudad / GMT +-H horas / hora local en 12h --
     * exactamente el formato del original ("Ciudad de Mexico / GMT -6
     * horas / 2:37PM"). */
    qtr = aura_worldclock_city_utc_quarters(idx);
    gmt_h = qtr / 4;
    gmt_q = (qtr < 0 ? -qtr : qtr) % 4;
    sign = qtr < 0 ? '-' : '+';
    if (gmt_q == 0)
        snprintf(gmt_buf, sizeof(gmt_buf), "GMT %c%d horas", sign,
                  gmt_h < 0 ? -gmt_h : gmt_h);
    else
        snprintf(gmt_buf, sizeof(gmt_buf), "GMT %c%d:%02d horas", sign,
                  gmt_h < 0 ? -gmt_h : gmt_h, gmt_q * 15);

    {
        struct tm *now = get_time();
        long mins = now ? now->tm_hour * 60 + now->tm_min : 0;
        int h12;

        mins += (qtr - aura_settings.tz_local_quarters) * 15;
        mins %= 24 * 60;
        if (mins < 0) mins += 24 * 60;
        hour24 = (int)(mins / 60);
        minute = (int)(mins % 60);
        h12 = hour24 % 12;
        if (h12 == 0) h12 = 12;
        snprintf(time_buf, sizeof(time_buf), "%d:%02d%s", h12, minute,
                  hour24 < 12 ? "AM" : "PM");
    }

    {
        int y = TZM_Y0 + TZM_H + A26_SPACING_MD;

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
        lcd_set_foreground(aura_accent());
        lcd_getstringsize((const unsigned char *)aura_worldclock_city_name(idx), &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y, (const unsigned char *)aura_worldclock_city_name(idx));
        y += h + A26_SPACING_XS;

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)gmt_buf, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y, (const unsigned char *)gmt_buf);
        y += h + A26_SPACING_XS;

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
        lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)time_buf, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y, (const unsigned char *)time_buf);
    }
}

static void handle_timezone(aura_nav_t *nav, long button)
{
    int n = aura_worldclock_city_count();

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_tzm_pos = aura_wheel_advance(s_tzm_pos, n, 1);
        break;
    case BUTTON_SCROLL_BACK:
        s_tzm_pos = aura_wheel_advance(s_tzm_pos, n, -1);
        break;
    case BUTTON_SELECT:
        aura_settings.tz_local_quarters =
            aura_worldclock_city_utc_quarters(aura_worldclock_city_by_lat_order(s_tzm_pos));
        aura_settings_save();
        s_tzm_inited = false;
        aura_nav_pop(nav);
        break;
    case BUTTON_MENU:
        s_tzm_inited = false;
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
    aura_widgets_draw_status_bar(NULL);

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
    aura_widgets_draw_status_bar(NULL);
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
    case AURA_SCREEN_MUSIC_COMPOSERS:
    case AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER:
    case AURA_SCREEN_MUSIC_SONGS_BY_ARTIST:
    case AURA_SCREEN_MUSIC_SONGS_BY_COMPOSER:
    case AURA_SCREEN_MUSIC_ARTISTS_BY_GENRE:
        return 1;
    default:
        return 0;
    }
}


/* Cover Flow es su propia puerta del submenu Musica (AURA_SCREEN_MUSIC_COVERFLOW,
 * music_entries[] arriba) -- YA NO una variante automatica de Albumes
 * segun aura_settings.graphics_mode (D-025 original). Ese diseno hacia
 * que entrar a "Albumes", o a "Albumes" de un artista especifico via
 * Artistas, disparara Cover Flow sin que el usuario lo hubiera elegido
 * -- un bug real de navegacion, no una decision de UX (componentes/left-panel.md
 * documenta Cover Flow como hermano de Artista/Albumes, no como su
 * disfraz). Albumes vuelve a ser SIEMPRE la lista plana
 * (is_music_browse_screen()), sin importar el modo grafico. */
static int is_coverflow_screen(aura_screen_id_t screen)
{
    return screen == AURA_SCREEN_MUSIC_COVERFLOW;
}

static aura_screen_id_t s_music_cache_screen = AURA_SCREEN_COUNT;
static int s_music_cache_generation = -1;
static aura_music_item_t s_music_cache[AURA_MUSIC_MAX_ITEMS];
static int s_music_cache_count = 0;
static aura_list_item_t s_music_items_buf[AURA_MUSIC_MAX_ITEMS];

/* Fila sintetica al tope de ciertas listas del original (2026-08-13):
 * "Canciones" en Albumes, "Todos" en las listas que agrupan (Artistas,
 * Autores, Albumes de un artista/autor, Generos, Artistas de un
 * genero). Devuelve la pantalla destino, o AURA_SCREEN_COUNT si esa
 * lista no lleva fila sintetica. Vive DENTRO de la cache de la lista
 * (indice 0), asi que rueda, seleccion y ScrollIndicator la tratan como
 * una fila mas, sin corrimientos por todos lados. */
static aura_screen_id_t browse_all_row_target(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_MUSIC_ALBUMS:            return AURA_SCREEN_MUSIC_SONGS;
    case AURA_SCREEN_MUSIC_ARTISTS:           return AURA_SCREEN_MUSIC_ALBUMS;
    case AURA_SCREEN_MUSIC_COMPOSERS:         return AURA_SCREEN_MUSIC_ALBUMS;
    case AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST:  return AURA_SCREEN_MUSIC_SONGS_BY_ARTIST;
    case AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER:return AURA_SCREEN_MUSIC_SONGS_BY_COMPOSER;
    case AURA_SCREEN_MUSIC_GENRES:            return AURA_SCREEN_MUSIC_ARTISTS;
    case AURA_SCREEN_MUSIC_ARTISTS_BY_GENRE:  return AURA_SCREEN_MUSIC_SONGS_BY_GENRE;
    default:                                  return AURA_SCREEN_COUNT;
    }
}

static aura_str_id_t browse_all_row_label(aura_screen_id_t screen)
{
    /* Albumes encabeza con "Canciones" (lleva a TODAS las canciones);
     * el resto con "Todos". */
    return (screen == AURA_SCREEN_MUSIC_ALBUMS) ? AURA_STR_MUSIC_SONGS
                                                 : AURA_STR_ALL;
}

static void ensure_music_cache(aura_screen_id_t screen)
{
    int gen = aura_music_filter_generation();
    int n;

    if (s_music_cache_screen == screen && s_music_cache_generation == gen)
        return;

    s_music_cache_screen = screen;
    s_music_cache_generation = gen;

    if (browse_all_row_target(screen) == AURA_SCREEN_COUNT)
    {
        s_music_cache_count = aura_music_browse(screen, s_music_cache,
                                                 AURA_MUSIC_MAX_ITEMS);
        return;
    }

    /* Con fila sintetica: se busca desde el indice 1 y la fila 0 se
     * rellena despues, para no mover memoria de mas. */
    n = aura_music_browse(screen, s_music_cache + 1, AURA_MUSIC_MAX_ITEMS - 1);
    strlcpy(s_music_cache[0].label, aura_str(browse_all_row_label(screen)),
            AURA_MUSIC_ITEM_LEN);
    s_music_cache[0].seek = -1; /* marca de fila sintetica */
    s_music_cache_count = n + 1;
}

/* -- Lista de Albumes: caratulas 42x42 (encargo 2026-08-13) ---------------
 *
 * Unica lista de CONTENIDO con miniaturas reales. Filas de 44px (vs las
 * ROW_HEIGHT estandar de aura_widgets.c) para dejar sitio a la
 * caratula; el padding superior es de solo 2px bajo la StatusBar (no
 * el A26_SPACING_SM=4 estandar del resto de las listas), para
 * maximizar cuantas filas caben. El calculo da 4 filas completas mas
 * una quinta al 95% de su alto (218/44) -- se lee como 5, como pide el
 * encargo. Aplica a las 3 pantallas que listan albumes. */
/* D-221 (encargo del dueno, 2026-08-14): "en el caso de la lista de
 * albumes, ayudame a hacerlos mas grandes, que nomas quepan 4
 * elementos en la pantalla". Esta pantalla NO comparte
 * aura_widgets_draw_list() con el resto de listas de contenido -- ya
 * tiene su propio renderizador dedicado (draw_album_list(), con
 * caratula real via draw_album_thumb()), asi que agrandarla es un
 * cambio local a estas macros, sin tocar el componente compartido ni
 * ninguna otra pantalla. Area util = 240 - ALBUM_LIST_TOP(22) = 218px;
 * 218/4 = 54px por fila (2px de sobra al fondo, antes del riel del
 * scroll indicator). La caratula crece de 42 a 48px (proporcional al
 * alto de fila nuevo, mismo margen relativo ~3px arriba/abajo que ya
 * tenia) -- el cache de caratulas usa el tamano en el NOMBRE del
 * archivo (aura_albumart.c: "<seek>-<size>.pfraw"), asi que el tamano
 * nuevo genera su propio cache aparte sin invalidar el de 42px que
 * pueda seguir usando otra pantalla. */
#define ALBUM_ROW_H     54
#define ALBUM_ART_SIZE  48
#define ALBUM_LIST_TOP  (A26_LAYOUT_STATUSBAR_HEIGHT + 2)
#define ALBUM_ART_X     A26_LAYOUT_LIST_INSET
#define ALBUM_TEXT_GAP  A26_SPACING_MD
#define ALBUM_VISIBLE   4

static bool is_album_list_screen(aura_screen_id_t screen)
{
    return screen == AURA_SCREEN_MUSIC_ALBUMS
        || screen == AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST
        || screen == AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER;
}

/* La caratula real usa el MISMO cache/pipeline que CoverFlow
 * (aura_albumart_load_for_album -- cache .pfraw en disco, cero
 * decodificacion JPEG en redibujados posteriores). El bitmap resultante
 * queda TRANSPUESTO (columna contigua, formato de CoverFlow): se
 * blitea columna por columna, mismo mecanismo que aura_nowplaying.c
 * usa para su propia caratula. La fila sintetica "Todos"/"Canciones"
 * (seek=-1) recibe la caratula Default, igual que un album sin arte
 * real -- no necesita un icono aparte. */
static void draw_album_thumb(int x, int y, int32_t seek)
{
    static unsigned char cover_buf[ALBUM_ART_SIZE * ALBUM_ART_SIZE * sizeof(fb_data)];
    static unsigned char refl_buf[ALBUM_ART_SIZE *
        (ALBUM_ART_SIZE * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT / 100 + 1)
        * sizeof(fb_data)];
    static fb_data col_buf[ALBUM_ART_SIZE];
    aura_albumart_t art;
    const fb_data *cover;
    int col, row;

    art.size = ALBUM_ART_SIZE;
    art.radius = A26_LAYOUT_CORNER_RADIUS_CARD;
    art.cover_data = cover_buf;
    art.reflection_data = refl_buf;

    if (seek < 0 || !aura_albumart_load_for_album(seek, &art))
        aura_albumart_load_default(&art);

    cover = (const fb_data *)art.cover_data;
    for (col = 0; col < ALBUM_ART_SIZE; col++)
    {
        for (row = 0; row < ALBUM_ART_SIZE; row++)
            col_buf[row] = cover[col * ALBUM_ART_SIZE + row];
        lcd_bitmap(col_buf, x + col, y, 1, ALBUM_ART_SIZE);
    }
}

static long s_album_activity_since = 0;
static int  s_album_last_selected = -1;

static void draw_album_list(aura_nav_t *nav, aura_screen_id_t screen)
{
    int selected = aura_nav_get_selection(nav);
    int count = s_music_cache_count;
    int visible = ALBUM_VISIBLE;
    int first = 0;
    int i, w, h;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(screen_title_id(screen)));

    if (count > visible)
    {
        first = selected - visible / 2;
        if (first < 0) first = 0;
        if (first > count - visible) first = count - visible;
    }

    if (selected != s_album_last_selected)
    {
        s_album_last_selected = selected;
        s_album_activity_since = current_tick;
    }

    /* Pastilla de seleccion: mismo padding horizontal que el resto de
     * las listas de contenido (correccion 2026-08-13, ver PILL_MARGIN_X
     * en aura_widgets.c). */
    if (selected >= first && selected < first + visible)
    {
        int sel_y = ALBUM_LIST_TOP + (selected - first) * ALBUM_ROW_H;
        a26_shell_fill_rounded_rect(A26_LAYOUT_LIST_INSET, sel_y,
                                     A26_SCREEN_WIDTH - 2 * A26_LAYOUT_LIST_INSET,
                                     ALBUM_ROW_H, A26_LAYOUT_CORNER_RADIUS_PILL,
                                     a26_color(A26_SELECTION_FILL),
                                     a26_color(A26_SHELL_BG));
    }

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    for (i = first; i < count && i < first + visible; i++)
    {
        int row_y = ALBUM_LIST_TOP + (i - first) * ALBUM_ROW_H;
        int art_y = row_y + (ALBUM_ROW_H - ALBUM_ART_SIZE) / 2;
        int text_x = ALBUM_ART_X + ALBUM_ART_SIZE + ALBUM_TEXT_GAP;
        bool is_sel = (i == selected);

        draw_album_thumb(ALBUM_ART_X, art_y, s_music_cache[i].seek);

        lcd_set_foreground(is_sel ? a26_color(A26_ACCENT) : a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)"Ay", &w, &h);
        aura_widgets_puts_clipped(text_x, row_y + (ALBUM_ROW_H - h) / 2,
                                   A26_SCREEN_WIDTH - text_x - A26_LAYOUT_LIST_INSET,
                                   s_music_cache[i].label);
    }

    aura_scroll_indicator_draw(A26_SCREEN_WIDTH, ALBUM_LIST_TOP,
                                ALBUM_VISIBLE * ALBUM_ROW_H, selected, count,
                                (current_tick - s_album_activity_since) * 1000L / HZ,
                                a26_color(A26_SHELL_BG), a26_color(A26_TEXT_TERTIARY));
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

    if (is_album_list_screen(screen))
    {
        draw_album_list(nav, screen);
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
/* Nombres CRUDOS de archivo (con extension .m3u/.m3u8) -- a diferencia
 * de antes (encargo del dueno, 2026-08-14), ya NO se pelan en el
 * momento de cachear: la
 * extension hace falta para encontrar el sidecar de portada
 * ("<mismo nombre sin extension>.jpg", aura_playlist_art_load()). El
 * nombre de exhibicion se resuelve al vuelo por fila en
 * draw_playlist_list() via aura_music_playlist_display_name(). */
static char s_playlist_cache[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
static int s_playlist_cache_count = 0;

static void ensure_playlist_cache(aura_screen_id_t screen)
{
    if (s_playlist_cache_screen == screen)
        return;

    s_playlist_cache_screen = screen;
    s_playlist_cache_count = aura_music_list_playlists(s_playlist_cache, AURA_MUSIC_MAX_ITEMS);
}

/* (encargo del dueno, 2026-08-14): "quiero que las playlists
 * tengan una imagen... la lista de playlists deberia verse como la
 * lista de albumes (4 elementos por pantalla, con la caratula a la
 * derecha)". Re-skin exacto de draw_album_thumb()/draw_album_list() de
 * arriba -- mismas macros de layout (ALBUM_ROW_H/ALBUM_ART_SIZE/
 * ALBUM_ART_X/ALBUM_TEXT_GAP/ALBUM_VISIBLE/ALBUM_LIST_TOP), unica
 * diferencia real es la fuente del bitmap (aura_playlist_art_load() en
 * vez de aura_albumart_load_for_album(), sin tagcache de por medio). */
static void draw_playlist_thumb(int x, int y, const char *raw_filename)
{
    static unsigned char cover_buf[ALBUM_ART_SIZE * ALBUM_ART_SIZE * sizeof(fb_data)];
    static unsigned char refl_buf[ALBUM_ART_SIZE *
        (ALBUM_ART_SIZE * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT / 100 + 1)
        * sizeof(fb_data)];
    static fb_data col_buf[ALBUM_ART_SIZE];
    aura_albumart_t art;
    const fb_data *cover;
    int col, row;

    art.size = ALBUM_ART_SIZE;
    art.radius = A26_LAYOUT_CORNER_RADIUS_CARD;
    art.cover_data = cover_buf;
    art.reflection_data = refl_buf;

    if (!aura_playlist_art_load(raw_filename, &art))
        aura_albumart_load_default(&art);

    cover = (const fb_data *)art.cover_data;
    for (col = 0; col < ALBUM_ART_SIZE; col++)
    {
        for (row = 0; row < ALBUM_ART_SIZE; row++)
            col_buf[row] = cover[col * ALBUM_ART_SIZE + row];
        lcd_bitmap(col_buf, x + col, y, 1, ALBUM_ART_SIZE);
    }
}

static long s_playlist_activity_since = 0;
static int  s_playlist_last_selected = -1;

static void draw_playlist_list(aura_nav_t *nav)
{
    int selected = aura_nav_get_selection(nav);
    int count = s_playlist_cache_count;
    int visible = ALBUM_VISIBLE;
    int first = 0;
    int i, w, h;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_MUSIC_PLAYLISTS));

    if (count > visible)
    {
        first = selected - visible / 2;
        if (first < 0) first = 0;
        if (first > count - visible) first = count - visible;
    }

    if (selected != s_playlist_last_selected)
    {
        s_playlist_last_selected = selected;
        s_playlist_activity_since = current_tick;
    }

    if (selected >= first && selected < first + visible)
    {
        int sel_y = ALBUM_LIST_TOP + (selected - first) * ALBUM_ROW_H;
        a26_shell_fill_rounded_rect(A26_LAYOUT_LIST_INSET, sel_y,
                                     A26_SCREEN_WIDTH - 2 * A26_LAYOUT_LIST_INSET,
                                     ALBUM_ROW_H, A26_LAYOUT_CORNER_RADIUS_PILL,
                                     a26_color(A26_SELECTION_FILL),
                                     a26_color(A26_SHELL_BG));
    }

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    for (i = first; i < count && i < first + visible; i++)
    {
        char display[AURA_MUSIC_ITEM_LEN];
        int row_y = ALBUM_LIST_TOP + (i - first) * ALBUM_ROW_H;
        int art_y = row_y + (ALBUM_ROW_H - ALBUM_ART_SIZE) / 2;
        int text_x = ALBUM_ART_X + ALBUM_ART_SIZE + ALBUM_TEXT_GAP;
        bool is_sel = (i == selected);

        draw_playlist_thumb(ALBUM_ART_X, art_y, s_playlist_cache[i]);

        aura_music_playlist_display_name(s_playlist_cache[i], display, sizeof(display));

        lcd_set_foreground(is_sel ? a26_color(A26_ACCENT) : a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)"Ay", &w, &h);
        aura_widgets_puts_clipped(text_x, row_y + (ALBUM_ROW_H - h) / 2,
                                   A26_SCREEN_WIDTH - text_x - A26_LAYOUT_LIST_INSET,
                                   display);
    }

    aura_scroll_indicator_draw(A26_SCREEN_WIDTH, ALBUM_LIST_TOP,
                                ALBUM_VISIBLE * ALBUM_ROW_H, selected, count,
                                (current_tick - s_playlist_activity_since) * 1000L / HZ,
                                a26_color(A26_SHELL_BG), a26_color(A26_TEXT_TERTIARY));
}

static void draw_playlists(aura_nav_t *nav)
{
    ensure_playlist_cache(AURA_SCREEN_MUSIC_PLAYLISTS);

    if (s_playlist_cache_count == 0)
    {
        draw_message_centered(AURA_STR_EMPTY_LIST);
        return;
    }

    draw_playlist_list(nav);
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
/* POLITICA DE LISTAS (regla del dueno del diseno, 2026-08-13):
 *
 *   Lista de MENU (opciones del aparato)      -> LeftPanel, layout SPLIT
 *   Lista de ELEMENTOS (contenido del usuario) -> pantalla COMPLETA
 *
 * "Elementos" son canciones, albumes, artistas, generos, autores,
 * recopilaciones, listas de reproduccion y todos sus derivados: son
 * contenido, no ajustes, y el original tambien los muestra a pantalla
 * completa. Antes, las 4 pantallas de nivel 2 (Artistas/Albumes/
 * Canciones/Generos) y Listas repr. se dibujaban SPLIT -- eran listas
 * de contenido con panel de menu, la incongruencia que el dueno
 * reporto. (La distincion nivel2/nivel3+ que introdujo AUDITORIA-01
 * A-03 desaparece: ahora TODO browse de musica es pantalla completa,
 * sin importar su profundidad.)
 *
 * Esta tabla es la UNICA fuente del layout: de ella salen el LeftPanel,
 * el ancho de la StatusBar (D-149) y el ancho del push T1/T3. */
static int screen_uses_split_layout(aura_screen_id_t screen)
{
    return screen == AURA_SCREEN_ROOT
        || screen == AURA_SCREEN_SETTINGS
        || screen == AURA_SCREEN_MUSIC
        || screen == AURA_SCREEN_EXTRAS
        || screen == AURA_SCREEN_SETTINGS_DATETIME
        || is_choice_screen(screen)
        || screen == AURA_SCREEN_SETTINGS_STYLE
        || screen == AURA_SCREEN_SETTINGS_BACKLIGHT
        || screen == AURA_SCREEN_SETTINGS_SLEEPTIMER
        || screen == AURA_SCREEN_SETTINGS_MAINMENU
        || screen == AURA_SCREEN_VIDEOS
        || screen == AURA_SCREEN_PHOTOS;
}

/* Categoria de la seccion activa para TODO el cuadro (encargo del dueno
 * 2026-08-14, "cascading a color hierarchy" -- aura_category.h): en la
 * raiz del Menu principal no hay una seccion propia todavia, la
 * categoria real es la del item resaltado (root_entries[sel].target,
 * MISMO dato que ya resuelve panel_icon en draw_nav_list mas abajo);
 * fuera de la raiz, la pantalla actual YA vive dentro de una seccion
 * (aura_category_for_screen cubre todo el arbol, sin importar la
 * profundidad -- Ajustes -> Pantalla -> Brillo sigue siendo Ajustes). */
static void update_active_category(aura_nav_t *nav, aura_screen_id_t screen)
{
    aura_category_t cat;

    if (screen == AURA_SCREEN_ROOT)
    {
        int sel = aura_nav_get_selection(nav);

        rebuild_root_entries();
        cat = (sel >= 0 && sel < root_entries_count)
            ? aura_category_for_screen(root_entries[sel].target)
            : AURA_CATEGORY_NONE;
    }
    else
    {
        cat = aura_category_for_screen(screen);
    }
    aura_category_set_current(cat);
}

void aura_screens_draw(aura_nav_t *nav)
{
    aura_screen_id_t screen = aura_nav_current(nav);

    update_active_category(nav, screen);

    aura_widgets_set_list_layout(screen_uses_split_layout(screen)
                                  ? AURA_LIST_SPLIT : AURA_LIST_FULL);

    if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS
        || screen == AURA_SCREEN_MUSIC || screen == AURA_SCREEN_EXTRAS
        || screen == AURA_SCREEN_VIDEOS || screen == AURA_SCREEN_PHOTOS
        || screen == AURA_SCREEN_SETTINGS_DATETIME)
        draw_nav_list(nav, screen);
    else if (is_choice_screen(screen))
        draw_choice_list(nav, screen);
    else if (screen == AURA_SCREEN_SETTINGS_STYLE)
        draw_style_list(nav);
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
    else if (screen == AURA_SCREEN_SETTINGS_COPYRIGHT)
        draw_long_text(AURA_STR_SETTINGS_COPYRIGHT, AURA_STR_COPYRIGHT_BODY);
    else if (screen == AURA_SCREEN_EXTRAS_NOTES)
        draw_long_text(AURA_STR_EXTRAS_NOTES, AURA_STR_NOTES_BODY);
    else if (screen == AURA_SCREEN_EXTRAS_GAMES)
        draw_games(nav);
    else if (screen == AURA_SCREEN_SETTINGS_AUDIOBOOKS)
        draw_audiobooks(nav);
    else if (screen == AURA_SCREEN_EXTRAS_STOPWATCH)
        aura_stopwatch_draw();
    else if (screen == AURA_SCREEN_MUSIC_SEARCH)
        aura_search_draw();
    else if (screen == AURA_SCREEN_EXTRAS_CLOCKS)
        aura_worldclock_draw();
    else if (screen == AURA_SCREEN_EXTRAS_CALENDAR)
        aura_calendar_draw();
    else if (screen == AURA_SCREEN_EXTRAS_CALENDAR_DAY)
        aura_calendar_day_draw();
    else if (screen == AURA_SCREEN_SETTINGS_SCREENLOCK)
        aura_screenlock_draw();
    else if (screen == AURA_SCREEN_SETTINGS_DATE_EDIT)
        draw_date_edit();
    else if (screen == AURA_SCREEN_SETTINGS_TIME_EDIT)
        draw_time_edit();
    else if (screen == AURA_SCREEN_SETTINGS_TIMEZONE)
        draw_timezone();
    else if (screen == AURA_SCREEN_EXTRAS_ALARMS)
        aura_alarms_draw();
    else if (screen == AURA_SCREEN_EXTRAS_ALARM_EDIT)
        aura_alarm_edit_draw();
    else if (screen == AURA_SCREEN_EXTRAS_ALARM_TIME)
        aura_alarm_time_draw();
    else if (screen == AURA_SCREEN_EXTRAS_ALARM_CHOICE)
        aura_alarm_choice_draw();
    else if (screen == AURA_SCREEN_EXTRAS_CLOCK_REGIONS)
        aura_worldclock_regions_draw();
    else if (screen == AURA_SCREEN_EXTRAS_CLOCK_CITIES)
        aura_worldclock_cities_draw();
    else if (screen == AURA_SCREEN_MUSIC_SEARCH_RESULTS)
        aura_search_results_draw();
    else if (screen == AURA_SCREEN_MUSIC_AUDIOBOOKS)
    {
        /* Pantallas del arbol del original cuya interfaz propia
         * todavia no se construyo: estado vacio honesto con su titulo,
         * nunca una pantalla en blanco ni una fila que no responde. */
        draw_message_centered(AURA_STR_EMPTY_GENERIC);
    }
    else if (is_coverflow_screen(screen))
        aura_coverflow_draw(nav, screen);
    else if (is_music_browse_screen(screen))
        draw_music_browse(nav, screen);
    else if (screen == AURA_SCREEN_MUSIC_PLAYLISTS)
        draw_playlists(nav);
    else if (screen == AURA_SCREEN_PHOTOS_ALL)
        /* D-291: AURA_SCREEN_PHOTOS (la fila-menu "Todas las fotos" que
         * cuelga del menu raiz) ya la intercepta draw_nav_list() mas
         * arriba en esta misma cadena -- el contenido real de la lista
         * vive en AURA_SCREEN_PHOTOS_ALL, que hasta aqui no tenia caso
         * de dibujo y caia al fallback generico ("Nada sonando" con
         * fotos reales en el disco). Mismo bug para Video, dos lineas
         * abajo. Ambos habian sido corregidos en D-251 y se perdieron
         * en el revert de D-253 (DECISIONS-ARCHIVE.md) sin reaplicarse
         * -- ver PLAN-image-viewer.md. */
        aura_photos_draw(nav);
    else if (screen == AURA_SCREEN_PHOTO_VIEWER)
        aura_photo_viewer_draw(nav);
    else if (screen == AURA_SCREEN_VIDEOS_ALL)
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
int aura_wheel_advance(int sel, int count, int direction)
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
        aura_nav_set_selection(nav, aura_wheel_advance(sel, count, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, count, -1));
        break;
    case BUTTON_SELECT:
        /* Filas booleanas (Aleatorio, Clicker): SELECT invierte el
         * switch in situ y NO navega -- doc de comportamiento SS1,
         * `[OPCION]` "no tiene mecanica propia" (D-075). Distinto de
         * cualquier otra fila de Ajustes, que si empuja su pantalla. */
        /* Filas inertes: presentes, no elegibles. */
        if (entries[sel].target == AURA_SCREEN_MUSIC_COMPILATIONS
            || entries[sel].target == AURA_SCREEN_VIDEOS_MOVIES
            || entries[sel].target == AURA_SCREEN_VIDEOS_TVSHOWS
            || entries[sel].target == AURA_SCREEN_VIDEOS_CLIPS
            || entries[sel].target == AURA_SCREEN_MUSIC_AUDIOBOOKS
            || entries[sel].target == AURA_SCREEN_EXTRAS_CONTACTS)
            break;
        if (settings_row_toggle_value(entries[sel].target) >= 0)
        {
            toggle_settings_row(entries[sel].target);
            break;
        }
        /* Repetir (D-264): en linea igual que las filas booleanas de
         * arriba, pero de 3 estados en vez de 2 -- SELECT cicla
         * Desactivado -> Todo -> Uno -> Desactivado (mismos indices
         * 0/1/2 que ya usaba la lista de eleccion retirada, D-021) y
         * persiste con settings_save() (ajuste real de Rockbox,
         * global_settings.repeat_mode -- mismo criterio que ya
         * documentaba apply_choice() antes de que esta fila dejara de
         * navegar ahi). */
        if (entries[sel].target == AURA_SCREEN_SETTINGS_REPEAT)
        {
            global_settings.repeat_mode = (global_settings.repeat_mode + 1) % 3;
            settings_save();
            break;
        }
        /* "Canciones aleat." (menu de inicio del original) es una
         * ACCION, no una pantalla: baraja toda la biblioteca, arranca
         * la reproduccion y abre el reproductor. */
        if (entries[sel].target == AURA_SCREEN_SHUFFLE_SONGS)
        {
            if (aura_music_play_all_shuffled())
                aura_nav_push(nav, AURA_SCREEN_NOWPLAYING);
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

    /* Filas inertes (idiomas sin traduccion): se pueden recorrer, no
     * elegir -- el catalogo completo se ve, pero el firmware no finge
     * soportar lo que no tiene. */
    if (button == BUTTON_SELECT && screen == AURA_SCREEN_SETTINGS_LANGUAGE
        && sel >= LANGUAGE_AVAILABLE_N)
        return;
    (void)labels;

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, count, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, count, -1));
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
        aura_nav_set_selection(nav, aura_wheel_advance(sel, count, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, count, -1));
        break;
    case BUTTON_SELECT:
        if (count == 0)
            break;
        /* Fila sintetica: no filtra por si misma, solo abre la lista
         * agregada del nivel siguiente conservando los filtros vigentes
         * (p.ej. "Todos" dentro de un artista = todas sus canciones). */
        if (sel == 0 && browse_all_row_target(screen) != AURA_SCREEN_COUNT)
        {
            aura_nav_push(nav, browse_all_row_target(screen));
            break;
        }
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
            aura_nav_push(nav, AURA_SCREEN_MUSIC_ARTISTS_BY_GENRE);
            break;
        case AURA_SCREEN_MUSIC_ARTISTS_BY_GENRE:
            aura_music_select_artist(s_music_cache[sel].seek);
            aura_nav_push(nav, AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST);
            break;
        case AURA_SCREEN_MUSIC_COMPOSERS:
            aura_music_select_composer(s_music_cache[sel].seek);
            aura_nav_push(nav, AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER);
            break;
        case AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER:
            aura_music_select_album(s_music_cache[sel].seek);
            aura_nav_push(nav, AURA_SCREEN_MUSIC_SONGS_BY_ALBUM);
            break;
        case AURA_SCREEN_MUSIC_SONGS:
        case AURA_SCREEN_MUSIC_SONGS_BY_ALBUM:
        case AURA_SCREEN_MUSIC_SONGS_BY_GENRE:
        case AURA_SCREEN_MUSIC_SONGS_BY_ARTIST:
        case AURA_SCREEN_MUSIC_SONGS_BY_COMPOSER:
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
        /* aura_wheel_advance(), no +-1 fijo (AUDITORIA-01 A-18): unico
         * manejador de lista que se habia quedado afuera de la
         * aceleracion real de rueda de Fase 29 (D-077). */
        aura_nav_set_selection(nav, aura_wheel_advance(sel, s_playlist_cache_count, 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, s_playlist_cache_count, -1));
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
     * no hace nada, no fuerza una pausa sin sentido.
     *
     * Excepcion (encargo del dueno del diseno, 2026-08-12): en Cover
     * Flow, PLAY tiene semantica propia -- reproduce el album enfocado
     * al instante sin salir del carrusel (aura_coverflow_handle_button)
     * -- asi que ahi NO se intercepta globalmente. */
    if (button == BUTTON_PLAY && !is_coverflow_screen(screen))
    {
        int status = audio_status();
        if (status & AUDIO_STATUS_PAUSE)
            audio_resume();
        else if (status & AUDIO_STATUS_PLAY)
            audio_pause();
        return;
    }

    if (screen == AURA_SCREEN_ROOT || screen == AURA_SCREEN_SETTINGS
        || screen == AURA_SCREEN_MUSIC || screen == AURA_SCREEN_EXTRAS
        || screen == AURA_SCREEN_VIDEOS || screen == AURA_SCREEN_PHOTOS
        || screen == AURA_SCREEN_SETTINGS_DATETIME)
        handle_nav_list(nav, screen, button);
    else if (is_choice_screen(screen))
        handle_choice_list(nav, screen, button);
    else if (screen == AURA_SCREEN_SETTINGS_STYLE)
        handle_style_list(nav, button);
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
    else if (screen == AURA_SCREEN_SETTINGS_COPYRIGHT
             || screen == AURA_SCREEN_EXTRAS_NOTES)
        handle_legal_text(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_AUDIOBOOKS)
        handle_audiobooks(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_GAMES)
        handle_games(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_STOPWATCH)
        aura_stopwatch_handle_button(nav, button);
    else if (screen == AURA_SCREEN_MUSIC_SEARCH)
        aura_search_handle_button(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_CLOCKS)
        aura_worldclock_handle_button(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_CALENDAR)
        aura_calendar_handle_button(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_CALENDAR_DAY)
        aura_calendar_day_handle_button(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_SCREENLOCK)
        aura_screenlock_handle_button(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_DATE_EDIT)
        handle_date_edit(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_TIME_EDIT)
        handle_time_edit(nav, button);
    else if (screen == AURA_SCREEN_SETTINGS_TIMEZONE)
        handle_timezone(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_ALARMS)
        aura_alarms_handle_button(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_ALARM_EDIT)
        aura_alarm_edit_handle_button(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_ALARM_TIME)
        aura_alarm_time_handle_button(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_ALARM_CHOICE)
        aura_alarm_choice_handle_button(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_CLOCK_REGIONS)
        aura_worldclock_regions_handle_button(nav, button);
    else if (screen == AURA_SCREEN_EXTRAS_CLOCK_CITIES)
        aura_worldclock_cities_handle_button(nav, button);
    else if (screen == AURA_SCREEN_MUSIC_SEARCH_RESULTS)
        aura_search_results_handle_button(nav, button);
    else if (is_coverflow_screen(screen))
        aura_coverflow_handle_button(nav, screen, button);
    else if (is_music_browse_screen(screen))
        handle_music_browse(nav, screen, button);
    else if (screen == AURA_SCREEN_MUSIC_PLAYLISTS)
        handle_playlists(nav, button);
    else if (screen == AURA_SCREEN_PHOTOS_ALL)
        aura_photos_handle_button(nav, button);
    else if (screen == AURA_SCREEN_PHOTO_VIEWER)
        aura_photo_viewer_handle_button(nav, button);
    else if (screen == AURA_SCREEN_VIDEOS_ALL)
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

        /* Fix 3 (encargo 2026-08-14): la busqueda de Musica persiste su
         * texto mientras el regreso se quede dentro de la app (p.ej.
         * Busqueda -> submenu Musica -> Busqueda), pero se reinicia al
         * desandar toda la pila hasta la raiz/menu principal. Este es
         * el UNICO lugar donde la profundidad de toda la navegacion se
         * conoce de forma centralizada (ver comentario D-024 abajo),
         * asi que es el gancho natural -- ninguna pantalla individual
         * sabe cuando la pila entera llego a la raiz. */
        if (depth_after < depth_before && to == AURA_SCREEN_ROOT)
            aura_search_reset();

        /* D-291: re-escanea /Photos y /Videos cada vez que se entra a
         * su lista desde el menu -- sin esto, un sync por USB durante
         * la sesion no se reflejaba hasta reiniciar (la cache de
         * ensure_photo_list()/ensure_video_list() solo se llenaba una
         * vez). Mismo gancho central que search_reset arriba: es el
         * unico lugar que ve ambos extremos de la navegacion. */
        if (depth_after > depth_before && to == AURA_SCREEN_PHOTOS_ALL)
            aura_photos_invalidate();
        if (depth_after > depth_before && to == AURA_SCREEN_VIDEOS_ALL)
            aura_video_invalidate();

        /* Una pulsacion = una navegacion, aunque el boton se sostenga
         * durante la transicion (los repeats acumulados/venideros de
         * ESTE boton se ignoran hasta su REL -- ver
         * aura_main_swallow_repeats). */
        aura_main_swallow_repeats(button);

        if (depth_after > depth_before && is_coverflow_screen(to))
            /* D-259: prueba acotada -- coreografia distinta SOLO si
             * CoverDrift estaba realmente montado (comprometido, no
             * solo pendiente) para esta fila justo antes del push.
             * aura_screens_coverdrift_active_for() lee s_panel_committed
             * (D-262), que el ultimo draw() ya dejo listo -- nunca da
             * true salvo entrando desde el submenu de Musica con la
             * fila Cover Flow resaltada (music_row_wants_coverdrift()
             * no califica ningun otro origen para este destino, ver esa
             * funcion). */
            aura_transition_coverflow_enter(nav, aura_screens_coverdrift_active_for(to));
        else if (screen == AURA_SCREEN_NOWPLAYING && depth_after < depth_before
                 && aura_nowplaying_take_fullscreen_exit())
        {
            /* Salida desde el Modo 4 (letras). Correccion 2026-08-12:
             * si se ENTRO por coverflow, SI se regresa al coverflow --
             * despliegue inverso del panel encadenado con el morph de
             * regreso al carrusel. A cualquier otro destino, la
             * pantalla se comporta como pantalla completa y se
             * desplaza a la derecha dando paso al menu anterior. */
            if (is_coverflow_screen(to))
            {
                aura_nowplaying_unfold_from_lyrics();
                aura_transition_flow_return(nav);
            }
            else
                aura_transition_slide(nav, -1, A26_SCREEN_WIDTH, false);
        }
        else if (screen == AURA_SCREEN_NOWPLAYING && depth_after < depth_before
                 && is_coverflow_screen(to))
        {
            /* Regreso reproductor -> Cover Flow (encargo 2026-08-12):
             * morph de la caratula de frente + laterales/titulo
             * entrando desde los bordes -- nunca el slide generico. */
            aura_transition_flow_return(nav);
        }
        else if (is_coverflow_screen(screen) && to == AURA_SCREEN_NOWPLAYING)
        {
            /* Flip-and-Flow + morph de entrada (now-playing.md) ya
             * corrieron DENTRO de aura_transition_flip_and_flow() --
             * aplicar el push generico encima era exactamente el bug
             * que el dueno del diseno reporto (2026-08-12): la
             * transicion correcta es el morph, no empujar la pantalla
             * del reproductor desde la derecha. */
        }
        else if ((depth_after > depth_before && to == AURA_SCREEN_SETTINGS_ABOUT
                      && aura_screens_about_reveal_active())
                 || (depth_after < depth_before && screen == AURA_SCREEN_SETTINGS_ABOUT))
        {
            /* D-278/D-279 (encargo del dueno: "el SelectionSummary pasa a
             * pantalla completa, con el icono de Aura desplazandose a la
             * izquierda y la barra expandiendose"): Shift-and-Reveal
             * reemplaza el revelado tras paneles (D-264, que hacia
             * DESAPARECER el badge) para esta fila -- ahora el tile VIAJA,
             * no se pierde. Misma guarda de evidencia que antes usaba
             * `use_reveal` para la entrada (aura_screens_about_reveal_active():
             * el panel derecho ya comprometio la variante de Acerca de,
             * no una fila intermedia a medio debounce); la salida no
             * tenia guarda antes (use_reveal jamas cubria este caso, caia
             * al push generico) -- ahora tambien es la inversa exacta,
             * simetrica con como D-267 ya trato la salida de Cover Flow.
             * `carry` es el mismo rect en los dos sentidos (el tile en
             * split, el tile en full) -- aura_transition_shift_and_reveal()
             * decide sola hacia donde viaja segun el signo de `direction`. */
            int split_x, split_y, split_w, split_h;
            aura_shift_rect_t carry;

            aura_selection_summary_tile_rect_split(&split_x, &split_y, &split_w, &split_h);
            carry.from_x = split_x;
            carry.from_y = split_y;
            carry.from_w = split_w;
            carry.from_h = split_h;
            carry.to_x = AURA_DS_METRICS_ABOUT_EXPANDED_TILE_X;
            carry.to_y = split_y;   /* mismo eje vertical (Q11): solo cambia X */
            carry.to_w = split_w;
            carry.to_h = split_h;

            if (depth_after < depth_before)
                aura_widgets_panel_force_next();

            aura_transition_shift_and_reveal(nav, depth_after > depth_before ? 1 : -1,
                                             &carry);
        }
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

            /* D-261: infraestructura generica lista. D-264 conecta el
             * primer destino real distinto de Cover Flow: Ajustes ->
             * Acerca de, encargo explicito del dueno ("morph al entrar,
             * reusa el revelado de CoverDrift"). Acotado a ESTE origen/
             * destino exacto -- Musica mas alla de Cover Flow, Canciones
             * aleatorias, Video, Fotos siguen sin conectar, trabajo
             * aparte.
             * D-267 (encargo del dueno: "la transicion... deberia ser
             * exactamente la misma pero invertida cuando retrocedamos,
             * por ejemplo, al salir del coverflow"): el revelado ahora
             * TAMBIEN se activa al SALIR de Cover Flow, si CoverDrift
             * seguia genuinamente comprometido para la fila a la que se
             * vuelve -- s_panel_committed (D-262) no cambia mientras se
             * esta DENTRO de Cover Flow (esa pantalla no pasa por
             * render_panel_debounced(), es su propio camino de dibujo),
             * asi que aura_screens_coverdrift_active_for(to) sigue
             * leyendo el mismo estado valido que tenia al entrar --
             * mismo criterio, sin necesidad de guardar nada aparte.
             * aura_transition_slide() decide la geometria exacta
             * (revelado de entrada vs de salida) por el signo de
             * `direction`, ver reveal_behind_panels_exit() en
             * aura_transitions.c. */
            bool use_reveal = (depth_after > depth_before
                                    && to == AURA_SCREEN_SETTINGS_ABOUT
                                    && aura_screens_about_reveal_active())
                            || (depth_after < depth_before
                                    && is_coverflow_screen(screen)
                                    && aura_screens_coverdrift_active_for(to));
            aura_transition_slide(nav, depth_after > depth_before ? 1 : -1,
                                  width, use_reveal);
        }
    }
}
