#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "font.h"
#include "file.h"
#include "rbpaths.h"
#include "recorder/bmp.h"
#include "tick.h"

#include "aura_widgets.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_status_bar_v2.h"
#include "aura_lang.h"
#include "aura_motion.h"
#include "aura_selection_summary.h"
#include "aura_category.h"
#include "aura_scroll_indicator.h"

/* Layout: barra de estado arriba (Fase 13, PLAN-UX.md), filas de lista
 * debajo. Pantalla dividida izquierda/derecha (Fase 15, L2): en
 * cualquier modo grafico salvo Ultra, la lista ocupa solo el panel
 * izquierdo (168px) y el panel derecho muestra un preview contextual
 * del item seleccionado, con un retardo de ~1s (L3) antes de
 * actualizarse. En Ultra, sigue siendo la lista de ancho completo de
 * siempre -- "sin panel derecho", L4. */
#define LIST_TOP      (A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_SM)
/* D-195 (encargo del dueno, 2026-08-13): filas de listas de contenido
 * (Musica/Video/Fotos/Alarmas/etc.) mas altas para que se lean menos
 * apretadas -- de ~10 filas visibles a 7 (visible_rows() se recalcula
 * solo, ver abajo). SPACING_MD en vez de SPACING_SM (21->29px) en vez
 * de subir A26_TYPE_BODY: ese tamano de fuente lo comparten CoverFlow/
 * Now Playing/Fotos/Video (aura_coverflow.c, aura_nowplaying.c, etc.),
 * que el encargo dice explicitamente NO tocar -- y agregar un tamano
 * de fuente nuevo excederia MAXUSERFONTS=12 (firmware/export/font.h),
 * ya al limite exacto con las 12 fuentes existentes. */
#define ROW_HEIGHT    (A26_TYPE_BODY + 2 * A26_SPACING_MD)
#define ROW_PAD_X     A26_LAYOUT_LIST_INSET
#define ICON_TEXT_GAP A26_SPACING_MD
#define PANEL_RETARDO_TICKS HZ

/* Pastilla de seleccion de lista (doc SS5.1): rectangulo que no toca los
 * bordes -- margen 8px desde el borde de la columna (mas angosto que el
 * inset de 16px del texto, asi la pastilla queda mas ancha que el
 * contenido pero sin llegar al borde) y 2px arriba/abajo. */
/* La pastilla de seleccion respeta el MISMO padding horizontal que el
 * contenido de las filas (ROW_PAD_X, 16px) -- correccion 2026-08-13:
 * antes usaba un margen propio de 8px, mas angosto que la sangria real
 * del texto, asi que la pastilla quedaba mas ANCHA que el contenido
 * que envuelve (visible en cualquier lista de contenido: Canciones,
 * Alarmas, Fotos...). El Selector de MenuList (aura_selector.c) ya
 * hacia esto bien -- pastilla y texto comparten un solo PADDING -- este
 * era el unico de los dos sistemas de lista que divergia. */
#define PILL_MARGIN_Y 2
#define PILL_RADIUS   A26_LAYOUT_CORNER_RADIUS_PILL

/* Switch inline para filas booleanas (SS5.1-bis, ver DECISIONS D-075):
 * pista capsula (radio = alto/2, geometria concentrica igual que el
 * resto de pastillas del sistema, SS5.4) + circulo que se desliza --
 * NUNCA una pantalla propia para un Si/No (doc de comportamiento SS1,
 * `[OPCION]`: "sigue la regla de profundidad por defecto, no tiene
 * mecanica propia"). Proporcion ~1.8:1 igual que el switch de iOS,
 * escalada a la fila de 21px del sistema. */
/* Mismas medidas y forma que el switch de MenuList (ver alli la nota
 * completa): pista en capsula y perilla en capsula dentro de ella. */
#define TOGGLE_W        28
#define TOGGLE_H        14
#define TOGGLE_MARGIN   2
#define TOGGLE_THUMB_W  15

static const char *theme_dir_name(void)
{
    return (aura_settings.theme == AURA_THEME_DARK) ? "dark" : "light";
}

/* Buffer de carga compartido, dimensionado para el icono mas grande
 * que se pide hoy (A26_ICON_SIZE_SELECTION_SUMMARY_SYMBOL, 60px) --
 * un buffer de sobra tambien sirve para los tamanos mas chicos de
 * listas/barra de estado. */
#define ICON_BUF_MAX A26_ICON_SIZE_SELECTION_SUMMARY_SYMBOL
static unsigned char icon_buf[ICON_BUF_MAX * ICON_BUF_MAX * 4 + 64];

/* Tinta runtime por variante -- el MISMO mapeo sufijo->token que
 * design-system/tokens.json (icon.variants) usa para hornear los bmp
 * pre-compuestos, resuelto aca contra la paleta viva en vez de en
 * generacion: "-selector" es el blanco constante del Selector (G5/D-086).
 * "-on" NO se resuelve aca desde el encargo de categorias (2026-08-14):
 * draw_icon_variant() la intercepta ANTES de llegar a esta funcion para
 * resolver un degradado de DOS tonos (aura_category_gradient(), no un
 * solo `unsigned` -- ninguna funcion que devuelve un color plano puede
 * representarlo), asi que este mapeo nunca ve ese sufijo. */
static unsigned variant_ink(const char *suffix)
{
    if (suffix[0] == '\0')
        return a26_color(A26_TEXT_PRIMARY);
    if (!strcmp(suffix, "-tertiary"))
        return a26_color(A26_TEXT_TERTIARY);
    if (!strcmp(suffix, "-rail"))
        return a26_color(A26_SHELL_RAIL);
    /* "-selector" */
    return AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT;
}

