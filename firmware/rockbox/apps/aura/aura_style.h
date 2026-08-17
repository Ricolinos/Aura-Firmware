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
/* Sistema de temas de Aura -- CONTRATO-formato-tema.md (raiz del repo)
 * define el formato completo, PLAN-themes-impl.md el diseno. Este
 * modulo:
 *  - carga/descarga las 14 fuentes del estilo activo (font_load()/
 *    font_unload() -- MAXUSERFONTS esta exactamente al limite, asi que
 *    cambiar de estilo es SIEMPRE descargar-y-recargar, nunca cargar
 *    al lado);
 *  - mantiene la tabla de paleta en runtime que a26_color() consulta;
 *  - resuelve rutas de iconos/fondos/tiles con fallback POR ARCHIVO al
 *    default si el estilo activo no trae ese archivo;
 *  - garantiza el fallback de seguridad: un estilo que no carga (o que
 *    deja de cargar a mitad de un intento) NUNCA deja el dispositivo
 *    sin fuentes -- revierte al estilo anterior, o al default si hace
 *    falta.
 *
 * "Estilo" es el nombre de este modulo en el CODIGO para no
 * confundirse con aura_settings.theme (AURA_THEME_LIGHT/DARK,
 * claro/oscuro -- un ajuste totalmente distinto, sin relacion). El
 * formato de paquete, la pantalla de Ajustes y toda la documentacion
 * siguen llamandolo "tema" -- CONTRATO-formato-tema.md.
 */
#ifndef AURA_STYLE_H
#define AURA_STYLE_H

#include <stdbool.h>
#include <stddef.h>

#include "recorder/bmp.h"

#include "aura_style_manifest.h"

#define AURA_STYLE_ID_DEFAULT "default"

/* Cota de estilos instalados que la pantalla "Estilo" puede mostrar a
 * la vez (MAX_MENU_ENTRIES de aura_screens.c es 32; se deja margen
 * para la fila fija "Aura"). */
#define AURA_STYLES_MAX 24

typedef struct {
    char id[AURA_STYLE_ID_LEN];
    char name[AURA_STYLE_NAME_LEN];
    /* false = fila inerte (formato incompatible, manifiesto ilegible,
     * o falta alguna de las 14 fuentes) -- se muestra atenuada, SELECT
     * no hace nada. */
    bool loadable;
} aura_style_entry_t;

/* Se llama UNA vez al arrancar, DESPUES de aura_settings_load() (que ya
 * trae aura_settings.style_id) -- reemplaza las 14 font_load() sueltas
 * que hacia a26_shell_init() antes. Intenta el estilo pedido; si falla,
 * cae al default (que siempre "carga", degradando por-fuente a
 * FONT_SYSFIXED si hiciera falta -- mismo comportamiento que el a26_shell_init()
 * original). Tambien inicializa la tabla de paleta runtime. */
void aura_style_boot(void);

/* Cambia el estilo activo en caliente: descarga las 14 fuentes
 * actuales, valida e intenta cargar `id`. Si `id` no es el default y
 * falla por cualquier motivo (manifiesto invalido, alguna fuente no
 * carga), REVIERTE al estilo que estaba activo antes de esta llamada
 * -- nunca deja el sistema sin fuentes utilizables. No persiste nada
 * en aura.cfg -- el llamador (aura_screens.c) decide cuando guardar
 * aura_settings.style_id, y solo si esta funcion devuelve true.
 * Devuelve true si `id` quedo realmente activo. */
bool aura_style_activate(const char *id);

/* Id del estilo REALMENTE activo ahora mismo (puede diferir de
 * aura_settings.style_id si el arranque tuvo que revertir al default). */
const char *aura_style_active_id(void);

/* True si el estilo activo es el default compilado. */
bool aura_style_is_default(void);

/* Cambia cada vez que aura_style_activate()/aura_style_boot() terminan
 * (exito o reversion) -- para que cachés de imagenes por nombre (fondo
 * del panel, tile-icon del badge) sepan invalidarse ademas de cuando
 * cambia el nombre pedido. */
unsigned aura_style_generation(void);

/* Rellena `out` (hasta `max` entradas) con: fila 0 fija "default"/
 * "Aura" (siempre presente, siempre loadable=true), seguida de un
 * renglon por cada AURA_STYLES_DIR "/<id>/theme.cfg" legible, hasta
 * `max`. Devuelve cuantas se llenaron. Plantilla:
 * aura_music_list_playlists() (aura_music.c). */
int aura_style_scan(aura_style_entry_t *out, int max);

/* Elimina el directorio de un estilo instalado (nunca "default"). NO
 * revierte el estilo activo -- si `id` era el activo, el llamador debe
 * llamar aura_style_activate(AURA_STYLE_ID_DEFAULT) primero. Devuelve
 * false si `id` es invalido, es "default", o no se pudo borrar. */
bool aura_style_delete(const char *id);

/* Color de paleta ya resuelto contra el estilo activo (hereda del
 * default compilado lo que el tema no traiga), para el modo `dark`
 * dado. `token_index` son los valores ordinales de a26_token_t
 * (apple2026_shell.h) -- se pasa como int en vez de incluir ese header
 * aca, para no acoplar este modulo a la enumeracion de la capa visual.
 * Formato nativo del LCD (LCD_RGBPACK). UNICO consumidor: a26_color(),
 * que delega aca. */
unsigned aura_style_palette_color(int token_index, bool dark);

/* Colores FIJOS de categoria (Ajustes/Video/Fotos/el amarillo de
 * Extras), 0xRRGGBB -- ya resueltos contra el estilo activo. UNICO
 * consumidor: aura_category_gradient() (apple2026_shell.c). */
unsigned aura_style_category_settings_gray_rgb24(void);
unsigned aura_style_category_video_rgb24(void);
unsigned aura_style_category_photos_rgb24(void);
unsigned aura_style_category_extras_yellow_rgb24(void);

/* Intenta leer un BMP bajo el estilo activo, en la subruta relativa
 * `rel` (p. ej. "masks/alarm-12.bmp", "light/alarm-12-on.bmp",
 * "backgrounds/pink.bmp", "tile-icons/aura_badge-light.bmp") -- NUNCA
 * incluye el "ICON_DIR/aura" ni el "AURA_STYLES_DIR/<id>/icons" del
 * prefijo, eso lo resuelve esta funcion. Si falla y el estilo activo
 * no es el default, reintenta la MISMA `rel` bajo el default (fallback
 * por archivo, CONTRATO-formato-tema.md: los BMP horneados y los
 * fondos/tiles son opcionales en un tema). Mismo contrato de retorno
 * que read_bmp_file(): >0 (ancho de la imagen) si tuvo exito. */
int aura_style_read_icon_bmp(const char *rel, struct bitmap *bm, size_t bufsize);

#endif /* AURA_STYLE_H */
