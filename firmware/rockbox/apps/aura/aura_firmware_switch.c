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
#include "aura_firmware_switch.h"

#include <stdbool.h>
#include <string.h>

#include "file.h"
#include "dir.h"
#include "rbpaths.h"
#include "system.h"
#include "settings.h"
#include "ata_idle_notify.h"
#include "tagcache.h"

#include "aura_settings.h"
#include "aura_sync.h"

#define FW_ACTIVE_DIR    ROCKBOX_DIR        /* "/.rockbox" */
#define FW_DORMANT_AURA  "/.firmware-aura"
#define FW_DORMANT_METRO "/.firmware-metro"
#define FW_ROOT_BINARY   "/rockbox.ipod"
#define FW_TREE_BINARY   ROCKBOX_DIR "/rockbox.ipod"

bool aura_firmware_metro_installed(void)
{
    return dir_exists(FW_DORMANT_METRO);
}

/* /rockbox.ipod := /.rockbox/rockbox.ipod, a trozos, con buffer estatico
 * (D-226: nunca en la pila de 8 KB). No es fatal si falla: el bootloader
 * prefiere el del arbol; este es solo el respaldo. */
static void refresh_root_binary(void)
{
    static char buf[16 * 1024];
    int in, out;
    ssize_t n;

    in = open(FW_TREE_BINARY, O_RDONLY);
    if (in < 0)
        return;
    out = creat(FW_ROOT_BINARY, 0666);
    if (out < 0)
    {
        close(in);
        return;
    }
    while ((n = read(in, buf, sizeof(buf))) > 0)
        if (write(out, buf, (size_t)n) != n)
            break;
    close(out);
    close(in);
}

bool aura_firmware_switch_to_metro(void)
{
    if (!aura_firmware_metro_installed())
        return false;
    if (dir_exists(FW_DORMANT_AURA))
        return false; /* no adivinar: Studio garantiza que no pase */

    /* 1. todo lo de Aura al disco, AHORA */
    aura_settings_save();
    settings_save();
    tagcache_shutdown();
    call_storage_idle_notifys(true);

    /* 2. saliente primero */
    if (rename(FW_ACTIVE_DIR, FW_DORMANT_AURA) < 0)
        return false;

    /* 3. entrante; si falla, seguimos siendo Aura */
    if (rename(FW_DORMANT_METRO, FW_ACTIVE_DIR) < 0)
    {
        rename(FW_DORMANT_AURA, FW_ACTIVE_DIR);
        return false;
    }

    /* 4 y 5 -- el marcador SOLO si la biblioteca cambio desde que Metro
     * construyo su base (D-329, contrato v12): sin sync de por medio el
     * cambio es instantaneo, sin reconstruccion. */
    refresh_root_binary();
    if (aura_sync_switch_needs_rebuild(FW_DORMANT_AURA))
        aura_sync_write_music_pending_marker();

    /* 6: en seco */
    system_reboot();
    return true; /* no se alcanza */
}
