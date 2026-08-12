/* Navegacion de la biblioteca musical (base de datos indexada de
 * Rockbox, no navegacion de carpetas) y arranque de reproduccion.
 *
 * Envuelve tagcache (apps/tagcache.h) y playlist (apps/playlist.h) del
 * fork; Aura solo decide que consultar y cuando. Ver DECISIONS.md D-021.
 */
#ifndef AURA_MUSIC_H
#define AURA_MUSIC_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "aura_nav.h"

#define AURA_MUSIC_MAX_ITEMS 300
#define AURA_MUSIC_ITEM_LEN  64

typedef struct {
    char label[AURA_MUSIC_ITEM_LEN];
    int32_t seek; /* tagcache: result_seek (agrupadores) o idx_id (titulo) */
} aura_music_item_t;

/* True si la base de datos de Rockbox esta lista para consultarse. */
bool aura_music_db_ready(void);

/* Llena `out` (hasta `max_items`) con los resultados de navegar
 * `screen`, aplicando el contexto de filtro activo (ver
 * aura_music_select_*). Devuelve la cantidad de items. */
int aura_music_browse(aura_screen_id_t screen, aura_music_item_t *out, int max_items);

/* Fija el contexto de filtro antes de empujar la pantalla hija
 * correspondiente (p.ej. tras elegir un artista, antes de mostrar sus
 * albumes). `seek` es el aura_music_item_t.seek del item elegido. */
void aura_music_select_artist(int32_t seek);
void aura_music_select_album(int32_t seek);
void aura_music_select_genre(int32_t seek);

/* Limpia todo el contexto de filtro; llamar al entrar a cualquier modo
 * de navegacion desde cero (Artistas/Albumes/Canciones/Generos) para
 * no arrastrar filtros de una sesion de navegacion anterior. */
void aura_music_reset_filters(void);

/* Artista del album identificado por `album_seek` (el del primer track
 * encontrado -- suficiente para la linea de artista de Cover Flow;
 * albumes multi-artista mostraran el primero, mismo criterio que la
 * busqueda de caratula). false = sin resultado, `out` queda vacio. */
bool aura_music_album_artist(int32_t album_seek, char *out, size_t outsz);

/* Se incrementa en cada llamada a aura_music_select_xxx o a
 * aura_music_reset_filters. aura_screens.c lo usa como parte de la
 * clave de su cache de listas:
 * el id de pantalla solo no alcanza para saber si hay que volver a
 * consultar tagcache (p.ej. re-entrar a "Albumes por artista" con un
 * artista distinto reutiliza el mismo id de pantalla). */
int aura_music_filter_generation(void);

/* Reconstruye la playlist dinamica con todas las canciones que
 * `screen` está mostrando actualmente (mismo filtro que aura_music_browse)
 * y arranca la reproduccion en start_index. */
bool aura_music_play_songs(aura_screen_id_t songs_screen, int start_index);

/* Catalogo de playlists guardadas (archivos .m3u/.m3u8). `labels` trae
 * el nombre de archivo completo, CON extension -- hace falta tal cual
 * para aura_music_play_playlist()/aura_music_add_track_to_playlist(),
 * que vuelven a resolver la ruta real a partir de el. Para mostrarle el
 * nombre al usuario, usar aura_music_playlist_display_name() (Fase 32,
 * D-081: mostrar ".m3u8" en pantalla es jerga tecnica de archivo, doc
 * Principio 7 -- vacio real encontrado en el barrido final, no en el
 * armado original). */
int aura_music_list_playlists(char labels[][AURA_MUSIC_ITEM_LEN], int max_items);
bool aura_music_play_playlist(int index);

/* Nombre de playlist para MOSTRAR (sin extension .m3u/.m3u8) -- separado
 * de aura_music_list_playlists() porque ese nombre con extension sigue
 * haciendo falta para las operaciones de archivo reales. */
void aura_music_playlist_display_name(const char *filename, char *out, size_t outsz);

/* Agrega `track_path` (ruta completa) al final de la playlist guardada
 * en la posicion `index` de aura_music_list_playlists() -- modo "Añadir
 * a lista" de la rueda en Ahora suena (Fase 30, doc SS5.3). No cambia
 * la reproduccion actual, solo el archivo .m3u8 en disco. */
bool aura_music_add_track_to_playlist(int index, const char *track_path);

#endif /* AURA_MUSIC_H */
