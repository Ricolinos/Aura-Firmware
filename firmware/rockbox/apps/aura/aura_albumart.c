#include <string.h>

#include "config.h"
#include "tagcache.h"
#include "metadata.h"
#include "lcd.h"
#include "file.h"
#include "recorder/albumart.h"
#include "recorder/bmp.h"
#include "recorder/jpeg_load.h"
#include "string-extra.h"

#include "aura_albumart.h"
#include "apple2026_shell.h"
#include "aura_art.h"

/* Buffer de trabajo para decodificar+remuestrear (FORMAT_RESIZE
 * necesita bastante mas espacio que el bitmap final, ver
 * BM_SCALED_SIZE en recorder/bmp.h); 64KB alcanza sobrado para
 * cualquier tamano de caratula que use Aura (<=100px). */
static unsigned char s_decode_scratch[64 * 1024];

/* Busca el path/artista/album de una pista cualquiera del album dado.
 * Devuelve false si el album no tiene ninguna pista indexada (no
 * deberia pasar si `album_seek` vino de una busqueda real, pero se
 * cubre por robustez). */
static bool find_any_track_in_album(int32_t album_seek, char *path, size_t path_sz,
                                     char *artist, size_t artist_sz,
                                     char *album, size_t album_sz)
{
    struct tagcache_search tcs;
    bool found = false;

    if (!tagcache_is_usable())
        return false;
    if (!tagcache_search(&tcs, tag_filename))
        return false;

    tagcache_search_add_filter(&tcs, tag_album, album_seek);

    if (tagcache_get_next(&tcs, path, path_sz))
    {
        tagcache_retrieve(&tcs, tcs.idx_id, tag_artist, artist, artist_sz);
        tagcache_retrieve(&tcs, tcs.idx_id, tag_album, album, album_sz);
        found = true;
    }

    tagcache_search_finish(&tcs);
    return found;
}

static void generate_reflection(const aura_albumart_t *art)
{
    /* Delega en el modulo compartido (Fase 29, D-076/D-077): esta era la
     * unica implementacion del sistema hasta ahora, extraida para que
     * Cover Flow (aura_coverflow.c) y Ahora suena (Fase 30) usen la
     * misma en vez de reimplementarla cada uno. */
    aura_art_generate_reflection((const fb_data *)art->cover_data,
                                  (fb_data *)art->reflection_data,
                                  art->size, a26_color(A26_SHELL_BG));
}

bool aura_albumart_load_for_album(int32_t album_seek, aura_albumart_t *out)
{
    char path[MAX_PATH];
    char artist[128] = "";
    char album[128] = "";
    struct mp3entry fake_id3;
    struct dim dim = { out->size, out->size };
    char art_path[MAX_PATH];
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int len, ret;
    struct bitmap bm;

    out->valid = false;

    if (!find_any_track_in_album(album_seek, path, sizeof(path),
                                  artist, sizeof(artist), album, sizeof(album)))
        return false;

    memset(&fake_id3, 0, sizeof(fake_id3));
    strlcpy(fake_id3.path, path, sizeof(fake_id3.path));
    fake_id3.artist = artist;
    fake_id3.album = album;

    if (!find_albumart(&fake_id3, art_path, sizeof(art_path), &dim))
        return false;

    /* FORMAT_RESIZE necesita, ademas del bitmap final, espacio de
     * trabajo para el remuestreo (ver BM_SCALED_SIZE en recorder/bmp.h
     * -- unas 9x el ancho en uint32_t). Se decodifica en un scratch
     * generoso y se copia solo el resultado final (arranca al
     * comienzo del buffer) a out->cover_data. */
    bm.width = out->size;
    bm.height = out->size;
    bm.data = (char *)s_decode_scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    len = (int)strlen(art_path);
    if (len > 4 && !strcasecmp(art_path + len - 4, ".bmp"))
        ret = read_bmp_file(art_path, &bm, sizeof(s_decode_scratch), format, NULL);
    else
        ret = read_jpeg_file(art_path, &bm, sizeof(s_decode_scratch), format, NULL);

    if (ret <= 0)
        return false;

    memcpy(out->cover_data, s_decode_scratch,
           (size_t)out->size * out->size * sizeof(fb_data));

    out->valid = true;
    generate_reflection(out);
    return true;
}
