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

#define PROGRESS_Y       (A26_SCREEN_HEIGHT - 34)
#define PROGRESS_TRACK_H AURA_DS_METRICS_NOW_PLAYING_PROGRESS_TRACK_HEIGHT
#define PROGRESS_FILL_H  AURA_DS_METRICS_NOW_PLAYING_PROGRESS_FILL_HEIGHT
/* Barra unificada progreso/volumen (encargo 2026-08-12): carril de
 * 300x7 centrado, relleno de 298x5 (1px de aire por lado), puntas
 * completamente redondeadas. Sin numeros de tiempo en reposo. */
#define PROGRESS_W       AURA_DS_METRICS_NOW_PLAYING_PROGRESS_WIDTH
#define PROGRESS_X       ((A26_SCREEN_WIDTH - PROGRESS_W) / 2)
#define TRANSPORT_Y      (A26_SCREEN_HEIGHT - 14)

/* Ventanas de estado de la barra (ver draw_progress): buscar con
 * botones sostenidos muestra tiempos + acento; el ajuste de volumen la
 * convierte en barra de nivel con bocinas; ambos vuelven solos al
 * reposo. El "fade muy sutil" de la bocina vive en el ultimo tramo de
 * la ventana de volumen. */
#define SEEK_SHOW_TICKS   (HZ / 2)
#define VOLUME_FADE_TICKS (HZ / 3)
static long s_seek_show_until = 0;
/* Tap vs mantener en LEFT/RIGHT (encargo 2026-08-12, mismo criterio
 * del iPod original: la decision es al soltar): un tap salta de pista
 * DESPUES de una ventana corta sin repeats; si llegan repeats, es
 * busqueda dentro de la cancion y el salto pendiente se cancela. */
#define LR_TAP_WINDOW     (HZ * 35 / 100)
#define SEEK_STEP_MS      3000L
static long s_lr_pending_until = 0;
static int  s_lr_pending_dir = 0;
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
/* Paso de busqueda por TIEMPO REAL, no por evento (correccion
 * 2026-08-12: con paso fijo por repeat, la cola de botones se atrasaba
 * respecto al render y la barra seguia avanzando sola despues de
 * soltar). Cada repeat avanza (ticks transcurridos desde el repeat
 * anterior) x SEEK_RATE -- los eventos rezagados que se procesan en
 * rafaga tras soltar aportan deltas casi nulos y la barra se detiene
 * al instante. */
#define SEEK_RATE          25  /* segundos de cancion por segundo sostenido */
#define SEEK_FIRST_KICK_MS 1500
static long s_seek_last_event = 0;

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

static const char *const MODE_ICONS[NP_MODE_COUNT] = {
    "volume-2", "scrub", "playlist-add", "lyrics", "star",
};

static np_mode_t s_mode = NP_MODE_VOLUME;

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

/* Resorte del icono que se activa (doc SS5: "mismo resorte corto con
 * sobrepaso que la pastilla de seleccion") -- un pequeno salto vertical
 * en vez de aparecer de golpe. El que se desactiva no se anima (fundido
 * lineal simple per doc, que en un icono binario ya-coloreado equivale
 * a simplemente redibujarlo en TEXT_TERTIARY sin mas). */
#define MODE_POP_TICKS (HZ * AURA_MOTION_SPRING_MS / 1000)
static long s_mode_pop_since = -1000000;

/* Panel de anadir a lista (modo 5.3). */
static int s_playlist_sel = -1; /* -1 = sin seleccion todavia, ver cycle_mode() */

/* Vista previa en vivo de la posicion mientras se escrubea (modo 5.2) --
 * separada de id3->elapsed real hasta soltar, para que el numero y el
 * relleno respondan a la rueda sin parpadeo (doc SS5.2) ni cambiar la
 * posicion real en cada tick de scroll (solo al confirmar el gesto).
 * -1 = sin vista previa activa (se usa el elapsed real). */
static long s_scrub_preview_ms = -1;

