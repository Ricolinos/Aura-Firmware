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
/* D-340/D-341: ver aura_master_art.h. */
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "file.h"
#include "dir.h"
#include "lcd.h"
#include "debug.h"
#include "kernel.h"
#include "mutex.h"
#include "recorder/bmp.h"
#include "recorder/jpeg_load.h"
#include "crc32.h"
#include "string-extra.h"

#include "apple2026_tokens.h"
#include "aura_sync.h"
#include "aura_master_art.h"

/* Buffer de trabajo para decodificar+remuestrear (FORMAT_RESIZE
 * necesita bastante mas espacio que el bitmap final, ver BM_SCALED_SIZE
 * en recorder/bmp.h) -- dimensionado sobre el mayor consumidor real
 * (D-254: CoverDrift a AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE, mayor que
 * los 130 px de Music Flow; pedir 290 px con 64 KB fallaba en silencio),
 * margen x2 para el intermedio del decodificador (JPEG_DECODE_OVERHEAD).
 * Antes vivia en aura_albumart.c (s_decode_scratch); D-341 lo mueve
 * aqui porque ahora lo comparten el hilo de UI y el constructor en
 * segundo plano, bajo s_decode_mutex. La caja del segundo decode del
 * fill-crop (hasta 4:1 => 130 x 521 px) cabe con holgura. */
#define AURA_MASTER_ART_SCRATCH_SIZE \
    (AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE * AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE * 2 * 2)
static unsigned char s_scratch[AURA_MASTER_ART_SCRATCH_SIZE];

/* Proporcion maxima que vale la pena "llenar" (4:1, en octavos). */
#define AURA_MASTER_ART_MAX_FILL_RATIO_X8 32

static struct mutex s_decode_mutex;
static bool s_decode_mutex_ready = false;

/* Inicializacion perezosa: el planificador de Rockbox es cooperativo
 * (un hilo solo cede en yield/sleep/bloqueo), asi que no hay ventana
 * entre la comprobacion y la marca. */
static void ensure_mutex(void)
{
    if (!s_decode_mutex_ready)
    {
        mutex_init(&s_decode_mutex);
        s_decode_mutex_ready = true;
    }
}

void aura_master_art_decode_lock(void)
{
    ensure_mutex();
    mutex_lock(&s_decode_mutex);
}

void aura_master_art_decode_unlock(void)
{
    mutex_unlock(&s_decode_mutex);
}

unsigned char *aura_master_art_scratch(size_t *size)
{
    if (size)
        *size = sizeof(s_scratch);
    return s_scratch;
}

void aura_master_art_key_from_path(const char *path, uint32_t mtime,
                                   aura_master_art_key_t *key)
{
    key->path_crc = crc_32(path, strlen(path), 0xffffffff);
    key->mtime = mtime;
}

static const char *dir_for(aura_master_art_kind_t kind)
{
    switch (kind)
    {
    case AURA_MASTER_ART_ALBUM:  return AURA_SHARED_ART_ALBUMS_DIR;
    case AURA_MASTER_ART_ARTIST: return AURA_SHARED_ART_ARTISTS_DIR;
    case AURA_MASTER_ART_PHOTO:  return AURA_SHARED_ART_PHOTOS_DIR;
    }
    return AURA_SHARED_ART_DIR;
}

static void ensure_dirs(aura_master_art_kind_t kind)
{
    const char *sub = dir_for(kind);

    /* /.aura lo crea aura_sync.c al escribir el marcador; aqui se
     * repite el mkdir por si esta pasada llega antes (arranque limpio). */
    if (!dir_exists("/.aura"))
        mkdir("/.aura");
    if (!dir_exists(AURA_SHARED_ART_DIR))
        mkdir(AURA_SHARED_ART_DIR);
    if (!dir_exists(sub))
        mkdir(sub);
}

static bool build_path(aura_master_art_kind_t kind, const aura_master_art_key_t *key,
                       bool negative, char *out, size_t outsz)
{
    char name[48];

    if (aura_master_art_name(name, sizeof(name), kind, key, negative) < 0)
        return false;
    snprintf(out, outsz, "%s/%s", dir_for(kind), name);
    return true;
}

