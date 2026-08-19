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
#include <stdlib.h>
#include <ctype.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "file.h"
#include "dir.h"
#include "plugin.h"
#include "string-extra.h"
#include "strnatcmp.h"
#include "recorder/bmp.h"
#include "recorder/jpeg_load.h"
#include "tick.h"

#include "aura_movieflow.h"
#include "aura_video.h"
#include "aura_art.h"
#include "aura_widgets.h"
#include "aura_wheel.h"
#include "aura_scroll_indicator.h"
#include "aura_marquee.h"
#include "aura_main.h"
#include "aura_flow.h"
#include "aura_motion.h"
#include "aura_patterns.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_status_bar_v2.h"

#define VIDEOS_DIR "/Videos"

/* -- Geometria (D-318) --------------------------------------------------
 *
 * A diferencia de Music Flow (tapa cuadrada, caratula de album), Movie
 * Flow usa formato RECTANGULAR 3:4 (encargo textual del dueno) --
 * proporcion de cartel de pelicula/serie, no de caratula. El motor de
 * proyeccion (aura_flow.c) ya es agnostico de ancho/alto -- solo pide un
 * `slide_width_px` para la horizontal; el alto lo decide el LLAMADOR via
 * aura_flow_vertical_scale() por columna (ver draw_slide_perspective()
 * mas abajo) -- generalizar de cuadrado a 3:4 no toco el motor
 * compartido en absoluto, solo este archivo. */
#define MVF_COVER_W       120
#define MVF_COVER_H       160 /* 120:160 = 3:4 exacto */
#define MVF_CORNER_RADIUS AURA_DS_METRICS_MUSIC_FLOW_CORNER_RADIUS
#define MVF_REFLECTION_PCT AURA_DS_METRICS_MUSIC_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT
#define MVF_REFLECTION_H  (MVF_COVER_H * MVF_REFLECTION_PCT / 100)
#define MVF_TOP_Y         30
#define MVF_VISIBLE_RADIUS AURA_DS_METRICS_MUSIC_FLOW_SIDE_SLIDES_PER_SIDE
#define MVF_CACHE_SLOTS   (2 * (MVF_VISIBLE_RADIUS + 15) + 3)
#define MVF_SIDE_FADE     165

#define MVF_TITLE_Y (MVF_TOP_Y + MVF_COVER_H + 14)

/* Reverso (lista de episodios de una TEMPORADA) -- MISMA geometria que
 * el reverso de Music Flow (200x200, cuadrado): decision del dueno de
 * reusar el panel existente, no un componente nuevo -- el panel en si
 * nunca dibuja la tapa (solo cabecera+lista de texto), asi que su forma
 * es independiente del 3:4 del carrusel. */
#define MVF_BACK_SIZE   200
#define MVF_BACK_X      ((A26_SCREEN_WIDTH - MVF_BACK_SIZE) / 2)
#define MVF_BACK_Y      (A26_LAYOUT_STATUSBAR_HEIGHT \
                         + (A26_SCREEN_HEIGHT - A26_LAYOUT_STATUSBAR_HEIGHT - MVF_BACK_SIZE) / 2)
#define MVF_BACK_HEADER_H 20
#define MVF_BACK_PADDING  4
/* Ancho objetivo de la tapa AL CRECER durante el giro -- 150 en vez de
 * 200 (a diferencia de Music Flow) porque el motor preserva la
 * proporcion 3:4 automaticamente (proyeccion real, no escalado en dos
 * ejes independientes): 150 de ancho da 150*(160/120)=200 de alto, el
 * mismo alto que el panel del reverso -- crece "hasta llenar" el panel
 * en su eje vertical sin necesitar logica nueva. */
#define MVF_GROWN_W 150
#define MVF_GROW_DISTANCE (AURA_FLOW_CAM_DIST * MVF_COVER_W / MVF_GROWN_W - AURA_FLOW_CAM_DIST)
#define MVF_BACK_VISIBLE_ROWS ((MVF_BACK_SIZE - MVF_BACK_HEADER_H) / MVF_TRACK_ROW_H_EARLY)
#define MVF_TRACK_ROW_H_EARLY 20

static int s_prev_ep_sel = -1;
static long s_ep_activity_since = 0;

static int s_header_for_index = -1;
static long s_header_since = 0;
static int s_header_overflowing = 0;

static int marquee_phase_scrolling(long since)
{
    long elapsed_ms = (current_tick - since) * 1000L / HZ;
    long cycle_ms = AURA_DS_METRICS_MARQUEE_STATIC_MS + AURA_DS_METRICS_MARQUEE_SCROLL_MS;

    if (cycle_ms <= 0)
        return 0;
    return (elapsed_ms % cycle_ms) >= AURA_DS_METRICS_MARQUEE_STATIC_MS;
}

/* -- Modelo de datos: Peliculas + Temporadas (D-318) --------------------
 *
 * Un slide de Movie Flow es una PELICULA o una TEMPORADA -- nunca un
 * episodio individual. El pool combinado es el mismo "Peliculas+Series,
 * nunca Videoclips" que ya usa CoverDrift de Video (restriccion textual
 * del dueno, D-316) -- aca se construye aparte porque CoverDrift solo
 * necesita CONTAR y nombrar archivos (aura_video_count_filtered()/
 * aura_video_filtered_filename()), nunca agruparlos por temporada. */
#define MVF_MAX_ENTRIES   100
#define MVF_MAX_EPISODES  60
#define MVF_NAME_LEN      96

typedef enum { MVF_ENTRY_MOVIE = 0, MVF_ENTRY_SEASON } mvf_entry_kind_t;

typedef struct {
    char filename[MVF_NAME_LEN]; /* nombre real en /Videos, con extension */
    char label[40];              /* "Episodio NN" */
    int episode_num;
} mvf_episode_t;

typedef struct {
    mvf_entry_kind_t kind;
    char title[MVF_NAME_LEN];       /* titulo mostrado bajo la tapa central */
    char poster_base[MVF_NAME_LEN]; /* nombre SIN extension del .jpg hermano */
    char video_filename[MVF_NAME_LEN]; /* solo MOVIE: archivo a reproducir */
    int episode_count;              /* solo SEASON */
    mvf_episode_t episodes[MVF_MAX_EPISODES];
} mvf_entry_t;

