#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "file.h"
#include "dir.h"
#include "string-extra.h"
#include "recorder/bmp.h"
#include "recorder/jpeg_load.h"

#include "aura_photos.h"
#include "aura_widgets.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"
#include "aura_status_bar_v2.h"

#define PHOTOS_DIR      "/Photos"
#define MAX_PHOTOS      200
#define PHOTO_NAME_LEN  64

/* FORMAT_RESIZE necesita bastante mas que el bitmap final (D-026 en
 * DECISIONS.md); a pantalla completa (320x240x2) mas margen de sobra. */
#define VIEW_SCRATCH_SIZE (240 * 1024)

typedef struct {
    char filename[PHOTO_NAME_LEN];
    char display[PHOTO_NAME_LEN]; /* filename sin extension, AUDITORIA-01 A-05 */
    bool supported; /* jpg/bmp = true; png/gif listados pero no decodificables (D-028) */
} photo_item_t;

static photo_item_t s_photos[MAX_PHOTOS];
static int s_photo_count = -1;
static int s_current_index = 0;

static unsigned char s_view_scratch[VIEW_SCRATCH_SIZE];
static int s_loaded_index = -1;
static bool s_loaded_ok = false;
static struct bitmap s_bm;

static bool has_ext(const char *name, const char *ext)
{
    size_t nlen = strlen(name), elen = strlen(ext);
    return nlen > elen && !strcasecmp(name + nlen - elen, ext);
}

static bool is_supported_image(const char *name)
{
    return has_ext(name, ".jpg") || has_ext(name, ".jpeg") || has_ext(name, ".bmp");
}

static bool is_listable_image(const char *name)
{
    return is_supported_image(name) || has_ext(name, ".png") || has_ext(name, ".gif");
}

/* Nombre para mostrar sin extension de archivo (AUDITORIA-01 A-05, doc
 * de diseno Principio 7: "nunca jerga" -- un nombre crudo de archivo con
 * extension es jerga tecnica, mismo principio que ya corrigio D-081 en
 * los nombres de playlist). Solo pela la extension para la lista; el
 * nombre real con extension sigue viviendo en filename para abrir el
 * archivo. */
static void strip_ext_for_display(const char *filename, char *out, size_t outsz)
{
    char *dot;

    strlcpy(out, filename, outsz);
    dot = strrchr(out, '.');
    if (dot)
        *dot = '\0';
}

static void ensure_photo_list(void)
{
    DIR *d;
    struct DIRENT *entry;

    if (s_photo_count >= 0)
        return;

    s_photo_count = 0;
    d = opendir(PHOTOS_DIR);
    if (!d)
        return;

    while (s_photo_count < MAX_PHOTOS && (entry = readdir(d)) != NULL)
    {
        if (!is_listable_image(entry->d_name))
            continue;
        strlcpy(s_photos[s_photo_count].filename, entry->d_name, PHOTO_NAME_LEN);
        strip_ext_for_display(entry->d_name, s_photos[s_photo_count].display, PHOTO_NAME_LEN);
        s_photos[s_photo_count].supported = is_supported_image(entry->d_name);
        s_photo_count++;
    }
    closedir(d);
}

/* -- CoverDrift para la lista de Fotos (D-251) -----------------------
 *
 * A diferencia de Musica (que reusa el cache .pfraw de aura_albumart.c
 * ya existente), aca no habia NINGUN cargador de miniatura -- solo el
 * visor de una foto a pantalla completa (load_current_photo() arriba,
 * buffer s_view_scratch/s_bm de UNA sola foto). Buffer NUEVO y
 * SEPARADO para no corromper ese estado si el usuario esta navegando
 * el visor a la vez que la lista pide una miniatura de fondo.
 *
 * `read_jpeg_file()` no garantiza el tamano exacto pedido salvo con
 * FORMAT_RESIZE (sin FORMAT_KEEP_ASPECT, para llenar el tile cuadrado
 * completo en vez de dejar franjas -- aceptable para un fondo
 * ambiental, no es el visor real). El presupuesto de bytes
 * (`maxsize`) necesario NO es proporcional al tamano final: el
 * decoder necesita margen para el factor de escala JPEG intermedio
 * antes del resize final (JPEG_DECODE_OVERHEAD son 38KB fijos, mas
 * el buffer intermedio del propio factor de escala) -- mismo motivo
 * por el que load_current_photo() ya reserva 240KB
 * (VIEW_SCRATCH_SIZE) para un destino final de solo 320x240x2=150KB;
 * se reusa el MISMO tamano probado en vez de arriesgar un numero mas
 * chico con fotos reales grandes. */
