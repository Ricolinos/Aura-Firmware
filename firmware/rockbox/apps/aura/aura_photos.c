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
#include "aura_media_categories.h" /* indice opcional Fotos/Imagenes/IA -- D-316 */
#include "aura_screens.h" /* aura_wheel_advance() -- D-323, dinamica real de la rueda */

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

/* D-323 (encargo del dueño, 2026-08-19: "una interfaz a pantalla
 * completa, cuadrícula sin nombres, para desplazarnos rápido -- muy
 * similar a la del iPod Classic original, donde sí se vea la barra de
 * estado"): reemplaza la lista con nombre de D-291 -- item "Pendiente
 * de definir" de componentes/photo-viewer.md ("Rejilla de miniaturas
 * (2D) en vez de lista -- el original la usa"). Miniatura: mismo
 * PHOTO_ART_SIZE de siempre (reusa photo_thumb_decode_and_cache()/
 * draw_photos_thumb() sin cambios, mismo cache .pfraw -- ninguna
 * pantalla usa ambos tamaños a la vez, no hace falta una segunda
 * llave). Celda CUADRADA de 55px: 5 columnas (275px, centradas --
 * sobran ~22px de cada lado, de sobra para el riel de scroll de 4px) x
 * 4 filas (220px, exacto el alto útil bajo la barra de estado, cero
 * sobra) -- ambos numeros salen de A26_SCREEN_WIDTH/A26_SCREEN_HEIGHT-
 * A26_LAYOUT_STATUSBAR_HEIGHT, no son arbitrarios. */
#define PHOTO_ART_SIZE     48
#define PHOTO_GRID_COLS    5
#define PHOTO_GRID_ROWS    4
#define PHOTO_GRID_CELL    55
#define PHOTO_GRID_TOP     A26_LAYOUT_STATUSBAR_HEIGHT
#define PHOTO_GRID_LEFT    ((A26_SCREEN_WIDTH - PHOTO_GRID_COLS * PHOTO_GRID_CELL) / 2)

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
    unsigned char category; /* D-316: aura_photo_cat_t, del indice OPCIONAL de Aura Studio */
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
/* D-303: alternado con SELECT dentro del visor; se reinicia a
 * "ajustar" (false) cada vez que se entra desde la lista -- ver
 * aura_photos_handle_button()/aura_photo_viewer_handle_button(). */
static bool s_cover_mode = false;

/* D-316: subconjunto de s_photos[] que coincide con la categoria de la
 * pantalla activa -- mismo patron que aura_video.c (ensure_video_filter()):
 * AURA_PHOTO_CAT_NONE (Todas las fotos) reproduce la lista sin filtrar,
 * CERO cambio de comportamiento respecto a antes de D-316. */
static int s_filtered_idx[MAX_PHOTOS];
static int s_filtered_count = 0;
static int s_filtered_built_for = 0; /* aura_photo_cat_t + 1; 0 = nada construido */
/* Filtro activo cuando se entro al visor desde la lista -- LEFT/RIGHT/
 * SCROLL dentro del visor navegan ESTE subconjunto (D-316), no la lista
 * completa; `s_viewer_pos` es la posicion dentro de el (lo que la lista
 * de origen restaura como seleccion al volver con MENU). */
static aura_photo_cat_t s_viewer_filter = AURA_PHOTO_CAT_NONE;
static int s_viewer_pos = 0;

static unsigned char s_view_scratch[VIEW_SCRATCH_SIZE];
static int s_loaded_index = -1;
static bool s_loaded_ok = false;
static struct bitmap s_bm;
/* D-303: tamano FINAL en pantalla -- puede ser mayor que s_bm.width/
 * height (lo que en verdad se decodifico) cuando "cubrir" agranda mas
 * alla de lo que el decoder pudo producir; draw_scaled_centered() usa
 * la diferencia para re-muestrear. En "ajustar" siempre coincide con
 * s_bm.width/height (sin agrandado, blit 1:1). */
static int s_display_w;
static int s_display_h;

static bool has_ext(const char *name, const char *ext)
{
    size_t nlen = strlen(name), elen = strlen(ext);
    return nlen > elen && !strcasecmp(name + nlen - elen, ext);
}

static bool is_supported_image(const char *name)
{
    return has_ext(name, ".jpg") || has_ext(name, ".jpeg") || has_ext(name, ".bmp");
}

