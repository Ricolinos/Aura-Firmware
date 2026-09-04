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
/* D-340/D-341: ver aura_master_art_builder.h para el diseno y la
 * evidencia de seguridad entre hilos. */
#include <stdint.h>

#include "config.h"
#include "file.h"
#include "dir.h"
#include "kernel.h"
#include "thread.h"
#include "debug.h"
#include "audio.h"

#include "aura_music.h"
#include "aura_albumart.h"
#include "aura_artist_images.h"
#include "aura_photos.h"
#include "aura_master_art.h"
#include "aura_master_art_builder.h"

/* Mismo dimensionamiento que tagcache_stack (apps/tagcache.c): este
 * hilo recorre cadenas de llamada tan profundas como las de tagcache
 * (busquedas) mas el decodificador JPEG -- nunca la pila de UI de 8 KB
 * (D-226). Propia, estatica, fuera del BSS de cualquier otro hilo. */
static long s_builder_stack[(DEFAULT_STACK_SIZE + 0x4000) / sizeof(long)];
static const char s_builder_thread_name[] = "aura master art";

static unsigned int s_thread_id = 0;
static bool s_started = false;              /* start() se llamo alguna vez */
static bool s_running = false;              /* el hilo esta vivo ahora mismo */
static volatile bool s_paused = false;      /* pausa rapida (animacion/scroll) */
static volatile bool s_stop_requested = false;    /* suspend(): salir YA */
static volatile bool s_restart_requested = false; /* restart(): reiniciar */
/* D-344: progreso de la pasada. Lo ESCRIBE solo el hilo del constructor
 * y lo LEE solo el de UI; son enteros de palabra y Rockbox planifica de
 * forma cooperativa (el cambio de contexto solo ocurre en yield/sleep/
 * bloqueo), asi que no hace falta candado -- misma disciplina que
 * s_paused. */
static volatile aura_master_art_phase_t s_phase = AURA_MASTER_ART_PHASE_IDLE;
static volatile int s_phase_done = 0;
static volatile int s_phase_total = 0;   /* 0 = desconocido (fotos) */
static volatile bool s_pass_done = false;
static volatile bool s_foreground = false;

/* Buffer de trabajo propio del constructor -- 130x130 alcanza para
 * album/artista (130) y fotos (80, cabe con margen). Nunca el scratch
 * transpuesto del hilo de UI (aura_albumart.c): ese es de otro dueno. */
static fb_data s_flat[AURA_MASTER_ART_ALBUM_SIZE * AURA_MASTER_ART_ALBUM_SIZE];

/* Tabla de claves vivas para el GC de cada subdirectorio -- mismo cupo
 * que usaba el precache retirado (D-338/D-339) para albumes; se reusa
 * para las tres fases (una a la vez, nunca simultaneas). */
#define BUILDER_MAX_KEYS AURA_MUSIC_MAX_ITEMS
static aura_master_art_key_t s_live_keys[BUILDER_MAX_KEYS];
/* Snapshot de albumes de esta pasada -- static: BUILDER_MAX_KEYS *
 * sizeof(aura_music_item_t) es demasiado para cualquier pila. */
static aura_music_item_t s_album_items[BUILDER_MAX_KEYS];

/* D-346: memoria de trabajo propia de ESTE hilo para la busqueda de
 * tagcache. Hasta D-346, aura_music_browse() usaba arreglos estaticos
 * del modulo aura_music.c que el hilo de UI escribia al mismo tiempo --
 * dos busquedas simultaneas se pisaban el buffer de valores unicos sin
 * candado alguno (entrar a Albumes mientras esta fase corre era
 * suficiente). Ahora cada hilo trae el suyo y no hay nada que
 * sincronizar.
 *
 * `tracknums` queda en NULL a proposito: esta fase solo lista ALBUMES y
 * el numero de pista no significa nada ahi, asi que se ahorran los
 * 20 KB de la tabla. aura_music_browse_scratch() rechaza en voz alta
 * una busqueda de titulos con este scratch, en vez de devolver una
 * lista mal ordenada. */
static aura_music_scratch_t s_builder_scratch; /* tracknums = NULL */

/* D-344: la espera entre elementos. En segundo plano es HZ/20 (deja el
 * disco y la CPU al usuario); en primer plano solo se cede el turno --
 * el usuario esta esperando esta pasada, no compitiendo con ella. */
static void breathe(void)
{
    if (s_foreground)
        yield();
    else
        sleep(HZ / 20);
}

static bool should_stop(void)
{
    return s_stop_requested || s_restart_requested;
}

/* Tramos cortos (HZ/10) para reaccionar rapido a pausa/paro/reinicio en
 * vez de un sleep(HZ) ciego -- acota la latencia de suspend() a esto,
 * no a un segundo entero. */
static void idle_wait(void)
{
    sleep(HZ / 10);
}

