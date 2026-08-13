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
#include "aura_lang.h"

/* Contexto de filtro para las pantallas "hijas" de una eleccion previa
 * (p.ej. AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST filtra por el artista
 * elegido en AURA_SCREEN_MUSIC_ARTISTS). -1 = sin filtro. Vive aca (no
 * en aura_nav, que es generico) porque es logica propia del dominio
 * musical -- ver D-021. */
static int32_t s_artist_seek = -1;
static int32_t s_album_seek = -1;
static int32_t s_genre_seek = -1;
static int32_t s_composer_seek = -1;
static int s_filter_generation = 0;

/* Suficiente para varios cientos de valores unicos (artista/album/genero);
 * tagcache_search_set_uniqbuf() ignora este buffer para tags no-unicos
 * (tag_title), ver D-021. */
static uint32_t s_uniqbuf[2048];

void aura_music_select_artist(int32_t seek) { s_artist_seek = seek; s_filter_generation++; }
void aura_music_select_album(int32_t seek)  { s_album_seek = seek; s_filter_generation++; }
void aura_music_select_composer(int32_t seek)
{
    s_composer_seek = seek;
    s_filter_generation++;
}

void aura_music_select_genre(int32_t seek)  { s_genre_seek = seek; s_filter_generation++; }

void aura_music_reset_filters(void)
{
    s_composer_seek = -1;
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
    static bool update_triggered = false;

    if (tagcache_is_fully_initialized() && !tagcache_is_usable() && !scan_triggered)
    {
        tagcache_rebuild();
        scan_triggered = true;
    }

    /* Base YA usable: dispararle una pasada de actualizacion UNA vez
     * por arranque (Q_START_SCAN si funciona aqui, porque tc_stat.ready
     * es true). Sin esto, una base construida en un arranque anterior
     * jamas se entera de la musica que Aura Studio sincronizo despues:
     * Rockbox solo re-escanea cuando el usuario entra a "Base de datos
     * > Actualizar ahora", pantalla que Aura no tiene por diseno --
     * exactamente el bug reportado en hardware real (2026-08-13):
     * archivos copiados por USB, biblioteca vacia en el aparato. La
     * pasada corre en el hilo de tagcache, en fondo; con la biblioteca
     * sin cambios es barata (solo recorre directorios y compara). */
    if (tagcache_is_usable() && !update_triggered)
    {
        tagcache_start_scan();
        update_triggered = true;
    }

    return tagcache_is_usable();
}

static aura_str_id_t untagged_label_for(int tag)
{
    switch (tag)
    {
    case tag_artist: return AURA_STR_UNKNOWN_ARTIST;
    case tag_genre:  return AURA_STR_UNKNOWN_GENRE;
    case tag_title:  return AURA_STR_UNKNOWN_TITLE;
    default:         return AURA_STR_UNKNOWN_ALBUM;
    }
}

/* Nombre del archivo de la pista actual de la busqueda, sin ruta ni
 * extension -- respaldo de titulo para pistas sin etiquetar. Devuelve
 * false si tagcache no puede darlo (entonces manda la etiqueta
 * natural). */
static bool title_from_filename(struct tagcache_search *tcs, char *out, size_t outsz)
{
    char path[MAX_PATH];
    const char *base;
    char *dot;

    if (!tagcache_retrieve(tcs, tcs->idx_id, tag_filename, path, sizeof(path)))
        return false;

    base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (!base[0])
        return false;

    strlcpy(out, base, outsz);
    dot = strrchr(out, '.');
    if (dot && dot != out)
        *dot = '\0';
    return out[0] != '\0';
}

/* Etiqueta VISIBLE de la pista actual de una busqueda abierta: el
 * titulo real, o el nombre de archivo si la pista no tiene tag de
 * titulo. "<Untagged>" es jerga tecnica de tagcache y nunca puede
 * llegar a la pantalla (regla dura del proyecto) -- este es el unico
 * punto que decide esa sustitucion, compartido por las listas y por
 * los resultados de Busqueda. */
void aura_music_visible_title(struct tagcache_search *tcs, const char *raw,
                               char *out, size_t outsz)
{
    if (strcmp(raw, UNTAGGED) != 0)
    {
        strlcpy(out, raw, outsz);
        return;
    }
    if (!title_from_filename(tcs, out, outsz))
        strlcpy(out, aura_str(AURA_STR_UNKNOWN_TITLE), outsz);
}

/* Ejecuta una busqueda tagcache sobre `tag`, aplicando los filtros que
 * correspondan segun la pantalla, y vuelca hasta `max` resultados en
 * `out`. Devuelve la cantidad de items. */
