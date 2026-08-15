#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "audio.h"
#include "tick.h"

#include "aura_coverflow.h"
#include "aura_music.h"
#include "aura_albumart.h"
#include "aura_art.h"
#include "aura_widgets.h"
#include "aura_wheel.h"
#include "aura_scroll_indicator.h"
#include "aura_marquee.h"
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
/* D-224 (encargo del dueno, 2026-08-13: "preparar los siguientes 15 a
 * menos que haya memoria suficiente"): visibles + 15 de margen por
 * lado, no solo 3 -- el iPod 6G del proyecto (MEMORYSIZE=64, ver
 * firmware/export/config.h) sobra para esto. Costo por slot: cover_buf
 * (CF_COVER_SIZE^2 * 2B = 130*130*2 = 33800B) + reflection_buf
 * (CF_COVER_SIZE*CF_REFLECTION_H*2B = 130*32*2 = 8320B) = 42120B; con
 * el CF_VISIBLE_RADIUS=3 vigente hoy, 2*(3+15)+3 = 39 slots =~1.60MB
 * (~2.5% de los 64MB) -- s_slots es estatico (BSS), no vive en el
 * stack, ver su declaracion mas abajo. get_slot_for() ya evita relleno
 * inutil de esos slots extra: solo carga bajo demanda cuando el
 * carrusel realmente visita ese indice (nunca precarga el vecindario
 * el solo), y la precarga en disco (aura_music_precache_album_art(),
 * aura_music.c) es la que de verdad llena el cache de TODOS los
 * albumes -- este numero mas grande es para que, al desplazarse rapido
 * por una biblioteca grande, la ventana de RAM cubra un tramo mas largo
 * antes de tener que re-leer el .pfraw de un album ya visitado
 * recientemente. */
#define CF_CACHE_SLOTS    (2 * (CF_VISIBLE_RADIUS + 15) + 3)
#define CF_SIDE_FADE      165 /* de 255 -- laterales visibles como en la referencia, no apagadas */

/* Reverso crecido (encargo del dueno del diseno, 2026-08-12): al
 * voltearse, el album pasa de 130px a 200x200 -- centrado en el area
 * util bajo la StatusBar (220px), con una cabecera de 20px que muestra
 * "Album - Artista" y ScrollIndicator si la lista no cabe. Los textos
 * del carrusel se deslizan fuera de pantalla durante el giro para
 * cederle el espacio. 200 elegido por el dueno del diseno sobre la
 * tabla de opciones (180/200/220). Definidos aca arriba (no junto al
 * render del panel) porque aura_coverflow_pending() -- que vive
 * temprano en el archivo -- consulta la ventana del Fade-on-Idle. */
#define CF_TRACK_ROW_H_EARLY 20 /* == CF_TRACK_ROW_H, ver abajo */
#define CF_BACK_SIZE   200
#define CF_BACK_X      ((A26_SCREEN_WIDTH - CF_BACK_SIZE) / 2)
#define CF_BACK_Y      (A26_LAYOUT_STATUSBAR_HEIGHT \
                        + (A26_SCREEN_HEIGHT - A26_LAYOUT_STATUSBAR_HEIGHT - CF_BACK_SIZE) / 2)
#define CF_BACK_HEADER_H 20
#define CF_BACK_PADDING  4  /* padding interno del reverso (encargo 2026-08-12) */
/* Zoom del giro: la tapa (y su reflejo, en proporcion) crece de
 * CF_COVER_SIZE a CF_BACK_SIZE DURANTE la rotacion -- acercando la
 * camara del proyector (distance negativo): escala = CAM_DIST /
 * (CAM_DIST + d)  =>  d = CAM_DIST * COVER/BACK - CAM_DIST. */
#define CF_GROW_DISTANCE (AURA_FLOW_CAM_DIST * CF_COVER_SIZE / CF_BACK_SIZE \
                          - AURA_FLOW_CAM_DIST)
#define CF_BACK_VISIBLE_ROWS ((CF_BACK_SIZE - CF_BACK_HEADER_H) / CF_TRACK_ROW_H_EARLY)

/* Reloj de actividad del ScrollIndicator del reverso (Fade-on-Idle) --
 * a nivel de archivo porque la puerta de energia (pending()) tambien
 * lo consulta: sin cuadros pedidos durante la ventana del fundido, la
 * animacion nunca avanzaba y el indicador quedaba congelado invisible
 * (mismo bug ya documentado para el marquee del titulo viejo, D-091). */
static int s_prev_track_sel = -1;
static long s_track_activity_since = 0;

/* Marquee Loop de la cabecera "Album - Artista" del reverso -- reloj
 * por album objetivo + bandera de desborde para la puerta de energia
 * (sin cuadros pedidos, el loop se congela: mismo patron que el resto
 * de los marquees del sistema). */
static int s_header_for_index = -1;
static long s_header_since = 0;
static int s_header_overflowing = 0;

static int marquee_phase_scrolling(long since)
{
    long elapsed_ms = (current_tick - since) * 1000L / HZ;
    long cycle_ms = AURA_DS_METRICS_MARQUEE_STATIC_MS + AURA_DS_METRICS_MARQUEE_SCROLL_MS;

    if (cycle_ms <= 0)
        return 0;
    return (elapsed_ms % cycle_ms) >= AURA_DS_METRICS_MARQUEE_STATIC_MS;
}
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

