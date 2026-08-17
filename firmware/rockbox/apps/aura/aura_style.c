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

#include "file.h"
#include "dir.h"
#include "font.h"
#include "rbpaths.h"
#include "string-extra.h"
#include "misc.h"
#include "recorder/bmp.h"

#include "aura_style.h"
#include "aura_style_manifest.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_settings.h"
#include "aura_color.h"

/* Mismo patron que AURA_DIR en aura_settings.c/aura_albumart.c/
 * aura_manifest.c -- cada .c que lo necesita lo re-declara, convencion
 * ya establecida en este arbol (no hay un header compartido para esto). */
#define AURA_DIR         ROCKBOX_DIR "/aura"
#define AURA_STYLES_DIR  AURA_DIR "/themes"

/* Nombre de archivo (sin ".fnt") de cada rol, dentro de
 * "<estilo>/fonts/" -- CONTRATO-formato-tema.md SS3. Mismo orden que
 * a26_font_style_t (apple2026_shell.h). */
static const char *const role_font_names[A26_FONT_STYLE_COUNT] = {
    [A26_FONT_STYLE_TITLE]          = "title",
    [A26_FONT_STYLE_BODY]           = "body",
    [A26_FONT_STYLE_CAPTION]        = "caption",
    [A26_FONT_STYLE_HEADER]         = "header",
    [A26_FONT_STYLE_MICRO]          = "micro",
    [A26_FONT_STYLE_DS_REG_8]       = "ds_reg_8",
    [A26_FONT_STYLE_DS_SEMIBOLD_15] = "ds_semibold_15",
    [A26_FONT_STYLE_DS_REG_10]      = "ds_reg_10",
    [A26_FONT_STYLE_DS_BOLD_10]     = "ds_bold_10",
    [A26_FONT_STYLE_DS_REG_12]      = "ds_reg_12",
    [A26_FONT_STYLE_DS_BOLD_12]     = "ds_bold_12",
    [A26_FONT_STYLE_DS_BOLD_14]     = "ds_bold_14",
    [A26_FONT_STYLE_DS_BOLD_18]     = "ds_bold_18",
    [A26_FONT_STYLE_DS_MEDIUM_16]   = "ds_medium_16",
};

/* AURA_STYLE_ROLE_* (aura_style_manifest.h, 8 roles de paleta) ->
 * ordinal de a26_token_t (apple2026_shell.h, 9 tokens: el 4.o,
 * A26_ACCENT, no es del tema -- se salta). */
static const int role_to_token[AURA_STYLE_ROLE_COUNT] = { 0, 1, 2, 3, 5, 6, 7, 8 };
#define TOKEN_COUNT 9

/* -- Estado del estilo activo ------------------------------------- */

static int font_ids[A26_FONT_STYLE_COUNT];
static char s_font_paths[A26_FONT_STYLE_COUNT][MAX_PATH];
static char s_active_id[AURA_STYLE_ID_LEN] = AURA_STYLE_ID_DEFAULT;
static unsigned s_generation = 0;

static unsigned s_palette[2][TOKEN_COUNT]; /* [[0]=claro, [1]=oscuro][token] */
static unsigned s_category_settings_gray_rgb24;
static unsigned s_category_video_rgb24;
static unsigned s_category_photos_rgb24;
static unsigned s_category_extras_yellow_rgb24;

int a26_font(a26_font_style_t style)
{
    if (style < 0 || style >= A26_FONT_STYLE_COUNT)
        return FONT_SYSFIXED;
    return font_ids[style];
}

const char *aura_style_active_id(void)
{
    return s_active_id;
}

bool aura_style_is_default(void)
{
    return !strcmp(s_active_id, AURA_STYLE_ID_DEFAULT);
}

unsigned aura_style_generation(void)
{
    return s_generation;
}

/* -- Paleta --------------------------------------------------------- */

static void set_palette(int dark, int token, unsigned rgb24)
{
    aura_rgb_t c = aura_color_from_rgb24(rgb24);
    s_palette[dark][token] = LCD_RGBPACK(c.r, c.g, c.b);
}

