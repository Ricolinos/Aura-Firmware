#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "button.h"
#include "kernel.h"
#include "tagcache.h"
#include "string-extra.h"

#include "aura_search.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_lang.h"
#include "aura_music.h"
#include "aura_main.h"
#include "aura_wheel.h"
#include "aura_screens.h"

/* -- La tira -------------------------------------------------------------
 *
 * '_' es el espacio: la rueda nunca cae sobre algo invisible. Se
 * MUESTRA en mayusculas y se ESCRIBE en minusculas (encargo
 * 2026-08-13); la busqueda no distingue mayusculas de todos modos, asi
 * que lo unico que decide el caso es como se lee lo escrito. */
static const char KB_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
#define KB_COUNT ((int)(sizeof(KB_CHARS) - 1))

/* Geometria (320x240, bajo la StatusBar de 20px). */
#define KB_FULL_Y      26              /* texto completo, letra chica */
#define KB_PILL_Y      44
#define KB_PILL_H      34
#define KB_PILL_X      A26_SPACING_LG
#define KB_PILL_W      (A26_SCREEN_WIDTH - 2 * A26_SPACING_LG)
#define KB_FIELD_DX    6
#define KB_FIELD_W     84
#define KB_FIELD_H     24
#define KB_STRIP_DX    (KB_FIELD_DX + KB_FIELD_W + 10)
#define KB_STRIP_PAD   10
#define KB_LEAD        2               /* caracteres visibles detras del cursor */
#define KB_CHAR_GAP    3
#define KB_BLINK       (HZ / 2)
#define KB_RESULTS_Y   (KB_PILL_Y + KB_PILL_H + A26_SPACING_MD)
#define KB_RESULT_H    18

/* Lo escrito, la posicion en la tira y los resultados: TODO estatico,
 * asi salir con MENU y volver reanuda la busqueda intacta (encargo:
 * "si nos salimos sin querer... al regresar se conserva lo escrito",
 * sin importar cuantos niveles se hayan salido). */
#define KB_TEXT_MAX 64
static char s_text[KB_TEXT_MAX];
static int  s_sel = 0;
static int  s_result_sel = 0;

/* Resultados en vivo: se recalculan solo cuando el texto cambia. */
#define KB_MAX_RESULTS 64
static aura_music_item_t s_results[KB_MAX_RESULTS];
static int s_result_count = 0;
static char s_results_for[KB_TEXT_MAX];
static bool s_results_valid = false;

bool aura_search_needs_tick(void)
{
    return true; /* el cursor parpadea mientras la pantalla este activa */
}

/* -- Resultados ----------------------------------------------------------- */

static void refresh_results(void)
{
    struct tagcache_search tcs;
    char buf[AURA_MUSIC_ITEM_LEN];

    if (s_results_valid && !strcmp(s_results_for, s_text))
        return;

    strlcpy(s_results_for, s_text, sizeof(s_results_for));
    s_results_valid = true;
    s_result_count = 0;
    s_result_sel = 0;

    if (!s_text[0] || !tagcache_is_usable())
        return;
    if (!tagcache_search(&tcs, tag_title))
        return;

    while (s_result_count < KB_MAX_RESULTS
           && tagcache_get_next(&tcs, buf, sizeof(buf)))
    {
        char label[AURA_MUSIC_ITEM_LEN];

        /* Se busca sobre la etiqueta VISIBLE, no sobre el crudo de
         * tagcache: si no, "<Untagged>" entraba como resultado de
         * cualquier busqueda que contuviera "a", "un", "tag"... y
         * ademas se mostraba tal cual, que es jerga prohibida. */
        aura_music_visible_title(&tcs, buf, label, sizeof(label));

        /* Subcadena en cualquier posicion, sin distinguir mayusculas --
         * el mismo criterio del `~` de tagcache (clause_contains). */
        if (strcasestr(label, s_text))
        {
            strlcpy(s_results[s_result_count].label, label, AURA_MUSIC_ITEM_LEN);
            s_results[s_result_count].seek = tcs.idx_id;
            s_result_count++;
        }
    }
    tagcache_search_finish(&tcs);
}

/* -- Dibujo --------------------------------------------------------------- */

