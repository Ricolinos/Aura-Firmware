#include "lcd.h"
#include "font.h"
#include "rbpaths.h"

#include "apple2026_shell.h"
#include "aura_settings.h"
#include "apple2026_tokens.h"
#include "aura_color.h"

static int font_ids[A26_FONT_STYLE_COUNT];

void a26_shell_init(void)
{
    static const char *const paths[A26_FONT_STYLE_COUNT] = {
        [A26_FONT_STYLE_TITLE]   = FONT_DIR "/" A26_FONT_TITLE,
        [A26_FONT_STYLE_BODY]    = FONT_DIR "/" A26_FONT_BODY,
        [A26_FONT_STYLE_CAPTION] = FONT_DIR "/" A26_FONT_CAPTION,
        [A26_FONT_STYLE_HEADER]  = FONT_DIR "/" A26_FONT_HEADER,
        [A26_FONT_STYLE_MICRO]   = FONT_DIR "/" A26_FONT_MICRO,
        [A26_FONT_STYLE_DS_REG_8]   = FONT_DIR "/" A26_FONT_DS_REG_8,
        [A26_FONT_STYLE_DS_BOLD_8]  = FONT_DIR "/" A26_FONT_DS_BOLD_8,
        [A26_FONT_STYLE_DS_REG_10]  = FONT_DIR "/" A26_FONT_DS_REG_10,
        [A26_FONT_STYLE_DS_BOLD_10] = FONT_DIR "/" A26_FONT_DS_BOLD_10,
        [A26_FONT_STYLE_DS_REG_12]  = FONT_DIR "/" A26_FONT_DS_REG_12,
        [A26_FONT_STYLE_DS_BOLD_12] = FONT_DIR "/" A26_FONT_DS_BOLD_12,
        [A26_FONT_STYLE_DS_BOLD_14] = FONT_DIR "/" A26_FONT_DS_BOLD_14,
    };
    int i;

    for (i = 0; i < A26_FONT_STYLE_COUNT; i++)
    {
        int id = font_load(paths[i]);
        font_ids[i] = (id >= 0) ? id : FONT_SYSFIXED;
    }

    /* Aura pinta su propio fondo solido por tema; el backdrop del tema
     * de Rockbox por defecto (cabbiev2.bmp) se ve "a traves" de
     * lcd_clear_display() si queda activo, asi que se desactiva aqui. */
#if LCD_DEPTH > 1
    lcd_set_backdrop(NULL);
#endif
}

int a26_font(a26_font_style_t style)
{
    if (style < 0 || style >= A26_FONT_STYLE_COUNT)
        return FONT_SYSFIXED;
    return font_ids[style];
}

unsigned a26_color(a26_token_t token)
{
    int dark = (aura_settings.theme == AURA_THEME_DARK);

    switch (token)
    {
    case A26_SHELL_BG:
        return dark ? A26_COLOR_DARK_SHELL_BG : A26_COLOR_LIGHT_SHELL_BG;
    case A26_TEXT_PRIMARY:
        return dark ? A26_COLOR_DARK_TEXT_PRIMARY : A26_COLOR_LIGHT_TEXT_PRIMARY;
    case A26_TEXT_SECONDARY:
        return dark ? A26_COLOR_DARK_TEXT_SECONDARY : A26_COLOR_LIGHT_TEXT_SECONDARY;
    case A26_TEXT_TERTIARY:
        return dark ? A26_COLOR_DARK_TEXT_TERTIARY : A26_COLOR_LIGHT_TEXT_TERTIARY;
    case A26_ACCENT:
        /* El acento es CONFIGURABLE por el usuario (T0.3) -- resolver
         * el token al ajuste vivo, no al default del tema compilado.
         * Antes cada consumidor debia acordarse de llamar aura_accent()
         * en vez del token; los que no (texto seleccionado de listas de
         * contenido, tracklist de Cover Flow, toggles viejos) se
         * quedaban en el rosa default aunque el usuario eligiera otro
         * color -- inconsistencia real vista en el simulador con acento
         * azul (2026-08-12). Un solo punto de verdad aca la elimina
         * para todos. */
        return aura_accent();
    case A26_SHELL_RAIL:
        return dark ? A26_COLOR_DARK_SHELL_RAIL : A26_COLOR_LIGHT_SHELL_RAIL;
    case A26_PROGRESS_FILL:
        return dark ? A26_COLOR_DARK_PROGRESS_FILL : A26_COLOR_LIGHT_PROGRESS_FILL;
    case A26_PROGRESS_TRACK:
        return dark ? A26_COLOR_DARK_PROGRESS_TRACK : A26_COLOR_LIGHT_PROGRESS_TRACK;
    case A26_SELECTION_FILL:
        return dark ? A26_COLOR_DARK_SELECTION_FILL : A26_COLOR_LIGHT_SELECTION_FILL;
    }
    return dark ? A26_COLOR_DARK_TEXT_PRIMARY : A26_COLOR_LIGHT_TEXT_PRIMARY;
}

