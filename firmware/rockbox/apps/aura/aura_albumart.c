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

#include "aura_albumart.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_art.h"

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
};

static void pfraw_path(int32_t album_seek, int size, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/%ld-%d.pfraw", CF_CACHE_DIR, (long)album_seek, size);
}

static bool read_pfraw(const char *path, int size, int radius, fb_data *out)
{
    struct pfraw_header hdr;
    size_t px_bytes = (size_t)size * size * sizeof(fb_data);
    int fd, n;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;

    n = read(fd, &hdr, sizeof(hdr));
    if (n != (int)sizeof(hdr) || hdr.size != size || hdr.radius != radius)
    {
        close(fd);
        return false;
    }

    n = read(fd, out, px_bytes);
    close(fd);
    return n == (int)px_bytes;
}

static void write_pfraw(const char *path, int size, int radius, const fb_data *data)
{
    struct pfraw_header hdr;
    int fd;

    hdr.size = size;
    hdr.radius = radius;

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
    int r2 = radius * radius;
    int row, col;

    for (row = 0; row < radius; row++)
    {
        for (col = 0; col < radius; col++)
        {
            int rr = radius - 1 - row;
            int rc = radius - 1 - col;

            if (rr * rr + rc * rc > r2)
            {
                buf[(size_t)col * size + row] = bg;
                buf[(size_t)(size - 1 - col) * size + row] = bg;
                buf[(size_t)col * size + (size - 1 - row)] = bg;
                buf[(size_t)(size - 1 - col) * size + (size - 1 - row)] = bg;
            }
        }
    }
}

/* Degradado diagonal de 3 puntos, version buffer TRANSPUESTO -- mismo
 * calculo exacto que draw_diagonal_gradient() (aura_selection_summary.c,
 * componentes/selection-summary.md: claro arriba-izquierda, acento al
 * centro, oscuro abajo-derecha) pero escribiendo directo a un fb_data[]
 * en vez de lcd_drawline() sobre la pantalla real -- ese componente
 * pinta la pantalla, esta funcion pinta un bitmap que despues pasa por
 * el mismo camino de proyeccion/reflejo que una caratula real (regla
 * dura 7: mismo patron visual, no un segundo look-and-feel para el caso
 * "sin caratula"). */
static void fill_diagonal_gradient_transposed(fb_data *buf, int size,
                                               unsigned color_a, unsigned color_center,
                                               unsigned color_b)
{
    int max_k = 2 * (size - 1);
    int row, col;

    for (row = 0; row < size; row++)
    {
        for (col = 0; col < size; col++)
        {
            int t256 = (row + col) * 256 / max_k;
            unsigned c = (t256 <= 128)
                ? a26_shell_blend(color_a, color_center, t256 * 2)
                : a26_shell_blend(color_center, color_b, (t256 - 128) * 2);

            buf[(size_t)col * size + row] = c;
        }
    }
}

void aura_albumart_load_default(aura_albumart_t *out)
{
    unsigned bg = a26_color(A26_SHELL_BG);

    fill_diagonal_gradient_transposed((fb_data *)out->cover_data, out->size,
                                       aura_accent_light(), aura_accent(), aura_accent_dark());
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

    if (!find_albumart(&fake_id3, art_path, sizeof(art_path), &dim))
        return false;

    bm.width = size;
    bm.height = size;
    bm.data = (char *)s_decode_scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    len = (int)strlen(art_path);
    if (len > 4 && !strcasecmp(art_path + len - 4, ".bmp"))
        ret = read_bmp_file(art_path, &bm, sizeof(s_decode_scratch), format, NULL);
    else
        ret = read_jpeg_file(art_path, &bm, sizeof(s_decode_scratch), format, NULL);

    return ret > 0;
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
