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
/* D-340/D-341: ver aura_master_art_format.h. */
#include <stdio.h>
#include <string.h>
#include "aura_master_art_format.h"

int aura_master_art_size_for(aura_master_art_kind_t kind)
{
    switch (kind)
    {
    case AURA_MASTER_ART_ALBUM:  return AURA_MASTER_ART_ALBUM_SIZE;
    case AURA_MASTER_ART_ARTIST: return AURA_MASTER_ART_ARTIST_SIZE;
    case AURA_MASTER_ART_PHOTO:  return AURA_MASTER_ART_PHOTO_SIZE;
    }
    return 0;
}

static bool kind_valid(int c)
{
    return c == AURA_MASTER_ART_ALBUM || c == AURA_MASTER_ART_ARTIST
        || c == AURA_MASTER_ART_PHOTO;
}

int aura_master_art_name(char *out, size_t outsz, aura_master_art_kind_t kind,
                         const aura_master_art_key_t *key, bool negative)
{
    int n;

    if (!kind_valid((int)kind) || key == NULL)
        return -1;
    n = snprintf(out, outsz, "%c-%08lx.%lu.%s", (char)kind,
                 (unsigned long)key->path_crc, (unsigned long)key->mtime,
                 negative ? "none" : "art");
    if (n < 0 || (size_t)n >= outsz)
        return -1;
    return n;
}

static bool parse_hex8(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    int i;

    for (i = 0; i < 8; i++)
    {
        char c = s[i];
        int d;

        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else
            return false;
        v = (v << 4) | (uint32_t)d;
    }
    *out = v;
    return true;
}

bool aura_master_art_parse_name(const char *name, aura_master_art_kind_t *kind,
                                aura_master_art_key_t *key, bool *negative)
{
    uint32_t crc;
    unsigned long mt = 0;
    const char *p;
    bool neg;

    if (name == NULL || !kind_valid((unsigned char)name[0]) || name[1] != '-')
        return false;
    p = name + 2;
    if (!parse_hex8(p, &crc) || p[8] != '.')
        return false;
    p += 9;
    if (*p < '0' || *p > '9')
        return false;
    while (*p >= '0' && *p <= '9')
    {
        mt = mt * 10 + (unsigned long)(*p - '0');
        p++;
    }
    if (strcmp(p, ".art") == 0)
        neg = false;
    else if (strcmp(p, ".none") == 0)
        neg = true;
    else
        return false;

    if (kind)     *kind = (aura_master_art_kind_t)name[0];
    if (key)      { key->path_crc = crc; key->mtime = (uint32_t)mt; }
    if (negative) *negative = neg;
    return true;
}

bool aura_master_art_is_orphan(const char *name, aura_master_art_kind_t kind,
                               const aura_master_art_key_t *keys, int count)
{
    aura_master_art_kind_t k;
    aura_master_art_key_t key;
    int i;

    if (!aura_master_art_parse_name(name, &k, &key, NULL) || k != kind)
        return false;
    for (i = 0; i < count; i++)
        if (keys[i].path_crc == key.path_crc && keys[i].mtime == key.mtime)
            return false;
    return true;
}

static void put_le32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static uint32_t get_le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void aura_master_art_header_pack(unsigned char hdr[AURA_MASTER_ART_HEADER_SIZE],
                                 unsigned width, unsigned height)
{
    put_le32(hdr, AURA_MASTER_ART_MAGIC);
    hdr[4] = (unsigned char)(width & 0xff);
    hdr[5] = (unsigned char)((width >> 8) & 0xff);
    hdr[6] = (unsigned char)(height & 0xff);
    hdr[7] = (unsigned char)((height >> 8) & 0xff);
    put_le32(hdr + 8, 0);  /* flags */
    put_le32(hdr + 12, 0); /* reservado */
}

