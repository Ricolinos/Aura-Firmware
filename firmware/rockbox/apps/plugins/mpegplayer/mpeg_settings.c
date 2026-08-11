#include "plugin.h"
#include "lib/helper.h"
#include "lib/configfile.h"

#include "mpegplayer.h"
#include "mpeg_settings.h"

struct mpeg_settings settings;

#define THUMB_DELAY (75*HZ/100)

/* button definitions */
#if (CONFIG_KEYPAD == IRIVER_H100_PAD) || \
    (CONFIG_KEYPAD == IRIVER_H300_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_ON
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_OFF

#elif (CONFIG_KEYPAD == IAUDIO_X5M5_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == IPOD_4G_PAD) || \
      (CONFIG_KEYPAD == IPOD_3G_PAD) || \
      (CONFIG_KEYPAD == IPOD_1G2G_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_SCROLL_FWD
#define MPEG_START_TIME_DOWN        BUTTON_SCROLL_BACK
#define MPEG_START_TIME_EXIT        BUTTON_MENU

#elif CONFIG_KEYPAD == GIGABEAT_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#define MPEG_START_TIME_RC_SELECT   (BUTTON_RC_PLAY | BUTTON_REL)
#define MPEG_START_TIME_RC_LEFT     BUTTON_RC_REW
#define MPEG_START_TIME_RC_RIGHT    BUTTON_RC_FF
#define MPEG_START_TIME_RC_UP       BUTTON_RC_VOL_UP
#define MPEG_START_TIME_RC_DOWN     BUTTON_RC_VOL_DOWN
#define MPEG_START_TIME_RC_EXIT     (BUTTON_RC_PLAY | BUTTON_REPEAT)

#elif CONFIG_KEYPAD == GIGABEAT_S_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#define MPEG_START_TIME_RC_SELECT   (BUTTON_RC_PLAY | BUTTON_REL)
#define MPEG_START_TIME_RC_LEFT     BUTTON_RC_REW
#define MPEG_START_TIME_RC_RIGHT    BUTTON_RC_FF
#define MPEG_START_TIME_RC_UP       BUTTON_RC_VOL_UP
#define MPEG_START_TIME_RC_DOWN     BUTTON_RC_VOL_DOWN
#define MPEG_START_TIME_RC_EXIT     (BUTTON_RC_PLAY | BUTTON_REPEAT)

#elif CONFIG_KEYPAD == IRIVER_H10_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_SCROLL_UP
#define MPEG_START_TIME_DOWN        BUTTON_SCROLL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == SANSA_E200_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_SCROLL_BACK
#define MPEG_START_TIME_RIGHT2      BUTTON_SCROLL_FWD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == SANSA_FUZE_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_SCROLL_BACK
#define MPEG_START_TIME_RIGHT2      BUTTON_SCROLL_FWD
#define MPEG_START_TIME_EXIT        (BUTTON_HOME|BUTTON_REPEAT)

#elif (CONFIG_KEYPAD == SANSA_C200_PAD) || \
(CONFIG_KEYPAD == SANSA_CLIP_PAD) || \
(CONFIG_KEYPAD == SANSA_M200_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == MROBE500_PAD
#define MPEG_START_TIME_SELECT      BUTTON_RC_HEART
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_RC_PLAY
#define MPEG_START_TIME_DOWN        BUTTON_RC_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_RC_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_RC_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == MROBE100_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_PLAY
#define MPEG_START_TIME_RIGHT2      BUTTON_MENU
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == IAUDIO_M3_PAD
#define MPEG_START_TIME_SELECT      BUTTON_RC_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_RC_REW
#define MPEG_START_TIME_RIGHT       BUTTON_RC_FF
#define MPEG_START_TIME_UP          BUTTON_RC_VOL_UP
#define MPEG_START_TIME_DOWN        BUTTON_RC_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_RC_REC

#elif CONFIG_KEYPAD == COWON_D2_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == CREATIVE_ZENXFI3_PAD)
#define MPEG_START_TIME_SELECT      (BUTTON_PLAY|BUTTON_REL)
#define MPEG_START_TIME_LEFT        BUTTON_BACK
#define MPEG_START_TIME_RIGHT       BUTTON_MENU
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        (BUTTON_PLAY|BUTTON_REPEAT)

#elif CONFIG_KEYPAD == PHILIPS_HDD1630_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == PHILIPS_HDD6330_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == PHILIPS_SA9200_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == ONDAVX747_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == ONDAVX777_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif (CONFIG_KEYPAD == SAMSUNG_YH820_PAD) || \
      (CONFIG_KEYPAD == SAMSUNG_YH92X_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_REW

#elif CONFIG_KEYPAD == PBELL_VIBE500_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_LEFT2       BUTTON_OK
#define MPEG_START_TIME_RIGHT2      BUTTON_CANCEL
#define MPEG_START_TIME_EXIT        BUTTON_REC

#elif CONFIG_KEYPAD == MPIO_HD200_PAD
#define MPEG_START_TIME_SELECT      BUTTON_FUNC
#define MPEG_START_TIME_LEFT        BUTTON_REW
#define MPEG_START_TIME_RIGHT       BUTTON_FF
#define MPEG_START_TIME_UP          BUTTON_VOL_UP
#define MPEG_START_TIME_DOWN        BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_REC

#elif CONFIG_KEYPAD == MPIO_HD300_PAD
#define MPEG_START_TIME_SELECT      BUTTON_ENTER
#define MPEG_START_TIME_LEFT        BUTTON_REW
#define MPEG_START_TIME_RIGHT       BUTTON_FF
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_REC

#elif CONFIG_KEYPAD == SANSA_FUZEPLUS_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == SANSA_CONNECT_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == SAMSUNG_YPR0_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif (CONFIG_KEYPAD == HM60X_PAD) || (CONFIG_KEYPAD == HM801_PAD)
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == SONY_NWZ_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif CONFIG_KEYPAD == CREATIVE_ZEN_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif CONFIG_KEYPAD == DX50_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_VOL_UP
#define MPEG_START_TIME_DOWN        BUTTON_VOL_DOWN

#elif CONFIG_KEYPAD == CREATIVE_ZENXFI2_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == AGPTEK_ROCKER_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == XDUOO_X3_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_HOME
#define MPEG_START_TIME_DOWN        BUTTON_OPTION
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == XDUOO_X3II_PAD || CONFIG_KEYPAD == XDUOO_X20_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_HOME
#define MPEG_START_TIME_DOWN        BUTTON_OPTION
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == FIIO_M3K_LINUX_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_PREV
#define MPEG_START_TIME_RIGHT       BUTTON_NEXT
#define MPEG_START_TIME_UP          BUTTON_HOME
#define MPEG_START_TIME_DOWN        BUTTON_OPTION
#define MPEG_START_TIME_LEFT2       BUTTON_VOL_UP
#define MPEG_START_TIME_RIGHT2      BUTTON_VOL_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == IHIFI_770_PAD || CONFIG_KEYPAD == IHIFI_800_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_HOME
#define MPEG_START_TIME_RIGHT       BUTTON_VOL_DOWN
#define MPEG_START_TIME_UP          BUTTON_PREV
#define MPEG_START_TIME_DOWN        BUTTON_NEXT
#define MPEG_START_TIME_LEFT2       (BUTTON_POWER + BUTTON_HOME)
#define MPEG_START_TIME_RIGHT2      (BUTTON_POWER + BUTTON_VOL_DOWN)
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == EROSQ_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_SCROLL_BACK
#define MPEG_START_TIME_RIGHT       BUTTON_SCROLL_FWD
#define MPEG_START_TIME_UP          BUTTON_PREV
#define MPEG_START_TIME_DOWN        BUTTON_NEXT
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == FIIO_M3K_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == MA_PAD
#define MPEG_START_TIME_SELECT      BUTTON_PLAY
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif CONFIG_KEYPAD == SHANLING_Q1_PAD || CONFIG_KEYPAD == HIBY_R3PROII_PAD
#define MPEG_START_TIME_EXIT        BUTTON_POWER

#elif CONFIG_KEYPAD == RG_NANO_PAD
#define MPEG_START_TIME_SELECT      BUTTON_A
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_LEFT2       BUTTON_L
#define MPEG_START_TIME_RIGHT2      BUTTON_R
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_START

#elif CONFIG_KEYPAD == CTRU_PAD
#define MPEG_START_TIME_SELECT      BUTTON_SELECT
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_BACK

#elif CONFIG_KEYPAD == ECHO_R1_PAD
#define MPEG_START_TIME_SELECT      BUTTON_A
#define MPEG_START_TIME_LEFT        BUTTON_LEFT
#define MPEG_START_TIME_RIGHT       BUTTON_RIGHT
#define MPEG_START_TIME_UP          BUTTON_UP
#define MPEG_START_TIME_DOWN        BUTTON_DOWN
#define MPEG_START_TIME_EXIT        BUTTON_B

#else
#error No keymap defined!
#endif

#ifdef HAVE_TOUCHSCREEN
#ifndef MPEG_START_TIME_SELECT
#define MPEG_START_TIME_SELECT      BUTTON_CENTER
#endif
#ifndef MPEG_START_TIME_LEFT
#define MPEG_START_TIME_LEFT        BUTTON_MIDLEFT
#endif
#ifndef MPEG_START_TIME_RIGHT
#define MPEG_START_TIME_RIGHT       BUTTON_MIDRIGHT
#endif
#ifndef MPEG_START_TIME_UP
#define MPEG_START_TIME_UP          BUTTON_TOPMIDDLE
#endif
#ifndef MPEG_START_TIME_DOWN
#define MPEG_START_TIME_DOWN        BUTTON_BOTTOMMIDDLE
#endif
#ifndef MPEG_START_TIME_LEFT2
#define MPEG_START_TIME_LEFT2       BUTTON_TOPRIGHT
#endif
#ifndef MPEG_START_TIME_RIGHT2
#define MPEG_START_TIME_RIGHT2      BUTTON_TOPLEFT
#endif
#ifndef MPEG_START_TIME_EXIT
#define MPEG_START_TIME_EXIT        BUTTON_TOPLEFT
#endif
#endif

static struct configdata config[] =
{
    {TYPE_INT, 0, 2, { .int_p = &settings.showfps }, "Show FPS", NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.limitfps }, "Limit FPS", NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.skipframes }, "Skip frames", NULL},
    {TYPE_INT, 0, INT_MAX, { .int_p = &settings.resume_count }, "Resume count",
     NULL},
    {TYPE_INT, 0, MPEG_RESUME_NUM_OPTIONS,
     { .int_p = &settings.resume_options }, "Resume options", NULL},
#if MPEG_OPTION_DITHERING_ENABLED
    {TYPE_INT, 0, INT_MAX, { .int_p = &settings.displayoptions },
     "Display options", NULL},
#endif
    {TYPE_INT, 0, 2, { .int_p = &settings.tone_controls }, "Tone controls",
     NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.channel_modes }, "Channel modes",
     NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.crossfeed }, "Crossfeed", NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.equalizer }, "Equalizer", NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.dithering }, "Dithering", NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.play_mode }, "Play mode", NULL},
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    {TYPE_INT, -1, INT_MAX, { .int_p = &settings.backlight_brightness },
     "Backlight brightness", NULL},