/* Composicion por cobertura contra el framebuffer REAL (corrige el
 * antialias de los iconos, reporte del dueno del diseno 2026-08-12):
 * los bmp pre-compuestos de generate.py hornean el borde antialiasado
 * contra UN fondo elegido en generacion -- correcto solo cuando el
 * fondo real coincide (fila en reposo sobre SHELL_BG). Sobre el
 * Selector de acento (configurable en runtime) o el tile con degradado
 * de SelectionSummary, ese borde pre-compuesto aparece como un halo
 * claro. La mascara de cobertura (icons/aura/masks/<name>-<size>.bmp,
 * R=G=B=cobertura del glifo) permite mezclar la tinta del token contra
 * lo que YA este dibujado debajo, pixel por pixel, con el mismo
 * a26_shell_blend() del resto del sistema -- antialias correcto sobre
 * cualquier fondo, incluidos degradados. Asume el viewport default a
 * pantalla completa (FBADDR absoluto), la misma convencion de
 * coordenadas de todo el dibujo de Aura.
 *
 * `extra_alpha_256` atenua el icono completo ademas de la cobertura
 * (256 = sin atenuar) -- reemplaza el camino especial que
 * aura_widgets_draw_icon_dimmed() tenia para el 50% de opacidad.
 *
 * `ink_a`/`ink_b` permiten un degradado DIAGONAL de dos tonos en vez de
 * una tinta plana (encargo del dueno 2026-08-14, jerarquia de color por
 * categoria: "todos los iconos... degradado entre un tono ligeramente
 * mas claro y uno ligeramente mas oscuro, no relleno plano"). Misma
 * convencion de diagonal que draw_diagonal_gradient() en
 * aura_selection_summary.c (claro arriba-izquierda, oscuro
 * abajo-derecha) pero por PIXEL en vez de por linea -- ya se recorre
 * pixel a pixel para la mascara de cobertura, asi que interpolar el tono
 * en el mismo bucle no agrega una pasada nueva. `ink_a == ink_b` (el
 * caso de siempre, tinta plana) colapsa a un blend con alpha_256=0 o 256
 * en cada pixel -- mismo resultado exacto que antes, sin rama especial. */
static int draw_icon_mask_2(const char *name, int size, int x, int y,
                             unsigned ink_a, unsigned ink_b, int extra_alpha_256)
{
    char path[MAX_PATH];
    struct bitmap bm;
    int ret, row, col, max_d;
    const fb_data *mask;

    snprintf(path, sizeof(path), "%s/aura/masks/%s-%d.bmp", ICON_DIR, name, size);

    bm.data = (char *)icon_buf;
    ret = read_bmp_file(path, &bm, sizeof(icon_buf), FORMAT_NATIVE, NULL);
    if (ret <= 0)
        return 0;

    max_d = (bm.width - 1) + (bm.height - 1);
    mask = (const fb_data *)bm.data;
    for (row = 0; row < bm.height; row++)
    {
        fb_data *dst = FBADDR(x, y + row);
        for (col = 0; col < bm.width; col++)
        {
            /* Canal verde crudo de RGB565 (6 bits): 0..63 -> 0..256
             * exacto en los extremos (63*256/63 = 256, tinta pura). */
            int cov = (mask[row * bm.width + col] >> 5) & 0x3F;
            int alpha, t256;
            unsigned ink;

            if (cov == 0)
                continue;
            alpha = cov * 256 / 63;
            if (extra_alpha_256 < 256)
                alpha = alpha * extra_alpha_256 / 256;
            t256 = (max_d > 0) ? ((row + col) * 256 / max_d) : 0;
            ink = (ink_a == ink_b) ? ink_a : a26_shell_blend(ink_a, ink_b, t256);
            dst[col] = a26_shell_blend(dst[col], ink, alpha);
        }
    }
    return bm.width;
}

static int draw_icon_mask(const char *name, int size, int x, int y,
                           unsigned ink, int extra_alpha_256)
{
    return draw_icon_mask_2(name, size, x, y, ink, ink, extra_alpha_256);
}

/* `suffix` elige la variante de color: "" es el color de texto normal,
 * "-on" el estado seleccionado/activo (degradado claro/oscuro del
 * ACENTO, siempre -- nunca el color de categoria), "-selector" el
 * blanco constante sobre el Selector. Camino primario: mascara de
 * cobertura + tinta runtime (ver draw_icon_mask/draw_icon_mask_2).
 * Fallback: el bmp pre-compuesto por tema/variante de siempre (D-010),
 * por robustez si el disco no trae mascaras -- ese camino de respaldo
 * sigue siendo tinta plana (el bmp horneado no tiene forma de llevar un
 * degradado en runtime), asi que SOLO en el caso raro de "sin mascaras
 * en disco" el icono "-on" pierde el degradado y vuelve al acento plano
 * de siempre (D-010) -- degradacion aceptable, nunca un crash.
 *
 * D-250 (correccion del dueno del producto, 2026-08-15): D-236 hacia
 * que "-on" resolviera el degradado de aura_category_current() -- eso
 * filtraba el color de categoria (gris de Ajustes, azul marino de
 * Video, amarillo/acento de Extras) a CUALQUIER icono "-on" en
 * cualquier pantalla (filas de menu/submenu, iconos de NowPlaying,
 * chevron del Selector -- ver todos los llamadores de
 * aura_widgets_draw_icon_selected()), no solo al tile de
 * SelectionSummary. El color de categoria debe vivir SOLO en el
 * contenedor del tile de SelectionSummary (aura_selection_summary.c ya
 * lo hace bien: llama aura_category_gradient() directo para el fondo
 * del tile, y dibuja el icono en blanco constante via el sufijo
 * "-selector", sin pasar por aqui) -- "-on" siempre usa el degradado
 * PLANO del acento, forzando AURA_CATEGORY_MUSIC (el caso de
 * fallback ya existente en aura_category_gradient(), identico al
 * comportamiento pre-D-236). */
static int draw_icon_variant(const char *name, int size, int x, int y,
                              const char *suffix)
{
    char path[MAX_PATH];
    struct bitmap bm;
    int ret;

    if (!name)
        return 0;

    if (!strcmp(suffix, "-on"))
    {
        unsigned ink_a, ink_center, ink_b;

        aura_category_gradient(AURA_CATEGORY_MUSIC, &ink_a, &ink_center, &ink_b);
        (void)ink_center;
        ret = draw_icon_mask_2(name, size, x, y, ink_a, ink_b, 256);
    }
    else
    {
        ret = draw_icon_mask(name, size, x, y, variant_ink(suffix), 256);
    }
    if (ret > 0)
        return ret;

    snprintf(path, sizeof(path), "%s/aura/%s/%s-%d%s.bmp",
              ICON_DIR, theme_dir_name(), name, size, suffix);

    bm.data = (char *)icon_buf;
    ret = read_bmp_file(path, &bm, sizeof(icon_buf), FORMAT_NATIVE, NULL);
    if (ret <= 0)
        return 0;

    lcd_bitmap_transparent((const fb_data *)bm.data, x, y, bm.width, bm.height);
    return bm.width;
}

