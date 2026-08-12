#include <string.h>
#include <stdio.h>

/* tagcache.h chequea "#ifdef HAVE_TAGCACHE" antes de incluir config.h
 * el mismo (esa macro la define config.h via config/ipod6g.h); si
 * tagcache.h fuera el primer header del archivo, HAVE_TAGCACHE aun no
 * existiria y todo su contenido desaparecería en silencio. Ver D-021. */
#include "config.h"
#include "tagcache.h"
#include "playlist.h"
#include "playlist_catalog.h"
#include "audio.h"
#include "dir.h"
#include "file.h"
#include "filetypes.h"
#include "string-extra.h"

#include "aura_music.h"

/* Contexto de filtro para las pantallas "hijas" de una eleccion previa
 * (p.ej. AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST filtra por el artista
 * elegido en AURA_SCREEN_MUSIC_ARTISTS). -1 = sin filtro. Vive aca (no
 * en aura_nav, que es generico) porque es logica propia del dominio
 * musical -- ver D-021. */
static int32_t s_artist_seek = -1;
static int32_t s_album_seek = -1;
static int32_t s_genre_seek = -1;
static int s_filter_generation = 0;

/* Suficiente para varios cientos de valores unicos (artista/album/genero);
 * tagcache_search_set_uniqbuf() ignora este buffer para tags no-unicos
 * (tag_title), ver D-021. */
static uint32_t s_uniqbuf[2048];

void aura_music_select_artist(int32_t seek) { s_artist_seek = seek; s_filter_generation++; }
void aura_music_select_album(int32_t seek)  { s_album_seek = seek; s_filter_generation++; }
void aura_music_select_genre(int32_t seek)  { s_genre_seek = seek; s_filter_generation++; }

void aura_music_reset_filters(void)
{
    s_artist_seek = -1;
    s_album_seek = -1;
    s_genre_seek = -1;
    s_filter_generation++;
}

int aura_music_filter_generation(void)
{
    return s_filter_generation;
}

bool aura_music_db_ready(void)
{
    /* Rockbox no escanea la biblioteca solo con tagcache_init(): en el
     * firmware original eso lo dispara el usuario a mano desde
     * "Base de datos > Inicializar ahora" en el navegador de archivos.
     * Aura no tiene esa pantalla (ni navegacion de carpetas, por
     * diseno), asi que dispara el escaneo ella misma la primera vez
     * que alguien entra a Musica y la base de datos no esta lista.
     *
     * tagcache_start_scan() (Q_START_SCAN) NO sirve para esto: su
     * manejador en tagcache.c hace "if (!tc_stat.ready) break;" antes
     * de escanear nada -- solo actualiza una base YA construida. Para
     * el build inicial (sin base de datos previa) hace falta
     * tagcache_rebuild() (Q_REBUILD), que no tiene esa condicion.
     *
     * Ojo con el orden: tagcache_init() determina si YA existe una
     * base valida en disco de forma asincrona, en un hilo de fondo,
     * y tarda ~1s (ver tagcache_commit_finalize() en tagcache.c). Si
     * se dispara tagcache_rebuild() antes de que esa determinacion
     * termine (tagcache_is_fully_initialized() == false todavia), se
     * borra y reconstruye una base que en realidad ya estaba lista --
     * cada arranque terminaria re-escaneando toda la biblioteca desde
     * cero en vez de reutilizar la del arranque anterior. Por eso se
     * espera a que la determinacion este hecha antes de decidir si
     * hace falta reconstruir. Ver D-021. */
    static bool scan_triggered = false;

    if (tagcache_is_fully_initialized() && !tagcache_is_usable() && !scan_triggered)
    {
        tagcache_rebuild();
        scan_triggered = true;
    }

    return tagcache_is_usable();
}

/* Ejecuta una busqueda tagcache sobre `tag`, aplicando los filtros que
 * correspondan segun la pantalla, y vuelca hasta `max` resultados en
 * `out`. Devuelve la cantidad de items. */
static int run_search(int tag, bool use_artist, bool use_album, bool use_genre,
                       aura_music_item_t *out, int max)
{
    struct tagcache_search tcs;
    char buf[TAGCACHE_BUFSZ];
    int n = 0;

    if (!tagcache_is_usable())
        return 0;
    if (!tagcache_search(&tcs, tag))
        return 0;

    tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, sizeof(s_uniqbuf));

    if (use_artist && s_artist_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_artist, s_artist_seek);
    if (use_album && s_album_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_album, s_album_seek);
    if (use_genre && s_genre_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_genre, s_genre_seek);

    while (n < max && tagcache_get_next(&tcs, buf, sizeof(buf)))
    {
        strlcpy(out[n].label, buf, AURA_MUSIC_ITEM_LEN);
        out[n].seek = (tag == tag_title) ? tcs.idx_id : tcs.result_seek;
        n++;
    }

    tagcache_search_finish(&tcs);
    return n;
}

