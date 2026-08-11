#include <string.h>
#include <stdio.h>

/* config.h primero: recorder/albumart.h chequea "#if HAVE_ALBUMART" antes
 * de incluirlo el mismo. Ver el comentario equivalente en aura_music.c
 * (D-021) -- lcd.h ya lo arrastra, pero se deja explicito. */
#include "config.h"
#include "lcd.h"
#include "font.h"
#include "button.h"
#include "audio.h"
#include "file.h"
#include "string-extra.h"
#include "recorder/albumart.h"
#include "recorder/bmp.h"
#include "recorder/jpeg_load.h"
#include "misc.h"
#include "sound.h"
#include "settings.h"
#include "status.h"
#include "tick.h"
#include "tagcache.h"

#include "aura_nowplaying.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "aura_lrc.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_widgets.h"
#include "aura_art.h"
#include "aura_motion.h"
#include "aura_music.h"

/* -- Layout (doc "Reproductor - Ahora suena.md" SS2) -------------------- */
#define ART_SIZE   84
#define ART_X      A26_SPACING_LG
#define ART_Y      28
#define REFL_GAP   2

#define TEXT_X     (ART_X + ART_SIZE + A26_SPACING_LG)
#define TEXT_W     (A26_SCREEN_WIDTH - TEXT_X - A26_SPACING_LG)

#define STAR_COUNT 5

#define PROGRESS_Y      (A26_SCREEN_HEIGHT - 34)
#define PROGRESS_H      A26_SPACING_SM
#define PROGRESS_X      40
#define PROGRESS_W      (A26_SCREEN_WIDTH - 2 * PROGRESS_X)
#define TRANSPORT_Y     (A26_SCREEN_HEIGHT - 14)

#define LRC_FILE_BUF_SIZE 8192

/* Overlay de volumen (Fase 17, PLAN-UX.md, wheel=volumen): visible
 * ~1.5s despues del ultimo scroll. */
#define VOLUME_OVERLAY_TICKS (HZ + HZ / 2)
static long s_volume_overlay_until = 0;

/* Panel flotante de "anadir a lista" (modo 5.3, doc SS5): visible
 * mientras el modo esta activo; ~1.5s de confirmacion tras SELECT antes
 * de que el rotulo vuelva a mostrar el nombre de la lista resaltada. */
#define PLAYLIST_CONFIRM_TICKS (HZ + HZ / 2)
static long s_playlist_confirm_until = 0;

static unsigned char s_art_buf[64 * 1024];
static struct bitmap s_art_bm;
static bool s_art_valid = false;
static unsigned char s_reflection_buf[ART_SIZE * (ART_SIZE * AURA_ART_REFLECTION_HEIGHT_PCT / 100) * sizeof(fb_data)];

static aura_lrc_t s_lrc;
static bool s_lrc_valid = false;

static char s_loaded_path[MAX_PATH];

/* -- Modos de la rueda (doc SS5): un icono activo a la vez, SELECT
 * cicla. El modo Letra (aura_nowplaying_wheel_lyrics) muestra la vista
 * de letra mientras esta activo -- no hay un toggle separado, "el modo
 * activo" y "la letra visible" son el mismo estado (simplificacion
 * deliberada: el doc describe un panel comprimido FULL-CARRY con la
 * lista de modos en columna, que no se construyo esta pasada -- ver
 * D-078). */
typedef enum {
    NP_MODE_VOLUME = 0,
    NP_MODE_SCRUB,
    NP_MODE_PLAYLIST,
    NP_MODE_LYRICS,
    NP_MODE_STARS,
    NP_MODE_COUNT,
} np_mode_t;

static const char *const MODE_ICONS[NP_MODE_COUNT] = {
    "volume-2", "scrub", "playlist-add", "lyrics", "star",
};

static np_mode_t s_mode = NP_MODE_VOLUME;