int aura_widgets_draw_icon(const char *name, int size, int x, int y)
{
    return draw_icon_variant(name, size, x, y, "");
}

int aura_widgets_draw_icon_selected(const char *name, int size, int x, int y)
{
    return draw_icon_variant(name, size, x, y, "-on");
}

int aura_widgets_draw_icon_tertiary(const char *name, int size, int x, int y)
{
    return draw_icon_variant(name, size, x, y, "-tertiary");
}

int aura_widgets_draw_icon_rail(const char *name, int size, int x, int y)
{
    return draw_icon_variant(name, size, x, y, "-rail");
}

int aura_widgets_draw_icon_variant_selector(const char *name, int size, int x, int y)
{
    return draw_icon_variant(name, size, x, y, "-selector");
}

/* Icono a opacidad simulada arbitraria (PLAN.md T3.1(b),
 * componentes/now-playing.md: "su icono sigue apareciendo... pero al
 * 50% de opacidad" -- modo de Letras sin .lrc). Con la composicion por
 * mascara, la atenuacion es solo un factor extra sobre la cobertura
 * (extra_alpha_256 de draw_icon_mask) -- ya no hace falta el camino
 * especial que re-mezclaba un bmp horneado contra SHELL_BG plano, y de
 * paso ahora tambien es correcto sobre fondos no planos. */
int aura_widgets_draw_icon_dimmed(const char *name, int size, int x, int y, int alpha_256)
{
    if (!name)
        return 0;
    return draw_icon_mask(name, size, x, y, a26_color(A26_TEXT_PRIMARY), alpha_256);
}

/* Estado "deshabilitado" de la fila de modos de Ahora suena (encargo
 * 2026-08-12): mismo color que el inactivo (-tertiary), atenuado. */
int aura_widgets_draw_icon_tertiary_dimmed(const char *name, int size, int x, int y, int alpha_256)
{
    if (!name)
        return 0;
    return draw_icon_mask(name, size, x, y, a26_color(A26_TEXT_TERTIARY), alpha_256);
}

/* Tinta arbitraria decidida en runtime (Modo 4 del reproductor: los
 * iconos viven sobre un panel del color promedio de la caratula, cuya
 * tinta legible se computa por luminancia -- no existe como variante
 * horneada ni como token fijo). */
int aura_widgets_draw_icon_ink(const char *name, int size, int x, int y,
                                unsigned ink, int alpha_256)
{
    if (!name)
        return 0;
    return draw_icon_mask(name, size, x, y, ink, alpha_256);
}

/* Layout declarado por la pantalla actual (lo fija aura_screens_draw
 * desde su tabla). Por defecto SPLIT: es el layout de la mayoria de los
 * menus del firmware original. */
static aura_list_layout_t s_list_layout = AURA_LIST_SPLIT;

void aura_widgets_set_list_layout(aura_list_layout_t layout)
{
    s_list_layout = layout;
}

int aura_widgets_split_active(void)
{
    return s_list_layout == AURA_LIST_SPLIT
        && aura_settings.graphics_mode != AURA_GFX_NONE;
}

static int list_width(void)
{
    return aura_widgets_split_active() ? A26_LAYOUT_PANEL_LEFT_WIDTH : A26_SCREEN_WIDTH;
}

/* REGLA DURA (encargo 2026-08-13): la StatusBar va en (split) SI Y SOLO
 * SI el LeftPanel de 160px esta en pantalla. Antes cada pantalla elegia
 * el ancho de la barra por su cuenta y divergia del layout real -- p.ej.
 * los menus forzaban STATUSBAR_WIDTH_SPLIT aunque Graficos=Ninguno
 * hubiera apagado el panel, y los estados vacios dibujaban barra full
 * dentro de una pantalla split. Ahora barra y panel salen del MISMO
 * dato (aura_widgets_split_active(), que ya combina la tabla de layout
 * de la pantalla con el ajuste de Graficos). */
/* Dibuja `text` recortado a `max_w` con la fuente y el color activos
 * (mismo mecanismo de viewport que el resto del sistema). */
void aura_widgets_puts_clipped(int x, int y, int max_w, const char *text)
{
    struct viewport vp = *lcd_current_viewport;
    struct viewport *saved;

    if (max_w <= 0)
        return;
    vp.x = x;
    vp.y = y;
    vp.width = max_w;
    saved = lcd_set_viewport(&vp);
    lcd_putsxy(0, 0, (const unsigned char *)text);
    lcd_set_viewport(saved);
}

void aura_widgets_draw_status_bar(const char *title)
{
    aura_status_bar_v2_draw_auto(0, list_width(), title);
}

int aura_widgets_visible_rows(void)
{
    int rows = (A26_SCREEN_HEIGHT - LIST_TOP) / ROW_HEIGHT;
    return rows > 0 ? rows : 1;
}

/* Retardo de ~1s antes de que el panel derecho refleje una nueva
 * seleccion (L3, PLAN-UX.md): mientras el usuario sigue moviendose por
 * la lista, el panel sigue mostrando el icono anterior. */
static const char *s_panel_pending_icon;
static const char *s_panel_shown_icon;
static long s_panel_pending_since = 0;
static int s_panel_force_next = 0;

void aura_widgets_panel_force_next(void)
{
    s_panel_force_next = 1;
}

