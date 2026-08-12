#include <string.h>
#include <stdlib.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "tick.h"

#include "aura_coverflow.h"
#include "aura_music.h"
#include "aura_albumart.h"
#include "aura_art.h"
#include "aura_widgets.h"
#include "aura_wheel.h"
#include "aura_main.h"
#include "aura_transitions.h"
#include "aura_flow.h"
#include "aura_motion.h"
#include "aura_patterns.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_status_bar_v2.h"

/* Coverflow simplificado (D-025): en vez de perspectiva 3D real por
 * cuadro (demasiado costosa para un ARM926EJ-S a ~216MHz), todas las
 * caratulas visibles se decodifican una sola vez al mismo tamano fijo
 * y se cachean; la caratula central se dibuja a brillo completo, las
 * laterales atenuadas hacia el color de fondo (mismo blend que usa el
 * reflejo, D-024/D-025; sin marco de acento -- AUDITORIA-01 A-11). El
 * "flujo" avanza segun la velocidad real del clickwheel
 * (aura_wheel_step(), AUDITORIA-01 A-13) -- ya no la heuristica vieja
 * de "dos eventos en menos de HZ/6 = paso 2".
 */

/* PLAN.md T3.2(b), componentes/cover-flow.md + G16: portada central
 * ~100px, radio de esquina 5px (empata Selector), reflejo al 25% del
 * alto del slide (mas sutil que el original de PictureFlow, ~33%), 3
 * por lado visibles -- los 4 provisionales de G16, ya resueltos en
 * T0.1/tokens.json, no inventados aqui. */
#define CF_COVER_SIZE     AURA_DS_METRICS_COVER_FLOW_CENTER_SLIDE_SIZE
#define CF_CORNER_RADIUS  AURA_DS_METRICS_COVER_FLOW_CORNER_RADIUS
#define CF_REFLECTION_PCT AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT
#define CF_TOP_Y          30 /* borde superior del central, medido de la referencia de Apple */
#define CF_VISIBLE_RADIUS AURA_DS_METRICS_COVER_FLOW_SIDE_SLIDES_PER_SIDE
#define CF_CACHE_SLOTS    (2 * CF_VISIBLE_RADIUS + 3) /* visibles + margen para scroll suave */
#define CF_SIDE_FADE      165 /* de 255 -- laterales visibles como en la referencia, no apagadas */
/* Alto del reflejo en tiempo de compilacion -- mismo calculo que
 * aura_art_reflection_height(), pero constante en tiempo de
 * compilacion (hace falta para dimensionar el buffer estatico de cada
 * slot). */
#define CF_REFLECTION_H   (CF_COVER_SIZE * CF_REFLECTION_PCT / 100)
/* Lineas de texto bajo el central (titulo bold + artista regular,
 * posiciones medidas de la referencia del original: ~y182/y202 en
 * 320x240 con central de 130 a partir de y30). */
#define CF_TITLE_Y  (CF_TOP_Y + CF_COVER_SIZE + 22)
#define CF_ARTIST_Y (CF_TITLE_Y + 18)

typedef struct {
    int album_index; /* -1 = slot libre/no cargado */
    aura_albumart_t art;
    unsigned char cover_buf[CF_COVER_SIZE * CF_COVER_SIZE * sizeof(fb_data)];
    unsigned char reflection_buf[CF_COVER_SIZE * CF_REFLECTION_H * sizeof(fb_data)];
} cf_slot_t;

static cf_slot_t s_slots[CF_CACHE_SLOTS];