#define DRIFT_THUMB_SCRATCH_SIZE VIEW_SCRATCH_SIZE
static unsigned char s_drift_thumb_scratch[DRIFT_THUMB_SCRATCH_SIZE];

static aura_coverdrift_image_t s_drift_photo_images[MAX_PHOTOS];
static int s_drift_photo_buf_a_idx = -1;
static int s_drift_photo_buf_b_idx = -1;
static struct bitmap s_drift_photo_bmp_a;
static struct bitmap s_drift_photo_bmp_b;
static fb_data s_drift_photo_pixels_a[AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE
                                       * AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE];
static fb_data s_drift_photo_pixels_b[AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE
                                       * AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE];
static int s_drift_photo_pool_count_last = -1;

/* Decodifica la foto `photo_idx` a un tile cuadrado de
 * AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE px fila-contigua en `dst`.
 * `false` si el formato no es decodificable (mismo criterio que
 * `.supported` del visor) o si la decodificacion fallo -- el llamador
 * cae al placeholder de CoverDrift dejando `.bmp` en NULL, nunca
 * arriesga un tile a medio llenar. */
static bool decode_photo_drift_tile(int photo_idx, fb_data *dst)
{
    enum { SZ = AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE };
    char path[MAX_PATH];
    struct bitmap bm;
    int format = FORMAT_NATIVE | FORMAT_RESIZE;
    int ret;

    if (!s_photos[photo_idx].supported)
        return false;

    snprintf(path, sizeof(path), "%s/%s", PHOTOS_DIR, s_photos[photo_idx].filename);

    bm.width = SZ;
    bm.height = SZ;
    bm.data = (char *)s_drift_thumb_scratch;
#if (LCD_DEPTH > 1)
    bm.maskdata = NULL;
#endif

    if (has_ext(path, ".bmp"))
        ret = read_bmp_file(path, &bm, sizeof(s_drift_thumb_scratch), format, NULL);
    else
        ret = read_jpeg_file(path, &bm, sizeof(s_drift_thumb_scratch), format, NULL);

    if (ret <= 0)
        return false;

    memcpy(dst, s_drift_thumb_scratch, SZ * SZ * sizeof(fb_data));
    return true;
}

/* Decodifica bajo demanda SOLO la miniatura activa y la anterior (ver
 * aura_coverdrift.h) -- nunca las s_photo_count de una vez. Si el
 * conteo de fotos cambia (lista releida), invalida todo -- mismo
 * criterio que ensure_drift_album_pool() en aura_screens.c. */
