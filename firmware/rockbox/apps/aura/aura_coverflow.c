#include <string.h>
#include <stdlib.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "tick.h"

#include "aura_coverflow.h"
#include "aura_music.h"
#include "aura_albumart.h"
#include "aura_art.h"
#include "aura_flow.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"

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
/* Alto del reflejo en tiempo de compilacion (Fase 29, doc SS5.4: 35%) --
 * mismo calculo que aura_art_reflection_height(), pero constante en
 * tiempo de compilacion (hace falta para dimensionar el buffer estatico
 * de cada slot). */
#define CF_REFLECTION_H   (CF_COVER_SIZE * AURA_ART_REFLECTION_HEIGHT_PCT / 100)

typedef struct {
    int album_index; /* -1 = slot libre/no cargado */
    aura_albumart_t art;
    unsigned char cover_buf[CF_COVER_SIZE * CF_COVER_SIZE * sizeof(fb_data)];
    unsigned char reflection_buf[CF_COVER_SIZE * CF_REFLECTION_H * sizeof(fb_data)];
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

/* Atenua un pixel hacia el color de fondo del tema segun `fade` (255 =
 * sin atenuar) -- mismo blend que aura_albumart.c usa para el reflejo,
 * factorizado para que tambien lo use el renderizador de perspectiva. */
static fb_data fade_pixel(unsigned px, int fade, int bg_r, int bg_g, int bg_b)
{
    int r = RGB_UNPACK_RED(px);
    int g = RGB_UNPACK_GREEN(px);
    int b = RGB_UNPACK_BLUE(px);

    if (fade < 255)
    {
        r = bg_r + ((r - bg_r) * fade) / 255;
        g = bg_g + ((g - bg_g) * fade) / 255;
        b = bg_b + ((b - bg_b) * fade) / 255;
    }
    return LCD_RGBPACK(r, g, b);
}

/* Dibuja src (w x h, formato nativo del LCD) en (x,y), atenuando hacia
 * el color de fondo del tema segun `fade` (255 = sin atenuar). */
static void blit_dimmed(const fb_data *src, int w, int h, int x, int y, int fade)
{
    static fb_data scratch[CF_COVER_SIZE * CF_COVER_SIZE];
    unsigned bg = a26_color(A26_SHELL_BG);
    int bg_r = RGB_UNPACK_RED(bg);
    int bg_g = RGB_UNPACK_GREEN(bg);
    int bg_b = RGB_UNPACK_BLUE(bg);
    int i, n = w * h;

    for (i = 0; i < n; i++)
        scratch[i] = fade_pixel(src[i], fade, bg_r, bg_g, bg_b);
    lcd_bitmap(scratch, x, y, w, h);
}

/* -- Perspectiva real (Fase 31.1/31.2, D-079/D-080) ----------------------
 *
 * Geometria de "reposo" de un coverflow clasico (pictureflow.c
 * reset_slides(), resuelta a numeros concretos para 320x240 -- ver
 * test_realistic_side_slide_layout() en test_flow.c, que documenta y
 * verifica la misma derivacion). Solo se usa para las tapas LATERALES
 * (offset != 0); la central sigue con blit_dimmed() de siempre -- angulo
 * 0 en la formula de perspectiva da un mapeo 1:1 sin distorsion, asi que
 * no hay diferencia visual, y la tapa mas importante en pantalla no
 * depende de la parte nueva y menos probada de este modulo. */
#define CF_ITILT           199    /* ~70 grados: 70*1024/360 */
#define CF_OFFSETX_R       144060 /* separacion centro-a-lateral, zoom=100 */
#define CF_SLIDE_SPACING_R (AURA_FLOW_ONE * (AURA_FLOW_DISPLAY_W / 4))

/* Primer intento de implementacion, deliberadamente simple (un
 * lcd_bitmap() de una columna por columna de pantalla, sin componer un
 * buffer completo antes de blitear) -- suficiente para verificar la
 * proyeccion en el simulador, que es todo lo que se pidio esta pasada
 * ("sin tocar hardware"). El costo real en el ARM926EJ-S del dispositivo
 * NO esta medido: si hace falta, la version para hardware compone en un
 * buffer y blitea una vez, como ya hace blit_dimmed() -- optimizacion
 * pendiente de la sesion guiada (D-079/D-080), no de esta pasada. */
static void draw_slide_perspective(const cf_slot_t *slot, int offset, int fade)
{
    aura_flow_slide_t slide;
    aura_flow_projection_t proj;
    int sign = (offset < 0) ? -1 : 1;
    int n = (offset < 0 ? -offset : offset) - 1; /* lateral 0-esimo, 1-esimo... */
    const fb_data *cover = (const fb_data *)slot->art.cover_data;
    const fb_data *refl = (const fb_data *)slot->art.reflection_data;
    int refl_h = aura_art_reflection_height(CF_COVER_SIZE);
    int total_h = CF_COVER_SIZE + refl_h;
    unsigned bg = a26_color(A26_SHELL_BG);
    int bg_r = RGB_UNPACK_RED(bg);
    int bg_g = RGB_UNPACK_GREEN(bg);
    int bg_b = RGB_UNPACK_BLUE(bg);
    static fb_data col_buf[CF_COVER_SIZE + CF_REFLECTION_H];

    /* Mismo signo que pictureflow.c reset_slides(): la lateral izquierda
     * (offset<0) angulo positivo y cx negativo, la derecha al reves --
     * la carga muestra su borde hacia el centro de la pantalla. */
    slide.angle = -sign * CF_ITILT;
    slide.distance = 0;
    slide.cx = sign * (CF_OFFSETX_R + n * CF_SLIDE_SPACING_R);

    aura_flow_begin_projection(&proj, &slide, CF_COVER_SIZE);

    while (proj.screen_x < AURA_FLOW_SCREEN_W)
    {
        int col = aura_flow_source_column(&proj);
        int dy = aura_flow_vertical_scale(&proj);
        int p = 0, dest_row, n_rows = 0;

        for (dest_row = 0; dest_row < total_h; dest_row++)
        {
            int source_row = p >> AURA_FLOW_SHIFT;
            unsigned px;

            if (source_row >= total_h)
                break;
            px = (source_row < CF_COVER_SIZE)
                ? cover[source_row * CF_COVER_SIZE + col]
                : refl[(source_row - CF_COVER_SIZE) * CF_COVER_SIZE + col];
            col_buf[dest_row] = fade_pixel(px, fade, bg_r, bg_g, bg_b);
            p += dy;
            n_rows++;
        }
        if (n_rows > 0)
            lcd_bitmap(col_buf, proj.screen_x, CF_TOP_Y, 1, n_rows);

        if (!aura_flow_advance_column(&proj))
            break;
    }
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

void aura_coverflow_draw(aura_nav_t *nav, aura_screen_id_t screen)
{
    int order[2 * CF_VISIBLE_RADIUS + 1];
    int oi, r;
    (void)nav;

    a26_shell_clear_screen();
    aura_statusbar_draw(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_MUSIC_ALBUMS), 0);

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

    /* De afuera hacia adentro (radio maximo -> 0), no de izquierda a
     * derecha: las laterales ahora se proyectan en coordenadas de
     * pantalla reales (perspectiva, no una grilla de columnas fijas) y
     * pueden solaparse con la central cerca del borde -- dibujando la
     * central al final garantiza que quede siempre encima, como en
     * cualquier coverflow real. */
    oi = 0;
    for (r = CF_VISIBLE_RADIUS; r >= 1; r--)
    {
        order[oi++] = -r;
        order[oi++] = r;
    }
    order[oi++] = 0;

    for (oi = 0; oi < 2 * CF_VISIBLE_RADIUS + 1; oi++)
    {
        int offset = order[oi];
        int idx = s_current_index + offset;
        int x, y, fade;
        cf_slot_t *slot;

        if (idx < 0 || idx >= s_album_count)
            continue;

        slot = get_slot_for(idx);
        y = CF_TOP_Y;
        fade = (offset == 0) ? 255 : CF_SIDE_FADE;

        if (!slot->art.valid)
        {
            x = A26_SCREEN_WIDTH / 2 + offset * CF_SPACING - CF_COVER_SIZE / 2;
            lcd_set_foreground(a26_color(offset == 0 ? A26_TEXT_PRIMARY : A26_SHELL_RAIL));
            lcd_drawrect(x, y, CF_COVER_SIZE, CF_COVER_SIZE);
            continue;
        }

        if (offset == 0)
        {
            x = A26_SCREEN_WIDTH / 2 - CF_COVER_SIZE / 2;
            blit_dimmed((const fb_data *)slot->art.cover_data,
                        CF_COVER_SIZE, CF_COVER_SIZE, x, y, fade);
            blit_dimmed((const fb_data *)slot->art.reflection_data,
                        CF_COVER_SIZE, CF_REFLECTION_H, x, y + CF_COVER_SIZE, fade);

            lcd_set_foreground(a26_color(A26_ACCENT));
            lcd_drawrect(x - 2, y - 2, CF_COVER_SIZE + 4, CF_COVER_SIZE + 4);
        }
        else
        {
            draw_slide_perspective(slot, offset, fade);
        }
    }

    {
        int w, h;
        const char *label = s_albums[s_current_index].label;

        lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
        lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)label, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2,
                   CF_TOP_Y + CF_COVER_SIZE + CF_COVER_SIZE / 2 + A26_SPACING_LG,
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
