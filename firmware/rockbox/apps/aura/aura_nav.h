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
/* Maquina de estados de navegacion de Aura UI.
 *
 * Modulo puro en C99, sin dependencias de Rockbox ni de ningun otro
 * modulo de Aura: se compila igual dentro del firmware que como
 * binario de test en el host (ver apps/aura/test/). Solo modela una
 * pila de pantallas con memoria de seleccion por nivel; no dibuja
 * nada ni toca hardware.
 */
#ifndef AURA_NAV_H
#define AURA_NAV_H

#define AURA_NAV_MAX_DEPTH 8

typedef enum {
    AURA_SCREEN_ROOT = 0,
    AURA_SCREEN_MUSIC,
    AURA_SCREEN_MUSIC_ARTISTS,
    AURA_SCREEN_MUSIC_ALBUMS,
    AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST,
    AURA_SCREEN_MUSIC_SONGS,
    AURA_SCREEN_MUSIC_SONGS_BY_ALBUM,
    AURA_SCREEN_MUSIC_SONGS_BY_GENRE,
    AURA_SCREEN_MUSIC_GENRES,
    AURA_SCREEN_MUSIC_PLAYLISTS,
    /* Arbol de Musica del firmware original (2026-08-13): Autores
     * (tag_composer), Recopilaciones (albumes marcados como
     * compilacion), Audiolibros, y los niveles que faltaban para que
     * las jerarquias sean las del original. */
    AURA_SCREEN_MUSIC_COMPOSERS,
    AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER,
    AURA_SCREEN_MUSIC_SONGS_BY_ARTIST,
    AURA_SCREEN_MUSIC_SONGS_BY_COMPOSER,
    AURA_SCREEN_MUSIC_ARTISTS_BY_GENRE,
    AURA_SCREEN_MUSIC_COMPILATIONS,
    AURA_SCREEN_MUSIC_AUDIOBOOKS,
    AURA_SCREEN_MUSIC_SEARCH,
    AURA_SCREEN_MUSIC_SEARCH_RESULTS,
    /* Menu de inicio del original */
    AURA_SCREEN_EXTRAS,
    AURA_SCREEN_SHUFFLE_SONGS,
    /* Extras del original (2026-08-13) */
    AURA_SCREEN_EXTRAS_CLOCKS,
    AURA_SCREEN_EXTRAS_CLOCK_REGIONS,
    AURA_SCREEN_EXTRAS_CLOCK_CITIES,
    AURA_SCREEN_EXTRAS_CALENDAR,
    AURA_SCREEN_EXTRAS_CALENDAR_DAY,
    AURA_SCREEN_EXTRAS_CONTACTS,
    AURA_SCREEN_EXTRAS_ALARMS,
    AURA_SCREEN_EXTRAS_ALARM_EDIT,
    AURA_SCREEN_EXTRAS_ALARM_TIME,
    AURA_SCREEN_EXTRAS_ALARM_CHOICE,
    AURA_SCREEN_EXTRAS_GAMES,
    AURA_SCREEN_EXTRAS_NOTES,
    /* Reubicado de Extras a Ajustes (encargo del dueno, bloqueo de PIN
     * global): renombrado para reflejarlo, pero se conserva en su
     * posicion del enum -- ningun valor de aura_screen_id_t se persiste
     * a disco, asi que reordenar seria seguro igual, pero renombrar en
     * el lugar deja un diff mas chico. */
    AURA_SCREEN_SETTINGS_SCREENLOCK,
    AURA_SCREEN_EXTRAS_STOPWATCH,
    /* Submenus de Videos y Fotos del original (2026-08-13) */
    AURA_SCREEN_VIDEOS_MOVIES,
    AURA_SCREEN_VIDEOS_TVSHOWS,
    AURA_SCREEN_VIDEOS_CLIPS,
    AURA_SCREEN_VIDEOS_ALL,
    AURA_SCREEN_PHOTOS_ALL,
    AURA_SCREEN_SETTINGS_COPYRIGHT,
    AURA_SCREEN_SETTINGS_AUDIOBOOKS,
    AURA_SCREEN_SETTINGS_VOLUME_NORM,
    AURA_SCREEN_SETTINGS_SORT_BY,
    AURA_SCREEN_SETTINGS_MUSICMENU,
    AURA_SCREEN_SETTINGS_DATE_EDIT,
    AURA_SCREEN_SETTINGS_TIME_EDIT,
    AURA_SCREEN_SETTINGS_DATETIME,
    AURA_SCREEN_SETTINGS_TIMEZONE,
    AURA_SCREEN_SETTINGS_CLOCK24,
    AURA_SCREEN_SETTINGS_CLOCK_TITLE,
    AURA_SCREEN_VIDEOS,
    AURA_SCREEN_PHOTOS,
    AURA_SCREEN_PHOTO_VIEWER,
    AURA_SCREEN_NOWPLAYING,
    AURA_SCREEN_SETTINGS,
    AURA_SCREEN_SETTINGS_THEME,
    AURA_SCREEN_SETTINGS_ANIMATIONS,
    AURA_SCREEN_SETTINGS_GRAPHICS,
    AURA_SCREEN_SETTINGS_EQ,
    AURA_SCREEN_SETTINGS_BRIGHTNESS,
    AURA_SCREEN_SETTINGS_LANGUAGE,
    AURA_SCREEN_SETTINGS_ABOUT,
    /* SHUFFLE y CLICKER ya NO son pantallas navegables (D-075): son
     * booleanos que viven inline en la fila de Ajustes con un switch.
     * El identificador se conserva como token estable para esa fila
     * (nav_entry_t.target, aura_screens.c) -- aura_nav_push() nunca se
     * llama con ellos. */
    AURA_SCREEN_SETTINGS_SHUFFLE,
    AURA_SCREEN_SETTINGS_REPEAT,
    AURA_SCREEN_SETTINGS_BACKLIGHT,
    AURA_SCREEN_SETTINGS_SLEEPTIMER,
    AURA_SCREEN_SETTINGS_VOLUME_LIMIT,
    AURA_SCREEN_SETTINGS_CLICKER,
    AURA_SCREEN_SETTINGS_MAINMENU,
    AURA_SCREEN_SETTINGS_RESET,
    /* Acento configurable (PLAN.md T0.3, fundamentos/01-color.md) --
     * agregada al final del enum, mismo criterio "solo-anadir-al-final"
     * que D-013 usa para aura_lang.h. */
    AURA_SCREEN_SETTINGS_ACCENT,
    /* Booleano real (aura_settings.left_panel_shadow, T0.4) -- vive
     * inline con switch (D-075), igual que SHUFFLE/CLICKER; el
     * identificador es solo el token estable de la fila. */
    AURA_SCREEN_SETTINGS_LEFT_PANEL_SHADOW,
    AURA_SCREEN_SETTINGS_SHOW_ICONS,
    /* Cover Flow como puerta propia del submenu Musica (D-025 la tenia
     * como una variante automatica de AURA_SCREEN_MUSIC_ALBUMS segun
     * aura_settings.graphics_mode -- eso hacia que entrar a Albumes, o a
     * Albumes-por-artista, disparara Cover Flow sin que el usuario lo
     * eligiera. componentes/left-panel.md la documenta como una entrada
     * de menu hermana de Artista/Albumes, asi que ahora es su propio
     * destino: Albumes vuelve a ser SIEMPRE la lista plana). Agregada al
     * final del enum, mismo criterio "solo-anadir-al-final" que el resto
     * de este archivo. */
    AURA_SCREEN_MUSIC_COVERFLOW,
    /* Apagado por inactividad (Task A, encargo del dueno): pantalla de
     * eleccion en Ajustes que envuelve global_settings.poweroff (nucleo
     * de Rockbox) -- ver aura_screens.c. Agregada al final del enum,
     * mismo criterio "solo-anadir-al-final". */
    AURA_SCREEN_SETTINGS_POWEROFF,
    /* D-289 (sistema de temas): lista de estilos instalados
     * (fuentes+iconos+paleta) -- "Estilo" en Ajustes > Apariencia,
     * justo despues de "Tema" (claro/oscuro, sin relacion con esto).
     * Agregada al final del enum, mismo criterio "solo-anadir-al-final". */
    AURA_SCREEN_SETTINGS_STYLE,
    AURA_SCREEN_COUNT,
} aura_screen_id_t;

