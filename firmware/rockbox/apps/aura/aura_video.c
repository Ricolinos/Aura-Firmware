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

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "file.h"
#include "dir.h"
#include "string-extra.h"
#include "rbpaths.h"
#include "plugin.h"
#include "kernel.h" /* HZ, D-298: espera del mensaje "No se pudo abrir" */

#include "aura_video.h"
#include "aura_widgets.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_status_bar_v2.h"
#include "aura_media_categories.h"

/* Aura solo reproduce su formato interno (MPEG-1/2 320x240, generado
 * por Aura Studio al sincronizar) delegando en el plugin mpegplayer
 * del propio fork -- portar un decoder de video propio queda fuera de
 * alcance de esta fase (D-029). plugin_load() hace toda la limpieza
 * de pantalla/viewport/fuente antes y despues por su cuenta; Aura solo
 * necesita llamarlo y redibujar su pantalla normal al volver. */

#define VIDEOS_DIR     "/Videos"
#define MAX_VIDEOS     100
/* D-298: 96, paridad con PHOTO_NAME_LEN -- el contrato
 * (docs/contracts/library-layout-v1.md SS1) limita el nombre en
 * /Videos a <= 95 bytes UTF-8 incluyendo la extension; 64 lo truncaba
 * en silencio y el archivo truncado no se encontraba al reproducir
 * (mismo sintoma que mpegplayer.rock ausente: "no se pudo abrir"). */
#define VIDEO_NAME_LEN 96

static char s_videos[MAX_VIDEOS][VIDEO_NAME_LEN];
static char s_videos_display[MAX_VIDEOS][VIDEO_NAME_LEN]; /* sin extension, AUDITORIA-01 A-05 */
/* D-316: categoria de cada video segun el indice OPCIONAL de Aura Studio
 * (aura_media_categories.h) -- resuelta UNA vez por archivo al escanear,
 * nunca en cada cuadro. AURA_VIDEO_CAT_NONE si el indice no existe o no
 * trae entrada para este archivo. */
static unsigned char s_videos_cat[MAX_VIDEOS];
static int s_video_count = -1;
/* D-298: total real en /Videos, puede ser mayor que s_video_count
 * (MAX_VIDEOS). Mismo patron que s_photo_total_count en aura_photos.c. */
static int s_video_total_count = -1;
static char s_more_label[40];

/* D-316: subconjunto de s_videos[] que coincide con la categoria de la
 * pantalla activa -- reconstruido bajo demanda (ensure_video_filter()),
 * nunca en cada cuadro. AURA_VIDEO_CAT_NONE (Todos los videos) copia el
 * indice identidad sin filtrar -- CERO cambio de comportamiento para esa
 * pantalla, que ya funcionaba antes de D-316. */
static int s_filtered_idx[MAX_VIDEOS];
static int s_filtered_count = 0;
static int s_filtered_built_for = -1; /* aura_video_cat_t + 1, o 0 = nada construido */

static bool has_ext(const char *name, const char *ext)
{
    size_t nlen = strlen(name), elen = strlen(ext);
    return nlen > elen && !strcasecmp(name + nlen - elen, ext);
}

/* D-302: mismo sidecar AppleDouble de macOS que aura_photos.c --
 * "._Nombre.mpg" comparte extension con un video real pero no lo es. */
static bool is_apple_double_sidecar(const char *name)
{
    return name[0] == '.' && name[1] == '_';
}

static bool is_video_file(const char *name)
{
    if (is_apple_double_sidecar(name))
        return false;
    return has_ext(name, ".mpg") || has_ext(name, ".mpeg");
}

/* Nombre para mostrar sin extension de archivo (AUDITORIA-01 A-05, doc
 * de diseno Principio 7: "nunca jerga"). Solo pela la extension para la
 * lista; s_videos[] con extension sigue siendo lo que se abre. */
