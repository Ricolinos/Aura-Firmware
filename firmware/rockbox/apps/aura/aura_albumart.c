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
#include <stdio.h>

#include "config.h"
#include "tagcache.h"
#include "metadata.h"
#include "lcd.h"
#include "file.h"
#include "dir.h"
#include "debug.h"
#include "rbpaths.h"
#include "recorder/albumart.h"
#include "recorder/bmp.h"
#include "recorder/jpeg_load.h"
#include "string-extra.h"
#include "playlist_catalog.h"

#include "aura_settings.h"
#include "aura_albumart.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_style.h"
#include "aura_art.h"
#include "aura_music.h" /* AURA_MUSIC_ITEM_LEN, mismo tope que aura_music_list_playlists() */
#include "aura_artist_images.h" /* D-322: lookup de la foto por tag de artista */
#include "aura_cache_keys.h"     /* D-338: nombre a-<crc>-<mtime>-<lado>.pfraw; D-339: a-<crc>-<mtime>.none */
#include "aura_master_art.h"     /* D-341: maestra compartida /.aura/art, fuente de todo lo de abajo */
#include "crc32.h"

/* D-291: pfraw_path()/is_cached() siguen siendo la llave PROPIA de este
 * archivo (D-338: crc32 de la ruta de la pista + mtime + size, sin
 * invalidacion extra -- ya es unica por archivo); leer/escribir el formato y transponer/enmascarar esquinas
 * ahora vive en aura_art.c (aura_art_read_pfraw()/aura_art_write_pfraw()/
 * aura_art_transpose()/aura_art_mask_corners_transposed()), compartido
 * con aura_photos.c (miniaturas de /Photos).
 *
 * D-341 (contrato v16): el .pfraw es ahora una cache L2 regenerable. La
 * fuente es la MAESTRA compartida (/.aura/art/albums, 130 px planos,
 * sin tema) -- de ella se DERIVA el .pfraw al cargar (reducir por caja
 * si el tile es menor, transponer, esquinas del tema, reflejo). Solo se
 * decodifica JPEG si no hay maestra, y entonces se escribe la maestra
 * primero. */
#define PFRAW_EXTRA_NONE 0

/* Scratch para transponer la imagen decodificada/derivada antes de
 * copiarla al buffer del llamador. Solo lo usa el hilo de UI (el
 * constructor en segundo plano nunca transpone: solo escribe maestras),
 * asi que no necesita el candado de aura_master_art. Dimensionado como
 * el scratch de decodificacion (CoverDrift a 320 px, D-254). */
#define AURA_ALBUMART_TRANSPOSE_SCRATCH_SIZE \
    (AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE * AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE * sizeof(fb_data))
static unsigned char s_transpose_scratch[AURA_ALBUMART_TRANSPOSE_SCRATCH_SIZE];

/* D-341: maestra leida a RAM (130 x 130 planos) y su reduccion por caja
 * al lado del tile (<= 130). Solo hilo de UI. */
static fb_data s_master_flat[AURA_MASTER_ART_ALBUM_SIZE * AURA_MASTER_ART_ALBUM_SIZE];
static fb_data s_derive_flat[AURA_MASTER_ART_ALBUM_SIZE * AURA_MASTER_ART_ALBUM_SIZE];

/* Mascara de icono para el tile default (masks/<icono>-<lado>.bmp, hasta
 * 64 px): buffer propio, ya no el scratch de decodificacion -- ese ahora
 * lo comparte el constructor en segundo plano (D-341) y el tile default
 * se compone en el hilo de UI sin candado. */
#define AURA_ALBUMART_ICON_SCRATCH_SIZE (64 * 64 * sizeof(fb_data) + 64)
static unsigned char s_icon_scratch[AURA_ALBUMART_ICON_SCRATCH_SIZE];

/* Mismo valor que aura_settings.c/aura_manifest.c -- no hay un header
 * compartido para esto en el proyecto, cada archivo lo redefine igual
 * (precedente ya establecido, no una duplicacion nueva de este commit). */
#define AURA_DIR     ROCKBOX_DIR "/aura"
#define CF_CACHE_DIR AURA_DIR "/cfcache"

static void pfraw_path(const aura_albumart_key_t *key, int size, char *out, size_t outsz)
{
    char name[48];

    aura_cache_keys_album_name(name, sizeof(name), key->path_crc, key->mtime, size);
    snprintf(out, outsz, "%s/%s", CF_CACHE_DIR, name);
}

/* D-339: a-<crc>-<mtime>.none junto a los .pfraw, sin lado. D-341: el
 * veredicto "sin arte" ahora vive en la maestra compartida
 * (/.aura/art/albums/a-<crc>.<mtime>.none); el .none privado de cfcache
 * solo sobrevive como ORIGEN de migracion. */
