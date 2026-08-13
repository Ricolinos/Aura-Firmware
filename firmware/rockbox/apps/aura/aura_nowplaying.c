#include <string.h>
#include <stdio.h>

/* config.h primero: recorder/albumart.h chequea "#if HAVE_ALBUMART" antes
 * de incluirlo el mismo. Ver el comentario equivalente en aura_music.c
 * (D-021) -- lcd.h ya lo arrastra, pero se deja explicito. */
#include "config.h"
#include "lcd.h"
#include "font.h"
#include "button.h"
#include "audio.h"
#include "file.h"
#include "string-extra.h"
#include "recorder/albumart.h"
#include "recorder/bmp.h"
#include "recorder/jpeg_load.h"
#include "misc.h"
#include "sound.h"
#include "settings.h"
#include "playlist.h"
#include "status.h"
#include "tick.h"
#include "tagcache.h"

#include "aura_nowplaying.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "aura_lrc.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_status_bar_v2.h"
#include "aura_widgets.h"
#include "aura_art.h"
#include "aura_albumart.h"
#include "aura_motion.h"
#include "aura_music.h"
#include "aura_main.h"
#include "aura_flow.h"
#include "aura_patterns.h"
#include "kernel.h"

/* -- Layout (PLAN.md T3.1, componentes/now-playing.md) -------------------
 * Geometria de la caratula RESCATADA del original (doc: "se rescata la
 * geometria, tamano y posicion... del Now Playing original del iPod
 * Classic"), valores "aproximados leidos de captura, a validar" segun
 * el propio documento -- se usan tal cual, es la unica fuente que existe
 * hoy. El angulo (7 grados) SI esta "fijado formalmente", a diferencia
 * de x/y/tamano que siguen marcados como estimados. */
#define ART_SIZE     AURA_DS_METRICS_NOW_PLAYING_COVER_SIZE
#define ART_X        AURA_DS_METRICS_NOW_PLAYING_COVER_X
#define ART_Y        AURA_DS_METRICS_NOW_PLAYING_COVER_Y
#define ART_RADIUS   AURA_DS_METRICS_COVER_FLOW_CORNER_RADIUS
#define ART_REFLECTION_PCT AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT
#define REFL_GAP     2

#define TEXT_X     (ART_X + ART_SIZE + A26_SPACING_LG)
#define TEXT_W     (A26_SCREEN_WIDTH - TEXT_X - A26_SPACING_LG)

#define STAR_COUNT 5

/* La barra sube (encargo 2026-08-12): los elementos transitorios
 * (tiempos de busqueda, bocinas -/+ de volumen) viven DEBAJO de ella. */
#define PROGRESS_Y       (A26_SCREEN_HEIGHT - AURA_DS_METRICS_NOW_PLAYING_PROGRESS_BOTTOM_OFFSET)
#define PROGRESS_TRACK_H AURA_DS_METRICS_NOW_PLAYING_PROGRESS_TRACK_HEIGHT
#define PROGRESS_FILL_H  AURA_DS_METRICS_NOW_PLAYING_PROGRESS_FILL_HEIGHT
/* Barra unificada progreso/volumen (encargo 2026-08-12): carril de
 * 300x7 centrado, relleno de 298x5 (1px de aire por lado), puntas
 * completamente redondeadas. Sin numeros de tiempo en reposo. */
#define PROGRESS_W       AURA_DS_METRICS_NOW_PLAYING_PROGRESS_WIDTH
#define PROGRESS_X       ((A26_SCREEN_WIDTH - PROGRESS_W) / 2)
/* Fila de transporte (encargo 2026-08-12): centrada verticalmente
 * entre la base de la barra y el borde inferior. Play/pause de 24px al
 * centro (al ajustar volumen, el mismo centro lo ocupa la bocina
 * dinamica); repetir (izq.) y aleatorio (der.) de 16px a los costados,
 * a 20px del bloque central -- ya no en los extremos de la barra. */
#define TRANSPORT_CENTER_Y ((PROGRESS_Y + PROGRESS_TRACK_H + A26_SCREEN_HEIGHT) / 2)
#define TRANSPORT_Y        (TRANSPORT_CENTER_Y - A26_ICON_SIZE_TRANSPORT / 2)
#define TRANSPORT_GAP      AURA_DS_METRICS_NOW_PLAYING_TRANSPORT_ICON_GAP

/* Modo 4 (letras, encargo 2026-08-12, correccion del mismo dia): panel
 * izquierdo de 130x240 a pantalla completa con el FONDO NORMAL; el
 * color promedio de la caratula pinta el panel DERECHO (letras). La
 * caratula pasa a un cuadrado perfecto mas pequeno con sombra
 * paralela, centrado verticalmente en el espacio sobre la fila de
 * modos; barra, transporte y fila de modos se reacomodan al panel via
 * el MISMO morph parametrizado por t256 (0 = layout normal, 256 =
 * panel) que anima la entrada/salida del modo. */
#define M4_ART_SIZE   106
#define M4_ART_X      ((MORPH_PANEL_W - M4_ART_SIZE) / 2)
/* Centro vertical del area util del panel (del borde superior al tope
 * de la fila de modos): la fila de controles de abajo no se invade. */
#define M4_ART_Y      (((PROGRESS_Y - 10 - A26_ICON_SIZE_MENU) - M4_ART_SIZE) / 2)
#define M4_ART_SHADOW_DY 4
#define M4_MORPH_MS   330 /* fundido lineal de contenido del sistema */
/* Panel derecho del Modo 4 (correccion 2026-08-12): una HOJA que se
 * desliza desde la derecha (no un fade), de vidrio traslucido tenido
 * con el color promedio de la caratula en degradado DIAGONAL, y que
 * proyecta una sombra paralela sobre lo que queda debajo de su borde
 * izquierdo. Excepcion deliberada del dueno del diseno a "el vidrio
 * vive solo en la capa de controles": pedido explicito ("si se puede,
 * mejor un efecto traslucido"). */
#define M4_RPANEL_W       (A26_SCREEN_WIDTH - MORPH_PANEL_W)
#define M4_GLASS_ALPHA    216 /* ~85% tinte, ~15% deja ver lo de abajo */
#define M4_PANEL_SHADOW_W 8

/* Ventanas de estado de la barra (ver draw_progress): el avance con
 * la rueda (modo 2) muestra tiempos + acento + indicador; el ajuste de
 * volumen la convierte en barra de nivel con bocinas; ambos vuelven
 * solos al reposo. El "fade muy sutil" de la bocina vive en el ultimo
 * tramo de la ventana de volumen. */
#define SEEK_SHOW_TICKS   (HZ / 2)
#define VOLUME_FADE_TICKS (HZ / 3)
static long s_seek_show_until = 0;
/* Tap vs mantener en LEFT/RIGHT (encargo 2026-08-12, corregido el
 * mismo dia: mantener ya NO busca dentro de la cancion -- eso vive en
 * la rueda del modo avance). Tap = pista anterior/siguiente (decision
 * al soltar, como el iPod original). MANTENER, en cualquier modo:
 * FORWARD alterna aleatorio; BACKWARD cicla repetir (todo -> una ->
 * apagado). Una sola conmutacion por pulsacion sostenida. */
#define LR_TAP_WINDOW     (HZ * 35 / 100)
static long s_lr_pending_until = 0;
static int  s_lr_pending_dir = 0;
static bool s_lr_hold_fired = false;
static long s_scrub_show_until = 0; /* ventana del indicador del modo scrub */
/* Busqueda con audio SIMULTANEO (correccion 2026-08-12): aplicar
 * audio_ff_rewind() en CADA repeat (~10/s) reiniciaba la busqueda del
 * motor sin darle tiempo a sonar -- el audio solo saltaba al soltar.
 * El preview visual avanza con cada repeat, y el salto real de audio
 * se aplica como maximo cada AUDIO_SEEK_APPLY_TICKS: entre saltos se
 * ESCUCHA la cancion desde la posicion nueva, como el seek del iPod
 * real. Al expirar la ventana se aplica la posicion final exacta. */
#define AUDIO_SEEK_APPLY_TICKS (HZ / 4)
static long s_seek_last_apply = 0;
static long s_seek_applied_ms = -1;
/* Conmutadores de modos de reproduccion (encargo 2026-08-12: mantener
 * FORWARD/BACKWARD). Mismos ajustes reales de Rockbox que las filas de
 * Ajustes (D-021), pero aqui con efecto INMEDIATO sobre la
 * reproduccion en curso, como el nucleo de Rockbox en onplay:
 * aleatorio re-baraja (o re-ordena) el playlist vivo manteniendo la
 * pista actual; repetir recarga la cola de pistas ya bufereadas. */
static void np_toggle_shuffle(void)
{
    global_settings.playlist_shuffle = !global_settings.playlist_shuffle;
    settings_save();
    if (audio_status() & AUDIO_STATUS_PLAY)
    {
        if (global_settings.playlist_shuffle)
            playlist_randomise(NULL, current_tick, true);
        else
            playlist_sort(NULL, true);
    }
}

static void np_cycle_repeat(void)
{
    if (global_settings.repeat_mode == REPEAT_OFF)
        global_settings.repeat_mode = REPEAT_ALL;
    else if (global_settings.repeat_mode == REPEAT_ALL)
        global_settings.repeat_mode = REPEAT_ONE;
    else
        global_settings.repeat_mode = REPEAT_OFF;
    settings_save();
    if (audio_status() & AUDIO_STATUS_PLAY)
        audio_flush_and_reload_tracks();
}

/* Nunca mas de UN salto de audio en vuelo (correccion 2026-08-12: "el
 * audio sigue quedandose atras"): cada audio_ff_rewind() implica
 * seek + rebuffer en el motor, y si uno tarda mas que el estrangulado
 * los saltos se APILAN en su cola -- al soltar, el motor seguia
 * procesando saltos viejos antes del final. Un salto intermedio solo
 * se emite cuando el elapsed real ya alcanzo el salto anterior
 * (motor listo); si el motor va lento, los intermedios se omiten (el
 * visual avanza igual) y el salto final de la ventana aterriza sin
 * cola por delante. */