static void draw_right_panel_debounced(const char *icon_name)
{
    if (icon_name != s_panel_pending_icon)
    {
        s_panel_pending_icon = icon_name;
        s_panel_pending_since = current_tick;
    }

    /* Al volver atras el preview del menu padre se restaura al
     * instante, sin esperar el retardo -- es contenido que el usuario
     * ya habia visto, no una seleccion nueva (observado cuadro a cuadro
     * en el firmware original, D-068). El retardo de ~1s aplica solo a
     * selecciones nuevas mientras se navega. */
    if (s_panel_force_next)
    {
        s_panel_force_next = 0;
        s_panel_shown_icon = s_panel_pending_icon;
    }

    if (TIME_AFTER(current_tick, s_panel_pending_since + PANEL_RETARDO_TICKS))
        s_panel_shown_icon = s_panel_pending_icon;

    aura_widgets_draw_right_panel_icon(s_panel_shown_icon);
}

int aura_widgets_panel_pending(void)
{
    return s_panel_shown_icon != s_panel_pending_icon;
}

void aura_widgets_draw_right_panel_icon(const char *icon_name)
{
    /* Panel derecho real: SelectionSummary (componentes/selection-summary.md)
     * -- icono sobre tile con degradado del acento. Desde la migracion
     * de TODOS los menus a MenuList v2 (auditoria 2026-08-12), los
     * unicos consumidores que quedan de este camino son listas de
     * CONTENIDO (canciones/artistas/albumes/videos/fotos), cuyas filas
     * no tienen icono 1:1 -- con icon_name NULL se dibuja solo el
     * separador + sombra de LeftPanel (panel limpio), nunca un tile
     * vacio: SelectionSummary exige icono por contrato y el componente
     * de contenido rico (CoverDrift y equivalentes) es trabajo aparte
     * ya diferido por el design system. */
    if (!icon_name)
    {
        lcd_set_foreground(a26_color(A26_SHELL_RAIL));
        lcd_vline(A26_LAYOUT_PANEL_LEFT_WIDTH - 1, 0, A26_SCREEN_HEIGHT - 1);
        aura_shell_draw_left_panel_shadow(A26_LAYOUT_PANEL_LEFT_WIDTH, 0, A26_SCREEN_HEIGHT);
        return;
    }

    /* Ruta B (listas de CONTENIDO): nunca es (split) hoy (ver comentario
     * de arriba), asi que este camino no se ejercita en la practica --
     * D-263 le agrego el parametro `category` a la firma, aca se pasa
     * aura_category_current() sin cambio de comportamiento (este
     * llamador nunca tuvo el mecanismo de congelado/debounce de D-262,
     * es dibujo directo). */
    aura_selection_summary_draw(A26_LAYOUT_PANEL_LEFT_WIDTH,
                                 A26_SCREEN_WIDTH - A26_LAYOUT_PANEL_LEFT_WIDTH,
                                 icon_name, aura_category_current(), NULL, NULL);
}

/* ScrollIndicator de las listas de contenido a pantalla completa (D-275):
 * el mismo componente v2 (aura_scroll_indicator.c, componentes/
 * scroll-indicator.md) que MenuList/LeftPanel -- 4px, alto fijo 24px,
 * SHELL_RAIL, umbral de 10 items, fundido 150/1500/500 y deslizamiento
 * por item. Reemplaza al scrollbar propio del sistema viejo (D-073: 3px,
 * alto proporcional, fundido 150/800/330, `count > visible` como umbral
 * -- por eso una lista de 8-10 items mostraba barra, bug reportado por
 * el dueno). Solo queda aca el reloj de actividad de la lista (una sola
 * lista visible a la vez, mismo patron que el debounce del panel
 * derecho) -- se reinicia si cambia la lista mostrada o la seleccion. */
static const aura_list_item_t *s_scrollbar_items;
static const char *s_scrollbar_title;
static int s_scrollbar_selected = -1;
static long s_scrollbar_activity_since;

static long scrollbar_idle_ms(void)
{
    long elapsed = current_tick - s_scrollbar_activity_since;
    return (elapsed < 0 ? 0 : elapsed) * 1000L / HZ;
}

int aura_widgets_scrollbar_pending(void)
{
    /* Ventana ENTERA entrada+persistencia+salida (o deslizamiento en
     * curso), no solo "alpha > 0": justo al reiniciar la actividad el
     * alpha arranca en 0 y si pending() mirara solo el instante, el
     * bucle nunca pediria el SIGUIENTE cuadro (D-074). Cadencia gruesa. */
    return aura_scroll_indicator_pending(scrollbar_idle_ms());
}

int aura_widgets_scrollbar_animating(void)
{
    /* Cadencia fina SOLO en los tramos que cambian cuadro a cuadro
     * (fundidos y deslizamiento), nunca en la persistencia -- pedir
     * 20fps de mas ahi desbordo la cola de botones (D-074). */
    return aura_scroll_indicator_animating(scrollbar_idle_ms());
}

static void draw_scrollbar(int width, int count, int selected)
{
    /* Columna derecha de la pantalla, con el inset propio de LISTA-
     * COMPLETA (2px, scroll_indicator.inset_full) -- en LeftPanel el
     * carril es el padding de 4px del panel, aca no hay panel. */
    aura_scroll_indicator_draw(width - AURA_DS_METRICS_SCROLL_INDICATOR_INSET_FULL,
                                LIST_TOP, A26_SCREEN_HEIGHT - LIST_TOP,
                                selected, count, scrollbar_idle_ms(),
                                a26_color(A26_SHELL_BG), a26_color(A26_SHELL_RAIL));
}

/* Pastilla de seleccion animada (doc SS6/SS9.2, Fase 28): antes saltaba
 * de fila en fila; ahora se desplaza con el resorte corto con sobrepaso
 * de aura_motion.c. Estado global -- una sola lista visible a la vez,
 * mismo patron que el resto de aura_widgets. Se reinicia sin animar
 * (aparece ya en su lugar) al cambiar de pantalla, para no arrastrar la
 * posicion de una lista distinta. */
#define PILL_SPRING_TICKS (HZ * AURA_MOTION_SPRING_MS / 1000)

static const aura_list_item_t *s_pill_items;
static const char *s_pill_title;
static int s_pill_drawn_y = -1;    /* donde se dibujo el cuadro anterior */
static int s_pill_anim_from_y;
static int s_pill_anim_to_y;
static long s_pill_anim_since;