static mvf_entry_t s_entries[MVF_MAX_ENTRIES];
static int s_entry_count = -1; /* -1 = sin escanear */

static void strip_ext(const char *filename, char *out, size_t outsz)
{
    char *dot;

    strlcpy(out, filename, outsz);
    dot = strrchr(out, '.');
    if (dot)
        *dot = '\0';
}

/* D-318: agrupacion por temporada via convencion de nombre "SxxEyy"
 * (mismo patron que Aura Studio ya usa internamente, ST-033 en
 * Aura-Studio: "series primero si trae SxxEyy") -- busca " SxxEyy"
 * (espacio, S, 2 digitos, E, 2 digitos, insensible a mayusculas) en
 * `display` (ya sin extension). `show_out` recibe todo lo anterior al
 * espacio (el nombre del programa); devuelve false si no hay match --
 * el llamador cae a un fallback de "temporada de un solo episodio". */
static bool parse_sxxeyy(const char *display, char *show_out, size_t show_out_sz,
                          int *season_out, int *episode_out)
{
    size_t len = strlen(display);
    size_t i;

    if (len < 7)
        return false;

    for (i = 0; i + 7 <= len; i++)
    {
        if (display[i] != ' ')
            continue;
        if ((display[i + 1] != 'S' && display[i + 1] != 's'))
            continue;
        if (!isdigit((unsigned char)display[i + 2]) || !isdigit((unsigned char)display[i + 3]))
            continue;
        if (display[i + 4] != 'E' && display[i + 4] != 'e')
            continue;
        if (!isdigit((unsigned char)display[i + 5]) || !isdigit((unsigned char)display[i + 6]))
            continue;

        if (i >= show_out_sz)
            i = show_out_sz - 1;
        memcpy(show_out, display, i);
        show_out[i] = '\0';
        *season_out = (display[i + 2] - '0') * 10 + (display[i + 3] - '0');
        *episode_out = (display[i + 5] - '0') * 10 + (display[i + 6] - '0');
        return true;
    }
    return false;
}

static int compare_entry_title(const void *a, const void *b)
{
    const mvf_entry_t *ea = a, *eb = b;
    return strnatcasecmp(ea->title, eb->title);
}

static mvf_entry_t *find_or_create_season(const char *show, int season)
{
    char poster_base[MVF_NAME_LEN];
    char title[MVF_NAME_LEN];
    int i;

    snprintf(poster_base, sizeof(poster_base), "%s S%02d", show, season);
    for (i = 0; i < s_entry_count; i++)
        if (s_entries[i].kind == MVF_ENTRY_SEASON
            && !strcmp(s_entries[i].poster_base, poster_base))
            return &s_entries[i];

    if (s_entry_count >= MVF_MAX_ENTRIES)
        return NULL;

    snprintf(title, sizeof(title), "%s \xe2\x80\x94 Temporada %d", show, season);
    s_entries[s_entry_count].kind = MVF_ENTRY_SEASON;
    strlcpy(s_entries[s_entry_count].title, title, sizeof(s_entries[s_entry_count].title));
    strlcpy(s_entries[s_entry_count].poster_base, poster_base,
            sizeof(s_entries[s_entry_count].poster_base));
    s_entries[s_entry_count].video_filename[0] = '\0';
    s_entries[s_entry_count].episode_count = 0;
    return &s_entries[s_entry_count++];
}

static void add_episode(mvf_entry_t *season, const char *filename, int episode_num)
{
    mvf_episode_t *ep;
    int i, pos;

    if (season->episode_count >= MVF_MAX_EPISODES)
        return;

    /* Insercion ordenada por numero de episodio -- MVF_MAX_EPISODES es
     * chico (60), insercion simple alcanza sin necesitar qsort aparte. */
    pos = season->episode_count;
    for (i = 0; i < season->episode_count; i++)
        if (season->episodes[i].episode_num > episode_num)
        { pos = i; break; }

    for (i = season->episode_count; i > pos; i--)
        season->episodes[i] = season->episodes[i - 1];

    ep = &season->episodes[pos];
    strlcpy(ep->filename, filename, sizeof(ep->filename));
    ep->episode_num = episode_num;
    snprintf(ep->label, sizeof(ep->label), "Episodio %02d", episode_num);
    season->episode_count++;
}

/* Reconstruye s_entries[] desde el pool filtrado de aura_video.c
 * (Peliculas + Series, D-316) -- bajo demanda, cacheado hasta la
 * proxima invalidacion (mismo criterio que aura_video.c/aura_photos.c). */
static void ensure_movieflow_entries(void)
{
    int i, n;

    if (s_entry_count >= 0)
        return;

    s_entry_count = 0;

    n = aura_video_count_filtered(AURA_VIDEO_CAT_MOVIE);
    for (i = 0; i < n && s_entry_count < MVF_MAX_ENTRIES; i++)
    {
        const char *fn = aura_video_filtered_filename(AURA_VIDEO_CAT_MOVIE, i);
        mvf_entry_t *e;

        if (!fn)
            continue;
        e = &s_entries[s_entry_count++];
        e->kind = MVF_ENTRY_MOVIE;
        strip_ext(fn, e->title, sizeof(e->title));
        strlcpy(e->poster_base, e->title, sizeof(e->poster_base));
        strlcpy(e->video_filename, fn, sizeof(e->video_filename));
        e->episode_count = 0;
    }

    n = aura_video_count_filtered(AURA_VIDEO_CAT_SERIES);
    for (i = 0; i < n; i++)
    {
        const char *fn = aura_video_filtered_filename(AURA_VIDEO_CAT_SERIES, i);
        char display[MVF_NAME_LEN];
        char show[MVF_NAME_LEN];
        int season, episode;

        if (!fn)
            continue;
        strip_ext(fn, display, sizeof(display));

        if (parse_sxxeyy(display, show, sizeof(show), &season, &episode))
        {
            mvf_entry_t *e = find_or_create_season(show, season);
            if (e)
                add_episode(e, fn, episode);
        }
        else if (s_entry_count < MVF_MAX_ENTRIES)
        {
            /* D-318: sin patron SxxEyy reconocible -- fallback honesto,
             * temporada de un solo episodio con su propio nombre, en vez
             * de descartar el archivo en silencio. */
            mvf_entry_t *e = &s_entries[s_entry_count++];
            e->kind = MVF_ENTRY_SEASON;
            strlcpy(e->title, display, sizeof(e->title));
            strlcpy(e->poster_base, display, sizeof(e->poster_base));
            e->video_filename[0] = '\0';
            e->episode_count = 0;
            add_episode(e, fn, 1);
        }
    }

    qsort(s_entries, s_entry_count, sizeof(s_entries[0]), compare_entry_title);
}