/* D-341: no hay una bandera publica de "cargando buffer" en audio.h
 * (solo PLAY/PAUSE/RECORD/PRERECORD/ERROR/WARNING) -- AUDIO_STATUS_PLAY
 * es el proxy mas cercano y el mas conservador disponible: se detiene
 * durante TODA la reproduccion activa (el hilo de audio puede pedir
 * disco para rellenar su buffer en cualquier momento mientras suena),
 * no solo en el instante exacto del relleno. Historial real de freezes
 * por contencion de disco/CPU con este hilo: D-204/D-206/D-214. */
static bool audio_wants_disk(void)
{
    return (audio_status() & AUDIO_STATUS_PLAY) != 0;
}

/* true: seguir trabajando. false: abortar la pasada en curso (pidieron
 * detenerse o reiniciar) -- el llamador vuelve sin tocar el GC de esta
 * fase (una pasada incompleta no debe borrar nada). */
static bool wait_if_blocked(void)
{
    /* D-344: en primer plano la pausa de animacion no aplica -- la que
     * la levanta es la cadencia de Music Flow, y durante la preparacion
     * lo que hay en pantalla es el progreso, no un carrusel. El audio
     * SI se sigue respetando: si suena musica, el disco es suyo. */
    while ((s_paused && !s_foreground) || audio_wants_disk())
    {
        if (should_stop())
            return false;
        idle_wait();
    }
    return !should_stop();
}

/* -- Fase de albumes (aura_music_browse() + aura_albumart_build_master())
 * ------------------------------------------------------------------- */
static void run_albums_phase(void)
{
    int count, i, nkeys = 0;

    if (!wait_if_blocked())
        return;
    count = aura_music_browse_scratch(AURA_SCREEN_MUSIC_ALBUMS, s_album_items,
                                      BUILDER_MAX_KEYS, &s_builder_scratch);
    s_phase_total = count > 0 ? count : 0;
    if (count <= 0)
        return;

    for (i = 0; i < count; i++)
    {
        s_phase_done = i;
        if (!wait_if_blocked())
            return; /* pasada incompleta: sin GC, se retoma en la proxima vuelta */
        if (aura_albumart_build_master(s_album_items[i].seek, &s_live_keys[nkeys], s_flat)
            && nkeys < BUILDER_MAX_KEYS)
            nkeys++;
        breathe();
    }
    /* GC de cfcache/a-* Y /.aura/art/albums con la misma tabla (D-338,
     * ahora tambien aura_master_art_gc() por dentro de esta funcion) --
     * unico lugar donde corre desde que se retiro el precache. */
    s_phase_done = count;
    aura_albumart_gc_orphans(s_live_keys, nkeys);
    DEBUGF("aura_master_art_builder: albumes %d, claves vivas %d\n", count, nkeys);
}

/* -- Fase de fotos de artista (aura_artist_images.c) -------------------- */
static void run_artists_phase(void)
{
    int count = aura_artist_images_count();
    int i, nkeys = 0;
    bool overflow = false;

    s_phase_total = count > 0 ? count : 0;
    if (count <= 0)
        return;
    for (i = 0; i < count; i++)
    {
        char path[MAX_PATH];
        uint32_t mtime = 0;
        aura_master_art_key_t key;

        s_phase_done = i;
        if (!wait_if_blocked())
            return; /* pasada incompleta: sin GC */
        if (!aura_artist_images_entry(i, path, sizeof(path), &mtime))
            continue;
        if (!aura_artist_art_build_master(path, mtime, &key, s_flat))
            continue; /* archivo del indice ausente: transitorio, D-341 */
        if (nkeys < BUILDER_MAX_KEYS)
            s_live_keys[nkeys++] = key;
        else
            overflow = true;
        breathe();
    }
    if (overflow)
        DEBUGF("aura_master_art_builder: %d fotos de artista > cupo GC %d, "
               "sin GC esta pasada\n", count, BUILDER_MAX_KEYS);
    else
        aura_master_art_gc(AURA_MASTER_ART_ARTIST, s_live_keys, nkeys);
}

/* -- Fase de fotos (aura_photos.c, recorrido propio de /Photos) --------- */
static void run_photos_phase(void)
{
    DIR *d;
    struct DIRENT *entry;
    int nkeys = 0;
    bool overflow = false;

    s_phase_total = 0; /* streaming del directorio: no hay conteo previo */
    if (!wait_if_blocked())
        return;
    d = opendir(aura_photos_dir());
    if (!d)
        return;

    while ((entry = readdir(d)) != NULL)
    {
        struct dirinfo info;
        aura_master_art_key_t key;

        if (!aura_photos_is_listable_name(entry->d_name))
            continue;
        if (!wait_if_blocked())
        {
            closedir(d);
            return; /* pasada incompleta: sin GC */
        }
        info = dir_get_info(d, entry);
        if (!aura_photos_build_master(entry->d_name, (uint32_t)info.mtime, &key, s_flat))
            continue;
        s_phase_done++;
        if (nkeys < BUILDER_MAX_KEYS)
            s_live_keys[nkeys++] = key;
        else
            overflow = true;
        breathe();
    }
    closedir(d);

    if (overflow)
        DEBUGF("aura_master_art_builder: fotos > cupo GC %d, sin GC esta pasada\n",
               BUILDER_MAX_KEYS);
    else
        aura_master_art_gc(AURA_MASTER_ART_PHOTO, s_live_keys, nkeys);
}