/* Identidad de la lista actual: `items` NO alcanza solo -- es un arreglo
 * local de `draw_nav_list()`/pantallas equivalentes, y el compilador le
 * da la MISMA direccion de stack en cada invocacion de esa funcion sin
 * importar que pantalla (Musica, Ajustes...) se este dibujando, asi que
 * dos listas distintas podian comparar como "la misma" y la pastilla
 * arrancaba un resorte real entre la fila de una lista y la fila de
 * otra -- bug real, visto en pantalla como una pastilla gris flotante
 * sin fila debajo tras entrar a Ajustes. `title` (un puntero estable
 * dentro de la tabla de aura_str(), distinto por pantalla) es la
 * identidad real; se comparan ambos por compatibilidad con cualquier
 * llamador que ya dependa de la deteccion por `items`. */
static int pill_animated_y(const aura_list_item_t *items, const char *title, int target_y)
{
    long elapsed;
    int eased;

    if (items != s_pill_items || title != s_pill_title)
    {
        s_pill_items = items;
        s_pill_title = title;
        s_pill_anim_from_y = target_y;
        s_pill_anim_to_y = target_y;
        s_pill_anim_since = current_tick - PILL_SPRING_TICKS; /* ya "asentada" */
    }
    else if (target_y != s_pill_anim_to_y)
    {
        /* Redirige desde donde la pastilla esta REALMENTE dibujada, no
         * desde el destino anterior -- si el usuario sigue girando la
         * rueda antes de que termine el resorte, continua desde el
         * punto visual actual en vez de saltar. */
        s_pill_anim_from_y = s_pill_drawn_y;
        s_pill_anim_to_y = target_y;
        s_pill_anim_since = current_tick;
    }

    if (s_pill_anim_from_y == s_pill_anim_to_y)
    {
        s_pill_drawn_y = target_y;
        return target_y;
    }

    elapsed = current_tick - s_pill_anim_since;
    if (elapsed >= PILL_SPRING_TICKS)
    {
        s_pill_drawn_y = s_pill_anim_to_y;
        return s_pill_drawn_y;
    }

    eased = aura_motion_spring(elapsed, PILL_SPRING_TICKS);
    s_pill_drawn_y = s_pill_anim_from_y
        + (s_pill_anim_to_y - s_pill_anim_from_y) * eased / 256;
    return s_pill_drawn_y;
}

int aura_widgets_pill_animating(void)
{
    long elapsed = current_tick - s_pill_anim_since;
    return s_pill_anim_from_y != s_pill_anim_to_y
        && elapsed >= 0 && elapsed < PILL_SPRING_TICKS;
}

/* IndexRail -- riel A-Z de las listas de ELEMENTOS a pantalla completa
 * (componentes/index-rail.md; D-155 lo construyo, D-276 lo redefine).
 * Columna estrecha pegada al borde derecho con las 27 posiciones FIJAS
 * `#` + A-Z (D-276, encargo del dueno: "todas las letras siempre
 * visibles"): la inicial del elemento seleccionado va en acento, las
 * iniciales presentes en la lista en tinta terciaria, y las que NO
 * tienen contenido en SHELL_RAIL (deshabilitadas -- y no seleccionables:
 * hoy el riel es un indicador pasivo, sin salto por letra). Antes solo
 * se dibujaban las iniciales presentes, asi que la barra cambiaba de
 * forma con cada lista y las letras se estiraban para llenar el alto.
 * Solo se dibuja si la lista es lo bastante larga para que indexar
 * signifique algo (index_rail.min_items). Fuente: la de 7px de SF Pro
 * (A26_FONT_STYLE_MICRO, glifo de 8px de alto) -- 27 x 8 = 216px, el
 * alto util exacto bajo la StatusBar; con DS_REG_8 (9px) las ultimas
 * tres letras no cabian. */
#define RAIL_SLOTS 27

static int rail_slot(const char *label)
{
    unsigned char c;

    while (*label == ' ')
        label++;
    c = (unsigned char)*label;
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c >= 'A' && c <= 'Z')
        return 1 + (c - 'A');
    return 0; /* digitos, acentuadas y simbolos: al grupo '#' (slot 0) */
}

static void draw_index_rail(const aura_list_item_t *items, int count, int selected)
{
    unsigned long present = 0; /* mascara de 27 bits: que iniciales tiene la lista */
    int i, y, step, h, w, sel_slot;
    int rail_w = AURA_DS_METRICS_INDEX_RAIL_WIDTH;

    if (count < AURA_DS_METRICS_INDEX_RAIL_MIN_ITEMS)
        return;

    for (i = 0; i < count; i++)
        present |= 1UL << rail_slot(items[i].label);

    sel_slot = (selected >= 0 && selected < count) ? rail_slot(items[selected].label) : -1;

    lcd_setfont(a26_font(A26_FONT_STYLE_MICRO));
    lcd_getstringsize((const unsigned char *)"A", &w, &h);
    step = (A26_SCREEN_HEIGHT - LIST_TOP) / RAIL_SLOTS;
    if (step < h)
        step = h; /* si no caben todas, se recortan por abajo antes que encimarse */

    for (i = 0; i < RAIL_SLOTS; i++)
    {
        char buf[2] = { (char)(i == 0 ? '#' : 'A' + (i - 1)), '\0' };
        unsigned color;

        y = LIST_TOP + i * step;
        if (y + h > A26_SCREEN_HEIGHT)
            break;
        if (i == sel_slot)
            color = a26_color(A26_ACCENT);
        else if (present & (1UL << i))
            color = a26_color(A26_TEXT_TERTIARY);
        else
            color = a26_color(A26_SHELL_RAIL); /* deshabilitada */
        lcd_getstringsize((const unsigned char *)buf, &w, &h);
        lcd_set_foreground(color);
        lcd_putsxy(A26_SCREEN_WIDTH - rail_w + (rail_w - w) / 2, y,
                   (const unsigned char *)buf);
    }
}