unsigned aura_accent(void)
{
    aura_rgb_t c = aura_color_from_rgb24(aura_settings.accent_rgb24);
    return LCD_RGBPACK(c.r, c.g, c.b);
}

static unsigned aura_accent_toward(unsigned toward_rgb24, int pct)
{
    aura_rgb_t accent = aura_color_from_rgb24(aura_settings.accent_rgb24);
    aura_rgb_t toward = aura_color_from_rgb24(toward_rgb24);
    aura_rgb_t out = aura_color_blend_pct(accent, toward, pct);
    return LCD_RGBPACK(out.r, out.g, out.b);
}

unsigned aura_accent_light(void)
{
    return aura_accent_toward(0xFFFFFFu, AURA_DS_COLOR_ACCENT_DERIVED_LIGHTEN_PCT);
}

unsigned aura_accent_dark(void)
{
    return aura_accent_toward(0x000000u, AURA_DS_COLOR_ACCENT_DERIVED_DARKEN_PCT);
}

bool aura_shadows_enabled(void)
{
    return aura_settings.left_panel_shadow;
}

void aura_shell_draw_left_panel_shadow(int x, int y, int height)
{
    int width = AURA_DS_METRICS_SHADOW_LEFT_PANEL_SHADOW_WIDTH;
    int max_pct = AURA_DS_OPACITY_SHADOW_PCT;
    unsigned bg = a26_color(A26_SHELL_BG);
    unsigned black = LCD_RGBPACK(0, 0, 0);
    int col;

    if (!aura_settings.left_panel_shadow)
        return;

    for (col = 0; col < width; col++)
    {
        /* Caida lineal: mas oscuro junto al panel (col=0), se desvanece
         * a 0% al llegar a `width`. */
        int pct = max_pct - (max_pct * col) / width;
        unsigned shade = a26_shell_blend(bg, black, pct * 256 / 100);

        lcd_set_foreground(shade);
        lcd_vline(x + col, y, y + height - 1);
    }
}

void a26_shell_clear_screen(void)
{
    lcd_set_background(a26_color(A26_SHELL_BG));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    /* lcd_clear_viewport, NO lcd_clear_display: clear_display ignora el
     * viewport activo (cambia a default_vp y borra la pantalla ENTERA,
     * firmware/drivers/lcd-color-common.c:100) -- durante una
     * transicion (aura_transitions.c dibuja dentro de un viewport
     * recortado/desplazado, o hacia un framebuffer offscreen) eso
     * destruia la pantalla anterior en cada cuadro, y el "push" se veia
     * como un deslizamiento sobre fondo vacio en vez de sobre la
     * pantalla que se va (D-068). Fuera de una transicion el viewport
     * activo ES default_vp (nadie mas instala viewports en Aura), asi
     * que el comportamiento es identico al de antes. */
    lcd_clear_viewport();

    /* DRMODE_FG (en vez del DRMODE_SOLID por defecto): el texto y los
     * bitmaps solo pintan sus pixeles de trazo y dejan el resto de la
     * celda intacto. Con SOLID, cada lcd_putsxy tambien rellena su caja
     * delimitadora con el color de fondo *registrado*, que no se
     * actualiza fila a fila -- sobre una fila resaltada con el color de
     * acento eso pintaba un rectangulo opaco que tapaba el texto. */
    lcd_set_drawmode(DRMODE_FG);
}

/* Raiz cuadrada entera en punto fijo 24.8: isqrt256(v) ~= sqrt(v)*256,
 * para `v` entero simple (distancias al cuadrado de radios <=16px --
 * cabe holgado en 32 bits: 512*512*65536 no se alcanza nunca aca).
 * Newton con semilla burda, converge en <6 iteraciones a estos rangos. */
unsigned a26_shell_isqrt256(unsigned v)
{
    unsigned x, prev;

    if (v == 0)
        return 0;
    v <<= 16; /* sqrt(v * 2^16) = sqrt(v) * 256 */
    x = v / 2 + 1;
    do {
        prev = x;
        x = (x + v / x) / 2;
    } while (x < prev);
    return prev;
}

