/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * mpegplayer video output routines
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
#include "libmpeg2/mpeg2dec_config.h"

#include "plugin.h"
#include "mpegplayer.h"
#include "mpeg_settings.h"

#define VO_NON_NULL_RECT 0x1
#define VO_VISIBLE       0x2

struct vo_data
{
    int image_width;
    int image_height;
    int image_chroma_x;
    int image_chroma_y;
    int display_width;
    int display_height;
    /* Aura (D-304): copia de sesion de settings.scale_mode -- SELECT la
     * alterna sin persistir (igual que s_cover_mode en aura_photos.c,
     * D-303: cada video vuelve a arrancar en el default guardado, ver
     * vo_setup()); el menu de ajustes SI la sincroniza con el default
     * persistido, via vo_update_scale_mode(). */
    int scale_mode;
    bool scale_mode_locked;
    int output_x;
    int output_y;
    int output_width;
    int output_height;
    unsigned flags;
    struct vo_rect rc_vid;
    struct vo_rect rc_clip;
    void (*post_draw_callback)(void);
};

#if NUM_CORES > 1
/* Cache aligned and padded to avoid clobbering other processors' cacheable
 * data */
static union {
	uint8_t __vo_data[CACHEALIGN_UP(sizeof(struct vo_data))];
	struct vo_data vo;
} vo_raw CACHEALIGN_ATTR;
#define vo vo_raw.vo
#else
static struct vo_data vo;
#endif

#if NUM_CORES > 1
static struct mutex vo_mtx SHAREDBSS_ATTR;
#endif

static inline void video_lock_init(void)
{
#if NUM_CORES > 1
    rb->mutex_init(&vo_mtx);
#endif
}

static inline void video_lock(void)
{
#if NUM_CORES > 1
    rb->mutex_lock(&vo_mtx);
#endif
}

static inline void video_unlock(void)
{
#if NUM_CORES > 1
    rb->mutex_unlock(&vo_mtx);
#endif
}


/* Draw a black rectangle if no video frame is available */
static void vo_draw_black(struct vo_rect *rc)
{
    int foreground;
    int x, y, w, h;

    video_lock();

    foreground = mylcd_get_foreground();

    mylcd_set_foreground(MYLCD_BLACK);

    if (rc)
    {
        x = rc->l;
        y = rc->t;
        w = rc->r - rc->l;
        h = rc->b - rc->t;
    }
    else
    {
#if LCD_WIDTH >= LCD_HEIGHT
        x = vo.output_x;
        y = vo.output_y;
        w = vo.output_width;
        h = vo.output_height;
#else
        x = LCD_WIDTH - vo.output_height - vo.output_y;
        y = vo.output_x;
        w = vo.output_height;
        h = vo.output_width;
#endif
    }

    mylcd_fillrect(x, y, w, h);
    mylcd_update_rect(x, y, w, h);

    mylcd_set_foreground(foreground);

    video_unlock();
}

static inline void yuv_blit(uint8_t * const * buf, int src_x, int src_y,
                            int stride, int x, int y, int width, int height)
{
    video_lock();

#ifdef HAVE_LCD_COLOR
    rb->lcd_blit_yuv(buf, src_x, src_y, stride, x, y , width, height);
#else
    grey_ub_gray_bitmap_part(buf[0], src_x, src_y, stride, x, y, width, height);
#endif

    video_unlock();
}

/* Forward decl: definida mas abajo, junto al resto del codigo de
 * escalado heredado de la miniatura del extinto selector de inicio. */
void stretch_image_plane(const uint8_t * src, uint8_t *dst, int stride,
                         int src_w, int src_h, int dst_w, int dst_h);

