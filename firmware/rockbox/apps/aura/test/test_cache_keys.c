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
/* Test host-side de la logica pura de claves de cache (D-337/D-338). */
#include <stdio.h>
#include <string.h>
#include "../aura_cache_keys.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static void test_tagcache_files(void)
{
    CHECK(aura_cache_keys_is_tagcache_file("database_idx.tcd"));
    CHECK(aura_cache_keys_is_tagcache_file("database_0.tcd"));
    CHECK(aura_cache_keys_is_tagcache_file("database_12.tcd"));
    CHECK(aura_cache_keys_is_tagcache_file("database_tmp.tcd"));
    CHECK(!aura_cache_keys_is_tagcache_file("database.ignore"));
    CHECK(!aura_cache_keys_is_tagcache_file("database_commit.ignore"));
    CHECK(!aura_cache_keys_is_tagcache_file("database_changelog.txt"));
    CHECK(!aura_cache_keys_is_tagcache_file("database_.tcd"));
    CHECK(!aura_cache_keys_is_tagcache_file("rockbox.ipod"));
    CHECK(!aura_cache_keys_is_tagcache_file(""));
    CHECK(!aura_cache_keys_is_tagcache_file(NULL));
}

static void test_stamp(void)
{
    CHECK(aura_cache_keys_stamp_needs_rebuild(NULL, "st-1"));
    CHECK(aura_cache_keys_stamp_needs_rebuild("", "st-1"));
    CHECK(aura_cache_keys_stamp_needs_rebuild("st-0", "st-1"));
    CHECK(!aura_cache_keys_stamp_needs_rebuild("st-1", "st-1"));
    CHECK(aura_cache_keys_stamp_needs_rebuild("st-1", NULL));
}

static void test_album_key_roundtrip(void)
{
    char name[64];
    uint32_t crc = 0, mt = 0;
    int size = 0;

    CHECK(aura_cache_keys_album_name(name, sizeof(name), 0xdeadbeefu, 1234567u, 130) > 0);
    CHECK(strcmp(name, "a-deadbeef-1234567-130.pfraw") == 0);
    CHECK(aura_cache_keys_album_parse(name, &crc, &mt, &size));
    CHECK(crc == 0xdeadbeefu);
    CHECK(mt == 1234567u);
    CHECK(size == 130);

    /* crc con ceros a la izquierda y mtime 0 */
    CHECK(aura_cache_keys_album_name(name, sizeof(name), 0x1u, 0u, 320) > 0);
    CHECK(strcmp(name, "a-00000001-0-320.pfraw") == 0);
    CHECK(aura_cache_keys_album_parse(name, &crc, &mt, &size));
    CHECK(crc == 1u && mt == 0u && size == 320);

    /* salida chica */
    CHECK(aura_cache_keys_album_name(name, 8, 0x1u, 0u, 130) < 0);
    /* punteros opcionales */
    CHECK(aura_cache_keys_album_parse("a-00000001-0-130.pfraw", NULL, NULL, NULL));
}

static void test_album_key_rejects_other_entries(void)
{
    CHECK(!aura_cache_keys_album_parse("116-130.pfraw", NULL, NULL, NULL)); /* pre-D-338 */
    CHECK(!aura_cache_keys_album_parse("pl-Favoritas-130.pfraw", NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse("ar-0badcafe-130.pfraw", NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse("a-DEADBEEF-1-130.pfraw", NULL, NULL, NULL)); /* solo minusculas */
    CHECK(!aura_cache_keys_album_parse("a-deadbee-1-130.pfraw", NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse("a-deadbeef-1-130.pfraw.tmp", NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse("a-deadbeef--130.pfraw", NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse("a-deadbeef-1-0.pfraw", NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse("", NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse(NULL, NULL, NULL, NULL));
}

int main(void)
{
    test_tagcache_files();
    test_stamp();
    test_album_key_roundtrip();
    test_album_key_rejects_other_entries();
    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}