static void reset_palette_to_compiled_defaults(void)
{
    s_palette[0][0] = A26_COLOR_LIGHT_SHELL_BG;
    s_palette[0][1] = A26_COLOR_LIGHT_TEXT_PRIMARY;
    s_palette[0][2] = A26_COLOR_LIGHT_TEXT_SECONDARY;
    s_palette[0][3] = A26_COLOR_LIGHT_TEXT_TERTIARY;
    s_palette[0][5] = A26_COLOR_LIGHT_SHELL_RAIL;
    s_palette[0][6] = A26_COLOR_LIGHT_PROGRESS_FILL;
    s_palette[0][7] = A26_COLOR_LIGHT_PROGRESS_TRACK;
    s_palette[0][8] = A26_COLOR_LIGHT_SELECTION_FILL;

    s_palette[1][0] = A26_COLOR_DARK_SHELL_BG;
    s_palette[1][1] = A26_COLOR_DARK_TEXT_PRIMARY;
    s_palette[1][2] = A26_COLOR_DARK_TEXT_SECONDARY;
    s_palette[1][3] = A26_COLOR_DARK_TEXT_TERTIARY;
    s_palette[1][5] = A26_COLOR_DARK_SHELL_RAIL;
    s_palette[1][6] = A26_COLOR_DARK_PROGRESS_FILL;
    s_palette[1][7] = A26_COLOR_DARK_PROGRESS_TRACK;
    s_palette[1][8] = A26_COLOR_DARK_SELECTION_FILL;

    s_category_settings_gray_rgb24 = AURA_DS_COLOR_CATEGORY_SETTINGS_GRAY_RGB24;
    s_category_video_rgb24 = AURA_DS_COLOR_CATEGORY_VIDEO_RGB24;
    s_category_photos_rgb24 = AURA_DS_COLOR_CATEGORY_PHOTOS_RGB24;
    s_category_extras_yellow_rgb24 = AURA_DS_COLOR_CATEGORY_EXTRAS_YELLOW_RGB24;
}

static void apply_manifest_palette(const aura_style_manifest_t *m)
{
    int i;

    for (i = 0; i < AURA_STYLE_ROLE_COUNT; i++)
    {
        int token = role_to_token[i];
        if (m->palette_light[i] >= 0)
            set_palette(0, token, (unsigned)m->palette_light[i]);
        if (m->palette_dark[i] >= 0)
            set_palette(1, token, (unsigned)m->palette_dark[i]);
    }
    if (m->category_settings_gray >= 0)
        s_category_settings_gray_rgb24 = (unsigned)m->category_settings_gray;
    if (m->category_video >= 0)
        s_category_video_rgb24 = (unsigned)m->category_video;
    if (m->category_photos >= 0)
        s_category_photos_rgb24 = (unsigned)m->category_photos;
    if (m->category_extras_yellow >= 0)
        s_category_extras_yellow_rgb24 = (unsigned)m->category_extras_yellow;
}

unsigned aura_style_palette_color(int token_index, bool dark)
{
    if (token_index < 0 || token_index >= TOKEN_COUNT)
        token_index = 1; /* TEXT_PRIMARY: mismo fallback que a26_color() */
    return s_palette[dark ? 1 : 0][token_index];
}

unsigned aura_style_category_settings_gray_rgb24(void) { return s_category_settings_gray_rgb24; }
unsigned aura_style_category_video_rgb24(void)         { return s_category_video_rgb24; }
unsigned aura_style_category_photos_rgb24(void)        { return s_category_photos_rgb24; }
unsigned aura_style_category_extras_yellow_rgb24(void) { return s_category_extras_yellow_rgb24; }

/* -- Rutas de fuentes ------------------------------------------------ */

static void build_default_font_paths(char paths[A26_FONT_STYLE_COUNT][MAX_PATH])
{
    strlcpy(paths[A26_FONT_STYLE_TITLE],          FONT_DIR "/" A26_FONT_TITLE,          MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_BODY],           FONT_DIR "/" A26_FONT_BODY,           MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_CAPTION],        FONT_DIR "/" A26_FONT_CAPTION,        MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_HEADER],         FONT_DIR "/" A26_FONT_HEADER,         MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_MICRO],          FONT_DIR "/" A26_FONT_MICRO,          MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_REG_8],       FONT_DIR "/" A26_FONT_DS_REG_8,       MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_SEMIBOLD_15], FONT_DIR "/" A26_FONT_DS_SEMIBOLD_15, MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_REG_10],      FONT_DIR "/" A26_FONT_DS_REG_10,      MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_BOLD_10],     FONT_DIR "/" A26_FONT_DS_BOLD_10,     MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_REG_12],      FONT_DIR "/" A26_FONT_DS_REG_12,      MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_BOLD_12],     FONT_DIR "/" A26_FONT_DS_BOLD_12,     MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_BOLD_14],     FONT_DIR "/" A26_FONT_DS_BOLD_14,     MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_BOLD_18],     FONT_DIR "/" A26_FONT_DS_BOLD_18,     MAX_PATH);
    strlcpy(paths[A26_FONT_STYLE_DS_MEDIUM_16],   FONT_DIR "/" A26_FONT_DS_MEDIUM_16,   MAX_PATH);
}

static void build_style_font_paths(const char *id, char paths[A26_FONT_STYLE_COUNT][MAX_PATH])
{
    int i;

    for (i = 0; i < A26_FONT_STYLE_COUNT; i++)
        snprintf(paths[i], MAX_PATH, "%s/%s/fonts/%s.fnt",
                 AURA_STYLES_DIR, id, role_font_names[i]);
}

