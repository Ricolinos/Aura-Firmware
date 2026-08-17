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
#include "file.h"

#include "aura_art.h"
#include "aura_settings.h"
#include "apple2026_shell.h"

int aura_art_reflection_height(int size, int height_pct)
{
    return size * height_pct / 100;
}

void aura_art_generate_reflection(const fb_data *cover, fb_data *out,
                                   int size, int height_pct, unsigned bg_color,
                                   bool transposed)
{
    int refl_h = aura_art_reflection_height(size, height_pct);
    int bg_r = RGB_UNPACK_RED(bg_color);
    int bg_g = RGB_UNPACK_GREEN(bg_color);
    int bg_b = RGB_UNPACK_BLUE(bg_color);
    int y, x;

    /* `transposed` (PLAN.md T3.2(a), componentes/cover-flow.md: cache
     * .pfraw guarda las caratulas TRANSPUESTAS -- columna contigua en
     * memoria, igual que pictureflow.c, regla dura 7) -- el espejo
     * vertical + atenuacion es una operacion por indices LOGICOS
     * (fila/columna), el layout de memoria solo cambia COMO se
     * calculan los offsets, no la formula. Nunca se transpone un
     * bitmap ya generado: se genera directo en el layout que hace
     * falta. */
    for (y = 0; y < refl_h; y++)
    {
        /* Fila y del reflejo = fila (size-1-y) de la caratula, invertida
         * verticalmente, difuminada hacia el fondo segun se aleja del
         * borde. */
        int source_row = size - 1 - y;
        /* Pico de opacidad del reflejo: 45% (correccion del dueno del
         * diseno 2026-08-12, "el reflejo es muy visible, debe tener una
         * opacidad menor, del 40-50%") -- antes arrancaba en espejo
         * nitido (100%) y solo se desvanecia hacia abajo. */
        int peak = 255 * AURA_ART_REFLECTION_PEAK_PCT / 100;
        int fade = peak - (peak * y) / refl_h;

        for (x = 0; x < size; x++)
        {
            unsigned px = transposed ? cover[(size_t)x * size + source_row]
                                      : cover[(size_t)source_row * size + x];
            int r = RGB_UNPACK_RED(px);
            int g = RGB_UNPACK_GREEN(px);
            int b = RGB_UNPACK_BLUE(px);
            fb_data result;

            r = bg_r + ((r - bg_r) * fade) / 255;
            g = bg_g + ((g - bg_g) * fade) / 255;
            b = bg_b + ((b - bg_b) * fade) / 255;
            result = LCD_RGBPACK(r, g, b);

            if (transposed)
                out[(size_t)x * refl_h + y] = result;
            else
                out[(size_t)y * size + x] = result;
        }
    }
}

/* D-291: header en disco del cache .pfraw -- extraido de
 * aura_albumart.c (PLAN.md T3.2(a)), ver aura_art.h. `extra` es la
 * unica diferencia frente al formato original (que solo tenia
 * size/radius/theme). */
struct aura_art_pfraw_header {
    int32_t size;
    int32_t radius;
    int32_t theme;
    int32_t extra;
};

static bool pfraw_header_valid(int fd, int size, int radius, int32_t extra)
{
    struct aura_art_pfraw_header hdr;
    int n = read(fd, &hdr, sizeof(hdr));

    return n == (int)sizeof(hdr) && hdr.size == size && hdr.radius == radius
           && hdr.theme == (int32_t)aura_settings.theme && hdr.extra == extra;
}

bool aura_art_read_pfraw(const char *path, int size, int radius, int32_t extra, fb_data *out)
{
    size_t px_bytes = (size_t)size * size * sizeof(fb_data);
    int fd, n;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;

    if (!pfraw_header_valid(fd, size, radius, extra))
    {
        close(fd);
        return false;
    }

    n = read(fd, out, px_bytes);
    close(fd);
    return n == (int)px_bytes;
}

bool aura_art_pfraw_is_cached(const char *path, int size, int radius, int32_t extra)
{
    int fd;
    bool ok;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;

    ok = pfraw_header_valid(fd, size, radius, extra);
    close(fd);
    return ok;
}

void aura_art_write_pfraw(const char *path, int size, int radius, int32_t extra,
                           const fb_data *data)
{
    struct aura_art_pfraw_header hdr;
    int fd;

    hdr.size = size;
    hdr.radius = radius;
    hdr.theme = (int32_t)aura_settings.theme;
    hdr.extra = extra;

    fd = creat(path, 0666);
    if (fd < 0)
        return;

    write(fd, &hdr, sizeof(hdr));
    write(fd, data, (size_t)size * size * sizeof(fb_data));
    close(fd);
}

void aura_art_transpose(const fb_data *src, fb_data *dst, int size)
{
    int row, col;

    for (row = 0; row < size; row++)
        for (col = 0; col < size; col++)
            dst[(size_t)col * size + row] = src[(size_t)row * size + col];
}

void aura_art_mask_corners_transposed(fb_data *buf, int size, int radius, unsigned bg)
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