/* -- Estados idle/scrolling (PLAN.md T3.2(b), componentes/cover-flow.md:
 * "idle -- carrusel quieto en un album" / "scrolling -- desplazandose
 * entre albumes") --------------------------------------------------------
 *
 * pictureflow.c entra a pf_scrolling mientras el boton de avance esta
 * FISICAMENTE sostenido y vuelve a pf_idle al soltarlo -- ese modelo no
 * mapea directo al clickwheel de Aura (eventos discretos de giro, no un
 * boton mantenido). La lectura equivalente para Aura: "scrolling" es
 * mientras la posicion animada todavia no alcanzo el indice objetivo
 * (`s_target_index`), "idle" es cuando ya coinciden -- mismo resultado
 * observable (el carrusel se desliza mientras el usuario gira la rueda
 * y hasta un instante despues, se asienta cuando termina), sin depender
 * de un modelo de boton sostenido que Aura no tiene.
 *
 * `s_target_index` es la seleccion COMPROMETIDA (la que usan SELECT y
 * la etiqueta) -- nunca la posicion visual a medio deslizar. La posicion
 * animada real se recalcula cada cuadro con aura_pattern_lerp() (T1.1,
 * cero matematica nueva) desde `s_anim_from_x256` (snapshot de portada
 * animada al momento en que arranco el paso ACTUAL, para poder
 * redirigir sin salto si el usuario sigue girando la rueda antes de que
 * el paso anterior termine de asentarse -- mismo patron ya usado en
 * CoverDrift/T2.9 y el morph de NowPlaying/T3.1c). */
#define CF_SCROLL_ANIM_MS 220 /* TODO(pendiente-doc): el documento no da un timing de "snap con aceleracion", ver DECISIONS.md D-103 */
static int s_target_index = 0;
static int s_anim_from_x256 = 0;
static long s_anim_since = 0;

static aura_screen_id_t s_cache_screen = AURA_SCREEN_COUNT;
static int s_cache_generation = -1;
static aura_music_item_t s_albums[AURA_MUSIC_MAX_ITEMS];
static int s_album_count = 0;

/* Posicion animada actual, en unidades de "indice de album" x256 --
 * interpola desde `s_anim_from_x256` hacia `s_target_index*256` en
 * CF_SCROLL_ANIM_MS. Nunca se llama dos veces por cuadro con resultados
 * distintos: current_tick es estable dentro de un mismo cuadro. */
static int anim_pos_x256(void)
{
    long elapsed_ms = (current_tick - s_anim_since) * 1000L / HZ;
    int t = aura_motion_linear(elapsed_ms, CF_SCROLL_ANIM_MS);
    return aura_pattern_lerp(s_anim_from_x256, s_target_index * 256, t);
}

/* -- Flip + TrackList (PLAN.md T3.2(c), componentes/cover-flow.md:
 * "seleccion de album: flip clasico fiel al iPod original -- la
 * portada gira/voltea como carta y detras esta la lista de pistas")
 * --------------------------------------------------------------------
 *
 * Estados heredados de pictureflow.c (investigados con un agente
 * dedicado antes de tocar codigo, regla dura 7): cover_in (la tapa
 * gira saliendo) -> show_tracks (lista visible) -> cover_out (la tapa
 * gira regresando) -> idle. pictureflow.c NO renderiza la lista A
 * TRAVES del giro continuo -- solo anima la tapa en cover_in/cover_out
 * y cambia a un camino de render aparte (2D plano) en show_tracks; se
 * sigue el mismo criterio aca, ver draw_slide_flip(). */
typedef enum {
    CF_STATE_IDLE = 0,
    CF_STATE_COVER_IN,
    CF_STATE_SHOW_TRACKS,
    CF_STATE_COVER_OUT,
} cf_state_t;

#define CF_FLIP_MS 260 /* TODO(pendiente-doc): "Timing de cada fase de Flip-and-Flow" no definido, ver DECISIONS.md D-104 */

static cf_state_t s_state = CF_STATE_IDLE;
static long s_state_since = 0;
static aura_music_item_t s_tracks[AURA_MUSIC_MAX_ITEMS];
static int s_track_count = 0;
static int s_track_sel = 0;

static int flip_progress_256(void)
{
    long elapsed_ms = (current_tick - s_state_since) * 1000L / HZ;
    return aura_motion_linear(elapsed_ms, CF_FLIP_MS);
}

int aura_coverflow_pending(void)
{
    return anim_pos_x256() != s_target_index * 256
        || s_state == CF_STATE_COVER_IN || s_state == CF_STATE_COVER_OUT;
}

int aura_coverflow_animating(void)
{
    return aura_coverflow_pending();
}