/* D-302 (el dueno, hardware real: "muchas se ven, pero hay otras que
 * no, empiezan con '._Nombre'"): sidecars AppleDouble de macOS --
 * resource fork/xattrs que macOS deja junto al archivo real al
 * escribirlo en un volumen sin ese soporte nativo (el FAT32 del iPod,
 * o al copiar/extraer desde un ZIP/USB de origen). Comparten la
 * extension del archivo real ("._Foto.jpg") pero su contenido no es
 * una imagen -- se listaban igual (misma extension) y nunca abrian.
 * Nunca son contenido real del usuario, se descartan siempre. */
static bool is_apple_double_sidecar(const char *name)
{
    return name[0] == '.' && name[1] == '_';
}

static bool is_listable_image(const char *name)
{
    if (is_apple_double_sidecar(name))
        return false;
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
        s_photos[s_photo_count].category =
            (unsigned char)aura_media_categories_photo_lookup(entry->d_name);
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
    s_filtered_built_for = 0;
    aura_media_categories_invalidate();
}

int aura_photos_count(void)
{
    ensure_photo_list();
    return s_photo_total_count;
}

/* D-316: reconstruye s_filtered_idx[]/s_filtered_count para `cat` --
 * mismo criterio que ensure_video_filter() (aura_video.c): cacheado por
 * categoria, recorre s_photos[] (ya ordenado por compare_photo_display())
 * una vez por cambio de pantalla, nunca por cuadro. El orden alfabetico
 * ya aplicado sobrevive al filtrado (se recorre en el mismo orden). */
static void ensure_photo_filter(aura_photo_cat_t cat)
{
    int i;
    int key = (int)cat + 1;

    ensure_photo_list();
    if (s_filtered_built_for == key)
        return;

    s_filtered_count = 0;
    for (i = 0; i < s_photo_count; i++)
    {
        if (cat != AURA_PHOTO_CAT_NONE && s_photos[i].category != (unsigned char)cat)
            continue;
        s_filtered_idx[s_filtered_count++] = i;
    }
    s_filtered_built_for = key;
}

/* Pantalla -> filtro de categoria (D-316). AURA_SCREEN_PHOTOS_ALL (y
 * cualquier otra) cae a AURA_PHOTO_CAT_NONE -- sin filtrar. */
static aura_photo_cat_t screen_to_photo_filter(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_PHOTOS_PHOTO: return AURA_PHOTO_CAT_PHOTO;
    case AURA_SCREEN_PHOTOS_IMAGE: return AURA_PHOTO_CAT_IMAGE;
    case AURA_SCREEN_PHOTOS_AI:    return AURA_PHOTO_CAT_AI;
    default:                       return AURA_PHOTO_CAT_NONE;
    }
}

int aura_photos_count_filtered(aura_photo_cat_t cat)
{
    ensure_photo_filter(cat);
    return s_filtered_count;
}

const char *aura_photos_filtered_filename(aura_photo_cat_t cat, int index)
{
    ensure_photo_filter(cat);
    if (index < 0 || index >= s_filtered_count)
        return NULL;
    return s_photos[s_filtered_idx[index]].filename;
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
 * §6.5 en PLAN-image-viewer.md). D-316: solo aplica a la lista SIN
 * filtrar -- las categorias siempre cargan su subconjunto completo
 * (acotado por el mismo MAX_PHOTOS de la lista base, nunca mas grande). */
static bool has_more_row(aura_photo_cat_t filter)
{
    return filter == AURA_PHOTO_CAT_NONE && s_photo_total_count > s_photo_count;
}