void aura_widgets_draw_list(const char *title, const aura_list_item_t *items,
                             int count, int selected)
{
    int split = aura_widgets_split_active();
    int width = list_width();
    int visible = aura_widgets_visible_rows();
    int first = 0;
    int i;

    if (count > visible)
    {
        first = selected - visible / 2;
        if (first < 0)
            first = 0;
        if (first > count - visible)
            first = count - visible;
    }

    if (items != s_scrollbar_items || title != s_scrollbar_title || selected != s_scrollbar_selected)
    {
        s_scrollbar_items = items;
        s_scrollbar_title = title;
        s_scrollbar_selected = selected;
        s_scrollbar_activity_since = current_tick;
    }

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(title);

    /* Encargo del dueno (2026-08-14), "misma indicacion de antes"
     * (D-195): tipografia de las filas de lista mas grande y legible.
     * A26_TYPE_BODY (13px) es el mas grande disponible en peso Regular
     * -- MAXUSERFONTS=12 sigue al limite exacto (ver tokens.json,
     * comment_ds), asi que no hay ningun "Regular 14/16px" para pedir
     * prestado. DS_BOLD_14 (14px, ya cargada para lyrics_active del
     * reproductor -- reutilizada aca, el reproductor no se toca) es la
     * unica fuente ya cargada mas grande que 13px disponible; el peso
     * Bold ademas ayuda a la legibilidad en una pantalla de 163ppi sin
     * subantialiasing tanto como el pixel extra de alto. */
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_14));

    /* Pastilla de fila activa (doc SS5.1), dibujada ANTES que el
     * contenido de las filas -- no dentro del loop de la fila
     * seleccionada. El resorte (SS9.2) puede tenerla en transito sobre
     * la posicion de OTRA fila mientras se desliza; si se dibujara
     * dentro de esa misma iteracion tapiaria el texto ya pintado de la
     * fila que esta cruzando (bug real, visto en pantalla, no en
     * teoria). Dibujando la pastilla primero y el texto de todas las
     * filas despues, ninguna fila pierde su contenido aunque la
     * pastilla este pasando por encima. */
    if (selected >= first && selected < first + visible)
    {
        int sel_row_y = LIST_TOP + (selected - first) * ROW_HEIGHT;
        int pill_y = pill_animated_y(items, title, sel_row_y + PILL_MARGIN_Y);
        a26_shell_fill_rounded_rect(ROW_PAD_X, pill_y,
                                     width - 2 * ROW_PAD_X,
                                     ROW_HEIGHT - 2 * PILL_MARGIN_Y,
                                     PILL_RADIUS,
                                     a26_color(A26_SELECTION_FILL),
                                     a26_color(A26_SHELL_BG));
    }

    for (i = first; i < count && i < first + visible; i++)
    {
        int row_y = LIST_TOP + (i - first) * ROW_HEIGHT;
        int is_selected = (i == selected);
        int text_x = ROW_PAD_X;

        /* D-195: A26_ICON_SIZE_CONTENT_LIST (28px) en vez de
         * A26_ICON_SIZE_MENU (20px, sin tocar) -- ese otro token
         * tambien lo usa la fila de iconos de modo de Ahora suena
         * (aura_nowplaying.c), que el encargo dice explicitamente NO
         * agrandar; compartirlo habria crecido esos iconos tambien. */
        if (items[i].icon_name)
        {
            int icon_y = row_y + (ROW_HEIGHT - A26_ICON_SIZE_CONTENT_LIST) / 2;
            int w = is_selected
                ? aura_widgets_draw_icon_selected(items[i].icon_name,
                                                   A26_ICON_SIZE_CONTENT_LIST, text_x, icon_y)
                : aura_widgets_draw_icon(items[i].icon_name,
                                          A26_ICON_SIZE_CONTENT_LIST, text_x, icon_y);
            if (w > 0)
                text_x += w + ICON_TEXT_GAP;
        }

        lcd_set_foreground(items[i].dimmed
            ? a26_shell_blend(a26_color(A26_SHELL_BG), a26_color(A26_TEXT_PRIMARY), 128)
            : (is_selected ? a26_color(A26_ACCENT) : a26_color(A26_TEXT_PRIMARY)));
        /* En pantalla completa el riel A-Z ocupa la columna derecha:
         * el texto se recorta antes de invadirla. */
        {
            struct viewport vp = *lcd_current_viewport;
            struct viewport *saved;
            int text_w = width - text_x - (split ? ROW_PAD_X : AURA_DS_METRICS_INDEX_RAIL_WIDTH + A26_SPACING_XS);

            vp.x = text_x;
            vp.y = row_y + A26_SPACING_SM;
            vp.width = (text_w > 0) ? text_w : 1;
            saved = lcd_set_viewport(&vp);
            lcd_putsxy(0, 0, (const unsigned char *)items[i].label);
            lcd_set_viewport(saved);
        }

        if (items[i].toggle >= 0)
        {
            int toggle_x = width - ROW_PAD_X - TOGGLE_W;
            int toggle_y = row_y + (ROW_HEIGHT - TOGGLE_H) / 2;
            unsigned row_bg = is_selected ? a26_color(A26_SELECTION_FILL)
                                           : a26_color(A26_SHELL_BG);
            aura_widgets_draw_toggle(toggle_x, toggle_y, items[i].toggle, row_bg);
        }
        else if (items[i].checked)
        {
            int check_x = width - ROW_PAD_X - A26_ICON_SIZE_CONTENT_LIST;
            int check_y = row_y + (ROW_HEIGHT - A26_ICON_SIZE_CONTENT_LIST) / 2;
            if (is_selected)
                aura_widgets_draw_icon_selected("check", A26_ICON_SIZE_CONTENT_LIST, check_x, check_y);
            else
                aura_widgets_draw_icon("check", A26_ICON_SIZE_CONTENT_LIST, check_x, check_y);
        }
    }

    if (split)
        draw_right_panel_debounced(count > 0 ? items[selected].icon_name : NULL);
    else
        draw_index_rail(items, count, selected);

    draw_scrollbar(width, count, selected);
}