static void strip_ext_for_display(const char *filename, char *out, size_t outsz)
{
    char *dot;

    strlcpy(out, filename, outsz);
    dot = strrchr(out, '.');
    if (dot)
        *dot = '\0';
}

static void ensure_video_list(void)
{
    DIR *d;
    struct DIRENT *entry;

    if (s_video_count >= 0)
        return;

    s_video_count = 0;
    s_video_total_count = 0;
    d = opendir(VIDEOS_DIR);
    if (!d)
        return;

    while ((entry = readdir(d)) != NULL)
    {
        if (!is_video_file(entry->d_name))
            continue;
        s_video_total_count++;
        if (s_video_count >= MAX_VIDEOS)
            continue; /* se sigue contando para el total, sin guardar */
        strlcpy(s_videos[s_video_count], entry->d_name, VIDEO_NAME_LEN);
        strip_ext_for_display(entry->d_name, s_videos_display[s_video_count], VIDEO_NAME_LEN);
        s_videos_cat[s_video_count] =
            (unsigned char)aura_media_categories_video_lookup(entry->d_name);
        s_video_count++;
    }
    closedir(d);
}

void aura_video_invalidate(void)
{
    s_video_count = -1;
    s_video_total_count = -1;
    s_filtered_built_for = -1;
    aura_media_categories_invalidate();
}

int aura_video_count(void)
{
    ensure_video_list();
    return s_video_count;
}

/* D-316: reconstruye s_filtered_idx[]/s_filtered_count para la
 * categoria `cat` -- AURA_VIDEO_CAT_NONE reproduce la lista SIN filtrar
 * (identidad), cualquier otro valor solo incluye los videos cuya
 * categoria del indice coincide. Cacheado por `cat`: no vuelve a
 * recorrer s_videos[] en cada cuadro mientras la pantalla activa no
 * cambie. */
static void ensure_video_filter(aura_video_cat_t cat)
{
    int i;
    int key = (int)cat + 1;

    ensure_video_list();
    if (s_filtered_built_for == key)
        return;

    s_filtered_count = 0;
    for (i = 0; i < s_video_count; i++)
    {
        if (cat != AURA_VIDEO_CAT_NONE && s_videos_cat[i] != (unsigned char)cat)
            continue;
        s_filtered_idx[s_filtered_count++] = i;
    }
    s_filtered_built_for = key;
}

/* Pantalla -> filtro de categoria (D-316). AURA_SCREEN_VIDEOS_ALL (y
 * cualquier otra) cae a AURA_VIDEO_CAT_NONE -- sin filtrar. */
static aura_video_cat_t screen_to_video_filter(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_VIDEOS_MOVIES:  return AURA_VIDEO_CAT_MOVIE;
    case AURA_SCREEN_VIDEOS_TVSHOWS: return AURA_VIDEO_CAT_SERIES;
    case AURA_SCREEN_VIDEOS_CLIPS:   return AURA_VIDEO_CAT_CLIP;
    default:                         return AURA_VIDEO_CAT_NONE;
    }
}

int aura_video_count_filtered(aura_video_cat_t cat)
{
    ensure_video_filter(cat);
    return s_filtered_count;
}

const char *aura_video_filtered_filename(aura_video_cat_t cat, int index)
{
    ensure_video_filter(cat);
    if (index < 0 || index >= s_filtered_count)
        return NULL;
    return s_videos[s_filtered_idx[index]];
}

/* D-316: titulo de barra + mensaje vacio por pantalla -- "Todos los
 * videos" conserva el mensaje/titulo de siempre (CERO cambio de
 * comportamiento); las tres categorias reusan los mismos AURA_STR_
 * VIDEOS_EMPTY_* que ya existian como texto FIJO del panel izquierdo
 * (aura_screens.c, mientras las filas eran inertes, D-264) -- ahora
 * tambien sirven de mensaje real de pantalla vacia, condicionado al
 * conteo filtrado real en vez de mostrarse siempre. */