#ifdef HAVE_LCD_COLOR
/* Aura (D-304): modo "cubrir pantalla" -- recorta, dentro del frame ya
 * decodificado, el area con la relacion de aspecto exacta de la
 * pantalla (sin deformar) y la escala para llenarla por completo, sin
 * franjas. Mismo criterio que el modo "cubrir" del visor de fotos
 * (D-303). El escalado nearest-neighbor (stretch_image_plane(), ya
 * existente para la miniatura del extinto selector de inicio) corre una
 * vez por frame sobre la memoria "sobrante" del arena de libmpeg2
 * (mpeg2_get_buf()): investigacion previa (ver DECISIONS.md D-304)
 * confirmo que ese arena solo lo usa el propio hilo de video, en el
 * mismo orden secuencial decode->draw, sin necesidad de locking
 * adicional, y con margen tipico de ~450 KB de sobra sobre los 115200 B
 * que este buffer necesita a 320x240. Si no hay margen suficiente
 * (video de resolucion muy superior a la pantalla), se degrada al blit
 * directo sin cubrir -- igual que ya hace vo_draw_frame_thumb(). */
static bool vo_draw_frame_cover(uint8_t * const * buf)
{
    void *mem;
    size_t bufsize = 0;
    uint8_t *yuv[3];
    int crop_w, crop_h, crop_x, crop_y;
    const uint8_t *src_y, *src_u, *src_v;
    enum { UV_W = SCREEN_WIDTH / 2, UV_H = SCREEN_HEIGHT / 2 };

    if (vo.display_width * SCREEN_HEIGHT >= vo.display_height * SCREEN_WIDTH)
    {
        /* Fuente relativamente mas ancha que la pantalla: recorta los
         * costados, conserva el alto completo. */
        crop_h = vo.display_height;
        crop_w = muldiv_uint32(crop_h, SCREEN_WIDTH, SCREEN_HEIGHT);
        crop_w = MIN(crop_w, vo.display_width);
        crop_x = ((vo.display_width - crop_w) / 2) & ~1;
        crop_y = 0;
    }
    else
    {
        /* Fuente relativamente mas alta: recorta arriba/abajo. */
        crop_w = vo.display_width;
        crop_h = muldiv_uint32(crop_w, SCREEN_HEIGHT, SCREEN_WIDTH);
        crop_h = MIN(crop_h, vo.display_height);
        crop_y = ((vo.display_height - crop_h) / 2) & ~1;
        crop_x = 0;
    }

    mem = mpeg2_get_buf(&bufsize);

    if (mem == NULL ||
        bufsize < (size_t)(SCREEN_WIDTH*SCREEN_HEIGHT) + 2u*(UV_W*UV_H))
    {
        DEBUGF("vo cover: insufficient buffer\n");
        return false;
    }

    yuv[0] = mem;
    yuv[1] = yuv[0] + SCREEN_WIDTH*SCREEN_HEIGHT;
    yuv[2] = yuv[1] + UV_W*UV_H;

    src_y = buf[0] + crop_y*vo.image_width + crop_x;
    stretch_image_plane(src_y, yuv[0], vo.image_width,
                        crop_w, crop_h, SCREEN_WIDTH, SCREEN_HEIGHT);

    src_u = buf[1] + (crop_y/2)*(vo.image_width/2) + crop_x/2;
    src_v = buf[2] + (crop_y/2)*(vo.image_width/2) + crop_x/2;
    stretch_image_plane(src_u, yuv[1], vo.image_width/2,
                        crop_w/2, crop_h/2, UV_W, UV_H);
    stretch_image_plane(src_v, yuv[2], vo.image_width/2,
                        crop_w/2, crop_h/2, UV_W, UV_H);

    /* Aura (D-305): el buffer recien armado ya cubre la pantalla
     * completa (sus coordenadas (i,j) coinciden 1:1 con las de
     * pantalla) -- pero solo hay que blitear la parte que
     * vo_set_clip_rect() dejo visible. osd_show() ya recorta
     * vo.rc_clip para excluir la franja del OSD mientras esta
     * visible (stream_vo_set_clip(), mpegplayer.c); blitear siempre
     * 0,0..SCREEN_WIDTH,SCREEN_HEIGHT (como se hacia antes) ignoraba
     * ese recorte y tapaba la barra de progreso/iconos en cada
     * cuadro, un parpadeo constante -- el path normal (sin cubrir)
     * nunca tuvo este bug porque ya usaba vo.output_* desde el
     * origen. */
    yuv_blit(yuv, vo.output_x, vo.output_y, SCREEN_WIDTH,
             vo.output_x, vo.output_y, vo.output_width, vo.output_height);
    return true;
}
#endif /* HAVE_LCD_COLOR */

