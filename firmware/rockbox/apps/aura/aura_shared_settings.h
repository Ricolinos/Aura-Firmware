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
/* Ajustes compartidos entre familias -- CONTRATO-firmware-studio.md SS D.6
 * (D-355/D-356). `/.aura/settings.cfg` deja que cambiar de firmware
 * (Ajustes > Cambiar sistema, D-333) no signifique reconfigurar brillo,
 * bloqueo, apagado automatico e idioma desde cero: las tres familias leen
 * y escriben el MISMO archivo, fuera de `.rockbox/` (que se renombra al
 * cambiar de familia), con el mismo formato exacto -- el vector de
 * prueba del contrato es literal e identico en los tests host de las
 * tres.
 *
 * Modulo puro en C99, sin dependencias de Rockbox NI de aura_settings.h
 * (mismo criterio que aura_sync_marker.c/aura_style_manifest.c): compila
 * igual en el host (apps/aura/test/) que en el firmware. Los enums de
 * este archivo son el VOCABULARIO DEL CONTRATO -- estable entre
 * familias -- deliberadamente independientes de aura_lock_require_t/
 * aura_lang_t/aura_theme_id_t (aura_settings.h): esos son la
 * representacion INTERNA de Aura, que podria reordenarse por razones
 * propias sin que eso deba romper el contrato. La capa que sabe
 * traducir entre los dos vocabularios es aura_sync.c (impura), no este
 * modulo.
 *
 * Solo parsea/serializa texto -- no toca disco (aura_sync.c es quien
 * hace la lectura/escritura atomica real, ruta AURA_SHARED_SETTINGS_PATH
 * en aura_sync.h). */
#ifndef AURA_SHARED_SETTINGS_H
#define AURA_SHARED_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>

/* Cabecera obligatoria en la primera linea del archivo (contrato SS D.6).
 * Un archivo sin esta linea exacta se trata como si no existiera --
 * regla 5 del contrato: nunca se interpreta a medias. */
#define AURA_SHARED_SETTINGS_HEADER "# aura-shared-settings v1"

/* "moonlit" son 7 caracteres -- el mas largo de los tres nombres de
 * familia (contrato SS A bis). */
#define AURA_SHARED_SETTINGS_UPDATED_BY_LEN 8
#define AURA_SHARED_SETTINGS_PIN_LEN 5

/* Claves desconocidas que un firmware mas nuevo (o un Studio futuro)
 * pueda haber escrito: se preservan verbatim para la proxima reescritura
 * (regla 2 del contrato) en vez de perderse. Cota generosa -- hoy son
 * 13 claves conocidas, esto deja sitio para que el formato crezca varias
 * rondas mas antes de necesitar ampliarse. */
#define AURA_SHARED_SETTINGS_MAX_UNKNOWN 8
#define AURA_SHARED_SETTINGS_UNKNOWN_KEY_LEN 40
#define AURA_SHARED_SETTINGS_UNKNOWN_VALUE_LEN 64

/* Cotas de SANIDAD del modulo puro para las claves numericas -- no son
 * los limites REALES del hardware (esos son de Rockbox: p. ej.
 * MAX_BRIGHTNESS_SETTING varia por target, ipod6g.h lo fija en 0x3f).
 * La capa impura (aura_sync.c) vuelve a acotar contra esos limites
 * reales antes de aplicar; estas cotas solo evitan que un valor
 * disparatado (`brightness: 999`, el caso del vector de prueba) entre
 * siquiera como "presente" -- suficientemente generosas para no
 * rechazar nunca un valor real de ningun target de las tres familias. */
#define AURA_SHARED_SETTINGS_BRIGHTNESS_MAX 255
#define AURA_SHARED_SETTINGS_BACKLIGHT_TIMEOUT_MAX (24 * 3600)
#define AURA_SHARED_SETTINGS_IDLE_POWEROFF_MAX (24 * 60)
#define AURA_SHARED_SETTINGS_VOLUME_LIMIT_MIN (-100)
#define AURA_SHARED_SETTINGS_VOLUME_LIMIT_MAX 0

