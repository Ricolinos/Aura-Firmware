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
#include "apple2026_tokens.h"

#define AURA_DIR       ROCKBOX_DIR "/aura"
#define AURA_CFG_PATH  AURA_DIR "/aura.cfg"

aura_settings_t aura_settings;

static const aura_settings_t aura_settings_defaults = {
    .theme = AURA_THEME_DARK,
    .animation_mode = AURA_ANIM_MINIMAL,
    .graphics_mode = AURA_GFX_MINIMAL,
    .eq_preset = AURA_EQ_FLAT,
    .language = AURA_LANG_ES,
    .show_videos = true,
    .show_photos = true,
    .show_nowplaying = true,
    /* AURA_DS_COLOR_ACCENT_DEFAULT_RGB24 (design-system/tokens.json,
     * generado -- 0xFF2D52, fundamentos/01-color.md). */
    .accent_rgb24 = AURA_DS_COLOR_ACCENT_DEFAULT_RGB24,
    /* "Maxima fidelidad primero" (00-INDICE.md): la sombra activada ES
     * el comportamiento base documentado, el toggle solo la apaga. */
    .left_panel_shadow = true,
    .show_icons = true,
    .tz_local_quarters = -24,
    .audiobook_speed = 1,
    .sort_by_lastname = 0,
    .root_shortcuts = 0,
    .clock_visible = true,
    /* Sin clave configurada ni bloqueo activo -- estado del iPod recien
     * salido de la caja (D-197). */
    .screen_lock_pin = 0,
    .screen_lock_configured = false,
    .screen_lock_active = false,
};

/* Ganancia (dB) por banda para cada preset; el resto de cada banda
 * (tipo/cutoff/q) se toma de eq_defaults[], la misma tabla que usa el
 * propio menu de EQ de Rockbox (apps/settings_list.c), para no inventar
 * valores de filtro propios. Ver D-012 en DECISIONS.md. */
/* Ganancia por banda en DECIMAS de dB, tabla del ecualizador del
 * firmware original (2026-08-13). Las 10 bandas son las de Rockbox
 * (32/64/125/250/500/1k/2k/4k/8k/16k Hz); `precut` es la atenuacion
 * previa que evita recorte cuando el preset sube bandas. */
static const struct {
    int gain[EQ_NUM_BANDS];
    int precut;
} aura_eq_presets[AURA_EQ_COUNT] = {
    [AURA_EQ_OFF] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
    [AURA_EQ_ACOUSTIC] = { { 45, 45, 10, 10, 15, 15, 30, 30, 20, 20 }, 45 },
    [AURA_EQ_BASS_BOOST] = { { 50, 50, 35, 35, 15, 15, 5, 5, -5, -5 }, 50 },
    [AURA_EQ_BASS_RED] = { { -50, -50, -35, -35, -15, -15, -5, -5, 5, 5 }, 0 },
    [AURA_EQ_CLASSICAL] = { { 0, 0, 0, 0, 0, 0, 0, -70, -70, -70 }, 0 },
    [AURA_EQ_DANCE] = { { 95, 70, 25, 0, 0, -55, -70, -70, 0, 0 }, 95 },
    [AURA_EQ_DEEP] = { { 50, 45, 25, 10, 15, 10, -15, -30, -40, -45 }, 50 },
    [AURA_EQ_ELECTRONIC] = { { 45, 45, 5, 5, 25, 25, 15, 15, 0, 55 }, 55 },
    [AURA_EQ_FLAT] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
    [AURA_EQ_HIPHOP] = { { 65, 65, 25, 25, -10, -10, 15, 15, 35, 35 }, 65 },
    [AURA_EQ_JAZZ] = { { 40, 40, 15, 15, -25, -25, 5, 5, 60, 60 }, 60 },
    [AURA_EQ_LATIN] = { { 50, 40, 0, 0, -20, -20, 0, 0, 40, 50 }, 50 },
    [AURA_EQ_LOUDNESS] = { { 70, 60, 0, 0, -20, 0, -30, 0, 60, 70 }, 70 },
    [AURA_EQ_LOUNGE] = { { -25, -25, 5, 5, 20, 20, -15, -15, 15, 15 }, 20 },
    [AURA_EQ_PIANO] = { { 30, 20, 0, 20, 25, 15, 30, 35, 25, 20 }, 35 },
    [AURA_EQ_POP] = { { -15, 50, 70, 80, 55, 0, -25, -25, 15, 15 }, 80 },
    [AURA_EQ_RNB] = { { 35, 35, 45, 45, 5, 5, 25, 25, 30, 30 }, 45 },
    [AURA_EQ_ROCK] = { { 80, 50, -55, -80, -30, 40, 90, 110, 110, 110 }, 110 },
    [AURA_EQ_SMALLSPK] = { { 70, 60, 40, 20, 0, -10, -20, -30, -40, -50 }, 70 },
    [AURA_EQ_SPOKEN] = { { -45, -45, 5, 5, 45, 45, 20, 20, 0, 0 }, 45 },
    [AURA_EQ_TREBLE_BOOST] = { { 0, 0, 0, 0, 0, 0, 20, 40, 60, 70 }, 70 },
    [AURA_EQ_TREBLE_RED] = { { 0, 0, 0, 0, 0, -10, -20, -40, -60, -70 }, 0 },
    [AURA_EQ_VOCAL_BOOST] = { { -20, -20, -10, 20, 40, 40, 30, 10, -10, -20 }, 40 },
};