int aura_nowplaying_wheel_animating(void)
{
    long elapsed = current_tick - s_mode_pop_since;
    return elapsed >= 0 && elapsed < MODE_POP_TICKS;
}

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
static void draw_cover_tilted(bool compact)
{
    aura_flow_slide_t slide;
    aura_flow_projection_t proj;
    int refl_h = aura_art_reflection_height(ART_SIZE, ART_REFLECTION_PCT);
    int total_h = compact ? ART_SIZE : (ART_SIZE + refl_h);
    int max_screen_x = compact ? MORPH_PANEL_W : AURA_FLOW_SCREEN_W;
    static fb_data col_buf[ART_SIZE + (ART_SIZE * 60 / 100)]; /* margen holgado sobre refl_h real */

    if (!s_art_valid)
    {
        lcd_set_foreground(a26_color(A26_SHELL_RAIL));
        lcd_drawrect(ART_X, ART_Y, ART_SIZE, ART_SIZE);
        return;
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
    slide.angle = NP_TILT_IANGLE;
    slide.distance = 0;
    slide.cx = NP_TILT_CX;

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
        int y_col = ART_Y + ART_SIZE / 2 - cover_disp / 2;

        for (dest_row = 0; dest_row < total_h; dest_row++)
        {
            int source_row = p >> AURA_FLOW_SHIFT;

            if (source_row >= total_h)
                break;
            col_buf[dest_row] = (source_row < ART_SIZE)
                ? cover[source_row * ART_SIZE + col]
                : refl[(source_row - ART_SIZE) * ART_SIZE + col];
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

static void draw_stars(int x, int y, int stars, bool editing)
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

        if (!filled)
            aura_widgets_draw_icon_rail(name, A26_ICON_SIZE_STATUS, x + i * step, y);
        else if (editing)
            aura_widgets_draw_icon_selected(name, A26_ICON_SIZE_STATUS, x + i * step, y);
        else
            aura_widgets_draw_icon(name, A26_ICON_SIZE_STATUS, x + i * step, y);
    }
}

/* -- Bloque de texto + fila de modos (doc SS2/SS5) ------------------------ */

/* `compact`: Modo 4 -- "todos los textos se desvanecen" (titulo,
 * artista, album, contador -- se interpreta que incluye el bloque de
 * estrellas, sin nada mas de donde colgarlo una vez que el texto que
 * lo introduce desaparece) y "los iconos de modos transicionan de
 * forma que todos los elementos del panel queden centrados en los
 * 130px" -- se omite el bloque de texto entero y se recentra la fila
 * de modos dentro de MORPH_PANEL_W en vez de alinearla a la derecha de
 * toda la pantalla. */
static void draw_text_and_modes(const struct mp3entry *id3, bool compact)
{
    char line[160];
    int w, h;
    int y = ART_Y;
    int i;
    int mode_row_w = NP_MODE_COUNT * A26_ICON_SIZE_MENU
                    + (NP_MODE_COUNT - 1) * A26_SPACING_SM;
    int mode_x = compact ? (MORPH_PANEL_W - mode_row_w) / 2
                          : A26_SCREEN_WIDTH - A26_SPACING_LG - mode_row_w;
    int mode_y;

    if (!compact)
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
        lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
        snprintf(line, sizeof(line), "%s", id3->title ? id3->title : "");
        lcd_getstringsize((const unsigned char *)line, &w, &h);
        lcd_putsxy(TEXT_X, y, (const unsigned char *)line);
        y += h + A26_SPACING_XS;

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        snprintf(line, sizeof(line), "%s", id3->artist ? id3->artist : "");
        lcd_getstringsize((const unsigned char *)line, &w, &h);
        lcd_putsxy(TEXT_X, y, (const unsigned char *)line);
        y += h + A26_SPACING_XS;

        snprintf(line, sizeof(line), "%s", id3->album ? id3->album : "");
        lcd_getstringsize((const unsigned char *)line, &w, &h);
        lcd_putsxy(TEXT_X, y, (const unsigned char *)line);
        y += h + A26_SPACING_MD;

        draw_stars(TEXT_X, y, stars_from_rating(id3->rating), s_mode == NP_MODE_STARS);
        y += A26_ICON_SIZE_STATUS + A26_SPACING_MD;
    }
    else
    {
        y = ART_Y + ART_SIZE + A26_SPACING_MD;
    }

    /* Fila de modos: debajo del bloque de texto, alineada a la derecha
     * de la pantalla (doc SS2) -- o centrada en MORPH_PANEL_W en Modo 4.
     * El icono activo va en ACCENT con un pequeno salto de resorte al
     * activarse (SS5); los otros 4 en TEXT_TERTIARY -- variante real
     * desde el Lote 5 de AUDITORIA-01 (A-16), ya no la aproximacion con
     * TEXT_PRIMARY que D-078 dejo documentada como limite de D-010. */
    mode_y = y;
    if (!compact && mode_y + A26_ICON_SIZE_MENU > ART_Y + ART_SIZE + REFL_GAP)
        mode_y = ART_Y + ART_SIZE + REFL_GAP - A26_ICON_SIZE_MENU; /* no invade el reflejo */
    if (!compact)
        s_last_mode_row_y = mode_y; /* para el morph de entrada, ver getter */

    for (i = 0; i < NP_MODE_COUNT; i++)
    {
        int icon_x = mode_x + i * (A26_ICON_SIZE_MENU + A26_SPACING_SM);
        int icon_y = mode_y;
        bool active = (i == (int)s_mode);

        if (active && aura_nowplaying_wheel_animating())
        {
            int eased = aura_motion_spring(current_tick - s_mode_pop_since, MODE_POP_TICKS);
            /* Salto vertical corto (doc: resorte de la pastilla,
             * reaplicado aca) -- hasta 4px de recorrido, con el mismo
             * sobrepaso que ya se ve en la pastilla de seleccion. */
            icon_y -= (4 * (256 - eased)) / 256;
        }

        if (i == (int)NP_MODE_LYRICS && !s_lrc_valid)
        {
            /* "Su icono sigue apareciendo... pero al 50% de opacidad"
             * (componentes/now-playing.md) -- distinto de -tertiary
             * (que es un color fijo para "inactivo pero disponible"):
             * este estado es "no disponible en absoluto", el loop de
             * modos ya lo salta (cycle_mode/mode_available). */
            aura_widgets_draw_icon_dimmed(MODE_ICONS[i], A26_ICON_SIZE_MENU, icon_x, icon_y, 128);
        }
        else if (active)
            aura_widgets_draw_icon_selected(MODE_ICONS[i], A26_ICON_SIZE_MENU, icon_x, icon_y);
        else
            aura_widgets_draw_icon_tertiary(MODE_ICONS[i], A26_ICON_SIZE_MENU, icon_x, icon_y);
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
 * `x`/`width`: normal o comprimida al panel de 130px en Modo 4. */
static void draw_progress(const struct mp3entry *id3, int scrub_preview_ms, int x, int width)
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
        int icon_y = PROGRESS_Y - A26_ICON_SIZE_STATUS - A26_SPACING_XS;

        aura_widgets_draw_icon_dimmed("speaker-minus", A26_ICON_SIZE_STATUS,
                                       x, icon_y, alpha);
        aura_widgets_draw_icon_dimmed("speaker-plus", A26_ICON_SIZE_STATUS,
                                       x + width - A26_ICON_SIZE_STATUS, icon_y, alpha);
    }
    else if (seeking)
    {
        /* Tiempos SOLO mientras se busca (formato del original:
         * transcurrido / restante con signo). */
        char timebuf[24], timebuf2[24];
        int w, h;
        unsigned long remaining = (id3->length > elapsed) ? (id3->length - elapsed) : 0;

        aura_format_track_time(elapsed, timebuf, sizeof(timebuf));
        timebuf2[0] = '-';
        aura_format_track_time(remaining, timebuf2 + 1, sizeof(timebuf2) - 1);

        lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_10));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_putsxy(x, PROGRESS_Y - A26_SPACING_MD - A26_SPACING_XS,
                   (const unsigned char *)timebuf);
        lcd_getstringsize((const unsigned char *)timebuf2, &w, &h);
        lcd_putsxy(x + width - w, PROGRESS_Y - A26_SPACING_MD - A26_SPACING_XS,
                   (const unsigned char *)timebuf2);
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
static void draw_transport(bool compact)
{
    int status = audio_status();
    bool paused = (status & AUDIO_STATUS_PAUSE) != 0;

    if (compact)
    {
        int cx = MORPH_PANEL_W / 2;
        aura_widgets_draw_icon(paused ? "play-fill" : "pause-fill", A26_ICON_SIZE_STATUS,
                                cx - A26_ICON_SIZE_STATUS / 2, TRANSPORT_Y);
        return;
    }
    int cx = A26_SCREEN_WIDTH / 2;
    int y = TRANSPORT_Y;

    /* Repetir (izq.) / retroceder-play/pausa-avanzar (centro) / aleatorio
     * (der.) -- doc SS2. Son indicadores de estado real (repeat_mode,
     * playlist_shuffle) mas los glifos de transporte; la interaccion en
     * si ya existe (BUTTON_LEFT/RIGHT = pista ant./sig., PLAY global
     * desde Fase 29) -- esta fila solo la hace visible en la pantalla,
     * no agrega botones nuevos.
     *
     * Los iconos son bitmaps ya horneados en un color fijo (D-010):
     * lcd_set_foreground() no los tine, asi que "activo" se expresa
     * eligiendo la variante -on/acento (aura_widgets_draw_icon_selected)
     * en vez de intentar recolorear -- no hay variante TEXT_TERTIARY
     * horneada, mismo limite que el resto de la pantalla (D-078). */
    if (global_settings.repeat_mode != 0)
        aura_widgets_draw_icon_selected("repeat", A26_ICON_SIZE_STATUS, A26_SPACING_XXL, y);
    else
        aura_widgets_draw_icon("repeat", A26_ICON_SIZE_STATUS, A26_SPACING_XXL, y);

    if (global_settings.playlist_shuffle)
        aura_widgets_draw_icon_selected("shuffle", A26_ICON_SIZE_STATUS,
                                         A26_SCREEN_WIDTH - A26_SPACING_XXL - A26_ICON_SIZE_STATUS, y);
    else
        aura_widgets_draw_icon("shuffle", A26_ICON_SIZE_STATUS,
                                A26_SCREEN_WIDTH - A26_SPACING_XXL - A26_ICON_SIZE_STATUS, y);

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

        aura_widgets_draw_icon_dimmed(icon, A26_ICON_SIZE_STATUS,
                                       cx - A26_ICON_SIZE_STATUS / 2, y, alpha);
    }
    else
        aura_widgets_draw_icon(paused ? "play-fill" : "pause-fill", A26_ICON_SIZE_STATUS,
                                cx - A26_ICON_SIZE_STATUS / 2, y);
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

