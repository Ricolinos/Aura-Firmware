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
/* Parseo puro del manifiesto de un paquete de tema (theme.cfg) --
 * CONTRATO-formato-tema.md define el formato completo. Modulo en C99
 * sin dependencias de Rockbox (ni siquiera apple2026_shell.h, que ya
 * arrastra lcd.h) -- mismo criterio que aura_color.c/aura_motion.c:
 * compila igual en el host (apps/aura/test/) que en el firmware. No
 * toca disco ni conoce settings_parseline()/read_line() (esos viven en
 * apps/misc.c, no host-includable) -- el llamador real (aura_style.c)
 * ya le entrega cada linea partida en name/value, con las mismas
 * funciones que usa aura_settings.c.
 *
 * "Estilo" es el nombre de este modulo en el CODIGO para no confundirse
 * con aura_settings.theme (AURA_THEME_LIGHT/DARK, claro/oscuro -- un
 * concepto totalmente distinto). El formato de paquete, la UI y la
 * documentacion siguen llamandolo "tema" -- ver CONTRATO-formato-tema.md.
 */
#ifndef AURA_STYLE_MANIFEST_H
#define AURA_STYLE_MANIFEST_H

#include <stdbool.h>

/* Los 8 roles de paleta de un tema -- EXACTAMENTE los tokens de
 * a26_token_t (apple2026_shell.h) salvo A26_ACCENT: el acento es un
 * ajuste del USUARIO, nunca del tema (CONTRATO-formato-tema.md SS2).
 * Mismo orden que ese enum a proposito, para que aura_style.c copie
 * directo sin tabla de traduccion -- este modulo no incluye
 * apple2026_shell.h porque eso arrastraria lcd.h (Rockbox) a un
 * modulo que debe compilar en el host. */
typedef enum {
    AURA_STYLE_ROLE_SHELL_BG = 0,
    AURA_STYLE_ROLE_TEXT_PRIMARY,
    AURA_STYLE_ROLE_TEXT_SECONDARY,
    AURA_STYLE_ROLE_TEXT_TERTIARY,
    AURA_STYLE_ROLE_SHELL_RAIL,
    AURA_STYLE_ROLE_PROGRESS_FILL,
    AURA_STYLE_ROLE_PROGRESS_TRACK,
    AURA_STYLE_ROLE_SELECTION_FILL,
    AURA_STYLE_ROLE_COUNT,
} aura_style_role_t;

/* 32 caracteres + NUL (CONTRATO-formato-tema.md: "<id>: [a-z0-9-]{1,32}"). */
#define AURA_STYLE_ID_LEN   33
#define AURA_STYLE_NAME_LEN 33

/* theme_format que este firmware entiende. Un manifiesto con formato
 * mayor no se carga (fallback al default); ver CONTRATO-formato-tema.md
 * SS6. */
#define AURA_STYLE_FORMAT_SUPPORTED 1

typedef struct {
    int format;                 /* -1 = "theme_format" ausente del manifiesto */
    char id[AURA_STYLE_ID_LEN];
    char name[AURA_STYLE_NAME_LEN];
    bool has_id;
    bool has_name;
    /* rgb24 (0xRRGGBB) o -1 = ausente -- hereda del default compilado. */
    int palette_light[AURA_STYLE_ROLE_COUNT];
    int palette_dark[AURA_STYLE_ROLE_COUNT];
    int category_settings_gray;
    int category_video;
    int category_photos;
    int category_extras_yellow;
} aura_style_manifest_t;

/* Deja `m` en su estado "vacio" (todo ausente/-1) -- llamar antes de
 * aplicar lineas. */
void aura_style_manifest_init(aura_style_manifest_t *m);

/* Aplica UNA linea ya partida en name/value (mismo contrato de salida
 * que settings_parseline() de Rockbox: sin el ':', sin espacios en los
 * bordes). Claves desconocidas (theme_author, theme_license,
 * theme_redistributable, requires_firmware_min, accent_default,
 * accent_presets -- de Aura Studio o de una version futura del
 * formato) se ignoran en silencio, mismo criterio que
 * aura_settings_load(). Pura logica de asignacion, sin tocar disco. */
void aura_style_manifest_apply_line(aura_style_manifest_t *m,
                                     const char *name, const char *value);

/* True si `id` es seguro para usar como componente de una ruta de
 * disco: 1..32 caracteres [a-z0-9-], nunca vacio, y nunca "default"
 * (reservado para el tema compilado -- un paquete en disco con ese id
 * jamas se trata como un estilo instalado). El alfabeto permitido no
 * incluye '.', asi que cualquier intento de recorrido de ruta ("..")
 * ya queda rechazado por el filtro de caracteres, sin chequeo aparte. */
bool aura_style_id_is_valid(const char *id);

/* True si el manifiesto trae lo minimo para cargar: theme_format
 * presente y <= AURA_STYLE_FORMAT_SUPPORTED. NO valida que existan los
 * 14 archivos de fuente ni las mascaras de icono en disco -- eso
 * requiere tocar el sistema de archivos, lo hace aura_style.c. */
bool aura_style_manifest_is_loadable(const aura_style_manifest_t *m);

#endif /* AURA_STYLE_MANIFEST_H */
