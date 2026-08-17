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
#include <stdint.h>

#include "kernel.h" /* current_tick, riel de scroll (D-291) */
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
#include "aura_scroll_indicator.h"
#include "aura_art.h" /* cache .pfraw generico -- D-291, compartido con aura_albumart.c */
#include "aura_transitions.h" /* Fade-Slide region entre fotos -- D-291 */

#define PHOTOS_DIR      "/Photos"
/* D-291: 200 -> 500 (limite del contrato Photos/ con Aura Studio,
 * PLAN-image-viewer.md §6.5); si hay mas, la lista muestra una fila
 * final inerte "...y N mas" en vez de truncar en silencio. */
#define MAX_PHOTOS      500
/* D-291: 64 -> 96 (contrato §6.4: nombres de hasta 95 bytes + '\0'). */
#define PHOTO_NAME_LEN  96

/* FORMAT_RESIZE necesita bastante mas que el bitmap final (D-026 en
 * DECISIONS.md); a pantalla completa (320x240x2) mas margen de sobra.
 * D-291: tambien es el buffer de trabajo de las miniaturas de la lista
 * (photo_thumb_decode_and_cache()) -- lista y visor nunca estan
 * activos a la vez, asi que comparten el mismo buffer estatico en vez
 * de duplicar 240KB; ver la nota sobre s_loaded_index mas abajo. */
#define VIEW_SCRATCH_SIZE (240 * 1024)

/* D-291: tope de tamano de fuente antes de decodificar -- el costo de
 * read_jpeg_file()/read_bmp_file() con FORMAT_RESIZE es CONSTANTE en
 * memoria (escalan en la IDCT/remuestrean por linea, jamas materializan
 * la imagen fuente completa, PLAN-image-viewer.md §4), asi que este
 * tope es por TIEMPO de CPU, no por RAM -- una fuente de 12 MP tarda
 * varios segundos en decodificar en el S5L8702 aunque quepa de sobra.
 * 12 megapixeles en el sentido decimal habitual de resolucion de
 * camara (no 2^20). Aplica tanto al visor de pantalla completa como a
 * las miniaturas de la lista (mismo buffer, mismo limite de tiempo). */
#define PHOTO_MAX_SIDE          4096
#define PHOTO_MAX_PIXELS        (12L * 1000L * 1000L)
/* Unico caso en que la decodificacion del VISOR es perceptible (D-291
 * §5.3): por debajo de esto (las fotos que produce Aura Studio,
 * <=640px) no hace falta avisar "Cargando...". */
#define PHOTO_LOADING_INDICATOR_SIDE 640

/* D-291: lista con miniaturas reales -- mismas medidas que la lista de
 * Álbumes (ALBUM_ROW_H/ALBUM_ART_SIZE en aura_screens.c, D-221): filas
 * de 54px (218px utiles / 4 = 54, deja 2px de sobra antes del riel de
 * scroll), miniatura de 48px. Reusar el mismo numero en vez de inventar
 * uno propio -- es el unico precedente de lista de contenido con
 * miniatura real en todo el sistema. */
#define PHOTO_ART_SIZE   48
#define PHOTO_ROW_H      54
#define PHOTO_LIST_TOP   (A26_LAYOUT_STATUSBAR_HEIGHT + 2)
#define PHOTO_ART_X      A26_LAYOUT_LIST_INSET
#define PHOTO_TEXT_GAP   A26_SPACING_MD
#define PHOTO_VISIBLE    4

/* Mismo valor que aura_albumart.c/aura_settings.c/aura_manifest.c --
 * sin header compartido para esto en el proyecto, cada archivo lo
 * redefine igual (precedente ya establecido). */
#define AURA_DIR          ROCKBOX_DIR "/aura"
#define PHOTO_CACHE_DIR   AURA_DIR "/photocache"

