#include <stdlib.h>

#include "lcd.h"
#include "tick.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_patterns.h"

#include "aura_coverdrift.h"

#define IMAGE_SIZE   AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE
#define CYCLE_MS     AURA_DS_METRICS_COVER_DRIFT_MOVE_DURATION_MS
#define CROSSFADE_MS AURA_DS_METRICS_COVER_DRIFT_CROSSFADE_MS
#define ANGLE_COUNT  AURA_DS_METRICS_COVER_DRIFT_DIRECTIONS_DEG_COUNT

static const int ANGLES_DEG[ANGLE_COUNT] = AURA_DS_METRICS_COVER_DRIFT_DIRECTIONS_DEG_VALUES;

static int s_index = -1;
static int s_prev_index = -1;
static int s_angle_idx = -1;
static int s_distance = 0;
static long s_since = 0;
static int s_count_last = 0;

/* Distancia de arranque de cada movimiento: doc confirma que varia,
 * sin dar rango -- provisional D-098, entre 40% y 100% del margen real
 * disponible (mitad del lado mas corto del panel menos medio tile), asi
 * que el movimiento siempre es visible pero nunca sale del area
 * dibujable. */
static int pick_distance(int max_distance)
{
    int min_distance = max_distance * 2 / 5;
    if (max_distance <= min_distance)
        return max_distance;
    return min_distance + (rand() % (max_distance - min_distance + 1));
}

static int pick_angle_idx_excluding(int prev_idx)
{
    int idx = rand() % ANGLE_COUNT;
    if (idx == prev_idx)
        idx = (idx + 1) % ANGLE_COUNT;
    return idx;
}

static void advance_cycle(int count, int max_distance)
{
    s_prev_index = s_index;
    s_index = (s_index < 0) ? 0 : (s_index + 1) % count;
    s_angle_idx = pick_angle_idx_excluding(s_angle_idx);
    s_distance = pick_distance(max_distance);
    s_since = current_tick;
}

int aura_coverdrift_should_mount(int image_count)
{
    return image_count >= AURA_DS_METRICS_COVER_DRIFT_MIN_IMAGES_TO_ACTIVATE;
}

int aura_coverdrift_pending(void)
{
    return s_index >= 0;
}

int aura_coverdrift_animating(void)
{
    /* A diferencia de MarqueeText/ScrollIndicator, el movimiento es
     * continuo durante TODO el ciclo (nunca hay un tramo realmente
     * estatico salvo el instante exacto en que llega al centro) --
     * pending() y animating() coinciden mientras este montado. */
    return s_index >= 0;
}

/* Placeholder solido cuando `bmp` es NULL (sin imagen real cargada
 * todavia -- ningun consumidor real conecta Musica/Fotos aqui en esta
 * pasada, ver header). Paleta sintetica solo para distinguir imagenes
 * a simple vista durante la verificacion, NO es una decision de
 * diseno real -- cuando haya un consumidor real esto se reemplaza por
 * el bitmap decodificado. */
static unsigned placeholder_color(int index)
{
    switch (index % 4)
    {
        case 0: return aura_accent();
        case 1: return aura_accent_light();
        case 2: return aura_accent_dark();
        default: return a26_shell_blend(aura_accent(), a26_color(A26_SHELL_BG), 128);
    }
}

static void draw_image_at(const aura_coverdrift_image_t *images, int index,
                           int cx, int cy, unsigned alpha_256)
{
    const struct bitmap *bmp = images[index].bmp;

    if (bmp)
    {
        /* Corte directo, sin fundido real de pixeles -- ver header:
         * mezclar dos bitmaps arbitrarios necesita un compositor
         * offscreen que este sistema no tiene todavia. `alpha_256` se
         * ignora a proposito en este camino. */
        (void)alpha_256;
        lcd_bitmap((const fb_data *)bmp->data, cx - bmp->width / 2,
                   cy - bmp->height / 2, bmp->width, bmp->height);
        return;
    }

    {
        int x = cx - IMAGE_SIZE / 2;
        int y = cy - IMAGE_SIZE / 2;
        unsigned bg = a26_color(A26_SHELL_BG);
        unsigned color = a26_shell_blend(bg, placeholder_color(index), alpha_256);

        a26_shell_fill_rounded_rect(x, y, IMAGE_SIZE, IMAGE_SIZE,
                                     AURA_DS_METRICS_SELECTION_SUMMARY_TILE_CORNER_RADIUS,
                                     color, bg);
    }
}

void aura_coverdrift_draw(int x, int width,
                           const aura_coverdrift_image_t *images, int count)
{
    int cx = x + width / 2;
    int cy = A26_SCREEN_HEIGHT / 2;
    int max_distance = (width < A26_SCREEN_HEIGHT ? width : A26_SCREEN_HEIGHT) / 2 - IMAGE_SIZE / 2;
    long elapsed_ms;
    aura_pattern_point_t pos;

    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_vline(x - 1, 0, A26_SCREEN_HEIGHT - 1);
    aura_shell_draw_left_panel_shadow(x, 0, A26_SCREEN_HEIGHT);

    if (max_distance < 0)
        max_distance = 0;

    if (count != s_count_last || s_index < 0 || s_index >= count)
    {
        s_count_last = count;
        s_index = -1;
        advance_cycle(count, max_distance);
    }

    elapsed_ms = (current_tick - s_since) * 1000L / HZ;
    if (elapsed_ms >= CYCLE_MS)
    {
        advance_cycle(count, max_distance);
        elapsed_ms = 0;
    }

    pos = aura_pattern_drift_pos(ANGLES_DEG[s_angle_idx], s_distance, elapsed_ms, CYCLE_MS);

    if (elapsed_ms < CROSSFADE_MS && s_prev_index >= 0)
    {
        int alpha = (int)(elapsed_ms * 256 / CROSSFADE_MS);

        draw_image_at(images, s_prev_index, cx, cy, 256 - alpha);
        draw_image_at(images, s_index, cx + pos.dx, cy + pos.dy, alpha);
    }
    else
    {
        draw_image_at(images, s_index, cx + pos.dx, cy + pos.dy, 256);
    }
}