static aura_str_id_t video_screen_title_id(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_VIDEOS_MOVIES:  return AURA_STR_VIDEOS_MOVIES;
    case AURA_SCREEN_VIDEOS_TVSHOWS: return AURA_STR_VIDEOS_TVSHOWS;
    case AURA_SCREEN_VIDEOS_CLIPS:   return AURA_STR_VIDEOS_CLIPS;
    default:                         return AURA_STR_VIDEOS;
    }
}

static aura_str_id_t video_empty_message_id(aura_screen_id_t screen)
{
    switch (screen)
    {
    case AURA_SCREEN_VIDEOS_MOVIES:  return AURA_STR_VIDEOS_EMPTY_MOVIES;
    case AURA_SCREEN_VIDEOS_TVSHOWS: return AURA_STR_VIDEOS_EMPTY_TVSHOWS;
    case AURA_SCREEN_VIDEOS_CLIPS:   return AURA_STR_VIDEOS_EMPTY_CLIPS;
    default:                         return AURA_STR_EMPTY_VIDEOS;
    }
}

void aura_video_draw(aura_nav_t *nav, aura_screen_id_t screen)
{
    aura_video_cat_t filter = screen_to_video_filter(screen);
    int i, row_count, count;
    bool has_more;
    static aura_list_item_t items[MAX_VIDEOS + 1];

    ensure_video_filter(filter);
    count = s_filtered_count;
    /* "...y N mas" solo tiene sentido en la lista SIN filtrar -- las
     * categorias siempre cargan su subconjunto completo (acotado por el
     * mismo MAX_VIDEOS de la lista base, nunca mas grande que ella). */
    has_more = (filter == AURA_VIDEO_CAT_NONE) && (s_video_total_count > s_video_count);
    a26_shell_clear_screen();

    if (count == 0)
    {
        int w, h;
        aura_str_id_t title_id = video_screen_title_id(screen);
        aura_str_id_t msg_id = video_empty_message_id(screen);

        /* AUDITORIA-01 A-15: mismo vacio que Fotos -- la barra nunca
         * cambia de forma entre pantallas (doc SS5). */
        aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(title_id));
        lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, (A26_SCREEN_HEIGHT - h) / 2,
                   (const unsigned char *)aura_str(msg_id));
        return;
    }

    for (i = 0; i < count; i++)
    {
        items[i].label = s_videos_display[s_filtered_idx[i]];
        /* Sin icono por fila (AUDITORIA-01 A-06, mismo criterio que
         * Fotos): "video" repetido en cada fila es el anti-patron SS8,
         * no una decision. */
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
        items[i].dimmed = 0;
    }
    row_count = count;
    if (has_more)
    {
        /* Fila inerte "...y N mas" -- mismo patron que Fotos (D-291).
         * No es elegible: aura_video_handle_button() sigue acotando la
         * seleccion a count - 1, esta fila queda siempre fuera de ese
         * rango. */
        snprintf(s_more_label, sizeof(s_more_label), aura_str(AURA_STR_LIST_MORE_FMT),
                 s_video_total_count - s_video_count);
        items[row_count].label = s_more_label;
        items[row_count].icon_name = NULL;
        items[row_count].checked = 0;
        items[row_count].toggle = -1;
        items[row_count].dimmed = 1;
        row_count++;
    }
    aura_widgets_draw_list(aura_str(video_screen_title_id(screen)), items, row_count,
                            aura_nav_get_selection(nav));
}