typedef struct {
    char filename[PHOTO_NAME_LEN];
    char display[PHOTO_NAME_LEN]; /* filename sin extension, AUDITORIA-01 A-05 */
    bool supported; /* jpg/bmp = true; png/gif listados pero no decodificables (D-028) */
    long mtime; /* D-291: llave secundaria del cache de miniaturas -- ver photo_pfraw_path() */
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

typedef enum {
    JPEG_PROBE_UNKNOWN = 0, /* cabecera no reconocida -- se deja decidir al decoder, como antes */
    JPEG_PROBE_BASELINE,
    JPEG_PROBE_UNSUPPORTED, /* SOF progresivo/aritmetico/etc (D-028) */
} jpeg_probe_t;

/* D-291: lee SOLO los marcadores JPEG hasta encontrar el SOFn -- nunca
 * decodifica pixeles. Formato JPEG: 0xFFD8 (SOI), luego una cadena de
 * marcadores 0xFFxx; los que llevan payload traen su longitud (2 bytes
 * big-endian, cuenta la longitud misma) justo despues del codigo de
 * marcador. SOF0/SOF1 (baseline) traen precision(1)+alto(2 BE)+
 * ancho(2 BE) al inicio de su payload; SOF2 en adelante es progresivo/
 * aritmetico -- Rockbox no lo decodifica (D-028), asi que ni vale la
 * pena intentarlo. Si la cabecera no se reconoce (formato raro, EOF
 * antes de tiempo), se devuelve JPEG_PROBE_UNKNOWN y el llamador cae al
 * comportamiento de siempre: se intenta decodificar igual. */
static jpeg_probe_t probe_jpeg_dimensions(const char *path, int *out_w, int *out_h)
{
    int fd;
    unsigned char marker[2];
    jpeg_probe_t result = JPEG_PROBE_UNKNOWN;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return JPEG_PROBE_UNKNOWN;

    if (read(fd, marker, 2) != 2 || marker[0] != 0xFF || marker[1] != 0xD8)
    {
        close(fd);
        return JPEG_PROBE_UNKNOWN;
    }

    for (;;)
    {
        unsigned char lenbuf[2];
        unsigned char sofbuf[5];
        int len;

        if (read(fd, marker, 2) != 2 || marker[0] != 0xFF)
            break;
        while (marker[1] == 0xFF) /* bytes de relleno antes del codigo real */
        {
            if (read(fd, &marker[1], 1) != 1)
                break;
        }

        if (marker[1] == 0x01 || (marker[1] >= 0xD0 && marker[1] <= 0xD7))
            continue; /* TEM/RSTn -- sin campo de longitud */
        if (marker[1] == 0xD9 || marker[1] == 0xDA)
            break; /* EOI/SOS sin haber visto un SOFn -- se abandona */

        if (read(fd, lenbuf, 2) != 2)
            break;
        len = (lenbuf[0] << 8) | lenbuf[1];

        if (marker[1] == 0xC0 || marker[1] == 0xC1) /* SOF0/SOF1 baseline */
        {
            if (read(fd, sofbuf, 5) == 5)
            {
                *out_h = (sofbuf[1] << 8) | sofbuf[2];
                *out_w = (sofbuf[3] << 8) | sofbuf[4];
                result = JPEG_PROBE_BASELINE;
            }
            break;
        }
        if (marker[1] >= 0xC2 && marker[1] <= 0xCF && marker[1] != 0xC4 && marker[1] != 0xC8)
        {
            /* SOF2 progresivo, SOF3 lossless, SOF5-7/9-11/13-15 -- ver
             * D-028: Rockbox solo decodifica SOF0/SOF1. 0xC4 (DHT) y
             * 0xC8 (JPG, reservado) no son SOFn. */
            result = JPEG_PROBE_UNSUPPORTED;
            break;
        }

        if (len < 2 || lseek(fd, len - 2, SEEK_CUR) < 0)
            break; /* saltar el resto del payload de este marcador */
    }

    close(fd);
    return result;
}

/* D-291: cabecera BMP -- BITMAPFILEHEADER (14 bytes) + los primeros 12
 * bytes de BITMAPINFOHEADER, que ya traen ancho/alto (offsets 18/22,
 * int32 little-endian; alto puede venir negativo para bitmaps
 * top-down). Nunca decodifica pixeles. */
static bool probe_bmp_dimensions(const char *path, int *out_w, int *out_h)
{
    int fd;
    unsigned char hdr[26];
    int w, h;
    bool ok;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;
    ok = (read(fd, hdr, sizeof(hdr)) == (int)sizeof(hdr) && hdr[0] == 'B' && hdr[1] == 'M');
    close(fd);
    if (!ok)
        return false;

    w = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);
    h = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);
    *out_w = (w < 0) ? -w : w;
    *out_h = (h < 0) ? -h : h;
    return true;
}