/* -- Zoom de la caratula central al scrollear (D-245, encargo del
 * dueno del producto) -----------------------------------------------
 *
 * Mismo patron target/from/since que s_target_index/s_anim_from_x256/
 * s_anim_since de arriba, para una segunda dimension de animacion (la
 * escala en vez de la posicion): un "paso real" de scroll dispara un
 * zoom-out con ease-in hacia CF_ZOOM_SCALE_SHRUNK; en cuanto la
 * posicion se asienta (anim_pos_x256() alcanza el indice objetivo) se
 * dispara el zoom-in de vuelta a CF_ZOOM_SCALE_NORMAL con ease-out. Si
 * el usuario sigue girando la rueda mientras ya esta encogida, el
 * target no cambia -- no vuelve a pulsar por cada paso de una rafaga,
 * se queda encogida hasta que de verdad se detiene. */
#define CF_ZOOM_SCALE_NORMAL  256 /* 100%: distance=0, identico al render de siempre */
#define CF_ZOOM_SCALE_SHRUNK  240 /* ~94%: ver docs/aura-design-system/componentes/cover-flow.md */
#define CF_ZOOM_OUT_MS 150 /* ease-in, mas corto: el arranque debe sentirse inmediato */
#define CF_ZOOM_IN_MS  CF_SCROLL_ANIM_MS /* ease-out, mismo tiempo que el asentamiento de posicion */
static int s_zoom_target_256 = CF_ZOOM_SCALE_NORMAL;
static int s_zoom_from_256 = CF_ZOOM_SCALE_NORMAL;
static long s_zoom_since = 0;
static int s_zoom_ease_in = 0; /* 1 mientras va hacia SHRUNK, 0 hacia NORMAL */

/* Escala interpolada actual de la caratula central, 256=tamano normal.
 * Cuando from==target (caso comun: reposo total, nunca hubo scroll o
 * ya termino de asentarse) devuelve el valor exacto sin pasar por
 * ninguna curva -- evita que un current_tick=0 inicial o un duration
 * agotado introduzcan un redondeo que deje la caratula perceptiblemente
 * distinta de 256 en reposo. */
static int zoom_scale_256(void)
{
    long elapsed_ms;
    int t;

    if (s_zoom_from_256 == s_zoom_target_256)
        return s_zoom_target_256;

    elapsed_ms = (current_tick - s_zoom_since) * 1000L / HZ;
    t = s_zoom_ease_in ? aura_motion_ease_in(elapsed_ms, CF_ZOOM_OUT_MS)
                        : aura_motion_ease_out(elapsed_ms, CF_ZOOM_IN_MS);
    return aura_pattern_lerp(s_zoom_from_256, s_zoom_target_256, t);
}

/* Distancia de camara (canal `slide.distance` de aura_flow, el mismo
 * que ya usa el crecimiento 130->200px del flip) equivalente a la
 * escala interpolada actual -- formula inversa de la ya documentada en
 * CF_GROW_DISTANCE (escala = CAM_DIST/(CAM_DIST+d) => d =
 * CAM_DIST*(256-escala)/escala). Con escala==256 (reposo) da
 * exactamente 0, igual que el `slide.distance = 0` que reemplaza.
 *
 * D-246 (encargo del dueno del producto: "no solo la tapa central,
 * todas junto con ella, para dar sensacion de rapidez fluida"): este
 * MISMO valor se le pasa por igual a TODAS las tapas visibles del
 * carrusel (ver aura_coverflow_draw() y draw_slide_perspective()) --
 * ya no se atenua hacia 0 segun que tan lateral sea cada una. */
static int zoom_distance(void)
{
    int scale = zoom_scale_256();
    if (scale >= CF_ZOOM_SCALE_NORMAL)
        return 0;
    return AURA_FLOW_CAM_DIST * (CF_ZOOM_SCALE_NORMAL - scale) / scale;
}

/* Para pending()/animating(): mientras la escala todavia no alcanzo su
 * target (zoom-out en curso, o zoom-in de vuelta a la normal en curso
 * DESPUES de que la posicion ya se asento) hacen falta mas cuadros, o
 * el zoom-in de cierre se congela a medio camino en cuanto la posicion
 * deja de exigir refresco por su cuenta. */
static int zoom_animating(void)
{
    return zoom_scale_256() != s_zoom_target_256;
}

/* Dispara el zoom-out (ease-in hacia SHRUNK) desde un paso de scroll
 * real -- llamar solo cuando el indice objetivo de verdad cambio (no
 * en un no-op de borde). Si ya esta encogida o encogiendose (rafaga de
 * pasos), no reinicia nada -- ver comentario de arriba. */
static void zoom_trigger_scroll_start(void)
{
    if (s_zoom_target_256 == CF_ZOOM_SCALE_SHRUNK)
        return;
    s_zoom_from_256 = zoom_scale_256();
    s_zoom_target_256 = CF_ZOOM_SCALE_SHRUNK;
    s_zoom_since = current_tick;
    s_zoom_ease_in = 1;
}

/* Dispara el zoom-in (ease-out hacia NORMAL) cuando la posicion ya se
 * asento. Idempotente igual que la anterior. */
static void zoom_trigger_settle(void)
{
    if (s_zoom_target_256 == CF_ZOOM_SCALE_NORMAL)
        return;
    s_zoom_from_256 = zoom_scale_256();
    s_zoom_target_256 = CF_ZOOM_SCALE_NORMAL;
    s_zoom_since = current_tick;
    s_zoom_ease_in = 0;
}

/* Corta cualquier animacion de zoom en curso y fuerza reposo instantaneo
 * -- para los mismos puntos donde la posicion tambien se fuerza a
 * reposo instantaneo (entrar a una lista distinta, volver de una
 * transicion), donde arrastrar una escala a medio encoger no tendria
 * sentido. */
