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
#include "aura_sync.h"

#include <string.h>
#include <stdio.h>
#include "file.h"
#include "dir.h"
#include "rbpaths.h"
#include "timefuncs.h"
#include "tagcache.h"
#include "aura_fsutil.h"
#include "aura_music.h"
#include "aura_video.h"
#include "aura_photos.h"

/* Contrato SS4: el marcador vive en la RAIZ del disco, no bajo
 * /.rockbox/aura -- es la unica cosa que Studio deja para el firmware
 * como "correo", separada de los ajustes/caches propios de Aura. */
#define AURA_SYNC_DIR         "/.aura"
#define AURA_SYNC_MARKER_PATH AURA_SYNC_DIR "/sync-pending.json"

/* Mismo criterio que aura_albumart.c: la cache de caratulas se indexa por
 * album_seek de tagcache, que cambia con cada commit que agrega/quita
 * albumes -- tras reconstruir la base hay que tirarla entera o Cover
 * Flow puede mostrar la portada de otro album (D-293). */
#define AURA_DIR      ROCKBOX_DIR "/aura"
#define CF_CACHE_DIR  AURA_DIR "/cfcache"

/* Un marcador real mide ~150 bytes; 1 KB deja sitio a claves futuras. */
#define MARKER_BUF_SIZE 1024

typedef enum { JOB_NONE = 0, JOB_UPDATE, JOB_REBUILD } job_kind_t;

static aura_sync_state_t s_state = AURA_SYNC_IDLE;
static aura_sync_marker_t s_marker;
static aura_sync_section_state_t s_section[AURA_SYNC_SECTION_COUNT];
static job_kind_t s_job = JOB_NONE;
static unsigned s_jobs_before = 0;
static int s_attempts_before = 0;
static int s_version_seen = -1;
/* Ajustes > Reconstruir biblioteca pide una reconstruccion COMPLETA
 * (Q_REBUILD, la base se tira y se vuelve a levantar de cero) -- es la
 * herramienta de ultimo recurso del usuario y debe cubrir tambien lo que
 * un update por mtime no ve. El marcador de Studio usa Q_UPDATE. */
static bool s_force_full = false;
/* Una sola recuperacion por trabajo: si el update dejo la base
 * inutilizable (tagcache la deshabilita al detectar estructuras corruptas
 * al recargarla a RAM -- p. ej. tras un corte de bateria en mitad de una
 * pasada anterior), se encadena un Q_REBUILD completo. */
static bool s_recovery_done = false;

static bool write_marker(const aura_sync_marker_t *m)
{
    char buf[MARKER_BUF_SIZE];
    int n = aura_sync_marker_serialize(m, buf, sizeof(buf));

    if (n < 0)
        return false;
    if (!dir_exists(AURA_SYNC_DIR))
        mkdir(AURA_SYNC_DIR);
    return aura_fsutil_write_all(AURA_SYNC_MARKER_PATH, buf, (size_t)n);
}

static void remove_marker(void)
{
    remove(AURA_SYNC_MARKER_PATH);
}

static void set_all_sections(aura_sync_section_state_t st)
{
    int i;
    for (i = 0; i < AURA_SYNC_SECTION_COUNT; i++)
        s_section[i] = st;
}

static void go_idle(void)
{
    s_state = AURA_SYNC_IDLE;
    s_job = JOB_NONE;
}