/* Comparacion de etiquetas para el orden alfabetico de las listas:
 * sin distinguir mayusculas, y los digitos antes que las letras (mismo
 * criterio visible del riel A-Z, que agrupa numeros bajo '#'). */
static int label_cmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb)
            return (int)ca - (int)cb;
        a++; b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static void sort_items_by_label(aura_music_item_t *items, long *nums, int n)
{
    int a, b;

    for (a = 1; a < n; a++)
    {
        aura_music_item_t key = items[a];
        long key_num = nums ? nums[a] : 0;

        b = a - 1;
        while (b >= 0 && label_cmp(items[b].label, key.label) > 0)
        {
            items[b + 1] = items[b];
            if (nums)
                nums[b + 1] = nums[b];
            b--;
        }
        items[b + 1] = key;
        if (nums)
            nums[b + 1] = key_num;
    }
}

static int run_search(int tag, bool use_artist, bool use_album, bool use_genre,
                       bool use_composer, aura_music_item_t *out, int max)
{
    struct tagcache_search tcs;
    char buf[TAGCACHE_BUFSZ];
    int n = 0;

    if (!tagcache_is_usable())
        return 0;
    if (!tagcache_search(&tcs, tag))
        return 0;

    tagcache_search_set_uniqbuf(&tcs, s_uniqbuf, sizeof(s_uniqbuf));

    if (use_composer && s_composer_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_composer, s_composer_seek);
    if (use_artist && s_artist_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_artist, s_artist_seek);
    if (use_album && s_album_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_album, s_album_seek);
    if (use_genre && s_genre_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_genre, s_genre_seek);
    if (use_composer && s_composer_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_composer, s_composer_seek);

    /* Numero de pista real por fila -- solo interesa para las listas
     * de canciones DE UN ALBUM (ordenarlas como el disco, D-118/nota);
     * en el resto de las busquedas se ignora. */
    static long s_tracknums[AURA_MUSIC_MAX_ITEMS];

    while (n < max && tagcache_get_next(&tcs, buf, sizeof(buf)))
    {
        /* "<Untagged>" (tagcache.h) es jerga tecnica -- regla dura del
         * proyecto: nunca visible. Se sustituye por la etiqueta natural
         * del tipo de tag; el seek se conserva tal cual, el filtro
         * sigue funcionando sobre la entrada real. */
        if (!strcmp(buf, UNTAGGED))
        {
            /* Una CANCION sin tag de titulo se muestra con el nombre
             * de su archivo, sin extension ni ruta -- exactamente lo
             * que hace el iPod original (y Rockbox) con material sin
             * etiquetar (correccion 2026-08-13: la biblioteca real del
             * dueno son exports de CD en AIFF que no traen chunk ID3
             * alguno -- verificado chunk por chunk: solo FVER/COMM/SSND
             * --, y la lista entera se veia como "Sin titulo" repetido,
             * indistinguible). Los demas tags (album/artista/genero)
             * si conservan su etiqueta natural: ahi el nombre de
             * archivo no aporta nada. */
            if (tag == tag_title && title_from_filename(&tcs, out[n].label,
                                                         AURA_MUSIC_ITEM_LEN))
                ; /* listo: nombre de archivo */
            else
                strlcpy(out[n].label, aura_str(untagged_label_for(tag)),
                        AURA_MUSIC_ITEM_LEN);
        }
        else
            strlcpy(out[n].label, buf, AURA_MUSIC_ITEM_LEN);
        out[n].seek = (tag == tag_title) ? tcs.idx_id : tcs.result_seek;
        s_tracknums[n] = tagcache_get_numeric(&tcs, tag_tracknumber);
        n++;
    }

    tagcache_search_finish(&tcs);

    /* Orden ALFABETICO por etiqueta en toda lista que no sea las
     * canciones de un album (encargo 2026-08-13: "organizadas
     * alfabeticamente", como el original). Insercion estable, sin
     * distinguir mayusculas; los seek y numeros de pista viajan con su
     * etiqueta. La lista de un album se ordena por numero de pista mas
     * abajo, y build_playlist_from_songs() aplica EL MISMO criterio
     * para que el indice elegido en pantalla y la cancion que arranca
     * sean siempre la misma. */
    if (!(tag == tag_title && use_album))
        sort_items_by_label(out, s_tracknums, n);

    /* Canciones de un album: orden del DISCO, no del indice de tagcache
     * (encargo del dueno del diseno 2026-08-12, cierra la nota de
     * D-118). Insercion estable: pistas sin numero (<=0) van al final
     * conservando su orden relativo. Mismo criterio en el playlist de
     * reproduccion (build_playlist_from_songs) -- el indice visible y
     * la pista que suena SIEMPRE coinciden. */
    if (tag == tag_title && use_album)
    {
        int a, b;
        for (a = 1; a < n; a++)
        {
            aura_music_item_t key = out[a];
            long key_num = s_tracknums[a] > 0 ? s_tracknums[a] : 0x7FFFFFFF;
            b = a - 1;
            while (b >= 0)
            {
                long b_num = s_tracknums[b] > 0 ? s_tracknums[b] : 0x7FFFFFFF;
                if (b_num <= key_num)
                    break;
                out[b + 1] = out[b];
                s_tracknums[b + 1] = s_tracknums[b];
                b--;
            }
            out[b + 1] = key;
            s_tracknums[b + 1] = key_num == 0x7FFFFFFF ? 0 : key_num;
        }
    }
    return n;
}