/* -- Hilo ---------------------------------------------------------------
 *
 * Una pasada completa deja resuelto todo lo que la base/el disco
 * describian en ese momento; despues se queda dormido (tramos cortos,
 * reactivo a stop/restart) hasta el proximo aura_master_art_builder_
 * restart() -- que solo llega cuando aura_sync.c termina de verdad una
 * reconstruccion (finish_ok()), nunca a mitad. */
static void builder_thread(void)
{
    while (1)
    {
        if (s_stop_requested)
            break;
        s_restart_requested = false;

        s_phase = AURA_MASTER_ART_PHASE_ALBUMS;
        s_phase_done = 0;
        s_phase_total = 0;
        run_albums_phase();
        if (s_stop_requested)
            break;
        if (s_restart_requested)
            continue;

        s_phase = AURA_MASTER_ART_PHASE_ARTISTS;
        s_phase_done = 0;
        s_phase_total = 0;
        run_artists_phase();
        if (s_stop_requested)
            break;
        if (s_restart_requested)
            continue;

        s_phase = AURA_MASTER_ART_PHASE_PHOTOS;
        s_phase_done = 0;
        s_phase_total = 0;
        run_photos_phase();
        if (s_stop_requested)
            break;
        if (s_restart_requested)
            continue;

        /* D-344: las tres fases completas y sin cortes -- lo unico que
         * autoriza a la pantalla de preparacion a cerrarse. Se pone
         * DESPUES de las guardas de stop/restart de arriba: una pasada
         * abortada nunca cuenta como terminada. */
        s_phase = AURA_MASTER_ART_PHASE_IDLE;
        s_pass_done = true;

        while (!s_stop_requested && !s_restart_requested)
            idle_wait();
    }
    s_running = false;
    thread_exit();
}

static void start_thread_if_needed(void)
{
    if (s_running)
        return;
    s_stop_requested = false;
    s_thread_id = create_thread(builder_thread, s_builder_stack, sizeof(s_builder_stack),
                                0, s_builder_thread_name
                                IF_PRIO(, PRIORITY_BACKGROUND)
                                IF_COP(, CPU));
    s_running = (s_thread_id != 0);
}

void aura_master_art_builder_start(void)
{
    if (s_started)
        return;
    s_started = true;
    start_thread_if_needed();
}

void aura_master_art_builder_restart(void)
{
    if (!s_started)
        return; /* nunca arranco: start() lo hara desde albumes de todos modos */
    s_pass_done = false;
    s_restart_requested = true;
    /* Si esta suspendido (p.ej. a mitad de un handoff), no hay hilo que
     * despertar -- resume() lo va a recrear y el bucle de arriba
     * arranca su primera vuelta con s_restart_requested todavia en
     * true, que se limpia justo antes de esa vuelta (comportamiento
     * identico a reiniciar desde albumes). */
}

void aura_master_art_builder_pause(bool pause)
{
    s_paused = pause;
}

void aura_master_art_builder_suspend(void)
{
    if (!s_running)
        return;
    s_stop_requested = true;
    thread_wait(s_thread_id);
    s_thread_id = 0;
    s_running = false;
}

void aura_master_art_builder_resume(void)
{
    if (!s_started || s_running)
        return;
    start_thread_if_needed();
}

/* -- D-344: preparacion explicita (Ajustes > Actualizar biblioteca,
 * marcador de sync, primer arranque tras actualizar) -------------------- */

bool aura_master_art_builder_progress(aura_master_art_phase_t *phase,
                                       int *done, int *total)
{
    if (phase)
        *phase = s_phase;
    if (done)
        *done = s_phase_done;
    if (total)
        *total = s_phase_total;
    return s_running && s_phase != AURA_MASTER_ART_PHASE_IDLE;
}

bool aura_master_art_builder_pass_done(void)
{
    return s_pass_done;
}

bool aura_master_art_builder_is_running(void)
{
    return s_running;
}

void aura_master_art_builder_set_foreground(bool foreground)
{
    s_foreground = foreground;
}

void aura_master_art_builder_begin_full_pass(void)
{
    s_pass_done = false;
    /* start() es no-op si el hilo ya existe; restart() lo devuelve al
     * principio de albumes. Juntos: "recorre TODO desde cero", que es lo
     * que una preparacion explicita promete -- continuar una pasada a
     * medias dejaria los albumes sin revisar. */
    aura_master_art_builder_start();
    aura_master_art_builder_restart();
}