static void ensure_albums(aura_screen_id_t screen)
{
    int gen = aura_music_filter_generation();
    int i;

    if (s_cache_screen == screen && s_cache_generation == gen)
        return;

    s_cache_screen = screen;
    s_cache_generation = gen;
    s_album_count = aura_music_browse(screen, s_albums, AURA_MUSIC_MAX_ITEMS);
    s_target_index = 0;
    s_anim_from_x256 = 0;
    s_anim_since = current_tick;

    for (i = 0; i < CF_CACHE_SLOTS; i++)
        s_slots[i].album_index = -1;
}

static cf_slot_t *get_slot_for(int album_index)
{
    int i, target, free_slot = -1, farthest = -1, farthest_dist = -1;

    for (i = 0; i < CF_CACHE_SLOTS; i++)
        if (s_slots[i].album_index == album_index)
            return &s_slots[i];

    for (i = 0; i < CF_CACHE_SLOTS; i++)
    {
        int dist;
        if (s_slots[i].album_index == -1)
        {
            free_slot = i;
            break;
        }
        dist = abs(s_slots[i].album_index - s_target_index);
        if (dist > farthest_dist)
        {
            farthest_dist = dist;
            farthest = i;
        }
    }
    target = (free_slot >= 0) ? free_slot : farthest;

    s_slots[target].album_index = album_index;
    s_slots[target].art.size = CF_COVER_SIZE;
    s_slots[target].art.radius = CF_CORNER_RADIUS;
    s_slots[target].art.cover_data = s_slots[target].cover_buf;
    s_slots[target].art.reflection_data = s_slots[target].reflection_buf;
    if (!aura_albumart_load_for_album(s_albums[album_index].seek, &s_slots[target].art))
        aura_albumart_load_default(&s_slots[target].art);

    return &s_slots[target];
}

/* Atenua un pixel hacia el color de fondo del tema segun `fade` (255 =
 * sin atenuar) -- mismo blend que aura_albumart.c usa para el reflejo,
 * factorizado para que tambien lo use el renderizador de perspectiva. */
static fb_data fade_pixel(unsigned px, int fade, int bg_r, int bg_g, int bg_b)
{
    int r = RGB_UNPACK_RED(px);
    int g = RGB_UNPACK_GREEN(px);
    int b = RGB_UNPACK_BLUE(px);

    if (fade < 255)
    {
        r = bg_r + ((r - bg_r) * fade) / 255;
        g = bg_g + ((g - bg_g) * fade) / 255;
        b = bg_b + ((b - bg_b) * fade) / 255;
    }
    return LCD_RGBPACK(r, g, b);
}

/* -- Perspectiva real (Fase 31.1/31.2, D-079/D-080; T3.2(a) extiende a
 * datos transpuestos; T3.2(b) generaliza a offset CONTINUO) -------------
 *
 * Geometria de "reposo" de un coverflow clasico (pictureflow.c
 * reset_slides(), resuelta a numeros concretos para 320x240 -- ver
 * test_realistic_side_slide_layout() en test_flow.c). Se usa para TODAS
 * las tapas, central incluida (offset=0, angle=0, cx=0 -- angulo 0 en
 * la formula de perspectiva da un mapeo 1:1 sin distorsion): ya no hay
 * un camino separado con blit_dimmed() para la central, que asumia
 * datos fila-contigua y quedo incompatible cuando el cache .pfraw
 * (T3.2(a)) empezo a guardar las caratulas TRANSPUESTAS -- unificar en
 * una sola funcion evita mantener dos formatos de lectura de pixel. */
#define CF_ITILT           199    /* ~70 grados: 70*1024/360, mismo valor fijo de pictureflow.c (no depende del tamano del slide) */
#define CF_OFFSETX_R       92000  /* separacion centro-a-lateral, re-derivada para CF_COVER_SIZE=130 contra la referencia del original de Apple (la primera lateral queda pegada al borde del central) */
#define CF_SLIDE_SPACING_R 29000  /* laterales apretadas y traslapadas (~28px entre vecinas), como la referencia */

