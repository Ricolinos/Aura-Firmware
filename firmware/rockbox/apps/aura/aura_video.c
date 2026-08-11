#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "font.h"
#include "button.h"
#include "file.h"
#include "dir.h"
#include "string-extra.h"
#include "rbpaths.h"
#include "plugin.h"

#include "aura_video.h"
#include "aura_widgets.h"
#include "apple2026_shell.h"
#include "aura_settings.h"
#include "aura_lang.h"
#include "apple2026_tokens.h"
#include "aura_statusbar.h"

/* Aura solo reproduce su formato interno (MPEG-1/2 320x240, generado
 * por Aura Studio al sincronizar) delegando en el plugin mpegplayer
 * del propio fork -- portar un decoder de video propio queda fuera de
 * alcance de esta fase (D-029). plugin_load() hace toda la limpieza
 * de pantalla/viewport/fuente antes y despues por su cuenta; Aura solo
 * necesita llamarlo y redibujar su pantalla normal al volver. */

#define VIDEOS_DIR     "/Videos"
#define MAX_VIDEOS     100
#define VIDEO_NAME_LEN 64

static char s_videos[MAX_VIDEOS][VIDEO_NAME_LEN];
static char s_videos_display[MAX_VIDEOS][VIDEO_NAME_LEN]; /* sin extension, AUDITORIA-01 A-05 */
static int s_video_count = -1;

static bool has_ext(const char *name, const char *ext)
{
    size_t nlen = strlen(name), elen = strlen(ext);
    return nlen > elen && !strcasecmp(name + nlen - elen, ext);
}

static bool is_video_file(const char *name)
{
    return has_ext(name, ".mpg") || has_ext(name, ".mpeg");
}

/* Nombre para mostrar sin extension de archivo (AUDITORIA-01 A-05, doc
 * de diseno Principio 7: "nunca jerga"). Solo pela la extension para la
 * lista; s_videos[] con extension sigue siendo lo que se abre. */
static void strip_ext_for_display(const char *filename, char *out, size_t outsz)
{
    char *dot;

    strlcpy(out, filename, outsz);
    dot = strrchr(out, '.');
    if (dot)
        *dot = '\0';
}

static void ensure_video_list(void)
{
    DIR *d;
    struct DIRENT *entry;

    if (s_video_count >= 0)
        return;

    s_video_count = 0;
    d = opendir(VIDEOS_DIR);
    if (!d)
        return;

    while (s_video_count < MAX_VIDEOS && (entry = readdir(d)) != NULL)
    {
        if (!is_video_file(entry->d_name))
            continue;
        strlcpy(s_videos[s_video_count], entry->d_name, VIDEO_NAME_LEN);
        strip_ext_for_display(entry->d_name, s_videos_display[s_video_count], VIDEO_NAME_LEN);
        s_video_count++;
    }
    closedir(d);
}

void aura_video_draw(aura_nav_t *nav)
{
    int i;
    static aura_list_item_t items[MAX_VIDEOS];

    ensure_video_list();
    a26_shell_clear_screen();

    if (s_video_count == 0)
    {
        int w, h;
        /* AUDITORIA-01 A-15: mismo vacio que Fotos -- la barra nunca
         * cambia de forma entre pantallas (doc SS5). */
        aura_statusbar_draw(0, A26_SCREEN_WIDTH, aura_str(AURA_STR_VIDEOS), 0);
        lcd_setfont(a26_font(A26_FONT_STYLE_BODY));
        lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
        lcd_getstringsize((const unsigned char *)aura_str(AURA_STR_EMPTY_VIDEOS), &w, &h);
        lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, (A26_SCREEN_HEIGHT - h) / 2,
                   (const unsigned char *)aura_str(AURA_STR_EMPTY_VIDEOS));
        return;
    }

    for (i = 0; i < s_video_count; i++)
    {
        items[i].label = s_videos_display[i];
        /* Sin icono por fila (AUDITORIA-01 A-06, mismo criterio que
         * Fotos): "video" repetido en cada fila es el anti-patron SS8,
         * no una decision. */
        items[i].icon_name = NULL;
        items[i].checked = 0;
        items[i].toggle = -1;
    }
    aura_widgets_draw_list(aura_str(AURA_STR_VIDEOS), items, s_video_count,
                            aura_nav_get_selection(nav));
}

void aura_video_handle_button(aura_nav_t *nav, long button)
{
    int sel = aura_nav_get_selection(nav);

    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        if (sel < s_video_count - 1)
            aura_nav_set_selection(nav, sel + 1);
        break;
    case BUTTON_SCROLL_BACK:
        if (sel > 0)
            aura_nav_set_selection(nav, sel - 1);
        break;
    case BUTTON_SELECT:
        if (s_video_count > 0)
        {
            char path[MAX_PATH];
            snprintf(path, sizeof(path), "%s/%s", VIDEOS_DIR, s_videos[sel]);
            plugin_load(VIEWERS_DIR "/mpegplayer.rock", path);
            /* plugin_load() ya restauro pantalla/viewport/fuente; el
             * proximo ciclo de aura_main() redibuja esta pantalla
             * normalmente, sin pasos extra aca. */
        }
        break;
    case BUTTON_MENU:
        aura_nav_pop(nav);
        break;
    default:
        break;
    }
}