bool aura_master_art_header_parse(const unsigned char hdr[AURA_MASTER_ART_HEADER_SIZE],
                                  unsigned *width, unsigned *height)
{
    unsigned w, h;

    if (get_le32(hdr) != AURA_MASTER_ART_MAGIC)
        return false;
    if (get_le32(hdr + 8) != 0 || get_le32(hdr + 12) != 0)
        return false;
    w = (unsigned)hdr[4] | ((unsigned)hdr[5] << 8);
    h = (unsigned)hdr[6] | ((unsigned)hdr[7] << 8);
    if (w == 0 || h == 0)
        return false;
    if (width)  *width = w;
    if (height) *height = h;
    return true;
}

void aura_master_art_downscale_box(const uint16_t *src, int src_size,
                                   uint16_t *dst, int dst_size)
{
    int dy, dx;

    if (dst_size <= 0 || src_size <= 0)
        return;
    if (dst_size == src_size)
    {
        memcpy(dst, src, (size_t)src_size * src_size * sizeof(uint16_t));
        return;
    }

    for (dy = 0; dy < dst_size; dy++)
    {
        int y0 = dy * src_size / dst_size;
        int y1 = (dy + 1) * src_size / dst_size;

        if (y1 <= y0)
            y1 = y0 + 1;
        if (y1 > src_size)
            y1 = src_size;

        for (dx = 0; dx < dst_size; dx++)
        {
            int x0 = dx * src_size / dst_size;
            int x1 = (dx + 1) * src_size / dst_size;
            unsigned r = 0, g = 0, b = 0, n = 0;
            int y, x;

            if (x1 <= x0)
                x1 = x0 + 1;
            if (x1 > src_size)
                x1 = src_size;

            for (y = y0; y < y1; y++)
            {
                const uint16_t *row = src + (size_t)y * src_size;
                for (x = x0; x < x1; x++)
                {
                    uint16_t px = row[x];
                    r += (px >> 11) & 0x1f;
                    g += (px >> 5) & 0x3f;
                    b += px & 0x1f;
                    n++;
                }
            }
            r = (r + n / 2) / n;
            g = (g + n / 2) / n;
            b = (b + n / 2) / n;
            dst[(size_t)dy * dst_size + dx] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }
}

void aura_master_art_center_crop(const uint16_t *src, int w, int h,
                                 uint16_t *dst, int size)
{
    int ox = (w - size) / 2;
    int oy = (h - size) / 2;
    int row;

    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    for (row = 0; row < size; row++)
        memcpy(dst + (size_t)row * size,
               src + (size_t)(oy + row) * w + ox,
               (size_t)size * sizeof(uint16_t));
}

bool aura_master_art_fill_box(int w, int h, int size, int max_ratio_x8,
                              int *box_w, int *box_h)
{
    long ratio_x8;

    if (w <= 0 || h <= 0 || size <= 0)
        return false;
    if (w >= size && h >= size)
        return false; /* ya llena (cuadrada) */
    if (w >= h)
    {
        /* Ancho lleno, alto corto: crecer hasta que el alto sea `size`. */
        ratio_x8 = (long)w * 8 / h;
        if (ratio_x8 > max_ratio_x8)
            return false;
        *box_h = size;
        *box_w = (int)(((long)w * size + h / 2) / h);
    }
    else
    {
        ratio_x8 = (long)h * 8 / w;
        if (ratio_x8 > max_ratio_x8)
            return false;
        *box_w = size;
        *box_h = (int)(((long)h * size + w / 2) / w);
    }
    /* Un pixel de holgura: el decodificador redondea hacia abajo al
     * ajustar dentro de la caja; con la caja un pixel mayor el lado
     * menor no queda en size-1. */
    if (*box_w > size) *box_w += 1;
    if (*box_h > size) *box_h += 1;
    return true;
}

void aura_master_art_center_on_tile(const uint16_t *src, int w, int h,
                                    uint16_t *dst, int size, uint16_t bg)
{
    int stride = w;
    int ox, oy, row, i;

    for (i = 0; i < size * size; i++)
        dst[i] = bg;
    if (w > size) w = size;
    if (h > size) h = size;
    ox = (size - w) / 2;
    oy = (size - h) / 2;
    for (row = 0; row < h; row++)
        memcpy(dst + (size_t)(oy + row) * size + ox,
               src + (size_t)row * stride, (size_t)w * sizeof(uint16_t));
}