/* `offset_x256` es la distancia (en unidades de "album", x256) entre
 * esta tapa y la posicion animada actual del carrusel -- puede ser
 * fraccionaria mientras el carrusel esta "scrolling" (T3.2(b)). angulo,
 * cx y fade se interpolan CONTINUOS a partir de esa distancia (con
 * aura_pattern_lerp, T1.1) en vez de tomar solo los valores discretos
 * de reposo -- exactos en los offsets enteros (fmuln con offset=k*256
 * da identico resultado a la formula discreta anterior), suaves entre
 * medio. Es lo que hace que las tapas se DESLICEN entre posiciones en
 * vez de saltar. */
static void draw_slide_perspective(const cf_slot_t *slot, int offset_x256)
{
    aura_flow_slide_t slide;
    aura_flow_projection_t proj;
    int sign = (offset_x256 < 0) ? -1 : (offset_x256 > 0 ? 1 : 0);
    int abs_x256 = (offset_x256 < 0) ? -offset_x256 : offset_x256;
    int t_center = abs_x256 < 256 ? abs_x256 : 256; /* 0..256: 0=centro, 256=primera lateral en reposo */
    int extra_x256 = abs_x256 - 256;
    int fade;
    const fb_data *cover = (const fb_data *)slot->art.cover_data;   /* transpuesto: cover[col*size+row] */
    const fb_data *refl = (const fb_data *)slot->art.reflection_data; /* transpuesto: refl[col*refl_h+row] */
    int refl_h = aura_art_reflection_height(CF_COVER_SIZE, CF_REFLECTION_PCT);
    int total_h = CF_COVER_SIZE + refl_h;
    unsigned bg = a26_color(A26_SHELL_BG);
    int bg_r = RGB_UNPACK_RED(bg);
    int bg_g = RGB_UNPACK_GREEN(bg);
    int bg_b = RGB_UNPACK_BLUE(bg);
    static fb_data col_buf[CF_COVER_SIZE + CF_REFLECTION_H];

    if (extra_x256 < 0)
        extra_x256 = 0;

    /* Mismo signo que pictureflow.c reset_slides(): la lateral izquierda
     * (offset<0) angulo positivo y cx negativo, la derecha al reves --
     * la carga muestra su borde hacia el centro de la pantalla. En
     * offset==0 (sign==0) todo colapsa a angulo/cx cero, sin distorsion,
     * igual que antes. */
    slide.angle = -sign * aura_pattern_lerp(0, CF_ITILT, t_center);
    slide.distance = 0;
    slide.cx = sign * (aura_pattern_lerp(0, CF_OFFSETX_R, t_center)
                        + (int)((long)CF_SLIDE_SPACING_R * extra_x256 / 256));

    fade = aura_pattern_lerp(255, CF_SIDE_FADE, t_center);

    aura_flow_begin_projection(&proj, &slide, CF_COVER_SIZE);

    while (proj.screen_x < AURA_FLOW_SCREEN_W)
    {
        int col = aura_flow_source_column(&proj);
        int dy = aura_flow_vertical_scale(&proj);
        int p = 0, dest_row, n_rows = 0;
        const fb_data *cover_col = cover + (size_t)col * CF_COVER_SIZE;
        const fb_data *refl_col = refl + (size_t)col * refl_h;
        /* Centrado VERTICAL por columna (correccion contra la captura
         * del original de Apple, 2026-08-12): cada columna se ancla de
         * modo que la CARATULA quede centrada en su linea media -- la
         * perspectiva encoge ambos bordes (superior e inferior
         * convergen hacia el punto de fuga), como el Cover Flow real.
         * Anclar todas las columnas al mismo borde superior (la
         * version anterior) producia tapas con el borde de arriba
         * perfectamente horizontal, que es lo que delato el error. */
        int cover_disp = (CF_COVER_SIZE << AURA_FLOW_SHIFT) / dy;
        int y_col = CF_TOP_Y + CF_COVER_SIZE / 2 - cover_disp / 2;

        for (dest_row = 0; dest_row < total_h; dest_row++)
        {
            int source_row = p >> AURA_FLOW_SHIFT;
            unsigned px;

            if (source_row >= total_h)
                break;
            px = (source_row < CF_COVER_SIZE)
                ? cover_col[source_row]
                : refl_col[source_row - CF_COVER_SIZE];
            col_buf[dest_row] = fade_pixel(px, fade, bg_r, bg_g, bg_b);
            p += dy;
            n_rows++;
        }
        if (n_rows > 0)
            lcd_bitmap(col_buf, proj.screen_x, y_col, 1, n_rows);

        if (!aura_flow_advance_column(&proj))
            break;
    }
}