/* -- Manifiesto (theme.cfg) ------------------------------------------ */

static bool load_manifest(const char *id, aura_style_manifest_t *m)
{
    char path[MAX_PATH];
    char line[128]; /* mas grande que aura.cfg (64): una linea larga en
                      * theme.cfg no debe truncar y arrastrar basura al
                      * resto del archivo (contrato: lineas <= 127). */
    int fd;

    aura_style_manifest_init(m);
    snprintf(path, sizeof(path), "%s/%s/theme.cfg", AURA_STYLES_DIR, id);

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return false;

    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char *name, *value;
        if (!settings_parseline(line, &name, &value))
            continue;
        aura_style_manifest_apply_line(m, name, value);
    }
    close(fd);
    return true;
}

/* Las 14 fuentes existen en disco (chequeo barato, sin decodificar
 * cabeceras) -- usado por aura_style_scan() para decidir filas
 * inertes sin pagar 14 font_load() por cada estilo instalado solo
 * para dibujar la lista. La validacion REAL (que el .fnt no este
 * corrupto) ocurre en try_activate(), con font_load() de verdad. */
static bool style_fonts_exist(const char *id)
{
    char paths[A26_FONT_STYLE_COUNT][MAX_PATH];
    int i;

    build_style_font_paths(id, paths);
    for (i = 0; i < A26_FONT_STYLE_COUNT; i++)
        if (!file_exists(paths[i]))
            return false;
    return true;
}

/* -- Activacion ------------------------------------------------------- */

static bool try_activate(const char *id)
{
    char candidate_paths[A26_FONT_STYLE_COUNT][MAX_PATH];
    aura_style_manifest_t m;
    bool is_default = !strcmp(id, AURA_STYLE_ID_DEFAULT);
    bool all_ok = true;
    int i;

    if (is_default)
    {
        build_default_font_paths(candidate_paths);
    }
    else
    {
        if (!aura_style_id_is_valid(id))
            return false;
        if (!load_manifest(id, &m) || !aura_style_manifest_is_loadable(&m))
            return false;
        build_style_font_paths(id, candidate_paths);
    }

    /* MAXUSERFONTS esta exactamente al limite (font.h) -- no hay cupo
     * para cargar el candidato al lado del actual. font_unload_all()
     * (no font_unload() ida por ida) fuerza el refcount de cada slot a
     * 1 antes de liberarlo -- necesario de verdad: un font_unload()
     * por id, uno a uno via font_ids[], dejaba UN slot sin liberar en
     * la practica (refcount > 1 en algun slot -- font 0 "debe estar
     * disponible al arrancar", font.h -- y font_load() del ultimo rol
     * fallaba por falta de cupo, verificado con captura en el
     * simulador antes de este cambio). */
    font_unload_all();

    for (i = 0; i < A26_FONT_STYLE_COUNT; i++)
    {
        int fid = font_load(candidate_paths[i]);
        font_ids[i] = fid;
        if (fid < 0)
            all_ok = false;
    }

    if (!all_ok && !is_default)
    {
        /* Candidato invalido a mitad de carga: descartar lo que si
         * cargo del candidato y volver EXACTAMENTE a lo que estaba
         * activo antes (s_font_paths todavia tiene esas rutas -- no
         * se tocan hasta el "exito" de abajo). */
        font_unload_all();
        for (i = 0; i < A26_FONT_STYLE_COUNT; i++)
        {
            int fid = font_load(s_font_paths[i]);
            font_ids[i] = (fid >= 0) ? fid : FONT_SYSFIXED;
        }
        s_generation++;
        return false;
    }

    /* Exito -- o es el default, que siempre "tiene exito": los slots
     * que fallaron ya cayeron a FONT_SYSFIXED individualmente arriba,
     * exactamente igual que el a26_shell_init() original. */
    for (i = 0; i < A26_FONT_STYLE_COUNT; i++)
    {
        strlcpy(s_font_paths[i], candidate_paths[i], MAX_PATH);
        if (font_ids[i] < 0)
            font_ids[i] = FONT_SYSFIXED;
    }

    reset_palette_to_compiled_defaults();
    if (!is_default)
        apply_manifest_palette(&m);

    strlcpy(s_active_id, id, sizeof(s_active_id));
    s_generation++;
    return true;
}

void aura_style_boot(void)
{
    const char *want;
    int i;

    for (i = 0; i < A26_FONT_STYLE_COUNT; i++)
        font_ids[i] = -1;

    want = (aura_settings.style_id[0] != '\0')
               ? aura_settings.style_id : AURA_STYLE_ID_DEFAULT;

    if (!strcmp(want, AURA_STYLE_ID_DEFAULT))
    {
        try_activate(AURA_STYLE_ID_DEFAULT);
        return;
    }

    if (!try_activate(want))
        try_activate(AURA_STYLE_ID_DEFAULT);
}

