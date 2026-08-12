#include "lcd.h"
#include "timefuncs.h"
#include "tick.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_marquee.h"

#include "aura_selection_summary.h"

#define TILE_SIZE   AURA_DS_METRICS_SELECTION_SUMMARY_TILE_SIZE
#define TILE_RADIUS AURA_DS_METRICS_SELECTION_SUMMARY_TILE_CORNER_RADIUS
/* A26_ICON_SIZE_SELECTION_SUMMARY_SYMBOL (pipeline de iconos, T2.8), no
 * AURA_DS_METRICS_SELECTION_SUMMARY_SYMBOL_SIZE (documentacion de
 * layout) -- mismo valor (60) pero es el define que garantiza que
 * exista un bitmap generado a ese tamano exacto, igual que el resto del
 * codigo nunca pide un tamano de icono que el pipeline no produjo. */
#define SYMBOL_SIZE A26_ICON_SIZE_SELECTION_SUMMARY_SYMBOL
#define TEXT_GAP    A26_SPACING_SM
#define TEXT_PAD    AURA_DS_METRICS_LEFT_PANEL_PADDING

/* Estado del marquee por slot (mismo patron de aura_statusbar.c/T2.1:
 * comparacion por puntero, los textos de este proyecto son siempre
 * literales de cadena estables). Dos relojes independientes -- ambos
 * slots pueden desbordar a la vez con textos de largo distinto. */
static const char *s_top_shown = NULL;
static const char *s_bottom_shown = NULL;
static long s_top_since = 0;
static long s_bottom_since = 0;
static int s_top_overflowing = 0;
static int s_bottom_overflowing = 0;

static int marquee_phase_animating(long since)
{
    long elapsed_ms, cycle_ms, t;

    elapsed_ms = (current_tick - since) * 1000L / HZ;
    cycle_ms = AURA_DS_METRICS_MARQUEE_STATIC_MS + AURA_DS_METRICS_MARQUEE_SCROLL_MS;
    if (cycle_ms <= 0)
        return 0;
    t = elapsed_ms % cycle_ms;

    return t >= AURA_DS_METRICS_MARQUEE_STATIC_MS;
}

int aura_selection_summary_pending(void)
{
    return s_top_overflowing || s_bottom_overflowing;
}

int aura_selection_summary_animating(void)
{
    if (s_top_overflowing && marquee_phase_animating(s_top_since))
        return 1;
    if (s_bottom_overflowing && marquee_phase_animating(s_bottom_since))
        return 1;
    return 0;
}

/* Degradado diagonal de 3 puntos (componentes/selection-summary.md):
 * `color_a` en la esquina superior izquierda, `color_center` (acento) a
 * la mitad del recorrido, `color_b` en la esquina inferior derecha --
 * direccion exacta de la diagonal es un pendiente del documento
 * (TODO(pendiente-doc), ver DECISIONS.md D-097; claro arriba-izquierda,
 * oscuro abajo-derecha elegido por ser la convencion mas comun de "luz
 * desde arriba"). Se dibuja como `size` lineas anti-diagonales (x+y
 * constante) en vez de por-pixel -- mismo color en toda la linea,
 * mucho mas barato que 8100 lcd_drawpixel en un LCD sin GPU. */
static void draw_diagonal_gradient(int x, int y, int size,
                                    unsigned color_a, unsigned color_center,
                                    unsigned color_b)
{
    int max_k = 2 * (size - 1);
    int k;

    for (k = 0; k <= max_k; k++)
    {
        int x0 = (k < size) ? 0 : k - size + 1;
        int y0 = (k < size) ? k : size - 1;
        int x1 = (k < size) ? k : size - 1;
        int y1 = (k < size) ? 0 : k - size + 1;
        int t256 = k * 256 / max_k;
        unsigned c = (t256 <= 128)
            ? a26_shell_blend(color_a, color_center, t256 * 2)
            : a26_shell_blend(color_center, color_b, (t256 - 128) * 2);

        lcd_set_foreground(c);
        lcd_drawline(x + x0, y + y0, x + x1, y + y1);
    }
}

/* Dibuja `text` centrado horizontalmente en [x, x+max_width), con
 * MarqueeText si desborda -- actualiza el reloj/estado del slot
 * correspondiente (`shown`/`since`/`overflowing`, pasados por
 * referencia porque son estaticos de archivo distintos por slot). */