static size_t payload_bytes(aura_master_art_kind_t kind)
{
    size_t s = (size_t)aura_master_art_size_for(kind);
    return s * s * sizeof(fb_data);
}

/* Cabecera valida para el tipo Y archivo completo (un corte de energia
 * a mitad de escritura deja un archivo corto: se trata como ausente). */
static bool header_ok(int fd, aura_master_art_kind_t kind)
{
    unsigned char hdr[AURA_MASTER_ART_HEADER_SIZE];
    unsigned w = 0, h = 0;
    unsigned expect = (unsigned)aura_master_art_size_for(kind);

    if (read(fd, hdr, sizeof(hdr)) != (int)sizeof(hdr))
        return false;
    if (!aura_master_art_header_parse(hdr, &w, &h))
        return false;
    if (w != expect || h != expect)
        return false;
    return (size_t)filesize(fd) == AURA_MASTER_ART_HEADER_SIZE + payload_bytes(kind);
}

bool aura_master_art_present(aura_master_art_kind_t kind, const aura_master_art_key_t *key)
{
    char path[MAX_PATH];
    int fd;
    bool ok;

    if (!build_path(kind, key, false, path, sizeof(path)))
        return false;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    ok = header_ok(fd, kind);
    close(fd);
    return ok;
}

bool aura_master_art_none_present(aura_master_art_kind_t kind, const aura_master_art_key_t *key)
{
    char path[MAX_PATH];

    if (!build_path(kind, key, true, path, sizeof(path)))
        return false;
    return file_exists(path);
}

bool aura_master_art_resolved(aura_master_art_kind_t kind, const aura_master_art_key_t *key)
{
    return aura_master_art_present(kind, key) || aura_master_art_none_present(kind, key);
}

bool aura_master_art_read(aura_master_art_kind_t kind, const aura_master_art_key_t *key,
                          fb_data *out)
{
    char path[MAX_PATH];
    size_t bytes = payload_bytes(kind);
    int fd, n;

    if (!build_path(kind, key, false, path, sizeof(path)))
        return false;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    if (!header_ok(fd, kind))
    {
        close(fd);
        return false;
    }
    n = read(fd, out, bytes);
    close(fd);
    return n == (int)bytes;
}

bool aura_master_art_write(aura_master_art_kind_t kind, const aura_master_art_key_t *key,
                           const fb_data *flat)
{
    char path[MAX_PATH];
    unsigned char hdr[AURA_MASTER_ART_HEADER_SIZE];
    unsigned size = (unsigned)aura_master_art_size_for(kind);
    size_t bytes = payload_bytes(kind);
    int fd;
    bool ok;

    if (!build_path(kind, key, false, path, sizeof(path)))
        return false;
    ensure_dirs(kind);
    aura_master_art_header_pack(hdr, size, size);
    fd = creat(path, 0666);
    if (fd < 0)
        return false;
    ok = write(fd, hdr, sizeof(hdr)) == (int)sizeof(hdr)
      && write(fd, flat, bytes) == (int)bytes;
    close(fd);
    if (!ok)
    {
        remove(path); /* nunca dejar una maestra corta con nombre valido */
        return false;
    }
    aura_master_art_remove_none(kind, key);
    return true;
}

void aura_master_art_write_none(aura_master_art_kind_t kind, const aura_master_art_key_t *key)
{
    char path[MAX_PATH];
    int fd;

    if (!build_path(kind, key, true, path, sizeof(path)))
        return;
    ensure_dirs(kind);
    fd = creat(path, 0666);
    if (fd >= 0)
        close(fd);
}

void aura_master_art_remove_none(aura_master_art_kind_t kind, const aura_master_art_key_t *key)
{
    char path[MAX_PATH];

    if (!build_path(kind, key, true, path, sizeof(path)))
        return;
    if (file_exists(path))
        remove(path);
}