bool aura_style_activate(const char *id)
{
    return try_activate(id);
}

/* -- Lista de instalados ---------------------------------------------- */

int aura_style_scan(aura_style_entry_t *out, int max)
{
    char dir[MAX_PATH];
    DIR *d;
    struct DIRENT *entry;
    int n = 0;

    if (max <= 0)
        return 0;

    strlcpy(out[0].id, AURA_STYLE_ID_DEFAULT, sizeof(out[0].id));
    strlcpy(out[0].name, "Aura", sizeof(out[0].name));
    out[0].loadable = true;
    n = 1;

    strlcpy(dir, AURA_STYLES_DIR, sizeof(dir));
    d = opendir(dir);
    if (!d)
        return n;

    while (n < max && (entry = readdir(d)) != NULL)
    {
        /* copia acotada a AURA_STYLE_ID_LEN (33): aura_style_id_is_valid()
         * ya rechaza cualquier d_name mas largo, pero copiar antes de
         * construir rutas le prueba a gcc un origen chico de verdad
         * (-Wformat-truncation) en vez del MAX_PATH (260) que declara
         * struct DIRENT -- sin esto, snprintf() de abajo asume el peor
         * caso posible de d_name y lo marca como truncable. */
        char id[AURA_STYLE_ID_LEN];
        aura_style_manifest_t m;
        char theme_cfg[MAX_PATH];
        bool has_manifest;

        if (entry->d_name[0] == '.')
            continue; /* "." / ".." / ocultos */
        if (!aura_style_id_is_valid(entry->d_name))
            continue;
        strlcpy(id, entry->d_name, sizeof(id));

        snprintf(theme_cfg, sizeof(theme_cfg), "%s/%s/theme.cfg", AURA_STYLES_DIR, id);
        if (!file_exists(theme_cfg))
            continue; /* no es un directorio de estilo */

        strlcpy(out[n].id, id, sizeof(out[n].id));
        has_manifest = load_manifest(id, &m);
        if (has_manifest && m.has_name)
            strlcpy(out[n].name, m.name, sizeof(out[n].name));
        else
            strlcpy(out[n].name, id, sizeof(out[n].name));
        out[n].loadable = has_manifest
                           && aura_style_manifest_is_loadable(&m)
                           && style_fonts_exist(id);
        n++;
    }
    closedir(d);

    return n;
}

/* Borrado recursivo propio (sin pasar por apps/fileop.c: esa ruta esta
 * atada al navegador de archivos de Rockbox -- pide confirmacion con
 * SU PROPIO dialogo, cromo que Aura no usa nunca, ver CLAUDE.md). Solo
 * remove()/rmdir()/opendir()/readdir() -- sin UI, sin cromo. */
static bool remove_dir_recursive(const char *path)
{
    DIR *d = opendir(path);
    struct DIRENT *entry;
    bool ok = true;

    if (!d)
        return false;

    while ((entry = readdir(d)) != NULL)
    {
        /* 2x MAX_PATH: `path` y `entry->d_name` son cada uno hasta
         * MAX_PATH -- el arbol real de un tema nunca se acerca a esto
         * (AURA_STYLES_DIR/<id de <=32>/fonts|icons/.../archivo.bmp),
         * pero un buffer del tamano justo hace que gcc no pueda probar
         * que snprintf() no trunca (-Wformat-truncation). */
        char child[MAX_PATH * 2];
        struct dirinfo info;

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        info = dir_get_info(d, entry);
        if (info.attribute & ATTR_DIRECTORY)
            ok = remove_dir_recursive(child) && ok;
        else
            ok = (remove(child) >= 0) && ok;
    }
    closedir(d);

    return (rmdir(path) >= 0) && ok;
}

bool aura_style_delete(const char *id)
{
    char path[MAX_PATH];

    if (!aura_style_id_is_valid(id))
        return false;

    snprintf(path, sizeof(path), "%s/%s", AURA_STYLES_DIR, id);
    return remove_dir_recursive(path);
}

/* -- Iconos, con fallback por archivo al default ----------------------- */

int aura_style_read_icon_bmp(const char *rel, struct bitmap *bm, size_t bufsize)
{
    char path[MAX_PATH];
    int ret;

    if (!aura_style_is_default())
    {
        snprintf(path, sizeof(path), "%s/%s/icons/%s",
                  AURA_STYLES_DIR, s_active_id, rel);
        ret = read_bmp_file(path, bm, bufsize, FORMAT_NATIVE, NULL);
        if (ret > 0)
            return ret;
    }

    snprintf(path, sizeof(path), "%s/aura/%s", ICON_DIR, rel);
    return read_bmp_file(path, bm, bufsize, FORMAT_NATIVE, NULL);
}