static void draw_text_slot(int x, int y, int max_width, const char *text,
                            const char **shown, long *since, int *overflowing)
{
    int w, h;
    long elapsed_ms;

    if (!text || !text[0])
    {
        *overflowing = 0;
        return;
    }

    if (text != *shown)
    {
        *shown = text;
        *since = current_tick;
    }
    elapsed_ms = (current_tick - *since) * 1000L / HZ;

    lcd_getstringsize((const unsigned char *)text, &w, &h);
    if (w <= max_width)
    {
        /* Centrado real solo cuando cabe -- MarqueeText ya asume su
         * propio origen en `x` para el caso de desborde (mismo criterio
         * que aura_marquee_draw: el llamador decide x, el widget no
         * recentra un texto que se mueve). */
        *overflowing = aura_marquee_draw(x + (max_width - w) / 2, y, max_width, text, elapsed_ms);
    }
    else
    {
        *overflowing = aura_marquee_draw(x, y, max_width, text, elapsed_ms);
    }
}

void aura_selection_summary_draw(int x, int width,
                                  const char *icon_name,
                                  const char *top_text,
                                  const char *bottom_text)
{
    int tile_x = x + (width - TILE_SIZE) / 2;
    int text_max_w = width - 2 * TEXT_PAD;
    int text_x = x + TEXT_PAD;
    int top_h = 0, bottom_h = 0;
    int total_h, tile_y, w, h;

    /* Separador + sombra de LeftPanel (efectos/01-sombras.md, "SIEMPRE
     * renderiza una sombra que simula que LeftPanel esta por encima de
     * este componente" -- misma primitiva de T0.4, ya provisional y
     * documentada ahi, D-088). Antes de todo lo demas, igual que el
     * panel derecho viejo: nunca debe tapar el contenido real. */
    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_vline(x - 1, 0, A26_SCREEN_HEIGHT - 1);
    aura_shell_draw_left_panel_shadow(x, 0, A26_SCREEN_HEIGHT);

    /* Altura de cada slot de texto medida con la fuente real antes de
     * calcular el centrado vertical del conjunto -- mismo criterio ya
     * usado en aura_statusbar.c (centrado por altura medida, no un
     * numero magico). */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));
    if (top_text && top_text[0])
    {
        lcd_getstringsize((const unsigned char *)top_text, &w, &h);
        top_h = h + TEXT_GAP;
    }
    if (bottom_text && bottom_text[0])
    {
        lcd_getstringsize((const unsigned char *)bottom_text, &w, &h);
        bottom_h = h + TEXT_GAP;
    }

    total_h = top_h + TILE_SIZE + bottom_h;
    tile_y = (A26_SCREEN_HEIGHT - total_h) / 2 + top_h;

    /* Tile con degradado del acento (componentes/selection-summary.md):
     * acento al centro, "contaminado" por los dos derivados calculados
     * en runtime (aura_accent_light()/_dark(), T0.3/G9) -- nunca un
     * valor fijo, el acento es configurable por el usuario. */
    draw_diagonal_gradient(tile_x, tile_y, TILE_SIZE,
                            aura_accent_light(), aura_accent(), aura_accent_dark());
    a26_shell_round_bitmap_corners(tile_x, tile_y, TILE_SIZE, TILE_SIZE, TILE_RADIUS,
                                    a26_color(A26_SHELL_BG));

    /* Simbolo blanco constante sobre el tile -- misma variante "-selector"
     * (G5, T2.2) que ya usa el contenido sobre la pastilla de acento del
     * Selector; el tile de SelectionSummary es, en los hechos, la misma
     * situacion (contenido claro sobre relleno de acento). */
    aura_widgets_draw_icon_variant_selector(icon_name, SYMBOL_SIZE,
        tile_x + (TILE_SIZE - SYMBOL_SIZE) / 2, tile_y + (TILE_SIZE - SYMBOL_SIZE) / 2);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_10));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));

    if (top_h)
        draw_text_slot(text_x, tile_y - top_h, text_max_w, top_text,
                        &s_top_shown, &s_top_since, &s_top_overflowing);
    else
        s_top_overflowing = 0;

    if (bottom_h)
        draw_text_slot(text_x, tile_y + TILE_SIZE + TEXT_GAP, text_max_w, bottom_text,
                        &s_bottom_shown, &s_bottom_since, &s_bottom_overflowing);
    else
        s_bottom_overflowing = 0;
}