/* Mascara de cuarto de circulo con ANTIALIAS por cobertura (correccion
 * "dientes de sierra" del dueno del diseno, 2026-08-12 -- el corte
 * binario por distancia si se nota como escalera, sobre todo en las
 * pastillas de acento/gris y las esquinas de caratulas): cada pixel de
 * la franja del borde se mezcla entre lo YA dibujado (leido del
 * framebuffer, asi funciona igual sobre relleno plano, texto o
 * degradado) y el fondo `bg`, con la fraccion de cobertura estimada
 * por distancia al arco (borde de 1px: cobertura lineal entre r-0.5 y
 * r+0.5). Fuera de esa franja el corte sigue siendo binario, igual de
 * barato que antes. */
static void stamp_corner(int corner_x, int corner_y, int step_x, int step_y,
                          int radius, unsigned bg)
{
    int dx, dy;
    int r256 = radius * 256;

    for (dy = 0; dy < radius; dy++)
    {
        for (dx = 0; dx < radius; dx++)
        {
            int rx = radius - 1 - dx;
            int ry = radius - 1 - dy;
            int dist256 = (int)a26_shell_isqrt256((unsigned)(rx * rx + ry * ry));
            int px = corner_x + dx * step_x;
            int py = corner_y + dy * step_y;
            fb_data *p;

            if (dist256 <= r256 - 128)
                continue; /* dentro del arco: se queda lo dibujado */

            p = FBADDR(px, py);
            if (dist256 >= r256 + 128)
                *p = bg; /* fuera del arco: fondo pleno */
            else
            {
                /* Franja del borde: cobertura de fondo 0..256 lineal. */
                int t = dist256 - (r256 - 128);
                *p = a26_shell_blend(*p, bg, t);
            }
        }
    }
}

void a26_shell_stamp_corners(void)
{
    int radius = A26_LAYOUT_CORNER_RADIUS_SCREEN;
    unsigned bg = a26_color(A26_SHELL_BG);
    int w = A26_SCREEN_WIDTH;
    int h = A26_SCREEN_HEIGHT;

    stamp_corner(0,     0,     1,  1,  radius, bg); /* superior izquierda */
    stamp_corner(w - 1, 0,    -1,  1,  radius, bg); /* superior derecha   */
    stamp_corner(0,     h - 1, 1, -1,  radius, bg); /* inferior izquierda */
    stamp_corner(w - 1, h - 1,-1, -1,  radius, bg); /* inferior derecha   */
}

/* Solo las esquinas IZQUIERDAS (Modo 4 del reproductor, correccion
 * 2026-08-12: la hoja de vidrio del panel derecho es CUADRADA -- las
 * esquinas de pantalla estampadas encima se leian como esquinas
 * redondeadas de la hoja). */
void a26_shell_stamp_corners_left_only(void)
{
    int radius = A26_LAYOUT_CORNER_RADIUS_SCREEN;
    unsigned bg = a26_color(A26_SHELL_BG);
    int h = A26_SCREEN_HEIGHT;

    stamp_corner(0, 0,     1,  1, radius, bg);
    stamp_corner(0, h - 1, 1, -1, radius, bg);
}

/* Capsula horizontal VERDADERA (encargo 2026-08-12, barra del
 * reproductor): extremos semicirculares de radio h/2 exacto -- con
 * radio entero h/2 en alturas impares (p.ej. 7px -> radio 3), la
 * esquina quedaba a 2/3 de cobertura y se leia cuadrada. Aca el
 * circulo vive en subunidades de 1/32 de pixel (centro a (h-1)/2,
 * radio h/2, ambos fraccionarios) y cada pixel del casquete se mezcla
 * por cobertura real entre `fill` y `bg` -- puntas completamente
 * redondeadas con antialias, sobre fondo plano conocido. */
void a26_shell_fill_capsule(int x, int y, int w, int h, unsigned fill, unsigned bg)
{
    int half = (h + 1) / 2;
    int r32 = h * 16;            /* h/2 en 1/32 px */
    int c32 = (h - 1) * 16;      /* centro del circulo, en 1/32 px */
    int px, py;

    if (w <= 0 || h <= 0)
        return;
    if (w < h)
        half = (w + 1) / 2;

    lcd_set_foreground(fill);
    if (w > 2 * half)
        lcd_fillrect(x + half, y, w - 2 * half, h);

    for (py = 0; py < h; py++)
    {
        fb_data *rowl = FBADDR(x, y + py);
        fb_data *rowr = FBADDR(x + w - half, y + py);
        int dy32 = py * 32 - c32;

        for (px = 0; px < half; px++)
        {
            int dx32 = px * 32 - c32;
            unsigned d32 = a26_shell_isqrt256((unsigned)(dx32 * dx32 + dy32 * dy32)) >> 8;
            int cov = (r32 + 16 - (int)d32) * 8; /* banda de 1px -> 0..256 */
            unsigned col;

            if (cov <= 0)
                col = bg;
            else if (cov >= 256)
                col = fill;
            else
                col = a26_shell_blend(bg, fill, cov);

            rowl[px] = col;
            rowr[half - 1 - px] = col;
        }
    }
}