int aura_music_browse(aura_screen_id_t screen, aura_music_item_t *out, int max_items)
{
    switch (screen)
    {
    case AURA_SCREEN_MUSIC_ARTISTS:
        return run_search(tag_artist, false, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_ALBUMS:
    case AURA_SCREEN_MUSIC_COVERFLOW:
        return run_search(tag_album, false, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_ALBUMS_BY_ARTIST:
        return run_search(tag_album, true, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_SONGS:
        return run_search(tag_title, false, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_SONGS_BY_ALBUM:
        return run_search(tag_title, true, true, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_SONGS_BY_ARTIST:
        return run_search(tag_title, true, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_SONGS_BY_GENRE:
        return run_search(tag_title, false, false, true, false, out, max_items);
    case AURA_SCREEN_MUSIC_GENRES:
        return run_search(tag_genre, false, false, false, false, out, max_items);
    /* Arbol del original (2026-08-13): Autores es la misma jerarquia
     * que Artistas pero sobre tag_composer; Artistas por genero y
     * Recopilaciones reusan los mismos filtros. */
    case AURA_SCREEN_MUSIC_COMPOSERS:
        return run_search(tag_composer, false, false, false, false, out, max_items);
    case AURA_SCREEN_MUSIC_ALBUMS_BY_COMPOSER:
        return run_search(tag_album, false, false, false, true, out, max_items);
    case AURA_SCREEN_MUSIC_SONGS_BY_COMPOSER:
        return run_search(tag_title, false, false, false, true, out, max_items);
    case AURA_SCREEN_MUSIC_ARTISTS_BY_GENRE:
        return run_search(tag_artist, false, false, true, false, out, max_items);
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
    else if (!strcmp(out, UNTAGGED))
        strlcpy(out, aura_str(AURA_STR_UNKNOWN_ARTIST), outsz);
    return found;
}

static bool build_playlist_from_songs(aura_screen_id_t songs_screen)
{
    struct tagcache_search tcs;
    char path[MAX_PATH];
    int tag = tag_title;
    /* Los mismos filtros con los que se construyo la lista visible
     * (aura_music_browse) -- el playlist tiene que coincidir con lo que
     * el usuario esta viendo, incluidas las jerarquias del original
     * agregadas el 2026-08-13 (por artista, por autor). */
    bool use_artist = (songs_screen == AURA_SCREEN_MUSIC_SONGS_BY_ALBUM
                       || songs_screen == AURA_SCREEN_MUSIC_SONGS_BY_ARTIST);
    bool use_album = (songs_screen == AURA_SCREEN_MUSIC_SONGS_BY_ALBUM);
    bool use_genre = (songs_screen == AURA_SCREEN_MUSIC_SONGS_BY_GENRE);
    bool use_composer = (songs_screen == AURA_SCREEN_MUSIC_SONGS_BY_COMPOSER);
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
    if (use_composer && s_composer_seek >= 0)
        tagcache_search_add_filter(&tcs, tag_composer, s_composer_seek);

    playlist_create(NULL, NULL);

    if (use_album)
    {
        /* Mismo orden del DISCO que muestra la lista (ver run_search):
         * primero se recolectan (idx_id, tracknumber), se ordenan, y
         * recien entonces se insertan -- el indice elegido en pantalla
         * y la cancion que arranca siempre son la misma. */
        static int32_t s_ids[AURA_MUSIC_MAX_ITEMS];
        static long s_nums[AURA_MUSIC_MAX_ITEMS];
        int n = 0, a, b;

        while (n < AURA_MUSIC_MAX_ITEMS && tagcache_get_next(&tcs, path, sizeof(path)))
        {
            s_ids[n] = tcs.idx_id;
            s_nums[n] = tagcache_get_numeric(&tcs, tag_tracknumber);
            if (s_nums[n] <= 0)
                s_nums[n] = 0x7FFFFFFF; /* sin numero: al final, orden estable */
            n++;
        }
        /* OJO: la busqueda sigue ABIERTA -- tagcache_retrieve() abajo
         * la necesita activa; el finish va al final del bloque. */

        for (a = 1; a < n; a++)
        {
            int32_t key_id = s_ids[a];
            long key_num = s_nums[a];
            b = a - 1;
            while (b >= 0 && s_nums[b] > key_num)
            {
                s_ids[b + 1] = s_ids[b];
                s_nums[b + 1] = s_nums[b];
                b--;
            }
            s_ids[b + 1] = key_id;
            s_nums[b + 1] = key_num;
        }

        for (a = 0; a < n; a++)
        {
            if (tagcache_retrieve(&tcs, s_ids[a], tag_filename, path, sizeof(path)))
            {
                playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST, false, true);
                inserted++;
            }
        }
        tagcache_search_finish(&tcs);
        return inserted > 0;
    }

    /* Resto de listas: orden ALFABETICO por titulo, el mismo que
     * muestra run_search() -- si el playlist quedara en orden de
     * tagcache, elegir la fila N reproduciria otra cancion. Se
     * recolecta (idx_id, titulo), se ordena y se inserta despues; la
     * busqueda sigue ABIERTA porque tagcache_retrieve() la necesita. */
    {
        static int32_t s_ids[AURA_MUSIC_MAX_ITEMS];
        static char s_titles[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
        int n = 0, a, b;

        while (n < AURA_MUSIC_MAX_ITEMS && tagcache_get_next(&tcs, path, sizeof(path)))
        {
            s_ids[n] = tcs.idx_id;
            strlcpy(s_titles[n], path, AURA_MUSIC_ITEM_LEN);
            n++;
        }

        for (a = 1; a < n; a++)
        {
            int32_t key_id = s_ids[a];
            char key_title[AURA_MUSIC_ITEM_LEN];

            strlcpy(key_title, s_titles[a], AURA_MUSIC_ITEM_LEN);
            b = a - 1;
            while (b >= 0 && label_cmp(s_titles[b], key_title) > 0)
            {
                s_ids[b + 1] = s_ids[b];
                strlcpy(s_titles[b + 1], s_titles[b], AURA_MUSIC_ITEM_LEN);
                b--;
            }
            s_ids[b + 1] = key_id;
            strlcpy(s_titles[b + 1], key_title, AURA_MUSIC_ITEM_LEN);
        }

        for (a = 0; a < n; a++)
        {
            if (tagcache_retrieve(&tcs, s_ids[a], tag_filename, path, sizeof(path)))
            {
                playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST, false, true);
                inserted++;
            }
        }
        tagcache_search_finish(&tcs);
        return inserted > 0;
    }

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

/* "Canciones aleat." del menu de inicio (encargo 2026-08-13): toda la
 * biblioteca en orden aleatorio, empezando a sonar de inmediato. Reusa
 * el constructor de playlist sin filtros y el barajado real del nucleo
 * (el mismo de playlist_randomise, no un indice al azar). */
/* Reproduce UNA pista por su idx_id de tagcache (resultados de
 * Busqueda, 2026-08-13): playlist de un solo elemento -- la busqueda no
 * define un album ni un orden, asi que encolar el resto seria inventar
 * un contexto que el usuario no pidio. */
bool aura_music_play_track(int32_t idx_id)
{
    struct tagcache_search tcs;
    char path[MAX_PATH];
    bool ok = false;

    if (!tagcache_is_usable())
        return false;
    if (!tagcache_search(&tcs, tag_title))
        return false;

    if (tagcache_retrieve(&tcs, idx_id, tag_filename, path, sizeof(path)))
    {
        playlist_create(NULL, NULL);
        if (playlist_insert_track(NULL, path, PLAYLIST_INSERT_LAST, false, true) >= 0)
        {
            playlist_start(0, 0, 0);
            ok = true;
        }
    }
    tagcache_search_finish(&tcs);
    return ok;
}

bool aura_music_play_all_shuffled(void)
{
    if (!build_playlist_from_songs(AURA_SCREEN_MUSIC_SONGS))
        return false;

    playlist_randomise(NULL, current_tick, true);
    playlist_start(0, 0, 0);
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
