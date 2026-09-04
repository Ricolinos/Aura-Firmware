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
/* Test host-side de la logica pura de claves de cache (D-337/D-338/D-339). */
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

/* D-339: marcador negativo a-<crc>-<mtime>.none */
static void test_none_marker(void)
{
    char name[64];
    uint32_t crc = 0, mt = 0;
    int size = -1;
    bool neg = false;

    CHECK(aura_cache_keys_album_none_name(name, sizeof(name), 0xdeadbeefu, 1234567u) > 0);
    CHECK(strcmp(name, "a-deadbeef-1234567.none") == 0);
    CHECK(aura_cache_keys_album_none_name(name, 8, 0x1u, 0u) < 0);

    /* parse_any reconoce ambos y distingue */
    CHECK(aura_cache_keys_album_parse_any("a-deadbeef-1234567.none", &crc, &mt, &size, &neg));
    CHECK(crc == 0xdeadbeefu && mt == 1234567u && size == 0 && neg);
    CHECK(aura_cache_keys_album_parse_any("a-deadbeef-1234567-130.pfraw", &crc, &mt, &size, &neg));
    CHECK(crc == 0xdeadbeefu && mt == 1234567u && size == 130 && !neg);
    CHECK(aura_cache_keys_album_parse_any("a-deadbeef-1234567.none", NULL, NULL, NULL, NULL));

    /* el parse de .pfraw NO acepta .none (el tamano no existe ahi) */
    CHECK(!aura_cache_keys_album_parse("a-deadbeef-1234567.none", NULL, NULL, NULL));
    /* pero el filtro de "sobrevive a la reconstruccion" conserva ambos */
    CHECK(aura_cache_keys_album_parse_name("a-deadbeef-1234567.none"));
    CHECK(aura_cache_keys_album_parse_name("a-deadbeef-1234567-130.pfraw"));
    CHECK(!aura_cache_keys_album_parse_name("pl-Favoritas-130.pfraw"));

    /* formas rotas */
    CHECK(!aura_cache_keys_album_parse_any("a-deadbeef-1234567-130.none", NULL, NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse_any("a-deadbeef-.none", NULL, NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse_any("a-deadbeef-1.none.tmp", NULL, NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse_any("a-DEADBEEF-1.none", NULL, NULL, NULL, NULL));
    CHECK(!aura_cache_keys_album_parse_any("116-130.none", NULL, NULL, NULL, NULL));
}

/* D-339: "resuelto" = .pfraw valido o .none presente */
static void test_resolved(void)
{
    CHECK(!aura_cache_keys_album_resolved(false, false));
    CHECK(aura_cache_keys_album_resolved(true, false));
    CHECK(aura_cache_keys_album_resolved(false, true));
    CHECK(aura_cache_keys_album_resolved(true, true));
}

/* D-338/D-339: GC de huerfanas sobre .pfraw, .none y los nombres viejos */
static void test_gc_orphans(void)
{
    const aura_cache_album_key_t keys[] = {
        { 0xdeadbeefu, 1234567u },
        { 0x00000001u, 0u },
    };
    const int n = 2;

    /* vigentes: ni .pfraw ni .none son huerfanos */
    CHECK(!aura_cache_keys_album_is_orphan("a-deadbeef-1234567-130.pfraw", keys, n));
    CHECK(!aura_cache_keys_album_is_orphan("a-deadbeef-1234567-320.pfraw", keys, n));
    CHECK(!aura_cache_keys_album_is_orphan("a-deadbeef-1234567.none", keys, n));
    CHECK(!aura_cache_keys_album_is_orphan("a-00000001-0.none", keys, n));

    /* misma pista, mtime distinto (pista reescrita por un sync): huerfano */
    CHECK(aura_cache_keys_album_is_orphan("a-deadbeef-1234568.none", keys, n));
    CHECK(aura_cache_keys_album_is_orphan("a-deadbeef-1234568-130.pfraw", keys, n));
    /* album que ya no existe */
    CHECK(aura_cache_keys_album_is_orphan("a-0badcafe-77.none", keys, n));
    CHECK(aura_cache_keys_album_is_orphan("a-0badcafe-77-130.pfraw", keys, n));
    /* <seek>-<lado>.pfraw de antes de D-338 */
    CHECK(aura_cache_keys_album_is_orphan("116-130.pfraw", keys, n));
    /* no son de este GC */
    CHECK(!aura_cache_keys_album_is_orphan("pl-Favoritas-130.pfraw", keys, n));
    CHECK(!aura_cache_keys_album_is_orphan("ar-0badcafe-130.pfraw", keys, n));
    CHECK(!aura_cache_keys_album_is_orphan("116-130.none", keys, n));
    CHECK(!aura_cache_keys_album_is_orphan("", keys, n));
    CHECK(!aura_cache_keys_album_is_orphan(NULL, keys, n));
    /* conjunto vacio: todo lo de album es huerfano */
    CHECK(aura_cache_keys_album_is_orphan("a-deadbeef-1234567.none", keys, 0));
}

/* D-350 (contrato v18): mtime de la clave de album y hermano cover.jpg. */
static void test_album_mtime_v18(void)
{
    char buf[64];

    /* Sin cover.jpg hermano: manda la pista, tal cual antes de v18. */
    CHECK(aura_cache_keys_album_mtime(1000, false, 0) == 1000);
    CHECK(aura_cache_keys_album_mtime(1000, false, 9999) == 1000);

    /* El caso que cierra la hipotesis (a): caratula REESCRITA sin tocar
     * la pista -- el mtime de la clave sube y la maestra vieja muere. */
    CHECK(aura_cache_keys_album_mtime(1000, true, 2000) == 2000);

    /* Pista mas nueva que la caratula: manda la pista. */
    CHECK(aura_cache_keys_album_mtime(3000, true, 2000) == 3000);
    /* Iguales: cualquiera. */
    CHECK(aura_cache_keys_album_mtime(1234, true, 1234) == 1234);
    /* Un cover.jpg con mtime 0 (FAT sin fecha) no debe TIRAR la clave. */
    CHECK(aura_cache_keys_album_mtime(1000, true, 0) == 1000);

    /* Ruta hermana */
    CHECK(aura_cache_keys_sibling_cover("/Music/Artista/Album/01.mp3",
                                        buf, sizeof(buf)));
    CHECK(!strcmp(buf, "/Music/Artista/Album/cover.jpg"));
    CHECK(aura_cache_keys_sibling_cover("/a.mp3", buf, sizeof(buf)));
    CHECK(!strcmp(buf, "/cover.jpg"));
    /* Sin directorio: no hay hermano posible. */
    CHECK(!aura_cache_keys_sibling_cover("solo.mp3", buf, sizeof(buf)));
    CHECK(!aura_cache_keys_sibling_cover(NULL, buf, sizeof(buf)));
    /* No cabe: se rechaza en vez de truncar a una ruta que no existe. */
    CHECK(!aura_cache_keys_sibling_cover("/Music/Artista/Album/01.mp3", buf, 20));
    CHECK(!aura_cache_keys_sibling_cover("/x.mp3", buf, 0));
}

int main(void)
{
    test_album_mtime_v18();
    test_tagcache_files();
    test_stamp();
    test_album_key_roundtrip();
    test_album_key_rejects_other_entries();
    test_none_marker();
    test_resolved();
    test_gc_orphans();
    if (failures)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all ok\n");
    return 0;
}