static int display_row_count(aura_photo_cat_t filter)
{
    ensure_photo_filter(filter);
    return s_filtered_count + (has_more_row(filter) ? 1 : 0);
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

/* -- Cuadrícula "Todas las fotos" (D-323, reemplaza la lista de D-291) -----
 *
 * PHOTO_GRID_COLS x PHOTO_GRID_ROWS celdas visibles a la vez, en orden
 * de lectura (izquierda a derecha, arriba a abajo) -- la selección
 * sigue siendo el mismo índice LINEAL de siempre (`aura_nav`); solo el
 * dibujado mapea índice -> (fila, columna) -- primer precedente de
 * layout 2D en todo apps/aura (todo lo demás es lista de una columna).
 * Sin nombre de archivo (el encargo lo pide explícito): cada celda es
 * solo la miniatura, con un realce de acento detrás cuando está
 * seleccionada -- mismo mecanismo que la pastilla de la lista de
 * antes (`a26_shell_fill_rounded_rect` ANTES de blitear la miniatura
 * encima, mismo orden de dibujo, ninguna primitiva nueva). La fila
 * final "...y N mas" de la lista se comprime a una celda "+N" (mismo
 * criterio: nunca trunca en silencio, SELECT no hace nada sobre ella,
 * `aura_photos_handle_button()` ya la excluye). */
static long s_photos_activity_since = 0;
static int s_photos_last_selected = -1;

/* D-316: titulo de barra + mensaje vacio por pantalla -- "Todas las
 * fotos" conserva su titulo/mensaje de siempre (CERO cambio de
 * comportamiento); las tres categorias reusan AURA_STR_PHOTOS/
 * AURA_STR_ABOUT_IMAGES (mismo texto exacto que ya usa "Acerca de") y
 * AURA_STR_PHOTOS_AI (nueva) como titulo, y sus AURA_STR_PHOTOS_EMPTY_*
 * correspondientes como mensaje de lista vacia. */
static aura_str_id_t photo_screen_title_id(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_PHOTOS_PHOTO: return AURA_STR_PHOTOS;
    case AURA_SCREEN_PHOTOS_IMAGE: return AURA_STR_ABOUT_IMAGES;
    case AURA_SCREEN_PHOTOS_AI:    return AURA_STR_PHOTOS_AI;
    default:                       return AURA_STR_PHOTOS;
    }
}

static aura_str_id_t photo_empty_message_id(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_PHOTOS_PHOTO: return AURA_STR_PHOTOS_EMPTY_PHOTO;
    case AURA_SCREEN_PHOTOS_IMAGE: return AURA_STR_PHOTOS_EMPTY_IMAGE;
    case AURA_SCREEN_PHOTOS_AI:    return AURA_STR_PHOTOS_EMPTY_AI;
    default:                       return AURA_STR_EMPTY_PHOTOS;
    }
}

static void draw_photos_grid(aura_nav_t *nav, aura_screen_id_t screen, aura_photo_cat_t filter)
{
    int selected = aura_nav_get_selection(nav);
    int count = display_row_count(filter); /* incluye la celda final "+N" si aplica */
    int total_rows = (count + PHOTO_GRID_COLS - 1) / PHOTO_GRID_COLS;
    int sel_row = selected / PHOTO_GRID_COLS;
    int first_row = 0;
    int row, col;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(photo_screen_title_id(screen)));

    if (total_rows > PHOTO_GRID_ROWS)
    {
        first_row = sel_row - PHOTO_GRID_ROWS / 2;
        if (first_row < 0) first_row = 0;
        if (first_row > total_rows - PHOTO_GRID_ROWS) first_row = total_rows - PHOTO_GRID_ROWS;
    }

    if (selected != s_photos_last_selected)
    {
        s_photos_last_selected = selected;
        s_photos_activity_since = current_tick;
    }

    for (row = 0; row < PHOTO_GRID_ROWS; row++)
    {
        int abs_row = first_row + row;
        int cell_y = PHOTO_GRID_TOP + row * PHOTO_GRID_CELL;

        for (col = 0; col < PHOTO_GRID_COLS; col++)
        {
            int i = abs_row * PHOTO_GRID_COLS + col;
            int cell_x = PHOTO_GRID_LEFT + col * PHOTO_GRID_CELL;
            bool is_sel = (i == selected);
            bool is_more = (i == s_filtered_count); /* celda inerte "+N" */

            if (i >= count)
                continue;

            if (is_sel)
                a26_shell_fill_rounded_rect(cell_x, cell_y, PHOTO_GRID_CELL, PHOTO_GRID_CELL,
                                             A26_LAYOUT_CORNER_RADIUS_CARD,
                                             a26_color(A26_SELECTION_FILL),
                                             a26_color(A26_SHELL_BG));

            if (is_more)
            {
                char label[16];
                int w, h;

                /* Numero solo, sin palabras -- no hace falta entrada
                 * de aura_lang.c para esto (D-323). */
                snprintf(label, sizeof(label), "+%d", s_photo_total_count - s_photo_count);
                lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
                lcd_set_foreground(a26_color(A26_TEXT_TERTIARY));
                lcd_getstringsize((const unsigned char *)label, &w, &h);
                lcd_putsxy(cell_x + (PHOTO_GRID_CELL - w) / 2, cell_y + (PHOTO_GRID_CELL - h) / 2,
                           (const unsigned char *)label);
                continue;
            }

            draw_photos_thumb(cell_x + (PHOTO_GRID_CELL - PHOTO_ART_SIZE) / 2,
                               cell_y + (PHOTO_GRID_CELL - PHOTO_ART_SIZE) / 2,
                               s_filtered_idx[i]);
        }
    }

    aura_scroll_indicator_draw(A26_SCREEN_WIDTH, PHOTO_GRID_TOP,
                                PHOTO_GRID_ROWS * PHOTO_GRID_CELL, selected, count,
                                (current_tick - s_photos_activity_since) * 1000L / HZ,
                                a26_color(A26_SHELL_BG), a26_color(A26_TEXT_TERTIARY));
}