/* Resorte del icono que se activa (doc SS5: "mismo resorte corto con
 * sobrepaso que la pastilla de seleccion") -- un pequeno salto vertical
 * en vez de aparecer de golpe. El que se desactiva no se anima (fundido
 * lineal simple per doc, que en un icono binario ya-coloreado equivale
 * a simplemente redibujarlo en TEXT_TERTIARY sin mas). */
#define MODE_POP_TICKS (HZ * AURA_MOTION_SPRING_MS / 1000)
static long s_mode_pop_since = -1000000;

/* Panel de anadir a lista (modo 5.3). */
static int s_playlist_sel = -1; /* -1 = sin seleccion todavia, ver cycle_mode() */

/* Vista previa en vivo de la posicion mientras se escrubea (modo 5.2) --
 * separada de id3->elapsed real hasta soltar, para que el numero y el
 * relleno respondan a la rueda sin parpadeo (doc SS5.2) ni cambiar la
 * posicion real en cada tick de scroll (solo al confirmar el gesto).
 * -1 = sin vista previa activa (se usa el elapsed real). */
static long s_scrub_preview_ms = -1;

int aura_nowplaying_wheel_animating(void)
{
    long elapsed = current_tick - s_mode_pop_since;
    return elapsed >= 0 && elapsed < MODE_POP_TICKS;
}

bool aura_nowplaying_active(void)
{
    return (audio_status() & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)) != 0;
}

static void derive_sibling_path(const char *audio_path, const char *new_ext,
                                 char *out, size_t outsz)
{
    char *dot;

    strlcpy(out, audio_path, outsz);
    dot = strrchr(out, '.');
    if (dot && (!strchr(dot, '/')))
        *dot = '\0';
    strlcat(out, new_ext, outsz);
}

static bool load_album_art(const struct mp3entry *id3)
{
    char path[MAX_PATH];
    struct dim d = { ART_SIZE, ART_SIZE };
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int len;
    int ret;

    if (!find_albumart(id3, path, sizeof(path), &d))
        return false;

    s_art_bm.width = ART_SIZE;
    s_art_bm.height = ART_SIZE;
    s_art_bm.data = (char *)s_art_buf;
#if (LCD_DEPTH > 1)
    s_art_bm.maskdata = NULL;
#endif

    len = (int)strlen(path);
    if (len > 4 && !strcasecmp(path + len - 4, ".bmp"))
        ret = read_bmp_file(path, &s_art_bm, sizeof(s_art_buf), format, NULL);
    else
        ret = read_jpeg_file(path, &s_art_bm, sizeof(s_art_buf), format, NULL);

    if (ret <= 0)
        return false;

    aura_art_generate_reflection((const fb_data *)s_art_bm.data,
                                  (fb_data *)s_reflection_buf,
                                  ART_SIZE, a26_color(A26_SHELL_BG));
    return true;
}

static bool load_lyrics(const char *audio_path)
{
    static char buf[LRC_FILE_BUF_SIZE];
    char lrc_path[MAX_PATH];
    int fd, n;

    derive_sibling_path(audio_path, ".lrc", lrc_path, sizeof(lrc_path));

    fd = open(lrc_path, O_RDONLY);
    if (fd < 0)
        return false;

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return false;

    buf[n] = '\0';
    aura_lrc_parse(buf, &s_lrc);
    return s_lrc.count > 0;
}

static void reload_for_track(const struct mp3entry *id3)
{
    if (!strcmp(s_loaded_path, id3->path))
        return;

    strlcpy(s_loaded_path, id3->path, sizeof(s_loaded_path));
    s_art_valid = load_album_art(id3);
    s_lrc_valid = load_lyrics(id3->path);
    s_scrub_preview_ms = -1; /* la cancion nueva arranca en su propio elapsed real */
    /* El modo NO se reinicia entre canciones (doc: no lo pide, y volver
     * siempre a Volumen al cambiar de tema seria mas sorpresa que
     * ayuda) -- si el modo activo era Letra y la cancion nueva no tiene
     * .lrc, la pantalla ya cae de vuelta al reproductor (ver
     * aura_nowplaying_draw). */
}

/* Nombre propio (no "format_time" a secas): apps/misc.h -- ahora
 * incluido para adjust_volume() -- ya declara un format_time() publico
 * con firma distinta; colisionaban. */