void aura_movieflow_invalidate(void)
{
    s_entry_count = -1;
}

/* -- Cache de tapas decodificadas (mismo patron que cf_slot_t de Music
 * Flow, D-318) ------------------------------------------------------
 *
 * Sin tagcache/aura_albumart.c de por medio -- el cartel es un archivo
 * de imagen directo (mismo mecanismo que ya usa CoverDrift de Video/
 * Fotos, aura_screens.c:decode_album_drift_tile()): read_jpeg_file()/
 * read_bmp_file() a un lienzo MVF_COVER_W x MVF_COVER_H (letterbox si el
 * origen no calza el 3:4 exacto), transpuesto a columna-contigua (lo que
 * espera el proyector) y con su reflejo generado a mano (aura_art.c
 * asume cuadrado, no reutilizable tal cual para W!=H). */
typedef struct {
    int entry_index; /* -1 = libre */
    bool valid;
    fb_data cover_buf[MVF_COVER_W * MVF_COVER_H];
    fb_data reflection_buf[MVF_COVER_W * MVF_REFLECTION_H];
} mvf_slot_t;

static mvf_slot_t s_slots[MVF_CACHE_SLOTS];

static unsigned mvf_placeholder_color(int index)
{
    switch (index % 4)
    {
        case 0: return aura_accent();
        case 1: return aura_accent_light();
        case 2: return aura_accent_dark();
        default: return a26_shell_blend(aura_accent(), a26_color(A26_SHELL_BG), 128);
    }
}

/* Esquinas redondeadas propias (W!=H, aura_art_mask_corners_transposed()
 * asume cuadrado -- un solo `size` para ambos ejes) -- mismo algoritmo
 * EXACTO (distancia radial + rampa antialiasada de 1px via
 * a26_shell_isqrt256(), idéntica a stamp_corner() de apple2026_shell.c),
 * generalizado a MVF_COVER_W x MVF_COVER_H. Opera sobre el buffer YA
 * transpuesto (columna-contigua): `buf[col*H + row]`. Debe correr ANTES
 * de generar el reflejo (mismo orden que aura_albumart.c) -- el reflejo
 * espeja lo que ya tiene las esquinas redondeadas. */
static void mvf_mask_corners(fb_data *buf, int radius, unsigned bg)
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

            idx[0] = (size_t)col * MVF_COVER_H + row;
            idx[1] = (size_t)(MVF_COVER_W - 1 - col) * MVF_COVER_H + row;
            idx[2] = (size_t)col * MVF_COVER_H + (MVF_COVER_H - 1 - row);
            idx[3] = (size_t)(MVF_COVER_W - 1 - col) * MVF_COVER_H + (MVF_COVER_H - 1 - row);

            if (dist256 >= r256 + 128)
            {
                for (k = 0; k < 4; k++)
                    buf[idx[k]] = bg;
                continue;
            }
            t = dist256 - (r256 - 128);
            for (k = 0; k < 4; k++)
                buf[idx[k]] = a26_shell_blend(buf[idx[k]], bg, t);
        }
    }
}

/* Reflejo propio (W!=H, aura_art_generate_reflection() asume cuadrado) --
 * mismo algoritmo exacto (espejo vertical + atenuacion lineal hacia el
 * fondo, pico AURA_ART_REFLECTION_PEAK_PCT), formato de salida SIEMPRE
 * transpuesto (columna-contigua), unico layout que este archivo usa. */
static void mvf_generate_reflection(const fb_data *cover_transposed, fb_data *out,
                                     unsigned bg_color)
{
    int bg_r = RGB_UNPACK_RED(bg_color);
    int bg_g = RGB_UNPACK_GREEN(bg_color);
    int bg_b = RGB_UNPACK_BLUE(bg_color);
    int y, x;

    for (y = 0; y < MVF_REFLECTION_H; y++)
    {
        int source_row = MVF_COVER_H - 1 - y;
        int peak = 255 * AURA_ART_REFLECTION_PEAK_PCT / 100;
        int fade = peak - (peak * y) / MVF_REFLECTION_H;

        for (x = 0; x < MVF_COVER_W; x++)
        {
            unsigned px = cover_transposed[(size_t)x * MVF_COVER_H + source_row];
            int r = RGB_UNPACK_RED(px), g = RGB_UNPACK_GREEN(px), b = RGB_UNPACK_BLUE(px);

            r = bg_r + ((r - bg_r) * fade) / 255;
            g = bg_g + ((g - bg_g) * fade) / 255;
            b = bg_b + ((b - bg_b) * fade) / 255;
            out[(size_t)x * MVF_REFLECTION_H + y] = LCD_RGBPACK(r, g, b);
        }
    }
}