static void zoom_settle_idle(void)
{
    s_zoom_target_256 = CF_ZOOM_SCALE_NORMAL;
    s_zoom_from_256 = CF_ZOOM_SCALE_NORMAL;
}

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

/* Solo la POSICION -- para el gate de SELECT (ver mas abajo), que
 * necesita saber si la tapa objetivo ya esta en su lugar, no si el
 * zoom (D-245) sigue interpolando de vuelta a tamano normal. Antes de
 * D-245 esto era exactamente lo que hacia aura_coverflow_pending(); al
 * sumarle zoom_animating() para la cadencia de render, el gate de
 * SELECT habria heredado ~220ms extra de espera injustificada cada vez
 * que se selecciona justo al terminar de scrollear. */
static int position_pending(void)
{
    return anim_pos_x256() != s_target_index * 256
        || s_state == CF_STATE_COVER_IN || s_state == CF_STATE_COVER_OUT;
}

int aura_coverflow_pending(void)
{
    if (s_state == CF_STATE_SHOW_TRACKS)
    {
        /* Marquee de la cabecera: mientras desborde, el ciclo completo
         * necesita cuadros con cadencia gruesa (para cruzar la
         * frontera estatico->movimiento a tiempo, D-091). */
        if (s_header_overflowing)
            return 1;

        /* Ventana del Fade-on-Idle del ScrollIndicator del reverso:
         * pedir cuadros mientras el fundido (entrada + persistencia +
         * salida) siga corriendo, o el pulgar se congela a medio
         * aparecer. */
        if (s_track_count > CF_BACK_VISIBLE_ROWS)
        {
            long idle_ms = (current_tick - s_track_activity_since) * 1000L / HZ;
            long window_ms = AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS
                            + AURA_DS_METRICS_SCROLL_INDICATOR_IDLE_BEFORE_FADE_MS
                            + AURA_DS_METRICS_SCROLL_INDICATOR_FADE_DURATION_MS;
            if (idle_ms < window_ms)
                return 1;
        }
    }

    return anim_pos_x256() != s_target_index * 256
        || zoom_animating()
        || s_state == CF_STATE_COVER_IN || s_state == CF_STATE_COVER_OUT;
}

int aura_coverflow_animating(void)
{
    /* Cadencia fina solo cuando los pixeles se mueven de verdad: el
     * carrusel/flip, el zoom de la caratula central (D-245), o el
     * tramo de MOVIMIENTO del marquee de la cabecera -- no durante su
     * tramo estatico ni la persistencia del ScrollIndicator (esos van
     * por pending() a cadencia gruesa). */
    if (s_state == CF_STATE_SHOW_TRACKS)
        return s_header_overflowing && marquee_phase_scrolling(s_header_since);

    return anim_pos_x256() != s_target_index * 256
        || zoom_animating()
        || s_state == CF_STATE_COVER_IN || s_state == CF_STATE_COVER_OUT;
}

static void ensure_albums(aura_screen_id_t screen)
{
    int gen = aura_music_filter_generation();
    int i;

    int same_list;

    if (s_cache_screen == screen && s_cache_generation == gen)
        return;

    /* El browse de Cover Flow (tag_album, sin filtros) NO depende de la
     * generacion de filtros: la generacion sube, entre otras cosas,
     * cuando ESTE MISMO componente llama a aura_music_select_album() al
     * abrir la lista de canciones de una tapa. Resetear la posicion en
     * ese caso era el bug reportado por el dueno del diseno
     * (2026-08-12): "el estado del resto de caratulas se debe mantener,
     * no regresar al inicio del coverflow". La posicion/animacion solo
     * se reinicia al entrar desde OTRA pantalla cacheada distinta (o si
     * la lista encogio por debajo del indice actual). */
    same_list = (s_cache_screen == screen);

    s_cache_screen = screen;
    s_cache_generation = gen;
    s_album_count = aura_music_browse(screen, s_albums, AURA_MUSIC_MAX_ITEMS);

    if (!same_list || s_target_index >= s_album_count)
    {
        s_target_index = 0;
        s_anim_from_x256 = 0;
        s_anim_since = current_tick;
        zoom_settle_idle();
    }

    for (i = 0; i < CF_CACHE_SLOTS; i++)
        s_slots[i].album_index = -1;
}

