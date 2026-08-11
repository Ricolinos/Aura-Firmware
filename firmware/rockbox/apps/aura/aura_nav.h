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
    AURA_SCREEN_VIDEOS,
    AURA_SCREEN_PHOTOS,
    AURA_SCREEN_PHOTO_VIEWER,
    AURA_SCREEN_NOWPLAYING,
    AURA_SCREEN_SETTINGS,
    AURA_SCREEN_SETTINGS_THEME,
    AURA_SCREEN_SETTINGS_GRAPHICS,
    AURA_SCREEN_SETTINGS_EQ,
    AURA_SCREEN_SETTINGS_BRIGHTNESS,
    AURA_SCREEN_SETTINGS_LANGUAGE,
    AURA_SCREEN_SETTINGS_ABOUT,
    AURA_SCREEN_SETTINGS_SHUFFLE,
    AURA_SCREEN_SETTINGS_REPEAT,
    AURA_SCREEN_SETTINGS_BACKLIGHT,
    AURA_SCREEN_SETTINGS_SLEEPTIMER,
    AURA_SCREEN_SETTINGS_VOLUME_LIMIT,
    AURA_SCREEN_SETTINGS_CLICKER,
    AURA_SCREEN_SETTINGS_MAINMENU,
    AURA_SCREEN_SETTINGS_RESET,
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