static void load_slot(mvf_slot_t *slot, int entry_index)
{
    static fb_data decode_buf[MVF_COVER_W * MVF_COVER_H]; /* scratch de decodificacion, fila-contigua */
    static fb_data flat[MVF_COVER_W * MVF_COVER_H];        /* lienzo final, fila-contigua, antes de transponer */
    char path[MAX_PATH];
    struct bitmap bm;
    unsigned bg = a26_color(A26_SHELL_BG);
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int ret, ox, oy, row, col;

    slot->entry_index = entry_index;
    slot->valid = false;

    snprintf(path, sizeof(path), "%s/%s.jpg", VIDEOS_DIR, s_entries[entry_index].poster_base);

    bm.width = MVF_COVER_W;
    bm.height = MVF_COVER_H;
    bm.data = (char *)decode_buf;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif
    ret = read_jpeg_file(path, &bm, sizeof(decode_buf), format, NULL);

    if (ret <= 0)
    {
        /* Sin cartel: placeholder solido -- mismo criterio que
         * CoverDrift (aura_coverdrift.c:placeholder_color()), nunca un
         * hueco en blanco. */
        unsigned ph = mvf_placeholder_color(entry_index);
        for (row = 0; row < MVF_COVER_H; row++)
            for (col = 0; col < MVF_COVER_W; col++)
                flat[row * MVF_COVER_W + col] = ph;
    }
    else
    {
        ox = (MVF_COVER_W - bm.width) / 2;
        oy = (MVF_COVER_H - bm.height) / 2;
        for (row = 0; row < MVF_COVER_H; row++)
        {
            int sy = row - oy;
            for (col = 0; col < MVF_COVER_W; col++)
            {
                int sx = col - ox;
                flat[row * MVF_COVER_W + col] =
                    (sx >= 0 && sx < bm.width && sy >= 0 && sy < bm.height)
                        ? decode_buf[sy * bm.width + sx] : bg;
            }
        }
    }

    /* Transpone a columna-contigua (lo que lee draw_slide_perspective()/
     * draw_slide_flip(), mismo formato que cf_slot_t de Music Flow). */
    for (row = 0; row < MVF_COVER_H; row++)
        for (col = 0; col < MVF_COVER_W; col++)
            slot->cover_buf[(size_t)col * MVF_COVER_H + row] = flat[row * MVF_COVER_W + col];

    mvf_mask_corners(slot->cover_buf, MVF_CORNER_RADIUS, bg);
    mvf_generate_reflection(slot->cover_buf, slot->reflection_buf, bg);
    slot->valid = true;
}

static mvf_slot_t *get_slot_for(int entry_index)
{
    static int s_slots_theme = -1;
    int i, target, free_slot = -1, farthest = -1, farthest_dist = -1;

    if (s_slots_theme != (int)aura_settings.theme)
    {
        s_slots_theme = (int)aura_settings.theme;
        for (i = 0; i < MVF_CACHE_SLOTS; i++)
            s_slots[i].entry_index = -1;
    }

    for (i = 0; i < MVF_CACHE_SLOTS; i++)
        if (s_slots[i].entry_index == entry_index)
            return &s_slots[i];

    for (i = 0; i < MVF_CACHE_SLOTS; i++)
    {
        int dist;
        if (s_slots[i].entry_index == -1)
        {
            free_slot = i;
            break;
        }
        dist = abs(s_slots[i].entry_index - entry_index);
        if (dist > farthest_dist)
        {
            farthest_dist = dist;
            farthest = i;
        }
    }
    target = (free_slot >= 0) ? free_slot : farthest;
    load_slot(&s_slots[target], entry_index);
    return &s_slots[target];
}

/* -- Posicion animada + zoom-on-scroll (D-245/D-246 de Music Flow,
 * mismo patron exacto) --------------------------------------------- */
#define MVF_SCROLL_ANIM_MS 220
static int s_target_index = 0;
static int s_anim_from_x256 = 0;
static long s_anim_since = 0;

static int anim_pos_x256(void)
{
    long elapsed_ms = (current_tick - s_anim_since) * 1000L / HZ;
    int t = aura_motion_linear(elapsed_ms, MVF_SCROLL_ANIM_MS);
    return aura_pattern_lerp(s_anim_from_x256, s_target_index * 256, t);
}

#define MVF_ZOOM_SCALE_NORMAL  256
#define MVF_ZOOM_SCALE_SHRUNK  216
#define MVF_ZOOM_OUT_MS 150
#define MVF_ZOOM_IN_MS  MVF_SCROLL_ANIM_MS
static int s_zoom_target_256 = MVF_ZOOM_SCALE_NORMAL;
static int s_zoom_from_256 = MVF_ZOOM_SCALE_NORMAL;
static long s_zoom_since = 0;
static int s_zoom_shrinking = 0;

static int zoom_scale_256(void)
{
    long elapsed_ms, duration_ms;
    int t;

    if (s_zoom_from_256 == s_zoom_target_256)
        return s_zoom_target_256;

    elapsed_ms = (current_tick - s_zoom_since) * 1000L / HZ;
    duration_ms = s_zoom_shrinking ? MVF_ZOOM_OUT_MS : MVF_ZOOM_IN_MS;
    t = s_zoom_shrinking ? aura_motion_ease_in(elapsed_ms, duration_ms)
                          : aura_motion_ease_out(elapsed_ms, duration_ms);
    return aura_pattern_lerp(s_zoom_from_256, s_zoom_target_256, t);
}

static int zoom_distance(void)
{
    int scale = zoom_scale_256();
    if (scale >= MVF_ZOOM_SCALE_NORMAL)
        return 0;
    return AURA_FLOW_CAM_DIST * (MVF_ZOOM_SCALE_NORMAL - scale) / scale;
}

static int zoom_animating(void)
{
    return zoom_scale_256() != s_zoom_target_256;
}

static void zoom_trigger_scroll_start(void)
{
    if (s_zoom_target_256 == MVF_ZOOM_SCALE_SHRUNK)
        return;
    s_zoom_from_256 = zoom_scale_256();
    s_zoom_target_256 = MVF_ZOOM_SCALE_SHRUNK;
    s_zoom_since = current_tick;
    s_zoom_shrinking = 1;
}

static void zoom_trigger_settle(void)
{
    if (s_zoom_target_256 == MVF_ZOOM_SCALE_NORMAL)
        return;
    s_zoom_from_256 = zoom_scale_256();
    s_zoom_target_256 = MVF_ZOOM_SCALE_NORMAL;
    s_zoom_since = current_tick;
    s_zoom_shrinking = 0;
}

/* -- Flip + lista de episodios (solo TEMPORADAS, D-318) -- las
 * PELICULAS nunca pasan por estos estados: SELECT las reproduce al
 * instante desde IDLE. --------------------------------------------- */
typedef enum {
    MVF_STATE_IDLE = 0,
    MVF_STATE_COVER_IN,
    MVF_STATE_SHOW_EPISODES,
    MVF_STATE_COVER_OUT,
} mvf_state_t;

#define MVF_FLIP_MS AURA_DS_METRICS_MUSIC_FLOW_FLIP_MS

static mvf_state_t s_state = MVF_STATE_IDLE;
static long s_state_since = 0;
static int s_ep_sel = 0;

static int flip_progress_256(void)
{
    long elapsed_ms = (current_tick - s_state_since) * 1000L / HZ;
    return aura_motion_linear(elapsed_ms, MVF_FLIP_MS);
}

