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
#include "aura_master_art_builder.h" /* D-344: aura_master_art_phase_t */

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
    AURA_SYNC_BUILDING_ART,   /* D-344: base lista; terminando la cache
                                 maestra de imagenes (/.aura/art) antes de
                                 devolver el control. Posponible con Menu:
                                 el constructor sigue en segundo plano. */
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

/* Ajustes > Actualizar biblioteca: escribe el marcador con las tres
 * secciones y arranca de inmediato. false si no se pudo escribir.
 * D-344: al terminar la base sigue la cache maestra de imagenes
 * (AURA_SYNC_BUILDING_ART) -- "preparar la biblioteca" incluye dejar las
 * caratulas listas, que es lo que hace que Music Flow y las rejillas no
 * esperen despues. */
bool aura_sync_request_manual(void);

/* D-344: progreso de la fase de imagenes, para la pantalla. Devuelve
 * false si no estamos en esa fase. `total` en 0 = desconocido. */
bool aura_sync_art_progress(aura_master_art_phase_t *phase, int *done, int *total);

/* D-327 (contrato v10): deja /.aura/sync-pending.json con music=true y
 * attempts=0 -- lo que el firmware que DESPIERTA tras un cambio de
 * firmware necesita para reconstruir su propia base de datos (la base
 * vive dentro de cada arbol). Misma escritura que usa el propio ciclo. */
bool aura_sync_write_music_pending_marker(void);

/* D-337 (contrato v15): la base tagcache (database_*.tcd) y su sello
 * db_stamp.txt viven en un directorio COMPARTIDO por las tres familias
 * (Aura, Metro-Aura, moonlit.aura -- apps/tagcache.c es byte-identico en
 * los tres), en la raiz del disco junto al marcador y al sello de
 * biblioteca, NUNCA dentro de un arbol /.rockbox: un cambio de firmware
 * (dos renombres, v10) no se lleva la base consigo y una reinstalacion
 * no la borra. Este header es el dueno de las rutas de /.aura del
 * contrato (AURA_SYNC_DIR en aura_sync.c); nadie mas deletrea esta. */
#define AURA_SHARED_DB_DIR "/.aura/tagcache"

/* D-340/D-341 (contrato v16): cache MAESTRA de imagenes compartida por
 * las tres familias -- albumes, fotos de artista y fotos -- en la raiz
 * del disco, propiedad del firmware activo, nunca borrada por Studio.
 * Formato y claves en aura_master_art_format.h; I/O en aura_master_art.c.
 * Mismo dueno de rutas que AURA_SHARED_DB_DIR: nadie mas deletrea esta. */
#define AURA_SHARED_ART_DIR         "/.aura/art"
#define AURA_SHARED_ART_ALBUMS_DIR  AURA_SHARED_ART_DIR "/albums"
#define AURA_SHARED_ART_ARTISTS_DIR AURA_SHARED_ART_DIR "/artists"
#define AURA_SHARED_ART_PHOTOS_DIR  AURA_SHARED_ART_DIR "/photos"

/* D-350 (contrato v18): version de FORMATO de la cache maestra y de las
 * L2 privadas derivadas de ella. Un entero decimal en un archivo suelto.
 * Al arrancar, cada familia lo lee: si falta o es menor que el suyo,
 * borra todo /.aura/art/{albums,artists,photos} y sus L2 privadas y
 * escribe su version. Existe porque una miniatura mal derivada por una
 * version anterior del codigo sobrevive para siempre -- la clave de
 * cache no cambia al corregir el DECODE, solo al cambiar la fuente.
 * Studio nunca lo toca ni lo borra, igual que el resto de /.aura/art. */
#define AURA_SHARED_ART_FORMAT_PATH AURA_SHARED_ART_DIR "/format.txt"
#define AURA_SHARED_ART_FORMAT      2

/* D-355/D-356 (contrato v19): ajustes compartidos entre familias
 * (bloqueo, brillo, apagado, idioma...) -- fuera de `.rockbox/` (que se
 * renombra al cambiar de familia, D-326), propiedad exclusiva de los
 * firmwares. Formato en CONTRATO-firmware-studio.md SS D.6 y en
 * aura_shared_settings.h (modulo puro que lo parsea/serializa). Mismo
 * dueno de rutas que las de arriba: nadie mas deletrea esta. */
#define AURA_SHARED_SETTINGS_PATH "/.aura/settings.cfg"

