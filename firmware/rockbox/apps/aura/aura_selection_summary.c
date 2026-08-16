#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "timefuncs.h"
#include "tick.h"
#include "string-extra.h"
#include "rbpaths.h"
#include "fs_defines.h"
#include "recorder/bmp.h"

#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_marquee.h"
#include "aura_flow.h"
#include "aura_category.h"

#include "aura_selection_summary.h"

#define TILE_SIZE   AURA_DS_METRICS_SELECTION_SUMMARY_TILE_SIZE
#define TILE_RADIUS AURA_DS_METRICS_SELECTION_SUMMARY_TILE_CORNER_RADIUS
/* Panel completo (D-267, fondo de imagen por acento) -- mismas
 * dimensiones que el resto del sistema ya asume para el panel derecho
 * (AURA_DS_METRICS_LEFT_PANEL_WIDTH=160, A26_SCREEN_HEIGHT=240), pero
 * expresadas aca como constantes propias porque el buffer estatico de
 * carga necesita un tamano de COMPILACION, no el `width` en tiempo de
 * ejecucion que ya recibe draw_summary() (siempre el mismo valor en la
 * practica, este componente no tiene otro llamador con otro ancho). */
#define BG_W (A26_SCREEN_WIDTH - AURA_DS_METRICS_LEFT_PANEL_WIDTH)
#define BG_H A26_SCREEN_HEIGHT
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
 * literales de cadena estables). El slot superior es una sola linea
 * (valor/titulo corto, D-263); el inferior admite hasta DOS lineas
 * (D-263, "el texto de abajo se podra mostrar en dos filas") -- cada
 * fila tiene su propio reloj de marquee independiente, igual que antes
 * lo tenian los dos SLOTS: una fila larga puede desbordar mientras la
 * otra no, sobre todo porque el texto de una fila casi siempre es mas
 * corto que el de la otra tras partir por palabra. */
static const char *s_top_shown = NULL;
static long s_top_since = 0;
static int s_top_overflowing = 0;

#define BOTTOM_LINES 2
static const char *s_bottom_shown[BOTTOM_LINES] = { NULL, NULL };
static long s_bottom_since[BOTTOM_LINES] = { 0, 0 };
static int s_bottom_overflowing[BOTTOM_LINES] = { 0, 0 };
/* Buffers de las dos lineas partidas -- estables mientras el texto
 * fuente (comparado por puntero) no cambie, ver split_two_lines(). */
static char s_bottom_line_buf[BOTTOM_LINES][64];
static const char *s_bottom_line_source = NULL;

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
    int i;

    if (s_top_overflowing)
        return 1;
    for (i = 0; i < BOTTOM_LINES; i++)
        if (s_bottom_overflowing[i])
            return 1;
    return 0;
}