/* D-291: true si (w,h) esta por encima del tope de tiempo de
 * decodificacion (PHOTO_MAX_SIDE/PHOTO_MAX_PIXELS) -- compartido entre
 * el visor (probe_current_photo()) y las miniaturas de la lista
 * (photo_thumb_decode_and_cache()), mismo criterio en los dos. */
static bool photo_dims_too_large(int w, int h)
{
    return w > PHOTO_MAX_SIDE || h > PHOTO_MAX_SIDE
           || (long)w * (long)h > PHOTO_MAX_PIXELS;
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
        struct dirinfo info;

        if (!is_listable_image(entry->d_name))
            continue;
        s_photo_total_count++;
        if (s_photo_count >= MAX_PHOTOS)
            continue; /* se sigue contando para el total, sin guardar */
        info = dir_get_info(d, entry);
        strlcpy(s_photos[s_photo_count].filename, entry->d_name, PHOTO_NAME_LEN);
        strip_ext_for_display(entry->d_name, s_photos[s_photo_count].display, PHOTO_NAME_LEN);
        s_photos[s_photo_count].supported = is_supported_image(entry->d_name);
        s_photos[s_photo_count].mtime = (long)info.mtime;
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

/* -- Miniaturas de la lista (D-291) -----------------------------------------
 *
 * Mismo pipeline .pfraw que aura_albumart.c (T3.2(a), ahora compartido
 * via aura_art.c): decodificar una vez, cachear en disco, transponer +
 * redondear esquinas UNA sola vez. Llave = nombre de archivo (unico
 * dentro de /Photos) + tamano; `extra` = mtime del archivo fuente, para
 * que un re-sync con el mismo nombre pero contenido distinto invalide
 * la miniatura vieja (aura_albumart.c no necesita esto -- su llave ya
 * es unica por archivo real). */

static void photo_pfraw_path(const char *filename, char *out, size_t outsz)
{
    snprintf(out, outsz, "%s/%s-%d.pfraw", PHOTO_CACHE_DIR, filename, PHOTO_ART_SIZE);
}

static void ensure_photo_cache_dir(void)
{
    if (!dir_exists(AURA_DIR))
        mkdir(AURA_DIR);
    if (!dir_exists(PHOTO_CACHE_DIR))
        mkdir(PHOTO_CACHE_DIR);
}

/* Decodifica la foto `idx` a una miniatura PHOTO_ART_SIZE x
 * PHOTO_ART_SIZE, la compone centrada sobre un tile de fondo (sin
 * distorsionar aspecto -- el decoder no tiene un modo "llenar
 * recortando", asi que se deja letterbox/pillarbox como el visor de
 * pantalla completa, mismo criterio), la transpone+enmascara y la deja
 * en `out_transposed` (reservado por el llamador). Si la fuente supera
 * el tope de tamano (mismo PHOTO_MAX_SIDE/PIXELS que el visor) o es un
 * formato no soportado, compone solo el tile de fondo -- placeholder
 * honesto, nunca intenta decodificar algo que va a tardar segundos
 * solo para una miniatura de 48px.
 *
 * Usa s_view_scratch como espacio de trabajo del decoder -- el mismo
 * buffer que load_current_photo() usa para el visor de pantalla
 * completa. Lista y visor nunca estan activos a la vez (son pantallas
 * distintas en la navegacion), asi que compartirlo evita duplicar
 * 240KB estaticos; el precio es que hay que invalidar s_loaded_index
 * aca (si no, volver al visor sobre una foto ya "cargada" mostraria la
 * miniatura mas reciente en vez de redecodificar). */
static void photo_thumb_decode_and_cache(int idx, fb_data *out_transposed)
{
    static fb_data flat[PHOTO_ART_SIZE * PHOTO_ART_SIZE];
    char src_path[MAX_PATH];
    char cache_path[MAX_PATH];
    unsigned bg = a26_color(A26_SHELL_BG);
    struct bitmap bm;
    bool ok = false;
    int i;

    snprintf(src_path, sizeof(src_path), "%s/%s", PHOTOS_DIR, s_photos[idx].filename);

    if (s_photos[idx].supported)
    {
        bool skip = false;
        int w = 0, h = 0;

        if (has_ext(src_path, ".bmp"))
        {
            if (probe_bmp_dimensions(src_path, &w, &h) && photo_dims_too_large(w, h))
                skip = true;
        }
        else
        {
            jpeg_probe_t r = probe_jpeg_dimensions(src_path, &w, &h);
            if (r == JPEG_PROBE_UNSUPPORTED)
                skip = true;
            else if (r == JPEG_PROBE_BASELINE && photo_dims_too_large(w, h))
                skip = true;
        }

        if (!skip)
        {
            int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
            int ret;

            bm.width = PHOTO_ART_SIZE;
            bm.height = PHOTO_ART_SIZE;
            bm.data = (char *)s_view_scratch;
#if (LCD_DEPTH > 1)
            bm.maskdata = NULL;
#endif
            if (has_ext(src_path, ".bmp"))
                ret = read_bmp_file(src_path, &bm, sizeof(s_view_scratch), format, NULL);
            else
                ret = read_jpeg_file(src_path, &bm, sizeof(s_view_scratch), format, NULL);
            ok = ret > 0;
        }
    }

    /* Ver comentario grande arriba de la funcion -- s_view_scratch
     * queda con la miniatura, no con lo que el visor pudiera tener
     * cargado. */
    s_loaded_index = -1;

    for (i = 0; i < PHOTO_ART_SIZE * PHOTO_ART_SIZE; i++)
        flat[i] = bg;

    if (ok)
    {
        const fb_data *decoded = (const fb_data *)s_view_scratch;
        int ox = (PHOTO_ART_SIZE - bm.width) / 2;
        int oy = (PHOTO_ART_SIZE - bm.height) / 2;
        int row, col;

        for (row = 0; row < bm.height; row++)
            for (col = 0; col < bm.width; col++)
                flat[(oy + row) * PHOTO_ART_SIZE + (ox + col)] =
                    decoded[row * bm.width + col];
    }

    aura_art_transpose(flat, out_transposed, PHOTO_ART_SIZE);
    aura_art_mask_corners_transposed(out_transposed, PHOTO_ART_SIZE,
                                      A26_LAYOUT_CORNER_RADIUS_CARD, bg);

    /* Se cachea tambien el placeholder (ok==false) -- evita re-probar
     * la misma foto demasiado-grande/no-soportada en cada redibujado
     * mientras siga visible en la ventana de la lista. */
    ensure_photo_cache_dir();
    photo_pfraw_path(s_photos[idx].filename, cache_path, sizeof(cache_path));
    aura_art_write_pfraw(cache_path, PHOTO_ART_SIZE, A26_LAYOUT_CORNER_RADIUS_CARD,
                          (int32_t)s_photos[idx].mtime, out_transposed);
}

/* Blitea la miniatura de la foto `idx` en (x,y) -- mismo mecanismo por
 * columnas que draw_album_thumb() (aura_screens.c). */
static void draw_photos_thumb(int x, int y, int idx)
{
    static fb_data thumb[PHOTO_ART_SIZE * PHOTO_ART_SIZE];
    static fb_data col_buf[PHOTO_ART_SIZE];
    char cache_path[MAX_PATH];
    int col, row;

    photo_pfraw_path(s_photos[idx].filename, cache_path, sizeof(cache_path));

    if (!aura_art_read_pfraw(cache_path, PHOTO_ART_SIZE, A26_LAYOUT_CORNER_RADIUS_CARD,
                              (int32_t)s_photos[idx].mtime, thumb))
        photo_thumb_decode_and_cache(idx, thumb);

    for (col = 0; col < PHOTO_ART_SIZE; col++)
    {
        for (row = 0; row < PHOTO_ART_SIZE; row++)
            col_buf[row] = thumb[col * PHOTO_ART_SIZE + row];
        lcd_bitmap(col_buf, x + col, y, 1, PHOTO_ART_SIZE);
    }
}

/* -- Lista "Todas las fotos" (D-291) ----------------------------------------
 *
 * Renderizador dedicado, mismo patron que draw_album_list()
 * (aura_screens.c, unica lista de CONTENIDO con miniatura real hasta
 * ahora) -- filas de PHOTO_ROW_H con PHOTO_ART_SIZE de miniatura,
 * pastilla de seleccion, riel de scroll. La fila final "...y N mas"
 * (has_more_row()) no tiene miniatura -- se distingue visualmente
 * (texto atenuado, sin thumb) ademas de ser inerte (aura_photos_
 * handle_button() ya la excluye de SELECT). */
static long s_photos_activity_since = 0;
static int s_photos_last_selected = -1;

static void draw_photos_list(aura_nav_t *nav)
{
    int selected = aura_nav_get_selection(nav);
    int count = display_row_count();
    int visible = PHOTO_VISIBLE;
    int first = 0;
    int i, w, h;
    static char s_more_label[48];

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_PHOTOS));

    if (count > visible)
    {
        first = selected - visible / 2;
        if (first < 0) first = 0;
        if (first > count - visible) first = count - visible;
    }

    if (selected != s_photos_last_selected)
    {
        s_photos_last_selected = selected;
        s_photos_activity_since = current_tick;
    }

    if (selected >= first && selected < first + visible)
    {
        int sel_y = PHOTO_LIST_TOP + (selected - first) * PHOTO_ROW_H;
        a26_shell_fill_rounded_rect(A26_LAYOUT_LIST_INSET, sel_y,
                                     A26_SCREEN_WIDTH - 2 * A26_LAYOUT_LIST_INSET,
                                     PHOTO_ROW_H, A26_LAYOUT_CORNER_RADIUS_PILL,
                                     a26_color(A26_SELECTION_FILL),
                                     a26_color(A26_SHELL_BG));
    }

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    for (i = first; i < count && i < first + visible; i++)
    {
        int row_y = PHOTO_LIST_TOP + (i - first) * PHOTO_ROW_H;
        int text_x = PHOTO_ART_X + PHOTO_ART_SIZE + PHOTO_TEXT_GAP;
        bool is_sel = (i == selected);
        bool is_more = (i == s_photo_count); /* fila inerte "...y N mas" */

        lcd_getstringsize((const unsigned char *)"Ay", &w, &h);

        if (is_more)
        {
            lcd_set_foreground(a26_color(A26_TEXT_TERTIARY));
            snprintf(s_more_label, sizeof(s_more_label), aura_str(AURA_STR_LIST_MORE_FMT),
                      s_photo_total_count - s_photo_count);
            aura_widgets_puts_clipped(PHOTO_ART_X, row_y + (PHOTO_ROW_H - h) / 2,
                                       A26_SCREEN_WIDTH - PHOTO_ART_X - A26_LAYOUT_LIST_INSET,
                                       s_more_label);
            continue;
        }

        draw_photos_thumb(PHOTO_ART_X, row_y + (PHOTO_ROW_H - PHOTO_ART_SIZE) / 2, i);

        lcd_set_foreground(is_sel ? a26_color(A26_ACCENT) : a26_color(A26_TEXT_PRIMARY));
        aura_widgets_puts_clipped(text_x, row_y + (PHOTO_ROW_H - h) / 2,
                                   A26_SCREEN_WIDTH - text_x - A26_LAYOUT_LIST_INSET,
                                   s_photos[i].display);
    }

    aura_scroll_indicator_draw(A26_SCREEN_WIDTH, PHOTO_LIST_TOP,
                                PHOTO_VISIBLE * PHOTO_ROW_H, selected, count,
                                (current_tick - s_photos_activity_since) * 1000L / HZ,
                                a26_color(A26_SHELL_BG), a26_color(A26_TEXT_TERTIARY));
}

