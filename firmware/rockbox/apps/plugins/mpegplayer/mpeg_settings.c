#include "plugin.h"
#include "lib/helper.h"
#include "lib/configfile.h"

#include "mpegplayer.h"
#include "mpeg_settings.h"

struct mpeg_settings settings;

#define THUMB_DELAY (75*HZ/100)

/* Aura (D-307): se elimino el bloque de defines MPEG_START_TIME_* por
 * target (~400 lineas) -- quedo muerto desde D-062 (get_start_time()/
 * show_start_menu() ya no existen, eran sus unicos usos), confirmado
 * sin ningun otro llamador en todo el arbol antes de borrarlo. */

static struct configdata config[] =
{
    {TYPE_INT, 0, 2, { .int_p = &settings.showfps }, "Show FPS", NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.limitfps }, "Limit FPS", NULL},
    {TYPE_INT, 0, 2, { .int_p = &settings.skipframes }, "Skip frames", NULL},
    {TYPE_INT, 0, 1, { .int_p = &settings.scale_mode }, "Scale mode", NULL},
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

/* Aura (D-013/D-304): rb->str(LANG_X) siempre resuelve a ingles de
 * fabrica -- los literales de cada opcion (antes struct opt_items para
 * rb->set_option(), D-304) ahora son arrays const char* locales a cada
 * funcion (D-307, ver aura_menu_pick()). */

static void mpeg_settings(void);

/* Aura (D-307): reemplaza rb->do_menu()/rb->set_option()/rb->set_int_ex()
 * -- son widgets 100% Rockbox nativo (icono de "atras" propio, resaltado
 * de seleccion propio, tipografia propia) que no leen ni un color de
 * aura.cfg; D-306 solo llega al OSD (codigo de Aura dentro de este mismo
 * plugin), nunca a este menu. aura_menu_draw()/aura_menu_pick() dibujan
 * con rb->lcd_* directo (mismo patron que ya usaba button_loop() para
 * limpiar la pantalla antes/despues de este menu) y los colores reales
 * del usuario via aura_osd_colors() (mpegplayer.c). */
#define AURA_MENU_ROW_PAD 6

static void aura_menu_draw(const char *title, const char *const *labels,
                           const char *const *values, int count, int sel)
{
    unsigned bg, fg, accent;
    int row_h, y, i;

    aura_osd_colors(&bg, &fg, &accent);

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_background(bg);
    rb->lcd_set_foreground(fg);
    rb->lcd_clear_display();

    rb->lcd_getstringsize("Ag", NULL, &row_h);
    row_h += AURA_MENU_ROW_PAD;

    rb->lcd_putsxy(8, 8, title);
    y = row_h + 12;

    for (i = 0; i < count; i++)
    {
        if (i == sel)
        {
            rb->lcd_set_foreground(accent);
            rb->lcd_fillrect(0, y, SCREEN_WIDTH, row_h);
            rb->lcd_set_foreground(bg);
        }
        else
        {
            rb->lcd_set_foreground(fg);
        }

        rb->lcd_putsxy(12, y + AURA_MENU_ROW_PAD / 2, labels[i]);

        if (values && values[i])
        {
            int vw;
            rb->lcd_getstringsize(values[i], &vw, NULL);
            rb->lcd_putsxy(SCREEN_WIDTH - vw - 12, y + AURA_MENU_ROW_PAD / 2,
                          values[i]);
        }

        y += row_h;
    }

    rb->lcd_update();
}

/* Devuelve el indice elegido (0..count-1), o -1 si el usuario cancelo
 * con MENU o por un evento de sistema (USB, apagado -- mpeg_sysevent()
 * distingue el segundo caso para que el llamador no siga navegando). */
static int aura_menu_pick(const char *title, const char *const *labels,
                          const char *const *values, int count, int start_sel)
{
    int sel = (start_sel >= 0 && start_sel < count) ? start_sel : 0;

    rb->button_clear_queue();
    mpeg_sysevent_clear();

    while (1)
    {
        int button;

        aura_menu_draw(title, labels, values, count, sel);

        button = mpeg_button_get(TIMEOUT_BLOCK);

        if (mpeg_sysevent() != 0)
            return -1;

        switch (button)
        {
        case BUTTON_SCROLL_FWD:
        case BUTTON_SCROLL_FWD | BUTTON_REPEAT:
            sel = (sel + 1) % count;
            break;

        case BUTTON_SCROLL_BACK:
        case BUTTON_SCROLL_BACK | BUTTON_REPEAT:
            sel = (sel - 1 + count) % count;
            break;

        case BUTTON_SELECT:
            return sel;

        case BUTTON_MENU:
            return -1;

        default:
            break;
        }
    }
}