void vo_draw_frame(uint8_t * const * buf)
{
    if ((vo.flags & (VO_NON_NULL_RECT | VO_VISIBLE)) !=
        (VO_NON_NULL_RECT | VO_VISIBLE))
    {
        /* Frame is hidden - either by being set invisible or is clipped
         * away - copout */
        DEBUGF("vo hidden\n");
    }
    else if (buf == NULL)
    {
        /* No frame exists - draw black */
        vo_draw_black(NULL);
        DEBUGF("vo no frame\n");
    }
#ifdef HAVE_LCD_COLOR
    else if (vo.scale_mode == MPEG_SCALE_MODE_COVER
             && vo_draw_frame_cover(buf))
    {
        /* Ya blanqueado por vo_draw_frame_cover() */
    }
#endif
    else
    {
        yuv_blit(buf, 0, 0, vo.image_width,
                 vo.output_x, vo.output_y, vo.output_width,
                 vo.output_height);
    }

    if (vo.post_draw_callback)
        vo.post_draw_callback();
}

static inline void vo_rect_clear_inl(struct vo_rect *rc)
{
    rc->l = rc->t = rc->r = rc->b = 0;
}

static inline bool vo_rect_empty_inl(const struct vo_rect *rc)
{
    return rc == NULL || rc->l >= rc->r || rc->t >= rc->b;
}

static inline bool vo_rects_intersect_inl(const struct vo_rect *rc1,
                                          const struct vo_rect *rc2)
{
    return !vo_rect_empty_inl(rc1) &&
           !vo_rect_empty_inl(rc2) &&
           rc1->l < rc2->r && rc1->r > rc2->l &&
           rc1->t < rc2->b && rc1->b > rc2->t;
}

/* Sets all coordinates of a vo_rect to 0 */
void vo_rect_clear(struct vo_rect *rc)
{
    vo_rect_clear_inl(rc);
}

/* Returns true if left >= right or top >= bottom */
bool vo_rect_empty(const struct vo_rect *rc)
{
    return vo_rect_empty_inl(rc);
}

/* Initializes a vo_rect using upper-left corner and extents */
void vo_rect_set_ext(struct vo_rect *rc, int x, int y,
                     int width, int height)
{
    rc->l = x;
    rc->t = y;
    rc->r = x + width;
    rc->b = y + height;
}

/* Query if two rectangles intersect */
bool vo_rects_intersect(const struct vo_rect *rc1,
                        const struct vo_rect *rc2)
{
    return vo_rects_intersect_inl(rc1, rc2);
}

/* Intersect two rectangles, placing the result in rc_dst */
bool vo_rect_intersect(struct vo_rect *rc_dst,
                       const struct vo_rect *rc1,
                       const struct vo_rect *rc2)
{
    if (rc_dst != NULL)
    {
        if (vo_rects_intersect_inl(rc1, rc2))
        {
            rc_dst->l = MAX(rc1->l, rc2->l);
            rc_dst->r = MIN(rc1->r, rc2->r);
            rc_dst->t = MAX(rc1->t, rc2->t);
            rc_dst->b = MIN(rc1->b, rc2->b);
            return true;
        }

        vo_rect_clear_inl(rc_dst);
    }

    return false;
}

bool vo_rect_union(struct vo_rect *rc_dst,
                   const struct vo_rect *rc1,
                   const struct vo_rect *rc2)
{
    if (rc_dst != NULL)
    {
        if (!vo_rect_empty_inl(rc1))
        {
            if (!vo_rect_empty_inl(rc2))
            {
                rc_dst->l = MIN(rc1->l, rc2->l);
                rc_dst->t = MIN(rc1->t, rc2->t);
                rc_dst->r = MAX(rc1->r, rc2->r);
                rc_dst->b = MAX(rc1->b, rc2->b);
            }
            else
            {
                *rc_dst = *rc1;
            }

            return true;
        }
        else if (!vo_rect_empty_inl(rc2))
        {
            *rc_dst = *rc2;
            return true;
        }

        vo_rect_clear_inl(rc_dst);
    }

    return false;
}