void aura_photos_draw(aura_nav_t *nav, aura_screen_id_t screen)
{
    aura_photo_cat_t filter = screen_to_photo_filter(screen);

    ensure_photo_filter(filter);

    if (s_filtered_count == 0)
    {
        /* AUDITORIA-01 A-15: "la barra nunca cambia de forma entre
         * pantallas" (doc SS5) -- este vacio era la unica pantalla del
         * sistema sin ella. D-316: "Todas las fotos" conserva el
         * mensaje con pista de siempre; las categorias usan un mensaje
         * simple (mismo criterio que aura_video.c), sin pista de sync --
         * el indice de categorias es un detalle interno, no algo que el
         * usuario deba "arreglar" sincronizando de nuevo. */
        a26_shell_clear_screen();
        aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(photo_screen_title_id(screen)));
        if (filter == AURA_PHOTO_CAT_NONE)
            draw_message_with_hint(AURA_STR_EMPTY_PHOTOS, AURA_STR_EMPTY_PHOTOS_HINT);
        else
            draw_message(photo_empty_message_id(screen));
        return;
    }

    draw_photos_grid(nav, screen, filter);
}

void aura_photos_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    aura_photo_cat_t filter = screen_to_photo_filter(screen);
    int sel = aura_nav_get_selection(nav);
    int count;

    ensure_photo_filter(filter);
    count = s_filtered_count;

    switch (button)
    {
    /* D-323: dinamica real de la rueda (1-3 celdas por evento segun
     * velocidad angular, aura_wheel_advance() -- mismo mecanismo de
     * "desplazarnos rápido" que ya usan Álbumes/menú principal/etc) en
     * vez del ±1 fijo de la lista de antes. La cuadrícula recorre las
     * celdas en orden de lectura (indice lineal), asi que "rapido"
     * simplemente salta varias celdas seguidas -- sin ejes fila/
     * columna independientes, como el iPod Classic original. */
    case BUTTON_SCROLL_FWD:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, display_row_count(filter), 1));
        break;
    case BUTTON_SCROLL_BACK:
        aura_nav_set_selection(nav, aura_wheel_advance(sel, display_row_count(filter), -1));
        break;
    case BUTTON_SELECT:
        /* sel == count solo puede ser la fila inerte "...y N mas"
         * (has_more_row()) -- nunca abre el visor. */
        if (sel >= 0 && sel < count)
        {
            s_viewer_filter = filter;
            s_viewer_pos = sel;
            s_current_index = s_filtered_idx[sel];
            /* D-303: cada entrada al visor arranca en "ajustar" --
             * s_loaded_index tambien se invalida aca (no solo cuando
             * cambia el indice de foto): reabrir la MISMA foto que se
             * habia dejado en "cubrir" sin esto se saltaba la
             * redecodificacion (s_loaded_index ya coincidia con
             * s_current_index) y se seguia viendo "cubrir" pese a que
             * s_cover_mode ya habia vuelto a false. */
            s_cover_mode = false;
            s_loaded_index = -1;
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
/* D-303 (el dueno: modo "cubriendo los bordes" en el visor, alternado
 * con SELECT): dimensiones reales de la foto, conocidas desde la
 * misma fase de sondeo que ya corria (solo cabeceras) -- hacen falta
 * para calcular el recorte de "cubrir" antes de decodificar. 0 si el
 * sondeo no pudo leerlas (cabecera no reconocida): ese caso cae a
 * modo "ajustar" sin importar s_cover_mode, ver load_current_photo(). */
static int s_probed_src_w = 0;
static int s_probed_src_h = 0;

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
    s_probed_src_w = 0;
    s_probed_src_h = 0;

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
    s_probed_src_w = w;
    s_probed_src_h = h;
    s_probe_needs_indicator = (w > PHOTO_LOADING_INDICATOR_SIDE || h > PHOTO_LOADING_INDICATOR_SIDE);
}