static void aura_format_track_time(unsigned long ms, char *buf, size_t bufsz)
{
    unsigned long total_s = ms / 1000;
    snprintf(buf, bufsz, "%lu:%02lu", total_s / 60, total_s % 60);
}

/* -- Caratula + reflejo (doc SS3) ---------------------------------------- */

/* Radio de esquina de la caratula (doc "Reproductor - Ahora suena.md"
 * SS3: "cuadrado, radio de esquina 8px" -- nivel 2 de la escala
 * concentrica, doc de diseno SS5.4). AUDITORIA-01 A-07: la caratula se
 * dibujaba con lcd_bitmap() directo, esquinas vivas -- vacio real, no
 * decision. Se recorta DESPUES de dibujar (la imagen ya existe en el
 * framebuffer), con la misma primitiva de corte por distancia que el
 * resto del sistema. */
#define COVER_RADIUS A26_LAYOUT_CORNER_RADIUS_CARD

static void draw_cover(void)
{
    if (s_art_valid)
    {
        int refl_h = aura_art_reflection_height(ART_SIZE);
        unsigned bg = a26_color(A26_SHELL_BG);

        lcd_bitmap((const fb_data *)s_art_bm.data, ART_X, ART_Y, ART_SIZE, ART_SIZE);
        a26_shell_round_bitmap_corners(ART_X, ART_Y, ART_SIZE, ART_SIZE, COVER_RADIUS, bg);

        lcd_bitmap((const fb_data *)s_reflection_buf, ART_X, ART_Y + ART_SIZE + REFL_GAP,
                   ART_SIZE, refl_h);
    }
    else
    {
        lcd_set_foreground(a26_color(A26_SHELL_RAIL));
        lcd_drawrect(ART_X, ART_Y, ART_SIZE, ART_SIZE);
    }
}

/* -- Rating / estrellas (doc SS4) ---------------------------------------- */

/* El rating nativo de Rockbox es 0-10 (tagcache tag_rating, ver
 * onplay.c set_rating_inline() -- mismo mecanismo de persistencia que
 * usa Aura aca, tagcache_update_numeric()); la UI de Aura pide 0-5
 * estrellas (doc SS4/SS5.5). Mapeo par: estrella N <-> rating N*2. */
static int stars_from_rating(int rating)
{
    int stars = (rating + 1) / 2;
    if (stars < 0) stars = 0;
    if (stars > STAR_COUNT) stars = STAR_COUNT;
    return stars;
}

static void commit_rating(struct mp3entry *id3, int stars)
{
    if (stars < 0) stars = 0;
    if (stars > STAR_COUNT) stars = STAR_COUNT;

    id3->rating = stars * 2;
    if (id3->tagcache_idx && global_settings.runtimedb)
        tagcache_update_numeric(id3->tagcache_idx - 1, tag_rating, id3->rating);
}

static void draw_stars(int x, int y, int stars, bool editing)
{
    /* Control de valor, no icono de menu: SI usa la variante .fill para
     * las llenas (doc SS4, unica excepcion documentada a "nunca .fill").
     * Editando (modo Estrellas activo): llenas en ACCENT -- el acento se
     * gana porque se esta editando con la rueda en ese momento
     * (Principio 2); llenas en reposo van en TEXT_PRIMARY; vacias van en
     * SHELL_RAIL siempre, editando o no (AUDITORIA-01 A-16: la variante
     * "-rail" del icono ya existe desde el Lote 5, destrabando el limite
     * de D-010/D-078 que antes solo generaba normal+acento). */
    int i;
    int step = A26_ICON_SIZE_STATUS + A26_SPACING_XS;

    for (i = 0; i < STAR_COUNT; i++)
    {
        bool filled = (i < stars);
        const char *name = filled ? "star-fill" : "star";

        if (!filled)
            aura_widgets_draw_icon_rail(name, A26_ICON_SIZE_STATUS, x + i * step, y);
        else if (editing)
            aura_widgets_draw_icon_selected(name, A26_ICON_SIZE_STATUS, x + i * step, y);
        else
            aura_widgets_draw_icon(name, A26_ICON_SIZE_STATUS, x + i * step, y);
    }
}