/* D-298: plugin_load() devuelve PLUGIN_ERROR (-1) cuando mpegplayer.rock
 * no existe o es incompatible (PLUGIN_API_VERSION distinto) -- antes de
 * esto, aura_video_handle_button() ignoraba el valor de retorno por
 * completo, asi que el unico rastro era el splash NATIVO de Rockbox EN
 * INGLES ("Can't open ...") pasando 2s y volviendo a la lista, cromo
 * indistinguible de "no paso nada" (ver D-297: la causa real de fondo
 * era que el Release no traia mpegplayer.rock). Los valores positivos
 * (PLUGIN_USB_CONNECTED/POWEROFF/GOTO_*) NO son error -- el plugin corrio
 * y decidio ceder el control; solo PLUGIN_ERROR (< 0) es "no se pudo
 * abrir". Mismo criterio de espera 2s que el splash nativo que reemplaza,
 * pero interrumpible con cualquier boton. */
/* D-298: espera el dismiss ignorando el/los BUTTON_REL sueltos de la
 * MISMA pulsacion de SELECT que abrio este mensaje -- un
 * button_get_w_tmo() a secas los recibe como si fueran una pulsacion
 * nueva y sale casi al instante (confirmado en simulador: el mensaje
 * se dibujaba y desaparecia en unos pocos ticks, muchisimo antes de
 * los 2s pedidos). Mismo criterio de filtrado que next_button() en
 * aura_main.c, reducido a este caso: solo una pulsacion NUEVA (sin
 * BUTTON_REL) o el timeout real cierran el mensaje. */
static void wait_dismiss(int ticks)
{
    long deadline = current_tick + ticks;

    for (;;)
    {
        long remaining = deadline - current_tick;
        long b;

        if (remaining <= 0)
            return;
        b = button_get_w_tmo(remaining);
        if (b == BUTTON_NONE || !(b & BUTTON_REL))
            return;
    }
}

static void show_cant_open_video(void)
{
    int w, h, y;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_VIDEOS));

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_getstringsize((const unsigned char *)aura_str(AURA_STR_VIDEO_CANT_OPEN), &w, &h);
    y = A26_LAYOUT_STATUSBAR_HEIGHT
        + (A26_SCREEN_HEIGHT - A26_LAYOUT_STATUSBAR_HEIGHT) / 2 - h;
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y,
               (const unsigned char *)aura_str(AURA_STR_VIDEO_CANT_OPEN));

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(AURA_STR_VIDEO_CANT_OPEN_HINT), &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y + A26_TYPE_BODY + A26_SPACING_SM,
               (const unsigned char *)aura_str(AURA_STR_VIDEO_CANT_OPEN_HINT));

    lcd_update();
    wait_dismiss(HZ * 2);
    /* Sin mas: el siguiente cuadro de aura_main() redibuja esta
     * pantalla (Videos) normalmente, sin pasos extra aca. */
}

void aura_video_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    aura_video_cat_t filter = screen_to_video_filter(screen);
    int sel = aura_nav_get_selection(nav);
    int count;

    ensure_video_filter(filter);
    count = s_filtered_count;

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < count - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        if (count > 0 && sel >= 0 && sel < count)
        {
            char path[MAX_PATH];
            int ret;
            snprintf(path, sizeof(path), "%s/%s", VIDEOS_DIR, s_videos[s_filtered_idx[sel]]);
            /* D-298: sin esto, plugin_load() mostraba su propio splash()
             * nativo en ingles ("Can't open ...") ANTES de devolver el
             * error -- cromo de Rockbox visible sin forma de evitarlo
             * revisando solo el valor de retorno (confirmado en
             * simulador: el splash nativo aparecia y se cerraba solo
             * ANTES de que show_cant_open_video() pudiera dibujar nada).
             * Ver plugin_set_silent_open_errors() en plugin.c/.h. */
            plugin_set_silent_open_errors(true);
            ret = plugin_load(VIEWERS_DIR "/mpegplayer.rock", path);
            plugin_set_silent_open_errors(false);
            /* plugin_load() ya restauro pantalla/viewport/fuente. Solo
             * PLUGIN_ERROR (< 0) es una falla real -- ver comentario de
             * show_cant_open_video(). */
            if (ret < 0)
                show_cant_open_video();
        }
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
