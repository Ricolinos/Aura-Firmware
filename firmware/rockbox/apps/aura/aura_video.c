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
static int s_video_count = -1;
/* D-298: total real en /Videos, puede ser mayor que s_video_count
 * (MAX_VIDEOS). Mismo patron que s_photo_total_count en aura_photos.c. */
static int s_video_total_count = -1;
static char s_more_label[40];

static bool has_ext(const char *name, const char *ext)
{
    size_t nlen = strlen(name), elen = strlen(ext);
    return nlen > elen && !strcasecmp(name + nlen - elen, ext);
}

static bool is_video_file(const char *name)
{
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
        s_video_count++;
    }
    closedir(d);
}

void aura_video_invalidate(void)
{
    s_video_count = -1;
    s_video_total_count = -1;
}

int aura_video_count(void)
{
    ensure_video_list();
    return s_video_count;
}

void aura_video_draw(aura_nav_t *nav)
{
    int i, row_count;
    bool has_more;
    static aura_list_item_t items[MAX_VIDEOS + 1];

    ensure_video_list();
    has_more = s_video_total_count > s_video_count;
    a26_shell_clear_screen();

    if (s_video_count == 0)
    {
        int w, h;
        /* AUDITORIA-01 A-15: mismo vacio que Fotos -- la barra nunca
         * cambia de forma entre pantallas (doc SS5). */
        aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_VIDEOS));
        lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)aura_str(AURA_STR_EMPTY_VIDEOS), &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, (A26_SCREEN_HEIGHT - h) / 2,
                   (const unsigned char *)aura_str(AURA_STR_EMPTY_VIDEOS));
        return;
    }

    for (i = 0; i < s_video_count; i++)
    {
        items[i].label = s_videos_display[i];
        /* Sin icono por fila (AUDITORIA-01 A-06, mismo criterio que
         * Fotos): "video" repetido en cada fila es el anti-patron SS8,
         * no una decision. */
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
        items[i].dimmed = 0;
    }
    row_count = s_video_count;
    if (has_more)
    {
        /* Fila inerte "...y N mas" -- mismo patron que Fotos (D-291).
         * No es elegible: aura_video_handle_button() sigue acotando la
         * seleccion a s_video_count - 1, esta fila queda siempre fuera
         * de ese rango. */
        snprintf(s_more_label, sizeof(s_more_label), aura_str(AURA_STR_LIST_MORE_FMT),
                 s_video_total_count - s_video_count);
        items[row_count].label = s_more_label;
        items[row_count].icon_name = NULL;
        items[row_count].checked = 0;
        items[row_count].toggle = -1;
        items[row_count].dimmed = 1;
        row_count++;
    }
    aura_widgets_draw_list(aura_str(AURA_STR_VIDEOS), items, row_count,
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

void aura_video_handle_button(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < s_video_count - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        if (s_video_count > 0)
        {
            char path[MAX_PATH];
            int ret;
            snprintf(path, sizeof(path), "%s/%s", VIDEOS_DIR, s_videos[sel]);
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