/* Flip real (giro sobre el eje Y): reusa DIRECTO el mismo motor de
 * proyeccion por columnas que draw_slide_perspective() (regla dura 7),
 * con un angulo continuo 0->256 (0->90 grados en unidades IANGLE,
 * 1024=360) en vez de la formula de lateral con tope en CF_ITILT. A 90
 * grados la tapa se ve de perfil (practicamente invisible) -- mas alla
 * (90-180) mostraria el reverso de la MISMA imagen reflejado, que no
 * es contenido real (una caratula no tiene "parte de atras"), asi que
 * el giro se corta ahi y el estado cambia a SHOW_TRACKS, que dibuja la
 * lista con un camino de render 2D aparte en vez de seguir
 * proyectando -- mismo criterio que el propio pictureflow.c (ver
 * comentario de la maquina de estados arriba). Sin reflejo ni
 * atenuacion durante el giro: es una animacion corta y unica, no vale
 * la pena la complejidad de desvanecerlas de paso. */
static void draw_slide_flip(const cf_slot_t *slot, int iangle_0_to_256)
{
    aura_flow_slide_t slide;
    aura_flow_projection_t proj;
    const fb_data *cover = (const fb_data *)slot->art.cover_data;
    static fb_data col_buf[CF_COVER_SIZE];

    slide.angle = iangle_0_to_256;
    slide.distance = 0;
    slide.cx = 0;

    aura_flow_begin_projection(&proj, &slide, CF_COVER_SIZE);

    while (proj.screen_x < AURA_FLOW_SCREEN_W)
    {
        int col = aura_flow_source_column(&proj);
        int dy = aura_flow_vertical_scale(&proj);
        int p = 0, dest_row, n_rows = 0;
        const fb_data *cover_col = cover + (size_t)col * CF_COVER_SIZE;

        int cover_disp = (CF_COVER_SIZE << AURA_FLOW_SHIFT) / dy;
        int y_col = CF_TOP_Y + CF_COVER_SIZE / 2 - cover_disp / 2;

        for (dest_row = 0; dest_row < CF_COVER_SIZE; dest_row++)
        {
            int source_row = p >> AURA_FLOW_SHIFT;

            if (source_row >= CF_COVER_SIZE)
                break;
            col_buf[dest_row] = cover_col[source_row];
            p += dy;
            n_rows++;
        }
        /* Centrado vertical por columna -- mismo criterio que
         * draw_slide_perspective(): el giro es sobre el eje central. */
        if (n_rows > 0)
            lcd_bitmap(col_buf, proj.screen_x, y_col, 1, n_rows);

        if (!aura_flow_advance_column(&proj))
            break;
    }
}

/* Recorta `text` al ancho `w` -- mismo mecanismo de viewport que
 * aura_marquee.c/aura_dynamic_title.c/LyricsPanel (T3.1c), reutilizado
 * aca por cuarta vez en la sesion. */
static void draw_clipped_text(int x, int y, int w, const char *text)
{
    struct viewport vp = *lcd_current_viewport;
    struct viewport *saved;

    vp.x = x;
    vp.y = y;
    vp.width = w;
    saved = lcd_set_viewport(&vp);
    lcd_putsxy(0, 0, (const unsigned char *)text);
    lcd_set_viewport(saved);
}

/* TrackList (componentes/cover-flow.md, G16: "lista simple centrada en
 * la cara trasera", ya resuelto en PLAN.md -- no una re-implementacion
 * de MenuList v2 completo). Ocupa el mismo hueco que la caratula, no
 * la pantalla completa -- "detras esta la lista de pistas" es
 * literal: mismo lugar, no una pantalla nueva. */
