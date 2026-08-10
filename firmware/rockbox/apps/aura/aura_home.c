#include <string.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "tick.h"
#include "system.h"

#include "aura_home.h"
#include "aura_music.h"
#include "aura_albumart.h"
#include "aura_theme.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "aura_tokens.h"

#define HOME_ART_SIZE     96
#define HOME_ART_Y        6
#define HOME_DIVIDER_Y    112
#define HOME_LIST_TOP     118
#define HOME_ROW_HEIGHT   24
#define HOME_ROTATE_TICKS (HZ * 4)
#define HOME_FADE_TICKS   HZ

typedef struct {
    aura_str_id_t label_id;
    aura_screen_id_t target;
} home_entry_t;

static const home_entry_t s_root_entries[] = {
    { AURA_STR_MUSIC,      AURA_SCREEN_MUSIC },
    { AURA_STR_VIDEOS,     AURA_SCREEN_VIDEOS },
    { AURA_STR_PHOTOS,     AURA_SCREEN_PHOTOS },
    { AURA_STR_NOWPLAYING, AURA_SCREEN_NOWPLAYING },
    { AURA_STR_SETTINGS,   AURA_SCREEN_SETTINGS },
};
#define ROOT_COUNT ((int)(sizeof(s_root_entries) / sizeof(s_root_entries[0])))

static bool s_initialized = false;

static aura_music_item_t s_albums[AURA_MUSIC_MAX_ITEMS];
static int s_album_count = -1; /* -1 = todavia no se conto */

static aura_albumart_t s_art_a, s_art_b;
static unsigned char s_art_a_cover[HOME_ART_SIZE * HOME_ART_SIZE * sizeof(fb_data)];
static unsigned char s_art_a_refl[HOME_ART_SIZE * (HOME_ART_SIZE / 2) * sizeof(fb_data)];
static unsigned char s_art_b_cover[HOME_ART_SIZE * HOME_ART_SIZE * sizeof(fb_data)];
static unsigned char s_art_b_refl[HOME_ART_SIZE * (HOME_ART_SIZE / 2) * sizeof(fb_data)];

static int s_art_index = 0;
static bool s_fading = false;
static long s_next_rotate_tick = 0;
static long s_fade_start_tick = 0;

static void ensure_albums(void)
{
    if (s_album_count >= 0)
        return;
    s_album_count = aura_music_browse(AURA_SCREEN_MUSIC_ALBUMS, s_albums, AURA_MUSIC_MAX_ITEMS);
}

static void load_art_slot(aura_albumart_t *art, unsigned char *cover_buf,
                          unsigned char *refl_buf, int album_idx)
{
    art->size = HOME_ART_SIZE;
    art->cover_data = cover_buf;
    art->reflection_data = refl_buf;
    art->valid = false;

    if (s_album_count > 0)
        aura_albumart_load_for_album(s_albums[album_idx % s_album_count].seek, art);
}

/* No se puede consultar tagcache sino hasta que aura_music_db_ready()
 * lo confirme -- y esta pantalla puede evaluarse (via
 * is_split_home_screen() en aura_screens.c) desde el primerisimo
 * frame, antes de que aura_main() haya llamado siquiera una vez a
 * aura_music_db_ready() (que es quien dispara el escaneo inicial, ver
 * D-021/D-022). Si se cachea "0 albumes" en ese momento, se queda asi
 * para siempre. Por eso NO se marca s_initialized hasta que la base
 * este realmente lista. */
static void ensure_init(void)
{
    if (s_initialized || !aura_music_db_ready())
        return;

    ensure_albums();
    if (s_album_count > 0)
        load_art_slot(&s_art_a, s_art_a_cover, s_art_a_refl, 0);

    s_next_rotate_tick = current_tick + HOME_ROTATE_TICKS;
    s_initialized = true;
}

bool aura_home_has_content(void)
{
    ensure_init();
    return s_art_a.valid;
}

bool aura_home_needs_tick(void)
{
    return s_initialized && s_album_count > 1;
}