int aura_selection_summary_animating(void)
{
    int i;

    if (s_top_overflowing && marquee_phase_animating(s_top_since))
        return 1;
    for (i = 0; i < BOTTOM_LINES; i++)
        if (s_bottom_overflowing[i] && marquee_phase_animating(s_bottom_since[i]))
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

/* Fondo completo del panel derecho, por preset de acento (D-267,
 * encargo del dueno de producto: "el background del selection summary
 * va a cambiar dependiendo del color de acento seleccionado"). Horneado
 * por design-system/generate.py (generate_panel_backgrounds()) a las
 * dimensiones EXACTAS del panel (BG_W x BG_H) -- se dibuja opaco con
 * lcd_bitmap() de un tiro, sin transparencia ni escalado en tiempo real.
 * Cache por nombre (comparacion por puntero, mismo patron que el resto
 * del archivo -- los nombres de preset son literales estables) --
 * recarga solo si cambia el preset pedido, no cada cuadro. Mientras
 * falten los otros 5 presets (el dueno solo compartio 'pink' para
 * probar), CUALQUIER nombre pedido cae a "pink" -- interino explicito,
 * documentado en tokens.json (aura_ds.metrics.right_panel_background). */
static fb_data s_bg_pixels[BG_W * BG_H];
static const char *s_bg_loaded_name = NULL;
static bool s_bg_load_ok = false;

static bool ensure_panel_background(const char *name)
{
    char path[MAX_PATH];
    struct bitmap bm;
    int ret;

    if (name == s_bg_loaded_name)
        return s_bg_load_ok;

    s_bg_loaded_name = name;
    snprintf(path, sizeof(path), "%s/aura/backgrounds/%s.bmp", ICON_DIR, name);
    bm.data = (char *)s_bg_pixels;
    ret = read_bmp_file(path, &bm, sizeof(s_bg_pixels), FORMAT_NATIVE, NULL);
    s_bg_load_ok = (ret > 0 && bm.width == BG_W && bm.height == BG_H);
    return s_bg_load_ok;
}

/* Sombra real bajo el tile flotante (D-267, mockup del dueno) --
 * compositing genuino contra lo que YA este dibujado (la imagen de
 * fondo, leida del framebuffer), no un color plano: sin primitiva de
 * blur en este LCD, se aproxima con un solo relleno semitransparente
 * (alpha fijo, sin caida gaussiana) recortado con la misma matematica de
 * distancia de esquina que ya usa stamp_corner()/a26_shell_isqrt256()
 * (apple2026_shell.c) para que el borde no se vea escalonado. Desplazada
 * `shadow_offset_y` px hacia abajo respecto al tile real -- el tile,
 * dibujado encima despues, tapa el centro y deja ver solo el borde
 * inferior de la sombra, el look clasico de icono flotante. */
static void draw_tile_shadow(int x, int y, int w, int h, int radius, int alpha_256)
{
    int px, py;
    int r256 = radius * 256;

    for (py = 0; py < h; py++)
    {
        for (px = 0; px < w; px++)
        {
            int a = alpha_256;
            int cx = -1, cy = -1;
            fb_data *p;

            if (px < radius && py < radius)             { cx = radius - 1; cy = radius - 1; }
            else if (px >= w - radius && py < radius)     { cx = w - radius; cy = radius - 1; }
            else if (px < radius && py >= h - radius)     { cx = radius - 1; cy = h - radius; }
            else if (px >= w - radius && py >= h - radius) { cx = w - radius; cy = h - radius; }

            if (cx >= 0)
            {
                int dx = px - cx, dy = py - cy;
                int dist256 = (int)a26_shell_isqrt256((unsigned)(dx * dx + dy * dy));
                if (dist256 > r256 + 128)
                    continue;
                if (dist256 > r256 - 128)
                    a = alpha_256 * (r256 + 128 - dist256) / 256;
            }
            if (a <= 0)
                continue;
            p = FBADDR(x + px, y + py);
            *p = a26_shell_blend(*p, 0 /* negro */, a);
        }
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

/* Parte `text` en hasta BOTTOM_LINES=2 lineas por palabra, cada una
 * buscando caber en `max_width` con la fuente YA configurada por el
 * llamador (D-263: "el texto de abajo se podra mostrar en dos filas si
 * es necesario"). Voraz: la linea 1 crece mientras el prefijo completo
 * siga cabiendo en un limite de palabra; el resto entero (quepa o no)
 * pasa a la linea 2 -- nunca se corte una palabra a la mitad. Si una
 * sola palabra ya excede `max_width` por si sola, se deja completa en
 * su linea -- esa linea usara MarqueeText (draw_text_slot ya lo hace
 * por su cuenta si sigue sin caber) en vez de forzar una tercera fila
 * que el documento no pide. Si el texto completo ya cabe en una sola
 * linea, `line2` queda vacio -- el llamador no reserva su altura en ese
 * caso. Resultado cacheado por puntero de `text` (mismo patron del
 * resto del archivo, D-091): recalcula solo cuando el texto fuente
 * cambia, no cada cuadro. */
static void split_two_lines(const char *text, int max_width)
{
    size_t len, split_at, i;
    int w, h;

    if (text == s_bottom_line_source)
        return;

    s_bottom_line_source = text;
    s_bottom_line_buf[0][0] = '\0';
    s_bottom_line_buf[1][0] = '\0';
    if (!text || !text[0])
        return;

    len = strlen(text);
    if (len >= sizeof(s_bottom_line_buf[0]) + sizeof(s_bottom_line_buf[1]))
        len = sizeof(s_bottom_line_buf[0]) + sizeof(s_bottom_line_buf[1]) - 2;
                                    /* recorte defensivo -- las etiquetas
                                     * reales de este sistema son cortas */

    lcd_getstringsize((const unsigned char *)text, &w, &h);
    if (w <= max_width)
    {
        strlcpy(s_bottom_line_buf[0], text, sizeof(s_bottom_line_buf[0]));
        return;
    }

    /* Ultimo espacio tal que el prefijo [0, i) todavia quepa -- se
     * detiene en el primero que ya no cabe (el ancho crece de forma
     * monotona con el prefijo, ningun prefijo posterior podria caber si
     * este ya no). */
    split_at = 0;
    for (i = 0; i < len; i++)
    {
        char tmp[sizeof(s_bottom_line_buf[0])];
        int tw, th;

        if (text[i] != ' ')
            continue;
        if (i >= sizeof(tmp))
            break;

        memcpy(tmp, text, i);
        tmp[i] = '\0';
        lcd_getstringsize((const unsigned char *)tmp, &tw, &th);
        if (tw > max_width)
            break;
        split_at = i;
    }

    if (split_at == 0)
    {
        strlcpy(s_bottom_line_buf[0], text, sizeof(s_bottom_line_buf[0]));
        return;
    }

    memcpy(s_bottom_line_buf[0], text, split_at);
    s_bottom_line_buf[0][split_at] = '\0';
    strlcpy(s_bottom_line_buf[1], text + split_at + 1, sizeof(s_bottom_line_buf[1]));
}

/* Layout completo (tile, degradado, sombra, texto) compartido por la
 * version estatica y la dinamica (B-04 en BLOCKED.md: "debe ser una
 * variante") -- lo UNICO que cambia entre las dos es como se pinta el
 * simbolo sobre el tile, aca aislado a las dos lineas finales del
 * bloque "Simbolo". `icon_name` NULL => usa `renderer` en su lugar.
 *
 * `category` (D-263): el degradado del tile YA NO consulta
 * aura_category_current() en vivo -- la recibe explicita del llamador,
 * que debe pasar la categoria CONGELADA de la identidad de panel que
 * este cuadro realmente esta dibujando (aura_screens.c:
 * panel_identity_category(), calculada sobre container_screen/
 * selected_target ya comprometidos por D-262, no sobre la seleccion en
 * vivo del LeftPanel). Bug real encontrado en la revision de D-262: con
 * aura_category_current() el color del tile saltaba al instante con
 * cada fila recorrida (se recalcula cada cuadro) mientras el icono
 * seguia congelado los 2s completos -- para cuando llegaba el fundido
 * real ya no quedaba color que fundir, solo el icono se movia. Con la
 * categoria tambien congelada, ambos (color e icono) cambian juntos,
 * en el mismo fundido. */
static void draw_summary(int x, int width, const char *icon_name,
                          aura_selection_summary_icon_renderer_t renderer,
                          aura_category_t category,
                          const char *top_text, const char *bottom_text,
                          aura_selection_summary_bottom_renderer_t bottom_renderer)
{
    int tile_x = x + (width - TILE_SIZE) / 2;
    int text_max_w = width - 2 * TEXT_PAD;
    int text_x = x + TEXT_PAD;
    int top_h = 0, bottom_h = 0, bottom_lines = 0;
    int total_h, tile_y, w, h;
    unsigned white = AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT;

    /* Fondo COMPLETO del panel (D-267, reemplaza el degradado diagonal
     * detras del tile de D-097) -- imagen por preset de acento, opaca,
     * un solo lcd_bitmap(). Si el archivo no esta (dispositivo real sin
     * los assets sincronizados todavia, o un preset que aun no existe),
     * cae a un relleno solido conocido en vez de dejar basura de memoria
     * en pantalla. */
    if (ensure_panel_background("pink"))
        lcd_bitmap(s_bg_pixels, x, 0, BG_W, BG_H);
    else
    {
        lcd_set_foreground(a26_color(A26_SHELL_BG));
        lcd_fillrect(x, 0, width, A26_SCREEN_HEIGHT);
    }

    /* Separador + sombra de LeftPanel (efectos/01-sombras.md, "SIEMPRE
     * renderiza una sombra que simula que LeftPanel esta por encima de
     * este componente" -- misma primitiva de T0.4, D-088). D-267: variante
     * de COMPOSITING real (D-258), no la de color plano -- el fondo ya no
     * es un color conocido, es la imagen que se acaba de dibujar. */
    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_vline(x - 1, 0, A26_SCREEN_HEIGHT - 1);
    aura_shell_draw_left_panel_shadow_over_content(x, 0, A26_SCREEN_HEIGHT);

    /* Altura de cada slot de texto medida con la fuente real antes de
     * calcular el centrado vertical del conjunto -- mismo criterio ya
     * usado en aura_statusbar.c (centrado por altura medida, no un
     * numero magico). D-267 (corrige D-263): slot superior SF Pro Bold
     * 13pt (DS_BOLD_13, 16pt se veia demasiado grande contra el fondo
     * nuevo), unica linea -- "valor" o titulo corto, nunca se envuelve.
     * Slot inferior SF Pro Medium 12pt (DS_MEDIUM_12, sube de Regular a
     * Medium y de 10 a 12pt), hasta DOS lineas por palabra
     * (split_two_lines()). */
    if (top_text && top_text[0])
    {
        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_13));
        lcd_getstringsize((const unsigned char *)top_text, &w, &h);
        top_h = h + TEXT_GAP;
    }
    if (bottom_text && bottom_text[0])
    {
        lcd_setfont(a26_font(A26_FONT_STYLE_DS_MEDIUM_12));
        split_two_lines(bottom_text, text_max_w);
        bottom_lines = s_bottom_line_buf[1][0] ? 2 : 1;
        lcd_getstringsize((const unsigned char *)s_bottom_line_buf[0], &w, &h);
        bottom_h = bottom_lines * h + TEXT_GAP;
    }
    else if (bottom_renderer)
    {
        /* Sin texto que medir -- reserva una franja fija (D-264, "Acerca
         * de": grafico de almacenamiento). A26_SPACING_LG es la misma
         * altura de barra que ya usa la pantalla completa de Acerca de
         * (draw_about_storage(), aura_screens.c) para su barra segmentada
         * -- consistente con el resto del sistema, no un numero suelto. */
        bottom_h = A26_SPACING_LG + TEXT_GAP;
    }

    total_h = top_h + TILE_SIZE + bottom_h;
    tile_y = (A26_SCREEN_HEIGHT - total_h) / 2 + top_h;

    /* Sombra + tile con esquinas redondeadas REALES (D-267): el fondo ya
     * no es un color plano conocido, asi que ni la sombra ni el recorte
     * de esquinas pueden pintar un color fijo (dejaria parches blancos
     * visibles contra la imagen -- exactamente el defecto que el dueno
     * senalo). Orden: (1) sombra offset, compositing real contra el
     * fondo ya dibujado; (2) SALVAR los pixeles del recuadro del tile
     * (fondo+sombra, lo que debe quedar visible en las esquinas
     * recortadas); (3) rellenar el tile COMPLETO (cuadrado, tapa la
     * sombra en su centro); (4) restaurar las 4 esquinas con los pixeles
     * salvados (a26_shell_round_bitmap_corners_over_content(), D-267) --
     * mismo antialias por distancia que la version de color plano, pero
     * compositing real. El tile en si sigue coloreado por CATEGORIA
     * (componentes/selection-summary.md, encargo del dueno 2026-08-14) --
     * eso NO cambio, solo el fondo detras dejo de ser el degradado y paso
     * a ser la imagen de acento. */
    {
        unsigned tile_a, tile_center, tile_b;
        static fb_data saved_tile[TILE_SIZE * TILE_SIZE];
        int sy;

        draw_tile_shadow(tile_x, tile_y + AURA_DS_METRICS_SELECTION_SUMMARY_SHADOW_OFFSET_Y,
                          TILE_SIZE, TILE_SIZE, TILE_RADIUS,
                          256 * AURA_DS_METRICS_SELECTION_SUMMARY_SHADOW_ALPHA_PCT / 100);

        for (sy = 0; sy < TILE_SIZE; sy++)
            memcpy(&saved_tile[sy * TILE_SIZE], FBADDR(tile_x, tile_y + sy),
                   TILE_SIZE * sizeof(fb_data));

        aura_category_gradient(category, &tile_a, &tile_center, &tile_b);
        draw_diagonal_gradient(tile_x, tile_y, TILE_SIZE, tile_a, tile_center, tile_b);

        a26_shell_round_bitmap_corners_over_content(tile_x, tile_y, TILE_SIZE, TILE_SIZE,
                                                      TILE_RADIUS, saved_tile, TILE_SIZE);
    }

    /* Simbolo: estatico (icono horneado, variante "-selector" blanco
     * constante, G5/T2.2) o dinamico (renderer real, B-04) -- mismo
     * hueco centrado sobre el tile para los dos casos. */
    if (icon_name)
        aura_widgets_draw_icon_variant_selector(icon_name, SYMBOL_SIZE,
            tile_x + (TILE_SIZE - SYMBOL_SIZE) / 2, tile_y + (TILE_SIZE - SYMBOL_SIZE) / 2);
    else if (renderer)
        renderer(tile_x + TILE_SIZE / 2, tile_y + TILE_SIZE / 2, SYMBOL_SIZE);

    /* Texto SIEMPRE blanco fijo (D-267) -- ya no A26_TEXT_PRIMARY (ese
     * token varia por tema claro/oscuro; el fondo nuevo es siempre
     * oscuro/saturado sin importar el tema, blanco es la unica opcion
     * legible en los dos). Mismo blanco constante que ya usa el icono
     * "-selector" y el reloj analogico sobre el tile de acento. */
    lcd_set_foreground(white);

    if (top_h)
    {
        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_13));
        draw_text_slot(text_x, tile_y - top_h, text_max_w, top_text,
                        &s_top_shown, &s_top_since, &s_top_overflowing);
    }
    else
        s_top_overflowing = 0;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_MEDIUM_12));
    if (bottom_lines >= 1)
    {
        int line_h = (bottom_h - TEXT_GAP) / bottom_lines;

        draw_text_slot(text_x, tile_y + TILE_SIZE + TEXT_GAP, text_max_w,
                        s_bottom_line_buf[0],
                        &s_bottom_shown[0], &s_bottom_since[0], &s_bottom_overflowing[0]);
        if (bottom_lines == 2)
            draw_text_slot(text_x, tile_y + TILE_SIZE + TEXT_GAP + line_h, text_max_w,
                            s_bottom_line_buf[1],
                            &s_bottom_shown[1], &s_bottom_since[1], &s_bottom_overflowing[1]);
        else
            s_bottom_overflowing[1] = 0;
    }
    else
    {
        s_bottom_overflowing[0] = 0;
        s_bottom_overflowing[1] = 0;
        if (bottom_renderer)
            bottom_renderer(text_x, tile_y + TILE_SIZE + TEXT_GAP, text_max_w,
                             bottom_h - TEXT_GAP);
    }
}