static int position_pending(void)
{
    return anim_pos_x256() != s_target_index * 256
        || s_state == MVF_STATE_COVER_IN || s_state == MVF_STATE_COVER_OUT;
}

int aura_movieflow_pending(void)
{
    if (s_state == MVF_STATE_SHOW_EPISODES)
    {
        if (s_header_overflowing)
            return 1;
        if (s_entries[s_target_index].episode_count > MVF_BACK_VISIBLE_ROWS)
        {
            long idle_ms = (current_tick - s_ep_activity_since) * 1000L / HZ;
            if (aura_scroll_indicator_pending(idle_ms))
                return 1;
        }
    }
    return anim_pos_x256() != s_target_index * 256
        || zoom_animating()
        || s_state == MVF_STATE_COVER_IN || s_state == MVF_STATE_COVER_OUT;
}

int aura_movieflow_animating(void)
{
    if (s_state == MVF_STATE_SHOW_EPISODES)
        return s_header_overflowing && marquee_phase_scrolling(s_header_since);
    return anim_pos_x256() != s_target_index * 256
        || zoom_animating()
        || s_state == MVF_STATE_COVER_IN || s_state == MVF_STATE_COVER_OUT;
}

/* -- Perspectiva real (adaptada de Music Flow para W!=H) ----------------
 *
 * Identica en estructura a draw_slide_perspective() de Music Flow --
 * unica diferencia real: el bucle vertical usa MVF_COVER_H (no
 * MVF_COVER_W) como limite de fuente, y aura_flow_begin_projection()
 * recibe MVF_COVER_W como ancho de slide -- el motor ya soporta esto,
 * ver comentario de geometria arriba. */
#define MVF_ITILT           199
/* D-318: reescalados proporcionalmente al ancho de slide (120 vs los
 * 130 de Music Flow) desde las constantes ya re-derivadas de esa
 * pantalla contra la referencia de Apple -- mismo espaciado RELATIVO,
 * sin una referencia visual propia para un carrusel de carteles. */
#define MVF_OFFSETX_R       84923  /* 92000 * 120/130 */
#define MVF_SLIDE_SPACING_R 26769  /* 29000 * 120/130 */

static void build_fade_lut(unsigned char *lut, int fade, int bg_channel)
{
    int v;
    for (v = 0; v < 256; v++)
        lut[v] = bg_channel + ((v - bg_channel) * fade) / 255;
}

static fb_data lut_pixel(unsigned px, const unsigned char *lut_r,
                          const unsigned char *lut_g, const unsigned char *lut_b)
{
    return LCD_RGBPACK(lut_r[RGB_UNPACK_RED(px)],
                        lut_g[RGB_UNPACK_GREEN(px)],
                        lut_b[RGB_UNPACK_BLUE(px)]);
}

static void draw_slide_perspective(const mvf_slot_t *slot, int offset_x256, int zoom_dist)
{
    aura_flow_slide_t slide;
    aura_flow_projection_t proj;
    int sign = (offset_x256 < 0) ? -1 : (offset_x256 > 0 ? 1 : 0);
    int abs_x256 = (offset_x256 < 0) ? -offset_x256 : offset_x256;
    int t_center = abs_x256 < 256 ? abs_x256 : 256;
    int extra_x256 = abs_x256 - 256;
    int fade;
    const fb_data *cover = slot->cover_buf;
    const fb_data *refl = slot->reflection_buf;
    int total_h = MVF_COVER_H + MVF_REFLECTION_H;
    unsigned bg = a26_color(A26_SHELL_BG);
    int bg_r = RGB_UNPACK_RED(bg), bg_g = RGB_UNPACK_GREEN(bg), bg_b = RGB_UNPACK_BLUE(bg);
    static fb_data col_buf[MVF_COVER_H + MVF_REFLECTION_H];
    static unsigned char lut_r[256], lut_g[256], lut_b[256];
    const unsigned char *use_lut_r = NULL, *use_lut_g = NULL, *use_lut_b = NULL;

    if (extra_x256 < 0)
        extra_x256 = 0;

    slide.angle = -sign * aura_pattern_lerp(0, MVF_ITILT, t_center);
    slide.distance = zoom_dist;
    slide.cx = sign * (aura_pattern_lerp(0, MVF_OFFSETX_R, t_center)
                        + (int)((long)MVF_SLIDE_SPACING_R * extra_x256 / 256));

    fade = aura_pattern_lerp(255, MVF_SIDE_FADE, t_center);
    if (fade < 255)
    {
        build_fade_lut(lut_r, fade, bg_r);
        build_fade_lut(lut_g, fade, bg_g);
        build_fade_lut(lut_b, fade, bg_b);
        use_lut_r = lut_r; use_lut_g = lut_g; use_lut_b = lut_b;
    }

    aura_flow_begin_projection(&proj, &slide, MVF_COVER_W);

    while (proj.screen_x < AURA_FLOW_SCREEN_W)
    {
        int col = aura_flow_source_column(&proj);
        int dy = aura_flow_vertical_scale(&proj);
        int p = 0, dest_row, n_rows = 0;
        const fb_data *cover_col = cover + (size_t)col * MVF_COVER_H;
        const fb_data *refl_col = refl + (size_t)col * MVF_REFLECTION_H;
        int cover_disp = (MVF_COVER_H << AURA_FLOW_SHIFT) / dy;
        int y_col = MVF_TOP_Y + MVF_COVER_H / 2 - cover_disp / 2;

        for (dest_row = 0; dest_row < total_h; dest_row++)
        {
            int source_row = p >> AURA_FLOW_SHIFT;
            unsigned px;

            if (source_row >= total_h)
                break;
            px = (source_row < MVF_COVER_H) ? cover_col[source_row]
                                             : refl_col[source_row - MVF_COVER_H];
            col_buf[dest_row] = (fade < 255) ? lut_pixel(px, use_lut_r, use_lut_g, use_lut_b) : px;
            p += dy;
            n_rows++;
        }
        if (n_rows > 0)
            lcd_bitmap(col_buf, proj.screen_x, y_col, 1, n_rows);

        if (!aura_flow_advance_column(&proj))
            break;
    }
}