#endif
};

static const struct opt_items noyes[2] = {
    { STR(LANG_SET_BOOL_NO) },
    { STR(LANG_SET_BOOL_YES) },
};

static const struct opt_items singleall[2] = {
    { STR(LANG_SINGLE) },
    { STR(LANG_ALL) },
};

static const struct opt_items globaloff[2] = {
    { STR(LANG_OFF) },
    { STR(LANG_USE_SOUND_SETTING) },
};

static void mpeg_settings(void);
static bool mpeg_set_option(const char* string,
                            void* variable,
                            enum optiontype type,
                            const struct opt_items* options,
                            int numoptions,
                            void (*function)(int))
{
    mpeg_sysevent_clear();

    /* This eats SYS_POWEROFF - :\ */
    bool usb = rb->set_option(string, variable, type, options, numoptions,
                              function);

    if (usb)
        mpeg_sysevent_set();

    return usb;
}

#ifdef HAVE_BACKLIGHT_BRIGHTNESS /* Only used for this atm */
static bool mpeg_set_int(const char *string, const char *unit,
                         int voice_unit, const int *variable,
                         void (*function)(int), int step,
                         int min,
                         int max,
                         const char* (*formatter)(char*, size_t, int, const char*),
                         int32_t (*get_talk_id)(int, int))
{
    mpeg_sysevent_clear();

    bool usb = rb->set_int_ex(string, unit, voice_unit, variable, function,
                           step, min, max, formatter, get_talk_id);

    if (usb)
        mpeg_sysevent_set();

    return usb;
}