static bool seek_engine_ready(const struct mp3entry *id3)
{
    long d;

    if (s_seek_applied_ms < 0)
        return true;
    d = (long)id3->elapsed - s_seek_applied_ms;
    if (d < 0)
        d = -d;
    return d < 2000;
}

/* -- Modo 4 (Letras), panel comprimido (PLAN.md T3.1(c),
 * componentes/now-playing.md) --------------------------------------------
 * "Panel izquierdo delgado de 130px" (el token se llama
 * lyrics_panel_width en tokens.json, pero mide el panel del REPRODUCTOR
 * comprimido, no el LyricsPanel -- 130-122=8=2*4px de padding interno,
 * el mismo patron que el resto del sistema). LyricsPanel ocupa el resto
 * de la pantalla (190px) a la derecha. */
#define MORPH_PANEL_W    AURA_DS_METRICS_NOW_PLAYING_LYRICS_PANEL_WIDTH
#define MORPH_PROGRESS_W AURA_DS_METRICS_NOW_PLAYING_LYRICS_PROGRESS_WIDTH
#define MORPH_PROGRESS_X ((MORPH_PANEL_W - MORPH_PROGRESS_W) / 2)
#define LYRICS_PANEL_X   MORPH_PANEL_W
#define LYRICS_PANEL_W   (A26_SCREEN_WIDTH - MORPH_PANEL_W)

#define LRC_FILE_BUF_SIZE 8192

/* Overlay de volumen (Fase 17, PLAN-UX.md, wheel=volumen): visible
 * ~1.5s despues del ultimo scroll. */
#define VOLUME_OVERLAY_TICKS (HZ + HZ / 2)
static long s_volume_overlay_until = 0;

static unsigned char s_art_buf[64 * 1024];
static struct bitmap s_art_bm;
static bool s_art_valid = false;
static unsigned char s_reflection_buf[ART_SIZE * (ART_SIZE * ART_REFLECTION_PCT / 100) * sizeof(fb_data)];

static aura_lrc_t s_lrc;
static bool s_lrc_valid = false;

static char s_loaded_path[MAX_PATH];

/* Color del panel del Modo 4: el PROMEDIO de la caratula en curso, con
 * un degradado ligero (mas claro arriba, mas oscuro abajo) y una tinta
 * legible decidida por luminancia -- todo derivado en runtime de la
 * imagen real, nunca un RGB fijo. */
static bool s_panel_colors_valid = false;
static unsigned s_panel_top = 0, s_panel_bot = 0, s_panel_ink = 0;

/* Salida de pantalla completa desde el Modo 4 (ver BUTTON_MENU):
 * aura_screens la consume para elegir el slide en vez del morph al
 * carrusel. */
static bool s_exit_fullscreen = false;

bool aura_nowplaying_take_fullscreen_exit(void)
{
    bool v = s_exit_fullscreen;
    s_exit_fullscreen = false;
    return v;
}

static void mode4_morph(int dir); /* definida junto a draw_player */

/* Regreso Modo 4 -> coverflow (correccion 2026-08-12: "si accedimos
 * por coverflow, SI podemos regresar al coverflow"): primero el
 * despliegue inverso del panel (sincrono), y el llamador encadena el
 * morph de regreso al carrusel -- dos morphs confirmados, un solo
 * gesto fluido. */
void aura_nowplaying_unfold_from_lyrics(void)
{
    mode4_morph(-1);
}

/* -- Modos de la rueda (doc SS5): un icono activo a la vez, SELECT
 * cicla. El modo Letra (aura_nowplaying_wheel_lyrics) muestra la vista
 * de letra mientras esta activo -- no hay un toggle separado, "el modo
 * activo" y "la letra visible" son el mismo estado (simplificacion
 * deliberada: el doc describe un panel comprimido FULL-CARRY con la
 * lista de modos en columna, que no se construyo esta pasada -- ver
 * D-078). */
typedef enum {
    NP_MODE_VOLUME = 0,
    NP_MODE_SCRUB,
    NP_MODE_PLAYLIST,
    NP_MODE_LYRICS,
    NP_MODE_STARS,
    NP_MODE_COUNT,
} np_mode_t;

/* Juego de iconos de modos (encargo 2026-08-12): inactivo = version
 * LINEAL en -tertiary; activo = version FILL en ACENTO; deshabilitado
 * (solo Playlist y Letra pueden estarlo) = lineal -tertiary al 50%. */
static const char *const MODE_ICONS[NP_MODE_COUNT] = {
    "mode-volume", "mode-scrub", "mode-playlist", "mode-lyrics", "mode-stars",
};
static const char *const MODE_ICONS_FILL[NP_MODE_COUNT] = {
    "mode-volume-fill", "mode-scrub-fill", "mode-playlist-fill",
    "mode-lyrics-fill", "mode-stars-fill",
};

static np_mode_t s_mode = NP_MODE_VOLUME;
static int mode_available(np_mode_t mode);

/* Y real de la fila de iconos de modos en el ultimo render no-compacto
 * -- lo consume aura_transition_flip_and_flow() para animar ese grupo
 * "desde la derecha" (now-playing.md, tabla de entrada por grupos) con
 * el layout REAL de la pantalla ya renderizada offscreen, sin duplicar
 * aca la logica de apilado de textos que decide su posicion. */
static int s_last_mode_row_y = 120;

int aura_nowplaying_last_mode_row_y(void)
{
    return s_last_mode_row_y;
}

/* El cambio de modo es INMEDIATO (encargo 2026-08-12: se retira el
 * salto de resorte del icono que se activa; la entrada al modo Letra
 * tendra su propio tratamiento, pendiente de detallar). */

/* Panel de anadir a lista (modo 5.3). */
static int s_playlist_sel = -1; /* -1 = sin seleccion todavia, ver cycle_mode() */

/* Vista previa en vivo de la posicion mientras se escrubea (modo 5.2) --
 * separada de id3->elapsed real hasta soltar, para que el numero y el
 * relleno respondan a la rueda sin parpadeo (doc SS5.2) ni cambiar la
 * posicion real en cada tick de scroll (solo al confirmar el gesto).
 * -1 = sin vista previa activa (se usa el elapsed real). */
static long s_scrub_preview_ms = -1;

bool aura_nowplaying_active(void)
{
    return (audio_status() & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)) != 0;
}

static void derive_sibling_path(const char *audio_path, const char *new_ext,
                                 char *out, size_t outsz)
{
    char *dot;

    strlcpy(out, audio_path, outsz);
    dot = strrchr(out, '.');
    if (dot && (!strchr(dot, '/')))
        *dot = '\0';
    strlcat(out, new_ext, outsz);
}

/* Recorta las 4 esquinas de un bitmap EN MEMORIA (no en pantalla) al
 * radio de CoverFlow (componentes/now-playing.md: "esquinas... heredadas
 * del bitmap .pfraw ya enmascarado"). El cache .pfraw con LUT por
 * esquina es trabajo de T3.2 (Cover Flow real) -- esta caratula se
 * proyecta con inclinacion via aura_flow (no un blit rectangular
 * simple), asi que a26_shell_round_bitmap_corners() (que pinta
 * directo sobre la pantalla ya dibujada) no aplica: hace falta
 * enmascarar la FUENTE antes de proyectarla, para que el area ya
 * transparente/de-fondo se deforme junto con el resto de la imagen.
 * Misma formula de distancia que stamp_corner() (apple2026_shell.c),
 * aplicada aca sobre el buffer en vez de sobre el framebuffer. */
static void mask_corners_buffer(fb_data *buf, int size, int radius, unsigned bg)
{
    int r256 = radius * 256;
    int dx, dy;

    for (dy = 0; dy < radius; dy++)
    {
        for (dx = 0; dx < radius; dx++)
        {
            int rx = radius - 1 - dx;
            int ry = radius - 1 - dy;
            int dist256 = (int)a26_shell_isqrt256((unsigned)(rx * rx + ry * ry));
            size_t idx[4];
            int k, t;

            if (dist256 <= r256 - 128)
                continue;

            idx[0] = (size_t)dy * size + dx;
            idx[1] = (size_t)dy * size + (size - 1 - dx);
            idx[2] = (size_t)(size - 1 - dy) * size + dx;
            idx[3] = (size_t)(size - 1 - dy) * size + (size - 1 - dx);

            if (dist256 >= r256 + 128)
            {
                for (k = 0; k < 4; k++)
                    buf[idx[k]] = bg;
                continue;
            }
            /* Borde antialiasado, misma rampa que stamp_corner. */
            t = dist256 - (r256 - 128);
            for (k = 0; k < 4; k++)
                buf[idx[k]] = a26_shell_blend(buf[idx[k]], bg, t);
        }
    }
}

static bool load_album_art(const struct mp3entry *id3)
{
    char path[MAX_PATH];
    struct dim d = { ART_SIZE, ART_SIZE };
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int len;
    int ret;

    s_art_bm.width = ART_SIZE;
    s_art_bm.height = ART_SIZE;
    s_art_bm.data = (char *)s_art_buf;
#if (LCD_DEPTH > 1)
    s_art_bm.maskdata = NULL;
#endif

    if (find_albumart(id3, path, sizeof(path), &d))
    {
        len = (int)strlen(path);
        if (len > 4 && !strcasecmp(path + len - 4, ".bmp"))
            ret = read_bmp_file(path, &s_art_bm, sizeof(s_art_buf), format, NULL);
        else
            ret = read_jpeg_file(path, &s_art_bm, sizeof(s_art_buf), format, NULL);
    }
    else if (id3->has_embedded_albumart
             && (id3->albumart.type & AA_CLEAR_FLAGS_MASK) == AA_TYPE_JPG)
    {
        /* Caratula embebida en el track (mismo criterio que
         * playback.c y aura_albumart.c) -- el id3 del track actual ya
         * trae pos/size llenos por el motor de reproduccion, sin
         * get_metadata() extra. */
        ret = clip_jpeg_file(id3->path, id3->albumart.pos,
                              id3->albumart.size, &s_art_bm,
                              sizeof(s_art_buf), format, NULL);
    }
    else
        return false;

    if (ret <= 0)
        return false;

    mask_corners_buffer((fb_data *)s_art_bm.data, ART_SIZE, ART_RADIUS,
                         a26_color(A26_SHELL_BG));
    aura_art_generate_reflection((const fb_data *)s_art_bm.data,
                                  (fb_data *)s_reflection_buf,
                                  ART_SIZE, ART_REFLECTION_PCT,
                                  a26_color(A26_SHELL_BG), false);
    return true;
}