void aura_master_art_gc(aura_master_art_kind_t kind, const aura_master_art_key_t *keys, int count)
{
    const char *dir = dir_for(kind);
    DIR *d = opendir(dir);
    struct DIRENT *entry;
    int removed = 0;

    if (!d)
        return;
    while (removed < AURA_MASTER_ART_GC_BUDGET && (entry = readdir(d)) != NULL)
    {
        char path[MAX_PATH * 2]; /* mismo motivo que aura_fsutil.c */

        if (!aura_master_art_is_orphan(entry->d_name, kind, keys, count))
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (remove(path) >= 0)
            removed++;
    }
    closedir(d);
}

static bool has_bmp_ext(const char *path)
{
    size_t n = strlen(path);
    return n > 4 && !strcasecmp(path + n - 4, ".bmp");
}

static int decode_to(const char *path, int clip_off, unsigned long clip_len,
                     struct bitmap *bm)
{
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;

    bm->data = (char *)s_scratch;
#if (LCD_DEPTH > 1)
    bm->maskdata = NULL;
#endif
    if (clip_len > 0)
        return clip_jpeg_file(path, clip_off, clip_len, bm, sizeof(s_scratch), format, NULL);
    if (has_bmp_ext(path))
        return read_bmp_file(path, bm, sizeof(s_scratch), format, NULL);
    return read_jpeg_file(path, bm, sizeof(s_scratch), format, NULL);
}

/* Color promedio RGB565 de un bitmap fila-contigua -- fondo neutro (sin
 * tema) para el caso raro en que no se puede llenar el cuadrado. */
static fb_data average_color(const fb_data *px, int w, int h)
{
    unsigned long r = 0, g = 0, b = 0, n = (unsigned long)w * h;
    unsigned long i;

    if (n == 0)
        return 0;
    for (i = 0; i < n; i++)
    {
        r += (px[i] >> 11) & 0x1f;
        g += (px[i] >> 5) & 0x3f;
        b += px[i] & 0x1f;
    }
    return (fb_data)(((r / n) << 11) | ((g / n) << 5) | (b / n));
}

bool aura_master_art_decode_fill(const char *path, int clip_off, unsigned long clip_len,
                                 int size, fb_data *out_flat)
{
    bool ok;

    aura_master_art_decode_lock();
    ok = aura_master_art_decode_fill_locked(path, clip_off, clip_len, size, out_flat);
    aura_master_art_decode_unlock();
    return ok;
}

bool aura_master_art_decode_fill_locked(const char *path, int clip_off,
                                        unsigned long clip_len,
                                        int size, fb_data *out_flat)
{
    struct bitmap bm;
    int ret, w, h, box_w = 0, box_h = 0;
    bool ok = false;

    bm.width = size;
    bm.height = size;
    ret = decode_to(path, clip_off, clip_len, &bm);
    DEBUGF("aura_master_art: decode %s -> %d (%dx%d)\n", path, ret, bm.width, bm.height);
    if (ret <= 0)
        goto out;

    w = bm.width;
    h = bm.height;
    if (w >= size && h >= size)
    {
        aura_master_art_center_crop((const uint16_t *)bm.data, w, h,
                                    (uint16_t *)out_flat, size);
        ok = true;
        goto out;
    }

    if (aura_master_art_fill_box(w, h, size, AURA_MASTER_ART_MAX_FILL_RATIO_X8,
                                 &box_w, &box_h))
    {
        bm.width = box_w;
        bm.height = box_h;
        ret = decode_to(path, clip_off, clip_len, &bm);
        DEBUGF("aura_master_art: fill decode %s -> %d (%dx%d)\n", path, ret, bm.width, bm.height);
        if (ret > 0 && bm.width >= size && bm.height >= size)
        {
            aura_master_art_center_crop((const uint16_t *)bm.data, bm.width, bm.height,
                                        (uint16_t *)out_flat, size);
            ok = true;
            goto out;
        }
        /* Segundo decode fallido: volver a la version ajustada. */
        bm.width = size;
        bm.height = size;
        ret = decode_to(path, clip_off, clip_len, &bm);
        if (ret <= 0)
            goto out;
        w = bm.width;
        h = bm.height;
    }

    aura_master_art_center_on_tile((const uint16_t *)bm.data, w, h,
                                   (uint16_t *)out_flat, size,
                                   average_color((const fb_data *)bm.data, w, h));
    ok = true;

out:
    return ok;
}
