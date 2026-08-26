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
/* Test host-side de la tabla pura de familias hermanas (D-333). */
#include <stdio.h>
#include <string.h>
#include "../aura_firmware_families.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
} while (0)

static void test_count(void)
{
    CHECK(aura_firmware_sibling_count() == 2);
}

static void test_dirs(void)
{
    int i, j, n = aura_firmware_sibling_count();
    for (i = 0; i < n; i++)
    {
        const char *dir = aura_firmware_sibling_dormant_dir(i);
        CHECK(dir != NULL);
        /* nunca el arbol dormido propio (Aura no puede "cambiar a Aura") */
        CHECK(strcmp(dir, AURA_FIRMWARE_OWN_DORMANT) != 0);
        /* contrato v14 §A bis: /.firmware-<familia> */
        CHECK(strncmp(dir, "/.firmware-", strlen("/.firmware-")) == 0);
        CHECK(strlen(dir) > strlen("/.firmware-"));
        for (j = 0; j < i; j++)
            CHECK(strcmp(dir, aura_firmware_sibling_dormant_dir(j)) != 0);
    }
    CHECK(strcmp(aura_firmware_sibling_dormant_dir(0), "/.firmware-metro") == 0);
    CHECK(strcmp(aura_firmware_sibling_dormant_dir(1), "/.firmware-moonlit") == 0);
}

static void test_names(void)
{
    int i, j, n = aura_firmware_sibling_count();
    for (i = 0; i < n; i++)
    {
        aura_str_id_t id = aura_firmware_sibling_name(i);
        CHECK(id >= 0 && id < AURA_STR_COUNT);
        for (j = 0; j < i; j++)
            CHECK(id != aura_firmware_sibling_name(j));
    }
    CHECK(aura_firmware_sibling_name(0) == AURA_STR_FAMILY_METRO);
    CHECK(aura_firmware_sibling_name(1) == AURA_STR_FAMILY_MOONLIT);
}

static void test_out_of_range(void)
{
    int n = aura_firmware_sibling_count();
    CHECK(aura_firmware_sibling_dormant_dir(-1) == NULL);
    CHECK(aura_firmware_sibling_dormant_dir(n) == NULL);
    CHECK(aura_firmware_sibling_dormant_dir(1000) == NULL);
    CHECK(aura_firmware_sibling_name(-1) == AURA_STR_COUNT);
    CHECK(aura_firmware_sibling_name(n) == AURA_STR_COUNT);
}

int main(void)
{
    test_count();
    test_dirs();
    test_names();
    test_out_of_range();
    if (failures)
    {
        printf("test_firmware_families: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_firmware_families: OK\n");
    return 0;
}
