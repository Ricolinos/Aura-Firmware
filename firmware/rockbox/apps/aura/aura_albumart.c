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
#include "aura_art.h"
#include "aura_music.h" /* AURA_MUSIC_ITEM_LEN, mismo tope que aura_music_list_playlists() */

/* Buffer de trabajo para decodificar+remuestrear (FORMAT_RESIZE
 * necesita bastante mas espacio que el bitmap final, ver
 * BM_SCALED_SIZE en recorder/bmp.h); 64KB alcanza sobrado para
 * cualquier tamano de caratula que use Aura (<=100px). */
static unsigned char s_decode_scratch[64 * 1024];
static unsigned char s_transpose_scratch[64 * 1024];

/* Mismo valor que aura_settings.c/aura_manifest.c -- no hay un header
 * compartido para esto en el proyecto, cada archivo lo redefine igual
 * (precedente ya establecido, no una duplicacion nueva de este commit). */
#define AURA_DIR     ROCKBOX_DIR "/aura"
#define CF_CACHE_DIR AURA_DIR "/cfcache"

/* Header en disco del cache .pfraw (PLAN.md T3.2(a),
 * componentes/cover-flow.md: "cache .pfraw... pre-escaladas y
 * transpuestas... runtime solo carga de un cache" -- mismo formato
 * conceptual que apps/plugins/pictureflow/pictureflow.c::pfraw_header
 * (regla dura 7: extender, no reimplementar la tecnica), con un campo
 * extra `radius` para invalidar el cache si el radio de esquina
 * redondeada cambia -- personalizacion de Aura, pictureflow.c no
 * hornea esquinas). Solo la caratula PLANA se cachea, no el reflejo
 * (mismo criterio que pictureflow.c: el reflejo es barato, se recalcula
 * siempre; cachearlo duplicaria el costo en disco sin necesidad). */
struct pfraw_header {
    int32_t size;
    int32_t radius;
    /* Las esquinas van horneadas contra el fondo del TEMA vigente al
     * escribir (correccion 2026-08-12): el tema es parte de la llave --
     * un mismatch regenera el cache, igual que un cambio de radio. */
    int32_t theme;
};

static void pfraw_path(int32_t album_seek, int size, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/%ld-%d.pfraw", CF_CACHE_DIR, (long)album_seek, size);
}

static bool pfraw_header_valid(int fd, int size, int radius)
{
    struct pfraw_header hdr;
    int n = read(fd, &hdr, sizeof(hdr));

    return n == (int)sizeof(hdr) && hdr.size == size && hdr.radius == radius
           && hdr.theme == (int32_t)aura_settings.theme;
}

static bool read_pfraw(const char *path, int size, int radius, fb_data *out)
{
    size_t px_bytes = (size_t)size * size * sizeof(fb_data);
    int fd, n;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;

    if (!pfraw_header_valid(fd, size, radius))
    {
        close(fd);
        return false;
    }

    n = read(fd, out, px_bytes);
    close(fd);
    return n == (int)px_bytes;
}

/* D-224: ver comentario en aura_albumart.h -- mismo chequeo de header
 * que read_pfraw(), sin el read() del payload de pixeles. */
bool aura_albumart_is_cached(int32_t album_seek, int size, int radius)
{
    char path[MAX_PATH];
    int fd;
    bool ok;

    pfraw_path(album_seek, size, path, sizeof(path));
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;

    ok = pfraw_header_valid(fd, size, radius);
    close(fd);
    return ok;
}

static void write_pfraw(const char *path, int size, int radius, const fb_data *data)
{
    struct pfraw_header hdr;
    int fd;

    hdr.size = size;
    hdr.radius = radius;
    hdr.theme = (int32_t)aura_settings.theme;

    if (!dir_exists(AURA_DIR))
        mkdir(AURA_DIR);
    if (!dir_exists(CF_CACHE_DIR))
        mkdir(CF_CACHE_DIR);

    fd = creat(path, 0666);
    if (fd < 0)
        return;

    write(fd, &hdr, sizeof(hdr));
    write(fd, data, (size_t)size * size * sizeof(fb_data));
    close(fd);
}