/* Texto completo arriba, en letra chica: hasta 22 caracteres y despues
 * puntos suspensivos A LA DERECHA (encargo 2026-08-13). */
static void draw_full_text(void)
{
    char shown[32];
    int w, h;

    if (!s_text[0])
        return;

    if ((int)strlen(s_text) > 22)
    {
        memcpy(shown, s_text, 22);
        strlcpy(shown + 22, "...", sizeof(shown) - 22);
    }
    else
        strlcpy(shown, s_text, sizeof(shown));

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_8));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)shown, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, KB_FULL_Y, (const unsigned char *)shown);
}

/* Campo de la caja: la COLA de lo escrito con puntos suspensivos por
 * delante ("...abc") cuando no cabe, mas el cursor parpadeante. */
static void draw_field(int field_x)
{
    char shown[KB_TEXT_MAX + 4];
    const char *tail = s_text;
    int avail = KB_FIELD_W - 2 * A26_SPACING_SM - 4; /* deja sitio al cursor */
    int w = 0, h, ty;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));

    if (s_text[0])
    {
        lcd_getstringsize((const unsigned char *)tail, &w, &h);
        while (w > avail && *tail)
        {
            tail++;
            snprintf(shown, sizeof(shown), "...%s", tail);
            lcd_getstringsize((const unsigned char *)shown, &w, &h);
        }
        if (tail != s_text)
            snprintf(shown, sizeof(shown), "...%s", tail);
        else
            strlcpy(shown, s_text, sizeof(shown));
    }
    else
        shown[0] = '\0';

    lcd_getstringsize((const unsigned char *)"Ay", &w, &h);
    ty = KB_PILL_Y + (KB_PILL_H - h) / 2;

    if (shown[0])
    {
        lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)shown, &w, &h);
        lcd_putsxy(field_x + A26_SPACING_SM, ty, (const unsigned char *)shown);
    }
    else
        w = 0;

    /* Cursor: barra fina justo tras el texto, medio segundo si y medio
     * no -- sin el, la rueda recorriendo la tira no da ninguna senal de
     * que hay un punto de escritura. */
    if (((current_tick / KB_BLINK) & 1) == 0)
    {
        lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
        lcd_fillrect(field_x + A26_SPACING_SM + w + 1, ty + 2, 1, h - 3);
    }
}

/* La tira: ventana deslizante que mantiene la letra activa en la
 * tercera posicion visible; se corta por la derecha cuando el siguiente
 * glifo ya no cabe. */
static void draw_strip(void)
{
    int start = s_sel - KB_LEAD;
    int limit = KB_PILL_X + KB_PILL_W - KB_STRIP_PAD;
    int x = KB_PILL_X + KB_STRIP_DX;
    unsigned accent = aura_accent();
    unsigned dim = a26_shell_blend(accent,
        (unsigned)AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT, 150);
    int i;

    if (start < 0)
        start = 0;

    for (i = start; i < KB_COUNT; i++)
    {
        char s[2] = { KB_CHARS[i], '\0' };
        bool active = (i == s_sel);
        int w, h;

        lcd_setfont(a26_font(active ? A26_FONT_STYLE_DS_BOLD_14
                                     : A26_FONT_STYLE_DS_REG_12));
        lcd_getstringsize((const unsigned char *)s, &w, &h);
        if (x + w > limit)
            break;
        lcd_set_foreground(active
            ? (unsigned)AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT
            : dim);
        lcd_putsxy(x, KB_PILL_Y + (KB_PILL_H - h) / 2, (const unsigned char *)s);
        x += w + KB_CHAR_GAP;
    }
}

static void draw_results_preview(void)
{
    int y = KB_RESULTS_Y;
    int i, w, h;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));

    if (!s_text[0])
        return;

    if (s_result_count == 0)
    {
        const char *msg = aura_str(AURA_STR_EMPTY_LIST);
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)msg, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y, (const unsigned char *)msg);
        return;
    }

    for (i = 0; i < s_result_count
                && y + KB_RESULT_H <= A26_SCREEN_HEIGHT; i++)
    {
        lcd_set_foreground(i == 0 ? a26_color(A26_TEXT_PRIMARY)
                                   : a26_color(A26_TEXT_SECONDARY));
        aura_widgets_puts_clipped(A26_SPACING_LG, y,
                                   A26_SCREEN_WIDTH - 2 * A26_SPACING_LG,
                                   s_results[i].label);
        y += KB_RESULT_H;
    }
}