static bool load_lyrics(const char *audio_path)
{
    static char buf[LRC_FILE_BUF_SIZE];
    char lrc_path[MAX_PATH];
    int fd, n;

    derive_sibling_path(audio_path, ".lrc", lrc_path, sizeof(lrc_path));

    fd = open(lrc_path, O_RDONLY);
    if (fd < 0)
        return false;

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return false;

    buf[n] = '\0';
    aura_lrc_parse(buf, &s_lrc);
    return s_lrc.count > 0;
}

static void reload_for_track(const struct mp3entry *id3)
{
    if (!strcmp(s_loaded_path, id3->path))
        return;

    strlcpy(s_loaded_path, id3->path, sizeof(s_loaded_path));
    s_panel_colors_valid = false;
    s_art_valid = load_album_art(id3);
    if (!s_art_valid)
    {
        /* Caratula "Default" (imagen de referencia del dueno del
         * diseno): nota gris sobre tile gris claro, mismo pipeline de
         * esquinas + reflejo que una caratula real -- reemplaza al
         * recuadro vacio que draw_cover_tilted() dibujaba antes para
         * este caso. */
        s_art_bm.width = ART_SIZE;
        s_art_bm.height = ART_SIZE;
        s_art_bm.data = (char *)s_art_buf;
#if (LCD_DEPTH > 1)
        s_art_bm.maskdata = NULL;
#endif
        aura_albumart_default_tile((fb_data *)s_art_bm.data, ART_SIZE, false);
        mask_corners_buffer((fb_data *)s_art_bm.data, ART_SIZE, ART_RADIUS,
                             a26_color(A26_SHELL_BG));
        aura_art_generate_reflection((const fb_data *)s_art_bm.data,
                                      (fb_data *)s_reflection_buf,
                                      ART_SIZE, ART_REFLECTION_PCT,
                                      a26_color(A26_SHELL_BG), false);
        s_art_valid = true;
    }
    s_lrc_valid = load_lyrics(id3->path);
    s_scrub_preview_ms = -1; /* la cancion nueva arranca en su propio elapsed real */
    /* El modo NO se reinicia entre canciones (doc: no lo pide, y volver
     * siempre a Volumen al cambiar de tema seria mas sorpresa que
     * ayuda) -- si el modo activo era Letra y la cancion nueva no tiene
     * .lrc, la pantalla ya cae de vuelta al reproductor (ver
     * aura_nowplaying_draw). */
}

/* Nombre propio (no "format_time" a secas): apps/misc.h -- ahora
 * incluido para adjust_volume() -- ya declara un format_time() publico
 * con firma distinta; colisionaban. */
static void aura_format_track_time(unsigned long ms, char *buf, size_t bufsz)
{
    unsigned long total_s = ms / 1000;
    snprintf(buf, bufsz, "%lu:%02lu", total_s / 60, total_s % 60);
}

/* -- Caratula + reflejo, con inclinacion real (PLAN.md T3.1,
 * componentes/now-playing.md) --------------------------------------------
 *
 * "Esa inclinacion sutil ES el angulo de aterrizaje de Flip-and-Flow" --
 * se reusa DIRECTO el motor de proyeccion por columnas de aura_flow.c
 * (T1.1 de la Fase 31 anterior, ya extendido y probado por Cover Flow
 * viejo en aura_coverflow.c::draw_slide_perspective(), regla dura 7: no
 * reimplementar, extender). Mismo signo de angulo que la lateral
 * DERECHA de Cover Flow (angle = -iangle): "borde derecho retrocedido,
 * mira hacia la derecha" es exactamente esa convencion, solo que a 7
 * grados en vez de a los ~70 grados de una lateral completa.
 *
 * `AURA_NOWPLAYING_TILT_CX` (posicion horizontal en PFreal) es una
 * constante derivada -- no una formula que se resuelve por si sola en
 * runtime, mismo criterio que CF_OFFSETX_R en aura_coverflow.c --
 * buscada para que el borde IZQUIERDO de la proyeccion caiga en ART_X
 * con ART_SIZE=135 y este angulo exacto (ver DECISIONS.md D-099 para
 * el metodo: busqueda numerica contra la misma formula de
 * aura_flow_begin_projection(), verificada por pixel contra el render
 * real, no solo calculada). Publicas en aura_nowplaying.h (T3.2(d)):
 * Flip-and-Flow aterriza en esta MISMA geometria exacta. */
#define NP_TILT_IANGLE AURA_NOWPLAYING_TILT_IANGLE
#define NP_TILT_CX     AURA_NOWPLAYING_TILT_CX

/* `compact`: Modo 4 (componentes/now-playing.md) -- "el reflejo se
 * desvanece durante la transicion (no existe en el Modo 4)" (se omite
 * directo, ver header del archivo para el corte de alcance sobre la
 * animacion de fundido real) y la caratula se recorta al ancho del
 * panel comprimido de 130px en vez de reescalarse -- no existe una
 * primitiva de resize en tiempo real para bitmaps ya decodificados en
 * este pipeline (limite real, no una eleccion de estilo), asi que
 * "comprimir" se logra recortando columnas de pantalla mas alla de
 * MORPH_PANEL_W en vez de encoger la imagen fuente. */
static unsigned scale_rgb_pct(int r8, int g8, int b8, int pct)
{
    r8 = r8 * pct / 100; if (r8 > 255) r8 = 255;
    g8 = g8 * pct / 100; if (g8 > 255) g8 = 255;
    b8 = b8 * pct / 100; if (b8 > 255) b8 = 255;
    return LCD_RGBPACK(r8, g8, b8);
}

static void ensure_panel_colors(void)
{
    const fb_data *px = (const fb_data *)s_art_buf;
    unsigned long r = 0, g = 0, b = 0;
    int i, r8, g8, b8, lum;

    if (s_panel_colors_valid)
        return;

    if (!s_art_valid)
    {
        /* Sin caratula alguna: panel neutro derivado de tokens. */
        s_panel_top = a26_color(A26_SELECTION_FILL);
        s_panel_bot = a26_color(A26_SHELL_RAIL);
        s_panel_ink = a26_color(A26_TEXT_PRIMARY);
        s_panel_colors_valid = true;
        return;
    }

    /* Promedio RGB565 de la caratula completa (las 4 esquinas ya
     * horneadas pesan <0.5% del total, no hace falta excluirlas). */
    for (i = 0; i < ART_SIZE * ART_SIZE; i++)
    {
        unsigned v = px[i];
        r += (v >> 11) & 0x1F;
        g += (v >> 5) & 0x3F;
        b += v & 0x1F;
    }
    r8 = (int)(r / (ART_SIZE * ART_SIZE)) << 3;
    g8 = (int)(g / (ART_SIZE * ART_SIZE)) << 2;
    b8 = (int)(b / (ART_SIZE * ART_SIZE)) << 3;

    s_panel_top = scale_rgb_pct(r8, g8, b8, 115); /* degradado ligero */
    s_panel_bot = scale_rgb_pct(r8, g8, b8, 85);

    /* Tinta legible por luminancia: blanco constante sobre panel
     * oscuro; sobre panel claro, una version muy oscura del mismo
     * color (derivada, no un negro fijo). */
    lum = (2 * r8 + 5 * g8 + b8) / 8;
    s_panel_ink = (lum < 140)
        ? (unsigned)AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT
        : scale_rgb_pct(r8, g8, b8, 22);
    s_panel_colors_valid = true;
}

/* Degradado DIAGONAL del panel derecho: interpola del color promedio
 * aclarado (esquina superior izquierda de la hoja) al oscurecido
 * (inferior derecha). `x_rel` es la columna relativa al borde de la
 * hoja. */
static unsigned m4_grad_diag(int x_rel, int y)
{
    ensure_panel_colors();
    return a26_shell_blend(s_panel_top, s_panel_bot,
                            (x_rel + y) * 256 / (M4_RPANEL_W + A26_SCREEN_HEIGHT));
}

/* Tinta de texto sobre la hoja: la tinta legible por luminancia contra
 * el degradado aproximado en esa fila, con `strength` (0..256) para la
 * jerarquia (titulo/linea activa plenos, secundarios atenuados). Los
 * textos llegan YA RENDERIZADOS montados en la hoja (sin fade). */
static unsigned m4_text_ink(int y, int strength)
{
    ensure_panel_colors();
    return a26_shell_blend(m4_grad_diag(M4_RPANEL_W / 2, y), s_panel_ink,
                            strength);
}

/* Desplazamiento de la hoja: entra desde la derecha con el morph. */
static int m4_panel_dx(int t256)
{
    return (256 - t256) * M4_RPANEL_W / 256;
}

/* La hoja de vidrio: tinte traslucido del degradado diagonal sobre lo
 * que YA este dibujado debajo (durante el morph se adivinan los restos
 * del layout viejo a traves del vidrio), mas la sombra paralela que la
 * hoja proyecta sobre el contenido a su izquierda. */
static void draw_m4_right_panel(int t256)
{
    int x0 = MORPH_PANEL_W + m4_panel_dx(t256);
    int x, y, sx;

    if (t256 <= 0 || x0 >= A26_SCREEN_WIDTH)
        return;

    for (y = 0; y < A26_SCREEN_HEIGHT; y++)
    {
        fb_data *row = FBADDR(0, y);

        for (x = x0; x < A26_SCREEN_WIDTH; x++)
            row[x] = a26_shell_blend(row[x], m4_grad_diag(x - x0, y),
                                      M4_GLASS_ALPHA);

        /* Sombra paralela del panel IZQUIERDO sobre la hoja
         * (correccion 2026-08-12: es el panel izquierdo el elevado,
         * no la hoja): franja fija en su borde, decayendo hacia la
         * derecha, que aparece con el morph. */
        for (sx = MORPH_PANEL_W; sx < MORPH_PANEL_W + M4_PANEL_SHADOW_W; sx++)
            row[sx] = a26_shell_blend(row[sx], LCD_RGBPACK(0, 0, 0),
                                       88 * (M4_PANEL_SHADOW_W - (sx - MORPH_PANEL_W))
                                          * t256 / (M4_PANEL_SHADOW_W * 256));
    }
}

