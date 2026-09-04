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
#include "aura_master_art_builder.h"
#include "aura_switch_wait.h"

#include <stdbool.h>
#include <string.h>

#include "file.h"
#include "dir.h"
#include "rbpaths.h"
#include "system.h"
#include "kernel.h"
#include "settings.h"
#include "ata_idle_notify.h"
#include "tagcache.h"

#include "aura_settings.h"
#include "aura_sync.h"

#define FW_ACTIVE_DIR    ROCKBOX_DIR        /* "/.rockbox" */
#define FW_OWN_DORMANT   AURA_FIRMWARE_OWN_DORMANT
#define FW_ROOT_BINARY   "/rockbox.ipod"
#define FW_TREE_BINARY   ROCKBOX_DIR "/rockbox.ipod"

/* Aura (D-342): tope de espera por un commit de tagcache en vuelo antes de
 * reiniciar -- ver la llamada a aura_switch_wait_for_commit() mas abajo. */
#define AURA_SWITCH_COMMIT_WAIT_TICKS (HZ * 8)

bool aura_firmware_sibling_installed(int i)
{
    const char *dir = aura_firmware_sibling_dormant_dir(i);
    return dir != NULL && dir_exists(dir);
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

bool aura_firmware_switch_to(int i)
{
    const char *incoming = aura_firmware_sibling_dormant_dir(i);

    if (incoming == NULL || !dir_exists(incoming))
        return false;
    if (dir_exists(FW_OWN_DORMANT))
        return false; /* no adivinar: Studio garantiza que no pase */

    /* 1. todo lo de Aura al disco, AHORA. D-341: el constructor de
     * maestras se detiene antes -- no puede quedar a mitad de una
     * escritura en el arbol que esta por renombrarse ni dentro de una
     * busqueda de tagcache mientras se apaga. */
    aura_master_art_builder_suspend();
    aura_settings_save();
    /* D-351: aqui NO alcanza con marcar pendiente -- lo que sigue es un
     * reinicio, no una salida de pantalla. Se fuerza la escritura ya. */
    aura_settings_core_touched();
    aura_settings_core_flush();

    /* Aura (D-342): no cortar un commit de tagcache a mitad de escritura --
     * ver DECISIONS.md. Un reinicio con commit_step != 0 puede dejar el
     * flag "dirty" del header maestro COMPARTIDO trabado, forzando a cada
     * familia a reconstruir desde cero en su siguiente arranque aunque los
     * datos estuvieran integros. Espera acotada; si el tope se agota de
     * todas formas seguimos adelante -- el caso es raro y no queremos
     * bloquear el switch indefinidamente. Sin pantalla de por medio hoy;
     * un sleep silencioso es aceptable. */
    {
        long deadline = current_tick + AURA_SWITCH_COMMIT_WAIT_TICKS;
        while (aura_switch_wait_for_commit(tagcache_get_commit_step(),
                                            current_tick, deadline))
            sleep(HZ / 10);
    }

    tagcache_shutdown();
    call_storage_idle_notifys(true);

    /* 2. saliente primero */
    if (rename(FW_ACTIVE_DIR, FW_OWN_DORMANT) < 0)
        return false;

    /* 3. entrante; si falla, seguimos siendo Aura */
    if (rename(incoming, FW_ACTIVE_DIR) < 0)
    {
        rename(FW_OWN_DORMANT, FW_ACTIVE_DIR);
        return false;
    }

    /* 4 y 5 -- el marcador SOLO si la biblioteca cambio desde que se
     * construyo la base COMPARTIDA (D-329 v12 / D-337 v15: la base ya no
     * viaja con el arbol, el sello vive en /.aura/tagcache): sin sync de
     * por medio el cambio es instantaneo, sin reconstruccion. */
    refresh_root_binary();
    if (aura_sync_switch_needs_rebuild())
        aura_sync_write_music_pending_marker();

    /* 6: en seco */
    system_reboot();
    return true; /* no se alcanza */
}