/* D-303 (correccion tras prueba en hardware real -- el dueno: "aún se
 * ven las franjas [...] necesitamos que agrande la imagen [...] aunque
 * se corten los bordes"): la primera version acotaba el factor de
 * "cubrir" a 1.0 para no pedirle al decoder que agrande (no puede,
 * ver abajo) -- pero eso dejaba SIN agrandar cualquier foto que ya
 * fuera mas chica que la pantalla en el eje que le falta, que es
 * exactamente el caso comun (las fotos que sincroniza Studio ya vienen
 * redimensionadas a 320 o 640px de lado mas largo, ST-022). Esta
 * version separa dos tamanos:
 *
 *  - `decode_w/h`: lo que se le PIDE AL DECODER. Nunca mayor al
 *    tamano de origen -- el decoder de JPEG de Rockbox SOLO reduce
 *    durante la decodificacion (escalado en el dominio DCT, potencias
 *    de 2), nunca agranda; pedirle mas dejaria bm->width/height
 *    desincronizados de lo que en realidad escribio en el scratch
 *    buffer. Si el factor ideal de "cubrir" ya es <= 1.0 (la foto
 *    alcanza para cubrir sin agrandar), decodificar YA al tamano final
 *    es lo mas eficiente. Si el factor ideal es > 1.0 (hace falta
 *    agrandar), se decodifica a resolucion NATIVA -- la fuente mas
 *    nitida posible para el agrandado que sigue.
 *  - `display_w/h`: el tamano FINAL que se ve en pantalla (recorte
 *    incluido) -- SIN tope de 1.0. Si `display > decode`, el agrandado
 *    real lo hace `draw_scaled_centered()` por muestreo de pixeles ya
 *    decodificados (nearest-neighbor), no el decoder.
 *
 * `s_view_scratch` sigue siendo un buffer fijo -- si `decode_w x
 * decode_h` no entra (una foto "HD" de 640px decodificada a tamano
 * nativo en modo cubrir), se reduce proporcionalmente (conserva el
 * aspecto) hasta que si. `display_w/h` NUNCA se reduce por esto: bajar
 * la nitidez de la fuente esta bien, dejar de cubrir la pantalla no --
 * es justo lo que se pidio evitar.
 */
static void compute_decode_and_display_size(int src_w, int src_h,
                                             int *decode_w, int *decode_h,
                                             int *display_w, int *display_h)
{
    /* Aritmetica en punto fijo Q16.16 -- este target no tiene FPU;
     * mismo criterio general (shift en vez de float) que ya usa
     * aura_flow.c para su proyeccion de perspectiva, aunque con otro
     * ancho de shift (10 alla, 16 aca -- este calculo no comparte
     * ninguna formula con esas, solo la tecnica). */
    long scale_x = ((long)A26_SCREEN_WIDTH << 16) / src_w;
    long scale_y = ((long)A26_SCREEN_HEIGHT << 16) / src_h;
    long ideal_scale = (scale_x > scale_y) ? scale_x : scale_y; /* max: cubrir, no ajustar */
    long dw, dh;

    dw = (src_w * ideal_scale) >> 16;
    dh = (src_h * ideal_scale) >> 16;
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    *display_w = (int)dw;
    *display_h = (int)dh;

    if (ideal_scale <= (1L << 16))
    {
        /* La foto ya alcanza para cubrir sin agrandar -- decodificar
         * directo al tamano final es lo mas eficiente (blit final 1:1,
         * sin muestreo de por medio). */
        *decode_w = *display_w;
        *decode_h = *display_h;
    }
    else
    {
        *decode_w = src_w;
        *decode_h = src_h;
    }

    if ((long)(*decode_w) * (*decode_h) * (long)sizeof(fb_data) > (long)VIEW_SCRATCH_SIZE)
    {
        /* isqrt256-style: sqrt entero por biseccion, ya que no hay FPU
         * -- el rango cabe sobrado en 32 bits para estos tamanos. Solo
         * reduce decode_w/h -- display_w/h queda intacto, ver
         * comentario de arriba. */
        long budget_px = (long)VIEW_SCRATCH_SIZE / (long)sizeof(fb_data);
        long num = budget_px << 16, den = (long)(*decode_w) * (*decode_h);
        long mem_scale2 = num / den; /* factor^2 en Q16.16 */
        long lo = 0, hi = 1L << 16, mem_scale;
        while (lo < hi)
        {
            long mid = (lo + hi + 1) / 2;
            if ((mid * mid) >> 16 <= mem_scale2)
                lo = mid;
            else
                hi = mid - 1;
        }
        mem_scale = lo;
        *decode_w = (int)(((long)(*decode_w) * mem_scale) >> 16);
        *decode_h = (int)(((long)(*decode_h) * mem_scale) >> 16);
        if (*decode_w < 1) *decode_w = 1;
        if (*decode_h < 1) *decode_h = 1;
    }
}