/* `t256`: morph del Modo 4 -- 0 = caratula inclinada de 135px con
 * reflejo (layout normal), 256 = cuadrado perfecto de 106px centrado
 * en el panel de 130px, SIN reflejo (se desvanece en el trayecto). El
 * mismo proyector de columnas hace la inclinacion, el zoom y el
 * desplazamiento (mismas formulas que el morph de regreso al
 * carrusel en aura_transitions.c). */
static void draw_cover_tilted(int t256)
{
    aura_flow_slide_t slide;
    aura_flow_projection_t proj;
    int refl_h = aura_art_reflection_height(ART_SIZE, ART_REFLECTION_PCT);
    int total_h = (t256 >= 256) ? ART_SIZE : (ART_SIZE + refl_h);
    int max_screen_x = AURA_FLOW_SCREEN_W;
    int size_vis = ART_SIZE - ((ART_SIZE - M4_ART_SIZE) * t256) / 256;
    int cy = aura_pattern_lerp(ART_Y + ART_SIZE / 2,
                                M4_ART_Y + M4_ART_SIZE / 2, t256);
    int refl_alpha = 256 - t256;
    unsigned shell = a26_color(A26_SHELL_BG);
    static fb_data col_buf[ART_SIZE + (ART_SIZE * 60 / 100)]; /* margen holgado sobre refl_h real */

    if (!s_art_valid)
    {
        lcd_set_foreground(a26_color(A26_SHELL_RAIL));
        lcd_drawrect(aura_pattern_lerp(ART_X, M4_ART_X, t256),
                     aura_pattern_lerp(ART_Y, M4_ART_Y, t256),
                     size_vis, size_vis);
        return;
    }

    if (t256 > 0)
    {
        /* Sombra paralela del album (correccion 2026-08-12): aparece
         * con el morph mientras el reflejo se va -- mismo criterio del
         * indicador de la barra (sombra dura sutil, D-123), sobre el
         * fondo uniforme del panel izquierdo. */
        int sx = aura_pattern_lerp(ART_X, M4_ART_X, t256);
        int sy = cy - size_vis / 2 + M4_ART_SHADOW_DY;
        unsigned sh = a26_shell_blend(shell, LCD_RGBPACK(0, 0, 0),
                                       72 * t256 / 256);
        a26_shell_fill_rounded_rect(sx, sy, size_vis, size_vis,
                                     ART_RADIUS, sh, shell);
    }

    /* Signo positivo (no `-NP_TILT_IANGLE`): con el signo negativo
     * original, el lado IZQUIERDO quedaba corto/retrocedido y el
     * DERECHO alto/completo -- al reves de lo pedido por el dueno del
     * diseno ("el lado alto, mas largo, deberia estar del lado
     * izquierdo, y el mas corto del derecho"). Verificado con angulo
     * exagerado (140) antes de fijar el real. Invertir el signo de
     * `angle` desplaza el borde izquierdo de la proyeccion -- no es
     * una reflexion en el lugar -- asi que `NP_TILT_CX` tambien se
     * re-derivo (ver aura_nowplaying.h) para que ese borde siga
     * cayendo en ART_X con el signo nuevo. */
    slide.angle = aura_pattern_lerp(NP_TILT_IANGLE, 0, t256);
    slide.distance = AURA_FLOW_CAM_DIST * ART_SIZE / size_vis - AURA_FLOW_CAM_DIST;
    /* El proyector escala cx junto con el zoom (distance): un cx
     * pensado en pixeles de pantalla se pre-multiplica por
     * fuente/visible para aterrizar donde se pide (el morph de regreso
     * al carrusel no lo nota porque su destino es el centro exacto,
     * cx = 0 -- medido en captura: sin esto la caratula caia 21px a la
     * derecha, a 106/135 del desplazamiento pedido). */
    slide.cx = aura_pattern_lerp(NP_TILT_CX,
        (MORPH_PANEL_W / 2 - A26_SCREEN_WIDTH / 2) << AURA_FLOW_SHIFT, t256)
        * ART_SIZE / size_vis;

    aura_flow_begin_projection(&proj, &slide, ART_SIZE);

    while (proj.screen_x < max_screen_x)
    {
        int col = aura_flow_source_column(&proj);
        int dy = aura_flow_vertical_scale(&proj);
        int p = 0, dest_row, n_rows = 0;
        const fb_data *cover = (const fb_data *)s_art_bm.data;
        const fb_data *refl = (const fb_data *)s_reflection_buf;
        /* Centrado vertical por columna (mismo criterio que el
         * carrusel, 2026-08-12): la caratula gira sobre su eje central
         * y ambos bordes convergen -- con el tilt de 7 grados es sutil
         * (~2px por lado), pero mantiene la continuidad exacta con la
         * geometria en la que aterriza Flip-and-Flow. */
        int cover_disp = (ART_SIZE << AURA_FLOW_SHIFT) / dy;
        int y_col = cy - cover_disp / 2;

        for (dest_row = 0; dest_row < total_h; dest_row++)
        {
            int source_row = p >> AURA_FLOW_SHIFT;

            if (source_row >= total_h)
                break;
            if (source_row < ART_SIZE)
                col_buf[dest_row] = cover[source_row * ART_SIZE + col];
            else
                /* El reflejo se desvanece con el morph (t=256: fuera). */
                col_buf[dest_row] = a26_shell_blend(shell,
                    refl[(source_row - ART_SIZE) * ART_SIZE + col], refl_alpha);
            p += dy;
            n_rows++;
        }
        if (n_rows > 0)
            lcd_bitmap(col_buf, proj.screen_x, y_col, 1, n_rows);

        if (!aura_flow_advance_column(&proj))
            break;
    }
}

/* -- Rating / estrellas (doc SS4) ---------------------------------------- */

/* El rating nativo de Rockbox es 0-10 (tagcache tag_rating, ver
 * onplay.c set_rating_inline() -- mismo mecanismo de persistencia que
 * usa Aura aca, tagcache_update_numeric()); la UI de Aura pide 0-5
 * estrellas (doc SS4/SS5.5). Mapeo par: estrella N <-> rating N*2. */
static int stars_from_rating(int rating)
{
    int stars = (rating + 1) / 2;
    if (stars < 0) stars = 0;
    if (stars > STAR_COUNT) stars = STAR_COUNT;
    return stars;
}

static void commit_rating(struct mp3entry *id3, int stars)
{
    if (stars < 0) stars = 0;
    if (stars > STAR_COUNT) stars = STAR_COUNT;

    id3->rating = stars * 2;
    if (id3->tagcache_idx && global_settings.runtimedb)
        tagcache_update_numeric(id3->tagcache_idx - 1, tag_rating, id3->rating);
}

static void draw_stars(int x, int y, int stars, bool editing, int alpha_256)
{
    /* Control de valor, no icono de menu: SI usa la variante .fill para
     * las llenas (doc SS4, unica excepcion documentada a "nunca .fill").
     * Editando (modo Estrellas activo): llenas en ACCENT -- el acento se
     * gana porque se esta editando con la rueda en ese momento
     * (Principio 2); llenas en reposo van en TEXT_PRIMARY; vacias van en
     * SHELL_RAIL siempre, editando o no (AUDITORIA-01 A-16: la variante
     * "-rail" del icono ya existe desde el Lote 5, destrabando el limite
     * de D-010/D-078 que antes solo generaba normal+acento). */
    int i;
    int step = A26_ICON_SIZE_STATUS + A26_SPACING_XS;

    for (i = 0; i < STAR_COUNT; i++)
    {
        bool filled = (i < stars);
        const char *name = filled ? "star-fill" : "star";

        if (alpha_256 < 256)
        {
            /* Desvanecimiento del Modo 4: misma tinta de cada estado,
             * con el alfa del morph (las estrellas desaparecen). */
            unsigned ink = !filled ? a26_color(A26_SHELL_RAIL)
                          : editing ? aura_accent()
                                     : a26_color(A26_TEXT_PRIMARY);
            aura_widgets_draw_icon_ink(name, A26_ICON_SIZE_STATUS,
                                        x + i * step, y, ink, alpha_256);
        }
        else if (!filled)
            aura_widgets_draw_icon_rail(name, A26_ICON_SIZE_STATUS, x + i * step, y);
        else if (editing)
            aura_widgets_draw_icon_selected(name, A26_ICON_SIZE_STATUS, x + i * step, y);
        else
            aura_widgets_draw_icon(name, A26_ICON_SIZE_STATUS, x + i * step, y);
    }
}

/* -- Bloque de texto + fila de modos (doc SS2/SS5) ------------------------ */

/* `t256`: morph del Modo 4 -- los textos y las estrellas se DESVANECEN
 * (fundido fluido; el titulo/artista/album renacen en el panel derecho,
 * ver draw_lyrics_panel) y la fila de modos viaja de su ancla derecha
 * al centro del panel de 130px. */