void vo_rect_offset(struct vo_rect *rc, int dx, int dy)
{
    rc->l += dx;
    rc->t += dy;
    rc->r += dx;
    rc->b += dy;
}

/* Shink or stretch each axis - rotate counter-clockwise to retain upright
 * orientation on rotated displays (they rotate clockwise) */
void stretch_image_plane(const uint8_t * src, uint8_t *dst, int stride,
                         int src_w, int src_h, int dst_w, int dst_h)
{
    uint8_t *dst_end = dst + dst_w*dst_h;

#if LCD_WIDTH >= LCD_HEIGHT
    int src_w2 = src_w*2;        /* 2x dimensions (for rounding before division) */
    int dst_w2 = dst_w*2;
    int src_h2 = src_h*2;
    int dst_h2 = dst_h*2;
    int qw = src_w2 / dst_w2;    /* src-dst width ratio quotient */
    int rw = src_w2 - qw*dst_w2; /* src-dst width ratio remainder */
    int qh = src_h2 / dst_h2;    /* src-dst height ratio quotient */
    int rh = src_h2 - qh*dst_h2; /* src-dst height ratio remainder */
    int dw = dst_w;              /* Width error accumulator  */
    int dh = dst_h;              /* Height error accumulator */
#else
    int src_w2 = src_w*2;
    int dst_w2 = dst_h*2;
    int src_h2 = src_h*2;
    int dst_h2 = dst_w*2;
    int qw = src_h2 / dst_w2;
    int rw = src_h2 - qw*dst_w2;
    int qh = src_w2 / dst_h2;
    int rh = src_w2 - qh*dst_h2;
    int dw = dst_h;
    int dh = dst_w;

    src += src_w - 1;
#endif

    while (1)
    {
        const uint8_t *s = src;
#if LCD_WIDTH >= LCD_HEIGHT
        uint8_t * const dst_line_end = dst + dst_w;
#else
        uint8_t * const dst_line_end = dst + dst_h;
#endif
        while (1)
        {
            *dst++ = *s;

            if (dst >= dst_line_end)
            {
                dw = dst_w;
                break;
            }

#if LCD_WIDTH >= LCD_HEIGHT
            s += qw;
#else
            s += qw*stride;
#endif
            dw += rw;

            if (dw >= dst_w2)
            {
                dw -= dst_w2;
#if LCD_WIDTH >= LCD_HEIGHT
                s++;
#else
                s += stride;
#endif
            }
        }

        if (dst >= dst_end)
            break;
#if LCD_WIDTH >= LCD_HEIGHT
        src += qh*stride;
#else
        src -= qh;
#endif
        dh += rh;

        if (dh >= dst_h2)
        {
            dh -= dst_h2;
#if LCD_WIDTH >= LCD_HEIGHT
            src += stride;
#else
            src--;
#endif
        }
    }
}