static void draw_slide_flip(const mvf_slot_t *slot, int iangle_0_to_256)
{
    aura_flow_slide_t slide;
    aura_flow_projection_t proj;
    const fb_data *cover = slot->cover_buf;
    const fb_data *refl = slot->reflection_buf;
    static fb_data col_buf[MVF_GROWN_W * MVF_COVER_H / MVF_COVER_W + 8];
    static fb_data refl_buf[MVF_REFLECTION_H * MVF_GROWN_W / MVF_COVER_W + 8];
    int refl_vis = 256 - iangle_0_to_256;
    int refl_drop = ((256 - refl_vis) * 64) / 256;
    int center_y = aura_pattern_lerp(MVF_TOP_Y + MVF_COVER_H / 2,
                                      MVF_BACK_Y + MVF_BACK_SIZE / 2,
                                      iangle_0_to_256);
    unsigned bg = a26_color(A26_SHELL_BG);

    slide.angle = iangle_0_to_256;
    slide.distance = (MVF_GROW_DISTANCE * iangle_0_to_256) / 256;
    slide.cx = 0;

    aura_flow_begin_projection(&proj, &slide, MVF_COVER_W);

    while (proj.screen_x < AURA_FLOW_SCREEN_W)
    {
        int col = aura_flow_source_column(&proj);
        int dy = aura_flow_vertical_scale(&proj);
        int p = 0, dest_row, n_rows = 0;
        const fb_data *cover_col = cover + (size_t)col * MVF_COVER_H;
        const fb_data *refl_col = refl + (size_t)col * MVF_REFLECTION_H;
        int cover_disp = (MVF_COVER_H << AURA_FLOW_SHIFT) / dy;
        int y_col = center_y - cover_disp / 2;

        for (dest_row = 0; dest_row < (int)(sizeof(col_buf) / sizeof(col_buf[0])); dest_row++)
        {
            int source_row = p >> AURA_FLOW_SHIFT;

            if (source_row >= MVF_COVER_H)
                break;
            col_buf[dest_row] = cover_col[source_row];
            p += dy;
            n_rows++;
        }
        if (n_rows > 0)
            lcd_bitmap(col_buf, proj.screen_x, y_col, 1, n_rows);

        if (refl_vis > 0)
        {
            int r_rows = 0;
            p = 0;
            for (dest_row = 0; dest_row < (int)(sizeof(refl_buf) / sizeof(refl_buf[0])); dest_row++)
            {
                int source_row = p >> AURA_FLOW_SHIFT;

                if (source_row >= MVF_REFLECTION_H)
                    break;
                refl_buf[dest_row] = a26_shell_blend(bg, refl_col[source_row], refl_vis);
                p += dy;
                r_rows++;
            }
            if (r_rows > 0)
                lcd_bitmap(refl_buf, proj.screen_x, y_col + n_rows + refl_drop, 1, r_rows);
        }

        if (!aura_flow_advance_column(&proj))
            break;
    }
}

static void draw_clipped_text(int x, int y, int w, const char *text)
{
    struct viewport vp = *lcd_current_viewport;
    struct viewport *saved;

    vp.x = x; vp.y = y; vp.width = w;
    saved = lcd_set_viewport(&vp);
    lcd_putsxy(0, 0, (const unsigned char *)text);
    lcd_set_viewport(saved);
}

/* -- Panel de episodios (D-318): reusa TAL CUAL la geometria/mecanica
 * del panel de pistas de Music Flow (decision del dueno: lista de
 * texto, no carteles nuevos) -- solo cambia la fuente de datos
 * (episodios de la temporada en vez de canciones del album). */
#define MVF_TRACK_ROW_H 20
#define MVF_TRACK_TEXT_INSET (MVF_BACK_PADDING + 10)

static void draw_episodelist_panel(void)
{
    const mvf_entry_t *season = &s_entries[s_target_index];
    int panel_x = MVF_BACK_X, panel_y = MVF_BACK_Y, pad = MVF_BACK_PADDING;
    int list_y = panel_y + MVF_BACK_HEADER_H + 1;
    int visible = MVF_BACK_VISIBLE_ROWS;
    int first, i, w, h;
    unsigned bg = a26_color(A26_SHELL_BG);
    unsigned panel_bg = a26_shell_blend(bg, a26_color(A26_TEXT_PRIMARY), 11);
    long header_elapsed_ms;

    a26_shell_fill_rounded_rect(panel_x, panel_y, MVF_BACK_SIZE, MVF_BACK_SIZE,
                                 MVF_CORNER_RADIUS, panel_bg, bg);

    if (s_header_for_index != s_target_index)
    {
        s_header_for_index = s_target_index;
        s_header_since = current_tick;
    }
    header_elapsed_ms = (current_tick - s_header_since) * 1000L / HZ;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_10));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_getstringsize((const unsigned char *)season->title, &w, &h);
    if (w <= MVF_BACK_SIZE - 2 * pad)
    {
        lcd_putsxy(panel_x + (MVF_BACK_SIZE - w) / 2,
                   panel_y + (MVF_BACK_HEADER_H - h) / 2,
                   (const unsigned char *)season->title);
        s_header_overflowing = 0;
    }
    else
        s_header_overflowing = aura_marquee_draw(panel_x + pad,
                                                  panel_y + (MVF_BACK_HEADER_H - h) / 2,
                                                  MVF_BACK_SIZE - 2 * pad,
                                                  season->title, header_elapsed_ms);

    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_hline(panel_x + pad, panel_x + MVF_BACK_SIZE - pad - 1, panel_y + MVF_BACK_HEADER_H);

    if (season->episode_count == 0)
        return;

    if (s_ep_sel != s_prev_ep_sel)
    {
        s_prev_ep_sel = s_ep_sel;
        s_ep_activity_since = current_tick;
    }

    first = s_ep_sel - visible / 2;
    if (first < 0) first = 0;
    if (first > season->episode_count - visible)
        first = season->episode_count > visible ? season->episode_count - visible : 0;

    if (s_ep_sel >= first && s_ep_sel < first + visible)
    {
        int sel_y = list_y + (s_ep_sel - first) * MVF_TRACK_ROW_H;
        a26_shell_fill_rounded_rect(panel_x + pad, sel_y, MVF_BACK_SIZE - 2 * pad, MVF_TRACK_ROW_H,
                                     AURA_DS_METRICS_SELECTOR_CORNER_RADIUS,
                                     a26_color(A26_SELECTION_FILL), panel_bg);
    }

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    for (i = 0; i < visible && first + i < season->episode_count; i++)
    {
        int idx = first + i;
        int row_y = list_y + i * MVF_TRACK_ROW_H + (MVF_TRACK_ROW_H - A26_TYPE_CAPTION) / 2;

        lcd_set_foreground(idx == s_ep_sel ? aura_accent() : a26_color(A26_TEXT_PRIMARY));
        draw_clipped_text(panel_x + MVF_TRACK_TEXT_INSET, row_y,
                           MVF_BACK_SIZE - 2 * MVF_TRACK_TEXT_INSET,
                           season->episodes[idx].label);
    }

    if (season->episode_count > visible)
    {
        long idle_ms = (current_tick - s_ep_activity_since) * 1000L / HZ;
        aura_scroll_indicator_draw(panel_x + MVF_BACK_SIZE - pad, list_y, visible * MVF_TRACK_ROW_H,
                                    s_ep_sel, season->episode_count, idle_ms,
                                    panel_bg, a26_color(A26_SHELL_RAIL));
    }
}