void aura_widgets_draw_toggle(int x, int y, int value, unsigned bg)
{
    int thumb_h = TOGGLE_H - 2 * TOGGLE_MARGIN;
    int thumb_x = value ? (x + TOGGLE_W - TOGGLE_MARGIN - TOGGLE_THUMB_W)
                         : (x + TOGGLE_MARGIN);
    unsigned off_track = a26_shell_blend(a26_color(A26_SHELL_RAIL),
                                          a26_color(A26_TEXT_PRIMARY), 90);
    unsigned track = value ? aura_accent() : off_track;
    unsigned white = (unsigned)AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT;
    unsigned thumb = value ? a26_shell_blend(track, white, 205)
                            : a26_shell_blend(off_track, white, 245);

    /* Capsulas via fill_rounded_rect con radio = alto/2 (la primitiva
     * las acota ahi): misma forma, y es el camino con antialias ya
     * probado en todo el sistema. */
    a26_shell_fill_rounded_rect(x, y, TOGGLE_W, TOGGLE_H, TOGGLE_H / 2, track, bg);
    a26_shell_fill_rounded_rect(thumb_x, y + TOGGLE_MARGIN, TOGGLE_THUMB_W, thumb_h,
                                 thumb_h / 2, thumb, track);
}

/* -- Fila booleana (L11) --------------------------------------------- */

void aura_widgets_draw_bool_row(const char *title, const char *label,
                                 int value)
{
    const char *value_text = aura_str(value ? AURA_STR_YES : AURA_STR_NO);
    int w, h;

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, title);

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_putsxy(ROW_PAD_X, LIST_TOP + A26_SPACING_SM,
               (const unsigned char *)label);

    lcd_set_foreground(a26_color(A26_ACCENT));
    lcd_getstringsize((const unsigned char *)value_text, &w, &h);
    lcd_putsxy(A26_SCREEN_WIDTH - ROW_PAD_X - w, LIST_TOP + A26_SPACING_SM,
               (const unsigned char *)value_text);
}

/* -- Slider horizontal (migra el de Brillo, D-014) -------------------- */

void aura_widgets_draw_slider(const char *title, int fraction,
                               const char *value_text)
{
    int bar_x = A26_SPACING_XXL;
    int bar_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    int bar_y = A26_SCREEN_HEIGHT / 2;
    int fill_w;

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, title);

    if (fraction < 0)   fraction = 0;
    if (fraction > 256) fraction = 256;

    /* PROGRESS_TRACK/PROGRESS_FILL (doc SS2), no SHELL_RAIL/ACCENT --
     * son tokens de progreso, no de separador/estado-activo (Fase 32,
     * D-081: el mismo error de token que D-073 ya habia corregido en
     * aura_widgets_draw_progress() se habia colado aca tambien, sin
     * auditar -- Brillo y Limite volumen lo usan). Extremos redondeados
     * con la misma primitiva compartida que el resto del sistema, radio
     * "tarjeta" (8px, SS5.4) -- es una barra mas gruesa que la pastilla
     * de progreso canonica (16px de alto vs 4px), no la misma pieza. */
    a26_shell_fill_rounded_rect(bar_x, bar_y, bar_w, A26_SPACING_XL,
                                 A26_LAYOUT_CORNER_RADIUS_CARD,
                                 a26_color(A26_PROGRESS_TRACK), a26_color(A26_SHELL_BG));

    fill_w = (bar_w * fraction) / 256;
    if (fill_w > 0)
        a26_shell_fill_rounded_rect(bar_x, bar_y, fill_w, A26_SPACING_XL,
                                     A26_LAYOUT_CORNER_RADIUS_CARD,
                                     a26_color(A26_PROGRESS_FILL), a26_color(A26_SHELL_BG));

    if (value_text)
    {
        lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_putsxy(bar_x, bar_y + A26_SPACING_XL + A26_SPACING_SM,
                   (const unsigned char *)value_text);
    }
}

/* -- Selector de digitos (L10, pantallas de fecha/hora/codigo) -------- */

#define DIGIT_BOX_W  24
#define DIGIT_BOX_H  32
#define DIGIT_GAP    A26_SPACING_SM

void aura_widgets_draw_digits(const char *title, const int *digits,
                               int count, int focus)
{
    int total_w = count * DIGIT_BOX_W + (count - 1) * DIGIT_GAP;
    int start_x = (A26_SCREEN_WIDTH - total_w) / 2;
    int box_y = A26_SCREEN_HEIGHT / 2 - DIGIT_BOX_H / 2;
    int i;

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, title);

    lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));

    for (i = 0; i < count; i++)
    {
        int box_x = start_x + i * (DIGIT_BOX_W + DIGIT_GAP);
        char digit_str[2] = { (char)('0' + digits[i]), '\0' };
        int w, h;
        int is_focus = (i == focus);

        lcd_set_foreground(is_focus ? a26_color(A26_SELECTION_FILL)
                                     : a26_color(A26_SHELL_RAIL));
        lcd_fillrect(box_x, box_y, DIGIT_BOX_W, DIGIT_BOX_H);

        lcd_getstringsize((const unsigned char *)digit_str, &w, &h);
        lcd_set_foreground(is_focus ? a26_color(A26_ACCENT)
                                     : a26_color(A26_TEXT_PRIMARY));
        lcd_putsxy(box_x + (DIGIT_BOX_W - w) / 2, box_y + (DIGIT_BOX_H - h) / 2,
                   (const unsigned char *)digit_str);
    }
}

/* -- Aviso bloqueante con 2 opciones (S3.8) ---------------------------- */

#define CONFIRM_MAX_LINES 4

/* Word-wrap simple y propio: no hace falta la generalidad de
 * apps/gui/splash.c (tabs, multi-pantalla, memoria de tamano maximo)
 * para un cuerpo corto de 2-3 lineas fijas. */