void aura_selection_summary_draw(int x, int width,
                                  const char *icon_name,
                                  aura_category_t category,
                                  const char *top_text,
                                  const char *bottom_text)
{
    draw_summary(x, width, icon_name, NULL, category, top_text, bottom_text, NULL);
}

void aura_selection_summary_draw_dynamic(int x, int width,
                                          aura_selection_summary_icon_renderer_t renderer,
                                          aura_category_t category,
                                          const char *top_text,
                                          const char *bottom_text,
                                          aura_selection_summary_bottom_renderer_t bottom_renderer)
{
    draw_summary(x, width, NULL, renderer, category, top_text, bottom_text, bottom_renderer);
}

/* Reloj analogico (componentes/selection-summary.md, "Variante
 * dinamica": "hoja de calendario... reloj analogico" -- este es el
 * segundo caso citado por el documento, el primer renderer real de
 * aura_selection_summary_draw_dynamic()). Circulo + manecillas de hora
 * y minuto en el mismo blanco constante que el resto del contenido
 * sobre el tile de acento (variante "-selector", G5). Trigonometria
 * entera reusada de aura_flow_fsin()/fcos() (T1.1/Fase 31, regla dura
 * 7 -- ninguna tabla de seno nueva): angulo 0 = manecilla apuntando
 * arriba (12 en punto), unidades IANGLE (1024 = vuelta completa),
 * creciendo en sentido horario. */