static void draw_message(aura_str_id_t msg_id)
{
    int w, h;
    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, (A26_SCREEN_HEIGHT - h) / 2,
               (const unsigned char *)aura_str(msg_id));
}

/* -- Entrada al reproductor: fundido a negro, NUNCA Flip-and-Flow
 * (D-318, encargo textual del dueno) -- y despues plugin_load()
 * DIRECTO del mpegplayer, el mismo mecanismo que ya usa aura_video.c
 * (nunca la pantalla NowPlaying de Musica: un video no es una cancion,
 * el plugin toma la pantalla completa por su cuenta). */
#define MVF_FADE_FRAMES_ALL     10
#define MVF_FADE_FRAMES_MINIMAL 5
#define MVF_FADE_MS 200

static void fade_to_black(void)
{
    static fb_data snapshot[A26_SCREEN_WIDTH * A26_SCREEN_HEIGHT];
    int frames, frame_delay, i, x, y;

    if (!lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE)
        return;

    for (y = 0; y < A26_SCREEN_HEIGHT; y++)
        memcpy(&snapshot[(size_t)y * A26_SCREEN_WIDTH], FBADDR(0, y),
               A26_SCREEN_WIDTH * sizeof(fb_data));

    if (aura_settings.animation_mode == AURA_ANIM_ALL)
    {
        frames = MVF_FADE_FRAMES_ALL;
        frame_delay = HZ * MVF_FADE_MS / 1000 / MVF_FADE_FRAMES_ALL;
    }
    else
    {
        frames = MVF_FADE_FRAMES_MINIMAL;
        frame_delay = HZ * MVF_FADE_MS / 1000 / MVF_FADE_FRAMES_MINIMAL;
    }
    if (frame_delay < 1)
        frame_delay = 1;

    for (i = 1; i <= frames; i++)
    {
        int alpha = 256 - (256 * i / frames); /* fraccion visible del cuadro original */

        for (y = 0; y < A26_SCREEN_HEIGHT; y++)
        {
            fb_data *row = FBADDR(0, y);
            const fb_data *src = &snapshot[(size_t)y * A26_SCREEN_WIDTH];

            for (x = 0; x < A26_SCREEN_WIDTH; x++)
                row[x] = a26_shell_blend(0, src[x], alpha);
        }
        lcd_update();
        if (button_queue_full())
            button_clear_queue();
        if (i < frames)
            sleep(frame_delay);
    }
}

/* Mismo mensaje/temporizacion que show_cant_open_video() de
 * aura_video.c -- no exportado desde ahi (static), copia local minima
 * en vez de tocar la visibilidad de ese archivo para un solo llamador
 * nuevo. Reusa AURA_STR_VIDEO_CANT_OPEN/_HINT (genericos, "video" ya
 * cubre pelicula/episodio, sin texto nuevo). */
static void wait_dismiss(int ticks)
{
    long deadline = current_tick + ticks;

    for (;;)
    {
        long remaining = deadline - current_tick;
        long b;

        if (remaining <= 0)
            return;
        b = button_get_w_tmo(remaining);
        if (b == BUTTON_NONE || !(b & BUTTON_REL))
            return;
    }
}

static void show_cant_open_movie(void)
{
    int w, h, y;

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_VIDEOS_MOVIEFLOW));

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_getstringsize((const unsigned char *)aura_str(AURA_STR_VIDEO_CANT_OPEN), &w, &h);
    y = A26_LAYOUT_STATUSBAR_HEIGHT
        + (A26_SCREEN_HEIGHT - A26_LAYOUT_STATUSBAR_HEIGHT) / 2 - h;
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y,
               (const unsigned char *)aura_str(AURA_STR_VIDEO_CANT_OPEN));

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(AURA_STR_VIDEO_CANT_OPEN_HINT), &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y + A26_TYPE_BODY + A26_SPACING_SM,
               (const unsigned char *)aura_str(AURA_STR_VIDEO_CANT_OPEN_HINT));

    lcd_update();
    wait_dismiss(HZ * 2);
}

static void enter_player(const char *video_filename)
{
    char path[MAX_PATH];
    int ret;

    if (!video_filename || !video_filename[0])
        return;

    fade_to_black();

    snprintf(path, sizeof(path), "%s/%s", VIDEOS_DIR, video_filename);
    plugin_set_silent_open_errors(true);
    ret = plugin_load(VIEWERS_DIR "/mpegplayer.rock", path);
    plugin_set_silent_open_errors(false);
    if (ret < 0)
        show_cant_open_movie();
}

/* -- Dibujo principal ---------------------------------------------------- */

typedef struct { int idx; int offset_x256; } mvf_carousel_entry_t;