/* Mezcla `a` y `b` (mismo tamano w x h) con peso `t` (0..255, 0=solo a,
 * 255=solo b) y lo dibuja en (x,y). Usado para el crossfade entre
 * caratulas consecutivas. */
static void blit_crossfade(const fb_data *a, const fb_data *b, int w, int h,
                            int x, int y, int t)
{
    static fb_data scratch[HOME_ART_SIZE * HOME_ART_SIZE];
    int i, n = w * h;

    for (i = 0; i < n; i++)
    {
        int ar = RGB_UNPACK_RED(a[i]);
        int ag = RGB_UNPACK_GREEN(a[i]);
        int ab = RGB_UNPACK_BLUE(a[i]);
        int br = RGB_UNPACK_RED(b[i]);
        int bg = RGB_UNPACK_GREEN(b[i]);
        int bb = RGB_UNPACK_BLUE(b[i]);

        int r = ar + ((br - ar) * t) / 255;
        int g = ag + ((bg - ag) * t) / 255;
        int bl = ab + ((bb - ab) * t) / 255;

        scratch[i] = LCD_RGBPACK(r, g, bl);
    }
    lcd_bitmap(scratch, x, y, w, h);
}

static void advance_rotation(void)
{
    int cx = (AURA_SCREEN_WIDTH - HOME_ART_SIZE) / 2;

    if (!s_fading && s_album_count > 1 && TIME_AFTER(current_tick, s_next_rotate_tick))
    {
        load_art_slot(&s_art_b, s_art_b_cover, s_art_b_refl, s_art_index + 1);
        s_fading = true;
        s_fade_start_tick = current_tick;
    }

    if (!s_art_a.valid && !s_fading)
        return;

    if (s_fading)
    {
        long elapsed = current_tick - s_fade_start_tick;
        int t = (int)(elapsed * 255 / HOME_FADE_TICKS);
        if (t > 255)
            t = 255;

        blit_crossfade((const fb_data *)s_art_a.cover_data, (const fb_data *)s_art_b.cover_data,
                       HOME_ART_SIZE, HOME_ART_SIZE, cx, HOME_ART_Y, t);

        if (elapsed >= HOME_FADE_TICKS)
        {
            memcpy(s_art_a_cover, s_art_b_cover, sizeof(s_art_a_cover));
            s_art_a.valid = s_art_b.valid;
            s_art_index = (s_art_index + 1) % s_album_count;
            s_fading = false;
            s_next_rotate_tick = current_tick + HOME_ROTATE_TICKS;
        }
    }
    else
    {
        lcd_bitmap((const fb_data *)s_art_a.cover_data, cx, HOME_ART_Y,
                   HOME_ART_SIZE, HOME_ART_SIZE);
    }
}

void aura_home_draw(aura_nav_t *nav)
{
    int i;
    int selected = aura_nav_get_selection(nav);

    ensure_init();
    aura_theme_clear_screen();
    advance_rotation();

    lcd_set_foreground(aura_color(AURA_TOK_BORDER));
    lcd_hline(0, AURA_SCREEN_WIDTH - 1, HOME_DIVIDER_Y);

    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    for (i = 0; i < ROOT_COUNT; i++)
    {
        int row_y = HOME_LIST_TOP + i * HOME_ROW_HEIGHT;
        bool sel = (i == selected);

        if (sel)
        {
            lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
            lcd_fillrect(0, row_y, AURA_SCREEN_WIDTH, HOME_ROW_HEIGHT);
        }
        lcd_set_foreground(sel ? aura_color(AURA_TOK_ACCENT_ON) : aura_color(AURA_TOK_TEXT_PRIMARY));
        lcd_putsxy(AURA_SPACING_LG, row_y + AURA_SPACING_SM,
                   (const unsigned char *)aura_str(s_root_entries[i].label_id));
    }
}

void aura_home_handle_button(aura_nav_t *nav, long button)
{
    int selected = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (selected < ROOT_COUNT - 1)
            aura_nav_set_selection(nav, selected + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (selected > 0)
            aura_nav_set_selection(nav, selected - 1);
        break;
    case BUTTON_SELECT:
        aura_nav_push(nav, s_root_entries[selected].target);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