/* Transpone (fila-contigua -> columna-contigua, doc: "recorrer columnas
 * = memoria contigua = rapido") -- pictureflow.c lo hace FUNDIDO con la
 * decodificacion JPEG via un callback custom_format
 * (output_row_*_transposed); aca se hace en un segundo paso explicito
 * sobre el resultado ya decodificado -- mismo resultado final (el
 * layout en disco/memoria es identico), pero sin engancharse al
 * mecanismo interno de bajo nivel del decodificador JPEG del plugin,
 * que este pipeline no expone. Costo: un recorrido extra de size*size
 * pixeles, una sola vez por caratula (se cachea despues), no en cada
 * cuadro. */
static void transpose(const fb_data *src, fb_data *dst, int size)
{
    int row, col;

    for (row = 0; row < size; row++)
        for (col = 0; col < size; col++)
            dst[(size_t)col * size + row] = src[(size_t)row * size + col];
}

/* Recorta las 4 esquinas de un bitmap YA TRANSPUESTO (buf[col*size+row])
 * al radio `radius` -- misma formula de distancia que stamp_corner()
 * (apple2026_shell.c) y mask_corners_buffer() (aura_nowplaying.c, T3.1),
 * adaptada al indexado transpuesto. Horneada UNA VEZ antes de escribir
 * el cache -- costo cero en cada cuadro de render (doc: "el bitmap
 * cacheado ya viene enmascarado"). */
static void mask_corners_transposed(fb_data *buf, int size, int radius, unsigned bg)
{
    int r256 = radius * 256;
    int row, col;

    for (row = 0; row < radius; row++)
    {
        for (col = 0; col < radius; col++)
        {
            int rr = radius - 1 - row;
            int rc = radius - 1 - col;
            int dist256 = (int)a26_shell_isqrt256((unsigned)(rr * rr + rc * rc));
            size_t idx[4];
            int k, t;

            if (dist256 <= r256 - 128)
                continue;

            idx[0] = (size_t)col * size + row;
            idx[1] = (size_t)(size - 1 - col) * size + row;
            idx[2] = (size_t)col * size + (size - 1 - row);
            idx[3] = (size_t)(size - 1 - col) * size + (size - 1 - row);

            if (dist256 >= r256 + 128)
            {
                for (k = 0; k < 4; k++)
                    buf[idx[k]] = bg;
                continue;
            }
            /* Borde antialiasado (misma rampa de 1px que stamp_corner,
             * apple2026_shell.c) -- cobertura de fondo lineal. */
            t = dist256 - (r256 - 128);
            for (k = 0; k < 4; k++)
                buf[idx[k]] = a26_shell_blend(buf[idx[k]], bg, t);
        }
    }
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
    char path[MAX_PATH];
    struct bitmap bm;
    int i, row, col, ret, ox, oy;
    const fb_data *mask;

    for (i = 0; i < size * size; i++)
        buf[i] = tile;

    snprintf(path, sizeof(path), "%s/aura/masks/music-%d.bmp",
              ICON_DIR, default_tile_icon_size(size));
    bm.data = (char *)s_decode_scratch;
    ret = read_bmp_file(path, &bm, sizeof(s_decode_scratch), FORMAT_NATIVE, NULL);
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
    mask_corners_transposed((fb_data *)out->cover_data, out->size, out->radius, bg);

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

    if (read_pfraw(path, out->size, out->radius, (fb_data *)out->cover_data))
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

    transpose((const fb_data *)s_decode_scratch, (fb_data *)s_transpose_scratch, out->size);
    mask_corners_transposed((fb_data *)s_transpose_scratch, out->size, out->radius, bg);
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

    if (read_pfraw(cache_path, out->size, out->radius, (fb_data *)out->cover_data))
    {
        aura_art_generate_reflection((const fb_data *)out->cover_data,
                                      (fb_data *)out->reflection_data,
                                      out->size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, bg, true);
        out->valid = true;
        return true;
    }

    if (!decode_playlist_art(playlist_filename, out->size))
        return false;

    transpose((const fb_data *)s_decode_scratch, (fb_data *)s_transpose_scratch, out->size);
    mask_corners_transposed((fb_data *)s_transpose_scratch, out->size, out->radius, bg);
    memcpy(out->cover_data, s_transpose_scratch,
           (size_t)out->size * out->size * sizeof(fb_data));

    write_pfraw(cache_path, out->size, out->radius, (const fb_data *)out->cover_data);

    aura_art_generate_reflection((const fb_data *)out->cover_data,
                                  (fb_data *)out->reflection_data,
                                  out->size, AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, bg, true);
    out->valid = true;
    return true;
}