void aura_search_draw(void)
{
    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_MUSIC_SEARCH));

    refresh_results();
    draw_full_text();

    /* La caja de busqueda es una pastilla de ACENTO: es el unico
     * elemento vivo de la pantalla y la tira se lee encima de ella. */
    a26_shell_fill_rounded_rect(KB_PILL_X, KB_PILL_Y, KB_PILL_W, KB_PILL_H,
                                 A26_LAYOUT_CORNER_RADIUS_PILL,
                                 aura_accent(), a26_color(A26_SHELL_BG));
    /* Campo blanco donde vive lo escrito. */
    a26_shell_fill_rounded_rect(KB_PILL_X + KB_FIELD_DX,
                                 KB_PILL_Y + (KB_PILL_H - KB_FIELD_H) / 2,
                                 KB_FIELD_W, KB_FIELD_H,
                                 A26_LAYOUT_CORNER_RADIUS_CARD,
                                 a26_color(A26_SHELL_BG), aura_accent());

    draw_field(KB_PILL_X + KB_FIELD_DX);
    draw_strip();
    draw_results_preview();
}

void aura_search_handle_button(aura_nav_t *nav, long button)
{
    size_t len = strlen(s_text);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_sel = aura_wheel_advance(s_sel, KB_COUNT, 1);
        break;
    case BUTTON_SCROLL_BACK:
        s_sel = aura_wheel_advance(s_sel, KB_COUNT, -1);
        break;

    case BUTTON_SELECT:
        if (len + 1 < sizeof(s_text))
        {
            char c = KB_CHARS[s_sel];
            /* La tira se ve en mayusculas y escribe minusculas. */
            if (c >= 'A' && c <= 'Z')
                c += 32;
            s_text[len] = (c == '_') ? ' ' : c;
            s_text[len + 1] = '\0';
            s_results_valid = false;
        }
        break;

    case BUTTON_RIGHT:
        /* Espacio (la tira tambien lo tiene, pero el original da el
         * atajo directo). */
        if (len + 1 < sizeof(s_text))
        {
            s_text[len] = ' ';
            s_text[len + 1] = '\0';
            s_results_valid = false;
        }
        break;

    case BUTTON_LEFT:
        /* Tap borra un caracter; MANTENER borra todo (encargo
         * 2026-08-13) -- se distingue con el mismo detector de repeat
         * que usa el reproductor. */
        if (aura_main_last_was_repeat())
            s_text[0] = '\0';
        else if (len > 0)
            s_text[len - 1] = '\0';
        s_results_valid = false;
        break;

    case BUTTON_PLAY:
        /* Confirma: abre los resultados a pantalla completa. */
        if (s_result_count > 0)
            aura_nav_push(nav, AURA_SCREEN_MUSIC_SEARCH_RESULTS);
        break;

    case BUTTON_MENU:
        /* Sale CONSERVANDO lo escrito -- volver a entrar reanuda. */
        aura_nav_pop(nav);
        break;

    default:
        break;
    }
}

/* -- Resultados a pantalla completa --------------------------------------- */

void aura_search_results_draw(void)
{
    static aura_list_item_t items[KB_MAX_RESULTS];
    int i;

    refresh_results();

    for (i = 0; i < s_result_count; i++)
    {
        items[i].label = s_results[i].label;
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
    }

    if (s_result_count == 0)
    {
        a26_shell_clear_screen();
        aura_widgets_draw_status_bar(aura_str(AURA_STR_MUSIC_SEARCH));
        return;
    }

    aura_widgets_draw_list(aura_str(AURA_STR_MUSIC_SEARCH), items,
                            s_result_count, s_result_sel);
}

void aura_search_results_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_result_sel = aura_wheel_advance(s_result_sel, s_result_count, 1);
        break;
    case BUTTON_SCROLL_BACK:
        s_result_sel = aura_wheel_advance(s_result_sel, s_result_count, -1);
        break;
    case BUTTON_SELECT:
        if (s_result_count > 0
            && aura_music_play_track(s_results[s_result_sel].seek))
            aura_nav_push(nav, AURA_SCREEN_NOWPLAYING);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