static int32_t backlight_brightness_getlang(int value, int unit)
{
    if (value < 0)
        return LANG_USE_COMMON_SETTING;

    return TALK_ID(value + MIN_BRIGHTNESS_SETTING, unit);
}

void mpeg_backlight_update_brightness(int value)
{
    if (value >= 0)
    {
        value += MIN_BRIGHTNESS_SETTING;
        backlight_brightness_set(value);
    }
    else
    {
        backlight_brightness_use_setting();
    }
}

static void backlight_brightness_function(int value)
{
    mpeg_backlight_update_brightness(value);
}

static const char* backlight_brightness_formatter(char *buf, size_t length,
                                                  int value, const char *input)
{
    (void)input;

    if (value < 0)
        return rb->str(LANG_USE_COMMON_SETTING);
    else
        rb->snprintf(buf, length, "%d", value + MIN_BRIGHTNESS_SETTING);
    return buf;
}
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */

/* Sync a particular audio setting to global or mpegplayer forced off */
static void sync_audio_setting(int setting, bool global)
{
    switch (setting)
    {
    case MPEG_AUDIO_TONE_CONTROLS:
    #ifdef AUDIOHW_HAVE_BASS
        rb->sound_set(SOUND_BASS, (global || settings.tone_controls)
            ? rb->global_settings->bass
            : rb->sound_default(SOUND_BASS));
    #endif
    #ifdef AUDIOHW_HAVE_TREBLE
        rb->sound_set(SOUND_TREBLE, (global || settings.tone_controls)
            ? rb->global_settings->treble
            : rb->sound_default(SOUND_TREBLE));
    #endif

    #ifdef AUDIOHW_HAVE_EQ
        for (int band = 0;; band++)
        {
            int setting = rb->sound_enum_hw_eq_band_setting(band, AUDIOHW_EQ_GAIN);

            if (setting == -1)
                break;

            rb->sound_set(setting, (global || settings.tone_controls)
                    ? rb->global_settings->hw_eq_bands[band].gain
                    : rb->sound_default(setting));
        }
    #endif /* AUDIOHW_HAVE_EQ */
        break;

    case MPEG_AUDIO_CHANNEL_MODES:
        rb->sound_set(SOUND_CHANNELS, (global || settings.channel_modes)
                ? rb->global_settings->channel_config
                : SOUND_CHAN_STEREO);
        break;

    case MPEG_AUDIO_CROSSFEED:
        rb->dsp_set_crossfeed_type((global || settings.crossfeed) ?
                                   rb->global_settings->crossfeed :
                                   CROSSFEED_TYPE_NONE);
        break;

    case MPEG_AUDIO_EQUALIZER:
        rb->dsp_eq_enable((global || settings.equalizer) ?
                          rb->global_settings->eq_enabled : false);
        break;

    case MPEG_AUDIO_DITHERING:
        rb->dsp_dither_enable((global || settings.dithering) ?
                              rb->global_settings->dithering_enabled : false);
       break;
    }
}