static int clamp_enum(int value, int count)
{
    if (value < 0 || value >= count)
        return 0;
    return value;
}

const int *aura_settings_eq_preset_gains(int preset)
{
    return aura_eq_presets[clamp_enum(preset, AURA_EQ_COUNT)].gain;
}

void aura_settings_apply_eq(void)
{
    int i;
    int idx = clamp_enum((int)aura_settings.eq_preset, AURA_EQ_COUNT);
    const int *gains = aura_eq_presets[idx].gain;

    for (i = 0; i < EQ_NUM_BANDS; i++)
    {
        global_settings.eq_band_settings[i] = eq_defaults[i];
        /* La tabla esta en decimas de dB, que es justo la unidad de
         * eq_band_setting.gain -- se copia tal cual. */
        global_settings.eq_band_settings[i].gain = gains[i];
    }
    global_settings.eq_precut = aura_eq_presets[idx].precut;
    /* "Desactivado" y "Flat" son la curva plana: el DSP se apaga en
     * vez de procesar 10 filtros a 0 dB (misma regla que usa Rockbox
     * por banda: ganancia 0 = banda apagada). */
    global_settings.eq_enabled = (idx != AURA_EQ_OFF && idx != AURA_EQ_FLAT);

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
    /* D-196: reporte del dueno en hardware real -- "el sonido del
     * clicker tampoco esta funcionando". El disparo (aura_main.c) y el
     * mixer de audio (pcm_mixer.c/beep.c) estan bien -- la causa real
     * es que `keyclick` es un CHOICE_SETTING de Rockbox con default
     * de fabrica 0 (apagado, settings_list.c), y esta funcion nunca lo
     * tocaba. El resultado: en cualquier instalacion nueva de Aura, el
     * Clicker esta apagado hasta que el usuario encuentra el ajuste y
     * lo prende a mano -- distinto al iPod original, que hace clic
     * desde que sale de la caja. 2 = "moderate" (el mismo valor que
     * usa la fila "Clicker" de Ajustes al encenderlo, aura_screens.c).
     */
    global_settings.keyclick = 2;
    backlight_set_timeout(global_settings.backlight_timeout);
    backlight_set_timeout_plugged(global_settings.backlight_timeout_plugged);
    settings_save();
}