typedef enum {
    AURA_SHARED_LOCK_REQUIRE_HOLD = 0,
    AURA_SHARED_LOCK_REQUIRE_1MIN,
    AURA_SHARED_LOCK_REQUIRE_5MIN,
    AURA_SHARED_LOCK_REQUIRE_BOOT,
    AURA_SHARED_LOCK_REQUIRE_COUNT, /* tambien "ausente/invalido" */
} aura_shared_lock_require_t;

typedef enum {
    AURA_SHARED_REPLAYGAIN_OFF = 0,
    AURA_SHARED_REPLAYGAIN_TRACK,
    AURA_SHARED_REPLAYGAIN_ALBUM,
    AURA_SHARED_REPLAYGAIN_COUNT, /* tambien "ausente/invalido" */
} aura_shared_replaygain_t;

/* Los seis idiomas del contrato (SS D.1 del maestro de la ronda). Aura
 * solo entiende es/en hasta que D-357 (Fase 3 de esta misma ronda)
 * amplia aura_lang_t a los seis -- este enum YA los tiene todos porque
 * es el vocabulario del CONTRATO, no el de aura_lang_t: un firmware que
 * todavia no sabe mostrar ruso debe poder, aun asi, PARSEAR
 * `language: ru` como una clave conocida y valida (para no perderla ni
 * tratarla como clave desconocida) y decidir el no-aplicarla en su
 * propia capa de aplicacion. */
typedef enum {
    AURA_SHARED_LANG_ES = 0,
    AURA_SHARED_LANG_EN,
    AURA_SHARED_LANG_FR,
    AURA_SHARED_LANG_DE,
    AURA_SHARED_LANG_RU,
    AURA_SHARED_LANG_IT,
    AURA_SHARED_LANG_COUNT, /* tambien "ausente/invalido" */
} aura_shared_lang_t;

typedef enum {
    AURA_SHARED_APPEARANCE_DARK = 0,
    AURA_SHARED_APPEARANCE_LIGHT,
    AURA_SHARED_APPEARANCE_COUNT, /* tambien "ausente/invalido" */
} aura_shared_appearance_t;

typedef struct {
    char key[AURA_SHARED_SETTINGS_UNKNOWN_KEY_LEN];
    char value[AURA_SHARED_SETTINGS_UNKNOWN_VALUE_LEN];
} aura_shared_settings_unknown_t;

/* Cada campo numerico/enum usa un valor centinela para "ausente o fuera
 * de rango" (documentado junto a cada uno) -- nunca se distingue con un
 * booleano aparte, mismo criterio que aura_style_manifest_t. La capa
 * que aplica estos valores a aura_settings/global_settings decide, para
 * cada clave, si el centinela significa "no tocar este ajuste" (regla 2
 * del contrato: "un valor fuera de rango se ignora clave por clave"). */
typedef struct {
    long rev;                          /* -1 = ausente/invalido; valido >= 1 */
    char updated_by[AURA_SHARED_SETTINGS_UPDATED_BY_LEN]; /* "" si ausente; solo diagnostico */

    int screen_lock_enabled;           /* -1 = ausente/invalido; 0 o 1 */
    char screen_lock_pin[AURA_SHARED_SETTINGS_PIN_LEN]; /* "" si ausente/invalido; si no, 4 digitos ASCII */
    aura_shared_lock_require_t screen_lock_require; /* _COUNT = ausente/invalido */

    int brightness;                    /* -1 = ausente/invalido; valido 1..BRIGHTNESS_MAX */
    int backlight_timeout;             /* -2 = ausente; -1 = "nunca" (valido); valido >= 0 hasta TIMEOUT_MAX */
    int idle_poweroff;                 /* -1 = ausente/invalido; 0 = "nunca"; valido hasta POWEROFF_MAX */
    int keyclick;                      /* -1 = ausente/invalido; 0 o 1 */
    int volume_limit;                  /* AURA_SHARED_SETTINGS_VOLUME_LIMIT_ABSENT = ausente; valido en [MIN,MAX] */
    aura_shared_replaygain_t replaygain; /* _COUNT = ausente/invalido */

    aura_shared_lang_t language;       /* _COUNT = ausente/invalido */
    aura_shared_appearance_t appearance; /* _COUNT = ausente/invalido */

    aura_shared_settings_unknown_t unknown[AURA_SHARED_SETTINGS_MAX_UNKNOWN];
    int unknown_count;
} aura_shared_settings_t;

