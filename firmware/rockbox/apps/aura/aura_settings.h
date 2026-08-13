/* Ajustes propios de Aura (tema, modo grafico, preset de EQ, idioma).
 *
 * El brillo se deja tal cual en global_settings.brightness (backend de
 * Rockbox, apps/settings.c) y el ecualizador reutiliza el motor DSP real
 * de Rockbox (dsp_set_eq_coefs et al. via sound_settings_apply()); Aura
 * solo anade una capa de presets con nombre encima. Ver DECISIONS.md.
 */
#ifndef AURA_SETTINGS_H
#define AURA_SETTINGS_H

#include <stdbool.h>

typedef enum {
    AURA_THEME_LIGHT = 0,
    AURA_THEME_DARK,
    AURA_THEME_COUNT,
} aura_theme_id_t;

/* Animaciones: todo lo que se MUEVE (transiciones entre pantallas y su
 * cantidad de cuadros). Ajuste propio, separado de los graficos: un
 * usuario puede querer la interfaz completa pero sin movimiento, o al
 * reves. */
typedef enum {
    AURA_ANIM_NONE = 0,   /* Ninguna */
    AURA_ANIM_MINIMAL,    /* Minimas */
    AURA_ANIM_ALL,        /* Todas */
    AURA_ANIM_COUNT,
} aura_anim_mode_t;

/* Graficos: todo lo que se DIBUJA de mas (panel derecho de preview,
 * coverflow, caratulas). Nada que ver con el movimiento. */
typedef enum {
    AURA_GFX_NONE = 0,    /* Ninguno */
    AURA_GFX_MINIMAL,     /* Minimos */
    AURA_GFX_ALL,         /* Todos */
    AURA_GFX_COUNT,
} aura_gfx_mode_t;

typedef enum {
    /* Presets del firmware original (2026-08-13), en su orden. */
    AURA_EQ_OFF = 0,
    AURA_EQ_ACOUSTIC,
    AURA_EQ_BASS_BOOST,
    AURA_EQ_BASS_RED,
    AURA_EQ_CLASSICAL,
    AURA_EQ_DANCE,
    AURA_EQ_DEEP,
    AURA_EQ_ELECTRONIC,
    AURA_EQ_FLAT,
    AURA_EQ_HIPHOP,
    AURA_EQ_JAZZ,
    AURA_EQ_LATIN,
    AURA_EQ_LOUDNESS,
    AURA_EQ_LOUNGE,
    AURA_EQ_PIANO,
    AURA_EQ_POP,
    AURA_EQ_RNB,
    AURA_EQ_ROCK,
    AURA_EQ_SMALLSPK,
    AURA_EQ_SPOKEN,
    AURA_EQ_TREBLE_BOOST,
    AURA_EQ_TREBLE_RED,
    AURA_EQ_VOCAL_BOOST,
    AURA_EQ_COUNT,
} aura_eq_preset_t;

typedef enum {
    AURA_LANG_ES = 0,
    AURA_LANG_EN,
    AURA_LANG_COUNT,
} aura_lang_t;

typedef struct {
    aura_theme_id_t theme;
    aura_anim_mode_t animation_mode;
    aura_gfx_mode_t graphics_mode;
    aura_eq_preset_t eq_preset;
    aura_lang_t language;
    /* Menu principal configurable (L14, Fase 18): Musica y Ajustes
     * siempre se muestran; estos tres son opcionales. */
    bool show_videos;
    bool show_photos;
    bool show_nowplaying;
    /* Acento configurable del sistema docs/aura-design-system/ (PLAN.md
     * T0.3, fundamentos/01-color.md: "CONFIGURABLE por el usuario desde
     * Ajustes"). 0xRRGGBB de 24 bits, no el formato nativo del LCD --
     * ver aura_color.h/apple2026_shell.h (aura_accent()) para el uso. */
    unsigned accent_rgb24;
    /* Sombra de LeftPanel sobre SelectionSummary/CoverDrift (PLAN.md
     * T0.4, efectos/01-sombras.md: "el usuario puede desactivarlo desde
     * Ajustes... la sombra activada es el comportamiento base"). */
    bool left_panel_shadow;
    /* Visibilidad de ClockIndicator (T2.5, componentes/clock-indicator.md).
     * Solo el modo "persistente" (true=siempre visible) tiene disparador
     * definido hoy -- "auto-oculta" y el atajo por mantener Select
     * quedan bloqueados (ver BLOCKED.md), asi que este bool cubre
     * persistente/oculto; el tercer valor se agrega cuando el
     * disparador exista de verdad. */
    bool clock_visible;
    /* Iconos de las filas de menu (encargo 2026-08-13): el usuario
     * puede apagarlos para una lectura mas limpia. Activado por
     * defecto -- es el aspecto base del sistema. */
    bool show_icons;
} aura_settings_t;

/* Instancia unica en memoria, cargada por aura_settings_load(). */
extern aura_settings_t aura_settings;

/* True si AURA_CFG_PATH todavia no existe -- primer arranque de Aura
 * en este dispositivo (o tras un "Restablecer ajustes"). Consultar
 * *antes* de aura_settings_load() si hace falta distinguir "nunca se
 * guardo nada" de "el usuario ya elige valores". Ver D-06x, Fase 18:
 * asi es como aura_main() decide si forzar los defaults opinados sobre
 * ajustes reales de Rockbox (backlight, volume_limit, poweroff,
 * sleeptimer) solo una vez, en vez de pisar la eleccion del usuario en
 * cada arranque. */
bool aura_settings_is_first_boot(void);

/* Carga aura_settings desde disco (o aplica los valores por defecto si
 * el archivo no existe o esta corrupto) y aplica el preset de EQ
 * cargado al motor DSP. Llamar una vez al arrancar. */
void aura_settings_load(void);

/* Persiste aura_settings a disco. */
void aura_settings_save(void);

/* Vuelve aura_settings a sus valores de fabrica (tema, modo grafico,
 * EQ, idioma, menu principal) y los persiste -- "Restablecer ajustes"
 * (Fase 18). No toca ningun ajuste real de Rockbox; para eso ver
 * aura_settings_apply_core_defaults(). */
void aura_settings_reset_to_defaults(void);

/* Aplica aura_settings.eq_preset al DSP real de Rockbox. */
void aura_settings_apply_eq(void);

/* Aplica los defaults opinados de Aura sobre ajustes reales de
 * Rockbox que tienen pantalla propia en Ajustes desde la Fase 18
 * (Temporiz. luz, Temporiz. reposo, Limite volumen, apagado por
 * inactividad) y los persiste con settings_save(). Dos llamadores:
 * aura_main() en el primer arranque (aura_settings_is_first_boot()),
 * y "Restablecer ajustes" -- para que un reset vuelva a los defaults
 * de Aura, no a los de fabrica de Rockbox (que settings_reset() por
 * si solo restauraria). */
void aura_settings_apply_core_defaults(void);

#endif /* AURA_SETTINGS_H */