static void load_current_photo(void)
{
    char path[MAX_PATH];
    int format;
    int ret;

    if (s_loaded_index == s_current_index)
        return;
    s_loaded_index = s_current_index;
    s_loaded_ok = false;

    if (!s_photos[s_current_index].supported)
        return;

    snprintf(path, sizeof(path), "%s/%s", PHOTOS_DIR, s_photos[s_current_index].filename);

    s_bm.data = (char *)s_view_scratch;
#if (LCD_DEPTH > 1)
    s_bm.maskdata = NULL;
#endif

    /* D-303: en modo "cubrir" el tamano de DECODIFICACION exacto ya
     * viene calculado (nunca mayor al de origen) -- sin
     * FORMAT_KEEP_ASPECT, que volvería a encoger para que quepa entero
     * en la caja (justo lo que "ajustar" hace y "cubrir" quiere
     * evitar). `s_display_w/h` es el tamano FINAL en pantalla, que
     * `draw_scaled_centered()` usa para agrandar por muestreo si hace
     * falta mas de lo que se pudo decodificar. Sin dimensiones de
     * origen (sondeo no las pudo leer) cae a "ajustar" sin importar
     * s_cover_mode -- no hay con que calcular el recorte/agrandado. */
    if (s_cover_mode && s_probed_src_w > 0 && s_probed_src_h > 0)
    {
        int decode_w, decode_h;
        compute_decode_and_display_size(s_probed_src_w, s_probed_src_h,
                                         &decode_w, &decode_h, &s_display_w, &s_display_h);
        s_bm.width = decode_w;
        s_bm.height = decode_h;
        format = FORMAT_NATIVE | FORMAT_RESIZE;
    }
    else
    {
        s_bm.width = A26_SCREEN_WIDTH;
        s_bm.height = A26_SCREEN_HEIGHT;
        format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    }

    if (has_ext(path, ".bmp"))
        ret = read_bmp_file(path, &s_bm, sizeof(s_view_scratch), format, NULL);
    else
        ret = read_jpeg_file(path, &s_bm, sizeof(s_view_scratch), format, NULL);

    s_loaded_ok = (ret > 0);
    if (s_loaded_ok && !s_cover_mode)
    {
        /* "Ajustar": el decoder ya decodifico al tamano final exacto
         * (KEEP_ASPECT) -- decode y display son lo mismo, blit 1:1. */
        s_display_w = s_bm.width;
        s_display_h = s_bm.height;
    }
}

/* D-303: dibuja `src` (decodificado a src_w x src_h, stride src_w)
 * centrado en pantalla a un tamano de despliegue display_w x
 * display_h. Tres casos, resueltos con la misma formula de recorte
 * (nunca deforma, mismo factor en X e Y por construccion de
 * compute_decode_and_display_size/FORMAT_KEEP_ASPECT):
 *  - display == pantalla en un eje: ese eje llena exacto, sin recorte
 *    ni banda.
 *  - display > pantalla en un eje: recorta ese eje del centro
 *    ("cubrir").
 *  - display < pantalla en un eje: banda/letterbox centrado
 *    ("ajustar", o "cubrir" degradado por el tope de memoria).
 * Si src coincide con display (el caso normal salvo cuando "cubrir"
 * agranda mas alla de lo decodificado) es un blit 1:1 sin muestreo. Si
 * src es mas chico que display, cada pixel de pantalla se resuelve al
 * pixel de origen mas cercano (nearest-neighbor) -- agrandado real,
 * mas alla de lo que el decoder de JPEG puede hacer en si mismo. */