static void legacy_none_path(const aura_albumart_key_t *key, char *out, size_t outsz)
{
    char name[48];

    aura_cache_keys_album_none_name(name, sizeof(name), key->path_crc, key->mtime);
    snprintf(out, outsz, "%s/%s", CF_CACHE_DIR, name);
}

/* true si el album esta marcado "sin arte": maestra .none, o un .none
 * privado de D-339 (que se migra a la maestra en el acto y se borra). */
static bool album_none_present(const aura_albumart_key_t *key)
{
    char path[MAX_PATH];

    if (aura_master_art_none_present(AURA_MASTER_ART_ALBUM, key))
        return true;
    legacy_none_path(key, path, sizeof(path));
    if (file_exists(path))
    {
        aura_master_art_write_none(AURA_MASTER_ART_ALBUM, key);
        remove(path);
        return true;
    }
    return false;
}

static void ensure_cache_dirs(void)
{
    if (!dir_exists(AURA_DIR))
        mkdir(AURA_DIR);
    if (!dir_exists(CF_CACHE_DIR))
        mkdir(CF_CACHE_DIR);
}

static bool find_any_track_in_album(int32_t album_seek, char *path, size_t path_sz,
                                     char *artist, size_t artist_sz,
                                     char *album, size_t album_sz,
                                     aura_albumart_key_t *key);

/* D-338: ver aura_albumart.h. */
bool aura_albumart_album_key(int32_t album_seek, aura_albumart_key_t *key)
{
    char path[MAX_PATH];

    return find_any_track_in_album(album_seek, path, sizeof(path),
                                   NULL, 0, NULL, 0, key);
}

bool aura_albumart_is_cached_key(const aura_albumart_key_t *key, int size, int radius)
{
    char path[MAX_PATH];

    pfraw_path(key, size, path, sizeof(path));
    return aura_cache_keys_album_resolved(
        aura_art_pfraw_is_cached(path, size, radius, PFRAW_EXTRA_NONE),
        album_none_present(key));
}

/* D-224: ver comentario en aura_albumart.h. */
bool aura_albumart_is_cached(int32_t album_seek, int size, int radius)
{
    aura_albumart_key_t key;

    if (!aura_albumart_album_key(album_seek, &key))
        return false;
    return aura_albumart_is_cached_key(&key, size, radius);
}

void aura_albumart_gc_orphans(const aura_albumart_key_t *keys, int count)
{
    DIR *d = opendir(CF_CACHE_DIR);
    struct DIRENT *entry;
    int removed = 0;

    /* D-341: la maestra compartida se barre con la misma tabla de claves
     * vivas (no depende de seeks: es la misma para las tres familias). */
    aura_master_art_gc(AURA_MASTER_ART_ALBUM, keys, count);

    if (!d)
        return;
    while (removed < AURA_ALBUMART_GC_BUDGET && (entry = readdir(d)) != NULL)
    {
        const char *name = entry->d_name;
        char path[MAX_PATH * 2]; /* mismo motivo que aura_fsutil.c */

        /* Decision pura (a-*.pfraw, a-*.none de D-339 y los
         * <seek>-<lado>.pfraw de antes de D-338; pl-/ar- no son de este
         * GC) -- aura_cache_keys.c, con test host. */
        if (!aura_cache_keys_album_is_orphan(name, keys, count))
            continue;
        snprintf(path, sizeof(path), "%s/%s", CF_CACHE_DIR, name);
        if (remove(path) >= 0)
            removed++;
    }
    closedir(d);
}

static void write_pfraw(const char *path, int size, int radius, const fb_data *data)
{
    ensure_cache_dirs();

    aura_art_write_pfraw(path, size, radius, PFRAW_EXTRA_NONE, data);
}

/* D-231 (reporte del dueno, 2026-08-14: "la version por default [en
 * la lista de albumes] se ve muy mal, con un icono que no cabe
 * completamente en ese pequeno cuadro"). Causa real: esta funcion
 * siempre pedia la mascara "music" de A26_ICON_SIZE_SELECTION_SUMMARY_
 * SYMBOL (60px, generate.py), sin importar el `size` del tile que la
 * va a mostrar -- correcto para Music Flow/transiciones (130-135px,
 * 60px cabe con margen) pero la lista de albumes usa ALBUM_ART_SIZE=48,
 * MENOR que el propio icono: `ox`/`oy` (mas abajo) daban negativo y la
 * nota se recortaba contra los cuatro bordes del tile.
 *
 * Fix: elegir la mascara horneada mas grande que siga respetando la
 * MISMA proporcion icono/tile que ya se veia bien en Music Flow
 * (60/130 =~ 46%), en vez de un tamano fijo -- design-system/generate.py
 * hornea "music" en un conjunto fijo de tamanos (ver icons/masks/
 * music-*.bmp): 12/16/20/24/28/36/48/60/64px. Para 130/135px esto
 * sigue devolviendo 60 (identico a antes, cero cambio visual ahi); para
 * 48px (lista de albumes) devuelve 20, que cabe con el mismo margen
 * proporcional que el resto del sistema. */