#define CF_TRACK_ROW_H 16

static void draw_tracklist_panel(void)
{
    int panel_x = A26_SCREEN_WIDTH / 2 - CF_COVER_SIZE / 2;
    int panel_y = CF_TOP_Y;
    int visible = CF_COVER_SIZE / CF_TRACK_ROW_H;
    int first, i;
    unsigned bg = a26_color(A26_SHELL_BG);

    a26_shell_fill_rounded_rect(panel_x, panel_y, CF_COVER_SIZE, CF_COVER_SIZE,
                                 CF_CORNER_RADIUS, a26_color(A26_SHELL_RAIL), bg);

    if (s_track_count == 0)
        return;

    first = s_track_sel - visible / 2;
    if (first < 0)
        first = 0;
    if (first > s_track_count - visible)
        first = s_track_count > visible ? s_track_count - visible : 0;

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    for (i = 0; i < visible && first + i < s_track_count; i++)
    {
        int idx = first + i;
        int row_y = panel_y + i * CF_TRACK_ROW_H + (CF_TRACK_ROW_H - A26_TYPE_CAPTION) / 2;

        lcd_set_foreground(a26_color(idx == s_track_sel ? A26_ACCENT : A26_TEXT_PRIMARY));
        draw_clipped_text(panel_x + A26_SPACING_SM, row_y,
                           CF_COVER_SIZE - 2 * A26_SPACING_SM, s_tracks[idx].label);
    }
}

static void draw_message(aura_str_id_t msg_id)
{
    int w, h;
    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)aura_str(msg_id), &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, (A26_SCREEN_HEIGHT - h) / 2,
               (const unsigned char *)aura_str(msg_id));
}

typedef struct { int idx; int offset_x256; } cf_entry_t;