static void draw_text_and_modes(const struct mp3entry *id3, int t256)
{
    char line[160];
    int w, h;
    int y = ART_Y;
    int i;
    unsigned shell = a26_color(A26_SHELL_BG);
    int mode_row_w = NP_MODE_COUNT * A26_ICON_SIZE_MENU
                    + (NP_MODE_COUNT - 1) * A26_SPACING_SM;
    int mode_x = aura_pattern_lerp(A26_SCREEN_WIDTH - A26_SPACING_LG - mode_row_w,
                                    (MORPH_PANEL_W - mode_row_w) / 2, t256);
    int mode_y;

    if (t256 < 256)
    {
        /* Tipografia nueva (fundamentos/02-tipografia.md, "Tokens de
         * NowPlaying"): titulo Bold 12px, artista y album Regular 12px --
         * ninguno de los tres es TITLE/CAPTION del sistema viejo. Orden
         * titulo->artista->album: el documento confirma los TRES textos y
         * su tipografia (tabla "Tipografia") pero no un orden de lectura
         * explicito (la tabla "Layout actual" solo nombra titulo+artista,
         * sin mencionar album en absoluto) -- se agrega album despues del
         * artista por ser la convencion mas comun, provisional, ver
         * DECISIONS.md D-099. */
        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
        lcd_set_foreground(a26_shell_blend(shell, a26_color(A26_TEXT_PRIMARY), 256 - t256));
        snprintf(line, sizeof(line), "%s", id3->title ? id3->title : "");
        lcd_getstringsize((const unsigned char *)line, &w, &h);
        lcd_putsxy(TEXT_X, y, (const unsigned char *)line);
        y += h + A26_SPACING_XS;

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
        lcd_set_foreground(a26_shell_blend(shell, a26_color(A26_TEXT_SECONDARY), 256 - t256));
        snprintf(line, sizeof(line), "%s", id3->artist ? id3->artist : "");
        lcd_getstringsize((const unsigned char *)line, &w, &h);
        lcd_putsxy(TEXT_X, y, (const unsigned char *)line);
        y += h + A26_SPACING_XS;

        snprintf(line, sizeof(line), "%s", id3->album ? id3->album : "");
        lcd_getstringsize((const unsigned char *)line, &w, &h);
        lcd_putsxy(TEXT_X, y, (const unsigned char *)line);
        y += h + A26_SPACING_MD;

        /* Las estrellas DESAPARECEN en el Modo 4 (encargo 2026-08-12). */
        draw_stars(TEXT_X, y, stars_from_rating(id3->rating),
                   s_mode == NP_MODE_STARS, 256 - t256);
        y += A26_ICON_SIZE_STATUS + A26_SPACING_MD;
    }

    /* Fila de modos (encargo 2026-08-12): anclada a la barra -- el
     * borde inferior de los iconos queda 10px por encima de la barra
     * de progreso, alineados entre si por su centro, separados 4px;
     * la estrella (ultimo icono) respeta el padding interno derecho.
     * En Modo 4 (compacto) se mantiene centrada bajo la caratula. */
    mode_y = PROGRESS_Y - 10 - A26_ICON_SIZE_MENU;
    if (t256 == 0)
        s_last_mode_row_y = mode_y; /* para el morph de entrada, ver getter */

    for (i = 0; i < NP_MODE_COUNT; i++)
    {
        int icon_x = mode_x + i * (A26_ICON_SIZE_MENU + A26_SPACING_SM);
        bool active = (i == (int)s_mode);

        if (!mode_available((np_mode_t)i))
        {
            /* Deshabilitado (solo Playlist y Letra pueden estarlo):
             * mismo glifo LINEAL y color del inactivo, al 50% de
             * opacidad -- el loop de modos ya lo salta
             * (cycle_mode/mode_available). Sobre el panel del Modo 4
             * la tinta interpola hacia la tinta legible del panel. */
            aura_widgets_draw_icon_tertiary_dimmed(MODE_ICONS[i], A26_ICON_SIZE_MENU,
                                                    icon_x, mode_y, 128);
        }
        else if (active)
            /* Activo: variante FILL en ACENTO, cambio inmediato (sin
             * salto de resorte, encargo 2026-08-12). */
            aura_widgets_draw_icon_selected(MODE_ICONS_FILL[i], A26_ICON_SIZE_MENU,
                                             icon_x, mode_y);
        else
            aura_widgets_draw_icon_tertiary(MODE_ICONS[i], A26_ICON_SIZE_MENU,
                                             icon_x, mode_y);
    }
}

/* -- Pastilla de progreso + transporte (doc SS2) -------------------------- */

/* Barra unificada de progreso/volumen (encargo 2026-08-12):
 * - Reposo: carril 300x7 del color del Selector (SELECTION_FILL),
 *   puntas completamente redondeadas; relleno 298x5 BLANCO. SIN
 *   numeros de tiempo.
 * - Buscando (mantener backward/forward): relleno en ACENTO y los
 *   tiempos (00:00 / -00:00) visibles solo mientras dura la busqueda.
 * - Scrub (modo avance, rueda): relleno en ACENTO + indicador de
 *   avance de 15x11 blanco con sombra paralela sutil, centrado en el
 *   borde derecho del relleno.
 * - Volumen (rueda en modo volumen): la MISMA barra muestra el nivel
 *   de volumen en ACENTO; speaker-minus/plus en los extremos donde
 *   vivian los tiempos.
 * `x`/`width`: geometria interpolada por el morph del Modo 4 (0 =
 * normal, 256 = panel de 130px); el panel izquierdo conserva el fondo
 * normal, asi que fondos y tintas no cambian con el morph. */
static void draw_progress(const struct mp3entry *id3, int scrub_preview_ms,
                           int x, int width)
{
    int fill_x = x + 1;
    int fill_max = width - 2;
    int fill_w;
    int fill_y = PROGRESS_Y + (PROGRESS_TRACK_H - PROGRESS_FILL_H) / 2;
    unsigned track_color = a26_color(A26_SELECTION_FILL);
    unsigned white = AURA_DS_METRICS_SELECTOR_CONTENT_TINT_HEX_ON_ACCENT;
    bool vol_active = current_tick < s_volume_overlay_until;
    bool seeking = current_tick < s_seek_show_until;
    bool scrubbing = (current_tick < s_scrub_show_until) && !vol_active;
    unsigned fill_color = (vol_active || seeking || scrubbing) ? aura_accent() : white;
    unsigned long elapsed = (scrub_preview_ms >= 0) ? (unsigned long)scrub_preview_ms : id3->elapsed;

    a26_shell_fill_capsule(x, PROGRESS_Y, width, PROGRESS_TRACK_H,
                            track_color, a26_color(A26_SHELL_BG));

    if (vol_active)
    {
        /* Nivel de volumen en la misma barra. */
        int vol_min = sound_min(SOUND_VOLUME);
        int vol_max = sound_max(SOUND_VOLUME);
        if (global_settings.volume_limit < vol_max)
            vol_max = global_settings.volume_limit;
        fill_w = (vol_max > vol_min)
            ? (fill_max * (global_status.volume - vol_min)) / (vol_max - vol_min)
            : 0;
    }
    else
        fill_w = id3->length ? (int)((unsigned long long)fill_max * elapsed / id3->length) : 0;

    if (fill_w < 0)        fill_w = 0;
    if (fill_w > fill_max) fill_w = fill_max;
    if (fill_w > 0)
        a26_shell_fill_capsule(fill_x, fill_y, fill_w, PROGRESS_FILL_H,
                                fill_color, track_color);

    if (vol_active)
    {
        /* La pildora tambien indica el nivel de volumen (correccion
         * 2026-08-12: "no se renderiza al subir o bajar el volumen"). */
        int tw = AURA_DS_METRICS_NOW_PLAYING_SCRUB_THUMB_W;
        int th = AURA_DS_METRICS_NOW_PLAYING_SCRUB_THUMB_H;
        int tx = fill_x + fill_w - tw / 2;
        int ty = PROGRESS_Y + PROGRESS_TRACK_H / 2 - th / 2;

        if (tx < x) tx = x;
        if (tx + tw > x + width) tx = x + width - tw;

        a26_shell_fill_capsule_over(tx + 1, ty + 2, tw, th, LCD_RGBPACK(0, 0, 0), 56);
        a26_shell_fill_capsule_over(tx, ty, tw, th, white, 256);

        /* speaker-minus / speaker-plus en los extremos, con el fade
         * sutil del tramo final de la ventana. */
        long left_ticks = s_volume_overlay_until - current_tick;
        int alpha = (left_ticks < VOLUME_FADE_TICKS)
            ? (int)(256L * left_ticks / VOLUME_FADE_TICKS) : 256;
        /* Centradas en el eje horizontal de la fila de transporte
         * (correccion 2026-08-12: "mas abajo, alineados al centro de
         * los iconos"), a 16px como repetir/aleatorio. */
        int icon_y = TRANSPORT_CENTER_Y - A26_ICON_SIZE_TRANSPORT_SIDE / 2;

        aura_widgets_draw_icon_dimmed("speaker-minus", A26_ICON_SIZE_TRANSPORT_SIDE,
                                       x, icon_y, alpha);
        aura_widgets_draw_icon_dimmed("speaker-plus", A26_ICON_SIZE_TRANSPORT_SIDE,
                                       x + width - A26_ICON_SIZE_TRANSPORT_SIDE, icon_y,
                                       alpha);
    }
    else if (seeking)
    {
        /* Tiempos SOLO mientras se busca (formato del original:
         * transcurrido / restante con signo). */
        char timebuf[24], timebuf2[24];
        int w, h, text_y;
        unsigned long remaining = (id3->length > elapsed) ? (id3->length - elapsed) : 0;

        aura_format_track_time(elapsed, timebuf, sizeof(timebuf));
        timebuf2[0] = '-';
        aura_format_track_time(remaining, timebuf2 + 1, sizeof(timebuf2) - 1);

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_10));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)timebuf2, &w, &h);
        /* Centrados en el eje horizontal de la fila de transporte
         * (correccion 2026-08-12), como las bocinas -/+ de volumen. */
        text_y = TRANSPORT_CENTER_Y - h / 2;
        lcd_putsxy(x, text_y, (const unsigned char *)timebuf);
        lcd_putsxy(x + width - w, text_y, (const unsigned char *)timebuf2);
    }

    if (scrubbing || seeking)
    {
        /* Indicador de avance (pildora 15x11, referencia visual del
         * dueno del diseno) con su CENTRO en el borde derecho del
         * relleno -- visible en AMBOS ajustes de posicion (rueda y
         * botones sostenidos). Sombra paralela sutil primero, despues
         * la perilla blanca; capsulas compuestas contra el framebuffer
         * porque cruzan relleno, carril y fondo a la vez. */
        int tw = AURA_DS_METRICS_NOW_PLAYING_SCRUB_THUMB_W;
        int th = AURA_DS_METRICS_NOW_PLAYING_SCRUB_THUMB_H;
        int tx = fill_x + fill_w - tw / 2;
        int ty = PROGRESS_Y + PROGRESS_TRACK_H / 2 - th / 2;

        if (tx < x) tx = x;
        if (tx + tw > x + width) tx = x + width - tw;

        a26_shell_fill_capsule_over(tx + 1, ty + 2, tw, th, LCD_RGBPACK(0, 0, 0), 56);
        a26_shell_fill_capsule_over(tx, ty, tw, th, white, 256);
    }
}