static int default_tile_icon_size(int size)
{
    static const int available[] = { 12, 16, 20, 24, 28, 36, 48, 60, 64 };
    int target = size * A26_ICON_SIZE_SELECTION_SUMMARY_SYMBOL
                 / AURA_DS_METRICS_MUSIC_FLOW_CENTER_SLIDE_SIZE;
    int best = available[0];
    size_t i;

    for (i = 0; i < sizeof(available) / sizeof(available[0]); i++)
    {
        if (available[i] > size)
            break;
        if (available[i] <= target)
            best = available[i];
    }
    return best;
}

/* Caratula "Default" (imagen de referencia del dueno del diseno,
 * 2026-08-12): nota musical gris sobre un tile gris claro plano --
 * reemplaza al degradado de acento de la primera version de D-109. Los
 * grises salen de tokens existentes (SELECTION_FILL de fondo,
 * SHELL_RAIL de tinta), asi el default respeta ambos temas sin colores
 * nuevos. La nota es la mascara de cobertura del icono "music" que ya
 * genera design-system/generate.py, elegida por tamano segun el tile
 * (D-231, ver default_tile_icon_size arriba) -- se compone contra el
 * tile con la rampa de antialias real, ningun bitmap nuevo. */
/* Compone `icon_name`-`icon_size`.bmp (mascara de cobertura horneada,
 * design-system/generate.py) centrado sobre un tile plano
 * A26_SELECTION_FILL -- compartido por el default de Musica ("nota
 * musical", tamano proporcional al tile) y el placeholder de Artistas
 * (D-322, "artist", 28px fijo -- ver aura_artist_art_default_tile()).
 * Tile plano sin icono (mejor que nada) si el asset faltara. */
static void default_tile_with_icon(fb_data *buf, int size, bool transposed,
                                    const char *icon_name, int icon_size)
{
    unsigned tile = a26_color(A26_SELECTION_FILL);
    /* Tinta del icono: punto medio entre SHELL_RAIL y TEXT_SECONDARY --
     * RAIL solo (primera version del default de Musica) quedaba casi
     * invisible sobre el tile claro; la referencia del dueno del diseno
     * usa un gris medio con contraste claramente visible. Mezcla de dos
     * tokens del tema, ningun color suelto nuevo. */
    unsigned ink = a26_shell_blend(a26_color(A26_SHELL_RAIL),
                                    a26_color(A26_TEXT_SECONDARY), 128);
    char rel[MAX_PATH];
    struct bitmap bm;
    int i, row, col, ret, ox, oy;
    const fb_data *mask;

    for (i = 0; i < size * size; i++)
        buf[i] = tile;

    /* D-289: mascara del estilo activo, con fallback por archivo al
     * default -- ver aura_style.c. */
    snprintf(rel, sizeof(rel), "masks/%s-%d.bmp", icon_name, icon_size);
    bm.data = (char *)s_icon_scratch;
    ret = aura_style_read_icon_bmp(rel, &bm, sizeof(s_icon_scratch));
    if (ret <= 0)
        return; /* tile plano sin icono -- mejor que nada si faltara el asset */

    mask = (const fb_data *)bm.data;
    ox = (size - bm.width) / 2;
    oy = (size - bm.height) / 2;
    for (row = 0; row < bm.height; row++)
    {
        for (col = 0; col < bm.width; col++)
        {
            int cov = (mask[row * bm.width + col] >> 5) & 0x3F;
            size_t di;

            if (cov == 0)
                continue;
            di = transposed ? (size_t)(ox + col) * size + (oy + row)
                             : (size_t)(oy + row) * size + (ox + col);
            buf[di] = a26_shell_blend(tile, ink, cov * 256 / 63);
        }
    }
}

void aura_albumart_default_tile(fb_data *buf, int size, bool transposed)
{
    default_tile_with_icon(buf, size, transposed, "music", default_tile_icon_size(size));
}

/* D-322: placeholder de Artistas -- mismo tile A26_SELECTION_FILL,
 * icono "artist" (el mismo que ya usa la fila de Artistas del menu de
 * Musica) a 28px FIJOS (no proporcional como el de Musica -- pedido
 * explicito del plan, PLAN-biblioteca-medios-v2.md §3.6). */
#define AURA_ARTIST_PLACEHOLDER_ICON_SIZE 28

static void artist_default_tile(fb_data *buf, int size, bool transposed)
{
    default_tile_with_icon(buf, size, transposed, "artist", AURA_ARTIST_PLACEHOLDER_ICON_SIZE);
}

