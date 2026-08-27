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
/* Test host-side del formato maestro compartido (D-340/D-341, contrato v16). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../aura_master_art_format.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static void test_sizes(void)
{
    CHECK(aura_master_art_size_for(AURA_MASTER_ART_ALBUM) == 130);
    CHECK(aura_master_art_size_for(AURA_MASTER_ART_ARTIST) == 130);
    CHECK(aura_master_art_size_for(AURA_MASTER_ART_PHOTO) == 80);
}

static void test_names(void)
{
    char name[64];
    aura_master_art_key_t key = { 0x53cc14bcu, 1787505971u }, back;
    aura_master_art_kind_t kind;
    bool neg;

    CHECK(aura_master_art_name(name, sizeof(name), AURA_MASTER_ART_ALBUM, &key, false) > 0);
    CHECK(strcmp(name, "a-53cc14bc.1787505971.art") == 0);
    CHECK(aura_master_art_parse_name(name, &kind, &back, &neg));
    CHECK(kind == AURA_MASTER_ART_ALBUM && !neg);
    CHECK(back.path_crc == key.path_crc && back.mtime == key.mtime);

    CHECK(aura_master_art_name(name, sizeof(name), AURA_MASTER_ART_ARTIST, &key, true) > 0);
    CHECK(strcmp(name, "r-53cc14bc.1787505971.none") == 0);
    CHECK(aura_master_art_parse_name(name, &kind, &back, &neg));
    CHECK(kind == AURA_MASTER_ART_ARTIST && neg);

    key.path_crc = 0x0000000fu;
    CHECK(aura_master_art_name(name, sizeof(name), AURA_MASTER_ART_PHOTO, &key, false) > 0);
    CHECK(strcmp(name, "p-0000000f.1787505971.art") == 0);
    CHECK(aura_master_art_parse_name(name, &kind, &back, &neg));
    CHECK(kind == AURA_MASTER_ART_PHOTO && back.path_crc == 0xfu);

    /* Rechazos: separador '-' de cfcache, lado, prefijo ajeno, extension. */
    CHECK(!aura_master_art_parse_name("a-53cc14bc-1787505971-130.pfraw", NULL, NULL, NULL));
    CHECK(!aura_master_art_parse_name("a-53cc14bc-1787505971.none", NULL, NULL, NULL));
    CHECK(!aura_master_art_parse_name("x-53cc14bc.1787505971.art", NULL, NULL, NULL));
    CHECK(!aura_master_art_parse_name("a-53cc14b.1787505971.art", NULL, NULL, NULL));
    CHECK(!aura_master_art_parse_name("a-53cc14bc.1787505971.mth", NULL, NULL, NULL));
    CHECK(!aura_master_art_parse_name("a-53cc14bc..art", NULL, NULL, NULL));
    CHECK(!aura_master_art_parse_name("", NULL, NULL, NULL));
    CHECK(!aura_master_art_parse_name(NULL, NULL, NULL, NULL));
    CHECK(aura_master_art_name(name, 10, AURA_MASTER_ART_ALBUM, &key, false) < 0);
}

static void test_orphans(void)
{
    aura_master_art_key_t live[2] = { { 0x11111111u, 100 }, { 0x22222222u, 200 } };

    CHECK(!aura_master_art_is_orphan("a-11111111.100.art", AURA_MASTER_ART_ALBUM, live, 2));
    CHECK(!aura_master_art_is_orphan("a-22222222.200.none", AURA_MASTER_ART_ALBUM, live, 2));
    CHECK(aura_master_art_is_orphan("a-11111111.101.art", AURA_MASTER_ART_ALBUM, live, 2));
    CHECK(aura_master_art_is_orphan("a-33333333.100.none", AURA_MASTER_ART_ALBUM, live, 2));
    /* Otro tipo o nombre ajeno: no es de este GC. */
    CHECK(!aura_master_art_is_orphan("r-33333333.100.art", AURA_MASTER_ART_ALBUM, live, 2));
    CHECK(!aura_master_art_is_orphan("db_stamp.txt", AURA_MASTER_ART_ALBUM, live, 2));
    CHECK(aura_master_art_is_orphan("p-33333333.100.art", AURA_MASTER_ART_PHOTO, live, 0));
}

static void test_header(void)
{
    unsigned char hdr[AURA_MASTER_ART_HEADER_SIZE];
    unsigned w = 0, h = 0;
    static const unsigned char expect[16] = {
        0x4D, 0x41, 0x53, 0x54, /* 'M','A','S','T' */
        0x82, 0x00, 0x82, 0x00, /* 130, 130 */
        0, 0, 0, 0, 0, 0, 0, 0
    };

    aura_master_art_header_pack(hdr, 130, 130);
    CHECK(memcmp(hdr, expect, 16) == 0);
    CHECK(aura_master_art_header_parse(hdr, &w, &h));
    CHECK(w == 130 && h == 130);

    aura_master_art_header_pack(hdr, 80, 80);
    CHECK(hdr[4] == 80 && hdr[6] == 80);
    CHECK(aura_master_art_header_parse(hdr, &w, &h) && w == 80 && h == 80);

    hdr[0] = 'X';
    CHECK(!aura_master_art_header_parse(hdr, &w, &h));
    aura_master_art_header_pack(hdr, 80, 80);
    hdr[8] = 1; /* flags != 0 */
    CHECK(!aura_master_art_header_parse(hdr, &w, &h));
    aura_master_art_header_pack(hdr, 0, 80);
    CHECK(!aura_master_art_header_parse(hdr, &w, &h));
}

