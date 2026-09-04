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
/* D-340/D-341 (contrato v16): constructor en SEGUNDO PLANO, sin
 * pantalla, de la cache maestra compartida (/.aura/art/{albums,artists,
 * photos}/, aura_master_art.h). Reemplaza la capsula "Preparando
 * caratulas N/M" (D-224, retirada) -- lo que falte se ve como tile por
 * defecto hasta que este constructor lo alcance, nunca bloquea al
 * usuario.
 *
 * Recorre albumes -> fotos de artista -> fotos, en ese orden, llamando
 * aura_albumart_build_master()/aura_artist_art_build_master()/
 * aura_photos_build_master() por cada elemento pendiente (dejan .art o
 * .none) y recogiendo huerfanas de cada subdirectorio con la tabla de
 * claves vivas de esa misma pasada.
 *
 * Hilo Rockbox propio a PRIORITY_BACKGROUND (aura_master_art_builder.c),
 * NUNCA la pila de UI de 8 KB (D-226) -- pila y buffers de trabajo
 * propios y estaticos. Evidencia de que esto es seguro (ver
 * DECISIONS.md D-341 para el detalle citado):
 *  - `apps/tagcache.c` ya corre su propio hilo de comitido/escaneo a
 *    PRIORITY_BACKGROUND (tagcache_thread, tagcache.c:5628) MIENTRAS el
 *    hilo de UI hace busquedas -- tagcache_search()/get_next()/
 *    retrieve() estan disenadas para llamadores concurrentes,
 *    coordinadas por sus propios read_lock/write_lock internos
 *    (tagcache.c: `while (read_lock) sleep(1);` al entrar a
 *    tagcache_search()).
 *  - Este mismo firmware YA llama a tagcache (tagcache_find_index()/
 *    tagcache_get_numeric()/tagcache_search_finish()) desde el hilo de
 *    audio/buffering, no el de UI: `aura_music_buffer_event()`
 *    (aura_music.c), registrada con
 *    `add_event(PLAYBACK_EVENT_TRACK_BUFFER, ...)`.
 *  - `apps/recorder/jpeg_load.c` no tiene estado global en el build del
 *    core: `static struct jpeg jpeg` solo existe bajo `JPEG_FROM_MEM`,
 *    que ningun Makefile de este arbol define fuera de los plugins que
 *    lo piden a proposito (`grep -rn JPEG_FROM_MEM apps/` sin ningun
 *    `-D` en este repo) -- el decodificador es reentrante si cada
 *    llamador le pasa su propio buffer, que es exactamente lo que hace
 *    aura_master_art.c (scratch propio bajo su propio candado
 *    recursivo, compartido por este constructor y el hilo de UI). */
#ifndef AURA_MASTER_ART_BUILDER_H
#define AURA_MASTER_ART_BUILDER_H

#include <stdbool.h>

/* D-344: fase de la pasada, para la pantalla "Actualizando biblioteca".
 * El orden es el del recorrido (albumes -> artistas -> fotos). */
typedef enum {
    AURA_MASTER_ART_PHASE_IDLE = 0,
    AURA_MASTER_ART_PHASE_ALBUMS,
    AURA_MASTER_ART_PHASE_ARTISTS,
    AURA_MASTER_ART_PHASE_PHOTOS,
} aura_master_art_phase_t;

/* Arranca el hilo la primera vez que se llama (aura_music_db_ready(),
 * la misma puerta de "base recien confirmada usable" que antes disparaba
 * el precache retirado). Llamadas repetidas son un no-op silencioso. */
void aura_master_art_builder_start(void);

/* La base cambio bajo los pies (reconstruccion/rebuild terminado,
 * aura_sync.c: finish_ok() -> aura_music_db_reset_triggers()): abandona
 * la pasada en curso (si la hay) y la reinicia desde el principio de
 * albumes en cuanto pueda correr de nuevo. Si aura_master_art_builder_
 * start() todavia no se llamo nunca, no hace nada -- start() arrancara
 * limpio cuando le toque. */