#ifdef HAVE_BACKLIGHT_BRIGHTNESS /* Only used for this atm */
static void aura_adjust_draw(const char *title, const char *value_text)
{
    unsigned bg, fg, accent;
    int tw, th, vw, vh;

    aura_osd_colors(&bg, &fg, &accent);

    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_UI);
    rb->lcd_set_background(bg);
    rb->lcd_set_foreground(fg);
    rb->lcd_clear_display();

    rb->lcd_getstringsize(title, &tw, &th);
    rb->lcd_putsxy((SCREEN_WIDTH - tw) / 2, SCREEN_HEIGHT / 2 - th - 8, title);

    rb->lcd_set_foreground(accent);
    rb->lcd_getstringsize(value_text, &vw, &vh);
    rb->lcd_putsxy((SCREEN_WIDTH - vw) / 2, SCREEN_HEIGHT / 2 + 8, value_text);

    rb->lcd_update();
}

/* Ajustador numerico simple -- solo lo usa el brillo de la luz de
 * fondo, el unico ajuste de este menu que no es una eleccion entre
 * unas pocas opciones fijas. IZQUIERDA/DERECHA cambian el valor de a
 * uno (aplicado en vivo via live_apply, igual que hacia
 * rb->set_int_ex), SELECT confirma, MENU/evento de sistema cancela y
 * restaura el valor original. */
static bool aura_menu_adjust_int(const char *title, int *value, int min, int max,
                                 const char* (*formatter)(char*, size_t, int, const char*),
                                 void (*live_apply)(int))
{
    int v = *value;
    int orig = v;

    rb->button_clear_queue();
    mpeg_sysevent_clear();

    while (1)
    {
        char buf[32];
        const char *text;
        int button;

        text = formatter(buf, sizeof(buf), v, NULL);

        if (live_apply)
            live_apply(v);

        aura_adjust_draw(title, text);

        button = mpeg_button_get(TIMEOUT_BLOCK);

        if (mpeg_sysevent() != 0)
        {
            if (live_apply)
                live_apply(orig);
            return false;
        }

        switch (button)
        {
        case BUTTON_LEFT:
        case BUTTON_LEFT | BUTTON_REPEAT:
            if (v > min) v--;
            break;

        case BUTTON_RIGHT:
        case BUTTON_RIGHT | BUTTON_REPEAT:
            if (v < max) v++;
            break;

        case BUTTON_SELECT:
            *value = v;
            return true;

        case BUTTON_MENU:
            if (live_apply)
                live_apply(orig);
            return false;

        default:
            break;
        }
    }
}
#endif /* HAVE_BACKLIGHT_BRIGHTNESS */

