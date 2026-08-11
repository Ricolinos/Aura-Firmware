#include <string.h>
#include <stdlib.h>

#include "file.h"
#include "dir.h"
#include "misc.h"
#include "rbpaths.h"
#include "settings.h"
#include "eq.h"
#include "backlight.h"

#include "aura_settings.h"

#define AURA_DIR       ROCKBOX_DIR "/aura"
#define AURA_CFG_PATH  AURA_DIR "/aura.cfg"

aura_settings_t aura_settings;

static const aura_settings_t aura_settings_defaults = {
    .theme = AURA_THEME_DARK,
    .graphics_mode = AURA_GFX_MINIMAL,
    .eq_preset = AURA_EQ_FLAT,
    .language = AURA_LANG_ES,
    .show_videos = true,
    .show_photos = true,
    .show_nowplaying = true,
};

/* Ganancia (dB) por banda para cada preset; el resto de cada banda
 * (tipo/cutoff/q) se toma de eq_defaults[], la misma tabla que usa el
 * propio menu de EQ de Rockbox (apps/settings_list.c), para no inventar
 * valores de filtro propios. Ver D-012 en DECISIONS.md. */
static const int aura_eq_gain_db[AURA_EQ_COUNT][EQ_NUM_BANDS] = {
    [AURA_EQ_FLAT]         = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    [AURA_EQ_BASS_BOOST]   = { 8, 7, 5, 2, 0, 0, 0, 0, 0, 0 },
    [AURA_EQ_VOCAL]        = { -3, -2, 0, 2, 4, 4, 2, 0, -1, -2 },
    [AURA_EQ_TREBLE_BOOST] = { 0, 0, 0, 0, 0, 0, 2, 4, 6, 7 },
};

static int clamp_enum(int value, int count)
{
    if (value < 0 || value >= count)
        return 0;
    return value;
}

void aura_settings_apply_eq(void)
{
    int i;
    const int *gains = aura_eq_gain_db[aura_settings.eq_preset];

    for (i = 0; i < EQ_NUM_BANDS; i++)
    {
        global_settings.eq_band_settings[i] = eq_defaults[i];
        global_settings.eq_band_settings[i].gain = gains[i];
    }
    global_settings.eq_enabled = (aura_settings.eq_preset != AURA_EQ_FLAT);

    sound_settings_apply();
}

bool aura_settings_is_first_boot(void)
{
    return !file_exists(AURA_CFG_PATH);
}

void aura_settings_reset_to_defaults(void)
{
    aura_settings = aura_settings_defaults;
    aura_settings_apply_eq();
    aura_settings_save();
}

void aura_settings_apply_core_defaults(void)
{
    global_settings.volume_limit = -6;
    global_settings.poweroff = 30;
    global_settings.backlight_timeout = 10;
    global_settings.backlight_timeout_plugged = 30;
    global_settings.sleeptimer_duration = 0;
    backlight_set_timeout(global_settings.backlight_timeout);
    backlight_set_timeout_plugged(global_settings.backlight_timeout_plugged);
    settings_save();
}

void aura_settings_load(void)
{
    int fd;
    char line[64];

    aura_settings = aura_settings_defaults;

    fd = open(AURA_CFG_PATH, O_RDONLY);
    if (fd >= 0)
    {
        while (read_line(fd, line, sizeof(line)) > 0)
        {
            char *name, *value;
            if (!settings_parseline(line, &name, &value))
                continue;

            int v = atoi(value);
            if (!strcmp(name, "theme"))
                aura_settings.theme = clamp_enum(v, AURA_THEME_COUNT);
            else if (!strcmp(name, "graphics_mode"))
                aura_settings.graphics_mode = clamp_enum(v, AURA_GFX_COUNT);
            else if (!strcmp(name, "eq_preset"))
                aura_settings.eq_preset = clamp_enum(v, AURA_EQ_COUNT);
            else if (!strcmp(name, "language"))
                aura_settings.language = clamp_enum(v, AURA_LANG_COUNT);
            else if (!strcmp(name, "show_videos"))
                aura_settings.show_videos = (v != 0);
            else if (!strcmp(name, "show_photos"))
                aura_settings.show_photos = (v != 0);
            else if (!strcmp(name, "show_nowplaying"))
                aura_settings.show_nowplaying = (v != 0);
        }
        close(fd);
    }

    aura_settings_apply_eq();
}

void aura_settings_save(void)
{
    int fd;

    if (!dir_exists(AURA_DIR))
        mkdir(AURA_DIR);

    fd = creat(AURA_CFG_PATH, 0666);
    if (fd < 0)
        return;

    fdprintf(fd, "theme: %d\n", (int)aura_settings.theme);
    fdprintf(fd, "graphics_mode: %d\n", (int)aura_settings.graphics_mode);
    fdprintf(fd, "eq_preset: %d\n", (int)aura_settings.eq_preset);
    fdprintf(fd, "language: %d\n", (int)aura_settings.language);
    fdprintf(fd, "show_videos: %d\n", (int)aura_settings.show_videos);
    fdprintf(fd, "show_photos: %d\n", (int)aura_settings.show_photos);
    fdprintf(fd, "show_nowplaying: %d\n", (int)aura_settings.show_nowplaying);

    close(fd);
}