void aura_sync_check_pending(void)
{
    static char buf[MARKER_BUF_SIZE];
    aura_sync_marker_status_t st;
    int n;

    /* Un trabajo en curso manda: no se relee el marcador debajo de el. */
    if (aura_sync_job_active())
        return;

    n = aura_fsutil_read_text(AURA_SYNC_MARKER_PATH, buf, sizeof(buf));
    if (n == -1)
    {
        go_idle();
        return;
    }
    if (n == -2)
    {
        /* Mas grande que cualquier marcador legitimo: no es nuestro. */
        remove_marker();
        go_idle();
        return;
    }

    st = aura_sync_marker_parse(buf, &s_marker);
    s_version_seen = s_marker.version;
    set_all_sections(AURA_SYNC_SECTION_SKIPPED);

    switch (st)
    {
    case AURA_SYNC_MARKER_MALFORMED:
    case AURA_SYNC_MARKER_MISSING_VERSION:
        /* Corrupto (Studio escribe atomico, asi que esto no deberia
         * pasar): no hay nada fiable que reconstruir, y dejarlo haria
         * que cada arranque tropezara con el. Fuera. */
        remove_marker();
        go_idle();
        return;

    case AURA_SYNC_MARKER_UNSUPPORTED:
        /* Contrato SS4: se ignora (no se reconstruye nada con reglas
         * que no entendemos) y se dice en pantalla. El marcador se
         * queda -- lo resolvera un firmware mas nuevo. */
        s_state = AURA_SYNC_ERROR_VERSION;
        return;

    case AURA_SYNC_MARKER_OK:
    default:
        break;
    }

    if (!aura_sync_marker_has_work(&s_marker))
    {
        remove_marker();
        go_idle();
        return;
    }

    if (s_marker.attempts >= AURA_SYNC_MARKER_MAX_ATTEMPTS)
    {
        s_state = AURA_SYNC_ERROR_ATTEMPTS;
        return;
    }

    if (s_marker.music)  s_section[AURA_SYNC_SECTION_MUSIC]  = AURA_SYNC_SECTION_PENDING;
    if (s_marker.video)  s_section[AURA_SYNC_SECTION_VIDEO]  = AURA_SYNC_SECTION_PENDING;
    if (s_marker.images) s_section[AURA_SYNC_SECTION_IMAGES] = AURA_SYNC_SECTION_PENDING;
    s_state = AURA_SYNC_WAIT_TAGCACHE;
}

bool aura_sync_needs_screen(void)
{
    return s_state == AURA_SYNC_WAIT_TAGCACHE
        || s_state == AURA_SYNC_RUNNING
        || s_state == AURA_SYNC_ERROR_VERSION
        || s_state == AURA_SYNC_ERROR_ATTEMPTS
        || s_state == AURA_SYNC_NEEDS_REBOOT;
}

bool aura_sync_job_active(void)
{
    return s_state == AURA_SYNC_WAIT_TAGCACHE
        || s_state == AURA_SYNC_RUNNING
        || s_state == AURA_SYNC_POSTPONED;
}

aura_sync_state_t aura_sync_state(void)
{
    return s_state;
}

aura_sync_section_state_t aura_sync_section_state(aura_sync_section_t s)
{
    unsigned idx = (unsigned)s;
    if (idx >= (unsigned)AURA_SYNC_SECTION_COUNT)
        return AURA_SYNC_SECTION_SKIPPED;
    return s_section[idx];
}

const aura_sync_marker_t *aura_sync_marker(void)
{
    return &s_marker;
}

int aura_sync_marker_version_seen(void)
{
    return s_version_seen;
}

/* Fin feliz: marcador fuera, caches dependientes de la base fuera,
 * y aura_music vuelve a correr su "primera vez por arranque"
 * (calificaciones de Studio + precache de caratulas) sobre la base nueva. */
static void finish_ok(void)
{
    remove_marker();
    if (s_section[AURA_SYNC_SECTION_MUSIC] != AURA_SYNC_SECTION_SKIPPED)
    {
        aura_fsutil_clear_dir(CF_CACHE_DIR);
        aura_music_db_reset_triggers();
    }
    set_all_sections(AURA_SYNC_SECTION_DONE);
    go_idle();
}