/* `compact`: Modo 4 -- "de los controles solo se visualiza el icono de
 * Play/Pausa", centrado en el panel de 130px. */
/* `t256`: la fila entera (repetir + play/pausa + aleatorio) viaja del
 * centro de pantalla al centro del panel de 130px del Modo 4 -- ya no
 * se queda solo el play/pausa: el encargo 2026-08-12 pide que todo el
 * transporte se reacomode al panel. */
static void draw_transport(int t256)
{
    int status = audio_status();
    bool paused = (status & AUDIO_STATUS_PAUSE) != 0;
    int cx = aura_pattern_lerp(A26_SCREEN_WIDTH / 2, MORPH_PANEL_W / 2, t256);
    int y = TRANSPORT_Y;
    int side_y = TRANSPORT_CENTER_Y - A26_ICON_SIZE_TRANSPORT_SIDE / 2;
    int repeat_x = cx - A26_ICON_SIZE_TRANSPORT / 2 - TRANSPORT_GAP
                   - A26_ICON_SIZE_TRANSPORT_SIDE;
    int shuffle_x = cx + A26_ICON_SIZE_TRANSPORT / 2 + TRANSPORT_GAP;

    /* Repetir (izq.) / play-pausa (centro) / aleatorio (der.), como
     * bloque compacto a los costados del icono central -- ya no en los
     * extremos de la barra (encargo 2026-08-12). Son indicadores de
     * estado real (repeat_mode, playlist_shuffle); la interaccion vive
     * en los botones fisicos, la fila solo la hace visible.
     *
     * Los iconos son bitmaps ya horneados en un color fijo (D-010):
     * lcd_set_foreground() no los tine, asi que "activo" se expresa
     * eligiendo la variante -on/acento (aura_widgets_draw_icon_selected)
     * en vez de intentar recolorear -- no hay variante TEXT_TERTIARY
     * horneada, mismo limite que el resto de la pantalla (D-078). */
    {
        /* Repetir tiene VARIANTE de glifo (encargo 2026-08-12): repeat.1
         * cuando repite una sola cancion; acento en cualquier modo
         * activo, tinta normal apagado. */
        const char *rep_icon = (global_settings.repeat_mode == REPEAT_ONE)
            ? "repeat-1" : "repeat";
        if (global_settings.repeat_mode != REPEAT_OFF)
            aura_widgets_draw_icon_selected(rep_icon, A26_ICON_SIZE_TRANSPORT_SIDE,
                                             repeat_x, side_y);
        else
            aura_widgets_draw_icon(rep_icon, A26_ICON_SIZE_TRANSPORT_SIDE,
                                    repeat_x, side_y);
    }

    if (global_settings.playlist_shuffle)
        aura_widgets_draw_icon_selected("shuffle", A26_ICON_SIZE_TRANSPORT_SIDE,
                                         shuffle_x, side_y);
    else
        aura_widgets_draw_icon("shuffle", A26_ICON_SIZE_TRANSPORT_SIDE,
                                shuffle_x, side_y);

    /* Solo play/pausa al centro (encargo 2026-08-12: los glifos de
     * backward/forward se retiraron -- la interaccion vive en los
     * botones fisicos, la fila no necesita duplicarla). Mientras se
     * ajusta el volumen, el mismo lugar muestra la BOCINA DINAMICA de
     * 5 estados segun el nivel, con el fade sutil del tramo final. */
    if (current_tick < s_volume_overlay_until)
    {
        int vol_min = sound_min(SOUND_VOLUME);
        int vol_max = sound_max(SOUND_VOLUME);
        int pct;
        const char *icon;
        long left_ticks = s_volume_overlay_until - current_tick;
        int alpha = (left_ticks < VOLUME_FADE_TICKS)
            ? (int)(256L * left_ticks / VOLUME_FADE_TICKS) : 256;

        if (global_settings.volume_limit < vol_max)
            vol_max = global_settings.volume_limit;
        pct = (vol_max > vol_min)
            ? (100 * (global_status.volume - vol_min)) / (vol_max - vol_min)
            : 0;

        /* Umbrales del encargo: 0-2 mute, 2-15 bocina sola, 15-50 una
         * onda, 50-80 dos, 80-100 tres. */
        if (pct <= 2)       icon = "speaker-slash";
        else if (pct <= 15) icon = "speaker";
        else if (pct <= 50) icon = "speaker-wave-1";
        else if (pct <= 80) icon = "speaker-wave-2";
        else                icon = "speaker-wave-3";

        /* Lienzo de 36px renderizado a pointSize FIJO (el de 24px):
         * el cuerpo de la bocina mide IGUAL en los 5 estados y solo
         * las ondas ocupan mas lienzo (encargo 2026-08-12: "la bocina
         * siempre debe ser del mismo tamano"). Centrada en el mismo
         * centro que play/pause. */
        aura_widgets_draw_icon_dimmed(icon, A26_ICON_SIZE_VOL_DYNAMIC,
                                       cx - A26_ICON_SIZE_VOL_DYNAMIC / 2,
                                       TRANSPORT_CENTER_Y - A26_ICON_SIZE_VOL_DYNAMIC / 2,
                                       alpha);
    }
    else
        /* Estado real, no accion pendiente (correccion 2026-08-12). */
        aura_widgets_draw_icon(paused ? "pause-fill" : "play-fill", A26_ICON_SIZE_TRANSPORT,
                                cx - A26_ICON_SIZE_TRANSPORT / 2, y);
}

/* -- Modo 5.3: anadir a lista (doc SS5.3) --------------------------------- */

static void draw_playlist_panel(void)
{
    char labels[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
    int n, i;
    int box_h = 28;
    int box_w = 220;
    int box_x = (A26_SCREEN_WIDTH - box_w) / 2;
    int box_y = TRANSPORT_Y - box_h - A26_SPACING_LG - 20;
    int w, h;
    const char *label;

    n = aura_music_list_playlists(labels, AURA_MUSIC_MAX_ITEMS);
    /* Solo para mostrar -- aura_music_add_track_to_playlist() vuelve a
     * resolver el archivo real por indice, no usa estas cadenas (Fase
     * 32, D-081: ".m3u8" a la vista es jerga tecnica de archivo). */
    for (i = 0; i < n; i++)
        aura_music_playlist_display_name(labels[i], labels[i], AURA_MUSIC_ITEM_LEN);

    /* Panel flotante, NUNCA pantalla completa (Principio 3, doc SS5,
     * nota de implementacion 5.3) -- pastilla SELECTION_FILL igual que
     * una fila de lista, sobre lo que ya esta dibujado. */
    a26_shell_fill_rounded_rect(box_x, box_y, box_w, box_h,
                                 A26_LAYOUT_CORNER_RADIUS_PILL,
                                 a26_color(A26_SELECTION_FILL), a26_color(A26_SHELL_BG));

    lcd_setfont(a26_font(A26_FONT_STYLE_BODY));

    if (n == 0)
    {
        label = aura_str(AURA_STR_EMPTY_LIST);
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    }
    else if (s_playlist_sel < 0)
    {
        /* Sin seleccion todavia (doc SS8): pista neutra, no una lista ya
         * resaltada -- Select en este estado no confirma nada. */
        label = aura_str(AURA_STR_PLAYLIST_PICK);
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    }
    else
    {
        if (s_playlist_sel >= n)
            s_playlist_sel = n - 1;
        label = labels[s_playlist_sel];
        lcd_set_foreground(a26_color(A26_ACCENT));
    }

    lcd_getstringsize((const unsigned char *)label, &w, &h);
    lcd_putsxy(box_x + (box_w - w) / 2, box_y + (box_h - h) / 2, (const unsigned char *)label);
}

/* -- LyricsPanel, Modo 4 (PLAN.md T3.1(c), componentes/now-playing.md) ---
 *
 * "Letras a 12px Regular; la linea activa a 14px Bold" -- reusa
 * aura_lrc_find_active_line() (ya probado en test_lrc.c, sin cambios).
 * Version real, no la simplificacion anterior (D-078: aquella dibujaba
 * SOLO la linea activa + la siguiente, centradas en TODA la pantalla,
 * porque el panel izquierdo comprimido no existia todavia) -- ahora
 * muestra hasta 2 lineas de contexto arriba y abajo de la activa,
 * recortadas al ancho del panel (mismo mecanismo de viewport que
 * aura_marquee.c/aura_dynamic_title.c).
 *
 * Sin scroll horizontal por linea ni animacion de desplazamiento
 * vertical continuo entre lineas -- "avanzan sincronizadas" se
 * interpreta como CUAL linea esta resaltada, no una animacion de
 * scroll (el documento no pide una explicitamente). */
static void draw_lyrics_line_clipped(int x, int y, int w, const char *text)
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

/* Panel derecho del Modo 4: los textos del reproductor RENACEN aqui
 * arriba (titulo/artista/album, encargo 2026-08-12: "viviran en la
 * parte superior de la pantalla", llegan ya renderizados en su lugar
 * con un desvanecimiento) y la letra corre debajo. `t256` es el alfa
 * de llegada del morph. */
static void draw_lyrics_panel(const struct mp3entry *id3, int t256)
{
    int active = aura_lrc_find_active_line(&s_lrc, (long)id3->elapsed);
    int dx = m4_panel_dx(t256);
    int panel_x = LYRICS_PANEL_X + dx + A26_SPACING_LG;
    int panel_w = LYRICS_PANEL_W - 2 * A26_SPACING_LG;
    int cy;
    int active_w, active_h;
    int y, i, w, h;
    char line[160];

    /* Cabecera en la parte superior de la pantalla, MONTADA en la hoja
     * (se desliza con ella, sin fade -- correccion 2026-08-12), con la
     * tinta legible por luminancia sobre el vidrio de color. */
    y = A26_SPACING_LG;
    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_12));
    lcd_set_foreground(m4_text_ink(y, 256));
    snprintf(line, sizeof(line), "%s", id3->title ? id3->title : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    draw_lyrics_line_clipped(panel_x, y, panel_w, line);
    y += h + A26_SPACING_XS;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(m4_text_ink(y, 176));
    snprintf(line, sizeof(line), "%s", id3->artist ? id3->artist : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    draw_lyrics_line_clipped(panel_x, y, panel_w, line);
    y += h + A26_SPACING_XS;

    lcd_set_foreground(m4_text_ink(y, 176));
    snprintf(line, sizeof(line), "%s", id3->album ? id3->album : "");
    lcd_getstringsize((const unsigned char *)line, &w, &h);
    draw_lyrics_line_clipped(panel_x, y, panel_w, line);
    y += h + A26_SPACING_MD;

    /* La letra se centra en el espacio que queda bajo la cabecera. */
    cy = (y + A26_SCREEN_HEIGHT) / 2;

    if (active < 0)
        return;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_14));
    lcd_set_foreground(m4_text_ink(cy, 256));
    lcd_getstringsize((const unsigned char *)s_lrc.lines[active].text, &active_w, &active_h);
    draw_lyrics_line_clipped(panel_x, cy - active_h / 2, panel_w, s_lrc.lines[active].text);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(m4_text_ink(cy, 150));

    y = cy - active_h / 2 - A26_SPACING_XS;
    for (i = active - 1; i >= 0 && i >= active - 2; i--)
    {
        lcd_getstringsize((const unsigned char *)s_lrc.lines[i].text, &w, &h);
        y -= h + A26_SPACING_XS;
        draw_lyrics_line_clipped(panel_x, y, panel_w, s_lrc.lines[i].text);
    }

    y = cy + active_h / 2 + A26_SPACING_XS;
    for (i = active + 1; i < s_lrc.count && i <= active + 2; i++)
    {
        lcd_getstringsize((const unsigned char *)s_lrc.lines[i].text, &w, &h);
        draw_lyrics_line_clipped(panel_x, y, panel_w, s_lrc.lines[i].text);
        y += h + A26_SPACING_XS;
    }
}