/* Sync all audio settings to global or mpegplayer forced off */
static void sync_audio_settings(bool global)
{
    static const int setting_index[] =
    {
        MPEG_AUDIO_TONE_CONTROLS,
        MPEG_AUDIO_CHANNEL_MODES,
        MPEG_AUDIO_CROSSFEED,
        MPEG_AUDIO_EQUALIZER,
        MPEG_AUDIO_DITHERING,
    };
    unsigned i;

    for (i = 0; i < ARRAYLEN(setting_index); i++)
    {
        sync_audio_setting(setting_index[i], global);
    }
}

#ifndef HAVE_LCD_COLOR
/* Cheapo splash implementation for the grey surface */
static void grey_splash(int ticks, const unsigned char *fmt, ...)
{
    unsigned char buffer[256];
    int x, y, w, h;
    int oldfg, oldmode;

    va_list ap;
    va_start(ap, fmt);

    rb->vsnprintf(buffer, sizeof (buffer), fmt, ap);

    va_end(ap);

    grey_getstringsize(buffer, &w, &h);

    oldfg = grey_get_foreground();
    oldmode = grey_get_drawmode();

    grey_set_drawmode(DRMODE_FG);
    grey_set_foreground(GREY_LIGHTGRAY);

    x = (LCD_WIDTH - w) / 2;
    y = (LCD_HEIGHT - h) / 2;

    grey_fillrect(x - 1, y - 1, w + 2, h + 2);

    grey_set_foreground(GREY_BLACK);

    grey_putsxy(x, y, buffer);
    grey_drawrect(x - 2, y - 2, w + 4, h + 4);

    grey_set_foreground(oldfg);
    grey_set_drawmode(oldmode);

    grey_update();

    if (ticks > 0)
        rb->sleep(ticks);
}
#endif /* !HAVE_LCD_COLOR */

