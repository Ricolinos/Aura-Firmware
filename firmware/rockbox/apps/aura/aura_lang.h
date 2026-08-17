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
    AURA_STR_SETTINGS_ANIMATIONS,
    AURA_STR_SETTINGS_GRAPHICS,
    AURA_STR_SETTINGS_EQ,
    AURA_STR_SETTINGS_BRIGHTNESS,
    AURA_STR_SETTINGS_LANGUAGE,
    AURA_STR_SETTINGS_ABOUT,
    AURA_STR_SETTINGS_SHUFFLE,
    AURA_STR_SETTINGS_REPEAT,

    AURA_STR_REPEAT_OFF,
    AURA_STR_REPEAT_ALL,
    AURA_STR_REPEAT_ONE,

    AURA_STR_SETTINGS_BACKLIGHT,
    AURA_STR_SETTINGS_SLEEPTIMER,
    AURA_STR_SETTINGS_VOLUME_LIMIT,
    AURA_STR_SETTINGS_CLICKER,
    AURA_STR_SETTINGS_MAINMENU,
    AURA_STR_SETTINGS_RESET,

    AURA_STR_TIMEOUT_OFF,
    AURA_STR_TIMEOUT_ALWAYS,

    AURA_STR_MAINMENU_RESTORE,

    AURA_STR_RESET_CONFIRM_TITLE,
    AURA_STR_RESET_CONFIRM_BODY,
    AURA_STR_RESET_DONE,

    AURA_STR_THEME_LIGHT,
    AURA_STR_THEME_DARK,

    AURA_STR_ANIM_NONE,
    AURA_STR_ANIM_MINIMAL,
    AURA_STR_ANIM_ALL,

    AURA_STR_GFX_NONE,
    AURA_STR_GFX_MINIMAL,
    AURA_STR_GFX_ALL,


    AURA_STR_LANG_ES,
    AURA_STR_LANG_EN,

    AURA_STR_ABOUT_BUILT_ON,
    AURA_STR_ABOUT_MUSIC,
    AURA_STR_ABOUT_VIDEOS,
    AURA_STR_ABOUT_PHOTOS,
    AURA_STR_ABOUT_PLAYLISTS,
    AURA_STR_ABOUT_NO_SYNC,

    /* D-283 (Estado 2 de "Acerca de", conteos detallados): "Musica" ya
     * cuenta canciones/artistas/listas; Video y Fotos se dividen en sus
     * categorias reales de Aura Studio (MediaCategory/classifyPhoto).
     * AURA_STR_ABOUT_MOVIES/SERIES/CLIPS y AURA_STR_ABOUT_IMAGES/
     * PHOTOS_TAKEN/AI son etiquetas de FILA (distintas de
     * AURA_STR_ABOUT_VIDEOS/PHOTOS de arriba, que son titulos de
     * SECCION) -- Fotografias no reutiliza AURA_STR_ABOUT_PHOTOS porque
     * ese string significa "Fotos" como categoria de contenido general,
     * no la subcategoria "camara real" de MediaCategoryHeuristics. */
    AURA_STR_ABOUT_SONGS,
    AURA_STR_ABOUT_ARTISTS,
    AURA_STR_ABOUT_MOVIES,
    AURA_STR_ABOUT_SERIES,
    AURA_STR_ABOUT_CLIPS,
    AURA_STR_ABOUT_IMAGES,
    AURA_STR_ABOUT_PHOTOS_TAKEN,
    AURA_STR_ABOUT_AI,
    /* Manifiesto de un sync ANTERIOR a esta sesion -- sin los campos
     * *_count por categoria (D-283). Distinto de AURA_STR_ABOUT_NO_SYNC
     * (nunca se sincronizo NADA). */
    AURA_STR_ABOUT_SYNC_FOR_DETAIL,

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

    AURA_STR_YES,
    AURA_STR_NO,

    AURA_STR_ABOUT_STORAGE,
    AURA_STR_PLAYLIST_PICK,

    /* Acento configurable (PLAN.md T0.3) -- presets provisionales, sin
     * consumidor de UI de swatch todavia (lista de eleccion por texto,
     * mismo patron que Tema/Idioma). */
    AURA_STR_SETTINGS_ACCENT,
    AURA_STR_ACCENT_PINK,
    AURA_STR_ACCENT_RED,
    AURA_STR_ACCENT_ORANGE,
    AURA_STR_ACCENT_GREEN,
    AURA_STR_ACCENT_BLUE,
    AURA_STR_ACCENT_PURPLE,
    AURA_STR_SETTINGS_LEFT_PANEL_SHADOW,

    AURA_STR_MUSIC_COVERFLOW,

    /* Etiquetas para entradas sin tag en la base de datos -- tagcache
     * genera el literal "<Untagged>", jerga tecnica que la regla dura
     * del proyecto prohibe mostrar; se sustituye por espanol natural en
     * aura_music.c segun el tipo de tag. */
    AURA_STR_UNKNOWN_ALBUM,
    AURA_STR_UNKNOWN_ARTIST,
    AURA_STR_UNKNOWN_GENRE,
    AURA_STR_UNKNOWN_TITLE,

    /* Arbol del firmware original (2026-08-13) */
    AURA_STR_MUSIC_COMPOSERS,
    AURA_STR_MUSIC_COMPILATIONS,
    AURA_STR_MUSIC_AUDIOBOOKS,
    AURA_STR_MUSIC_SEARCH,
    AURA_STR_ALL,
    AURA_STR_EXTRAS,
    AURA_STR_SHUFFLE_SONGS,

    AURA_STR_EXTRAS_CLOCKS,
    AURA_STR_EXTRAS_CALENDAR,
    AURA_STR_EXTRAS_CONTACTS,
    AURA_STR_EXTRAS_ALARMS,
    AURA_STR_EXTRAS_GAMES,
    AURA_STR_EXTRAS_NOTES,
    /* Reubicado de Extras a Ajustes -- ver AURA_SCREEN_SETTINGS_SCREENLOCK
     * en aura_nav.h. */
    AURA_STR_SETTINGS_SCREENLOCK,
    AURA_STR_EXTRAS_STOPWATCH,
    AURA_STR_EMPTY_GENERIC,

    AURA_STR_SETTINGS_SHOW_ICONS,

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

    AURA_STR_LANG_DA,
    AURA_STR_LANG_DE,
    AURA_STR_LANG_FR,
    AURA_STR_LANG_IT,
    AURA_STR_LANG_NL,
    AURA_STR_LANG_NO,
    AURA_STR_LANG_PT,
    AURA_STR_LANG_FI,
    AURA_STR_LANG_SV,
    AURA_STR_LANG_JA,
    AURA_STR_LANG_ZH,
    AURA_STR_LANG_KO,
    AURA_STR_LANG_RU,

    AURA_STR_VIDEOS_MOVIES,
    AURA_STR_VIDEOS_TVSHOWS,
    AURA_STR_VIDEOS_CLIPS,
    AURA_STR_VIDEOS_ALL,
    AURA_STR_PHOTOS_ALL,
    AURA_STR_SETTINGS_COPYRIGHT,

    AURA_STR_COPYRIGHT_BODY,

    AURA_STR_WC_LOCAL,
    AURA_STR_WC_ADD,
    AURA_STR_WC_EDIT,
    AURA_STR_WC_DELETE,

    AURA_STR_SETTINGS_AUDIOBOOKS,
    AURA_STR_SETTINGS_VOLUME_NORM,
    AURA_STR_SETTINGS_SORT_BY,
    AURA_STR_SETTINGS_MUSICMENU,
    AURA_STR_SPEED_SLOW,
    AURA_STR_SPEED_NORMAL,
    AURA_STR_SPEED_FAST,
    AURA_STR_AUDIOBOOKS_BODY,
    AURA_STR_SORT_FIRSTNAME,
    AURA_STR_SORT_LASTNAME,
    AURA_STR_NOTES_BODY,
    AURA_STR_GAME_IQUIZ,
    AURA_STR_GAME_KLONDIKE,
    AURA_STR_GAME_VORTEX,

    AURA_STR_CAL_NO_EVENTS,

    AURA_STR_LOCK_SET,
    AURA_STR_LOCK_CONFIRM,
    /* D-197: pantalla de DESBLOQUEO (distinta de configurar una clave
     * nueva) -- se muestra cuando aura_settings.screen_lock_active es
     * verdadero, en cualquier arranque o si se vuelve a esta pantalla
     * ya bloqueada. */
    AURA_STR_LOCK_ENTER,

    AURA_STR_ALARM_NEW,
    AURA_STR_ALARM_HOUR,
    AURA_STR_ALARM_MINUTE,

    AURA_STR_SETTINGS_DATETIME,
    AURA_STR_SETTINGS_TIMEZONE,
    AURA_STR_SETTINGS_CLOCK24,
    AURA_STR_SETTINGS_CLOCK_TITLE,

    AURA_STR_SETTINGS_DATE,
    AURA_STR_SETTINGS_TIME,

    /* D-224: precarga de caratulas de Cover Flow en el primer arranque
     * tras cada escaneo de biblioteca -- plantilla combinada con
     * snprintf(..., "%s %d/%d", aura_str(...), hechos, total) en
     * aura_music.c, mismo patron que AURA_STR_ABOUT_MUSIC en
     * aura_screens.c. */
    AURA_STR_PRECACHE_ART,

    /* Apagado del iPod (Task A, encargo del dueno): eleccion del
     * temporizador de apagado por inactividad, envuelve
     * global_settings.poweroff. AURA_STR_TIMEOUT_OFF (ya existe) cubre
     * "Desactivado" -- solo hacen falta las 3 opciones con tiempo. */
    AURA_STR_SETTINGS_POWEROFF,
    AURA_STR_POWEROFF_10MIN,
    AURA_STR_POWEROFF_20MIN,
    AURA_STR_POWEROFF_1HOUR,

    /* Bloqueo de pantalla global (Task B): pantalla de confirmacion
     * para "Desactivar" -- borra la clave guardada, distinta del flujo
     * de "Activar" (configurar clave, AURA_STR_LOCK_SET/CONFIRM ya
     * existentes). */
    AURA_STR_SCREENLOCK_DISABLE_TITLE,
    AURA_STR_SCREENLOCK_DISABLE_BODY,

    /* D-264: contenido real del panel derecho por fila dentro de cada
     * submenu de biblioteca (Musica/Video/Fotos, generaliza
     * root_selection_description() del menu raiz) y para un puñado de
     * filas especiales de Ajustes (Aleatorio, Repetir, Fecha y hora,
     * Acerca de). */
    AURA_STR_MUSIC_EMPTY_ARTISTS,
    AURA_STR_MUSIC_EMPTY_ALBUMS,
    AURA_STR_MUSIC_EMPTY_SONGS,
    AURA_STR_MUSIC_EMPTY_PLAYLISTS,
    AURA_STR_MUSIC_EMPTY_GENRES,
    AURA_STR_MUSIC_EMPTY_COMPOSERS,
    AURA_STR_MUSIC_EMPTY_AUDIOBOOKS,
    AURA_STR_VIDEOS_EMPTY_MOVIES,
    AURA_STR_VIDEOS_EMPTY_TVSHOWS,
    AURA_STR_VIDEOS_EMPTY_CLIPS,

    /* Generico -- reusado por Aleatorio y por el resumen de Repetir
     * (D-264): "Activado"/"Desactivado" ligado al valor real del
     * ajuste, distinto de AURA_STR_YES/_NO (esos son de confirmacion,
     * no de estado). */
    AURA_STR_TOGGLE_ON,
    AURA_STR_TOGGLE_OFF,

    /* Repetir como fila en linea (D-264): texto del resumen, distinto
     * de AURA_STR_REPEAT_ALL/_ONE ("Todo"/"Uno", usados por la lista de
     * eleccion vieja que esta fila reemplaza) -- frase completa para el
     * slot inferior del panel derecho. */
    AURA_STR_REPEAT_ROW_TOP,
    AURA_STR_REPEAT_SUMMARY_ALL,
    AURA_STR_REPEAT_SUMMARY_ONE,

    AURA_STR_ABOUT_MY_IPOD,

    /* D-279: cuarto segmento de la barra de almacenamiento (encargo del
     * dueno) -- residual "Otros" (firmware, listas, cache, sueltos), no
     * una categoria de contenido real como las otras tres. */
    AURA_STR_ABOUT_OTHER,
    /* D-282: quinto segmento -- "Sistema" separa de "Otros" lo que ocupa
     * el propio firmware de Aura (/.rockbox/) del resto del residual. */
    AURA_STR_ABOUT_SYSTEM,
    /* D-283 (Estado 3, PLAN-about-fixes.md Q7): reemplaza la pagina 3
     * vieja ("Basado en Rockbox" solo) -- creditos completos con la
     * mencion GPL v2 y la URL del codigo fuente que la licencia exige
     * (GPL v2 SS3), en el ancho angosto de la region derecha con el
     * tile persistente (Q8). */
    AURA_STR_ABOUT_CREDITS_BODY,
    /* D-284: identidad de la unidad de almacenamiento en la pagina de
     * almacenamiento de "Acerca de" -- tipo detectado por ATA IDENTIFY
     * (palabra 217, "nominal media rotation rate": 1 = estado solido) mas
     * el modelo que reporta la unidad. En el simulador no hay ATA: se dice
     * explicitamente que es un disco simulado, nunca se inventa un modelo. */
    AURA_STR_ABOUT_DRIVE_SSD,
    AURA_STR_ABOUT_DRIVE_HDD,
    AURA_STR_ABOUT_DRIVE_UNKNOWN,
    AURA_STR_ABOUT_DRIVE_SIMULATED,

    /* D-289 (sistema de temas): fila de Ajustes y titulo de la
     * pantalla de eleccion de estilo instalado. */
    AURA_STR_SETTINGS_STYLE,

    AURA_STR_COUNT,
} aura_str_id_t;

/* Devuelve la cadena para el idioma activo (aura_settings.language). */
const char *aura_str(aura_str_id_t id);

#endif /* AURA_LANG_H */