/* Capsula compuesta SOBRE el framebuffer (para la pildora del
 * indicador de la barra, que cruza colores distintos debajo: relleno,
 * carril y fondo): cada pixel se mezcla contra lo ya dibujado con
 * cobertura*alpha -- sirve igual para la sombra paralela (negro a baja
 * alpha) que para la perilla blanca opaca. */
void a26_shell_fill_capsule_over(int x, int y, int w, int h,
                                  unsigned fill, int alpha_256)
{
    int half = (h + 1) / 2;
    int r32 = h * 16;
    int c32 = (h - 1) * 16;
    int px, py;

    if (w <= 0 || h <= 0)
        return;
    if (w < h)
        half = (w + 1) / 2;

    for (py = 0; py < h; py++)
    {
        fb_data *row = FBADDR(x, y + py);
        int dy32 = py * 32 - c32;

        for (px = 0; px < w; px++)
        {
            int cov;

            if (px >= half && px < w - half)
            {
                /* Tramo recto: cobertura vertical pura. */
                unsigned d32 = (dy32 < 0) ? (unsigned)-dy32 : (unsigned)dy32;
                cov = (r32 + 16 - (int)d32) * 8;
            }
            else
            {
                /* Casquetes: distancia radial al centro del semicirculo
                 * (el derecho, espejado). */
                int ex = (px < half) ? px : (w - 1 - px);
                int dx32 = ex * 32 - c32;
                unsigned d32 = a26_shell_isqrt256((unsigned)(dx32 * dx32 + dy32 * dy32)) >> 8;
                cov = (r32 + 16 - (int)d32) * 8;
            }

            if (cov > 256) cov = 256;
            if (cov <= 0)
                continue;
            row[px] = a26_shell_blend(row[px], fill, cov * alpha_256 / 256);
        }
    }
}

void a26_shell_fill_rounded_rect(int x, int y, int w, int h, int radius,
                                  unsigned fill, unsigned bg)
{
    if (radius > w / 2)
        radius = w / 2;
    if (radius > h / 2)
        radius = h / 2;

    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(fill);
    lcd_fillrect(x, y, w, h);
    lcd_set_drawmode(DRMODE_FG);

    if (radius <= 0)
        return;

    stamp_corner(x,         y,          1,  1, radius, bg); /* superior izquierda */
    stamp_corner(x + w - 1, y,         -1,  1, radius, bg); /* superior derecha   */
    stamp_corner(x,         y + h - 1,  1, -1, radius, bg); /* inferior izquierda */
    stamp_corner(x + w - 1, y + h - 1, -1, -1, radius, bg); /* inferior derecha   */
}

void a26_shell_round_bitmap_corners(int x, int y, int w, int h, int radius,
                                     unsigned bg)
{
    if (radius > w / 2)
        radius = w / 2;
    if (radius > h / 2)
        radius = h / 2;
    if (radius <= 0)
        return;

    stamp_corner(x,         y,          1,  1, radius, bg); /* superior izquierda */
    stamp_corner(x + w - 1, y,         -1,  1, radius, bg); /* superior derecha   */
    stamp_corner(x,         y + h - 1,  1, -1, radius, bg); /* inferior izquierda */
    stamp_corner(x + w - 1, y + h - 1, -1, -1, radius, bg); /* inferior derecha   */
}

void a26_shell_outline_rounded_rect(int x, int y, int w, int h, int radius,
                                     unsigned fill, unsigned border, unsigned bg)
{
    int inner_radius = (radius > 0) ? radius - 1 : 0;

    a26_shell_fill_rounded_rect(x, y, w, h, radius, border, bg);
    a26_shell_fill_rounded_rect(x + 1, y + 1, w - 2, h - 2, inner_radius,
                                 fill, border);
}

unsigned a26_shell_blend(unsigned from, unsigned to, int alpha_256)
{
    int r, g, b;

    if (alpha_256 <= 0)
        return from;
    if (alpha_256 >= 256)
        return to;

    r = RGB_UNPACK_RED(from)   + ((RGB_UNPACK_RED(to)   - RGB_UNPACK_RED(from))   * alpha_256 / 256);
    g = RGB_UNPACK_GREEN(from) + ((RGB_UNPACK_GREEN(to) - RGB_UNPACK_GREEN(from)) * alpha_256 / 256);
    b = RGB_UNPACK_BLUE(from)  + ((RGB_UNPACK_BLUE(to)  - RGB_UNPACK_BLUE(from))  * alpha_256 / 256);

    return LCD_RGBPACK(r, g, b);
}