/* D-356: lee AURA_SHARED_SETTINGS_PATH y, si trae un `rev` mayor que
 * aura_settings.shared_rev_applied, aplica las 13 claves conocidas a
 * global_settings/aura_settings y sube shared_rev_applied. Mismo punto
 * que la hora (SS D.4): al arrancar y al volver de la pantalla USB
 * (aura_main_sync_after_disk_handoff(), D-293). No hace nada si el
 * archivo no existe o no trae la cabecera -- todo queda local, como
 * antes de v19 (regla 5 del contrato). */
void aura_shared_settings_apply_if_newer(void);

/* D-356: reescribe AURA_SHARED_SETTINGS_PATH completo con el estado
 * ACTUAL de global_settings/aura_settings, `rev+1`, `updated_by:
 * "aura"`, preservando las claves desconocidas que ya hubiera en el
 * archivo anterior (regla 2 del contrato) -- para eso relee el archivo
 * antes de reescribirlo.
 *
 * A diferencia de aura_settings_core_touched()/_flush() (D-351), esto
 * NO se difiere a un punto centralizado: escribe YA, en cada uno de los
 * ~10 sitios que cambian una de las 13 claves compartidas. D-351 difiere
 * porque settings_save() de Rockbox es perezoso por diseno (registra un
 * callback, el flush real espera hasta 30s salvo que se fuerce) -- esta
 * escritura, en cambio, ya es un unico open+write+rename sincrono
 * (aura_fsutil_write_all_atomic()); diferirla no evitaria ningun trabajo
 * de mas, y varios de esos sitios (aura_screenlock.c: Activar, Quitar
 * bloqueo) salen de su pantalla con aura_nav_pop() sin pasar nunca por
 * el BUTTON_MENU del despachador central -- un punto centralizado ahi
 * se las habria perdido. Se llama en el mismo lugar donde ya se guarda
 * el ajuste local (aura_settings_save()/aura_settings_core_touched()),
 * nunca en su lugar, y en Restablecer ajustes. */
void aura_shared_settings_write_current(void);

/* D-337: apunta global_settings.tagcache_db_path a AURA_SHARED_DB_DIR y
 * migra por rename() (sin copiar) una base previa a v15 que siga en
 * ROCKBOX_DIR (database_*.tcd + aura/db_stamp.txt) si el compartido no
 * tiene base todavia; si ya la tiene, la del arbol es peso muerto y se
 * borra. Se llama desde apps/main.c DESPUES de settings_load() y ANTES
 * de tagcache_init(), que es quien copia la ruta a tc_stat.db_path. */
void aura_sync_force_shared_db_path(void);

/* D-350: lee AURA_SHARED_ART_FORMAT_PATH y, si falta o quedo por debajo
 * de AURA_SHARED_ART_FORMAT, purga la cache maestra y las L2 privadas de
 * esta familia y escribe la version. Se llama UNA vez al arrancar, antes
 * de que nada lea arte (desde apps/main.c, junto a
 * aura_sync_force_shared_db_path()). Devuelve true si purgo. */
bool aura_sync_check_art_format(void);

/* D-329 (contrato v12) / D-337 (v15): sello de biblioteca.
 * /.aura/library-stamp solo cambia cuando un sync de Studio toca la
 * musica; el firmware anota en AURA_SHARED_DB_DIR/db_stamp.txt contra
 * que sello se construyo la base compartida. record: al terminar BIEN
 * una (re)construccion (marcador, manual de Ajustes, y el rebuild de
 * primer arranque de aura_music_db_ready()). switch_needs_rebuild: para
 * el cambio de firmware, DESPUES de los renombres -- crea el sello de
 * biblioteca si falta (la base compartida esta al dia: el saliente
 * acaba de correr con ella) y dice si el entrante necesita el marcador,
 * comparando SOLO el sello compartido (el arbol entrante ya no lleva
 * ninguno). */
void aura_sync_record_db_stamp(void);
/* Sella SOLO si la base compartida no tiene sello todavia (base migrada
 * de un arbol anterior a v15, o construida por un firmware que aun no
 * sella): una base usable al arrancar sin trabajo de sync pendiente
 * describe la biblioteca vigente -- mismo razonamiento que el arranque
 * en frio de switch_needs_rebuild(). */
void aura_sync_ensure_db_stamp(void);
/* D-339: lee el sello de la base compartida (sin salto de linea). Bytes
 * copiados, o <0 si no existe. El precache de caratulas (aura_music.c)
 * lo memoriza para no recorrer tagcache otra vez si nada cambio. */
int aura_sync_read_db_stamp(char *buf, size_t bufsz);
bool aura_sync_switch_needs_rebuild(void);

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