void aura_master_art_builder_restart(void);

/* Pausa/reanuda RAPIDO sin detener el hilo: el elemento en curso
 * termina siempre (nunca queda a medias), y el hilo se queda dormido
 * hasta el proximo aura_master_art_builder_pause(false). Para la
 * cadencia fina de animacion (aura_main.c) y el scroll de Music Flow
 * (aura_musicflow.c) -- un decode de 100-300 ms no debe competir por
 * CPU/disco con un carrusel a 20 fps. Tambien se detiene solo, sin que
 * nadie llame a esto, mientras suene musica (ver el comentario grande
 * de arriba sobre el hilo de audio). */
void aura_master_art_builder_pause(bool pause);

/* Detiene el hilo por completo antes de un handoff que no admite
 * trabajo de fondo a medias -- USB, apagado, reinicio, cambio de
 * firmware: espera (bloqueante pero acotado, un elemento tarda como
 * mucho unos cientos de ms) a que el elemento en curso termine y el
 * hilo salga limpio, sin ninguna escritura a medias en /.aura/art ni
 * busqueda de tagcache abierta. Si el hilo no estaba corriendo, no hace
 * nada. El iterador (fase/indice) NO se reinicia -- resume() continua
 * donde se quedo. */
void aura_master_art_builder_suspend(void);

/* Reanuda tras un suspend(): vuelve a crear el hilo si aura_master_art_
 * builder_start() ya se habia llamado antes, retomando el iterador
 * donde suspend() lo dejo. No-op si nunca arranco o si ya esta
 * corriendo. */
void aura_master_art_builder_resume(void);

/* D-344 (encargo del dueno: "Actualizar Biblioteca"). Una PREPARACION
 * explicita -- la manual de Ajustes, el marcador de un sync de Studio y
 * el primer arranque tras actualizar el firmware -- termina la pasada de
 * imagenes ANTES de devolverle el control al usuario, con su progreso en
 * la misma pantalla de "Actualizando biblioteca". Es exactamente lo que
 * D-341 quito del camino normal (y con razon: ahi bloqueaba sin que
 * nadie lo pidiera); aca el usuario lo pidio y se le advirtio cuanto
 * tarda, que es la diferencia entre las dos situaciones.
 *
 * `aura_sync.c` es el unico llamador de estas cuatro. */

/* Progreso de la pasada en curso. `total` en 0 = todavia no se sabe
 * (la fase de fotos recorre el directorio en streaming, sin conteo
 * previo): la pantalla muestra solo el hecho. Devuelve false si no hay
 * pasada corriendo. */
bool aura_master_art_builder_progress(aura_master_art_phase_t *phase,
                                       int *done, int *total);

/* true en cuanto una pasada COMPLETA (las tres fases, sin cortes)
 * termino desde el ultimo restart(). Lo que espera la pantalla de
 * preparacion para cerrarse. */
bool aura_master_art_builder_pass_done(void);

/* true si el hilo existe ahora mismo. La pantalla de preparacion lo usa
 * para no quedarse esperando una pasada que nunca va a llegar (el hilo
 * no se pudo crear, o un suspend() lo bajo): sin hilo no hay progreso
 * posible y devolver el control es mejor que un progreso congelado. */
bool aura_master_art_builder_is_running(void);

/* Primer plano: sin la espera de HZ/20 entre elementos y sin ceder ante
 * la pausa de animacion -- el usuario esta mirando una pantalla de
 * progreso y no hay carrusel con el que competir. Sigue cediendo la CPU
 * (yield) para que la pantalla se redibuje, y sigue respetando
 * suspend() y el audio. */
void aura_master_art_builder_set_foreground(bool foreground);

/* Arranca el hilo si hacia falta Y reinicia la pasada desde el
 * principio, para que la preparacion recorra TODO y no continue una
 * pasada a medias que ya paso por los albumes. */
void aura_master_art_builder_begin_full_pass(void);

#endif /* AURA_MASTER_ART_BUILDER_H */