void aura_settings_load(void)
{
    int fd;
    char line[64];
    bool has_animation_mode = false;

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
            else if (!strcmp(name, "animation_mode"))
            {
                aura_settings.animation_mode = clamp_enum(v, AURA_ANIM_COUNT);
                has_animation_mode = true;
            }
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
            else if (!strcmp(name, "accent_rgb24"))
                /* Hex (0xRRGGBB), no decimal como el resto de este
                 * archivo -- strtoul en vez de atoi(), es un color, no
                 * un indice de enum. */
                aura_settings.accent_rgb24 =
                    (unsigned)strtoul(value, NULL, 16) & 0xFFFFFFu;
            else if (!strcmp(name, "left_panel_shadow"))
                aura_settings.left_panel_shadow = (v != 0);
            else if (!strcmp(name, "show_icons"))
                aura_settings.show_icons = (v != 0);
            else if (!strcmp(name, "tz_local_quarters"))
                aura_settings.tz_local_quarters = v;
            else if (!strcmp(name, "audiobook_speed"))
                aura_settings.audiobook_speed = v;
            else if (!strcmp(name, "sort_by_lastname"))
                aura_settings.sort_by_lastname = (v != 0);
            else if (!strcmp(name, "root_shortcuts"))
                aura_settings.root_shortcuts = (unsigned)v;
            else if (!strcmp(name, "clock_visible"))
                aura_settings.clock_visible = (v != 0);
            else if (!strcmp(name, "screen_lock_pin"))
                aura_settings.screen_lock_pin = (unsigned short)(v < 0 ? 0 : v > 9999 ? 9999 : v);
            else if (!strcmp(name, "screen_lock_configured"))
                aura_settings.screen_lock_configured = (v != 0);
            else if (!strcmp(name, "screen_lock_active"))
                aura_settings.screen_lock_active = (v != 0);
        }
        close(fd);

        /* Migracion silenciosa de los aura.cfg escritos antes de que
         * "Graficos" se partiera en Animaciones + Graficos: el ajuste
         * viejo controlaba las dos cosas a la vez y sus tres valores
         * coinciden 1:1 en orden con los nuevos, asi que se adopta como
         * valor inicial de Animaciones. Sin esto, un usuario que tenia
         * todo desactivado veria las animaciones reaparecer solas. */
        if (!has_animation_mode)
            aura_settings.animation_mode = (aura_anim_mode_t)aura_settings.graphics_mode;
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
    fdprintf(fd, "animation_mode: %d\n", (int)aura_settings.animation_mode);
    fdprintf(fd, "graphics_mode: %d\n", (int)aura_settings.graphics_mode);
    fdprintf(fd, "eq_preset: %d\n", (int)aura_settings.eq_preset);
    fdprintf(fd, "language: %d\n", (int)aura_settings.language);
    fdprintf(fd, "show_videos: %d\n", (int)aura_settings.show_videos);
    fdprintf(fd, "show_photos: %d\n", (int)aura_settings.show_photos);
    fdprintf(fd, "show_nowplaying: %d\n", (int)aura_settings.show_nowplaying);
    fdprintf(fd, "accent_rgb24: %06lx\n", (unsigned long)aura_settings.accent_rgb24);
    fdprintf(fd, "left_panel_shadow: %d\n", (int)aura_settings.left_panel_shadow);
    fdprintf(fd, "show_icons: %d\n", (int)aura_settings.show_icons);
    fdprintf(fd, "tz_local_quarters: %d\n", aura_settings.tz_local_quarters);
    fdprintf(fd, "audiobook_speed: %d\n", aura_settings.audiobook_speed);
    fdprintf(fd, "sort_by_lastname: %d\n", aura_settings.sort_by_lastname);
    fdprintf(fd, "root_shortcuts: %d\n", (int)aura_settings.root_shortcuts);
    fdprintf(fd, "clock_visible: %d\n", (int)aura_settings.clock_visible);
    fdprintf(fd, "screen_lock_pin: %d\n", (int)aura_settings.screen_lock_pin);
    fdprintf(fd, "screen_lock_configured: %d\n", (int)aura_settings.screen_lock_configured);
    fdprintf(fd, "screen_lock_active: %d\n", (int)aura_settings.screen_lock_active);

    close(fd);
}