bool vo_draw_frame_thumb(uint8_t * const * buf, const struct vo_rect *rc)
{
    void *mem;
    size_t bufsize = 0;
    uint8_t *yuv[3];
    struct vo_rect thumb_rc;
    int thumb_width, thumb_height;
#ifdef HAVE_LCD_COLOR
    int thumb_uv_width, thumb_uv_height;
#endif

    /* Obtain rectangle as clipped to the screen */
    vo_rect_set_ext(&thumb_rc, 0, 0, LCD_WIDTH, LCD_HEIGHT);
    if (!vo_rect_intersect(&thumb_rc, rc, &thumb_rc))
        return true;

    if (buf == NULL)
        goto no_thumb_exit;

    DEBUGF("thumb_rc: %d, %d, %d, %d\n", thumb_rc.l, thumb_rc.t,
           thumb_rc.r, thumb_rc.b);

    thumb_width = rc->r - rc->l;
    thumb_height = rc->b - rc->t;
#ifdef HAVE_LCD_COLOR
    thumb_uv_width = thumb_width / 2;
    thumb_uv_height = thumb_height / 2;

    DEBUGF("thumb: w: %d h: %d uvw: %d uvh: %d\n", thumb_width,
           thumb_height, thumb_uv_width, thumb_uv_height);
#else
    DEBUGF("thumb: w: %d h: %d\n", thumb_width, thumb_height);
#endif

    /* Use remaining mpeg2 buffer as temp space */
    mem = mpeg2_get_buf(&bufsize);

    if (bufsize < (size_t)(thumb_width*thumb_height)
#ifdef HAVE_LCD_COLOR
            + 2u*(thumb_uv_width * thumb_uv_height)
#endif
            )
    {
        DEBUGF("thumb: insufficient buffer\n");
        goto no_thumb_exit;
    }

    yuv[0] = mem;
    stretch_image_plane(buf[0], yuv[0], vo.image_width,
                        vo.display_width, vo.display_height,
                        thumb_width, thumb_height);

#ifdef HAVE_LCD_COLOR
    yuv[1] = yuv[0] + thumb_width*thumb_height;
    yuv[2] = yuv[1] + thumb_uv_width*thumb_uv_height;

    stretch_image_plane(buf[1], yuv[1], vo.image_width / 2,
                        vo.display_width / 2, vo.display_height / 2,
                        thumb_uv_width, thumb_uv_height);

    stretch_image_plane(buf[2], yuv[2], vo.image_width / 2,
                        vo.display_width / 2, vo.display_height / 2,
                        thumb_uv_width, thumb_uv_height);
#endif

#if LCD_WIDTH >= LCD_HEIGHT
    yuv_blit(yuv, 0, 0, thumb_width,
             thumb_rc.l, thumb_rc.t,
             thumb_rc.r - thumb_rc.l,
             thumb_rc.b - thumb_rc.t);
#else
    yuv_blit(yuv, 0, 0, thumb_height,
             thumb_rc.t, thumb_rc.l,
             thumb_rc.b - thumb_rc.t,
             thumb_rc.r - thumb_rc.l);
#endif /* LCD_WIDTH >= LCD_HEIGHT */

    return true;

no_thumb_exit:
    vo_draw_black(&thumb_rc);
    return false;
}

/* Aura (D-304): recalcula vo.rc_vid a partir de vo.display_width/height
 * (ya cacheados) y settings.scale_mode -- comun a vo_setup() (primera
 * vez, tras la cabecera de secuencia) y a vo_update_scale_mode()
 * (alternar en caliente con SELECT o desde el menu de ajustes, sin
 * esperar una nueva secuencia). */
static void vo_recalc_rect(void)
{
    if (vo.scale_mode == MPEG_SCALE_MODE_COVER)
    {
        /* El recorte+escalado real ocurre en vo_draw_frame_cover() justo
         * antes del blit -- el rectangulo de destino es siempre la
         * pantalla completa, nunca hay franjas que calcular aqui. */
        vo.rc_vid.l = 0;
        vo.rc_vid.t = 0;
        vo.rc_vid.r = SCREEN_WIDTH;
        vo.rc_vid.b = SCREEN_HEIGHT;
    }
    else
    {
        if (vo.display_width >= SCREEN_WIDTH)
        {
            vo.rc_vid.l = 0;
            vo.rc_vid.r = SCREEN_WIDTH;
        }
        else
        {
            vo.rc_vid.l = (SCREEN_WIDTH - vo.display_width) / 2;
#ifdef HAVE_LCD_COLOR
            vo.rc_vid.l &= ~1;
#endif
            vo.rc_vid.r = vo.rc_vid.l + vo.display_width;
        }

        if (vo.display_height >= SCREEN_HEIGHT)
        {
            vo.rc_vid.t = 0;
            vo.rc_vid.b = SCREEN_HEIGHT;
        }
        else
        {
            vo.rc_vid.t = (SCREEN_HEIGHT - vo.display_height) / 2;
#ifdef HAVE_LCD_COLOR
            vo.rc_vid.t &= ~1;
#endif
            vo.rc_vid.b = vo.rc_vid.t + vo.display_height;
        }
    }

    vo_set_clip_rect(&vo.rc_clip);
}