static void start_job(void)
{
    /* El intento cuenta desde que arranca: si se va la luz a mitad, el
     * marcador ya lleva el contador subido. Un posponer lo restaura. */
    s_attempts_before = s_marker.attempts;
    s_marker.attempts++;
    write_marker(&s_marker);

    /* Videos/Fotos: listados de directorio bajo demanda (aura_video.c /
     * aura_photos.c) -- basta invalidarlos; la miniatura .pfraw de cada
     * foto se lleva por mtime del original, se renueva sola. */
    if (s_section[AURA_SYNC_SECTION_VIDEO] == AURA_SYNC_SECTION_PENDING)
    {
        aura_video_invalidate();
        s_section[AURA_SYNC_SECTION_VIDEO] = AURA_SYNC_SECTION_DONE;
    }
    if (s_section[AURA_SYNC_SECTION_IMAGES] == AURA_SYNC_SECTION_PENDING)
    {
        aura_photos_invalidate();
        s_section[AURA_SYNC_SECTION_IMAGES] = AURA_SYNC_SECTION_DONE;
    }

    if (s_section[AURA_SYNC_SECTION_MUSIC] != AURA_SYNC_SECTION_PENDING)
    {
        finish_ok();
        return;
    }

    /* Musica. Con base usable y sin un temporal huerfano de una pasada
     * abortada, Q_UPDATE: recorre todo el arbol, re-lee los archivos
     * cuyo mtime cambio (Studio nunca preserva fechas al copiar, asi que
     * todo lo que toco tiene mtime nuevo), agrega los nuevos y quita los
     * borrados (check_deleted_files) -- y la base VIEJA sigue usable
     * mientras tanto. Sin base (primer arranque, o Studio viejo que la
     * borro), o con temporal huerfano (do_tagcache_build lo ve y NO hace
     * nada), Q_REBUILD desde cero. */
    s_jobs_before = tagcache_get_build_jobs_done();
    if (!s_force_full && tagcache_is_usable() && !tagcache_has_pending_temp())
    {
        s_job = JOB_UPDATE;
        tagcache_update();
    }
    else
    {
        s_job = JOB_REBUILD;
        tagcache_rebuild();
    }
    s_force_full = false;
    s_recovery_done = false;
    s_section[AURA_SYNC_SECTION_MUSIC] = AURA_SYNC_SECTION_RUNNING;
    s_state = AURA_SYNC_RUNNING;
}

/* El trabajo encolado termino (bien o abortado). */
static void job_ended(void)
{
    bool ok = tagcache_is_usable() && !tagcache_has_pending_temp();

    if (ok)
    {
        finish_ok();
        return;
    }

    if (!tagcache_is_usable() && !tagcache_has_pending_temp() && !s_recovery_done)
    {
        /* Termino, pero la base quedo deshabilitada: tagcache detecto
         * estructuras corruptas al recargarla (ver load_ramcache()). La
         * unica salida es levantarla de cero -- mismo trabajo, misma
         * pantalla, una sola vez. */
        s_recovery_done = true;
        s_jobs_before = tagcache_get_build_jobs_done();
        s_job = JOB_REBUILD;
        tagcache_rebuild();
        if (s_state != AURA_SYNC_POSTPONED)
            s_state = AURA_SYNC_RUNNING;
        return;
    }

    if (tagcache_get_stat()->commit_delayed)
    {
        /* tagcache escaneo todo pero no consiguio buffer temporal para
         * ordenar los indices y dejo el commit "para el proximo arranque"
         * (su propio mecanismo: el hilo lo confirma al arrancar, antes
         * de nada). No es un fallo nuestro: el contador vuelve a donde
         * estaba, el marcador se queda, y al encender el update se
         * completa (todo ya indexado, pasada rapida) y lo borra. */
        s_marker.attempts = s_attempts_before;
        write_marker(&s_marker);
        set_all_sections(AURA_SYNC_SECTION_SKIPPED);
        s_state = (s_state == AURA_SYNC_POSTPONED) ? AURA_SYNC_IDLE : AURA_SYNC_NEEDS_REBOOT;
        s_job = JOB_NONE;
        return;
    }

    if (s_state == AURA_SYNC_POSTPONED)
    {
        /* Abortado a proposito (Menu): no es un fallo -- el contador
         * vuelve a donde estaba y el marcador queda para el proximo
         * arranque. Un update abortado deja un temporal a medias que
         * bloquearia cualquier build posterior en este arranque; la base
         * vieja esta intacta, asi que se descarta. Un rebuild abortado
         * ya no tiene base vieja: el temporal se conserva para que el
         * arranque de tagcache lo confirme (commit) y no se pierda lo
         * escaneado. */
        s_marker.attempts = s_attempts_before;
        write_marker(&s_marker);
        if (s_job == JOB_UPDATE && tagcache_has_pending_temp())
            tagcache_discard_pending_temp();
        set_all_sections(AURA_SYNC_SECTION_SKIPPED);
        go_idle();
        return;
    }

    /* Fallo real (USB enchufado a mitad, base que no quedo usable...):
     * el contador ya subio; el marcador se queda y se reintenta en el
     * proximo arranque, salvo que ya sean demasiados. */
    set_all_sections(AURA_SYNC_SECTION_SKIPPED);
    if (s_marker.attempts >= AURA_SYNC_MARKER_MAX_ATTEMPTS)
    {
        s_state = AURA_SYNC_ERROR_ATTEMPTS;
        s_job = JOB_NONE;
    }
    else
        go_idle();
}