/* Fuera del rango real de dB (siempre <= 0): centinela de "ausente",
 * distinto de cualquier valor valido de volume_limit. */
#define AURA_SHARED_SETTINGS_VOLUME_LIMIT_ABSENT 1000

/* Deja `out` en "todo ausente" -- llamar antes de parsear o antes de
 * poblar a mano para un serialize() nuevo. */
void aura_shared_settings_init(aura_shared_settings_t *out);

typedef enum {
    /* Parseado bien -- puede tener 0 o mas claves ausentes/invalidas
     * (cada una se resuelve por su propio centinela, esto no es un
     * fallo del archivo entero). */
    AURA_SHARED_SETTINGS_OK = 0,
    /* Regla 5 del contrato: la primera linea no es EXACTAMENTE
     * AURA_SHARED_SETTINGS_HEADER. El archivo entero se trata como si
     * no existiera -- `out` queda igual que tras _init(). */
    AURA_SHARED_SETTINGS_NO_HEADER,
} aura_shared_settings_status_t;

/* Parsea el contenido COMPLETO del archivo (texto plano, lineas
 * "clave: valor", terminador \n o \r\n indistinto). Dos pasadas nunca
 * hacen falta: cada linea se resuelve sola. Un valor mal formado para
 * una clave conocida dejа esa clave en su centinela de "ausente" (no
 * aborta el parseo de las demas). Las lineas con clave desconocida (no
 * alguna de las 13 conocidas) se guardan verbatim en `out->unknown`
 * hasta AURA_SHARED_SETTINGS_MAX_UNKNOWN -- de ahi en adelante se
 * descartan en silencio (mismo criterio de degradacion que el resto del
 * arbol: preservar lo posible, nunca reventar por un archivo mas largo
 * de lo esperado). */
aura_shared_settings_status_t aura_shared_settings_parse(const char *text,
                                                          aura_shared_settings_t *out);

/* Serializa `m` completo -- TODOS los campos deben estar poblados con
 * valores validos (nunca centinelas: esta funcion es para cuando el
 * firmware escribe su propio estado actual, regla 3/4 del contrato, no
 * para volcar de vuelta un parseo a medias) mas las claves desconocidas
 * preservadas. Devuelve los bytes escritos (sin el NUL), o -1 si no
 * entraron en `bufsize` -- el llamador decide que hacer (mismo
 * contrato de retorno que aura_sync_marker_serialize()). */
int aura_shared_settings_serialize(const aura_shared_settings_t *m,
                                   char *buf, size_t bufsize);

/* Nombres de las claves de enum, en el orden del contrato -- usados por
 * el parser y por quien escriba un valor a mano (aura_sync.c). Publicos
 * porque un test host los necesita para verificar el vector A.3 sin
 * duplicar la tabla. NULL-terminados en su propio arreglo (ver .c). */
const char *aura_shared_lock_require_name(aura_shared_lock_require_t v);
const char *aura_shared_replaygain_name(aura_shared_replaygain_t v);
const char *aura_shared_lang_name(aura_shared_lang_t v);
const char *aura_shared_appearance_name(aura_shared_appearance_t v);

#endif /* AURA_SHARED_SETTINGS_H */
