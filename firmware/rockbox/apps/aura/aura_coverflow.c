#include <string.h>
#include <stdlib.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "tick.h"

#include "aura_coverflow.h"
#include "aura_music.h"
#include "aura_albumart.h"
#include "aura_theme.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "aura_tokens.h"

/* Coverflow simplificado (D-025): en vez de perspectiva 3D real por
 * cuadro (demasiado costosa para un ARM926EJ-S a ~216MHz), todas las
 * caratulas visibles se decodifican una sola vez al mismo tamano fijo
 * y se cachean; la caratula central se dibuja a brillo completo con
 * un marco de acento, las laterales atenuadas hacia el color de fondo
 * (mismo blend que usa el reflejo, D-024/D-025). El "flujo" avanza de
 * a un album por vez, pero dos eventos de scroll seguidos en menos de
 * HZ/6 ticks avanzan de a dos -- aproximacion barata de "respuesta
 * proporcional a la velocidad del clickwheel" sin necesitar la senal
 * cruda de repeticion de boton (aura_main.c la descarta, ver D-022).
 */

#define CF_COVER_SIZE     56
#define CF_SPACING        70
#define CF_TOP_Y          28
#define CF_VISIBLE_RADIUS 2
#define CF_CACHE_SLOTS    8
#define CF_SIDE_FADE      130 /* de 255 */

typedef struct {
    int album_index; /* -1 = slot libre/no cargado */
    aura_albumart_t art;
    unsigned char cover_buf[CF_COVER_SIZE * CF_COVER_SIZE * sizeof(fb_data)];
    unsigned char reflection_buf[CF_COVER_SIZE * (CF_COVER_SIZE / 2) * sizeof(fb_data)];
} cf_slot_t;

static cf_slot_t s_slots[CF_CACHE_SLOTS];
static int s_current_index = 0;
static long s_last_scroll_tick = 0;

static aura_screen_id_t s_cache_screen = AURA_SCREEN_COUNT;
static int s_cache_generation = -1;
static aura_music_item_t s_albums[AURA_MUSIC_MAX_ITEMS];
static int s_album_count = 0;

static void ensure_albums(aura_screen_id_t screen)
{
    int gen = aura_music_filter_generation();
    int i;

    if (s_cache_screen == screen && s_cache_generation == gen)
        return;

    s_cache_screen = screen;
    s_cache_generation = gen;
    s_album_count = aura_music_browse(screen, s_albums, AURA_MUSIC_MAX_ITEMS);
    s_current_index = 0;

    for (i = 0; i < CF_CACHE_SLOTS; i++)
        s_slots[i].album_index = -1;
}

static cf_slot_t *get_slot_for(int album_index)
{
    int i, target, free_slot = -1, farthest = -1, farthest_dist = -1;

    for (i = 0; i < CF_CACHE_SLOTS; i++)
        if (s_slots[i].album_index == album_index)
            return &s_slots[i];

    for (i = 0; i < CF_CACHE_SLOTS; i++)
    {
        int dist;
        if (s_slots[i].album_index == -1)
        {
            free_slot = i;
            break;
        }
        dist = abs(s_slots[i].album_index - s_current_index);
        if (dist > farthest_dist)
        {
            farthest_dist = dist;
            farthest = i;
        }
    }
    target = (free_slot >= 0) ? free_slot : farthest;

    s_slots[target].album_index = album_index;
    s_slots[target].art.size = CF_COVER_SIZE;
    s_slots[target].art.cover_data = s_slots[target].cover_buf;
    s_slots[target].art.reflection_data = s_slots[target].reflection_buf;
    aura_albumart_load_for_album(s_albums[album_index].seek, &s_slots[target].art);

    return &s_slots[target];
}

/* Dibuja src (w x h, formato nativo del LCD) en (x,y), atenuando hacia
 * el color de fondo del tema segun `fade` (255 = sin atenuar). Mismo
 * blend que aura_albumart.c usa para el reflejo. */