static cf_slot_t *get_slot_for(int album_index)
{
    static int s_slots_theme = -1;
    int i, target, free_slot = -1, farthest = -1, farthest_dist = -1;

    /* Tema nuevo = esquinas horneadas contra el fondo viejo en TODOS
     * los slots en RAM (correccion 2026-08-12): se tiran completos y
     * se recargan bajo demanda (el .pfraw en disco tambien se
     * auto-invalida por tema, ver aura_albumart.c). */
    if (s_slots_theme != (int)aura_settings.theme)
    {
        s_slots_theme = (int)aura_settings.theme;
        for (i = 0; i < CF_CACHE_SLOTS; i++)
            s_slots[i].album_index = -1;
    }

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

/* D-219 (encargo del dueno, 2026-08-14: coverflow "deberia ser mas
 * rapido y fluido"): `draw_slide_perspective()` llama esto una vez POR
 * PIXEL de CADA columna de CADA tapa lateral visible -- con
 * CF_COVER_SIZE=130 y hasta ~8 laterales en pantalla, son decenas de
 * miles de llamadas por cuadro mientras el carrusel se desliza, y cada
 * una hacia TRES divisiones enteras de 8 bits (una por canal) -- el
 * ARM926EJ-S del iPod no tiene divisor de hardware, cada division
 * entera cuesta una rutina de biblioteca de docenas de ciclos.
 * `fade`/`bg_r`/`bg_g`/`bg_b` son CONSTANTES durante todo el dibujo de
 * una tapa (se calculan una sola vez en draw_slide_perspective(), no
 * cambian columna a columna) -- eso significa que "blend hacia bg" es
 * una funcion PURA de un solo byte de entrada (el canal 0-255), la
 * misma para los ~16000 pixeles de la tapa. Precalcularla una vez por
 * tapa (256 divisiones) y volverla una lectura de tabla por pixel es
 * la misma idea que ya usa aura_art.c para el reflejo ("atenuacion
 * precalculada por fila, sin gradiente en tiempo real", ver
 * aura_flow.h) -- aca se aplica al blend de perspectiva. */
static void build_fade_lut(unsigned char *lut, int fade, int bg_channel)
{
    int v;
    for (v = 0; v < 256; v++)
        lut[v] = bg_channel + ((v - bg_channel) * fade) / 255;
}

static fb_data lut_pixel(unsigned px, const unsigned char *lut_r,
                          const unsigned char *lut_g, const unsigned char *lut_b)
{
    return LCD_RGBPACK(lut_r[RGB_UNPACK_RED(px)],
                        lut_g[RGB_UNPACK_GREEN(px)],
                        lut_b[RGB_UNPACK_BLUE(px)]);
}

/* D-240 (sesion 2026-08-14, "todavia se puede optimizar mas"):
 * `fade` (ver draw_slide_perspective) solo es CONTINUO mientras una
 * tapa esta a menos de un paso de indice del centro animado
 * (t_center<256) -- aura_pattern_lerp() satura a CF_SIDE_FADE exacto
 * en cuanto t_center llega a 256 (progress_256>=256 => return b, ver
 * aura_patterns.c). Con CF_VISIBLE_RADIUS=3 se dibujan hasta 8
 * laterales por cuadro; EN REPOSO las 8 estan en fade==CF_SIDE_FADE
 * fijo (ninguna esta "a medio paso" del centro), y DURANTE el scroll
 * a lo sumo 2 (las vecinas inmediatas del indice animado) tienen un
 * fade realmente en transicion -- las otras 6+ siguen fijas en
 * CF_SIDE_FADE. build_fade_lut() (256*3 divisiones enteras, sin
 * divisor de hardware en el ARM926EJ-S) se reconstruia identica para
 * cada una de esas tapas "lejanas", cada cuadro: una sola tabla
 * compartida, invalidada solo cuando cambia el color de fondo (tema),
 * cubre todos esos casos sin alterar un solo pixel de salida (mismo
 * build_fade_lut(), mismo fade). Las tapas realmente en transicion
 * siguen construyendo su propia tabla en draw_slide_perspective(): su
 * valor de fade es distinto cuadro a cuadro, no se puede compartir. */
static void get_far_fade_lut(int bg_r, int bg_g, int bg_b,
                              const unsigned char **out_r,
                              const unsigned char **out_g,
                              const unsigned char **out_b)
{
    static unsigned char lut_r[256], lut_g[256], lut_b[256];
    static int cached_bg_r = -1, cached_bg_g = -1, cached_bg_b = -1;

    if (bg_r != cached_bg_r || bg_g != cached_bg_g || bg_b != cached_bg_b)
    {
        build_fade_lut(lut_r, CF_SIDE_FADE, bg_r);
        build_fade_lut(lut_g, CF_SIDE_FADE, bg_g);
        build_fade_lut(lut_b, CF_SIDE_FADE, bg_b);
        cached_bg_r = bg_r;
        cached_bg_g = bg_g;
        cached_bg_b = bg_b;
    }

    *out_r = lut_r;
    *out_g = lut_g;
    *out_b = lut_b;
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
 * vez de saltar.
 *
 * `zoom_dist` (D-245/D-246) es el `slide.distance` que produce el zoom
 * de scroll -- calculado UNA vez por cuadro en aura_coverflow_draw()
 * (ver zoom_distance()), no por tapa, para no repetir su division en
 * cada una de las ~9 tapas visibles. A diferencia de angle/cx/fade
 * (que SI se atenuan con t_center segun que tan lateral es cada tapa,
 * para la coreografia normal de perspectiva), este valor se aplica TAL
 * CUAL a todas las tapas por igual -- D-246: el dueno del producto
 * pidio explicitamente que todo el carrusel se encoja junto, no solo
 * la central, para que la sensacion sea de velocidad fluida del
 * conjunto en vez de un efecto aislado en una sola tapa. */
static void draw_slide_perspective(const cf_slot_t *slot, int offset_x256,
                                    int zoom_dist)
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
    /* D-240: CF_COVER_SIZE/CF_REFLECTION_PCT son macros (constantes de
     * compilacion) -- aura_art_reflection_height(CF_COVER_SIZE,
     * CF_REFLECTION_PCT) siempre devolvia el mismo valor que
     * CF_REFLECTION_H (definido arriba con la MISMA formula, para
     * dimensionar cover_buf/reflection_buf) pero via una llamada real a
     * otra unidad de compilacion, una vez por tapa visible por cuadro.
     * Usar la macro ya existente evita esa llamada+division redundante. */
    int refl_h = CF_REFLECTION_H;
    int total_h = CF_COVER_SIZE + refl_h;
    unsigned bg = a26_color(A26_SHELL_BG);
    int bg_r = RGB_UNPACK_RED(bg);
    int bg_g = RGB_UNPACK_GREEN(bg);
    int bg_b = RGB_UNPACK_BLUE(bg);
    static fb_data col_buf[CF_COVER_SIZE + CF_REFLECTION_H];
    /* D-219: tablas de blend, ver build_fade_lut() -- una sola vez por
     * tapa, no una vez por pixel. D-240: usadas solo para el fade EN
     * TRANSICION (ver get_far_fade_lut() arriba); use_lut_* apunta a
     * estas o a la tabla compartida segun el caso. */
    static unsigned char lut_r[256], lut_g[256], lut_b[256];
    const unsigned char *use_lut_r = NULL, *use_lut_g = NULL, *use_lut_b = NULL;

    if (extra_x256 < 0)
        extra_x256 = 0;

    /* Mismo signo que pictureflow.c reset_slides(): la lateral izquierda
     * (offset<0) angulo positivo y cx negativo, la derecha al reves --
     * la carga muestra su borde hacia el centro de la pantalla. En
     * offset==0 (sign==0) todo colapsa a angulo/cx cero, sin distorsion,
     * igual que antes. */
    slide.angle = -sign * aura_pattern_lerp(0, CF_ITILT, t_center);
    slide.distance = zoom_dist;
    slide.cx = sign * (aura_pattern_lerp(0, CF_OFFSETX_R, t_center)
                        + (int)((long)CF_SLIDE_SPACING_R * extra_x256 / 256));

    fade = aura_pattern_lerp(255, CF_SIDE_FADE, t_center);
    if (fade < 255)
    {
        if (fade == CF_SIDE_FADE)
        {
            /* Caso comun (ver comentario de get_far_fade_lut()): tabla
             * compartida, sin reconstruir. */
            get_far_fade_lut(bg_r, bg_g, bg_b, &use_lut_r, &use_lut_g, &use_lut_b);
        }
        else
        {
            /* Tapa realmente en transicion (t_center<256): su fade es
             * unico para este cuadro, no se puede compartir. */
            build_fade_lut(lut_r, fade, bg_r);
            build_fade_lut(lut_g, fade, bg_g);
            build_fade_lut(lut_b, fade, bg_b);
            use_lut_r = lut_r;
            use_lut_g = lut_g;
            use_lut_b = lut_b;
        }
    }

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
            col_buf[dest_row] = (fade < 255) ? lut_pixel(px, use_lut_r, use_lut_g, use_lut_b) : px;
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
    const fb_data *refl = (const fb_data *)slot->art.reflection_data;
    static fb_data col_buf[CF_BACK_SIZE + 8];
    static fb_data refl_buf[CF_REFLECTION_H * CF_BACK_SIZE / CF_COVER_SIZE + 8];
    /* Transicion del reflejo (encargo del dueno del diseno,
     * 2026-08-12: "que no aparezca y desaparezca repentinamente...
     * que gire a la vez que se desvanece y desaparece hacia abajo...
     * ademas de salir y voltearse, deberia agrandarse en proporcion"):
     * el reflejo se proyecta CON el giro (misma columna/escala que la
     * tapa, asi el zoom de abajo lo agranda en proporcion exacta), y
     * su visibilidad va atada al angulo -- a 0 grados esta completo,
     * hacia 90 se funde al fondo y se desliza hacia abajo. COVER_OUT
     * pasa el angulo invertido, asi que la inversa sale gratis. */
    int refl_vis = 256 - iangle_0_to_256;
    int refl_drop = ((256 - refl_vis) * 64) / 256;
    /* Centro vertical: viaja del centro del carrusel al centro del
     * reverso crecido, atado al mismo angulo que el zoom. */
    int center_y = aura_pattern_lerp(CF_TOP_Y + CF_COVER_SIZE / 2,
                                      CF_BACK_Y + CF_BACK_SIZE / 2,
                                      iangle_0_to_256);
    unsigned bg = a26_color(A26_SHELL_BG);

    slide.angle = iangle_0_to_256;
    /* Zoom continuo 130 -> 200 durante la rotacion (y de vuelta en
     * COVER_OUT, que pasa el angulo invertido). */
    slide.distance = (CF_GROW_DISTANCE * iangle_0_to_256) / 256;
    slide.cx = 0;

    aura_flow_begin_projection(&proj, &slide, CF_COVER_SIZE);

    while (proj.screen_x < AURA_FLOW_SCREEN_W)
    {
        int col = aura_flow_source_column(&proj);
        int dy = aura_flow_vertical_scale(&proj);
        int p = 0, dest_row, n_rows = 0;
        const fb_data *cover_col = cover + (size_t)col * CF_COVER_SIZE;
        const fb_data *refl_col = refl + (size_t)col * CF_REFLECTION_H;

        int cover_disp = (CF_COVER_SIZE << AURA_FLOW_SHIFT) / dy;
        int y_col = center_y - cover_disp / 2;

        for (dest_row = 0; dest_row < (int)(sizeof(col_buf) / sizeof(col_buf[0])); dest_row++)
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

        if (refl_vis > 0)
        {
            int r_rows = 0;

            p = 0;
            for (dest_row = 0; dest_row < (int)(sizeof(refl_buf) / sizeof(refl_buf[0])); dest_row++)
            {
                int source_row = p >> AURA_FLOW_SHIFT;

                if (source_row >= CF_REFLECTION_H)
                    break;
                refl_buf[dest_row] = a26_shell_blend(bg, refl_col[source_row], refl_vis);
                p += dy;
                r_rows++;
            }
            if (r_rows > 0)
                lcd_bitmap(refl_buf, proj.screen_x,
                           y_col + n_rows + refl_drop, 1, r_rows);
        }

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
#define CF_TRACK_ROW_H 20 /* filas mas espaciadas -> pastilla mas gruesa (encargo 2026-08-12) */
/* El texto de la lista vive 10px MAS adentro que el padding (encargo
 * 2026-08-12: "a 10px mas alejado de los bordes, para que el selector
 * pueda respetar el padding interno") -- la pastilla usa los 4px de
 * padding, el texto usa padding+10. */
#define CF_TRACK_TEXT_INSET (CF_BACK_PADDING + 10)

/* Artista del album objetivo, cacheado por seleccion (lookup de
 * tagcache solo al cambiar de album, nunca por cuadro) -- compartido
 * entre las dos lineas del carrusel y la cabecera del reverso. */
static int s_artist_for_index = -1;
static char s_artist[64];

static const char *target_artist(void)
{
    if (s_artist_for_index != s_target_index && s_album_count > 0)
    {
        s_artist_for_index = s_target_index;
        aura_music_album_artist(s_albums[s_target_index].seek,
                                 s_artist, sizeof(s_artist));
    }
    return s_artist;
}

static void draw_tracklist_panel(void)
{
    int panel_x = CF_BACK_X;
    int panel_y = CF_BACK_Y;
    int pad = CF_BACK_PADDING;
    int list_y = panel_y + CF_BACK_HEADER_H + 1; /* +1: el separador */
    int visible = CF_BACK_VISIBLE_ROWS;
    int first, i, w, h;
    unsigned bg = a26_color(A26_SHELL_BG);
    /* "Un poco menos blanco" que el fondo, para que el reverso se note
     * (encargo 2026-08-12, #F4F4F4): derivado como mezcla de tokens --
     * blend(SHELL_BG, TEXT_PRIMARY, 11/256) da EXACTO 0xF4F4F4 en el
     * tema claro, y en el oscuro produce el mismo "apenas distinto del
     * fondo" sin inventar un segundo valor. Ningun RGB suelto en C
     * (regla dura del proyecto). */
    unsigned panel_bg = a26_shell_blend(bg, a26_color(A26_TEXT_PRIMARY), 11);
    /* D-240: "Album - Artista" solo cambia de CONTENIDO cuando cambia
     * el album objetivo -- antes se reformateaba con snprintf() en
     * CADA cuadro que este panel estaba visible (hasta HZ/20 = 20
     * veces por segundo mientras el marquee desborda o el
     * ScrollIndicator sigue en su ventana de Fade-on-Idle, ver
     * aura_coverflow_pending()), aunque el texto resultante fuera
     * BYTE POR BYTE identico cuadro a cuadro. static + gate detras del
     * mismo chequeo `s_header_for_index != s_target_index` que ya
     * existia para resetear el reloj del marquee -- se reformatea una
     * sola vez por cambio de album, se reusa el resto de los cuadros. */
    static char s_header_cache[160];
    const char *artist = target_artist();
    long header_elapsed_ms;

    a26_shell_fill_rounded_rect(panel_x, panel_y, CF_BACK_SIZE, CF_BACK_SIZE,
                                 CF_CORNER_RADIUS, panel_bg, bg);

    /* Cabecera "Album - Artista" con Marquee Loop cuando no cabe
     * (marquee-text.md: 2s estatico + 5s de loop continuo) -- mismo
     * componente que StatusBar/SelectionSummary, nunca un corte seco. */
    if (s_header_for_index != s_target_index)
    {
        s_header_for_index = s_target_index;
        s_header_since = current_tick;

        if (artist[0])
            snprintf(s_header_cache, sizeof(s_header_cache), "%s - %s",
                      s_albums[s_target_index].label, artist);
        else
            snprintf(s_header_cache, sizeof(s_header_cache), "%s",
                      s_albums[s_target_index].label);
    }
    header_elapsed_ms = (current_tick - s_header_since) * 1000L / HZ;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_10));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_getstringsize((const unsigned char *)s_header_cache, &w, &h);
    if (w <= CF_BACK_SIZE - 2 * pad)
    {
        lcd_putsxy(panel_x + (CF_BACK_SIZE - w) / 2,
                   panel_y + (CF_BACK_HEADER_H - h) / 2,
                   (const unsigned char *)s_header_cache);
        s_header_overflowing = 0;
    }
    else
        s_header_overflowing = aura_marquee_draw(panel_x + pad,
                                                  panel_y + (CF_BACK_HEADER_H - h) / 2,
                                                  CF_BACK_SIZE - 2 * pad,
                                                  s_header_cache, header_elapsed_ms);

    /* Separador cabecera/lista: barra de 1px al ancho del contenido
     * (respeta el padding interno de 4px, misma regla que el divisor
     * de seccion de MenuList). */
    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_hline(panel_x + pad, panel_x + CF_BACK_SIZE - pad - 1,
              panel_y + CF_BACK_HEADER_H);

    if (s_track_count == 0)
        return;

    if (s_track_sel != s_prev_track_sel)
    {
        s_prev_track_sel = s_track_sel;
        s_track_activity_since = current_tick;
    }

    first = s_track_sel - visible / 2;
    if (first < 0)
        first = 0;
    if (first > s_track_count - visible)
        first = s_track_count > visible ? s_track_count - visible : 0;

    /* Pastilla de seleccion GRIS con esquinas redondeadas, ANTES del
     * texto de todas las filas (D-081) -- mismos estilos del menu:
     * pastilla SELECTION_FILL, texto seleccionado en acento, el resto
     * en el color primario. */
    if (s_track_sel >= first && s_track_sel < first + visible)
    {
        int sel_y = list_y + (s_track_sel - first) * CF_TRACK_ROW_H;
        a26_shell_fill_rounded_rect(panel_x + pad, sel_y,
                                     CF_BACK_SIZE - 2 * pad, CF_TRACK_ROW_H,
                                     AURA_DS_METRICS_SELECTOR_CORNER_RADIUS,
                                     a26_color(A26_SELECTION_FILL), panel_bg);
    }

    lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
    for (i = 0; i < visible && first + i < s_track_count; i++)
    {
        int idx = first + i;
        int row_y = list_y + i * CF_TRACK_ROW_H + (CF_TRACK_ROW_H - A26_TYPE_CAPTION) / 2;
        char row[AURA_MUSIC_ITEM_LEN + 8];

        /* "N. Nombre de la cancion" (encargo 2026-08-12) -- numero de
         * posicion en la lista del album. */
        snprintf(row, sizeof(row), "%d. %s", idx + 1, s_tracks[idx].label);

        lcd_set_foreground(idx == s_track_sel ? aura_accent()
                                               : a26_color(A26_TEXT_PRIMARY));
        draw_clipped_text(panel_x + CF_TRACK_TEXT_INSET, row_y,
                           CF_BACK_SIZE - 2 * CF_TRACK_TEXT_INSET, row);
    }

    /* ScrollIndicator (T2.4, Fade-on-Idle) dentro del padding derecho
     * -- fondo blanco del panel, tinta del riel, como en el menu. */
    if (s_track_count > visible)
    {
        long idle_ms = (current_tick - s_track_activity_since) * 1000L / HZ;
        aura_scroll_indicator_draw(panel_x + CF_BACK_SIZE - pad,
                                    list_y, visible * CF_TRACK_ROW_H,
                                    first, s_track_count, visible, idle_ms,
                                    panel_bg, a26_color(A26_SHELL_RAIL));
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
    int pos_x256, center_idx, i, n, zoom_dist;
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

    /* D-245: la posicion del carrusel ya se asento (sin esto, un
     * asentamiento detectado a medio camino de un salto acelerado
     * dispararia el zoom-in de forma prematura, antes de que el
     * carrusel de verdad termine de moverse). */
    if (pos_x256 == s_target_index * 256)
        zoom_trigger_settle();
    zoom_dist = zoom_distance();

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
        draw_slide_perspective(slot, offset_x256, zoom_dist);
    }

    {
        int w, h;
        const char *label = s_albums[s_target_index].label;

        /* Dos lineas centradas bajo el central, como el original de
         * Apple: titulo del album (bold) + artista (regular, un tono
         * abajo). Durante el giro de la tapa se DESLIZAN hacia abajo
         * hasta salir de pantalla (encargo del dueno del diseno,
         * 2026-08-12: ceden su espacio al reverso crecido); ocultos
         * mientras la lista esta abierta, regresan subiendo con el
         * giro de vuelta. 64px de recorrido: suficiente para sacar
         * ambas lineas (la mas alta esta a 58px del borde). */
        const char *artist = target_artist();
        int text_dy = 0;
        int text_hidden = 0;

        if (s_state == CF_STATE_COVER_IN)
            text_dy = 64 * flip_progress_256() / 256;
        else if (s_state == CF_STATE_SHOW_TRACKS)
            text_hidden = 1;
        else if (s_state == CF_STATE_COVER_OUT)
            text_dy = 64 * (256 - flip_progress_256()) / 256;

        if (!text_hidden)
        {
            lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
            lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
            lcd_getstringsize((const unsigned char *)label, &w, &h);
            lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, CF_TITLE_Y + text_dy,
                       (const unsigned char *)label);

            if (artist[0])
            {
                lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
                lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
                lcd_getstringsize((const unsigned char *)artist, &w, &h);
                lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, CF_ARTIST_Y + text_dy,
                           (const unsigned char *)artist);
            }
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
/* Salto rapido por bloque (encargo del dueno del diseno, 2026-08-12):
 * los botones backward/forward desplazan el carrusel 10 albumes de un
 * golpe, con el mismo deslizamiento suave con redireccion del scroll
 * normal (el recorrido mas largo en la misma duracion de animacion se
 * lee como un barrido veloz). Acotado en los extremos, sin loop -- el
 * carrusel nunca salta de inicio a final ni viceversa, igual que el
 * scroll de la rueda. */
#define CF_FAST_JUMP 10

static void jump_albums(int delta)
{
    int new_target = s_target_index + delta;

    if (new_target < 0)
        new_target = 0;
    if (new_target >= s_album_count)
        new_target = s_album_count > 0 ? s_album_count - 1 : 0;
    if (new_target == s_target_index)
        return;

    s_anim_from_x256 = anim_pos_x256();
    s_target_index = new_target;
    s_anim_since = current_tick;
    zoom_trigger_scroll_start();
}

/* PLAY sobre la tapa enfocada (encargo del dueno del diseno,
 * 2026-08-12): reproduce el album COMPLETO al instante, sin navegar al
 * reproductor -- el usuario sigue en el carrusel y puede seguir
 * hojeando mientras suena (el icono de reproduccion de la StatusBar es
 * la confirmacion visual). Si la tapa enfocada ES el album que este
 * mismo carrusel ya puso a sonar, PLAY alterna pausa/reanudar
 * (semantica natural de play/pausa; sin esto no habria forma de pausar
 * dentro del Cover Flow) -- provisional razonable, el encargo solo
 * define el caso "reproducir", TODO(pendiente-doc). */
static int32_t s_playing_album_seek = -1;

static void play_or_toggle_focused_album(void)
{
    int status;

    if (s_album_count <= 0)
        return;

    status = audio_status();
    if (s_playing_album_seek == s_albums[s_target_index].seek
        && (status & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)))
    {
        if (status & AUDIO_STATUS_PAUSE)
            audio_resume();
        else
            audio_pause();
        return;
    }

    aura_music_select_album(s_albums[s_target_index].seek);
    if (aura_music_play_songs(AURA_SCREEN_MUSIC_SONGS_BY_ALBUM, 0))
        s_playing_album_seek = s_albums[s_target_index].seek;
}

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
    if (new_target != s_target_index)
        zoom_trigger_scroll_start(); /* D-245: solo en un paso real, no en el no-op de borde */
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
                aura_transition_flip_and_flow(nav, s_albums[s_target_index].seek);
            break;
        case BUTTON_PLAY:
            /* Mismo comportamiento que en el carrusel: el album
             * enfocado se reproduce completo (o alterna pausa si ya
             * era el que sonaba), sin salir de la lista. */
            play_or_toggle_focused_album();
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
         * veria mal, la tapa objetivo todavia no esta en su lugar.
         * position_pending() (no aura_coverflow_pending()): el zoom de
         * D-245 no debe añadir latencia a SELECT, solo la posicion
         * importa aca -- ver comentario de position_pending(). */
        if (s_album_count > 0 && !position_pending())
        {
            aura_music_select_album(s_albums[s_target_index].seek);
            s_track_count = aura_music_browse(AURA_SCREEN_MUSIC_SONGS_BY_ALBUM,
                                               s_tracks, AURA_MUSIC_MAX_ITEMS);
            s_track_sel = 0;
            s_state = CF_STATE_COVER_IN;
            s_state_since = current_tick;
        }
        break;
    case BUTTON_PLAY:
        play_or_toggle_focused_album();
        break;
    case BUTTON_LEFT:
        jump_albums(-CF_FAST_JUMP);
        break;
    case BUTTON_RIGHT:
        jump_albums(CF_FAST_JUMP);
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

/* -- Coreografia de vuelo (ver aura_coverflow.h) ------------------------ */

/* Laterales con desplazamiento extra hacia su borde: reusa
 * draw_slide_perspective() aumentando |offset| -- mas offset = mas
 * lejos del centro Y mas inclinada, que es exactamente "salir hacia su
 * lado". 7 posiciones extra (~200px) las saca de pantalla. */
#define CF_EXIT_UNITS (7 * 256)

static void draw_carousel_sides(int extra_out_x256)
{
    int d, side;

    /* Orden pintor REAL: distancia decreciente al centro, ambos lados
     * por nivel (correccion 2026-08-12: la aproximacion anterior de
     * "alternar extremos" dibujaba t+3 al FINAL -- encima de t+2 y
     * t+1 -- y t-2 sobre t-1: las caratulas del fondo se veian
     * brevemente encima de las de adelante durante el morph de
     * regreso). */
    for (d = CF_VISIBLE_RADIUS; d >= 1; d--)
    {
        for (side = -1; side <= 1; side += 2)
        {
            int use = s_target_index + side * d;
            int offset;
            cf_slot_t *slot;

            if (use < 0 || use >= s_album_count)
                continue;

            offset = (use - s_target_index) * 256;
            offset += (offset > 0 ? extra_out_x256 : -extra_out_x256);

            slot = get_slot_for(use);
            /* d>=1 siempre: nunca dibuja la tapa central (offset==0)
             * aca, es coreografia de vuelo de entrada/salida -- sin
             * zoom D-245, que solo aplica durante scroll dentro del
             * carrusel normal. */
            draw_slide_perspective(slot, offset, 0);
        }
    }
}

void aura_coverflow_draw_exit_frame(int out_t256)
{
    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_MUSIC_COVERFLOW));
    draw_carousel_sides(out_t256 * CF_EXIT_UNITS / 256);
}