/* -- Bloque de texto + fila de modos (doc SS2/SS5) ------------------------ */

static void draw_text_and_modes(const struct mp3entry *id3)
{
    char line[160];
    int w, h;
    int y = ART_Y;
    int i;
    int mode_row_w = NP_MODE_COUNT * A26_ICON_SIZE_MENU
                    + (NP_MODE_COUNT - 1) * A26_SPACING_SM;
    int mode_x = A26_SCREEN_WIDTH - A26_SPACING_LG - mode_row_w;
    int mode_y;

    lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    snprintf(line, sizeof(line), "%s", id3->title ? id3->title : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    lcd_putsxy(TEXT_X, y, (const unsigned char *)line);
    y += h + A26_SPACING_XS;

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    snprintf(line, sizeof(line), "%s", id3->artist ? id3->artist : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    lcd_putsxy(TEXT_X, y, (const unsigned char *)line);
    y += h + A26_SPACING_MD;

    draw_stars(TEXT_X, y, stars_from_rating(id3->rating), s_mode == NP_MODE_STARS);
    y += A26_ICON_SIZE_STATUS + A26_SPACING_MD;

    /* Fila de modos: debajo del bloque de texto, alineada a la derecha
     * de la pantalla (doc SS2). El icono activo va en ACCENT con un
     * pequeno salto de resorte al activarse (SS5); los otros 4 en
     * TEXT_TERTIARY -- variante real desde el Lote 5 de AUDITORIA-01
     * (A-16), ya no la aproximacion con TEXT_PRIMARY que D-078 dejo
     * documentada como limite de D-010. */
    mode_y = y;
    if (mode_y + A26_ICON_SIZE_MENU > ART_Y + ART_SIZE + REFL_GAP)
        mode_y = ART_Y + ART_SIZE + REFL_GAP - A26_ICON_SIZE_MENU; /* no invade el reflejo */

    for (i = 0; i < NP_MODE_COUNT; i++)
    {
        int icon_x = mode_x + i * (A26_ICON_SIZE_MENU + A26_SPACING_SM);
        int icon_y = mode_y;
        bool active = (i == (int)s_mode);

        if (active && aura_nowplaying_wheel_animating())
        {
            int eased = aura_motion_spring(current_tick - s_mode_pop_since, MODE_POP_TICKS);
            /* Salto vertical corto (doc: resorte de la pastilla,
             * reaplicado aca) -- hasta 4px de recorrido, con el mismo
             * sobrepaso que ya se ve en la pastilla de seleccion. */
            icon_y -= (4 * (256 - eased)) / 256;
        }

        if (active)
            aura_widgets_draw_icon_selected(MODE_ICONS[i], A26_ICON_SIZE_MENU, icon_x, icon_y);
        else
            aura_widgets_draw_icon_tertiary(MODE_ICONS[i], A26_ICON_SIZE_MENU, icon_x, icon_y);
    }
}

/* -- Pastilla de progreso + transporte (doc SS2) -------------------------- */

static void draw_progress(const struct mp3entry *id3, int scrub_preview_ms)
{
    char timebuf[24], timebuf2[24];
    int w, h;
    int fill_w;
    unsigned long elapsed = (scrub_preview_ms >= 0) ? (unsigned long)scrub_preview_ms : id3->elapsed;

    a26_shell_fill_rounded_rect(PROGRESS_X, PROGRESS_Y, PROGRESS_W, PROGRESS_H,
                                 PROGRESS_H / 2, a26_color(A26_PROGRESS_TRACK),
                                 a26_color(A26_SHELL_BG));
    fill_w = id3->length ? (int)((unsigned long long)PROGRESS_W * elapsed / id3->length) : 0;
    if (fill_w > 0)
        a26_shell_fill_rounded_rect(PROGRESS_X, PROGRESS_Y, fill_w, PROGRESS_H,
                                     PROGRESS_H / 2, a26_color(A26_PROGRESS_FILL),
                                     a26_color(A26_SHELL_BG));

    aura_format_track_time(elapsed, timebuf, sizeof(timebuf));
    aura_format_track_time(id3->length, timebuf2, sizeof(timebuf2));
    lcd_setfont(a26_font(A26_FONT_STYLE_MICRO));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_putsxy(PROGRESS_X, PROGRESS_Y - A26_SPACING_MD, (const unsigned char *)timebuf);
    lcd_getstringsize((const unsigned char *)timebuf2, &w, &h);
    lcd_putsxy(PROGRESS_X + PROGRESS_W - w, PROGRESS_Y - A26_SPACING_MD,
               (const unsigned char *)timebuf2);
}

static void draw_transport(void)
{
    int status = audio_status();
    bool paused = (status & AUDIO_STATUS_PAUSE) != 0;
    int cx = A26_SCREEN_WIDTH / 2;
    int y = TRANSPORT_Y;

    /* Repetir (izq.) / retroceder-play/pausa-avanzar (centro) / aleatorio
     * (der.) -- doc SS2. Son indicadores de estado real (repeat_mode,
     * playlist_shuffle) mas los glifos de transporte; la interaccion en
     * si ya existe (BUTTON_LEFT/RIGHT = pista ant./sig., PLAY global
     * desde Fase 29) -- esta fila solo la hace visible en la pantalla,
     * no agrega botones nuevos.
     *
     * Los iconos son bitmaps ya horneados en un color fijo (D-010):
     * lcd_set_foreground() no los tine, asi que "activo" se expresa
     * eligiendo la variante -on/acento (aura_widgets_draw_icon_selected)
     * en vez de intentar recolorear -- no hay variante TEXT_TERTIARY
     * horneada, mismo limite que el resto de la pantalla (D-078). */
    if (global_settings.repeat_mode != 0)
        aura_widgets_draw_icon_selected("repeat", A26_ICON_SIZE_STATUS, A26_SPACING_XXL, y);
    else
        aura_widgets_draw_icon("repeat", A26_ICON_SIZE_STATUS, A26_SPACING_XXL, y);

    if (global_settings.playlist_shuffle)
        aura_widgets_draw_icon_selected("shuffle", A26_ICON_SIZE_STATUS,
                                         A26_SCREEN_WIDTH - A26_SPACING_XXL - A26_ICON_SIZE_STATUS, y);
    else
        aura_widgets_draw_icon("shuffle", A26_ICON_SIZE_STATUS,
                                A26_SCREEN_WIDTH - A26_SPACING_XXL - A26_ICON_SIZE_STATUS, y);

    aura_widgets_draw_icon("backward", A26_ICON_SIZE_STATUS,
                            cx - A26_ICON_SIZE_STATUS - A26_SPACING_LG - A26_ICON_SIZE_STATUS / 2, y);
    aura_widgets_draw_icon(paused ? "play" : "pause", A26_ICON_SIZE_STATUS,
                            cx - A26_ICON_SIZE_STATUS / 2, y);
    aura_widgets_draw_icon("forward", A26_ICON_SIZE_STATUS,
                            cx + A26_SPACING_LG + A26_ICON_SIZE_STATUS / 2, y);
}

/* -- Modo 5.3: anadir a lista (doc SS5.3) --------------------------------- */

static void draw_playlist_panel(void)
{
    char labels[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
    int n, i;
    int box_h = 28;
    int box_w = 220;
    int box_x = (A26_SCREEN_WIDTH - box_w) / 2;
    int box_y = TRANSPORT_Y - box_h - A26_SPACING_LG - 20;
    int w, h;
    const char *label;

    n = aura_music_list_playlists(labels, AURA_MUSIC_MAX_ITEMS);
    /* Solo para mostrar -- aura_music_add_track_to_playlist() vuelve a
     * resolver el archivo real por indice, no usa estas cadenas (Fase
     * 32, D-081: ".m3u8" a la vista es jerga tecnica de archivo). */
    for (i = 0; i < n; i++)
        aura_music_playlist_display_name(labels[i], labels[i], AURA_MUSIC_ITEM_LEN);

    /* Panel flotante, NUNCA pantalla completa (Principio 3, doc SS5,
     * nota de implementacion 5.3) -- pastilla SELECTION_FILL igual que
     * una fila de lista, sobre lo que ya esta dibujado. */
    a26_shell_fill_rounded_rect(box_x, box_y, box_w, box_h,
                                 A26_LAYOUT_CORNER_RADIUS_PILL,
                                 a26_color(A26_SELECTION_FILL), a26_color(A26_SHELL_BG));

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));

    if (current_tick < s_playlist_confirm_until)
    {
        label = aura_str(AURA_STR_PLAYLIST_ADDED);
        lcd_set_foreground(a26_color(A26_ACCENT));
    }
    else if (n == 0)
    {
        label = aura_str(AURA_STR_EMPTY_LIST);
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    }
    else if (s_playlist_sel < 0)
    {
        /* Sin seleccion todavia (doc SS8): pista neutra, no una lista ya
         * resaltada -- Select en este estado no confirma nada. */
        label = aura_str(AURA_STR_PLAYLIST_PICK);
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    }
    else
    {
        if (s_playlist_sel >= n)
            s_playlist_sel = n - 1;
        label = labels[s_playlist_sel];
        lcd_set_foreground(a26_color(A26_ACCENT));
    }

    lcd_getstringsize((const unsigned char *)label, &w, &h);
    lcd_putsxy(box_x + (box_w - w) / 2, box_y + (box_h - h) / 2, (const unsigned char *)label);
}

/* -- Letra (doc SS6, version simplificada -- ver D-078) ------------------- */

static void draw_lyrics(const struct mp3entry *id3)
{
    int active = aura_lrc_find_active_line(&s_lrc, (long)id3->elapsed);
    int cy = A26_SCREEN_HEIGHT / 2;

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));

    if (active >= 0)
    {
        int w, h;
        const char *text = s_lrc.lines[active].text;
        lcd_getstringsize((const unsigned char *)text, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, cy - h, (const unsigned char *)text);

        if (active + 1 < s_lrc.count)
        {
            lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
            lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
            text = s_lrc.lines[active + 1].text;
            lcd_getstringsize((const unsigned char *)text, &w, &h);
            lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, cy + A26_SPACING_MD,
                       (const unsigned char *)text);
        }
    }
}