static uint16_t rgb565(unsigned r, unsigned g, unsigned b)
{
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void test_downscale(void)
{
    static uint16_t src[130 * 130];
    static uint16_t dst[130 * 130];
    int i, x, y;

    /* Identidad. */
    for (i = 0; i < 4 * 4; i++)
        src[i] = (uint16_t)(i * 37);
    aura_master_art_downscale_box(src, 4, dst, 4);
    CHECK(memcmp(src, dst, 16 * sizeof(uint16_t)) == 0);

    /* 4 -> 2: cada destino promedia un bloque 2x2 de valores conocidos. */
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++)
            src[y * 4 + x] = (x < 2) ? rgb565(31, 0, 0) : rgb565(0, 63, 0);
    aura_master_art_downscale_box(src, 4, dst, 2);
    CHECK(dst[0] == rgb565(31, 0, 0));
    CHECK(dst[1] == rgb565(0, 63, 0));
    CHECK(dst[2] == rgb565(31, 0, 0));
    CHECK(dst[3] == rgb565(0, 63, 0));

    /* 2 -> 1: promedio real (31+0)/2 -> 16 con redondeo, (0+63)/2 -> 32. */
    src[0] = rgb565(31, 0, 0); src[1] = rgb565(0, 63, 0);
    src[2] = rgb565(31, 0, 0); src[3] = rgb565(0, 63, 0);
    aura_master_art_downscale_box(src, 2, dst, 1);
    CHECK(dst[0] == rgb565(16, 32, 0));

    /* 130 -> 120 y 130 -> 48: un color uniforme sigue uniforme. */
    for (i = 0; i < 130 * 130; i++)
        src[i] = rgb565(12, 40, 7);
    aura_master_art_downscale_box(src, 130, dst, 120);
    for (i = 0; i < 120 * 120; i++)
        if (dst[i] != rgb565(12, 40, 7)) { CHECK(!"130->120 uniforme"); break; }
    aura_master_art_downscale_box(src, 130, dst, 48);
    for (i = 0; i < 48 * 48; i++)
        if (dst[i] != rgb565(12, 40, 7)) { CHECK(!"130->48 uniforme"); break; }

    /* 80 -> 48: gradiente horizontal se conserva monotono. */
    for (y = 0; y < 80; y++)
        for (x = 0; x < 80; x++)
            src[y * 80 + x] = rgb565((unsigned)(x * 31 / 79), 0, 0);
    aura_master_art_downscale_box(src, 80, dst, 48);
    for (x = 1; x < 48; x++)
        if (((dst[x] >> 11) & 0x1f) < ((dst[x - 1] >> 11) & 0x1f)) { CHECK(!"80->48 monotono"); break; }
    CHECK(((dst[0] >> 11) & 0x1f) == 0);
    CHECK(((dst[47] >> 11) & 0x1f) == 31);
}

static void test_crop_and_fill(void)
{
    static uint16_t src[6 * 4];
    static uint16_t dst[4 * 4];
    int x, y, bw = 0, bh = 0;

    /* 6x4 -> recorte centrado de 4x4: columnas 1..4. */
    for (y = 0; y < 4; y++)
        for (x = 0; x < 6; x++)
            src[y * 6 + x] = (uint16_t)(y * 10 + x);
    aura_master_art_center_crop(src, 6, 4, dst, 4);
    CHECK(dst[0] == 1 && dst[3] == 4);
    CHECK(dst[3 * 4 + 0] == 31 && dst[3 * 4 + 3] == 34);

    /* Caja del segundo decode: 130x98 (ancho lleno) -> alto a 130, ancho 173(+1). */
    CHECK(aura_master_art_fill_box(130, 98, 130, 32, &bw, &bh));
    CHECK(bh == 130 && bw == 173);
    /* Retrato 98x130 -> ancho a 130, alto 173(+1). */
    CHECK(aura_master_art_fill_box(98, 130, 130, 32, &bw, &bh));
    CHECK(bw == 130 && bh == 173);
    /* Ya cuadrada: nada que hacer. */
    CHECK(!aura_master_art_fill_box(130, 130, 130, 32, &bw, &bh));
    /* Panorama 4.5:1 con tope 4:1: se rechaza. */
    CHECK(!aura_master_art_fill_box(80, 17, 80, 32, &bw, &bh));
    CHECK(!aura_master_art_fill_box(0, 10, 80, 32, &bw, &bh));

    /* Centrado sobre tile: 2x1 sobre 4x4 con fondo 7. */
    src[0] = 1; src[1] = 2;
    aura_master_art_center_on_tile(src, 2, 1, dst, 4, 7);
    CHECK(dst[0] == 7 && dst[1 * 4 + 1] == 1 && dst[1 * 4 + 2] == 2 && dst[15] == 7);
}

int main(void)
{
    test_sizes();
    test_names();
    test_orphans();
    test_header();
    test_downscale();
    test_crop_and_fill();

    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("test_master_art: all tests passed\n");
    return 0;
}
