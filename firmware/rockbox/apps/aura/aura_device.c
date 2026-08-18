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
#include "aura_device.h"
#include "aura_device_name.h"

#include <string.h>
#include "file.h"
#include "misc.h"
#include "rbpaths.h"
#include "settings.h"

#define AURA_DIR         ROCKBOX_DIR "/aura"
#define DEVICE_CFG_PATH  AURA_DIR "/device.cfg"
#define VERSION_TXT_PATH AURA_DIR "/version.txt"

static char s_name[AURA_DEVICE_NAME_BUF];
static bool s_has_name = false;
static char s_version[AURA_FIRMWARE_VERSION_BUF];
static bool s_has_version = false;

static void reload_device_name(void)
{
    int fd;
    char line[64]; /* contrato SS A: lineas <= 63 bytes, mismo buffer que aura.cfg */

    s_has_name = false;
    s_name[0] = '\0';

    fd = open(DEVICE_CFG_PATH, O_RDONLY);
    if (fd < 0)
        return;

    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *name, *value;
        if (!settings_parseline(line, &name, &value))
            continue;
        if (!strcmp(name, "device_name"))
        {
            s_has_name = aura_device_name_sanitize(value, s_name, sizeof(s_name)) > 0;
            break;
        }
    }
    close(fd);
}

/* version.txt NO es "clave: valor" -- una sola linea con el tag tal
 * cual (package_dist.sh: `echo "$RELEASE_TAG" > version.txt`). Sin
 * settings_parseline() de por medio. */
static void reload_firmware_version(void)
{
    int fd;

    s_has_version = false;
    s_version[0] = '\0';

    fd = open(VERSION_TXT_PATH, O_RDONLY);
    if (fd < 0)
        return;

    s_has_version = read_line(fd, s_version, sizeof(s_version)) > 0 && s_version[0] != '\0';
    close(fd);
}

void aura_device_reload(void)
{
    reload_device_name();
    reload_firmware_version();
}

const char *aura_device_name(void)
{
    return s_has_name ? s_name : NULL;
}

const char *aura_firmware_version(void)
{
    return s_has_version ? s_version : NULL;
}
