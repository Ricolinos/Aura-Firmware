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

#include "aura_nowplaying.h"
#include "aura_theme.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "aura_lrc.h"
#include "aura_tokens.h"
#include "aura_statusbar.h"

#define ART_SIZE   100
#define ART_X      ((AURA_SCREEN_WIDTH - ART_SIZE) / 2)
#define ART_Y      (AURA_LAYOUT_STATUSBAR_HEIGHT + AURA_SPACING_SM)

#define LRC_FILE_BUF_SIZE 8192

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

static void format_time(unsigned long ms, char *buf, size_t bufsz)
{
    unsigned long total_s = ms / 1000;
    snprintf(buf, bufsz, "%lu:%02lu", total_s / 60, total_s % 60);
}

static void draw_lyrics(const struct mp3entry *id3)
{
    int active = aura_lrc_find_active_line(&s_lrc, (long)id3->elapsed);
    int cy = AURA_SCREEN_HEIGHT / 2;

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));

    if (active >= 0)
    {
        int w, h;
        const char *text = s_lrc.lines[active].text;
        lcd_getstringsize((const unsigned char *)text, &w, &h);
        lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2, cy - h, (const unsigned char *)text);

        if (active + 1 < s_lrc.count)
        {
            lcd_setfont(aura_font(AURA_FONT_STYLE_CAPTION));
            lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
            text = s_lrc.lines[active + 1].text;
            lcd_getstringsize((const unsigned char *)text, &w, &h);
            lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2, cy + AURA_SPACING_MD,
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

    int text_y = ART_Y + ART_SIZE + AURA_SPACING_LG;

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));
    snprintf(line, sizeof(line), "%s", id3->title ? id3->title : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2, text_y, (const unsigned char *)line);
    text_y += h + AURA_SPACING_SM;

    lcd_setfont(aura_font(AURA_FONT_STYLE_CAPTION));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
    snprintf(line, sizeof(line), "%s - %s",
             id3->artist ? id3->artist : "", id3->album ? id3->album : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2, text_y, (const unsigned char *)line);
    text_y += h + AURA_SPACING_LG;

    bar_x = AURA_SPACING_XXL;
    bar_w = AURA_SCREEN_WIDTH - 2 * AURA_SPACING_XXL;
    bar_y = text_y;

    lcd_set_foreground(aura_color(AURA_TOK_BORDER));
    lcd_drawrect(bar_x, bar_y, bar_w, AURA_SPACING_SM);

    fill_w = id3->length ? (int)((unsigned long long)bar_w * id3->elapsed / id3->length) : 0;
    lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
    lcd_fillrect(bar_x, bar_y, fill_w, AURA_SPACING_SM);

    format_time(id3->elapsed, timebuf, sizeof(timebuf));
    format_time(id3->length, timebuf2, sizeof(timebuf2));
    lcd_setfont(aura_font(AURA_FONT_STYLE_MICRO));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
    lcd_putsxy(bar_x, bar_y + AURA_SPACING_SM + AURA_SPACING_XS,
               (const unsigned char *)timebuf);
    snprintf(line, sizeof(line), "%s", timebuf2);
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    lcd_putsxy(bar_x + bar_w - w, bar_y + AURA_SPACING_SM + AURA_SPACING_XS,
               (const unsigned char *)line);
    /* El estado de pausa ya lo indica el icono de la barra de estado
     * (Fase 13, PLAN-UX.md, L5) -- sin duplicarlo aca dentro. */
}

void aura_nowplaying_draw(void)
{
    struct mp3entry *id3 = audio_current_track();

    aura_theme_clear_screen();
    aura_statusbar_draw(0, AURA_SCREEN_WIDTH, aura_str(AURA_STR_NOWPLAYING), 1);

    if (!id3)
        return;

    reload_for_track(id3);

    if (s_show_lyrics && s_lrc_valid)
        draw_lyrics(id3);
    else
        draw_player(id3);
}

void aura_nowplaying_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SELECT:
        if (s_lrc_valid)
            s_show_lyrics = !s_show_lyrics;
        break;
    case BUTTON_PLAY:
        if (audio_status() & AUDIO_STATUS_PAUSE)
            audio_resume();
        else
            audio_pause();
        break;
    case BUTTON_RIGHT:
        audio_next();
        break;
    case BUTTON_LEFT:
        audio_prev();
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