void aura_albumart_load_default(aura_albumart_t *out)
{
    unsigned bg = a26_color(A26_SHELL_BG);

    aura_albumart_default_tile((fb_data *)out->cover_data, out->size, true);
    aura_art_mask_corners_transposed((fb_data *)out->cover_data, out->size, out->radius, bg);

    aura_art_generate_reflection((const fb_data *)out->cover_data,
                                  (fb_data *)out->reflection_data,
                                  out->size, AURA_DS_METRICS_MUSIC_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT,
                                  bg, true);
    out->valid = true;
}

/* artist/album/key son opcionales (NULL). D-338: `key` sale de la misma
 * busqueda -- crc32 de la ruta devuelta + tag_mtime de esa entrada. */
static bool find_any_track_in_album(int32_t album_seek, char *path, size_t path_sz,
                                     char *artist, size_t artist_sz,
                                     char *album, size_t album_sz,
                                     aura_albumart_key_t *key)
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
        if (artist)
            tagcache_retrieve(&tcs, tcs.idx_id, tag_artist, artist, artist_sz);
        if (album)
            tagcache_retrieve(&tcs, tcs.idx_id, tag_album, album, album_sz);
        if (key)
        {
            key->path_crc = crc_32(path, strlen(path), 0xffffffff);
            key->mtime = (uint32_t)tagcache_get_numeric(&tcs, tag_mtime);
        }
        found = true;
    }

    tagcache_search_finish(&tcs);
    return found;
}

/* -- Derivacion desde la maestra (D-341) -------------------------------- */

/* De la maestra plana (130^2) al formato del consumidor: reduccion por
 * caja si el tile es menor (48 de las listas), transposicion, esquinas
 * del tema al radio pedido. false si el tile es MAYOR que la maestra
 * (Ahora suena 135, CoverDrift 320): ahi no hay derivacion posible y el
 * llamador decodifica como antes. Solo hilo de UI (s_derive_flat,
 * s_transpose_scratch). */
static bool derive_transposed(const fb_data *master_flat, int master_size,
                              aura_albumart_t *out, unsigned bg)
{
    const fb_data *src = master_flat;

    if (out->size > master_size)
        return false;
    if (out->size < master_size)
    {
        aura_master_art_downscale_box((const uint16_t *)master_flat, master_size,
                                      (uint16_t *)s_derive_flat, out->size);
        src = s_derive_flat;
    }
    aura_art_transpose(src, (fb_data *)s_transpose_scratch, out->size);
    aura_art_mask_corners_transposed((fb_data *)s_transpose_scratch, out->size, out->radius, bg);
    memcpy(out->cover_data, s_transpose_scratch,
           (size_t)out->size * out->size * sizeof(fb_data));
    return true;
}

static void finish_with_reflection(aura_albumart_t *out, unsigned bg)
{
    aura_art_generate_reflection((const fb_data *)out->cover_data,
                                  (fb_data *)out->reflection_data,
                                  out->size, AURA_DS_METRICS_MUSIC_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, bg, true);
    out->valid = true;
}

/* -- Decodificacion (solo sin maestra) ----------------------------------- */

/* Localiza la fuente de la caratula del album -- archivo junto al album
 * (find_albumart(), misma logica que Ahora suena) o JPEG embebido en la
 * pista representativa -- y la decodifica con fill-crop al lado de la
 * maestra en `flat`. Repite la busqueda de pista (barata, base en RAM,
 * D-021) y devuelve la clave de la pista que de verdad uso.
 *
 * D-339: `*definitive` queda en true solo cuando la busqueda y la
 * decodificacion CONCLUYEN "no hay arte" (sin archivo junto al album y
 * sin JPEG embebido, o JPEG rechazado por el decodificador) -- lo unico
 * que merece marcador .none. Un open() de la pista en error (disco
 * ausente/ocupado, -EBUSY) es transitorio: false y sin marcador.
 *
 * Todo bajo el candado de aura_master_art: s_probe_id3 y el scratch de
 * decodificacion son compartidos con el constructor en segundo plano. */