#ifdef HAVE_BACKLIGHT_BRIGHTNESS /* Only used for this atm */
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
        return "Usar ajuste general";
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
    static const char *const items[] = { "Ajustes", "Salir" };
    int result;

    result = aura_menu_pick("Reproductor de video", items, NULL, 2, 0);

    switch (result)
    {
    case MPEG_MENU_SETTINGS:
        mpeg_settings();
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
    static const char *const items[] = {
#if MPEG_OPTION_DITHERING_ENABLED
        "Tramado",
#endif
        "Mostrar FPS",
        "Limitar FPS",
        "Omitir fotogramas",
        "Modo de ajuste",
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
        "Brillo de la luz de fondo",
#endif
    };
    static const char *const yesno[] = { "No", "Si" };
    static const char *const scalemodes[] = { "Ajustar", "Cubrir" };
    int selected = 0;
    int result;
    bool menu_quit = false;

    while (!menu_quit)
    {
        result = aura_menu_pick("Opciones de pantalla", items, NULL,
                                ARRAYLEN(items), selected);
        if (result >= 0)
            selected = result;

        switch (result)
        {
#if MPEG_OPTION_DITHERING_ENABLED
        case MPEG_OPTION_DITHERING:
        {
            int cur = (settings.displayoptions & LCD_YUV_DITHER) ? 1 : 0;
            int picked = aura_menu_pick("Tramado", yesno, NULL, 2, cur);
            if (picked >= 0)
            {
                settings.displayoptions =
                    (settings.displayoptions & ~LCD_YUV_DITHER)
                          | (picked ? LCD_YUV_DITHER : 0);
                rb->lcd_yuv_set_options(settings.displayoptions);
            }
            break;
        }
#endif /* MPEG_OPTION_DITHERING_ENABLED */

        case MPEG_OPTION_DISPLAY_FPS:
        {
            int picked = aura_menu_pick("Mostrar FPS", yesno, NULL, 2,
                                        settings.showfps);
            if (picked >= 0) settings.showfps = picked;
            break;
        }

        case MPEG_OPTION_LIMIT_FPS:
        {
            int picked = aura_menu_pick("Limitar FPS", yesno, NULL, 2,
                                        settings.limitfps);
            if (picked >= 0) settings.limitfps = picked;
            break;
        }

        case MPEG_OPTION_SKIP_FRAMES:
        {
            int picked = aura_menu_pick("Omitir fotogramas", yesno, NULL, 2,
                                        settings.skipframes);
            if (picked >= 0) settings.skipframes = picked;
            break;
        }

        case MPEG_OPTION_SCALE_MODE:
        {
            int picked = aura_menu_pick("Modo de ajuste", scalemodes, NULL, 2,
                                        settings.scale_mode);
            if (picked >= 0)
            {
                settings.scale_mode = picked;
                vo_update_scale_mode();
            }
            break;
        }

#ifdef HAVE_BACKLIGHT_BRIGHTNESS
        case MPEG_OPTION_BACKLIGHT_BRIGHTNESS:
        {
            int v = settings.backlight_brightness;
            mpeg_backlight_update_brightness(v);
            aura_menu_adjust_int("Brillo de la luz de fondo", &v, -1,
                                 MAX_BRIGHTNESS_SETTING - MIN_BRIGHTNESS_SETTING,
                                 backlight_brightness_formatter,
                                 backlight_brightness_function);
            settings.backlight_brightness = v;
            mpeg_backlight_update_brightness(-1);
            break;
        }
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
    static const char *const items[] = {
        "Controles de tono",
        "Configuracion de canales",
        "Crossfeed",
        "Ecualizador",
        "Tramado",
    };
    static const char *const off_setting[] = {
        "Desactivado", "Usar ajuste de sonido"
    };
    int selected = 0;
    int result;
    bool menu_quit = false;

    while (!menu_quit)
    {
        result = aura_menu_pick("Opciones de audio", items, NULL,
                                ARRAYLEN(items), selected);
        if (result >= 0)
            selected = result;

        switch (result)
        {
        case MPEG_AUDIO_TONE_CONTROLS:
        {
            int picked = aura_menu_pick("Controles de tono", off_setting, NULL,
                                        2, settings.tone_controls);
            if (picked >= 0)
            {
                settings.tone_controls = picked;
                sync_audio_setting(MPEG_AUDIO_TONE_CONTROLS, false);
            }
            break;
        }

        case MPEG_AUDIO_CHANNEL_MODES:
        {
            int picked = aura_menu_pick("Configuracion de canales", off_setting,
                                        NULL, 2, settings.channel_modes);
            if (picked >= 0)
            {
                settings.channel_modes = picked;
                sync_audio_setting(MPEG_AUDIO_CHANNEL_MODES, false);
            }
            break;
        }

        case MPEG_AUDIO_CROSSFEED:
        {
            int picked = aura_menu_pick("Crossfeed", off_setting, NULL, 2,
                                        settings.crossfeed);
            if (picked >= 0)
            {
                settings.crossfeed = picked;
                sync_audio_setting(MPEG_AUDIO_CROSSFEED, false);
            }
            break;
        }

        case MPEG_AUDIO_EQUALIZER:
        {
            int picked = aura_menu_pick("Ecualizador", off_setting, NULL, 2,
                                        settings.equalizer);
            if (picked >= 0)
            {
                settings.equalizer = picked;
                sync_audio_setting(MPEG_AUDIO_EQUALIZER, false);
            }
            break;
        }

        case MPEG_AUDIO_DITHERING:
        {
            int picked = aura_menu_pick("Tramado", off_setting, NULL, 2,
                                        settings.dithering);
            if (picked >= 0)
            {
                settings.dithering = picked;
                sync_audio_setting(MPEG_AUDIO_DITHERING, false);
            }
            break;
        }

        default:
            menu_quit = true;
            break;
        }

        if (mpeg_sysevent() != 0)
            menu_quit = true;
    }
}

static void clear_resume_count(void)
{
    settings.resume_count = 0;
    configfile_save(SETTINGS_FILENAME, config, ARRAYLEN(config),
                    SETTINGS_VERSION);
}

static void mpeg_settings(void)
{
    static const char *const items[] = {
        "Opciones de pantalla",
        "Opciones de audio",
        "Modo de reproduccion",
        "Borrar todas las reanudaciones",
    };
    static const char *const single_all[] = { "Uno", "Todos" };
    int selected = 0;
    int result;
    bool menu_quit = false;

    while (!menu_quit)
    {
        result = aura_menu_pick("Ajustes", items, NULL, ARRAYLEN(items),
                                selected);
        if (result >= 0)
            selected = result;

        switch (result)
        {
        case MPEG_SETTING_DISPLAY_SETTINGS:
            display_options();
            break;

        case MPEG_SETTING_AUDIO_SETTINGS:
            audio_options();
            break;

        case MPEG_SETTING_PLAY_MODE:
        {
            int picked = aura_menu_pick("Modo de reproduccion", single_all,
                                        NULL, 2, settings.play_mode);
            if (picked >= 0) settings.play_mode = picked;
            break;
        }

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
    settings.scale_mode = MPEG_SCALE_MODE_FIT; /* Aura (D-304) */
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