static void blit_dimmed(const fb_data *src, int w, int h, int x, int y, int fade)
{
    static fb_data scratch[CF_COVER_SIZE * CF_COVER_SIZE];
    unsigned bg = aura_color(AURA_TOK_BACKGROUND);
    int bg_r = RGB_UNPACK_RED(bg);
    int bg_g = RGB_UNPACK_GREEN(bg);
    int bg_b = RGB_UNPACK_BLUE(bg);
    int i, n = w * h;

    for (i = 0; i < n; i++)
    {
        unsigned px = src[i];
        int r = RGB_UNPACK_RED(px);
        int g = RGB_UNPACK_GREEN(px);
        int b = RGB_UNPACK_BLUE(px);

        if (fade < 255)
        {
            r = bg_r + ((r - bg_r) * fade) / 255;
            g = bg_g + ((g - bg_g) * fade) / 255;
            b = bg_b + ((b - bg_b) * fade) / 255;
        }
        scratch[i] = LCD_RGBPACK(r, g, b);
    }
    lcd_bitmap(scratch, x, y, w, h);
}

static void draw_message(aura_str_id_t msg_id)
{
    int w, h;
    lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w, &h);
    lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2, (AURA_SCREEN_HEIGHT - h) / 2,
               (const unsigned char *)aura_str(msg_id));
}

void aura_coverflow_draw(aura_nav_t *nav, aura_screen_id_t screen)
{
    int offset;
    (void)nav;

    aura_theme_clear_screen();

    if (!aura_music_db_ready())
    {
        draw_message(AURA_STR_DB_NOT_READY);
        return;
    }

    ensure_albums(screen);

    if (s_album_count == 0)
    {
        draw_message(AURA_STR_EMPTY_LIST);
        return;
    }

    for (offset = -CF_VISIBLE_RADIUS; offset <= CF_VISIBLE_RADIUS; offset++)
    {
        int idx = s_current_index + offset;
        int x, y, fade;
        cf_slot_t *slot;

        if (idx < 0 || idx >= s_album_count)
            continue;

        slot = get_slot_for(idx);
        x = AURA_SCREEN_WIDTH / 2 + offset * CF_SPACING - CF_COVER_SIZE / 2;
        y = CF_TOP_Y;
        fade = (offset == 0) ? 255 : CF_SIDE_FADE;

        if (slot->art.valid)
        {
            blit_dimmed((const fb_data *)slot->art.cover_data,
                        CF_COVER_SIZE, CF_COVER_SIZE, x, y, fade);
            blit_dimmed((const fb_data *)slot->art.reflection_data,
                        CF_COVER_SIZE, CF_COVER_SIZE / 2, x, y + CF_COVER_SIZE, fade);
        }
        else
        {
            lcd_set_foreground(aura_color(offset == 0 ? AURA_TOK_TEXT_PRIMARY : AURA_TOK_BORDER));
            lcd_drawrect(x, y, CF_COVER_SIZE, CF_COVER_SIZE);
        }

        if (offset == 0)
        {
            lcd_set_foreground(aura_color(AURA_TOK_ACCENT));
            lcd_drawrect(x - 2, y - 2, CF_COVER_SIZE + 4, CF_COVER_SIZE + 4);
        }
    }

    {
        int w, h;
        const char *label = s_albums[s_current_index].label;

        lcd_setfont(aura_font(AURA_FONT_STYLE_BODY));
        lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)label, &w, &h);
        lcd_putsxy((AURA_SCREEN_WIDTH - w) / 2,
                   CF_TOP_Y + CF_COVER_SIZE + CF_COVER_SIZE / 2 + AURA_SPACING_LG,
                   (const unsigned char *)label);
    }
}

static void scroll_step(int dir)
{
    long now = current_tick;
    int step = 1;

    if (s_last_scroll_tick != 0 && (now - s_last_scroll_tick) < (HZ / 6))
        step = 2;
    s_last_scroll_tick = now;

    s_current_index += dir * step;
    if (s_current_index < 0)
        s_current_index = 0;
    if (s_current_index >= s_album_count)
        s_current_index = s_album_count > 0 ? s_album_count - 1 : 0;
}

void aura_coverflow_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    (void)screen;

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        scroll_step(1);
        break;
    case BUTTON_SCROLL_BACK:
        scroll_step(-1);
        break;
    case BUTTON_SELECT:
        if (s_album_count > 0)
        {
            aura_music_select_album(s_albums[s_current_index].seek);
            aura_nav_push(nav, AURA_SCREEN_MUSIC_SONGS_BY_ALBUM);
        }
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
