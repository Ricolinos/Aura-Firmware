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

#include "aura_nowplaying.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "aura_lrc.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_widgets.h"

#define ART_SIZE   100
#define ART_X      ((A26_SCREEN_WIDTH - ART_SIZE) / 2)
#define ART_Y      (A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_SM)

#define LRC_FILE_BUF_SIZE 8192

/* Overlay de volumen (Fase 17, PLAN-UX.md, wheel=volumen): visible
 * ~1.5s despues del ultimo scroll. */
#define VOLUME_OVERLAY_TICKS (HZ + HZ / 2)
static long s_volume_overlay_until = 0;

static unsigned char s_art_buf[64 * 1024];
static struct bitmap s_art_bm;
static bool s_art_valid = false;

static aura_lrc_t s_lrc;
static bool s_lrc_valid = false;
static bool s_show_lyrics = false;

static char s_loaded_path[MAX_PATH];

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

    return ret > 0;
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
    s_show_lyrics = false;
}

/* Nombre propio (no "format_time" a secas): apps/misc.h -- ahora
 * incluido para adjust_volume() -- ya declara un format_time() publico
 * con firma distinta; colisionaban. */
static void aura_format_track_time(unsigned long ms, char *buf, size_t bufsz)
{
    unsigned long total_s = ms / 1000;
    snprintf(buf, bufsz, "%lu:%02lu", total_s / 60, total_s % 60);
}

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

static void draw_player(const struct mp3entry *id3)
{
    char timebuf[24], timebuf2[24];
    char line[160];
    int w, h;
    int bar_x, bar_y, bar_w, fill_w;

    if (s_art_valid)
        lcd_bitmap((const fb_data *)s_art_bm.data, ART_X, ART_Y,
                   s_art_bm.width, s_art_bm.height);
    else
        lcd_drawrect(ART_X, ART_Y, ART_SIZE, ART_SIZE);

    int text_y = ART_Y + ART_SIZE + A26_SPACING_LG;

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    snprintf(line, sizeof(line), "%s", id3->title ? id3->title : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, text_y, (const unsigned char *)line);
    text_y += h + A26_SPACING_SM;

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    snprintf(line, sizeof(line), "%s - %s",
             id3->artist ? id3->artist : "", id3->album ? id3->album : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, text_y, (const unsigned char *)line);
    text_y += h + A26_SPACING_LG;

    bar_x = A26_SPACING_XXL;
    bar_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    bar_y = text_y;

    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_drawrect(bar_x, bar_y, bar_w, A26_SPACING_SM);

    fill_w = id3->length ? (int)((unsigned long long)bar_w * id3->elapsed / id3->length) : 0;
    lcd_set_foreground(a26_color(A26_ACCENT));
    lcd_fillrect(bar_x, bar_y, fill_w, A26_SPACING_SM);

    aura_format_track_time(id3->elapsed, timebuf, sizeof(timebuf));
    aura_format_track_time(id3->length, timebuf2, sizeof(timebuf2));
    lcd_setfont(a26_font(A26_FONT_STYLE_MICRO));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_putsxy(bar_x, bar_y + A26_SPACING_SM + A26_SPACING_XS,
               (const unsigned char *)timebuf);
    snprintf(line, sizeof(line), "%s", timebuf2);
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    lcd_putsxy(bar_x + bar_w - w, bar_y + A26_SPACING_SM + A26_SPACING_XS,
               (const unsigned char *)line);
    /* El estado de pausa ya lo indica el icono de la barra de estado
     * (Fase 13, PLAN-UX.md, L5) -- sin duplicarlo aca dentro. */
}

/* Overlay temporal encima del reproductor (o de la letra) mientras el
 * usuario gira el clickwheel -- no limpia pantalla, se dibuja sobre lo
 * que ya esta. El rango va contra el limite efectivo (el menor entre
 * el maximo real del hardware y volume_limit, D-051), no el maximo
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

    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_fillrect(box_x, box_y, box_w, box_h);
    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_drawrect(box_x, box_y, box_w, box_h);

    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_drawrect(bar_x, bar_y, bar_w, A26_SPACING_SM);
    fill_w = (vol_max > vol_min)
        ? (bar_w * (global_status.volume - vol_min)) / (vol_max - vol_min)
        : 0;
    if (fill_w < 0)      fill_w = 0;
    if (fill_w > bar_w)  fill_w = bar_w;
    lcd_set_foreground(a26_color(A26_ACCENT));
    lcd_fillrect(bar_x, bar_y, fill_w, A26_SPACING_SM);

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

    if (s_show_lyrics && s_lrc_valid)
        draw_lyrics(id3);
    else
        draw_player(id3);

    if (current_tick < s_volume_overlay_until)
        draw_volume_overlay();
}

bool aura_nowplaying_needs_tick(void)
{
    return current_tick < s_volume_overlay_until;
}

void aura_nowplaying_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SELECT:
        if (s_lrc_valid)
            s_show_lyrics = !s_show_lyrics;
        break;
    /* BUTTON_PLAY ya NO se maneja aca -- es global desde Fase 29
     * (aura_screens_handle_button() lo intercepta antes de despachar a
     * cualquier pantalla, doc de comportamiento SS7). */
    case BUTTON_RIGHT:
        audio_next();
        break;
    case BUTTON_LEFT:
        audio_prev();
        break;
    case BUTTON_SCROLL_FWD:
        /* Wheel = volumen (PLAN-UX.md, Fase 17) -- adjust_volume() ya
         * reusa el backend real (sound_set_volume(), que respeta
         * volume_limit, D-051) en vez de reimplementar el clamp. */
        adjust_volume(1);
        s_volume_overlay_until = current_tick + VOLUME_OVERLAY_TICKS;
        break;
    case BUTTON_SCROLL_BACK:
        adjust_volume(-1);
        s_volume_overlay_until = current_tick + VOLUME_OVERLAY_TICKS;
        break;
    case BUTTON_MENU:
        if (s_show_lyrics)
            s_show_lyrics = false;
        else
            aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