void aura_movieflow_draw(aura_nav_t *nav, aura_screen_id_t screen)
{
    int pos_x256, center_idx, i, n, zoom_dist;
    mvf_carousel_entry_t carousel[2 * MVF_VISIBLE_RADIUS + 3];
    (void)nav;
    (void)screen;

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_VIDEOS_MOVIEFLOW));

    ensure_movieflow_entries();

    if (s_entry_count == 0)
    {
        draw_message(AURA_STR_EMPTY_LIST);
        return;
    }
    if (s_target_index >= s_entry_count)
        s_target_index = s_entry_count - 1;

    if (s_state == MVF_STATE_COVER_IN && flip_progress_256() >= 256)
        s_state = MVF_STATE_SHOW_EPISODES;
    else if (s_state == MVF_STATE_COVER_OUT && flip_progress_256() >= 256)
        s_state = MVF_STATE_IDLE;

    pos_x256 = anim_pos_x256();
    center_idx = (pos_x256 + (pos_x256 >= 0 ? 128 : -128)) / 256;

    if (pos_x256 == s_target_index * 256)
        zoom_trigger_settle();
    zoom_dist = zoom_distance();

    n = 0;
    for (i = -(MVF_VISIBLE_RADIUS + 1); i <= MVF_VISIBLE_RADIUS + 1; i++)
    {
        int idx = center_idx + i;
        if (idx < 0 || idx >= s_entry_count)
            continue;
        carousel[n].idx = idx;
        carousel[n].offset_x256 = idx * 256 - pos_x256;
        n++;
    }

    {
        int a, b;
        for (a = 1; a < n; a++)
        {
            mvf_carousel_entry_t key = carousel[a];
            int key_abs = key.offset_x256 < 0 ? -key.offset_x256 : key.offset_x256;
            b = a - 1;
            while (b >= 0)
            {
                int b_abs = carousel[b].offset_x256 < 0 ? -carousel[b].offset_x256 : carousel[b].offset_x256;
                if (b_abs >= key_abs)
                    break;
                carousel[b + 1] = carousel[b];
                b--;
            }
            carousel[b + 1] = key;
        }
    }

    for (i = 0; i < n; i++)
    {
        int idx = carousel[i].idx;
        int offset_x256 = carousel[i].offset_x256;
        mvf_slot_t *slot;

        if (s_state != MVF_STATE_IDLE && idx == s_target_index)
        {
            slot = get_slot_for(idx);
            if (slot->valid)
            {
                if (s_state == MVF_STATE_COVER_IN)
                    draw_slide_flip(slot, flip_progress_256());
                else if (s_state == MVF_STATE_COVER_OUT)
                    draw_slide_flip(slot, 256 - flip_progress_256());
                else
                    draw_episodelist_panel();
            }
            continue;
        }

        slot = get_slot_for(idx);
        draw_slide_perspective(slot, offset_x256, zoom_dist);
    }

    {
        int w, h;
        const char *title = s_entries[s_target_index].title;
        int text_dy = 0;
        int text_hidden = 0;

        if (s_state == MVF_STATE_COVER_IN)
            text_dy = 64 * flip_progress_256() / 256;
        else if (s_state == MVF_STATE_SHOW_EPISODES)
            text_hidden = 1;
        else if (s_state == MVF_STATE_COVER_OUT)
            text_dy = 64 * (256 - flip_progress_256()) / 256;

        if (!text_hidden)
        {
            lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
            lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
            lcd_getstringsize((const unsigned char *)title, &w, &h);
            lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, MVF_TITLE_Y + text_dy,
                       (const unsigned char *)title);
        }
    }
}

/* -- Entrada ------------------------------------------------------------- */

#define MVF_FAST_JUMP 10

static void jump_entries(int delta)
{
    int new_target = s_target_index + delta;

    if (new_target < 0) new_target = 0;
    if (new_target >= s_entry_count) new_target = s_entry_count > 0 ? s_entry_count - 1 : 0;
    if (new_target == s_target_index)
        return;

    s_anim_from_x256 = anim_pos_x256();
    s_target_index = new_target;
    s_anim_since = current_tick;
    zoom_trigger_scroll_start();
}

static void scroll_step(int dir)
{
    int step = aura_wheel_step((int)aura_main_wheel_velocity());
    int new_target = s_target_index + dir * step;

    if (new_target < 0) new_target = 0;
    if (new_target >= s_entry_count) new_target = s_entry_count > 0 ? s_entry_count - 1 : 0;

    if (new_target != s_target_index)
        zoom_trigger_scroll_start();
    s_anim_from_x256 = anim_pos_x256();
    s_target_index = new_target;
    s_anim_since = current_tick;
}

void aura_movieflow_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    (void)screen;

    if (s_state == MVF_STATE_COVER_IN || s_state == MVF_STATE_COVER_OUT)
        return;

    if (s_state == MVF_STATE_SHOW_EPISODES)
    {
        const mvf_entry_t *season = &s_entries[s_target_index];

        switch (button)
        {
        case BUTTON_SCROLL_FWD:
            if (season->episode_count > 0)
                s_ep_sel = (s_ep_sel + 1) % season->episode_count;
            break;
        case BUTTON_SCROLL_BACK:
            if (season->episode_count > 0)
                s_ep_sel = (s_ep_sel - 1 + season->episode_count) % season->episode_count;
            break;
        case BUTTON_SELECT:
            /* D-318: nunca Flip-and-Flow -- fundido a negro y
             * plugin_load() directo, ver enter_player(). */
            if (season->episode_count > 0)
                enter_player(season->episodes[s_ep_sel].filename);
            break;
        case BUTTON_MENU:
            s_state = MVF_STATE_COVER_OUT;
            s_state_since = current_tick;
            break;
        default:
            break;
        }
        return;
    }

    /* MVF_STATE_IDLE */
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        scroll_step(1);
        break;
    case BUTTON_SCROLL_BACK:
        scroll_step(-1);
        break;
    case BUTTON_SELECT:
        if (s_entry_count > 0 && !position_pending())
        {
            const mvf_entry_t *e = &s_entries[s_target_index];

            if (e->kind == MVF_ENTRY_MOVIE)
            {
                /* D-318: una PELICULA se reproduce al instante -- SIN
                 * el flip a lista (no hay nada que listar). */
                enter_player(e->video_filename);
            }
            else
            {
                s_ep_sel = 0;
                s_state = MVF_STATE_COVER_IN;
                s_state_since = current_tick;
            }
        }
        break;
    case BUTTON_LEFT:
        jump_entries(-MVF_FAST_JUMP);
        break;
    case BUTTON_RIGHT:
        jump_entries(MVF_FAST_JUMP);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
