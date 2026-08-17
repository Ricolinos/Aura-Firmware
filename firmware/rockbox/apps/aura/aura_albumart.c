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

/* D-291: pfraw_path()/is_cached() siguen siendo la llave PROPIA de este
 * archivo (album_seek+size, sin invalidacion extra -- ya es unica por
 * archivo); leer/escribir el formato y transponer/enmascarar esquinas
 * ahora vive en aura_art.c (aura_art_read_pfraw()/aura_art_write_pfraw()/
 * aura_art_transpose()/aura_art_mask_corners_transposed()), compartido
 * con aura_photos.c (miniaturas de /Photos). */
#define PFRAW_EXTRA_NONE 0

/* Buffer de trabajo para decodificar+remuestrear (FORMAT_RESIZE
 * necesita bastante mas espacio que el bitmap final, ver
 * BM_SCALED_SIZE en recorder/bmp.h) -- dimensionado sobre el mayor
 * consumidor real, no un numero fijo adivinado. D-254 (CoverDrift en
 * Musica) es hoy ese consumidor: AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE
 * es mayor que los 130px de Cover Flow, que era el limite implicito de
 * los 64KB anteriores (bug real encontrado en verificacion: pedir
 * 290px ahi hacia que read_jpeg_file()/read_bmp_file() fallaran en
 * silencio por falta de espacio -- degradaba al placeholder solido sin
 * ningun error visible, nunca llegaba a decodificar la caratula real).
 * Margen x2 sobre el tamano final en pixeles (mismo orden de magnitud
 * que ya probaron suficiente los 64KB sobre el final de 130px de Cover
 * Flow, ~1.9x) para el factor de escala JPEG intermedio antes del
 * resize final (JPEG_DECODE_OVERHEAD, recorder/jpeg_load.h, mas el
 * margen de ese intermedio). */
#define AURA_ALBUMART_DECODE_SCRATCH_SIZE \
    (AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE * AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE * 2 * 2)
static unsigned char s_decode_scratch[AURA_ALBUMART_DECODE_SCRATCH_SIZE];
static unsigned char s_transpose_scratch[AURA_ALBUMART_DECODE_SCRATCH_SIZE];

/* Mismo valor que aura_settings.c/aura_manifest.c -- no hay un header
 * compartido para esto en el proyecto, cada archivo lo redefine igual
 * (precedente ya establecido, no una duplicacion nueva de este commit). */
#define AURA_DIR     ROCKBOX_DIR "/aura"
#define CF_CACHE_DIR AURA_DIR "/cfcache"

static void pfraw_path(int32_t album_seek, int size, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/%ld-%d.pfraw", CF_CACHE_DIR, (long)album_seek, size);
}

/* D-224: ver comentario en aura_albumart.h. */
bool aura_albumart_is_cached(int32_t album_seek, int size, int radius)
{
    char path[MAX_PATH];

    pfraw_path(album_seek, size, path, sizeof(path));
    return aura_art_pfraw_is_cached(path, size, radius, PFRAW_EXTRA_NONE);
}

static void write_pfraw(const char *path, int size, int radius, const fb_data *data)
{
    if (!dir_exists(AURA_DIR))
        mkdir(AURA_DIR);
    if (!dir_exists(CF_CACHE_DIR))
        mkdir(CF_CACHE_DIR);

    aura_art_write_pfraw(path, size, radius, PFRAW_EXTRA_NONE, data);
}

