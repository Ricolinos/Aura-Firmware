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
#include <string.h>
#include <stdio.h>

#include <stdlib.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "file.h"
#include "dir.h"
#include "string-extra.h"
#include "strnatcmp.h"
#include "recorder/bmp.h"
#include "recorder/jpeg_load.h"

#include "aura_photos.h"
#include "aura_widgets.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_status_bar_v2.h"

#define PHOTOS_DIR      "/Photos"
/* D-291: 200 -> 500 (limite del contrato Photos/ con Aura Studio,
 * PLAN-image-viewer.md §6.5); si hay mas, la lista muestra una fila
 * final inerte "...y N mas" en vez de truncar en silencio. */
#define MAX_PHOTOS      500
/* D-291: 64 -> 96 (contrato §6.4: nombres de hasta 95 bytes + '\0'). */
#define PHOTO_NAME_LEN  96

/* FORMAT_RESIZE necesita bastante mas que el bitmap final (D-026 en
 * DECISIONS.md); a pantalla completa (320x240x2) mas margen de sobra. */
#define VIEW_SCRATCH_SIZE (240 * 1024)

typedef struct {
    char filename[PHOTO_NAME_LEN];
    char display[PHOTO_NAME_LEN]; /* filename sin extension, AUDITORIA-01 A-05 */
    bool supported; /* jpg/bmp = true; png/gif listados pero no decodificables (D-028) */
} photo_item_t;

static photo_item_t s_photos[MAX_PHOTOS];
/* Entradas realmente almacenadas en s_photos[] (<= MAX_PHOTOS). */
static int s_photo_count = -1;
/* D-291: total de imagenes listables en /Photos, SIN el tope de
 * MAX_PHOTOS -- puede ser mayor que s_photo_count. Es lo que reporta
 * aura_photos_count() (el panel derecho del menu solo necesita saber
 * si hay 0 o mas) y lo que decide la fila final "...y N mas". */
static int s_photo_total_count = -1;
static int s_current_index = 0;

static unsigned char s_view_scratch[VIEW_SCRATCH_SIZE];
static int s_loaded_index = -1;
static bool s_loaded_ok = false;
static struct bitmap s_bm;

static bool has_ext(const char *name, const char *ext)
{
    size_t nlen = strlen(name), elen = strlen(ext);
    return nlen > elen && !strcasecmp(name + nlen - elen, ext);
}

static bool is_supported_image(const char *name)
{
    return has_ext(name, ".jpg") || has_ext(name, ".jpeg") || has_ext(name, ".bmp");
}

static bool is_listable_image(const char *name)
{
    return is_supported_image(name) || has_ext(name, ".png") || has_ext(name, ".gif");
}

/* Nombre para mostrar sin extension de archivo (AUDITORIA-01 A-05, doc
 * de diseno Principio 7: "nunca jerga" -- un nombre crudo de archivo con
 * extension es jerga tecnica, mismo principio que ya corrigio D-081 en
 * los nombres de playlist). Solo pela la extension para la lista; el
 * nombre real con extension sigue viviendo en filename para abrir el
 * archivo. */
static void strip_ext_for_display(const char *filename, char *out, size_t outsz)
{
    char *dot;

    strlcpy(out, filename, outsz);
    dot = strrchr(out, '.');
    if (dot)
        *dot = '\0';
}

/* D-291: orden natural insensible a mayusculas ("Foto 2" antes que
 * "Foto 10") sobre el nombre de display -- el orden fisico de la FAT
 * que readdir() daba antes no tenia relacion con lo que el usuario
 * espera ver. strnatcasecmp ya vive en firmware/common/, usado por
 * filetree.c/tagtree.c -- no es una dependencia nueva. */
static int compare_photo_display(const void *a, const void *b)
{
    const photo_item_t *pa = a, *pb = b;
    return strnatcasecmp(pa->display, pb->display);
}

static void ensure_photo_list(void)
{
    DIR *d;
    struct DIRENT *entry;

    if (s_photo_count >= 0)
        return;

    s_photo_count = 0;
    s_photo_total_count = 0;
    d = opendir(PHOTOS_DIR);
    if (!d)
        return;

    while ((entry = readdir(d)) != NULL)
    {
        if (!is_listable_image(entry->d_name))
            continue;
        s_photo_total_count++;
        if (s_photo_count >= MAX_PHOTOS)
            continue; /* se sigue contando para el total, sin guardar */
        strlcpy(s_photos[s_photo_count].filename, entry->d_name, PHOTO_NAME_LEN);
        strip_ext_for_display(entry->d_name, s_photos[s_photo_count].display, PHOTO_NAME_LEN);
        s_photos[s_photo_count].supported = is_supported_image(entry->d_name);
        s_photo_count++;
    }
    closedir(d);

    qsort(s_photos, s_photo_count, sizeof(s_photos[0]), compare_photo_display);
}

void aura_photos_invalidate(void)
{
    s_photo_count = -1;
    s_photo_total_count = -1;
    s_loaded_index = -1;
}

int aura_photos_count(void)
{
    ensure_photo_list();
    return s_photo_total_count;
}

static void draw_message(aura_str_id_t msg_id)
{
    int w, h;
    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, (A26_SCREEN_HEIGHT - h) / 2,
               (const unsigned char *)aura_str(msg_id));
}