/* Fase 20 (PLAN-UX.md) / D-06x: show_loading()/draw_slider()/
 * display_thumb_image()/increment_time() tambien se removieron --
 * solo las usaba get_start_time(), ya eliminada arriba. */


/* Fase 20 (PLAN-UX.md) / D-06x: se removieron get_start_time_lcd_enable_hook(),
 * get_start_time() y show_start_menu() -- codigo muerto tras eliminar el menu
 * de inicio de mpegplayer (mpeg_start_menu() ahora resuelve directo). Ver
 * DECISIONS.md. */

/* Return the desired resume action.
 *
 * Fase 20 (PLAN-UX.md) / D-06x: Aura entra directo reproduciendo (o
 * retomando desde donde quedo), sin el menu "MPEG Player: Play from
 * beginning / Resume Playback / Set resume time / Settings / Quit"
 * que se veia siempre antes de cada video (inventario de la Fase 12,
 * superficie de nivel 1) -- Aura no tiene un equivalente propio de
 * ese menu ni quiere exponerlo. Se ignora settings.resume_options a
 * proposito (en vez de solo cambiar su valor por default): asi el
 * comportamiento es correcto incluso si el .rockbox/mpegplayer.cfg
 * del dispositivo ya tenia guardado MPEG_RESUME_MENU_ALWAYS de una
 * instalacion previa de Rockbox. MPEG_START_SEEK a resume_time=0 (caso
 * de un video nunca visto) reproduce igual desde el principio. */
int mpeg_start_menu(uint32_t duration)
{
    (void)duration;
    mpeg_sysevent_clear();
    return MPEG_START_SEEK;
}

int mpeg_menu(void)
{
    int result;

    MENUITEM_STRINGLIST(menu, "MPEG Player", mpeg_sysevent_callback,
                        ID2P(LANG_SETTINGS),
                        ID2P(LANG_RESUME_PLAYBACK),
                        ID2P(LANG_MENU_QUIT));

    rb->button_clear_queue();

    mpeg_sysevent_clear();

    result = rb->do_menu(&menu, NULL, NULL, false);

    switch (result)
    {
    case MPEG_MENU_SETTINGS:
        mpeg_settings();
        break;

    case MPEG_MENU_RESUME:
        break;

    case MPEG_MENU_QUIT:
        break;

    default:
        break;
    }

    if (mpeg_sysevent() != 0)
        result = MPEG_MENU_QUIT;

    return result;
}

