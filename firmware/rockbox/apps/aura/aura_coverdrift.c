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
 * disponible, asi que el movimiento siempre es visible pero nunca sale
 * del area dibujable (D-254: "area dibujable" ahora es el margen de
 * sobrante de la imagen, no el panel -- ver max_distance en
 * aura_coverdrift_draw()). */
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

int aura_coverdrift_active_index(void)
{
    return s_index;
}

int aura_coverdrift_prev_index(void)
{
    return s_prev_index;
}

/* Placeholder solido cuando `bmp` es NULL -- la imagen activa/anterior
 * todavia no termino de decodificarse (el llamador decodifica bajo
 * demanda, ver aura_coverdrift.h). Paleta sintetica solo para
 * distinguir imagenes a simple vista, NO es una decision de diseno
 * real -- desaparece en cuanto el llamador entrega el bitmap real,
 * normalmente en 1 cuadro. Llena el panel completo (no un tile chico),
 * mismo criterio de recorte que draw_image_at() para bitmaps reales. */
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

/* D-254: dibuja UNA imagen (ya sea placeholder o bitmap real) llenando
 * el panel [px, px+pw) x [0, A26_SCREEN_HEIGHT), recortada a partir del
 * desplazamiento de deriva (dx, dy) desde el centro del panel. La
 * imagen (IMAGE_SIZE x IMAGE_SIZE, mayor que el panel en ambos ejes)
 * SIEMPRE cubre el panel completo por construccion -- max_distance en
 * aura_coverdrift_draw() esta acotado exactamente para garantizar esto
 * (ver comentario ahi), asi que nunca hace falta clampear ni revela
 * fondo. `margin_x`/`margin_y` son el sobrante de la imagen sobre el
 * panel en cada eje quieto (dx=dy=0); el origen de muestreo dentro de
 * la imagen es simplemente ese margen desplazado por (dx, dy). */
static void sample_offsets(int margin_x, int margin_y, int dx, int dy,
                            int *src_x, int *src_y)
{
    *src_x = margin_x - dx;
    *src_y = margin_y - dy;
}

static void draw_image_solid(const aura_coverdrift_image_t *images, int index,
                              int px, int pw, int margin_x, int margin_y,
                              int dx, int dy)
{
    const struct bitmap *bmp = images[index].bmp;

    if (bmp)
    {
        int src_x, src_y;
        sample_offsets(margin_x, margin_y, dx, dy, &src_x, &src_y);
        lcd_bitmap_part((const fb_data *)bmp->data, src_x, src_y, IMAGE_SIZE,
                         px, 0, pw, A26_SCREEN_HEIGHT);
        return;
    }

    a26_shell_fill_rounded_rect(px, 0, pw, A26_SCREEN_HEIGHT, 0,
                                 placeholder_color(index), a26_color(A26_SHELL_BG));
}

/* D-254: fundido real pixel a pixel entre la imagen SALIENTE (quieta,
 * en el centro exacto -- termino su ciclo ahi, D-089/G10) y la
 * ENTRANTE (moviendose, en (dx,dy)) -- ambas garantizado que cubren el
 * panel completo (mismo invariante que draw_image_solid()), asi que
 * cada pixel del panel tiene una muestra valida de las dos imagenes
 * sin necesidad de clampear. Costo real: A26_SCREEN_HEIGHT*pw pixeles
 * (38400 con el panel de 160x240) por cuadro, solo durante la ventana
 * de CROSSFADE_MS (~400ms, unos 8 cuadros a la cadencia de animacion
 * de 20fps que ya usa este modulo) -- comparable en orden de magnitud
 * a los compositores de mascara de icono ya existentes en
 * aura_widgets.c (draw_icon_mask_2()), que recorren pixel a pixel sin
 * problema de rendimiento observado a esa misma cadencia. */