static void draw_scaled_centered(const fb_data *src, int src_w, int src_h,
                                  int display_w, int display_h)
{
    int display_x0 = (display_w > A26_SCREEN_WIDTH) ? (display_w - A26_SCREEN_WIDTH) / 2 : 0;
    int display_y0 = (display_h > A26_SCREEN_HEIGHT) ? (display_h - A26_SCREEN_HEIGHT) / 2 : 0;
    int screen_x0 = (display_w < A26_SCREEN_WIDTH) ? (A26_SCREEN_WIDTH - display_w) / 2 : 0;
    int screen_y0 = (display_h < A26_SCREEN_HEIGHT) ? (A26_SCREEN_HEIGHT - display_h) / 2 : 0;
    int draw_w = (display_w < A26_SCREEN_WIDTH) ? display_w : A26_SCREEN_WIDTH;
    int draw_h = (display_h < A26_SCREEN_HEIGHT) ? display_h : A26_SCREEN_HEIGHT;

    if (src_w == display_w && src_h == display_h)
    {
        lcd_bitmap_part(src, display_x0, display_y0, src_w, screen_x0, screen_y0, draw_w, draw_h);
        return;
    }

    {
        int x, y;
        for (y = 0; y < draw_h; y++)
        {
            int dy = display_y0 + y;
            int sy = dy * src_h / display_h;
            const fb_data *srow;
            fb_data *drow = FBADDR(screen_x0, screen_y0 + y);

            if (sy >= src_h) sy = src_h - 1;
            srow = src + (long)sy * src_w;
            for (x = 0; x < draw_w; x++)
            {
                int dx = display_x0 + x;
                int sx = dx * src_w / display_w;
                if (sx >= src_w) sx = src_w - 1;
                drow[x] = srow[sx];
            }
        }
    }
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

    draw_scaled_centered((const fb_data *)s_view_scratch, s_bm.width, s_bm.height,
                          s_display_w, s_display_h);
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
    /* D-316: navega dentro del subconjunto FILTRADO con el que se entro
     * al visor (s_viewer_filter/s_viewer_pos) -- no la lista completa de
     * s_photos[]. Con AURA_PHOTO_CAT_NONE (Todas las fotos) el
     * subconjunto ES la lista completa, CERO cambio de comportamiento. */
    case BUTTON_SCROLL_FWD:
    case BUTTON_RIGHT:
        if (s_viewer_pos < aura_photos_count_filtered(s_viewer_filter) - 1)
        {
            s_viewer_pos++;
            s_current_index = s_filtered_idx[s_viewer_pos];
            aura_transition_fade_slide_region(nav, 0, 0, A26_SCREEN_WIDTH, A26_SCREEN_HEIGHT, 1);
        }
        break;
    case BUTTON_SCROLL_BACK:
    case BUTTON_LEFT:
        if (s_viewer_pos > 0)
        {
            s_viewer_pos--;
            s_current_index = s_filtered_idx[s_viewer_pos];
            aura_transition_fade_slide_region(nav, 0, 0, A26_SCREEN_WIDTH, A26_SCREEN_HEIGHT, -1);
        }
        break;
    case BUTTON_MENU:
        /* D-291 §5.3: la lista recupera la seleccion de la foto que se
         * estaba viendo, no la que tenia al entrar -- aura_nav_pop()
         * PRIMERO (decrementa depth), aura_nav_set_selection() DESPUES
         * (para que depth-1 ya apunte al slot de la LISTA, no al del
         * propio visor -- ver aura_nav_set_selection()/aura_nav.c).
         * D-316: `s_viewer_pos` (posicion dentro del subconjunto
         * filtrado), no `s_current_index` (indice absoluto) -- son el
         * mismo valor solo cuando el filtro es AURA_PHOTO_CAT_NONE. */
        aura_nav_pop(nav);
        aura_nav_set_selection(nav, s_viewer_pos);
        break;
    /* D-303 (encargo del dueno): Select alterna "ajustar" (foto
     * completa, con bandas si no llena la pantalla) / "cubrir" (llena
     * la pantalla, recortando el sobrante, sin deformar) -- toma el
     * lugar que antes tenia como atajo redundante de Menu para salir;
     * Menu sigue siendo la unica salida. s_loaded_index = -1 fuerza
     * una redecodificacion con el tamano objetivo del modo nuevo. */
    case BUTTON_SELECT:
        s_cover_mode = !s_cover_mode;
        s_loaded_index = -1;
        break;
    default:
        break;
    }
}