static void draw_lyrics_panel(const struct mp3entry *id3)
{
    int active = aura_lrc_find_active_line(&s_lrc, (long)id3->elapsed);
    int panel_x = LYRICS_PANEL_X + A26_SPACING_LG;
    int panel_w = LYRICS_PANEL_W - 2 * A26_SPACING_LG;
    int cy = A26_SCREEN_HEIGHT / 2;
    int active_w, active_h;
    int y, i, w, h;

    if (active < 0)
        return;

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_BOLD_14));
    lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
    lcd_getstringsize((const unsigned char *)s_lrc.lines[active].text, &active_w, &active_h);
    draw_lyrics_line_clipped(panel_x, cy - active_h / 2, panel_w, s_lrc.lines[active].text);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));

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

/* `compact`: Modo 4 -- panel izquierdo comprimido a MORPH_PANEL_W (130px),
 * LyricsPanel ocupa el resto (dibujado por el llamador). Ver
 * DECISIONS.md D-101 para el alcance real de esta pasada: los DOS
 * estados (normal y Modo 4) estan completos, el corte entre ambos es
 * directo -- "Morph Directo" (la transicion fluida en si, con
 * aura_pattern_lerp ya listo desde T1.1) queda diferida porque el
 * tamano exacto de la caratula comprimida no esta definido en el
 * documento y este pipeline no tiene una primitiva de reescalado de
 * bitmaps en tiempo real. */