static bool decode_album_master(int32_t album_seek, aura_albumart_key_t *key,
                                fb_data *flat, bool *definitive)
{
    static struct mp3entry s_probe_id3; /* ~1KB, fuera del stack; bajo candado */
    char path[MAX_PATH];
    char artist[128] = "";
    char album[128] = "";
    struct mp3entry fake_id3;
    struct dim dim = { AURA_MASTER_ART_ALBUM_SIZE, AURA_MASTER_ART_ALBUM_SIZE };
    char art_path[MAX_PATH];
    bool ok = false;

    *definitive = false;
    if (!find_any_track_in_album(album_seek, path, sizeof(path),
                                  artist, sizeof(artist), album, sizeof(album), key))
        return false;

    memset(&fake_id3, 0, sizeof(fake_id3));
    strlcpy(fake_id3.path, path, sizeof(fake_id3.path));
    fake_id3.artist = artist;
    fake_id3.album = album;

    aura_master_art_decode_lock();

    if (find_albumart(&fake_id3, art_path, sizeof(art_path), &dim))
    {
        ok = aura_master_art_decode_fill(art_path, 0, 0, AURA_MASTER_ART_ALBUM_SIZE, flat);
        /* El archivo existe (find_albumart lo acaba de ver): un rechazo
         * del decodificador es un veredicto sobre ESE archivo, no un
         * fallo transitorio. */
        *definitive = !ok;
        aura_master_art_decode_unlock();
        return ok;
    }

    /* Sin archivo de imagen junto al album: caratula EMBEBIDA en el
     * track (biblioteca real del dueno del diseno, 2026-08-12 -- los
     * exports de CD de Musica.app llevan el arte dentro del m4a/mp3,
     * no como cover.jpg de carpeta). Mismo criterio que playback.c:
     * solo JPG embebido (AA_CLEAR_FLAGS_MASK) -- no hay decoder PNG en
     * el core de Rockbox, un "covr" PNG cae a la caratula Default. */
    {
        int fd = open(path, O_RDONLY);

        if (fd < 0)
        {
            aura_master_art_decode_unlock();
            return false; /* transitorio: sin marcador (D-339) */
        }
        if (!get_metadata(&s_probe_id3, fd, path)
            || !s_probe_id3.has_embedded_albumart
            || (s_probe_id3.albumart.type & AA_CLEAR_FLAGS_MASK) != AA_TYPE_JPG)
        {
            close(fd);
            *definitive = true; /* pista legible y sin arte embebido util */
            aura_master_art_decode_unlock();
            return false;
        }
        close(fd);

        ok = aura_master_art_decode_fill(path, s_probe_id3.albumart.pos,
                                         s_probe_id3.albumart.size,
                                         AURA_MASTER_ART_ALBUM_SIZE, flat);
        *definitive = !ok;
    }
    aura_master_art_decode_unlock();
    return ok;
}

/* Decodifica la caratula real al lado pedido (> maestra: Ahora suena
 * 135, CoverDrift 320), fila-contigua en el scratch compartido -- el
 * camino anterior a D-341, conservado solo para esos lados. Devuelve el
 * puntero al bitmap dentro del scratch (valido con el candado tomado,
 * que el LLAMADOR ya tiene) o NULL. */
static const fb_data *decode_album_at(int32_t album_seek, int size, aura_albumart_key_t *key)
{
    static struct mp3entry s_probe_id3; /* bajo candado */
    char path[MAX_PATH];
    char artist[128] = "";
    char album[128] = "";
    struct mp3entry fake_id3;
    struct dim dim = { size, size };
    char art_path[MAX_PATH];
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    size_t scratch_sz;
    unsigned char *scratch = aura_master_art_scratch(&scratch_sz);
    struct bitmap bm;
    int len, ret;

    if (!find_any_track_in_album(album_seek, path, sizeof(path),
                                  artist, sizeof(artist), album, sizeof(album), key))
        return NULL;

    memset(&fake_id3, 0, sizeof(fake_id3));
    strlcpy(fake_id3.path, path, sizeof(fake_id3.path));
    fake_id3.artist = artist;
    fake_id3.album = album;

    bm.width = size;
    bm.height = size;
    bm.data = (char *)scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    if (find_albumart(&fake_id3, art_path, sizeof(art_path), &dim))
    {
        len = (int)strlen(art_path);
        if (len > 4 && !strcasecmp(art_path + len - 4, ".bmp"))
            ret = read_bmp_file(art_path, &bm, scratch_sz, format, NULL);
        else
            ret = read_jpeg_file(art_path, &bm, scratch_sz, format, NULL);
        DEBUGF("aura_master_art: decode %s at %d -> %d\n", art_path, size, ret);
        return ret > 0 ? (const fb_data *)scratch : NULL;
    }

    {
        int fd = open(path, O_RDONLY);

        if (fd < 0)
            return NULL;
        if (!get_metadata(&s_probe_id3, fd, path)
            || !s_probe_id3.has_embedded_albumart
            || (s_probe_id3.albumart.type & AA_CLEAR_FLAGS_MASK) != AA_TYPE_JPG)
        {
            close(fd);
            return NULL;
        }
        close(fd);

        ret = clip_jpeg_file(path, s_probe_id3.albumart.pos,
                              s_probe_id3.albumart.size, &bm,
                              scratch_sz, format, NULL);
        DEBUGF("aura_master_art: decode %s (embedded) at %d -> %d\n", path, size, ret);
        return ret > 0 ? (const fb_data *)scratch : NULL;
    }
}

bool aura_albumart_build_master(int32_t album_seek, aura_albumart_key_t *key, fb_data *flat)
{
    bool definitive;

    if (!aura_albumart_album_key(album_seek, key))
        return false;
    if (aura_master_art_resolved(AURA_MASTER_ART_ALBUM, key) || album_none_present(key))
        return true;
    if (decode_album_master(album_seek, key, flat, &definitive))
    {
        aura_master_art_write(AURA_MASTER_ART_ALBUM, key, flat);
        return true;
    }
    if (definitive)
        aura_master_art_write_none(AURA_MASTER_ART_ALBUM, key);
    return true;
}