void aura_coverflow_draw_return_frame(int in_t256)
{
    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_MUSIC_COVERFLOW));
    draw_carousel_sides((256 - in_t256) * CF_EXIT_UNITS / 256);
}

void aura_coverflow_draw_return_texts(int in_t256)
{
    int w, h;
    const char *label;
    const char *artist;
    int text_dy = ((256 - in_t256) * 64) / 256;

    if (s_album_count <= 0)
        return;

    /* Titulo/artista entrando desde el borde inferior, mismo recorrido
     * de 64px que usan al ceder espacio en el flip (D-116). Funcion
     * APARTE del fondo a proposito: se dibujan DESPUES de la caratula
     * y su reflejo en cada cuadro del morph de regreso -- el mismo
     * orden de capas que el carrusel en reposo (texto ENCIMA del
     * reflejo); dibujarlos antes los dejaba tapados por el reflejo
     * durante la transicion y "saltaban" encima al terminar (error
     * visual reportado por el dueno del diseno, 2026-08-12). */
    label = s_albums[s_target_index].label;
    artist = target_artist();

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_getstringsize((const unsigned char *)label, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, CF_TITLE_Y + text_dy,
               (const unsigned char *)label);

    if (artist[0])
    {
        lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)artist, &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, CF_ARTIST_Y + text_dy,
                   (const unsigned char *)artist);
    }
}

int32_t aura_coverflow_current_album_seek(void)
{
    if (s_album_count <= 0)
        return -1;
    return s_albums[s_target_index].seek;
}

void aura_coverflow_settle_idle(void)
{
    s_state = CF_STATE_IDLE;
    s_anim_from_x256 = s_target_index * 256;
    s_anim_since = current_tick;
    zoom_settle_idle();
}
