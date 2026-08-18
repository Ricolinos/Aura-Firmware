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
#include "aura_fsutil.h"

#include <string.h>
#include <stdio.h>
#include "file.h"
#include "dir.h"
#include "rbpaths.h"

static bool remove_children(const char *path, DIR *d)
{
    struct DIRENT *entry;
    bool ok = true;

    while ((entry = readdir(d)) != NULL)
    {
        /* 2x MAX_PATH: `path` y `entry->d_name` son cada uno hasta
         * MAX_PATH -- ningun arbol real de Aura se acerca a esto, pero un
         * buffer del tamano justo hace que gcc no pueda probar que
         * snprintf() no trunca (-Wformat-truncation). */
        char child[MAX_PATH * 2];
        struct dirinfo info;

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        info = dir_get_info(d, entry);
        if (info.attribute & ATTR_DIRECTORY)
            ok = aura_fsutil_remove_tree(child) && ok;
        else
            ok = (remove(child) >= 0) && ok;
    }
    return ok;
}

bool aura_fsutil_remove_tree(const char *path)
{
    DIR *d = opendir(path);
    bool ok;

    if (!d)
        return false;
    ok = remove_children(path, d);
    closedir(d);
    return (rmdir(path) >= 0) && ok;
}

bool aura_fsutil_clear_dir(const char *path)
{
    DIR *d = opendir(path);
    bool ok;

    if (!d)
        return false;
    ok = remove_children(path, d);
    closedir(d);
    return ok;
}

int aura_fsutil_read_text(const char *path, char *buf, size_t bufsize)
{
    int fd, n;

    if (bufsize == 0)
        return -2;
    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    n = read(fd, buf, bufsize - 1);
    close(fd);
    if (n < 0)
        n = 0;
    if ((size_t)n >= bufsize - 1)
    {
        buf[0] = '\0';
        return -2;
    }
    buf[n] = '\0';
    return n;
}

bool aura_fsutil_write_all(const char *path, const char *data, size_t len)
{
    int fd = creat(path, 0666);
    int n;

    if (fd < 0)
        return false;
    n = write(fd, data, len);
    close(fd);
    return n >= 0 && (size_t)n == len;
}