bool aura_albumart_load_for_album(int32_t album_seek, aura_albumart_t *out)
{
    char path[MAX_PATH];
    unsigned bg = a26_color(A26_SHELL_BG);
    aura_albumart_key_t key;

    out->valid = false;
    if (!aura_albumart_album_key(album_seek, &key))
        return false; /* album sin pistas en la base: tampoco habria caratula */
    pfraw_path(&key, out->size, path, sizeof(path));

    if (aura_art_read_pfraw(path, out->size, out->radius, PFRAW_EXTRA_NONE, (fb_data *)out->cover_data))
    {
        /* Acierto de cache L2 -- cero decodificacion ni derivacion. El
         * reflejo NO se cachea (ver header del .pfraw arriba), se
         * recalcula siempre desde la caratula transpuesta ya en
         * memoria -- liviano, sin decodificacion de por medio. */
        finish_with_reflection(out, bg);
        return true;
    }

    /* D-339/D-341: marcador negativo -- ya se supo que este album no
     * tiene arte; ni busqueda de archivo ni decodificacion. */
    if (album_none_present(&key))
        return false;

    if (out->size <= AURA_MASTER_ART_ALBUM_SIZE)
    {
        /* Camino normal (Music Flow 130, listas 48): maestra -> derivar.
         * Sin maestra todavia (el constructor no llego), se construye
         * aqui mismo -- y queda para las tres familias. */
        if (!aura_master_art_read(AURA_MASTER_ART_ALBUM, &key, s_master_flat))
        {
            bool definitive;

            if (!decode_album_master(album_seek, &key, s_master_flat, &definitive))
            {
                if (definitive)
                    aura_master_art_write_none(AURA_MASTER_ART_ALBUM, &key);
                return false;
            }
            aura_master_art_write(AURA_MASTER_ART_ALBUM, &key, s_master_flat);
            pfraw_path(&key, out->size, path, sizeof(path));
        }
        DEBUGF("aura_master_art: derive album %08lx at %d\n",
               (unsigned long)key.path_crc, out->size);
        derive_transposed(s_master_flat, AURA_MASTER_ART_ALBUM_SIZE, out, bg);
        write_pfraw(path, out->size, out->radius, (const fb_data *)out->cover_data);
        finish_with_reflection(out, bg);
        return true;
    }

    /* Lado mayor que la maestra (Ahora suena 135, CoverDrift 320): no se
     * puede derivar sin ampliar; se decodifica al lado pedido como
     * antes de D-341 (y se cachea en .pfraw privado, como siempre). Si
     * ademas falta la maestra, se construye de paso: "siempre se
     * escribe la maestra cuando se decodifica". */
    {
        const fb_data *decoded;

        if (!aura_master_art_resolved(AURA_MASTER_ART_ALBUM, &key))
        {
            bool definitive;

            if (decode_album_master(album_seek, &key, s_master_flat, &definitive))
                aura_master_art_write(AURA_MASTER_ART_ALBUM, &key, s_master_flat);
            else if (definitive)
            {
                aura_master_art_write_none(AURA_MASTER_ART_ALBUM, &key);
                return false;
            }
        }

        aura_master_art_decode_lock();
        decoded = decode_album_at(album_seek, out->size, &key);
        if (decoded == NULL)
        {
            aura_master_art_decode_unlock();
            return false;
        }
        pfraw_path(&key, out->size, path, sizeof(path));
        aura_art_transpose(decoded, (fb_data *)s_transpose_scratch, out->size);
        aura_master_art_decode_unlock();

        aura_art_mask_corners_transposed((fb_data *)s_transpose_scratch, out->size, out->radius, bg);
        memcpy(out->cover_data, s_transpose_scratch,
               (size_t)out->size * out->size * sizeof(fb_data));
        write_pfraw(path, out->size, out->radius, (const fb_data *)out->cover_data);
        finish_with_reflection(out, bg);
        return true;
    }
}

/* -- Portada de playlist (encargo del dueno, 2026-08-14) ------------------
 *
 * Mismo mecanismo que arriba (cache .pfraw en disco, decodificar JPEG
 * solo en un fallo de cache) pero SIN tagcache de por medio: la llave
 * es el nombre de archivo de la playlist en vez de un album_seek, y el
 * origen es directo -- "<directorio de catalogo>/<nombre sin
 * extension>.jpg" -- en vez de find_albumart(). D-341: las playlists
 * NO tienen maestra compartida (siguen privadas): su portada es un
 * sidecar de Studio que las otras familias no muestran igual. */

