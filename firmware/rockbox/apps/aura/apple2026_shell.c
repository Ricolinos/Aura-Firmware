#include "lcd.h"
#include "font.h"
#include "rbpaths.h"

#include "apple2026_shell.h"
#include "aura_settings.h"
#include "apple2026_tokens.h"

static int font_ids[A26_FONT_STYLE_COUNT];

void a26_shell_init(void)
{
    static const char *const paths[A26_FONT_STYLE_COUNT] = {
        [A26_FONT_STYLE_TITLE]   = FONT_DIR "/" A26_FONT_TITLE,
        [A26_FONT_STYLE_BODY]    = FONT_DIR "/" A26_FONT_BODY,
        [A26_FONT_STYLE_CAPTION] = FONT_DIR "/" A26_FONT_CAPTION,
        [A26_FONT_STYLE_MICRO]   = FONT_DIR "/" A26_FONT_MICRO,
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
        return dark ? A26_COLOR_DARK_ACCENT : A26_COLOR_LIGHT_ACCENT;
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

/* Mascara de cuarto de circulo por corte de distancia (sin trig, sin
 * antialias -- una pantalla LCD de 320x240 sin filtro no lo necesita a
 * radio 12): un pixel a (dx, dy) del vertice de la esquina se pinta con
 * el fondo del tema si cae fuera del cuarto de circulo de radio R. */
static void stamp_corner(int corner_x, int corner_y, int step_x, int step_y,
                          int radius, unsigned bg)
{
    int dx, dy;
    int r2 = radius * radius;

    lcd_set_drawmode(DRMODE_SOLID);
    lcd_set_foreground(bg);
    lcd_set_background(bg);

    for (dy = 0; dy < radius; dy++)
    {
        for (dx = 0; dx < radius; dx++)
        {
            int rx = radius - 1 - dx;
            int ry = radius - 1 - dy;
            if (rx * rx + ry * ry > r2)
                lcd_drawpixel(corner_x + dx * step_x, corner_y + dy * step_y);
        }
    }

    lcd_set_drawmode(DRMODE_FG);
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