/* -- Reproductor completo -------------------------------------------------- */

static void draw_player(const struct mp3entry *id3, int scrub_preview_ms)
{
    draw_cover();
    draw_text_and_modes(id3);
    draw_progress(id3, scrub_preview_ms);
    draw_transport();

    if (s_mode == NP_MODE_PLAYLIST)
        draw_playlist_panel();
}

/* Overlay temporal encima del reproductor mientras el usuario gira el
 * clickwheel en modo Volumen -- no limpia pantalla, se dibuja sobre lo
 * que ya esta. El rango va contra el limite efectivo (el menor entre el
 * maximo real del hardware y volume_limit, D-051), no el maximo
 * absoluto, para que la barra siempre se vea llena al tope real. */
static void draw_volume_overlay(void)
{
    int box_w = 200, box_h = 40;
    int box_x = (A26_SCREEN_WIDTH - box_w) / 2;
    int box_y = A26_SCREEN_HEIGHT - box_h - A26_SPACING_XXL;
    int bar_x = box_x + A26_SPACING_LG;
    int bar_w = box_w - 2 * A26_SPACING_LG;
    int bar_y = box_y + box_h / 2;
    int vol_min = sound_min(SOUND_VOLUME);
    int vol_max = sound_max(SOUND_VOLUME);
    int fill_w;
    char buf[8];

    if (global_settings.volume_limit < vol_max)
        vol_max = global_settings.volume_limit;

    a26_shell_outline_rounded_rect(box_x, box_y, box_w, box_h,
                                    A26_LAYOUT_CORNER_RADIUS_CAPSULE,
                                    a26_color(A26_SHELL_BG), a26_color(A26_SHELL_RAIL),
                                    a26_color(A26_SHELL_BG));

    /* Misma pieza que el resto del sistema (AUDITORIA-01 A-14/A-f):
     * carril PROGRESS_TRACK relleno + PROGRESS_FILL, extremos
     * redondeados -- no un rectangulo delineado con relleno en ACCENT.
     * Ambiguedad A-f resuelta: PROGRESS_FILL es siempre el relleno de
     * progreso/nivel, ACCENT se reserva para iconos y estado (mismo
     * criterio que D-081 ya aplico a Brillo/Limite volumen). */
    a26_shell_fill_rounded_rect(bar_x, bar_y, bar_w, A26_SPACING_SM, A26_SPACING_SM / 2,
                                 a26_color(A26_PROGRESS_TRACK), a26_color(A26_SHELL_BG));
    fill_w = (vol_max > vol_min)
        ? (bar_w * (global_status.volume - vol_min)) / (vol_max - vol_min)
        : 0;
    if (fill_w < 0)      fill_w = 0;
    if (fill_w > bar_w)  fill_w = bar_w;
    if (fill_w > 0)
        a26_shell_fill_rounded_rect(bar_x, bar_y, fill_w, A26_SPACING_SM, A26_SPACING_SM / 2,
                                     a26_color(A26_PROGRESS_FILL), a26_color(A26_SHELL_BG));

    lcd_setfont(a26_font(A26_FONT_STYLE_MICRO));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    snprintf(buf, sizeof(buf), "%d%%",
             (vol_max > vol_min)
                 ? (100 * (global_status.volume - vol_min)) / (vol_max - vol_min)
                 : 0);
    lcd_putsxy(box_x + A26_SPACING_LG, box_y + A26_SPACING_XS,
               (const unsigned char *)buf);
}