typedef struct {
    aura_screen_id_t screens[AURA_NAV_MAX_DEPTH];
    int selection[AURA_NAV_MAX_DEPTH];
    int depth; /* numero de pantallas en la pila, siempre >= 1 tras init */
} aura_nav_t;

/* Inicializa la pila con `root` como unica pantalla (profundidad 1). */
void aura_nav_init(aura_nav_t *nav, aura_screen_id_t root);

/* Entra a una pantalla nueva. No hace nada si la pila ya esta llena
 * (AURA_NAV_MAX_DEPTH); devuelve true si pudo apilar. */
int aura_nav_push(aura_nav_t *nav, aura_screen_id_t screen);

/* Vuelve a la pantalla anterior. Devuelve false (y no hace nada) si
 * ya esta en la raiz -- la raiz nunca se desapila. */
int aura_nav_pop(aura_nav_t *nav);

/* Vuelve directamente a la raiz, descartando toda la pila intermedia. */
void aura_nav_reset_to_root(aura_nav_t *nav);

aura_screen_id_t aura_nav_current(const aura_nav_t *nav);
int aura_nav_depth(const aura_nav_t *nav);
int aura_nav_is_root(const aura_nav_t *nav);

/* Recuerda/recupera el indice de seleccion de la pantalla actual (tope
 * de la pila), para restaurar el cursor de una lista al volver a ella. */
void aura_nav_set_selection(aura_nav_t *nav, int index);
int aura_nav_get_selection(const aura_nav_t *nav);

#endif /* AURA_NAV_H */