static void draw_crossfade(const aura_coverdrift_image_t *images,
                            int px, int pw, int margin_x, int margin_y,
                            int dx, int dy, int alpha_256)
{
    const struct bitmap *bmp_out = images[s_prev_index].bmp;
    const struct bitmap *bmp_in = images[s_index].bmp;
    int out_src_x, out_src_y, in_src_x, in_src_y;
    int row, col;

    if (!bmp_out || !bmp_in)
    {
        /* Alguna de las dos todavia no decodifico -- no hay como
         * mezclar pixel real, se muestra solo la entrante (o el
         * placeholder si tampoco esa esta lista) sin fundido este
         * cuadro. Degradacion de un solo cuadro, imperceptible (mismo
         * criterio que el resto del sistema). */
        draw_image_solid(images, s_index, px, pw, margin_x, margin_y, dx, dy);
        return;
    }

    sample_offsets(margin_x, margin_y, 0, 0, &out_src_x, &out_src_y);
    sample_offsets(margin_x, margin_y, dx, dy, &in_src_x, &in_src_y);

    for (row = 0; row < A26_SCREEN_HEIGHT; row++)
    {
        const fb_data *out_row = (const fb_data *)bmp_out->data
            + (out_src_y + row) * IMAGE_SIZE + out_src_x;
        const fb_data *in_row = (const fb_data *)bmp_in->data
            + (in_src_y + row) * IMAGE_SIZE + in_src_x;
        fb_data *dst = FBADDR(px, row);

        for (col = 0; col < pw; col++)
            dst[col] = a26_shell_blend(out_row[col], in_row[col], alpha_256);
    }
}

void aura_coverdrift_draw(int x, int width,
                           const aura_coverdrift_image_t *images, int count)
{
    /* D-254: margen de sobrante de la imagen (IMAGE_SIZE x IMAGE_SIZE)
     * sobre el panel en cada eje -- puede ser negativo si el panel
     * fuera mayor que la imagen en ese eje (no pasa con las
     * dimensiones reales de este sistema, pero se acota a 0 por
     * seguridad: sin margen, sin deriva posible en ese eje). La
     * distancia maxima de deriva es el MENOR de los dos margenes: eso
     * garantiza que, en cualquiera de las 8 direcciones (incluidas las
     * diagonales, donde la distancia se reparte entre ambos ejes), la
     * imagen nunca deja de cubrir el panel completo -- ver
     * draw_image_solid()/draw_crossfade(). */
    int margin_x = (IMAGE_SIZE - width) / 2;
    int margin_y = (IMAGE_SIZE - A26_SCREEN_HEIGHT) / 2;
    int max_distance = margin_x < margin_y ? margin_x : margin_y;
    long elapsed_ms;
    aura_pattern_point_t pos;

    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_vline(x - 1, 0, A26_SCREEN_HEIGHT - 1);
    aura_shell_draw_left_panel_shadow(x, 0, A26_SCREEN_HEIGHT);

    if (margin_x < 0) margin_x = 0;
    if (margin_y < 0) margin_y = 0;
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

    /* D-255 (correccion del dueno del producto, 2026-08-15): de vuelta a
     * velocidad CONSTANTE -- D-254 habia agregado una desaceleracion
     * real (ease_out_effective_ms(), remapeando el tiempo antes de
     * pasarlo aca) que el dueno probo y pidio quitar. aura_pattern_drift_pos()
     * (aura_patterns.c) ya es puramente lineal por su cuenta -- se le
     * pasa elapsed_ms directo, sin ningun remapeo de tiempo. */
    pos = aura_pattern_drift_pos(ANGLES_DEG[s_angle_idx], s_distance, elapsed_ms, CYCLE_MS);

    if (elapsed_ms < CROSSFADE_MS && s_prev_index >= 0)
    {
        int alpha = (int)(elapsed_ms * 256 / CROSSFADE_MS);
        draw_crossfade(images, x, width, margin_x, margin_y, pos.dx, pos.dy, alpha);
    }
    else
    {
        draw_image_solid(images, s_index, x, width, margin_x, margin_y, pos.dx, pos.dy);
    }
}