void aura_nowplaying_draw(void)
{
    struct mp3entry *id3 = audio_current_track();

    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_NOWPLAYING), 1);

    if (!id3)
        return;

    reload_for_track(id3);

    if (s_mode == NP_MODE_LYRICS && s_lrc_valid)
        draw_lyrics(id3);
    else
        draw_player(id3, (int)s_scrub_preview_ms);

    if (current_tick < s_volume_overlay_until)
        draw_volume_overlay();
}

bool aura_nowplaying_needs_tick(void)
{
    return current_tick < s_volume_overlay_until
        || aura_nowplaying_wheel_animating()
        || current_tick < s_playlist_confirm_until;
}

static void cycle_mode(int direction)
{
    int next = ((int)s_mode + direction + NP_MODE_COUNT) % NP_MODE_COUNT;
    s_mode = (np_mode_t)next;
    s_mode_pop_since = current_tick;
    s_scrub_preview_ms = -1;
    /* Doc SS8 (anti-patron): "Seleccion inicial activa en listas
     * destructivas -- anadir a playlist debe entrar sin seleccion".
     * Sin este reset, entrar al modo Playlist ya dejaba la primera lista
     * pre-resaltada -- un SELECT reflejo (el mismo gesto que cicla en
     * cualquier otro modo) agregaba la cancion sin que el usuario
     * hubiera elegido nada a proposito (Fase 32, D-081). */
    if (s_mode == NP_MODE_PLAYLIST)
        s_playlist_sel = -1;
}