static void display_options(void)
{
    int selected = 0;
    int result;
    bool menu_quit = false;

    MENUITEM_STRINGLIST(menu, ID2P(LANG_MENU_DISPLAY_OPTIONS), mpeg_sysevent_callback,
#if MPEG_OPTION_DITHERING_ENABLED
                        ID2P(LANG_DITHERING),
#endif
                        ID2P(LANG_DISPLAY_FPS),
                        ID2P(LANG_LIMIT_FPS),
                        ID2P(LANG_SKIP_FRAMES),
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
                        ID2P(LANG_BACKLIGHT_BRIGHTNESS),
#endif
                        );

    rb->button_clear_queue();

    while (!menu_quit)
    {
        mpeg_sysevent_clear();
        result = rb->do_menu(&menu, &selected, NULL, false);

        switch (result)
        {
#if MPEG_OPTION_DITHERING_ENABLED
        case MPEG_OPTION_DITHERING:
            result = (settings.displayoptions & LCD_YUV_DITHER) ? 1 : 0;
            mpeg_set_option(rb->str(LANG_DITHERING), &result, RB_INT, noyes, 2, NULL);
            settings.displayoptions =
                (settings.displayoptions & ~LCD_YUV_DITHER)
                      | ((result != 0) ? LCD_YUV_DITHER : 0);
            rb->lcd_yuv_set_options(settings.displayoptions);
            break;
#endif /* MPEG_OPTION_DITHERING_ENABLED */

        case MPEG_OPTION_DISPLAY_FPS:
            mpeg_set_option(rb->str(LANG_DISPLAY_FPS), &settings.showfps, RB_INT,
                            noyes, 2, NULL);
            break;

        case MPEG_OPTION_LIMIT_FPS:
            mpeg_set_option(rb->str(LANG_LIMIT_FPS), &settings.limitfps, RB_INT,
                            noyes, 2, NULL);
            break;

        case MPEG_OPTION_SKIP_FRAMES:
            mpeg_set_option(rb->str(LANG_SKIP_FRAMES), &settings.skipframes, RB_INT,
                            noyes, 2, NULL);
            break;

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
        case MPEG_OPTION_BACKLIGHT_BRIGHTNESS:
            result = settings.backlight_brightness;
            mpeg_backlight_update_brightness(result);
            mpeg_set_int(rb->str(LANG_BACKLIGHT_BRIGHTNESS), NULL, UNIT_INT, &result,
                         backlight_brightness_function, 1, -1,
                         MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING,
                         backlight_brightness_formatter,
                         backlight_brightness_getlang);
            settings.backlight_brightness = result;
            mpeg_backlight_update_brightness(-1);
            break;
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */

        default:
            menu_quit = true;
            break;
        }

        if (mpeg_sysevent() != 0)
            menu_quit = true;
    }
}

