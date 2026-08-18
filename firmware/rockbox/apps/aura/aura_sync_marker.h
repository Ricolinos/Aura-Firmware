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
/* Marcador de sincronizacion pendiente -- docs/contracts/library-layout-v1.md
 * SS4 define el formato. Aura Studio escribe /.aura/sync-pending.json al
 * terminar cada sincronizacion; el firmware lo lee al arrancar y al
 * volver de la pantalla USB, reconstruye los indices de las secciones
 * marcadas y borra el archivo solo si termino bien.
 *
 * Modulo puro en C99, sin dependencias de Rockbox (mismo criterio que
 * aura_style_manifest.c): compila igual en el host (apps/aura/test/) que
 * en el firmware. Solo parsea/serializa texto -- no toca disco. El JSON
 * que entiende es EXACTAMENTE el del contrato (un objeto plano con un
 * subobjeto "changes"); no es un parser JSON general: busca cada clave
 * conocida por nombre e ignora todo lo demas, para que una clave nueva
 * de un Studio futuro no rompa un firmware viejo (mismo criterio que
 * aura.cfg / theme.cfg con settings_parseline()). */
#ifndef AURA_SYNC_MARKER_H
#define AURA_SYNC_MARKER_H

#include <stdbool.h>
#include <stddef.h>

/* Version del esquema que este firmware entiende. Un marcador con
 * "version" MAYOR se ignora (no se reconstruye nada) y se reporta en
 * pantalla -- ver aura_sync.c. */
#define AURA_SYNC_MARKER_VERSION_SUPPORTED 1

/* Intentos consecutivos fallidos tolerados antes de dejar de reintentar
 * solo y ofrecer el disparo manual (contrato SS4: el contador vive
 * dentro del propio marcador, lo escribe el firmware). */
#define AURA_SYNC_MARKER_MAX_ATTEMPTS 3

/* "2026-08-17T20:15:00Z" son 20; margen para milisegundos/offset. */
#define AURA_SYNC_MARKER_TIMESTAMP_LEN 40

typedef struct {
    int  version;       /* obligatorio; -1 si falta o no es numero */
    char timestamp[AURA_SYNC_MARKER_TIMESTAMP_LEN]; /* tal cual, "" si falta */
    bool music;         /* changes.music  */
    bool video;         /* changes.video  */
    bool images;        /* changes.images */
    int  attempts;      /* contador del firmware; 0 si falta */
} aura_sync_marker_t;

typedef enum {
    AURA_SYNC_MARKER_OK = 0,
    AURA_SYNC_MARKER_MALFORMED,       /* no es un objeto JSON reconocible */
    AURA_SYNC_MARKER_MISSING_VERSION, /* sin "version" numerica */
    AURA_SYNC_MARKER_UNSUPPORTED,     /* version > la que entendemos */
} aura_sync_marker_status_t;

/* Deja `out` en el estado "sin cambios": version -1, sin secciones,
 * attempts 0, timestamp vacio. */
void aura_sync_marker_init(aura_sync_marker_t *out);

/* Parsea el texto completo del archivo. Nunca falla por claves extra ni
 * por espacios/saltos de linea. Con AURA_SYNC_MARKER_UNSUPPORTED `out`
 * queda igualmente rellenado (para poder mostrar la version en
 * pantalla). Con MALFORMED/MISSING_VERSION `out` queda como init(). */
aura_sync_marker_status_t aura_sync_marker_parse(const char *text,
                                                 aura_sync_marker_t *out);

/* Escribe el JSON canonico del contrato en `buf`. Devuelve los bytes
 * escritos sin el NUL, o -1 si no cabe. Formato de una sola linea por
 * clave, para que sea legible en un editor de texto si hace falta
 * inspeccionar el iPod a mano. */
int aura_sync_marker_serialize(const aura_sync_marker_t *m,
                               char *buf, size_t bufsize);

/* true si alguna seccion esta marcada -- si ninguna lo esta, no hay
 * nada que reconstruir y el marcador se puede borrar sin mas. */
bool aura_sync_marker_has_work(const aura_sync_marker_t *m);

#endif /* AURA_SYNC_MARKER_H */