/* -- Reproductor completo -------------------------------------------------- */

/* `t256`: 0 = layout normal, 256 = Modo 4 completo (panel de 130x240
 * del color promedio de la caratula + LyricsPanel), intermedios = el
 * morph fluido entre ambos (encargo 2026-08-12 -- reemplaza el corte
 * directo que D-101 habia dejado diferido: ahora el proyector de
 * columnas SI reescala la caratula en el trayecto, la misma tecnica
 * del morph de regreso al carrusel). */
static void draw_player(const struct mp3entry *id3, int scrub_preview_ms, int t256)
{
    int px = aura_pattern_lerp(PROGRESS_X, MORPH_PROGRESS_X, t256);
    int pw = aura_pattern_lerp(PROGRESS_W, MORPH_PROGRESS_W, t256);

    draw_cover_tilted(t256);
    draw_text_and_modes(id3, t256);
    draw_progress(id3, scrub_preview_ms, px, pw);
    draw_transport(t256);
    if (t256 > 0)
    {
        /* La hoja de vidrio AL FINAL: tinte sobre lo ya dibujado +
         * sombra sobre el contenido a su izquierda; sus textos van
         * montados encima. */
        draw_m4_right_panel(t256);
        draw_lyrics_panel(id3, t256);
    }

    if (t256 == 0 && s_mode == NP_MODE_PLAYLIST)
        draw_playlist_panel();
}

/* Morph de entrada/salida del Modo 4 (encargo 2026-08-12): sincrono y
 * acotado como el resto de las transiciones de Aura. dir > 0 entra al
 * panel, dir < 0 regresa al layout normal. La StatusBar no participa:
 * el Modo 4 es pantalla completa (el panel mide 240 de alto). */
static void mode4_morph(int dir)
{
    struct mp3entry *id3 = audio_current_track();
    int frames, frame_delay, i;

    if (!id3 || !lcd_active() || aura_settings.animation_mode == AURA_ANIM_NONE)
        return;

    if (aura_settings.animation_mode == AURA_ANIM_ALL)
    {
        frames = M4_MORPH_MS * 60 / 1000;
        frame_delay = HZ / 60;
    }
    else
    {
        frames = M4_MORPH_MS * 30 / 1000;
        frame_delay = HZ / 30;
    }

    for (i = 1; i <= frames; i++)
    {
        int t = 256 * i / frames; /* fundido lineal de contenido */

        if (dir < 0)
            t = 256 - t;
        a26_shell_clear_screen();
        draw_player(id3, (int)s_scrub_preview_ms, t);
        lcd_update();
        if (button_queue_full())
            button_clear_queue();
        if (i < frames)
            sleep(frame_delay);
    }
}

void aura_nowplaying_draw(void)
{
    struct mp3entry *id3 = audio_current_track();
    bool lyrics_mode;

    a26_shell_clear_screen();

    if (!id3)
    {
        aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_NOWPLAYING));
        return;
    }

    reload_for_track(id3);

    /* Si la pista nueva no trae letras, el Modo 4 no puede sostenerse. */
    if (s_mode == NP_MODE_LYRICS && !s_lrc_valid)
        s_mode = NP_MODE_VOLUME;

    /* Alcance continuo del motor (correccion 2026-08-12): mientras hay
     * un ajuste en curso, cada tick de dibujo empuja el objetivo MAS
     * RECIENTE en cuanto el motor queda libre -- sin esperar otro
     * evento de boton/rueda. Asi el ultimo salto nunca se queda
     * esperando una ventana. */
    if (s_scrub_preview_ms >= 0
        && s_seek_applied_ms >= 0
        && s_seek_applied_ms != s_scrub_preview_ms
        && seek_engine_ready(id3)
        && current_tick - s_seek_last_apply >= HZ / 5)
    {
        audio_ff_rewind(s_scrub_preview_ms);
        s_seek_applied_ms = s_scrub_preview_ms;
        s_seek_last_apply = current_tick;
    }

    /* Asentamiento tras el ajuste (correccion 2026-08-12: "la barra
     * blanca no conserva la posicion, se tarda en llegar al punto
     * ajustado"): audio_ff_rewind() es ASINCRONO -- el motor tarda en
     * completar el salto y id3->elapsed sigue reportando la posicion
     * vieja un rato. Soltar el preview al expirar la ventana hacia que
     * la barra blanca regresara atras y "persiguiera" el ajuste. Ahora:
     * (1) al expirar, se aplica la posicion final exacta UNA vez;
     * (2) la barra blanca SIGUE mostrando la posicion ajustada hasta
     * que el elapsed real la alcanza (~2s de tolerancia), con un
     * tope de seguridad de 5s por si el salto fallara. */
    if (s_scrub_preview_ms >= 0
        && current_tick >= s_seek_show_until
        && current_tick >= s_scrub_show_until)
    {
        static long s_settle_since = 0;
        long diff;

        if (s_seek_applied_ms >= 0 && s_seek_applied_ms != s_scrub_preview_ms)
        {
            audio_ff_rewind(s_scrub_preview_ms);
            s_seek_applied_ms = s_scrub_preview_ms;
            s_settle_since = current_tick;
        }
        else if (s_settle_since == 0)
            s_settle_since = current_tick;

        diff = (long)id3->elapsed - s_scrub_preview_ms;
        if (diff < 0)
            diff = -diff;

        if (diff < 2000 || current_tick - s_settle_since > 5 * HZ)
        {
            s_seek_applied_ms = -1;
            s_scrub_preview_ms = -1;
            s_settle_since = 0;
        }
    }

    /* Tap pendiente de LEFT/RIGHT: si su ventana expiro sin repeats,
     * era un tap de verdad -- pista anterior/siguiente ahora. */
    if (s_lr_pending_dir != 0 && current_tick >= s_lr_pending_until)
    {
        int dir = s_lr_pending_dir;
        s_lr_pending_dir = 0;
        s_scrub_preview_ms = -1;
        if (dir > 0)
            audio_next();
        else
            audio_prev();
    }

    /* Modo 4 (componentes/now-playing.md): el panel izquierdo
     * comprimido y el LyricsPanel conviven -- ya no es "o uno o el
     * otro" como en la simplificacion anterior (D-078), que trataba
     * Letra como una vista de pantalla completa separada. */
    lyrics_mode = (s_mode == NP_MODE_LYRICS && s_lrc_valid);
    /* El Modo 4 es pantalla completa: sin StatusBar (el panel de
     * 130x240 y la cabecera del panel derecho ocupan su franja). */
    if (!lyrics_mode)
        aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_NOWPLAYING));
    draw_player(id3, (int)s_scrub_preview_ms, lyrics_mode ? 256 : 0);
}

bool aura_nowplaying_needs_tick(void)
{
    return current_tick < s_volume_overlay_until
        || current_tick < s_seek_show_until
        || current_tick < s_scrub_show_until
        || s_scrub_preview_ms >= 0 /* fase de asentamiento post-ajuste */
        || s_lr_pending_dir != 0;
}

/* La hoja del Modo 4 esta en pantalla: aura_main estampa solo las
 * esquinas izquierdas (la hoja es cuadrada). */
bool aura_nowplaying_sheet_active(void)
{
    return s_mode == NP_MODE_LYRICS && s_lrc_valid;
}