bool aura_sync_tick(void)
{
    switch (s_state)
    {
    case AURA_SYNC_WAIT_TAGCACHE:
        /* Misma precaucion que aura_music_db_ready() (D-021/D-206): no
         * decidir update-vs-rebuild hasta que tagcache haya determinado
         * si YA hay una base usable en disco (~1 s tras el arranque). */
        if (!tagcache_is_fully_initialized())
            return false;
        start_job();
        return true;

    case AURA_SYNC_RUNNING:
    case AURA_SYNC_POSTPONED:
        if (tagcache_get_build_jobs_done() == s_jobs_before)
            return s_state == AURA_SYNC_RUNNING; /* progreso: redibujar */
        job_ended();
        return true;

    default:
        return false;
    }
}

void aura_sync_postpone(void)
{
    switch (s_state)
    {
    case AURA_SYNC_WAIT_TAGCACHE:
        /* Nada arranco todavia: el marcador queda tal cual. */
        set_all_sections(AURA_SYNC_SECTION_SKIPPED);
        s_force_full = false;
        go_idle();
        break;
    case AURA_SYNC_RUNNING:
        tagcache_stop_scan();
        s_state = AURA_SYNC_POSTPONED;
        break;
    default:
        break;
    }
}

void aura_sync_dismiss(void)
{
    if (s_state == AURA_SYNC_ERROR_VERSION || s_state == AURA_SYNC_ERROR_ATTEMPTS
        || s_state == AURA_SYNC_NEEDS_REBOOT)
        go_idle();
}

bool aura_sync_request_manual(void)
{
    aura_sync_marker_t m;
    struct tm *now;

    if (aura_sync_job_active())
        return true; /* ya hay uno andando: la pantalla lo muestra */

    aura_sync_marker_init(&m);
    m.version = AURA_SYNC_MARKER_VERSION_SUPPORTED;
    m.music = m.video = m.images = true;
    m.attempts = 0;
    now = get_time();
    if (now && valid_time(now))
        snprintf(m.timestamp, sizeof(m.timestamp), "%04d-%02d-%02dT%02d:%02d:%02d",
                 now->tm_year + 1900, now->tm_mon + 1, now->tm_mday,
                 now->tm_hour, now->tm_min, now->tm_sec);
    else
        strcpy(m.timestamp, "1970-01-01T00:00:00");

    if (!write_marker(&m))
        return false;

    /* Un error pendiente en pantalla (version/intentos) queda superado
     * por el marcador recien escrito. */
    go_idle();
    s_force_full = true;
    aura_sync_check_pending();
    if (!aura_sync_job_active())
        s_force_full = false;
    return aura_sync_needs_screen();
}

bool aura_sync_music_indexing(void)
{
    return s_state == AURA_SYNC_RUNNING && tagcache_get_stat()->commit_step > 0;
}

int aura_sync_music_progress_256(char *detail, size_t detail_len)
{
    struct tagcache_stat *st;
    int max_step, pct;

    if (detail && detail_len)
        detail[0] = '\0';

    if (s_state != AURA_SYNC_RUNNING)
        return -1;
    if (s_section[AURA_SYNC_SECTION_MUSIC] != AURA_SYNC_SECTION_RUNNING)
        return -1;

    st = tagcache_get_stat();
    max_step = tagcache_get_max_commit_step();

    /* Dos tramos: escaneo de disco (0..200) e indexado/commit (200..256).
     * El porcentaje de escaneo de tagcache es una estimacion (0 al
     * principio y con dircache frio), asi que el detalle lleva siempre
     * el conteo real de carpetas, que si avanza de forma visible. */
    if (st->commit_step > 0 && max_step > 0)
    {
        if (detail && detail_len)
            snprintf(detail, detail_len, "%d/%d", st->commit_step, max_step);
        return 200 + (56 * st->commit_step) / max_step;
    }

    pct = st->progress;
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    if (detail && detail_len)
        snprintf(detail, detail_len, "%d", st->processed_entries);
    if (pct == 0 && st->processed_entries == 0)
        return -1;
    return (200 * pct) / 100;
}