/* D-231 (reporte del dueno, 2026-08-14: "la version por default [en
 * la lista de albumes] se ve muy mal, con un icono que no cabe
 * completamente en ese pequeno cuadro"). Causa real: esta funcion
 * siempre pedia la mascara "music" de A26_ICON_SIZE_SELECTION_SUMMARY_
 * SYMBOL (60px, generate.py), sin importar el `size` del tile que la
 * va a mostrar -- correcto para Cover Flow/transiciones (130-135px,
 * 60px cabe con margen) pero la lista de albumes usa ALBUM_ART_SIZE=48,
 * MENOR que el propio icono: `ox`/`oy` (mas abajo) daban negativo y la
 * nota se recortaba contra los cuatro bordes del tile.
 *
 * Fix: elegir la mascara horneada mas grande que siga respetando la
 * MISMA proporcion icono/tile que ya se veia bien en Cover Flow
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
                 / AURA_DS_METRICS_COVER_FLOW_CENTER_SLIDE_SIZE;
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
void aura_albumart_default_tile(fb_data *buf, int size, bool transposed)
{
    unsigned tile = a26_color(A26_SELECTION_FILL);
    /* Tinta de la nota: punto medio entre SHELL_RAIL y TEXT_SECONDARY
     * -- RAIL solo (primera version) quedaba casi invisible sobre el
     * tile claro; la referencia del dueno del diseno usa un gris medio
     * con contraste claramente visible. Mezcla de dos tokens del tema,
     * ningun color suelto nuevo. */
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
    snprintf(rel, sizeof(rel), "masks/music-%d.bmp", default_tile_icon_size(size));
    bm.data = (char *)s_decode_scratch;
    ret = aura_style_read_icon_bmp(rel, &bm, sizeof(s_decode_scratch));
    if (ret <= 0)
        return; /* tile plano sin nota -- mejor que nada si faltara el asset */

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

void aura_albumart_load_default(aura_albumart_t *out)
{
    unsigned bg = a26_color(A26_SHELL_BG);

    aura_albumart_default_tile((fb_data *)out->cover_data, out->size, true);
    aura_art_mask_corners_transposed((fb_data *)out->cover_data, out->size, out->radius, bg);

    aura_art_generate_reflection((const fb_data *)out->cover_data,
                                  (fb_data *)out->reflection_data,
                                  out->size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT,
                                  bg, true);
    out->valid = true;
}

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

/* Decodifica la caratula real (JPEG/BMP) a `s_decode_scratch`
 * (fila-contigua, tamano out->size x out->size) -- mismo camino que
 * usaba la version anterior de este archivo antes del cache. Solo se
 * llama en un fallo de cache. */
static bool decode_album_art(int32_t album_seek, int size)
{
    char path[MAX_PATH];
    char artist[128] = "";
    char album[128] = "";
    struct mp3entry fake_id3;
    struct dim dim = { size, size };
    char art_path[MAX_PATH];
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int len, ret;
    struct bitmap bm;

    if (!find_any_track_in_album(album_seek, path, sizeof(path),
                                  artist, sizeof(artist), album, sizeof(album)))
        return false;

    memset(&fake_id3, 0, sizeof(fake_id3));
    strlcpy(fake_id3.path, path, sizeof(fake_id3.path));
    fake_id3.artist = artist;
    fake_id3.album = album;

    bm.width = size;
    bm.height = size;
    bm.data = (char *)s_decode_scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    if (find_albumart(&fake_id3, art_path, sizeof(art_path), &dim))
    {
        len = (int)strlen(art_path);
        if (len > 4 && !strcasecmp(art_path + len - 4, ".bmp"))
            ret = read_bmp_file(art_path, &bm, sizeof(s_decode_scratch), format, NULL);
        else
            ret = read_jpeg_file(art_path, &bm, sizeof(s_decode_scratch), format, NULL);
        return ret > 0;
    }

    /* Sin archivo de imagen junto al album: caratula EMBEBIDA en el
     * track (biblioteca real del dueno del diseno, 2026-08-12 -- los
     * exports de CD de Musica.app llevan el arte dentro del m4a/mp3,
     * no como cover.jpg de carpeta). Mismo criterio que playback.c:
     * solo JPG embebido (AA_CLEAR_FLAGS_MASK) -- no hay decoder PNG en
     * el core de Rockbox, un "covr" PNG cae a la caratula Default. */
    {
        static struct mp3entry s_probe_id3; /* ~1KB, fuera del stack */
        int fd = open(path, O_RDONLY);

        if (fd < 0)
            return false;
        if (!get_metadata(&s_probe_id3, fd, path)
            || !s_probe_id3.has_embedded_albumart
            || (s_probe_id3.albumart.type & AA_CLEAR_FLAGS_MASK) != AA_TYPE_JPG)
        {
            close(fd);
            return false;
        }
        close(fd);

        ret = clip_jpeg_file(path, s_probe_id3.albumart.pos,
                              s_probe_id3.albumart.size, &bm,
                              sizeof(s_decode_scratch), format, NULL);
        return ret > 0;
    }
}