static void clock_hand(int cx, int cy, int iangle, int length, unsigned color)
{
    int dx = aura_flow_fsin(iangle) * length / AURA_FLOW_ONE;
    int dy = -aura_flow_fcos(iangle) * length / AURA_FLOW_ONE;

    lcd_set_foreground(color);
    lcd_drawline(cx, cy, cx + dx, cy + dy);
}

void aura_selection_summary_render_analog_clock(int x, int y, int size)
{
    struct tm *now = get_time();
    int hour12 = now->tm_hour % 12;
    int hour_iangle = hour12 * (AURA_FLOW_IANGLE_MAX / 12)
                     + now->tm_min * (AURA_FLOW_IANGLE_MAX / 12) / 60;
    int min_iangle = now->tm_min * (AURA_FLOW_IANGLE_MAX / 60);
    unsigned white = AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT;
    int r = size / 2;
    /* Marco circular como poligono de 16 lados -- no hay primitiva de
     * circulo en este LCD (ver lcd.h) y "recortar esquinas" contra un
     * fondo desconocido (aca el degradado del tile, no un color plano)
     * pintaria un cuadrado solido en cada esquina en vez de curvar de
     * verdad. Un poligono de segmentos SI funciona sobre cualquier
     * fondo: nunca toca los pixeles que no forman parte del trazo. */
    #define CLOCK_FACE_SIDES 16
    int prev_dx = 0, prev_dy = -r;
    int i;

    lcd_set_foreground(white);
    for (i = 1; i <= CLOCK_FACE_SIDES; i++)
    {
        int iangle = i * (AURA_FLOW_IANGLE_MAX / CLOCK_FACE_SIDES);
        int dx = aura_flow_fsin(iangle) * r / AURA_FLOW_ONE;
        int dy = -aura_flow_fcos(iangle) * r / AURA_FLOW_ONE;

        lcd_drawline(x + prev_dx, y + prev_dy, x + dx, y + dy);
        prev_dx = dx;
        prev_dy = dy;
    }
    #undef CLOCK_FACE_SIDES

    clock_hand(x, y, hour_iangle, r * 55 / 100, white);
    clock_hand(x, y, min_iangle, r * 85 / 100, white);
}