/* MAX_PATH (260) + AURA_MUSIC_ITEM_LEN (64) + separador/extension --
 * mismo margen que full_path[] en aura_music_add_track_to_playlist(),
 * para que el snprintf() de playlist_art_source_path() no pueda
 * truncar nunca (evita el -Wformat-truncation que sale si el destino
 * es un MAX_PATH "justo" combinado con otro %s de tamano variable). */
#define AURA_PLAYLIST_ART_PATH_LEN (MAX_PATH + AURA_MUSIC_ITEM_LEN + 8)

/* Nombre base del cache .pfraw: quitar la extension (.m3u/.m3u8) del
 * nombre de playlist alcanza para tener una llave unica por playlist,
 * los mismos caracteres que ya son validos en el nombre del .m3u8
 * (PathSanitizer del lado de Aura Studio) tambien lo son en un nombre
 * de archivo de cache. */
static void playlist_art_base_name(const char *playlist_filename, char *out, size_t outsz)
{
    char *dot;

    strlcpy(out, playlist_filename, outsz);
    dot = strrchr(out, '.');
    if (dot)
        *dot = '\0';
}

static void playlist_pfraw_path(const char *playlist_filename, int size, char *out, size_t outsz)
{
    /* static: por encima de los ~200 bytes de buffer local que este
     * proyecto evita en el hilo de UI (D-226/D-227). */
    static char base[AURA_MUSIC_ITEM_LEN];

    playlist_art_base_name(playlist_filename, base, sizeof(base));
    snprintf(out, outsz, "%s/pl-%s-%d.pfraw", CF_CACHE_DIR, base, size);
}

/* Sidecar que Aura Studio deja junto al .m3u8 (LibrarySync.swift,
 * PlaylistExporter.imageFileName): mismo directorio que
 * catalog_get_directory() (donde aura_music_list_playlists() ya busca
 * los .m3u/.m3u8), mismo nombre base con ".jpg" en vez de la extension
 * de playlist. */
static void playlist_art_source_path(const char *playlist_filename, char *out, size_t outsz)
{
    /* static: mismo motivo que playlist_pfraw_path() arriba -- dir+base
     * combinados pasan largo los ~200 bytes. */
    static char dir[MAX_PATH];
    static char base[AURA_MUSIC_ITEM_LEN];

    catalog_get_directory(dir, sizeof(dir));
    playlist_art_base_name(playlist_filename, base, sizeof(base));
    snprintf(out, outsz, "%s/%s.jpg", dir, base);
}

/* Decodifica el sidecar al scratch compartido (candado tomado por el
 * llamador) -- el sidecar siempre es JPEG (Aura Studio nunca escribe
 * otra cosa), asi que no hace falta la rama .bmp. */
static const fb_data *decode_playlist_art(const char *playlist_filename, int size)
{
    static char path[AURA_PLAYLIST_ART_PATH_LEN]; /* static: D-226/D-227 */
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    size_t scratch_sz;
    unsigned char *scratch = aura_master_art_scratch(&scratch_sz);
    struct bitmap bm;

    playlist_art_source_path(playlist_filename, path, sizeof(path));

    bm.width = size;
    bm.height = size;
    bm.data = (char *)scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    if (read_jpeg_file(path, &bm, scratch_sz, format, NULL) <= 0)
        return NULL;
    return (const fb_data *)scratch;
}

bool aura_playlist_art_load(const char *playlist_filename, aura_albumart_t *out)
{
    static char cache_path[AURA_PLAYLIST_ART_PATH_LEN]; /* static: D-226/D-227 */
    unsigned bg = a26_color(A26_SHELL_BG);
    const fb_data *decoded;

    out->valid = false;
    if (!playlist_filename || !*playlist_filename)
        return false;

    playlist_pfraw_path(playlist_filename, out->size, cache_path, sizeof(cache_path));

    if (aura_art_read_pfraw(cache_path, out->size, out->radius, PFRAW_EXTRA_NONE, (fb_data *)out->cover_data))
    {
        finish_with_reflection(out, bg);
        return true;
    }

    aura_master_art_decode_lock();
    decoded = decode_playlist_art(playlist_filename, out->size);
    if (decoded == NULL)
    {
        aura_master_art_decode_unlock();
        return false;
    }
    aura_art_transpose(decoded, (fb_data *)s_transpose_scratch, out->size);
    aura_master_art_decode_unlock();

    aura_art_mask_corners_transposed((fb_data *)s_transpose_scratch, out->size, out->radius, bg);
    memcpy(out->cover_data, s_transpose_scratch,
           (size_t)out->size * out->size * sizeof(fb_data));

    write_pfraw(cache_path, out->size, out->radius, (const fb_data *)out->cover_data);
    finish_with_reflection(out, bg);
    return true;
}

/* -- Fotos de artista (D-322, D-341) --------------------------------------- */

/* D-322 (PLAN-biblioteca-medios-v2.md §3.6): hash FNV-1a de 32 bits del
 * tag de artista -- clave del cache .pfraw en vez del nombre crudo
 * (evita depender de longitud/charset del nombre de archivo, y no
 * colisiona con "<seek>-<size>" de albumes ni "pl-..." de playlists). */