int aura_music_browse(aura_screen_id_t screen, aura_music_item_t *out, int max_items)
{
    switch (screen)
    {
    case AURA_SCREEN_MUSIC_ARTISTS:
        return run_search(tag_artist, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_ALBUMS:
    case AURA_SCREEN_MUSIC_COVERFLOW:
        return run_search(tag_album, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST:
        return run_search(tag_album, true, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_SONGS:
        return run_search(tag_title, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_SONGS_BY_ALBUM:
        return run_search(tag_title, true, true, false, out, max_items);
    case AURA_SCREEN_MUSIC_SONGS_BY_GENRE:
        return run_search(tag_title, false, false, true, out, max_items);
    case AURA_SCREEN_MUSIC_GENRES:
        return run_search(tag_genre, false, false, false, out, max_items);
    default:
        return 0;
    }
}

bool aura_music_album_artist(int32_t album_seek, char *out, size_t outsz)
{
    struct tagcache_search tcs;
    bool found = false;

    out[0] = '\0';
    if (!tagcache_is_usable())
        return false;
    if (!tagcache_search(&tcs, tag_artist))
        return false;

    tagcache_search_add_filter(&tcs, tag_album, album_seek);
    if (tagcache_get_next(&tcs, out, outsz))
        found = true;
    tagcache_search_finish(&tcs);
    if (!found)
        out[0] = '\0';
    return found;
}

static bool build_playlist_from_songs(aura_screen_id_t songs_screen)
{
    struct tagcache_search tcs;
    char path[MAX_PATH];
    int tag = tag_title;
    bool use_artist = (songs_screen == AURA_SCREEN_MUSIC_SONGS_BY_ALBUM);
    bool use_album = (songs_screen == AURA_SCREEN_MUSIC_SONGS_BY_ALBUM);
    bool use_genre = (songs_screen == AURA_SCREEN_MUSIC_SONGS_BY_GENRE);
    int inserted = 0;

    if (!tagcache_is_usable())
        return false;
    if (!tagcache_search(&tcs, tag))
        return false;

    tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, sizeof(s_uniqbuf));
    if (use_artist && s_artist_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_artist, s_artist_seek);
    if (use_album && s_album_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_album, s_album_seek);
    if (use_genre && s_genre_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_genre, s_genre_seek);

    playlist_create(NULL, NULL);

    while (tagcache_get_next(&tcs, path, sizeof(path)))
    {
        /* tag_title da el titulo en `path`, no el nombre de archivo:
         * se recupera la ruta real con tagcache_retrieve() usando el
         * idx_id de esta misma entrada. */
        if (tagcache_retrieve(&tcs, tcs.idx_id, tag_filename, path, sizeof(path)))
        {
            playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST, false, true);
            inserted++;
        }
    }

    tagcache_search_finish(&tcs);
    return inserted > 0;
}

bool aura_music_play_songs(aura_screen_id_t songs_screen, int start_index)
{
    if (!build_playlist_from_songs(songs_screen))
        return false;

    playlist_start(start_index, 0, 0);
    return true;
}

int aura_music_list_playlists(char labels[][AURA_MUSIC_ITEM_LEN], int max_items)
{
    char dir[MAX_PATH];
    DIR *d;
    struct DIRENT *entry;
    int n = 0;

    catalog_get_directory(dir, sizeof(dir));

    d = opendir(dir);
    if (!d)
        return 0;

    while (n < max_items && (entry = readdir(d)) != NULL)
    {
        size_t len = strlen(entry->d_name);
        bool is_m3u = (len > 4 && !strcasecmp(entry->d_name + len - 4, ".m3u"));
        bool is_m3u8 = (len > 5 && !strcasecmp(entry->d_name + len - 5, ".m3u8"));

        if (!is_m3u && !is_m3u8)
            continue;

        strlcpy(labels[n], entry->d_name, AURA_MUSIC_ITEM_LEN);
        n++;
    }
    closedir(d);

    return n;
}

void aura_music_playlist_display_name(const char *filename, char *out, size_t outsz)
{
    size_t len;

    strlcpy(out, filename, outsz);
    len = strlen(out);
    if (len > 4 && !strcasecmp(out + len - 4, ".m3u"))
        out[len - 4] = '\0';
    else if (len > 5 && !strcasecmp(out + len - 5, ".m3u8"))
        out[len - 5] = '\0';
}

bool aura_music_play_playlist(int index)
{
    char dir[MAX_PATH];
    char labels[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
    int n;

    catalog_get_directory(dir, sizeof(dir));
    n = aura_music_list_playlists(labels, AURA_MUSIC_MAX_ITEMS);
    if (index < 0 || index >= n)
        return false;

    if (playlist_create(dir, labels[index]) == -1)
        return false;

    playlist_start(0, 0, 0);
    return true;
}

bool aura_music_add_track_to_playlist(int index, const char *track_path)
{
    char dir[MAX_PATH];
    char full_path[MAX_PATH + AURA_MUSIC_ITEM_LEN + 1];
    char labels[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
    int n;

    catalog_get_directory(dir, sizeof(dir));
    n = aura_music_list_playlists(labels, AURA_MUSIC_MAX_ITEMS);
    if (index < 0 || index >= n)
        return false;

    snprintf(full_path, sizeof(full_path), "%s/%s", dir, labels[index]);
    return catalog_insert_into(full_path, false, track_path, FILE_ATTR_AUDIO) == 0;
}