static void ensure_drift_photos_decoded(void)
{
    enum { SZ = AURA_DS_METRICS_COVER_DRIFT_IMAGE_SIZE };
    int active, prev;

    if (s_photo_count != s_drift_photo_pool_count_last)
    {
        s_drift_photo_pool_count_last = s_photo_count;
        memset(s_drift_photo_images, 0, sizeof(s_drift_photo_images));
        s_drift_photo_buf_a_idx = -1;
        s_drift_photo_buf_b_idx = -1;
    }

    active = aura_coverdrift_active_index();
    prev = aura_coverdrift_prev_index();

    if (active >= 0 && active < s_photo_count && active != s_drift_photo_buf_a_idx)
    {
        s_drift_photo_buf_a_idx = active;
        if (decode_photo_drift_tile(active, s_drift_photo_pixels_a))
        {
            s_drift_photo_bmp_a.width = SZ;
            s_drift_photo_bmp_a.height = SZ;
            s_drift_photo_bmp_a.data = (char *)s_drift_photo_pixels_a;
            s_drift_photo_images[active].bmp = &s_drift_photo_bmp_a;
        }
        else
        {
            s_drift_photo_images[active].bmp = NULL;
        }
    }

    if (prev >= 0 && prev < s_photo_count && prev != s_drift_photo_buf_b_idx)
    {
        s_drift_photo_buf_b_idx = prev;
        if (decode_photo_drift_tile(prev, s_drift_photo_pixels_b))
        {
            s_drift_photo_bmp_b.width = SZ;
            s_drift_photo_bmp_b.height = SZ;
            s_drift_photo_bmp_b.data = (char *)s_drift_photo_pixels_b;
            s_drift_photo_images[prev].bmp = &s_drift_photo_bmp_b;
        }
        else
        {
            s_drift_photo_images[prev].bmp = NULL;
        }
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

void aura_photos_draw(aura_nav_t *nav)
{
    int i;
    static aura_list_item_t items[MAX_PHOTOS];

    ensure_photo_list();

    if (s_photo_count == 0)
    {
        /* AUDITORIA-01 A-15: "la barra nunca cambia de forma entre
         * pantallas" (doc SS5) -- este vacio era la unica pantalla del
         * sistema sin ella. */
        a26_shell_clear_screen();
        aura_status_bar_v2_draw_auto(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_PHOTOS));
        draw_message(AURA_STR_EMPTY_PHOTOS);
        return;
    }

    for (i = 0; i < s_photo_count; i++)
    {
        items[i].label = s_photos[i].display;
        /* Sin icono por fila (AUDITORIA-01 A-06, anti-patron SS8: "icono
         * repetido sin variacion entre hermanos") -- el original tampoco
         * lo tiene en listas de contenido; una miniatura real por foto
         * es mejora futura, no un icono generico repetido N veces. */
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
        items[i].dimmed = 0;
    }
    ensure_drift_photos_decoded();

    aura_widgets_draw_list_with_art(aura_str(AURA_STR_PHOTOS), items, s_photo_count,
                                     aura_nav_get_selection(nav),
                                     s_drift_photo_images, s_photo_count);
}

void aura_photos_handle_button(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < s_photo_count - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        if (s_photo_count > 0)
        {
            s_current_index = sel;
            aura_nav_push(nav, AURA_SCREEN_PHOTO_VIEWER);
        }
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}

static void load_current_photo(void)
{
    char path[MAX_PATH];
    int format = FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT;
    int ret;

    if (s_loaded_index == s_current_index)
        return;
    s_loaded_index = s_current_index;
    s_loaded_ok = false;

    if (!s_photos[s_current_index].supported)
        return;

    snprintf(path, sizeof(path), "%s/%s", PHOTOS_DIR, s_photos[s_current_index].filename);

    s_bm.width = A26_SCREEN_WIDTH;
    s_bm.height = A26_SCREEN_HEIGHT;
    s_bm.data = (char *)s_view_scratch;
#if (LCD_DEPTH > 1)
    s_bm.maskdata = NULL;
#endif

    if (has_ext(path, ".bmp"))
        ret = read_bmp_file(path, &s_bm, sizeof(s_view_scratch), format, NULL);
    else
        ret = read_jpeg_file(path, &s_bm, sizeof(s_view_scratch), format, NULL);

    s_loaded_ok = (ret > 0);
}

void aura_photo_viewer_draw(aura_nav_t *nav)
{
    (void)nav;

    a26_shell_clear_screen();

    if (s_photo_count == 0)
        return;

    load_current_photo();

    if (!s_photos[s_current_index].supported || !s_loaded_ok)
    {
        draw_message(AURA_STR_UNSUPPORTED_FORMAT);
        return;
    }

    lcd_bitmap((const fb_data *)s_view_scratch,
               (A26_SCREEN_WIDTH - s_bm.width) / 2,
               (A26_SCREEN_HEIGHT - s_bm.height) / 2,
               s_bm.width, s_bm.height);
}

void aura_photo_viewer_handle_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_RIGHT:
        if (s_current_index < s_photo_count - 1)
            s_current_index++;
        break;
    case BUTTON_LEFT:
        if (s_current_index > 0)
            s_current_index--;
        break;
    case BUTTON_MENU:
    case BUTTON_SELECT:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