static uint32_t artist_tag_hash(const char *artist_tag)
{
    uint32_t h = 2166136261u;
    const unsigned char *p = (const unsigned char *)artist_tag;

    while (*p)
    {
        h ^= *p++;
        h *= 16777619u;
    }
    return h;
}

static void artist_pfraw_path(const char *artist_tag, int size, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/ar-%08lx-%d.pfraw", CF_CACHE_DIR,
             (unsigned long)artist_tag_hash(artist_tag), size);
}

bool aura_artist_art_build_master(const char *image_path, uint32_t mtime,
                                  aura_master_art_key_t *key, fb_data *flat)
{
    aura_master_art_key_from_path(image_path, mtime, key);
    if (aura_master_art_resolved(AURA_MASTER_ART_ARTIST, key))
        return true;
    if (!file_exists(image_path))
        return false; /* el indice apunta a un archivo que no esta: transitorio */
    /* Contrato §D.3: JPEG baseline, cuadrada, <=128px -- el fill-crop no
     * hace nada visible con una fuente cuadrada; cubre la que no lo sea. */
    if (aura_master_art_decode_fill(image_path, 0, 0, AURA_MASTER_ART_ARTIST_SIZE, flat))
        aura_master_art_write(AURA_MASTER_ART_ARTIST, key, flat);
    else
        aura_master_art_write_none(AURA_MASTER_ART_ARTIST, key); /* archivo presente y rechazado */
    return true;
}

/* Foto de artista, CIRCULAR (radius = size/2, aura_art_mask_corners_
 * transposed() produce un circulo perfecto para `size` par -- D-322).
 * D-341: la fuente es la maestra compartida r-<crc ruta jpg>.<mtime>
 * (130 px planos); el .pfraw privado ar-<hash tag>-<lado> es L2 y se
 * deriva de ella (reduccion por caja 130 -> 48 + circulo). `extra` del
 * .pfraw sigue en PFRAW_EXTRA_NONE: aura_sync.c tira los ar-* al
 * terminar un sync de musica (la foto pudo cambiar), y la maestra
 * lleva el mtime del jpg en su clave, asi que una foto reescrita
 * produce maestra nueva sin stat-ear nada por fila (el mtime lo aporta
 * aura_artist_images.c de una sola pasada de directorio al cargar el
 * indice). */
bool aura_artist_art_load(const char *artist_tag, aura_albumart_t *out)
{
    static char image_path[MAX_PATH];
    static char cache_path[MAX_PATH];
    unsigned bg = a26_color(A26_SHELL_BG);
    aura_master_art_key_t key;
    uint32_t mtime = 0;

    out->valid = false;
    if (!artist_tag || !*artist_tag)
        return false;
    if (!aura_artist_images_lookup_mtime(artist_tag, image_path, sizeof(image_path), &mtime))
        return false;

    artist_pfraw_path(artist_tag, out->size, cache_path, sizeof(cache_path));

    if (aura_art_read_pfraw(cache_path, out->size, out->radius, PFRAW_EXTRA_NONE, (fb_data *)out->cover_data))
    {
        out->valid = true;
        return true;
    }

    aura_master_art_key_from_path(image_path, mtime, &key);
    if (aura_master_art_none_present(AURA_MASTER_ART_ARTIST, &key))
        return false;
    if (!aura_master_art_read(AURA_MASTER_ART_ARTIST, &key, s_master_flat))
    {
        if (!aura_artist_art_build_master(image_path, mtime, &key, s_master_flat))
            return false;
        if (!aura_master_art_read(AURA_MASTER_ART_ARTIST, &key, s_master_flat))
            return false; /* quedo .none (rechazada) */
    }
    if (!derive_transposed(s_master_flat, AURA_MASTER_ART_ARTIST_SIZE, out, bg))
        return false; /* lado mayor que la maestra: no hay consumidor asi hoy */
    DEBUGF("aura_master_art: derive artist %08lx at %d\n",
           (unsigned long)key.path_crc, out->size);

    write_pfraw(cache_path, out->size, out->radius, (const fb_data *)out->cover_data);

    out->valid = true;
    return true;
}

/* Placeholder circular de Artistas (sin foto, o `artist_tag` vacio --
 * fila "Todos"): mismo tile que artist_default_tile() de arriba,
 * recortado al circulo. No pasa por el cache .pfraw -- es una
 * composicion trivial (un tile + un icono), mas barata que leer un
 * archivo. */
void aura_artist_art_load_default(aura_albumart_t *out)
{
    unsigned bg = a26_color(A26_SHELL_BG);

    artist_default_tile((fb_data *)out->cover_data, out->size, true);
    aura_art_mask_corners_transposed((fb_data *)out->cover_data, out->size, out->radius, bg);
    out->valid = true;
}