void aura_photos_draw(aura_nav_t *nav)
{
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

    draw_photos_list(nav);
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

typedef enum {
    PHOTO_PROBE_OK = 0,   /* decodificable -- dimensiones conocidas o no */
    PHOTO_PROBE_TOO_LARGE,
    PHOTO_PROBE_UNSUPPORTED, /* extension no soportada o JPEG progresivo/etc */
} photo_probe_t;

static int s_probed_index = -1;
static photo_probe_t s_probe_result = PHOTO_PROBE_OK;
static bool s_probe_needs_indicator = false;

/* D-291: fase barata (solo cabeceras, nunca pixeles) que decide ANTES
 * de decodificar si la foto cabe en el tope, si es un formato que ya
 * se sabe que va a fallar (JPEG progresivo), y si el visor debe avisar
 * "Cargando..." antes de bloquear en la decodificacion real -- todo
 * esto tiene que resolverse ANTES de llamar a load_current_photo(),
 * que es la que bloquea. Idempotente por indice (s_probed_index),
 * igual que load_current_photo() lo es via s_loaded_index. */
static void probe_current_photo(void)
{
    char path[MAX_PATH];
    int w = 0, h = 0;
    bool have_dims = false;

    s_probed_index = s_current_index;
    s_probe_result = PHOTO_PROBE_OK;
    s_probe_needs_indicator = false;

    if (!s_photos[s_current_index].supported)
    {
        s_probe_result = PHOTO_PROBE_UNSUPPORTED;
        return;
    }

    snprintf(path, sizeof(path), "%s/%s", PHOTOS_DIR, s_photos[s_current_index].filename);

    if (has_ext(path, ".bmp"))
    {
        have_dims = probe_bmp_dimensions(path, &w, &h);
    }
    else
    {
        jpeg_probe_t r = probe_jpeg_dimensions(path, &w, &h);
        if (r == JPEG_PROBE_UNSUPPORTED)
        {
            s_probe_result = PHOTO_PROBE_UNSUPPORTED;
            return;
        }
        have_dims = (r == JPEG_PROBE_BASELINE);
    }

    if (!have_dims)
        return; /* cabecera no reconocida -- se deja decidir al decoder, como siempre */

    if (photo_dims_too_large(w, h))
    {
        s_probe_result = PHOTO_PROBE_TOO_LARGE;
        return;
    }
    s_probe_needs_indicator = (w > PHOTO_LOADING_INDICATOR_SIDE || h > PHOTO_LOADING_INDICATOR_SIDE);
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

    if (s_probed_index != s_current_index)
        probe_current_photo();

    if (s_probe_result == PHOTO_PROBE_TOO_LARGE)
    {
        draw_message(AURA_STR_PHOTO_TOO_LARGE);
        return;
    }
    if (s_probe_result == PHOTO_PROBE_UNSUPPORTED)
    {
        draw_message(AURA_STR_UNSUPPORTED_FORMAT);
        return;
    }

    if (s_loaded_index != s_current_index)
    {
        /* Solo se avisa cuando de verdad se va a notar (D-291 §5.3) --
         * hay que forzar un lcd_update() aca mismo porque
         * load_current_photo() bloquea; el ciclo normal de
         * aura_main.c solo vuelca a LCD DESPUES de que este draw()
         * completo regrese. */
        if (s_probe_needs_indicator)
        {
            draw_message(AURA_STR_LOADING);
            lcd_update();
        }
        load_current_photo();
    }

    if (!s_loaded_ok)
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
    /* D-291 §5.3: la rueda es la navegacion PRIMARIA entre fotos;
     * LEFT/RIGHT se conservan como atajo (mismo destino, misma
     * transicion) -- no se retiran, el encargo solo pedia agregar la
     * rueda. `Fade-Slide` variante de region (D-283,
     * aura_transition_fade_slide_region()) DESPUES de actualizar
     * s_current_index -- mismo contrato que handle_about(): la
     * transicion prerrenderiza el DESTINO llamando aura_screens_draw(),
     * que lee s_current_index para decidir que foto dibujar. Region =
     * pantalla completa (el visor no tiene StatusBar que excluir). */
    case BUTTON_SCROLL_FWD:
    case BUTTON_RIGHT:
        if (s_current_index < s_photo_count - 1)
        {
            s_current_index++;
            aura_transition_fade_slide_region(nav, 0, 0, A26_SCREEN_WIDTH, A26_SCREEN_HEIGHT, 1);
        }
        break;
    case BUTTON_SCROLL_BACK:
    case BUTTON_LEFT:
        if (s_current_index > 0)
        {
            s_current_index--;
            aura_transition_fade_slide_region(nav, 0, 0, A26_SCREEN_WIDTH, A26_SCREEN_HEIGHT, -1);
        }
        break;
    case BUTTON_MENU:
    case BUTTON_SELECT:
        /* D-291 §5.3: la lista recupera la seleccion de la foto que se
         * estaba viendo, no la que tenia al entrar -- aura_nav_pop()
         * PRIMERO (decrementa depth), aura_nav_set_selection() DESPUES
         * (para que depth-1 ya apunte al slot de la LISTA, no al del
         * propio visor -- ver aura_nav_set_selection()/aura_nav.c). */
        aura_nav_pop(nav);
        aura_nav_set_selection(nav, s_current_index);
        break;
    default:
        break;
    }
}