static void draw_player(const struct mp3entry *id3, int scrub_preview_ms, bool compact)
{
    draw_cover_tilted(compact);
    draw_text_and_modes(id3, compact);
    if (compact)
        draw_progress(id3, scrub_preview_ms, MORPH_PROGRESS_X, MORPH_PROGRESS_W);
    else
        draw_progress(id3, scrub_preview_ms, PROGRESS_X, PROGRESS_W);
    draw_transport(compact);

    if (s_mode == NP_MODE_PLAYLIST)
        draw_playlist_panel();
}

void aura_nowplaying_draw(void)
{
    struct mp3entry *id3 = audio_current_track();
    bool lyrics_mode;

    a26_shell_clear_screen();
    aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_NOWPLAYING));

    if (!id3)
        return;

    reload_for_track(id3);

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
    draw_player(id3, (int)s_scrub_preview_ms, lyrics_mode);
    if (lyrics_mode)
        draw_lyrics_panel(id3);

}

bool aura_nowplaying_needs_tick(void)
{
    return current_tick < s_volume_overlay_until
        || current_tick < s_seek_show_until
        || current_tick < s_scrub_show_until
        || s_scrub_preview_ms >= 0 /* fase de asentamiento post-ajuste */
        || s_lr_pending_dir != 0
        || aura_nowplaying_wheel_animating();
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
static int mode_available(np_mode_t mode)
{
    if (mode == NP_MODE_LYRICS)
        return s_lrc_valid;
    if (mode == NP_MODE_PLAYLIST)
        return playlist_count() > 0;
    return 1;
}

static void cycle_mode(int direction)
{
    /* Acotado a NP_MODE_COUNT intentos -- nunca gira en un bucle
     * infinito: Volumen/Busqueda/Estrellas nunca se saltan, asi que
     * siempre hay al menos un modo disponible donde detenerse. */
    int next = (int)s_mode;
    int tries;

    for (tries = 0; tries < NP_MODE_COUNT; tries++)
    {
        next = (next + direction + NP_MODE_COUNT) % NP_MODE_COUNT;
        if (mode_available((np_mode_t)next))
            break;
    }

    s_mode = (np_mode_t)next;
    s_mode_pop_since = current_tick;
    s_scrub_preview_ms = -1;
    /* Doc SS8 (anti-patron): "Seleccion inicial activa en listas
     * destructivas -- anadir a playlist debe entrar sin seleccion".
     * Sin este reset, entrar al modo Playlist ya dejaba la primera lista
     * pre-resaltada -- un SELECT reflejo (el mismo gesto que cicla en
     * cualquier otro modo) agregaba la cancion sin que el usuario
     * hubiera elegido nada a proposito (Fase 32, D-081). */
    if (s_mode == NP_MODE_PLAYLIST)
        s_playlist_sel = -1;
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
                s_mode_pop_since = current_tick;
            }
        }
        else if (s_mode != NP_MODE_PLAYLIST || s_playlist_sel < 0)
        {
            cycle_mode(1);
        }
        break;
    case BUTTON_RIGHT:
    case BUTTON_LEFT:
    {
        int dir = (button == BUTTON_RIGHT) ? 1 : -1;

        if (aura_main_last_was_repeat())
        {
            /* Mantener = adelantar/atrasar rapido dentro de la cancion
             * (encargo 2026-08-12): cada repeat avanza SEEK_STEP_MS;
             * la barra pasa a acento y muestra los tiempos SOLO
             * mientras dura la busqueda. El salto de pista pendiente
             * de este mismo press queda cancelado. */
            if (id3 && id3->length)
            {
                long base = (s_scrub_preview_ms >= 0) ? s_scrub_preview_ms
                                                       : (long)id3->elapsed;
                long delta_ticks = current_tick - s_seek_last_event;
                long step_ms;
                long next;
                bool fresh_hold = (delta_ticks > HZ / 2);

                /* Primer repeat de un hold nuevo: arranque con un
                 * empujon fijo y salto de audio INMEDIATO; los
                 * siguientes avanzan por tiempo real transcurrido. */
                step_ms = fresh_hold ? SEEK_FIRST_KICK_MS
                                      : delta_ticks * 1000L * SEEK_RATE / HZ;
                s_seek_last_event = current_tick;

                next = base + dir * step_ms;
                if (next < 0) next = 0;
                if ((unsigned long)next > id3->length) next = (long)id3->length;
                s_scrub_preview_ms = next;

                if (fresh_hold
                    || current_tick - s_seek_last_apply >= AUDIO_SEEK_APPLY_TICKS)
                {
                    audio_ff_rewind(next);
                    s_seek_last_apply = current_tick;
                    s_seek_applied_ms = next;
                }
                s_seek_show_until = current_tick + SEEK_SHOW_TICKS;
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
                /* Mismo estrangulado de audio que los botones: primer
                 * click aplica al instante, los siguientes cada ~250ms
                 * -- clicks en rafaga reiniciaban la busqueda del
                 * motor y el audio solo saltaba al parar la rueda. */
                if (s_seek_applied_ms < 0
                    || current_tick - s_seek_last_apply >= AUDIO_SEEK_APPLY_TICKS)
                {
                    audio_ff_rewind(next);
                    s_seek_last_apply = current_tick;
                    s_seek_applied_ms = next;
                }
                s_scrub_show_until = current_tick + SEEK_SHOW_TICKS;
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
        if (s_mode == NP_MODE_LYRICS || s_mode == NP_MODE_PLAYLIST)
        {
            s_mode = NP_MODE_VOLUME;
            s_mode_pop_since = current_tick;
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