static int playlist_count(void)
{
    char labels[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
    return aura_music_list_playlists(labels, AURA_MUSIC_MAX_ITEMS);
}

/* Modos que pueden no estar disponibles se saltan en el loop (PLAN.md
 * T3.1(b), componentes/now-playing.md: Letra "si la cancion no tiene
 * letras, el modo se desactiva -- el loop de modos lo salta"; Playlist
 * "si no hay playlists existentes, el modo no se activa". Volumen,
 * Busqueda y Estrellas siempre estan disponibles. */
static int playlists_exist_cached(void)
{
    /* mode_available() ahora corre por CUADRO (los iconos dibujan su
     * estado deshabilitado, D-134) y a 60fps durante el morph del
     * Modo 4 -- playlist_count() escanea el directorio de listas en
     * disco, asi que se cachea con un TTL de 1s. */
    static long s_checked_tick = 0;
    static int s_cached = 0;

    if (s_checked_tick == 0 || current_tick - s_checked_tick > HZ)
    {
        s_cached = playlist_count() > 0;
        s_checked_tick = current_tick;
    }
    return s_cached;
}

static int mode_available(np_mode_t mode)
{
    if (mode == NP_MODE_LYRICS)
        return s_lrc_valid;
    if (mode == NP_MODE_PLAYLIST)
        return playlists_exist_cached();
    return 1;
}

static void cycle_mode(int direction)
{
    /* Acotado a NP_MODE_COUNT intentos -- nunca gira en un bucle
     * infinito: Volumen/Busqueda/Estrellas nunca se saltan, asi que
     * siempre hay al menos un modo disponible donde detenerse. */
    np_mode_t prev = s_mode;
    int next = (int)s_mode;
    int tries;

    for (tries = 0; tries < NP_MODE_COUNT; tries++)
    {
        next = (next + direction + NP_MODE_COUNT) % NP_MODE_COUNT;
        if (mode_available((np_mode_t)next))
            break;
    }

    s_mode = (np_mode_t)next;
    s_scrub_preview_ms = -1;
    /* Doc SS8 (anti-patron): "Seleccion inicial activa en listas
     * destructivas -- anadir a playlist debe entrar sin seleccion".
     * Sin este reset, entrar al modo Playlist ya dejaba la primera lista
     * pre-resaltada -- un SELECT reflejo (el mismo gesto que cicla en
     * cualquier otro modo) agregaba la cancion sin que el usuario
     * hubiera elegido nada a proposito (Fase 32, D-081). */
    if (s_mode == NP_MODE_PLAYLIST)
        s_playlist_sel = -1;

    /* Entrada/salida del Modo 4 con su morph (encargo 2026-08-12);
     * salir hacia el modo siguiente es la misma transicion invertida. */
    if (prev != NP_MODE_LYRICS && s_mode == NP_MODE_LYRICS)
        mode4_morph(1);
    else if (prev == NP_MODE_LYRICS && s_mode != NP_MODE_LYRICS)
        mode4_morph(-1);
}

void aura_nowplaying_handle_button(aura_nav_t *nav, long button)
{
    struct mp3entry *id3 = audio_current_track();

    switch (button)
    {
    case BUTTON_SELECT:
        /* Select cicla los modos de la rueda (doc base SS7, extendido a
         * 5 modos por SS5 de este documento) -- CON una excepcion
         * explicita del propio doc (fila 5.3: "Select confirma anadir"):
         * en modo Playlist CON al menos una playlist real para elegir,
         * Select confirma en vez de ciclar, para que el usuario pueda
         * agregar a varias listas seguidas sin volver a entrar al modo
         * cada vez. Bug real encontrado probando la pantalla (no en
         * teoria): sin este chequeo de "hay algo que confirmar", con
         * cero playlists creadas Select nunca volvia a llamar
         * cycle_mode() estando en este modo -- Letra y Estrellas
         * quedaban inalcanzables, no solo Playlist atascado (ese caso ya
         * lo cubre el escape de MENU). Entrar/salir de Letra no necesita
         * una excepcion propia: ya es simplemente que el modo activo sea
         * o no NP_MODE_LYRICS (simplificacion respecto al panel
         * comprimido del doc, ver D-078). */
        if (s_mode == NP_MODE_PLAYLIST && id3 && s_playlist_sel >= 0 && playlist_count() > 0)
        {
            /* "Select con una playlist seleccionada = agrega la cancion
             * Y regresa al Modo 1" (componentes/now-playing.md) -- una
             * sola accion atomica, no una confirmacion que se queda en
             * pantalla dentro del modo Playlist (asi lo hacia el
             * sistema viejo, D-081; el documento nuevo ya no lo pide). */
            if (aura_music_add_track_to_playlist(s_playlist_sel, id3->path))
            {
                s_mode = NP_MODE_VOLUME;
            }
        }
        else if (s_mode != NP_MODE_PLAYLIST || s_playlist_sel < 0)
        {
            cycle_mode(1);
        }
        break;
    case BUTTON_RIGHT | BUTTON_REL:
    case BUTTON_LEFT | BUTTON_REL:
        /* Al SOLTAR se decide el tap, sin timeouts (correccion
         * 2026-08-12): tap pendiente -> cambia de pista YA. Si la
         * pulsacion fue un hold, su conmutacion ya ocurrio en el
         * primer repeat -- aqui solo se libera el latch. */
        s_lr_hold_fired = false;
        if (s_lr_pending_dir != 0)
        {
            int dir = s_lr_pending_dir;
            s_lr_pending_dir = 0;
            s_scrub_preview_ms = -1;
            s_seek_applied_ms = -1;
            if (dir > 0)
                audio_next();
            else
                audio_prev();
        }
        break;

    case BUTTON_RIGHT:
    case BUTTON_LEFT:
    {
        int dir = (button == BUTTON_RIGHT) ? 1 : -1;

        if (aura_main_last_was_repeat())
        {
            /* Mantener = conmutar modos de reproduccion (encargo
             * 2026-08-12, en CUALQUIER modo de la rueda): FORWARD
             * alterna aleatorio; BACKWARD cicla repetir. UNA sola
             * conmutacion por pulsacion sostenida (latch hasta el
             * REL). El salto de pista pendiente de este mismo press
             * queda cancelado. */
            if (!s_lr_hold_fired)
            {
                s_lr_hold_fired = true;
                if (dir > 0)
                    np_toggle_shuffle();
                else
                    np_cycle_repeat();
            }
            s_lr_pending_dir = 0;
        }
        else
        {
            /* Tap: decidir al "soltar" (ventana corta sin repeats),
             * como el iPod original -- si llegan repeats, era un hold
             * y el salto no ocurre. */
            s_lr_pending_dir = dir;
            s_lr_pending_until = current_tick + LR_TAP_WINDOW;
        }
        break;
    }
    case BUTTON_SCROLL_FWD:
    case BUTTON_SCROLL_BACK:
    {
        int dir = (button == BUTTON_SCROLL_FWD) ? 1 : -1;

        switch (s_mode)
        {
        case NP_MODE_VOLUME:
            adjust_volume(dir);
            s_volume_overlay_until = current_tick + VOLUME_OVERLAY_TICKS;
            break;

        case NP_MODE_SCRUB:
            if (id3)
            {
                long base = (s_scrub_preview_ms >= 0) ? s_scrub_preview_ms : (long)id3->elapsed;
                long delta = 3000L * dir; /* 3s por click, doc SS5.2 */
                long next = base + delta;

                if (next < 0) next = 0;
                if ((unsigned long)next > id3->length) next = (long)id3->length;

                s_scrub_preview_ms = next;
                /* Estrangulado de audio: primer click aplica al
                 * instante, los siguientes cuando el motor alcanzo el
                 * salto anterior -- clicks en rafaga reiniciaban la
                 * busqueda del motor y el audio solo saltaba al parar
                 * la rueda. */
                if (s_seek_applied_ms < 0
                    || (current_tick - s_seek_last_apply >= AUDIO_SEEK_APPLY_TICKS
                        && seek_engine_ready(id3)))
                {
                    audio_ff_rewind(next);
                    s_seek_last_apply = current_tick;
                    s_seek_applied_ms = next;
                }
                /* El avance de rueda hereda el comportamiento completo
                 * de la busqueda (encargo 2026-08-12): acento +
                 * indicador + TIEMPOS debajo de la barra. */
                s_scrub_show_until = current_tick + SEEK_SHOW_TICKS;
                s_seek_show_until = current_tick + SEEK_SHOW_TICKS;
            }
            break;

        case NP_MODE_PLAYLIST:
        {
            char labels[AURA_MUSIC_MAX_ITEMS][AURA_MUSIC_ITEM_LEN];
            int n = aura_music_list_playlists(labels, AURA_MUSIC_MAX_ITEMS);
            if (n > 0)
            {
                /* Primer giro sin seleccion previa: entra a la lista en
                 * vez de calcular un salto relativo a -1 (doc SS8). */
                s_playlist_sel = (s_playlist_sel < 0)
                    ? (dir > 0 ? 0 : n - 1)
                    : (s_playlist_sel + dir + n) % n;
            }
            break;
        }

        case NP_MODE_LYRICS:
            /* El icono no reacciona a la rueda directamente (doc SS5.4). */
            break;

        case NP_MODE_STARS:
            if (id3)
                commit_rating(id3, stars_from_rating(id3->rating) + dir);
            break;

        default:
            break;
        }
        break;
    }
    case BUTTON_MENU:
        /* Doc SS6: "Menu... cierra la letra". Mismo escape para Playlist
         * (SS5.3): con Select reservado para confirmar un agregado ahi,
         * hacia falta una salida -- si no, una vez dentro del modo
         * Playlist no habia forma de volver a ciclar (bug real,
         * encontrado probando la pantalla, no en teoria: Select nunca
         * volvia a llamar cycle_mode() estando en ese modo). MENU vuelve
         * a Volumen para ambos en vez de salir de la pantalla; salir de
         * verdad requiere estar en cualquier otro modo primero (mismo
         * "un paso atras a la vez" que el resto de la navegacion). */
        if (s_mode == NP_MODE_LYRICS)
        {
            /* Salida de pantalla completa (encargo 2026-08-12): desde
             * el Modo 4 se SALE del reproductor -- la pantalla se
             * desplaza a la derecha hacia el MENU anterior, nunca
             * transiciona al coverflow (aura_screens consume el flag
             * y salta el morph de regreso al carrusel). */
            s_exit_fullscreen = true;
            s_mode = NP_MODE_VOLUME;
            aura_nav_pop(nav);
        }
        else if (s_mode == NP_MODE_PLAYLIST)
        {
            s_mode = NP_MODE_VOLUME;
        }
        else
        {
            aura_nav_pop(nav);
        }
        break;
    default:
        break;
    }
}