void vo_setup(const mpeg2_sequence_t * sequence)
{
    vo.image_width = sequence->width;
    vo.image_height = sequence->height;
    vo.display_width = sequence->display_width;
    vo.display_height = sequence->display_height;

    DEBUGF("vo_setup - w:%d h:%d\n", vo.display_width, vo.display_height);

    vo.image_chroma_x = vo.image_width / sequence->chroma_width;
    vo.image_chroma_y = vo.image_height / sequence->chroma_height;

    /* Aura (D-308): un codificador MPEG-2 con GOP corto (mpeg2video de
     * ffmpeg con -g 15, ver FFmpegTranscoder.swift) repite la cabecera
     * de secuencia varias veces por segundo durante la reproduccion
     * NORMAL, no solo al abrir el archivo -- video_thread.c llama a
     * vo_setup() cada vez que libmpeg2 la vuelve a parsear. Sin este
     * guard, cada repeticion pisaba vo.scale_mode con el default
     * persistido, revirtiendo el toggle de SELECT casi al instante
     * (bug real, encontrado investigando por que "cubrir" no se veia
     * aplicar). Solo la primera vez (justo despues de vo_init(), que
     * limpia scale_mode_locked) se adopta settings.scale_mode -- de ahi
     * en mas la sesion de reproduccion conserva lo que el usuario haya
     * elegido, mismo criterio que s_cover_mode en el visor de fotos
     * (D-303: persiste mientras se navega dentro de una misma sesion). */
    if (!vo.scale_mode_locked)
    {
        vo.scale_mode = settings.scale_mode;
        vo.scale_mode_locked = true;
    }

    vo_recalc_rect();
}

void vo_update_scale_mode(void)
{
    vo.scale_mode = settings.scale_mode;
    vo_recalc_rect();
}

void vo_toggle_scale_mode(void)
{
    vo.scale_mode = (vo.scale_mode == MPEG_SCALE_MODE_COVER)
                     ? MPEG_SCALE_MODE_FIT : MPEG_SCALE_MODE_COVER;
    vo_recalc_rect();
}

void vo_dimensions(struct vo_ext *sz)
{
    sz->w = vo.display_width;
    sz->h = vo.display_height;
}

bool vo_init(void)
{
    vo.flags = 0;
    vo.scale_mode_locked = false;
    vo_rect_set_ext(&vo.rc_clip, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    video_lock_init();
    return true;
}

bool vo_show(bool show)
{
    bool vis = vo.flags & VO_VISIBLE;

    if (show)
        vo.flags |= VO_VISIBLE;
    else
        vo.flags &= ~VO_VISIBLE;

    return vis;
}

bool vo_is_visible(void)
{
    return vo.flags & VO_VISIBLE;
}

void vo_cleanup(void)
{
    vo.flags = 0;
}

void vo_set_clip_rect(const struct vo_rect *rc)
{
    struct vo_rect rc_out;

    if (rc == NULL)
        vo_rect_set_ext(&vo.rc_clip, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    else
        vo.rc_clip = *rc;

    if (!vo_rect_intersect(&rc_out, &vo.rc_vid, &vo.rc_clip))
        vo.flags &= ~VO_NON_NULL_RECT;
    else
        vo.flags |= VO_NON_NULL_RECT;

    vo.output_x = rc_out.l;
    vo.output_y = rc_out.t;
    vo.output_width = rc_out.r - rc_out.l;
    vo.output_height = rc_out.b - rc_out.t;
}

bool vo_get_clip_rect(struct vo_rect *rc)
{
    rc->l = vo.output_x;
    rc->t = vo.output_y;
    rc->r = rc->l + vo.output_width;
    rc->b = rc->t + vo.output_height;
    return (vo.flags & VO_NON_NULL_RECT) != 0;
}

void vo_set_post_draw_callback(void (*cb)(void))
{
    vo.post_draw_callback = cb;
}

#if NUM_CORES > 1
void vo_lock(void)
{
    video_lock();
}

void vo_unlock(void)
{
    video_unlock();
}
#endif