static int playlist_count(void)
{
    char labels[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
    return aura_music_list_playlists(labels, AURA_MUSIC_MAX_ITEMS);
}

void aura_nowplaying_handle_button(aura_nav_t *nav, long button)
{
    struct mp3entry *id3 = audio_current_track();

    switch (button)
    {
    case BUTTON_SELECT:
        /* Select cicla los modos de la rueda (doc base SS7, extendido a
         * 5 modos por SS5 de este documento) -- CON una excepcion
         * explicita del propio doc (fila 5.3: "Select confirma anadir"):
         * en modo Playlist CON al menos una playlist real para elegir,
         * Select confirma en vez de ciclar, para que el usuario pueda
         * agregar a varias listas seguidas sin volver a entrar al modo
         * cada vez. Bug real encontrado probando la pantalla (no en
         * teoria): sin este chequeo de "hay algo que confirmar", con
         * cero playlists creadas Select nunca volvia a llamar
         * cycle_mode() estando en este modo -- Letra y Estrellas
         * quedaban inalcanzables, no solo Playlist atascado (ese caso ya
         * lo cubre el escape de MENU). Entrar/salir de Letra no necesita
         * una excepcion propia: ya es simplemente que el modo activo sea
         * o no NP_MODE_LYRICS (simplificacion respecto al panel
         * comprimido del doc, ver D-078). */
        if (s_mode == NP_MODE_PLAYLIST && id3 && s_playlist_sel >= 0 && playlist_count() > 0)
        {
            if (aura_music_add_track_to_playlist(s_playlist_sel, id3->path))
                s_playlist_confirm_until = current_tick + PLAYLIST_CONFIRM_TICKS;
        }
        else if (s_mode != NP_MODE_PLAYLIST || s_playlist_sel < 0)
        {
            cycle_mode(1);
        }
        break;
    case BUTTON_RIGHT:
        audio_next();
        break;
    case BUTTON_LEFT:
        audio_prev();
        break;
    case BUTTON_SCROLL_FWD:
    case BUTTON_SCROLL_BACK:
    {
        int dir = (button == BUTTON_SCROLL_FWD) ? 1 : -1;

        switch (s_mode)
        {
        case NP_MODE_VOLUME:
            adjust_volume(dir);
            s_volume_overlay_until = current_tick + VOLUME_OVERLAY_TICKS;
            break;

        case NP_MODE_SCRUB:
            if (id3)
            {
                long base = (s_scrub_preview_ms >= 0) ? s_scrub_preview_ms : (long)id3->elapsed;
                long delta = 3000L * dir; /* 3s por click, doc SS5.2 */
                long next = base + delta;

                if (next < 0) next = 0;
                if ((unsigned long)next > id3->length) next = (long)id3->length;

                s_scrub_preview_ms = next;
                audio_ff_rewind(next);
            }
            break;

        case NP_MODE_PLAYLIST:
        {
            char labels[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
            int n = aura_music_list_playlists(labels, AURA_MUSIC_MAX_ITEMS);
            if (n > 0)
            {
                /* Primer giro sin seleccion previa: entra a la lista en
                 * vez de calcular un salto relativo a -1 (doc SS8). */
                s_playlist_sel = (s_playlist_sel < 0)
                    ? (dir > 0 ? 0 : n - 1)
                    : (s_playlist_sel + dir + n) % n;
            }
            break;
        }

        case NP_MODE_LYRICS:
            /* El icono no reacciona a la rueda directamente (doc SS5.4). */
            break;

        case NP_MODE_STARS:
            if (id3)
                commit_rating(id3, stars_from_rating(id3->rating) + dir);
            break;

        default:
            break;
        }
        break;
    }
    case BUTTON_MENU:
        /* Doc SS6: "Menu... cierra la letra". Mismo escape para Playlist
         * (SS5.3): con Select reservado para confirmar un agregado ahi,
         * hacia falta una salida -- si no, una vez dentro del modo
         * Playlist no habia forma de volver a ciclar (bug real,
         * encontrado probando la pantalla, no en teoria: Select nunca
         * volvia a llamar cycle_mode() estando en ese modo). MENU vuelve
         * a Volumen para ambos en vez de salir de la pantalla; salir de
         * verdad requiere estar en cualquier otro modo primero (mismo
         * "un paso atras a la vez" que el resto de la navegacion). */
        if (s_mode == NP_MODE_LYRICS || s_mode == NP_MODE_PLAYLIST)
        {
            s_mode = NP_MODE_VOLUME;
            s_mode_pop_since = current_tick;
        }
        else
        {
            aura_nav_pop(nav);
        }
        break;
    default:
        break;
    }
}
