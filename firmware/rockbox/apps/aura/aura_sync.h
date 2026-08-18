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
/* Reconstruccion de la biblioteca tras una sincronizacion (D-293,
 * docs/contracts/library-layout-v1.md SS4).
 *
 * El firmware NO corre mientras el iPod esta montado por USB, asi que
 * Aura Studio no puede pedirle nada: solo deja /.aura/sync-pending.json
 * al terminar cada sincronizacion. Este modulo lo lee en dos momentos --
 * al arrancar y al volver de la pantalla USB (cuando el firmware
 * recupera el disco) -- y reconstruye los indices de las secciones
 * marcadas: la base de datos de tagcache para Musica (la unica que
 * de verdad cuesta), y los listados de Videos/Fotos (escaneo de
 * directorio bajo demanda, basta invalidarlos). Borra el marcador solo
 * al terminar bien; si se interrumpe (bateria, apagado) queda en disco
 * y se reintenta en el siguiente arranque, con un contador de intentos
 * DENTRO del propio marcador: al tercer fallo consecutivo deja de
 * reintentar solo, lo dice en pantalla y ofrece el disparo manual
 * (Ajustes > Reconstruir biblioteca), que escribe el mismo marcador con
 * todas las secciones y arranca de inmediato.
 *
 * Maquina de estados que la pantalla AURA_SCREEN_LIBRARY_SYNC (en
 * aura_screens.c) solo DIBUJA; el avance vive en aura_sync_tick(),
 * llamado en cada vuelta del loop principal (aura_main.c) -- tambien con
 * la pantalla pospuesta, para poder cerrar el trabajo en fondo.
 *
 * Todo el trabajo pesado lo hace el hilo de tagcache (Q_UPDATE /
 * Q_REBUILD): aqui no se crea ningun hilo (regla del dueno, ver
 * aura_music.c junto a aura_music_precache_album_art()). */
#ifndef AURA_SYNC_H
#define AURA_SYNC_H

#include <stdbool.h>
#include <stddef.h>
#include "aura_sync_marker.h"

typedef enum {
    AURA_SYNC_IDLE = 0,       /* nada pendiente, ninguna pantalla */
    AURA_SYNC_WAIT_TAGCACHE,  /* marcador leido; esperando que tagcache
                                 termine de decidir si hay base usable */
    AURA_SYNC_RUNNING,        /* trabajo encolado en tagcache, en curso */
    AURA_SYNC_POSTPONED,      /* Menu: pantalla cerrada, el trabajo ya
                                 encolado se cierra en fondo (ver .c) */
    AURA_SYNC_ERROR_VERSION,  /* marcador de una version que no entendemos */
    AURA_SYNC_ERROR_ATTEMPTS, /* AURA_SYNC_MARKER_MAX_ATTEMPTS fallos seguidos */
    AURA_SYNC_NEEDS_REBOOT,   /* tagcache pospuso el commit "hasta el proximo
                                 arranque" (sin buffer temporal suficiente):
                                 no es un fallo, se termina solo al encender */
} aura_sync_state_t;

typedef enum {
    AURA_SYNC_SECTION_MUSIC = 0,
    AURA_SYNC_SECTION_VIDEO,
    AURA_SYNC_SECTION_IMAGES,
    AURA_SYNC_SECTION_COUNT,
} aura_sync_section_t;

typedef enum {
    AURA_SYNC_SECTION_SKIPPED = 0, /* no marcada en el marcador */
    AURA_SYNC_SECTION_PENDING,
    AURA_SYNC_SECTION_RUNNING,
    AURA_SYNC_SECTION_DONE,
} aura_sync_section_state_t;

/* Lee el marcador (si existe) y decide. Llamar al arrancar (antes del
 * primer cuadro) y al volver de la pantalla USB. Tras la llamada,
 * aura_sync_needs_screen() dice si hay que empujar la pantalla. */
void aura_sync_check_pending(void);

/* true mientras hay algo que mostrar a pantalla completa: espera,
 * progreso o un error. La pantalla se cierra sola (aura_sync_tick()
 * pasa a IDLE) al terminar bien. */
bool aura_sync_needs_screen(void);

aura_sync_state_t aura_sync_state(void);
aura_sync_section_state_t aura_sync_section_state(aura_sync_section_t s);
const aura_sync_marker_t *aura_sync_marker(void);

/* Avanza la maquina de estados. Barato; llamar en cada vuelta del loop
 * principal. Devuelve true si algo cambio (para redibujar). */
bool aura_sync_tick(void);

/* Menu en la pantalla de progreso: la cierra sin cancelar. */
void aura_sync_postpone(void);

/* Menu en una pantalla de error: la cierra hasta el proximo arranque o
 * la proxima desconexion USB (el marcador se queda). */
void aura_sync_dismiss(void);

/* Ajustes > Reconstruir biblioteca: escribe el marcador con las tres
 * secciones y arranca de inmediato. false si no se pudo escribir. */
bool aura_sync_request_manual(void);

/* Progreso estimado de la seccion Musica en [0, 256]; -1 si no aplica
 * (todavia sin empezar / indeterminado). Texto corto de detalle
 * ("32 carpetas", "indexando 4/9") en `detail`. */
int aura_sync_music_progress_256(char *detail, size_t detail_len);

/* true en la fase de indexado/commit de tagcache (el detalle es "k/K"),
 * false en la fase de escaneo de disco (el detalle es un conteo). */
bool aura_sync_music_indexing(void);

/* Solo para el mensaje de error de version: la version leida. */
int aura_sync_marker_version_seen(void);

/* Ver aura_music.c: mientras hay un trabajo activo aqui, aura_music_db_ready()
 * no dispara su propio tagcache_rebuild()/start_scan(). */
bool aura_sync_job_active(void);

#endif /* AURA_SYNC_H */
