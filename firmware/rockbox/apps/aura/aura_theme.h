/* Aplicacion en runtime del sistema de diseno Aura: colores por tema y
 * fuentes bitmap cargadas desde disco (generadas por
 * design-system/generate.py, ver aura_tokens.h). */
#ifndef AURA_THEME_H
#define AURA_THEME_H

typedef enum {
    AURA_TOK_BACKGROUND = 0,
    AURA_TOK_SURFACE,
    AURA_TOK_TEXT_PRIMARY,
    AURA_TOK_TEXT_SECONDARY,
    AURA_TOK_BORDER,
    AURA_TOK_ACCENT,
    AURA_TOK_ACCENT_ON,
} aura_color_token_t;

typedef enum {
    AURA_FONT_STYLE_TITLE = 0,
    AURA_FONT_STYLE_BODY,
    AURA_FONT_STYLE_CAPTION,
    AURA_FONT_STYLE_MICRO,
    AURA_FONT_STYLE_COUNT,
} aura_font_style_t;

/* Carga las 4 fuentes Aura desde FONT_DIR. Si alguna falta (p. ej. no
 * se corrio design-system/generate.py + build_sim.sh), degrada a
 * FONT_SYSFIXED en vez de fallar: la UI sigue siendo usable. Llamar
 * una vez al arrancar, despues de aura_settings_load(). */
void aura_theme_init(void);

/* Color del token dado, resuelto contra aura_settings.theme, ya
 * empaquetado al formato nativo del LCD (listo para
 * lcd_set_foreground/background). */
unsigned aura_color(aura_color_token_t token);

/* Font id (para lcd_setfont) del estilo dado. */
int aura_font(aura_font_style_t style);

/* lcd_set_background(fondo del tema) + lcd_clear_display(). */
void aura_theme_clear_screen(void);

#endif /* AURA_THEME_H */
