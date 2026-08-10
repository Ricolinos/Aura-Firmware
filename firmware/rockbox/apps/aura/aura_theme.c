#include "lcd.h"
#include "font.h"
#include "rbpaths.h"

#include "aura_theme.h"
#include "aura_settings.h"
#include "aura_tokens.h"

static int font_ids[AURA_FONT_STYLE_COUNT];

void aura_theme_init(void)
{
    static const char *const paths[AURA_FONT_STYLE_COUNT] = {
        [AURA_FONT_STYLE_TITLE]   = FONT_DIR "/" AURA_FONT_TITLE,
        [AURA_FONT_STYLE_BODY]    = FONT_DIR "/" AURA_FONT_BODY,
        [AURA_FONT_STYLE_CAPTION] = FONT_DIR "/" AURA_FONT_CAPTION,
        [AURA_FONT_STYLE_MICRO]   = FONT_DIR "/" AURA_FONT_MICRO,
    };
    int i;

    for (i = 0; i < AURA_FONT_STYLE_COUNT; i++)
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

int aura_font(aura_font_style_t style)
{
    if (style < 0 || style >= AURA_FONT_STYLE_COUNT)
        return FONT_SYSFIXED;
    return font_ids[style];
}

unsigned aura_color(aura_color_token_t token)
{
    int dark = (aura_settings.theme == AURA_THEME_DARK);

    switch (token)
    {
    case AURA_TOK_BACKGROUND:
        return dark ? AURA_COLOR_DARK_BACKGROUND : AURA_COLOR_LIGHT_BACKGROUND;
    case AURA_TOK_SURFACE:
        return dark ? AURA_COLOR_DARK_SURFACE : AURA_COLOR_LIGHT_SURFACE;
    case AURA_TOK_TEXT_PRIMARY:
        return dark ? AURA_COLOR_DARK_TEXT_PRIMARY : AURA_COLOR_LIGHT_TEXT_PRIMARY;
    case AURA_TOK_TEXT_SECONDARY:
        return dark ? AURA_COLOR_DARK_TEXT_SECONDARY : AURA_COLOR_LIGHT_TEXT_SECONDARY;
    case AURA_TOK_BORDER:
        return dark ? AURA_COLOR_DARK_BORDER : AURA_COLOR_LIGHT_BORDER;
    case AURA_TOK_ACCENT:
        return dark ? AURA_COLOR_DARK_ACCENT : AURA_COLOR_LIGHT_ACCENT;
    case AURA_TOK_ACCENT_ON:
        return dark ? AURA_COLOR_DARK_ACCENT_ON : AURA_COLOR_LIGHT_ACCENT_ON;
    }
    return dark ? AURA_COLOR_DARK_TEXT_PRIMARY : AURA_COLOR_LIGHT_TEXT_PRIMARY;
}

void aura_theme_clear_screen(void)
{
    lcd_set_background(aura_color(AURA_TOK_BACKGROUND));
    lcd_set_foreground(aura_color(AURA_TOK_TEXT_PRIMARY));
    lcd_clear_display();

    /* DRMODE_FG (en vez del DRMODE_SOLID por defecto): el texto y los
     * bitmaps solo pintan sus pixeles de trazo y dejan el resto de la
     * celda intacto. Con SOLID, cada lcd_putsxy tambien rellena su caja
     * delimitadora con el color de fondo *registrado*, que no se
     * actualiza fila a fila -- sobre una fila resaltada con el color de
     * acento eso pintaba un rectangulo opaco que tapaba el texto. */
    lcd_set_drawmode(DRMODE_FG);
}