int aura_widgets_wrap_text(const char *text, int max_width, const char **lines, int *lens, int max_lines)
{
    int n = 0;
    const char *p = text;

    while (*p && n < max_lines)
    {
        const char *line_start = p;
        const char *last_space = NULL;
        const char *cursor = p;
        char buf[128];

        /* Salto de linea explicito: corta AQUI y arranca parrafo nuevo
         * (correccion 2026-08-13 -- sin esto, los textos largos con
         * parrafos, como Notas o Avisos legales, se leian como un solo
         * bloque corrido). */
        if (*p == '\n')
        {
            lines[n] = p;
            lens[n] = 0;   /* linea en blanco = separacion de parrafos */
            n++;
            p++;
            continue;
        }

        while (*cursor && *cursor != '\n')
        {
            int len = (int)(cursor - line_start) + 1;
            int w, h;
            if (len >= (int)sizeof(buf))
                break;
            memcpy(buf, line_start, len);
            buf[len] = '\0';
            lcd_getstringsize((const unsigned char *)buf, &w, &h);
            if (w > max_width && last_space)
                break;
            if (*cursor == ' ')
                last_space = cursor;
            cursor++;
        }

        if (*cursor == '\n')
        {
            lines[n] = line_start;
            lens[n] = (int)(cursor - line_start);
            n++;
            p = cursor + 1;
            continue;
        }

        if (*cursor == '\0')
        {
            lines[n] = line_start;
            lens[n] = (int)(cursor - line_start);
            n++;
            break;
        }

        if (last_space)
        {
            lines[n] = line_start;
            lens[n] = (int)(last_space - line_start);
            p = last_space + 1;
        }
        else
        {
            lines[n] = line_start;
            lens[n] = (int)(cursor - line_start);
            p = cursor;
        }
        n++;
    }
    return n;
}

void aura_widgets_draw_confirm(const char *title, const char *body, int yes_selected)
{
    const char *lines[CONFIRM_MAX_LINES];
    int lens[CONFIRM_MAX_LINES];
    int box_w = A26_SCREEN_WIDTH - 2 * A26_SPACING_XXL;
    int box_x = A26_SPACING_XXL;
    int text_y, i, n;
    int btn_y, btn_w, yes_x, no_x;
    const char *yes_label = aura_str(AURA_STR_YES);
    const char *no_label = aura_str(AURA_STR_NO);

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, title);

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));

    n = aura_widgets_wrap_text(body, box_w, lines, lens, CONFIRM_MAX_LINES);
    text_y = A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_XXL;
    for (i = 0; i < n; i++)
    {
        /* lcd_putsxy no acepta longitud: se corta a un buffer chico con
         * terminador nulo antes de dibujar cada linea envuelta. */
        char buf[128];
        int len = lens[i];
        if (len >= (int)sizeof(buf))
            len = sizeof(buf) - 1;
        memcpy(buf, lines[i], len);
        buf[len] = '\0';
        lcd_putsxy(box_x, text_y, (const unsigned char *)buf);
        text_y += A26_TYPE_BODY + A26_SPACING_SM;
    }

    btn_w = 100;
    btn_y = A26_SCREEN_HEIGHT - A26_SPACING_XXL - 32;
    no_x = box_x;
    yes_x = box_x + box_w - btn_w;

    /* Chips redondeados, mismo radio de pastilla que la lista (SS5.4:
     * nunca un radio suelto por componente) -- antes eran rectangulos a
     * bordes vivos, el unico control del sistema que no seguia la
     * jerarquia concentrica de radios. */
    a26_shell_fill_rounded_rect(no_x, btn_y, btn_w, 32, PILL_RADIUS,
                                 a26_color(!yes_selected ? A26_SELECTION_FILL : A26_SHELL_RAIL),
                                 a26_color(A26_SHELL_BG));
    a26_shell_fill_rounded_rect(yes_x, btn_y, btn_w, 32, PILL_RADIUS,
                                 a26_color(yes_selected ? A26_SELECTION_FILL : A26_SHELL_RAIL),
                                 a26_color(A26_SHELL_BG));

    {
        int w, h;
        lcd_set_foreground(a26_color(!yes_selected ? A26_ACCENT : A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)no_label, &w, &h);
        lcd_putsxy(no_x + (btn_w - w) / 2, btn_y + (32 - h) / 2, (const unsigned char *)no_label);

        lcd_set_foreground(a26_color(yes_selected ? A26_ACCENT : A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)yes_label, &w, &h);
        lcd_putsxy(yes_x + (btn_w - w) / 2, btn_y + (32 - h) / 2, (const unsigned char *)yes_label);
    }
}

/* Capsula flotante de espera (doc SS5.2, Principio 3: "la carga
 * convive, no interrumpe"): NO limpia pantalla ni ocupa una pagina
 * completa -- se dibuja encima de lo que ya esta a la vista, como
 * ultimo paso del dibujo de la pantalla que la necesita. Geometria fija
 * del documento: x=40, ancho pantalla-80, y=alto-14, capsula de 12px con
 * radio 6 (A26_LAYOUT_CORNER_RADIUS_CAPSULE), fondo SHELL_BG y borde
 * SHELL_RAIL de 1px -- unica superficie de esperas del sistema; unica
 * pagina completa que le queda al aparato es para sus propios estados
 * (apagado, USB, base de datos vacia), no para un progreso. */
void aura_widgets_draw_wait_capsule(const char *text)
{
    int cap_h = 12;
    int cap_y = A26_SCREEN_HEIGHT - 14;
    int cap_x = 40;
    int cap_w = A26_SCREEN_WIDTH - 80;
    int w, h;

    a26_shell_outline_rounded_rect(cap_x, cap_y, cap_w, cap_h,
                                    A26_LAYOUT_CORNER_RADIUS_CAPSULE,
                                    a26_color(A26_SHELL_BG),
                                    a26_color(A26_SHELL_RAIL),
                                    a26_color(A26_SHELL_BG));

    if (!text)
        return;

    /* MICRO (7px), no CAPTION (13px) -- AUDITORIA-01 A-20/A-d: el cuerpo
     * de 13px no entra en los 12px de alto de la capsula (doc SS5.2) sin
     * pisar el borde superior/inferior. La geometria de la capsula es
     * deliberadamente la misma que la capsula flotante de progreso (SS5.2)
     * -- crecerla rompe esa consistencia entre las dos unicas superficies
     * flotantes del sistema; achicar el texto no. */
    lcd_setfont(a26_font(A26_FONT_STYLE_MICRO));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)text, &w, &h);
    lcd_putsxy(cap_x + (cap_w - w) / 2, cap_y + (cap_h - h) / 2,
               (const unsigned char *)text);
}
