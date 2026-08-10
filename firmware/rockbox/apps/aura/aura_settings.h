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

typedef enum {
    AURA_GFX_ULTRA = 0,   /* cero animaciones, refrescos minimos */
    AURA_GFX_MINIMAL,     /* solo transicion direccional breve */
    AURA_GFX_FULL,        /* coverflow, crossfade, transiciones completas */
    AURA_GFX_COUNT,
} aura_gfx_mode_t;

typedef enum {
    AURA_EQ_FLAT = 0,
    AURA_EQ_BASS_BOOST,
    AURA_EQ_VOCAL,
    AURA_EQ_TREBLE_BOOST,
    AURA_EQ_COUNT,
} aura_eq_preset_t;

typedef enum {
    AURA_LANG_ES = 0,
    AURA_LANG_EN,
    AURA_LANG_COUNT,
} aura_lang_t;

typedef struct {
    aura_theme_id_t theme;
    aura_gfx_mode_t graphics_mode;
    aura_eq_preset_t eq_preset;
    aura_lang_t language;
} aura_settings_t;

/* Instancia unica en memoria, cargada por aura_settings_load(). */
extern aura_settings_t aura_settings;

/* Carga aura_settings desde disco (o aplica los valores por defecto si
 * el archivo no existe o esta corrupto) y aplica el preset de EQ
 * cargado al motor DSP. Llamar una vez al arrancar. */
void aura_settings_load(void);

/* Persiste aura_settings a disco. */
void aura_settings_save(void);

/* Aplica aura_settings.eq_preset al DSP real de Rockbox. */
void aura_settings_apply_eq(void);

#endif /* AURA_SETTINGS_H */