bool aura_albumart_load_for_album(int32_t album_seek, aura_albumart_t *out)
{
    char path[MAX_PATH];
    unsigned bg = a26_color(A26_SHELL_BG);

    out->valid = false;
    pfraw_path(album_seek, out->size, path, sizeof(path));

    if (aura_art_read_pfraw(path, out->size, out->radius, PFRAW_EXTRA_NONE, (fb_data *)out->cover_data))
    {
        /* Acierto de cache -- cero decodificacion JPEG (doc). El
         * reflejo NO se cachea (ver header del .pfraw arriba), se
         * recalcula siempre desde la caratula transpuesta ya en
         * memoria -- liviano, sin decodificacion de por medio. */
        aura_art_generate_reflection((const fb_data *)out->cover_data,
                                      (fb_data *)out->reflection_data,
                                      out->size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, bg, true);
        out->valid = true;
        return true;
    }

    if (!decode_album_art(album_seek, out->size))
        return false;

    aura_art_transpose((const fb_data *)s_decode_scratch, (fb_data *)s_transpose_scratch, out->size);
    aura_art_mask_corners_transposed((fb_data *)s_transpose_scratch, out->size, out->radius, bg);
    memcpy(out->cover_data, s_transpose_scratch,
           (size_t)out->size * out->size * sizeof(fb_data));

    write_pfraw(path, out->size, out->radius, (const fb_data *)out->cover_data);

    aura_art_generate_reflection((const fb_data *)out->cover_data,
                                  (fb_data *)out->reflection_data,
                                  out->size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, bg, true);
    out->valid = true;
    return true;
}

/* -- Portada de playlist (encargo del dueno, 2026-08-14) ------------------
 *
 * Mismo mecanismo que arriba (cache .pfraw en disco, decodificar JPEG
 * solo en un fallo de cache) pero SIN tagcache de por medio: la llave
 * es el nombre de archivo de la playlist en vez de un album_seek, y el
 * origen es directo -- "<directorio de catalogo>/<nombre sin
 * extension>.jpg" -- en vez de find_albumart(). */

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

/* Decodifica el sidecar a `s_decode_scratch` -- mismo scratch buffer y
 * mismo formato (FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT)
 * que decode_album_art(); el sidecar siempre es JPEG (Aura Studio nunca
 * escribe otra cosa), asi que a diferencia de decode_album_art() no
 * hace falta la rama .bmp. */
static bool decode_playlist_art(const char *playlist_filename, int size)
{
    static char path[AURA_PLAYLIST_ART_PATH_LEN]; /* static: D-226/D-227 */
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    struct bitmap bm;

    playlist_art_source_path(playlist_filename, path, sizeof(path));

    bm.width = size;
    bm.height = size;
    bm.data = (char *)s_decode_scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    return read_jpeg_file(path, &bm, sizeof(s_decode_scratch), format, NULL) > 0;
}

bool aura_playlist_art_load(const char *playlist_filename, aura_albumart_t *out)
{
    static char cache_path[AURA_PLAYLIST_ART_PATH_LEN]; /* static: D-226/D-227 */
    unsigned bg = a26_color(A26_SHELL_BG);

    out->valid = false;
    if (!playlist_filename || !*playlist_filename)
        return false;

    playlist_pfraw_path(playlist_filename, out->size, cache_path, sizeof(cache_path));

    if (aura_art_read_pfraw(cache_path, out->size, out->radius, PFRAW_EXTRA_NONE, (fb_data *)out->cover_data))
    {
        aura_art_generate_reflection((const fb_data *)out->cover_data,
                                      (fb_data *)out->reflection_data,
                                      out->size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, bg, true);
        out->valid = true;
        return true;
    }

    if (!decode_playlist_art(playlist_filename, out->size))
        return false;

    aura_art_transpose((const fb_data *)s_decode_scratch, (fb_data *)s_transpose_scratch, out->size);
    aura_art_mask_corners_transposed((fb_data *)s_transpose_scratch, out->size, out->radius, bg);
    memcpy(out->cover_data, s_transpose_scratch,
           (size_t)out->size * out->size * sizeof(fb_data));

    write_pfraw(cache_path, out->size, out->radius, (const fb_data *)out->cover_data);

    aura_art_generate_reflection((const fb_data *)out->cover_data,
                                  (fb_data *)out->reflection_data,
                                  out->size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, bg, true);
    out->valid = true;
    return true;
}
