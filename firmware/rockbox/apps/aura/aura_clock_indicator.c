#include "lcd.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_patterns.h"

#include "aura_clock_indicator.h"

void aura_clock_indicator_draw(int x, int width, int y, int enter_progress_256)
{
    char buf[16];
    int w, h, text_x, draw_y;
    int height = AURA_DS_METRICS_CLOCK_INDICATOR_HEIGHT;

    if (enter_progress_256 <= 0)
        return; /* totalmente fuera de pantalla por arriba, nada que dibujar */

    aura_format_clock(buf, sizeof(buf));

    /* Opacidad simulada 80% (status-bar.md, tabla de tipografia:
     * "--font-statusbar-time" -- ClockIndicator solo vive dentro de
     * StatusBar, doc header de este archivo) via a26_shell_blend()
     * hacia el fondo del shell, mismo mecanismo que sombras/scrollbars. */
    /* Encargo del dueno (2026-08-14): tipografia de la barra de estado
     * mas legible -- DS_REG_10 (10px) en vez de DS_REG_8 (8px), mismo
     * peso Regular, ya cargada (np_counter/aura_alarms.c la usan
     * tambien). Sin tocar AURA_DS_METRICS_CLOCK_INDICATOR_HEIGHT ni
     * ningun otro token de geometria -- solo la fuente. */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));
    lcd_set_foreground(a26_shell_blend(a26_color(A26_SHELL_BG), a26_color(A26_TEXT_PRIMARY),
                                        AURA_DS_OPACITY_STATUSBAR_TIME_PCT * 256 / 100));
    lcd_getstringsize((const unsigned char *)buf, &w, &h);
    if (w > AURA_DS_METRICS_CLOCK_INDICATOR_MAX_WIDTH)
        w = AURA_DS_METRICS_CLOCK_INDICATOR_MAX_WIDTH; /* HH:MM/HH:MM AM nunca deberia excederlo */

    text_x = x + (width - w) / 2;

    /* Drop-and-Lift (T1.1 aura_pattern_lerp): interpola la Y entre
     * "fuera de pantalla por arriba" (-height) y su posicion final
     * (y), con el progreso ya calculado por el llamador (T1.1 dice que
     * este patron es un solo paso lineal aplicado por cada componente
     * a su propio eje -- aca es Y). */
    draw_y = aura_pattern_lerp(y - height, y, enter_progress_256);

    lcd_putsxy(text_x, draw_y, (const unsigned char *)buf);
}
