#include <string.h>
#include <stdio.h>

#include "lcd.h"
#include "button.h"

#include "aura_screenlock.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"
#include "aura_lang.h"
#include "aura_screens.h"
#include "aura_settings.h"

#define SL_DIGITS   4
#define SL_BOX_W    34
#define SL_BOX_H    44
#define SL_GAP      A26_SPACING_MD

/* Dos pasadas: escribir la clave y confirmarla. Si la confirmacion no
 * coincide, se vuelve a la primera -- sin mensajes de error tecnicos,
 * el propio reinicio de los digitos es la senal. */
static int  s_digits[SL_DIGITS];
static int  s_first[SL_DIGITS];
static int  s_focus = 0;
static bool s_confirming = false;

static void reset_all(void)
{
    memset(s_digits, 0, sizeof(s_digits));
    memset(s_first, 0, sizeof(s_first));
    s_focus = 0;
    s_confirming = false;
}

void aura_screenlock_draw(void)
{
    int total_w = SL_DIGITS * SL_BOX_W + (SL_DIGITS - 1) * SL_GAP;
    int x0 = (A26_SCREEN_WIDTH - total_w) / 2;
    int y = A26_SCREEN_HEIGHT / 2 - SL_BOX_H / 2 + 10;
    /* D-197: candado EN VIVO (screen_lock_active) -- pantalla de
     * desbloqueo, un solo paso, texto distinto del flujo de
     * configuracion (que sigue usando LOCK_SET/LOCK_CONFIRM). */
    const char *hint = aura_settings.screen_lock_active
        ? aura_str(AURA_STR_LOCK_ENTER)
        : aura_str(s_confirming ? AURA_STR_LOCK_CONFIRM : AURA_STR_LOCK_SET);
    int i, w, h;

    a26_shell_clear_screen();
    aura_widgets_draw_status_bar(aura_str(AURA_STR_EXTRAS_SCREENLOCK));

    /* Candado grande arriba, el mismo simbolo del sistema. */
    aura_widgets_draw_icon("lock", A26_ICON_SIZE_PREVIEW,
                            (A26_SCREEN_WIDTH - A26_ICON_SIZE_PREVIEW) / 2,
                            A26_LAYOUT_STATUSBAR_HEIGHT + A26_SPACING_LG);

    lcd_setfont(a26_font(A26_FONT_STYLE_DS_REG_12));
    lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
    lcd_getstringsize((const unsigned char *)hint, &w, &h);
    lcd_putsxy((A26_SCREEN_WIDTH - w) / 2, y - A26_SPACING_XXL,
               (const unsigned char *)hint);

    for (i = 0; i < SL_DIGITS; i++)
    {
        int x = x0 + i * (SL_BOX_W + SL_GAP);
        char d[2] = { (char)('0' + s_digits[i]), '\0' };
        bool focus = (i == s_focus);

        a26_shell_fill_rounded_rect(x, y, SL_BOX_W, SL_BOX_H,
                                     A26_LAYOUT_CORNER_RADIUS_CARD,
                                     a26_color(A26_SELECTION_FILL),
                                     a26_color(A26_SHELL_BG));
        lcd_setfont(a26_font(A26_FONT_STYLE_TITLE));
        lcd_set_foreground(focus ? aura_accent() : a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)d, &w, &h);
        lcd_putsxy(x + (SL_BOX_W - w) / 2, y + (SL_BOX_H - h) / 2,
                   (const unsigned char *)d);
    }
}

/* Empaqueta los 4 digitos en un numero 0-9999 para compararlos/guardarlos
 * como un solo entero (mismo formato que aura_settings.screen_lock_pin). */
static unsigned digits_to_pin(const int digits[SL_DIGITS])
{
    return (unsigned)(digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3]);
}

/* D-197: desbloqueo -- UN solo paso (sin confirmar dos veces, la clave
 * ya existe), compara contra aura_settings.screen_lock_pin. MENU no
 * hace nada: a diferencia de configurar una clave nueva, aca no hay
 * "cancelar" legitimo -- la unica salida es acertar la clave. */
static void handle_unlock_button(long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_digits[s_focus] = (s_digits[s_focus] + 1) % 10;
        break;
    case BUTTON_SCROLL_BACK:
        s_digits[s_focus] = (s_digits[s_focus] + 9) % 10;
        break;

    case BUTTON_SELECT:
        if (s_focus < SL_DIGITS - 1)
        {
            s_focus++;
            break;
        }
        if (digits_to_pin(s_digits) == aura_settings.screen_lock_pin)
        {
            /* Clave correcta: se apaga el candado y se guarda de
             * inmediato -- si el usuario apaga el aparato en este
             * instante, debe volver a arrancar YA desbloqueado. */
            aura_settings.screen_lock_active = false;
            aura_settings_save();
            reset_all();
        }
        else
        {
            /* Incorrecta: mismo lenguaje que el flujo de configurar
             * (reiniciar los digitos es la senal, sin texto de error) --
             * se queda en esta pantalla, nunca se sale sola. */
            reset_all();
        }
        break;

    default:
        /* MENU y cualquier otro boton: sin efecto. Ningun otro camino
         * de esta pantalla llama a aura_nav_push/pop en modo
         * desbloqueo -- el aparato entero queda atrapado aca hasta
         * acertar (aura_main.c intercepta el loop principal). */
        break;
    }
}

/* Configurar una clave nueva (flujo original, sin cambios de
 * comportamiento): dos pasadas, MENU cancela sin guardar. Unica
 * diferencia con antes (D-197): al coincidir, la clave se GUARDA de
 * verdad y el candado se enciende ya mismo -- antes esto se
 * descartaba en silencio, que es exactamente el reporte del dueno
 * ("configura la clave, pero de nada sirve"). */
static void handle_configure_button(aura_nav_t *nav, long button)
{
    switch (button)
    {
    case BUTTON_SCROLL_FWD:
        s_digits[s_focus] = (s_digits[s_focus] + 1) % 10;
        break;
    case BUTTON_SCROLL_BACK:
        s_digits[s_focus] = (s_digits[s_focus] + 9) % 10;
        break;

    case BUTTON_SELECT:
        if (s_focus < SL_DIGITS - 1)
        {
            s_focus++;
            break;
        }
        if (!s_confirming)
        {
            /* Primera pasada completa: pedir confirmacion. */
            memcpy(s_first, s_digits, sizeof(s_first));
            memset(s_digits, 0, sizeof(s_digits));
            s_focus = 0;
            s_confirming = true;
        }
        else if (memcmp(s_first, s_digits, sizeof(s_first)) == 0)
        {
            /* Coincide: se guarda de verdad y el candado queda
             * ENCENDIDO ya -- la proxima vuelta del loop en
             * aura_main.c ya va a mostrar esta misma pantalla en modo
             * desbloqueo, confirmando que el bloqueo es real. */
            aura_settings.screen_lock_pin = (unsigned short)digits_to_pin(s_first);
            aura_settings.screen_lock_configured = true;
            aura_settings.screen_lock_active = true;
            aura_settings_save();
            reset_all();
            aura_nav_pop(nav);
        }
        else
        {
            /* No coincide: vuelve a la primera pasada. */
            reset_all();
        }
        break;

    case BUTTON_MENU:
        /* Restablece y NO configura nada (regla del original). */
        reset_all();
        aura_nav_pop(nav);
        break;

    default:
        break;
    }
}

void aura_screenlock_handle_button(aura_nav_t *nav, long button)
{
    if (aura_settings.screen_lock_active)
        handle_unlock_button(button);
    else
        handle_configure_button(nav, button);
}