static void audio_options(void)
{
    int selected = 0;
    int result;
    bool menu_quit = false;

    MENUITEM_STRINGLIST(menu, ID2P(LANG_MENU_AUDIO_OPTIONS), mpeg_sysevent_callback,
                        ID2P(LANG_TONE_CONTROLS),
                        ID2P(LANG_CHANNEL_CONFIGURATION),
                        ID2P(LANG_CROSSFEED),
                        ID2P(LANG_EQUALIZER),
                        ID2P(LANG_DITHERING));

    rb->button_clear_queue();

    while (!menu_quit)
    {
        mpeg_sysevent_clear();
        result = rb->do_menu(&menu, &selected, NULL, false);

        switch (result)
        {
        case MPEG_AUDIO_TONE_CONTROLS:
            mpeg_set_option(rb->str(LANG_TONE_CONTROLS), &settings.tone_controls, RB_INT,
                            globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        case MPEG_AUDIO_CHANNEL_MODES:
            mpeg_set_option(rb->str(LANG_CHANNEL_CONFIGURATION), &settings.channel_modes,
                            RB_INT, globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        case MPEG_AUDIO_CROSSFEED:
            mpeg_set_option(rb->str(LANG_CROSSFEED), &settings.crossfeed, RB_INT,
                            globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        case MPEG_AUDIO_EQUALIZER:
            mpeg_set_option(rb->str(LANG_EQUALIZER), &settings.equalizer, RB_INT,
                            globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        case MPEG_AUDIO_DITHERING:
            mpeg_set_option(rb->str(LANG_DITHERING), &settings.dithering, RB_INT,
                            globaloff, 2, NULL);
            sync_audio_setting(result, false);
            break;

        default:
            menu_quit = true;
            break;
        }

        if (mpeg_sysevent() != 0)
            menu_quit = true;
    }
}

static void resume_options(void)
{
    static const struct opt_items items[MPEG_RESUME_NUM_OPTIONS] = {
        [MPEG_RESUME_MENU_ALWAYS] =
            { STR(LANG_FORCE_START_MENU) },
        [MPEG_RESUME_MENU_IF_INCOMPLETE] =
            { STR(LANG_CONDITIONAL_START_MENU) },
        [MPEG_RESUME_ALWAYS] =
            { STR(LANG_AUTO_RESUME) },
        [MPEG_RESUME_RESTART] =
            { STR(LANG_RESTART_PLAYBACK) },
    };

    mpeg_set_option(rb->str(LANG_MENU_RESUME_OPTIONS), &settings.resume_options,
                    RB_INT, items, MPEG_RESUME_NUM_OPTIONS, NULL);
}

static void clear_resume_count(void)
{
    settings.resume_count = 0;
    configfile_save(SETTINGS_FILENAME, config, ARRAYLEN(config),
                    SETTINGS_VERSION);
}

static void mpeg_settings(void)
{
    int selected = 0;
    int result;
    bool menu_quit = false;

    MENUITEM_STRINGLIST(menu, ID2P(LANG_SETTINGS), mpeg_sysevent_callback,
                        ID2P(LANG_MENU_DISPLAY_OPTIONS),
                        ID2P(LANG_MENU_AUDIO_OPTIONS),
                        ID2P(LANG_MENU_RESUME_OPTIONS),
                        ID2P(LANG_MENU_PLAY_MODE),
                        ID2P(LANG_CLEAR_ALL_RESUMES));

    rb->button_clear_queue();

    while (!menu_quit)
    {
        mpeg_sysevent_clear();

        result = rb->do_menu(&menu, &selected, NULL, false);

        switch (result)
        {
        case MPEG_SETTING_DISPLAY_SETTINGS:
            display_options();
            break;

        case MPEG_SETTING_AUDIO_SETTINGS:
            audio_options();
            break;

        case MPEG_SETTING_ENABLE_START_MENU:
            resume_options();
            break;

        case MPEG_SETTING_PLAY_MODE:
            mpeg_set_option(rb->str(LANG_MENU_PLAY_MODE), &settings.play_mode,
                            RB_INT, singleall, 2, NULL);
            break;

        case MPEG_SETTING_CLEAR_RESUMES:
            clear_resume_count();
            break;

        default:
            menu_quit = true;
            break;
        }

        if (mpeg_sysevent() != 0)
            menu_quit = true;
    }
}

void init_settings(const char* filename)
{
    /* Set the default settings */
    settings.showfps = 0;     /* Do not show FPS */
    settings.limitfps = 1;    /* Limit FPS */
    settings.skipframes = 1;  /* Skip frames */
    settings.play_mode = 0;   /* Play single video */
    settings.resume_options = MPEG_RESUME_MENU_ALWAYS; /* Enable start menu */
    settings.resume_count = 0;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    settings.backlight_brightness = -1; /* Use default setting */
#endif
#if MPEG_OPTION_DITHERING_ENABLED
    settings.displayoptions = 0; /* No visual effects */
#endif
    settings.tone_controls = false;
    settings.channel_modes = false;
    settings.crossfeed = false;
    settings.equalizer = false;
    settings.dithering = false;

    if (configfile_load(SETTINGS_FILENAME, config, ARRAYLEN(config),
                        SETTINGS_MIN_VERSION) < 0)
    {
        /* Generate a new config file with default values */
        configfile_save(SETTINGS_FILENAME, config, ARRAYLEN(config),
                        SETTINGS_VERSION);
    }

    rb->strlcpy(settings.resume_filename, filename, MAX_PATH);

    /* get the resume time for the current mpeg if it exists */
    if ((settings.resume_time = configfile_get_value
         (SETTINGS_FILENAME, filename)) < 0)
    {
        settings.resume_time = 0;
    }

#if MPEG_OPTION_DITHERING_ENABLED
    rb->lcd_yuv_set_options(settings.displayoptions);
#endif

    /* Set our audio options */
    sync_audio_settings(false);
}

void save_settings(void)
{
    unsigned i;
    for (i = 0; i < ARRAYLEN(config); i++)
    {
        configfile_update_entry(SETTINGS_FILENAME, config[i].name,
                                *(config[i].int_p));
    }

    /* If this was a new resume entry then update the total resume count */
    if (configfile_update_entry(SETTINGS_FILENAME, settings.resume_filename,
                                settings.resume_time) == 0)
    {
        configfile_update_entry(SETTINGS_FILENAME, "Resume count",
                                ++settings.resume_count);
    }

    /* Restore audio options */
    sync_audio_settings(true);
}
