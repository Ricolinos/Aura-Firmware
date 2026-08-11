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

#define PHOTOS_DIR      "/Photos"
#define MAX_PHOTOS      200
#define PHOTO_NAME_LEN  64

/* FORMAT_RESIZE necesita bastante mas que el bitmap final (D-026 en
 * DECISIONS.md); a pantalla completa (320x240x2) mas margen de sobra. */
#define VIEW_SCRATCH_SIZE (240 * 1024)

typedef struct {
    char filename[PHOTO_NAME_LEN];
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
        s_photos[s_photo_count].supported = is_supported_image(entry->d_name);
        s_photo_count++;
    }
    closedir(d);
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
        a26_shell_clear_screen();
        draw_message(AURA_STR_EMPTY_PHOTOS);
        return;
    }

    for (i = 0; i < s_photo_count; i++)
    {
        items[i].label = s_photos[i].filename;
        items[i].icon_name = "image";
        items[i].checked = 0;
    }
    aura_widgets_draw_list(aura_str(AURA_STR_PHOTOS), items, s_photo_count,
                            aura_nav_get_selection(nav));
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