/* D-291: variante de draw_message() con una segunda linea de ayuda,
 * mas chica y mas atenuada, debajo del mensaje principal -- para el
 * vacio de Fotos, que ahora dice donde resolverlo (AURA_STR_EMPTY_
 * PHOTOS_HINT) en vez de solo describir el vacio. */
static void draw_message_with_hint(aura_str_id_t msg_id, aura_str_id_t hint_id)
{
    int w1, h1, w2, h2, total_h, top;

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w1, &h1);
    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    lcd_getstringsize((const unsigned char *)aura_str(hint_id), &w2, &h2);

    total_h = h1 + A26_SPACING_SM + h2;
    top = (A26_SCREEN_HEIGHT - total_h) / 2;

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_putsxy((A26_SCREEN_WIDTH - w1) / 2, top, (const unsigned char *)aura_str(msg_id));

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    lcd_set_foreground(a26_color(A26_TEXT_TERTIARY));
    lcd_putsxy((A26_SCREEN_WIDTH - w2) / 2, top + h1 + A26_SPACING_SM,
               (const unsigned char *)aura_str(hint_id));
}

/* D-291: hay mas imagenes en /Photos de las que caben en MAX_PHOTOS --
 * se dice explicitamente en vez de truncar en silencio (ver contrato
 * §6.5 en PLAN-image-viewer.md). */
static bool has_more_row(void)
{
    return s_photo_total_count > s_photo_count;
}

static int display_row_count(void)
{
    return s_photo_count + (has_more_row() ? 1 : 0);
}

void aura_photos_draw(aura_nav_t *nav)
{
    int i;
    static aura_list_item_t items[MAX_PHOTOS + 1];
    static char s_more_label[48];

    ensure_photo_list();

    if (s_photo_count == 0)
    {
        /* AUDITORIA-01 A-15: "la barra nunca cambia de forma entre
         * pantallas" (doc SS5) -- este vacio era la unica pantalla del
         * sistema sin ella. */
        a26_shell_clear_screen();
        aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_PHOTOS));
        draw_message_with_hint(AURA_STR_EMPTY_PHOTOS, AURA_STR_EMPTY_PHOTOS_HINT);
        return;
    }

    for (i = 0; i < s_photo_count; i++)
    {
        items[i].label = s_photos[i].display;
        /* Sin icono por fila (AUDITORIA-01 A-06, anti-patron SS8: "icono
         * repetido sin variacion entre hermanos") -- el original tampoco
         * lo tiene en listas de contenido; una miniatura real por foto
         * es mejora futura, no un icono generico repetido N veces. */
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
        items[i].dimmed = 0;
    }
    if (has_more_row())
    {
        snprintf(s_more_label, sizeof(s_more_label), aura_str(AURA_STR_LIST_MORE_FMT),
                  s_photo_total_count - s_photo_count);
        items[i].label = s_more_label;
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
        items[i].dimmed = 1; /* fila presente pero inerte, no elegible */
    }
    aura_widgets_draw_list(aura_str(AURA_STR_PHOTOS), items, display_row_count(),
                            aura_nav_get_selection(nav));
}

void aura_photos_handle_button(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < display_row_count() - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        /* sel == s_photo_count solo puede ser la fila inerte "...y N
         * mas" (has_more_row()) -- nunca abre el visor. */
        if (sel >= 0 && sel < s_photo_count)
        {
            s_current_index = sel;
            aura_nav_push(nav, AURA_SCREEN_PHOTO_VIEWER);
        }
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

static void load_current_photo(void)
{
    char path[MAX_PATH];
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int ret;

    if (s_loaded_index == s_current_index)
        return;
    s_loaded_index = s_current_index;
    s_loaded_ok = false;

    if (!s_photos[s_current_index].supported)
        return;

    snprintf(path, sizeof(path), "%s/%s", PHOTOS_DIR, s_photos[s_current_index].filename);

    s_bm.width = A26_SCREEN_WIDTH;
    s_bm.height = A26_SCREEN_HEIGHT;
    s_bm.data = (char *)s_view_scratch;
#if (LCD_DEPTH > 1)
    s_bm.maskdata = NULL;
#endif

    if (has_ext(path, ".bmp"))
        ret = read_bmp_file(path, &s_bm, sizeof(s_view_scratch), format, NULL);
    else
        ret = read_jpeg_file(path, &s_bm, sizeof(s_view_scratch), format, NULL);

    s_loaded_ok = (ret > 0);
}

void aura_photo_viewer_draw(aura_nav_t *nav)
{
    (void)nav;

    a26_shell_clear_screen();

    if (s_photo_count == 0)
        return;

    load_current_photo();

    if (!s_photos[s_current_index].supported || !s_loaded_ok)
    {
        draw_message(AURA_STR_UNSUPPORTED_FORMAT);
        return;
    }

    lcd_bitmap((const fb_data *)s_view_scratch,
               (A26_SCREEN_WIDTH - s_bm.width) / 2,
               (A26_SCREEN_HEIGHT - s_bm.height) / 2,
               s_bm.width, s_bm.height);
}

void aura_photo_viewer_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_RIGHT:
        if (s_current_index < s_photo_count - 1)
            s_current_index++;
        break;
    case BUTTON_LEFT:
        if (s_current_index > 0)
            s_current_index--;
        break;
    case BUTTON_MENU:
    case BUTTON_SELECT:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