void aura_coverflow_draw(aura_nav_t *nav, aura_screen_id_t screen)
{
    int pos_x256, center_idx, i, n;
    cf_entry_t entries[2 * CF_VISIBLE_RADIUS + 3];
    (void)nav;

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_MUSIC_COVERFLOW));

    if (!aura_music_db_ready())
    {
        /* Capsula flotante, no pagina completa (AUDITORIA-01 A-12,
         * Principio 3: "la carga convive, no interrumpe") -- mismo
         * anti-patron que D-073 ya habia corregido en
         * draw_music_browse(), sin replicar aca todavia. La barra de
         * estado ya se dibujo arriba; la capsula va encima. */
        aura_widgets_draw_wait_capsule(aura_str(AURA_STR_DB_NOT_READY));
        return;
    }

    ensure_albums(screen);

    if (s_album_count == 0)
    {
        draw_message(AURA_STR_EMPTY_LIST);
        return;
    }

    /* Transicion de fase de flip -- se resuelve UNA vez por cuadro,
     * antes del render, para que el resto de la funcion vea un estado
     * ya consistente (nunca a medio cambiar mientras dibuja). */
    if (s_state == CF_STATE_COVER_IN && flip_progress_256() >= 256)
        s_state = CF_STATE_SHOW_TRACKS;
    else if (s_state == CF_STATE_COVER_OUT && flip_progress_256() >= 256)
        s_state = CF_STATE_IDLE;

    /* Ventana de indices alrededor de la posicion ANIMADA (no del
     * objetivo comprometido) -- mientras "scrolling" (T3.2(b)), la
     * ventana se desliza cuadro a cuadro con la posicion real, dejando
     * ver de paso los albumes intermedios de un salto acelerado en vez
     * de solo aparecer/desaparecer en los extremos. */
    pos_x256 = anim_pos_x256();
    center_idx = (pos_x256 + (pos_x256 >= 0 ? 128 : -128)) / 256; /* redondeo al mas cercano */

    n = 0;
    for (i = -(CF_VISIBLE_RADIUS + 1); i <= CF_VISIBLE_RADIUS + 1; i++)
    {
        int idx = center_idx + i;
        if (idx < 0 || idx >= s_album_count)
            continue;
        entries[n].idx = idx;
        entries[n].offset_x256 = idx * 256 - pos_x256;
        n++;
    }

    /* De afuera hacia adentro (mayor |offset| -> menor), no de izquierda
     * a derecha: las laterales se proyectan en coordenadas de pantalla
     * reales (perspectiva, no una grilla de columnas fijas) y pueden
     * solaparse con la central cerca del borde -- dibujar la mas
     * cercana al final garantiza que quede siempre encima, como en
     * cualquier coverflow real. Insercion simple: `n` nunca pasa de
     * 2*radio+3 (un puñado de elementos). */
    {
        int a, b;
        for (a = 1; a < n; a++)
        {
            cf_entry_t key = entries[a];
            int key_abs = key.offset_x256 < 0 ? -key.offset_x256 : key.offset_x256;
            b = a - 1;
            while (b >= 0)
            {
                int b_abs = entries[b].offset_x256 < 0 ? -entries[b].offset_x256 : entries[b].offset_x256;
                if (b_abs >= key_abs)
                    break;
                entries[b + 1] = entries[b];
                b--;
            }
            entries[b + 1] = key;
        }
    }

    for (i = 0; i < n; i++)
    {
        int idx = entries[i].idx;
        int offset_x256 = entries[i].offset_x256;
        cf_slot_t *slot;

        /* Flip/TrackList (T3.2(c)): mientras no este IDLE, la tapa
         * objetivo (la unica que puede estar flipeando/mostrando su
         * lista -- scroll_step() no se llama en estos estados, ver
         * aura_coverflow_handle_button) se reemplaza por su propio
         * render en vez del carrusel normal; las laterales siguen
         * dibujandose igual que siempre. */
        if (s_state != CF_STATE_IDLE && idx == s_target_index)
        {
            slot = get_slot_for(idx);
            if (slot->art.valid)
            {
                if (s_state == CF_STATE_COVER_IN)
                    draw_slide_flip(slot, flip_progress_256());
                else if (s_state == CF_STATE_COVER_OUT)
                    draw_slide_flip(slot, 256 - flip_progress_256());
                else /* CF_STATE_SHOW_TRACKS */
                    draw_tracklist_panel();
            }
            continue;
        }

        slot = get_slot_for(idx);

        /* Sin marco de acento (AUDITORIA-01 A-11, Principio 1 "nada de
         * marcos/biseles" + Principio 2 "el acento nunca es decoracion
         * de superficie") -- la central ya se distingue por brillo (255
         * vs CF_SIDE_FADE), tamano y perspectiva, igual que en
         * cualquier coverflow real; el marco era decoracion, no
         * significado. Un solo camino de render para central y
         * laterales, con distancia CONTINUA (ver comentario de
         * draw_slide_perspective). */
        draw_slide_perspective(slot, offset_x256);
    }

    {
        int w, h;
        const char *label = s_albums[s_target_index].label;

        /* Dos lineas centradas bajo el central, como el original de
         * Apple: titulo del album (bold) + artista (regular, un tono
         * abajo). Se dibujan DESPUES del carrusel, encima de la zona
         * del reflejo (45% de pico, legible). El artista se busca en
         * tagcache solo cuando cambia el album objetivo (cache
         * estatico), nunca por cuadro. */
        static int s_artist_for_index = -1;
        static char s_artist[64];

        if (s_artist_for_index != s_target_index)
        {
            s_artist_for_index = s_target_index;
            aura_music_album_artist(s_albums[s_target_index].seek,
                                     s_artist, sizeof(s_artist));
        }

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
        lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)label, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, CF_TITLE_Y,
                   (const unsigned char *)label);

        if (s_artist[0])
        {
            lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
            lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
            lcd_getstringsize((const unsigned char *)s_artist, &w, &h);
            lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, CF_ARTIST_Y,
                       (const unsigned char *)s_artist);
        }
    }
}

