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
#include "aura_category.h"

aura_category_t aura_category_for_screen(aura_screen_id_t screen)
{
    switch (screen)
    {
    /* -- Musica (incluye Ahora suena/Canciones aleat.: son reproduccion,
     * mismo color que el resto del arbol de Musica) -- */
    case AURA_SCREEN_MUSIC:
    case AURA_SCREEN_MUSIC_ARTISTS:
    case AURA_SCREEN_MUSIC_ALBUMS:
    case AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST:
    case AURA_SCREEN_MUSIC_SONGS:
    case AURA_SCREEN_MUSIC_SONGS_BY_ALBUM:
    case AURA_SCREEN_MUSIC_SONGS_BY_GENRE:
    case AURA_SCREEN_MUSIC_GENRES:
    case AURA_SCREEN_MUSIC_PLAYLISTS:
    case AURA_SCREEN_MUSIC_COMPOSERS:
    case AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER:
    case AURA_SCREEN_MUSIC_SONGS_BY_ARTIST:
    case AURA_SCREEN_MUSIC_SONGS_BY_COMPOSER:
    case AURA_SCREEN_MUSIC_ARTISTS_BY_GENRE:
    case AURA_SCREEN_MUSIC_COMPILATIONS:
    case AURA_SCREEN_MUSIC_AUDIOBOOKS:
    case AURA_SCREEN_MUSIC_SEARCH:
    case AURA_SCREEN_MUSIC_SEARCH_RESULTS:
    case AURA_SCREEN_MUSIC_FLOW:
    case AURA_SCREEN_SHUFFLE_SONGS:
    case AURA_SCREEN_NOWPLAYING:
        return AURA_CATEGORY_MUSIC;

    /* -- Video -- */
    case AURA_SCREEN_VIDEOS:
    case AURA_SCREEN_VIDEOS_MOVIES:
    case AURA_SCREEN_VIDEOS_TVSHOWS:
    case AURA_SCREEN_VIDEOS_CLIPS:
    case AURA_SCREEN_VIDEOS_ALL:
    case AURA_SCREEN_VIDEOS_MOVIEFLOW:
        return AURA_CATEGORY_VIDEO;

    /* -- Fotos -- */
    case AURA_SCREEN_PHOTOS:
    case AURA_SCREEN_PHOTOS_ALL:
    case AURA_SCREEN_PHOTO_VIEWER:
    /* D-316: las tres filas nuevas de categoria son la misma seccion. */
    case AURA_SCREEN_PHOTOS_PHOTO:
    case AURA_SCREEN_PHOTOS_IMAGE:
    case AURA_SCREEN_PHOTOS_AI:
        return AURA_CATEGORY_PHOTOS;

    /* -- Extras -- */
    case AURA_SCREEN_EXTRAS:
    case AURA_SCREEN_EXTRAS_CLOCKS:
    case AURA_SCREEN_EXTRAS_CLOCK_REGIONS:
    case AURA_SCREEN_EXTRAS_CLOCK_CITIES:
    case AURA_SCREEN_EXTRAS_CALENDAR:
    case AURA_SCREEN_EXTRAS_CALENDAR_DAY:
    case AURA_SCREEN_EXTRAS_CONTACTS:
    case AURA_SCREEN_EXTRAS_ALARMS:
    case AURA_SCREEN_EXTRAS_ALARM_EDIT:
    case AURA_SCREEN_EXTRAS_ALARM_TIME:
    case AURA_SCREEN_EXTRAS_ALARM_CHOICE:
    case AURA_SCREEN_EXTRAS_GAMES:
    case AURA_SCREEN_EXTRAS_NOTES:
    case AURA_SCREEN_EXTRAS_STOPWATCH:
        return AURA_CATEGORY_EXTRAS;

    /* -- Ajustes (incluye Fecha y Hora, todo son sub-pantallas de
     * Ajustes aunque el original las agrupe bajo Extras en el menu --
     * ver AURA_SCREEN_SETTINGS_DATETIME/DATE_EDIT/TIME_EDIT/TIMEZONE/
     * CLOCK24/CLOCK_TITLE, todas alcanzables solo desde Ajustes en
     * Aura, sistema/03-arbol-de-menus.md) -- */
    case AURA_SCREEN_SETTINGS:
    case AURA_SCREEN_SETTINGS_COPYRIGHT:
    case AURA_SCREEN_SETTINGS_AUDIOBOOKS:
    case AURA_SCREEN_SETTINGS_VOLUME_NORM:
    case AURA_SCREEN_SETTINGS_SORT_BY:
    case AURA_SCREEN_SETTINGS_MUSICMENU:
    case AURA_SCREEN_SETTINGS_DATE_EDIT:
    case AURA_SCREEN_SETTINGS_TIME_EDIT:
    case AURA_SCREEN_SETTINGS_DATETIME:
    case AURA_SCREEN_SETTINGS_TIMEZONE:
    case AURA_SCREEN_SETTINGS_CLOCK24:
    case AURA_SCREEN_SETTINGS_CLOCK_TITLE:
    /* D-292: submenu "Personalizacion" -- misma categoria que sus 7
     * hijas de abajo, ya listadas cada una por separado. */
    case AURA_SCREEN_SETTINGS_PERSONALIZATION:
    case AURA_SCREEN_SETTINGS_THEME:
    /* D-289: "Estilo" es hermana de "Tema" en Apariencia -- misma
     * categoria que el resto del grupo. */
    case AURA_SCREEN_SETTINGS_STYLE:
    case AURA_SCREEN_SETTINGS_ANIMATIONS:
    case AURA_SCREEN_SETTINGS_GRAPHICS:
    case AURA_SCREEN_SETTINGS_EQ:
    case AURA_SCREEN_SETTINGS_BRIGHTNESS:
    case AURA_SCREEN_SETTINGS_LANGUAGE:
    case AURA_SCREEN_SETTINGS_ABOUT:
    case AURA_SCREEN_SETTINGS_SHUFFLE:
    case AURA_SCREEN_SETTINGS_REPEAT:
    case AURA_SCREEN_SETTINGS_BACKLIGHT:
    case AURA_SCREEN_SETTINGS_SLEEPTIMER:
    case AURA_SCREEN_SETTINGS_VOLUME_LIMIT:
    case AURA_SCREEN_SETTINGS_CLICKER:
    case AURA_SCREEN_SETTINGS_MAINMENU:
    case AURA_SCREEN_SETTINGS_RESET:
    case AURA_SCREEN_SETTINGS_ACCENT:
    case AURA_SCREEN_SETTINGS_LEFT_PANEL_SHADOW:
    case AURA_SCREEN_SETTINGS_SHOW_ICONS:
    /* D-234 (fusionado despues de que este archivo se escribio):
     * apagado por inactividad y bloqueo con PIN, ambos reubicados a/
     * agregados en Ajustes -- misma categoria que el resto del arbol. */
    case AURA_SCREEN_SETTINGS_POWEROFF:
    case AURA_SCREEN_SETTINGS_SCREENLOCK:
    /* D-293: la fila de Ajustes y la pantalla de progreso que dispara --
     * la misma pantalla aparece sola al arrancar tras un sync, pero
     * sigue siendo "cosa del aparato", categoria Ajustes. */
    case AURA_SCREEN_SETTINGS_REBUILD_LIBRARY:
    case AURA_SCREEN_LIBRARY_SYNC:
        return AURA_CATEGORY_SETTINGS;

    /* Raiz del Menu principal (sin seccion propia -- la categoria de
     * cada fila se resuelve por su `target`, no por AURA_SCREEN_ROOT en
     * si) y el centinela de conteo: ninguno de los dos es una pantalla
     * navegable real. */
    case AURA_SCREEN_ROOT:
    case AURA_SCREEN_COUNT:
    default:
        return AURA_CATEGORY_NONE;
    }
}

static aura_category_t s_current_category = AURA_CATEGORY_NONE;

void aura_category_set_current(aura_category_t cat)
{
    s_current_category = cat;
}

aura_category_t aura_category_current(void)
{
    return s_current_category;
}