/* Velocidad real del clickwheel (AUDITORIA-01 A-13, doc de diseno SS7 +
 * Fase 29 del plan: "modulo unico consumido por listas, coverflow y
 * scrub") -- reemplaza la heuristica vieja de "dos eventos en menos de
 * HZ/6 = paso 2", una aproximacion barata de antes de que existiera
 * aura_wheel_step() con la velocidad angular real del driver. Las
 * listas ya migraron en D-077/Fase 29; coverflow habia quedado afuera,
 * vacio real no anotado en su momento. */
static void scroll_step(int dir)
{
    int step = aura_wheel_step((int)aura_main_wheel_velocity());
    int new_target = s_target_index + dir * step;

    if (new_target < 0)
        new_target = 0;
    if (new_target >= s_album_count)
        new_target = s_album_count > 0 ? s_album_count - 1 : 0;

    /* Redirige desde la posicion animada ACTUAL (no desde el objetivo
     * viejo) -- si el usuario sigue girando la rueda antes de que el
     * paso anterior termine de asentarse, el carrusel no da un salto
     * brusco al nuevo destino, sigue deslizandose suave desde donde
     * ya estaba (mismo patron que el cross-fade de CoverDrift/T2.9 y
     * el morph de NowPlaying/T3.1c). */
    s_anim_from_x256 = anim_pos_x256();
    s_target_index = new_target;
    s_anim_since = current_tick;
}

void aura_coverflow_handle_button(aura_nav_t *nav, aura_screen_id_t screen, long button)
{
    (void)screen;

    /* Mientras el flip esta EN VUELO (cover_in/cover_out), el input se
     * ignora -- mismo criterio real que pictureflow.c usa para sus
     * transiciones (el usuario no puede interrumpir un flip a medio
     * camino). Solo IDLE y SHOW_TRACKS reaccionan a botones. */
    if (s_state == CF_STATE_COVER_IN || s_state == CF_STATE_COVER_OUT)
        return;

    if (s_state == CF_STATE_SHOW_TRACKS)
    {
        switch (button)
        {
        case BUTTON_SCROLL_FWD:
            if (s_track_count > 0)
                s_track_sel = (s_track_sel + 1) % s_track_count;
            break;
        case BUTTON_SCROLL_BACK:
            if (s_track_count > 0)
                s_track_sel = (s_track_sel - 1 + s_track_count) % s_track_count;
            break;
        case BUTTON_SELECT:
            /* Arranca la reproduccion real (mismo mecanismo que la
             * lista de canciones vieja) y vuela la caratula hasta
             * NowPlaying con Flip-and-Flow (T3.2(d)) -- la funcion de
             * transicion ya hace el aura_nav_push() al terminar. */
            if (s_track_count > 0 && aura_music_play_songs(AURA_SCREEN_MUSIC_SONGS_BY_ALBUM, s_track_sel))
                aura_transition_flip_and_flow(nav, s_albums[s_target_index].seek,
                                               CF_TOP_Y + CF_COVER_SIZE / 2);
            break;
        case BUTTON_MENU:
            /* "El album gira de nuevo (vuelve a mostrar la caratula)"
             * (doc, coreografia de salida) -- cover_out, no un pop de
             * navegacion (nunca se empujo una pantalla nueva). */
            s_state = CF_STATE_COVER_OUT;
            s_state_since = current_tick;
            break;
        default:
            break;
        }
        return;
    }

    /* CF_STATE_IDLE */
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        scroll_step(1);
        break;
    case BUTTON_SCROLL_BACK:
        scroll_step(-1);
        break;
    case BUTTON_SELECT:
        /* Select solo cicla el carrusel a reposo exacto -- flipear a
         * mitad de un deslizamiento (target != posicion animada) se
         * veria mal, la tapa objetivo todavia no esta en su lugar. */
        if (s_album_count > 0 && !aura_coverflow_pending())
        {
            aura_music_select_album(s_albums[s_target_index].seek);
            s_track_count = aura_music_browse(AURA_SCREEN_MUSIC_SONGS_BY_ALBUM,
                                               s_tracks, AURA_MUSIC_MAX_ITEMS);
            s_track_sel = 0;
            s_state = CF_STATE_COVER_IN;
            s_state_since = current_tick;
        }
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
