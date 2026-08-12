#include <stdio.h>

#include "lcd.h"
#include "button.h"
#include "audio.h"
#include "powermgmt.h"
#include "power.h"
#include "timefuncs.h"
#include "settings.h"
#include "tick.h"

#include "aura_statusbar.h"
#include "aura_widgets.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_marquee.h"

const char *aura_battery_icon_name(void)
{
#if CONFIG_CHARGING
    if (charging_state())
        return "battery-charging";
#endif

    int level = battery_level();
    if (level < 0)
        return "battery-medium"; /* desconocido: ni alarmar ni mentir con "full" */
    if (level >= 60)
        return "battery-full";
    if (level >= 20)
        return "battery-medium";
    return "battery-low";
}

/* HH:MM, reusando el ajuste real de Rockbox (global_settings.timeformat,
 * D-021: mismo criterio que el resto del proyecto, ningun formato propio
 * inventado). Fecha/hora todavia no tienen pantalla de ajustes propia en
 * Aura (Temporiz. luz/reposo si, Fecha y hora sigue diferida -- D-060),
 * asi que "Reloj 24 horas" hoy solo se puede cambiar indirectamente via
 * el ajuste crudo de Rockbox; cuando esa pantalla se construya, este
 * reloj ya la respeta sin cambios. */
/* Estado del marquee del titulo (T2.1/MarqueeText) -- hoisted a estatico
 * de archivo (no local a aura_statusbar_draw) para que
 * aura_statusbar_title_animating() pueda consultarlo desde aura_main.c,
 * mismo patron que aura_widgets_pill_animating()/
 * aura_nowplaying_wheel_animating(). `s_marquee_overflowing` refleja el
 * resultado real de la ULTIMA llamada a aura_marquee_draw() -- sin esto,
 * un titulo que SI cabe (aura_marquee_draw devuelve 0, nada que animar)
 * seguiria pidiendo cuadros extra en vano. */
static int s_marquee_overflowing = 0;
static long s_marquee_since = 0;

/* Bug real encontrado verificando esta misma tarea (no en teoria): la
 * primera version de esto solo devolvia 1 durante el TRAMO de
 * movimiento -- pero durante el tramo estatico (los primeros 2s) nadie
 * pedia un cuadro futuro, asi que el bucle principal se dormia
 * indefinidamente (next_button(-1), sin timeout) y nunca volvia a
 * despertar para notar que el tramo estatico ya habia terminado y
 * tocaba empezar a mover el texto -- el marquee se congelaba en su
 * primer cuadro para siempre. Mismo par pending()/animating() que ya
 * usa aura_widgets_scrollbar_*(): _pending() cubre el CICLO COMPLETO
 * mientras el texto siga desbordando (cadencia gruesa, solo para
 * cruzar la frontera estatico->movimiento a tiempo), _animating() solo
 * el tramo donde los pixeles realmente se mueven (cadencia fina). */
int aura_statusbar_title_pending(void)
{
    return s_marquee_overflowing;
}

int aura_statusbar_title_animating(void)
{
    long elapsed_ms, cycle_ms, t;

    if (!s_marquee_overflowing)
        return 0;

    elapsed_ms = (current_tick - s_marquee_since) * 1000L / HZ;
    cycle_ms = AURA_DS_METRICS_MARQUEE_STATIC_MS + AURA_DS_METRICS_MARQUEE_SCROLL_MS;
    if (cycle_ms <= 0)
        return 0;
    t = elapsed_ms % cycle_ms;

    return t >= AURA_DS_METRICS_MARQUEE_STATIC_MS;
}

void aura_format_clock(char *buf, size_t bufsz)
{
    struct tm *now = get_time();
    int hour = now->tm_hour;

    if (global_settings.timeformat == 1) /* 12 horas */
    {
        int hour12 = hour % 12;
        if (hour12 == 0)
            hour12 = 12;
        snprintf(buf, bufsz, "%d:%02d %s", hour12, now->tm_min,
                  (hour < 12) ? "AM" : "PM");
    }
    else
    {
        snprintf(buf, bufsz, "%02d:%02d", hour, now->tm_min);
    }
}

void aura_statusbar_draw(int x, int width, const char *title, int centered)
{
    int right = x + width - A26_SPACING_SM;
    int icon_y = (A26_LAYOUT_STATUSBAR_HEIGHT - A26_ICON_SIZE_STATUS) / 2;
    int status = audio_status();
    int is_playing = (status & (AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE)) != 0;
    int is_paused = (status & AUDIO_STATUS_PAUSE) != 0;
    int is_hold = button_hold();
    int is_full = (width == A26_SCREEN_WIDTH);
    int title_right_edge = x; /* borde derecho ocupado por el titulo, si hay */

    /* Sin relleno propio: la barra es parte del mismo fondo que el
     * contenido (Principio 1, "contenido antes que cromo") -- solo la
     * separa del cuerpo un hairline de 1px (doc SS5). */
    lcd_set_foreground(a26_color(A26_SHELL_RAIL));
    lcd_hline(x, x + width - 1, A26_LAYOUT_STATUSBAR_HEIGHT - 1);

    /* Bateria: siempre visible, siempre en el extremo derecho (L5,
     * PLAN-UX.md). */
    right -= A26_ICON_SIZE_STATUS;
    aura_widgets_draw_icon(aura_battery_icon_name(), A26_ICON_SIZE_STATUS, right, icon_y);

    /* Candado de Hold y play/pausa: el candado ocupa el lugar del
     * play/pausa si no suena nada; si suena, se coloca a la izquierda
     * del icono de reproduccion (L5). El indicador de reproduccion solo
     * vive en la barra COMPLETA (doc SS5): en la barra dividida de 160px
     * el hueco entre reloj y bateria son los 15px del candado, no
     * alcanza para los dos. La reposicion documentada -- la tarjeta de
     * reproduccion del panel derecho, centrado sobre la barra de
     * progreso -- todavia no existe (queda para Fase 30, junto con el
     * resto de "Ahora suena"); mientras tanto la barra dividida omite el
     * indicador en vez de mostrarlo donde el documento dice que no
     * corresponde. */
    if (is_full && is_playing)
    {
        right -= A26_SPACING_XS + A26_ICON_SIZE_STATUS;
        aura_widgets_draw_icon(is_paused ? "pause" : "play",
                                A26_ICON_SIZE_STATUS, right, icon_y);
        if (is_hold)
        {
            right -= A26_SPACING_XS + A26_ICON_SIZE_STATUS;
            aura_widgets_draw_icon("lock", A26_ICON_SIZE_STATUS, right, icon_y);
        }
    }
    else if (is_hold)
    {
        right -= A26_SPACING_XS + A26_ICON_SIZE_STATUS;
        aura_widgets_draw_icon("lock", A26_ICON_SIZE_STATUS, right, icon_y);
    }

    if (title && title[0])
    {
        int w, h, text_x, text_y;

        /* Semibold, no Regular (AUDITORIA-01 A-04/A-a, doc SS3: "Semibold
         * -> titulos de pantalla") -- el titulo de la barra es un titulo
         * de pantalla, no texto secundario; antes usaba CAPTION (13px
         * Regular) por descuido, ninguna cara Semibold de 13px existia
         * todavia en el pipeline. */
        lcd_setfont(a26_font(A26_FONT_STYLE_HEADER));
        lcd_set_foreground(a26_color(A26_TEXT_PRIMARY));
        lcd_getstringsize((const unsigned char *)title, &w, &h);
        /* Centrado vertical por altura medida: es el equivalente
         * practico de la "linea base compartida y=16" del doc (SS5) --
         * esa tabla da compensaciones para un set de fuentes distinto
         * al nuestro; centrar por altura real dentro de la franja de
         * 20px logra el mismo resultado visual (todo el texto de la
         * barra alineado) sin numeros magicos que no calzan con
         * A26_TYPE_CAPTION. */
        text_y = (A26_LAYOUT_STATUSBAR_HEIGHT - h) / 2;
        /* A26_LAYOUT_LIST_INSET (16px), no A26_SPACING_LG (12px) --
         * AUDITORIA-01 A-21: el titulo debe alinearse verticalmente con
         * el inset real de las filas de lista (doc SS5), no un
         * espaciado suelto que por coincidencia quedaba cerca. */
        text_x = centered ? x + (width - w) / 2 : x + A26_LAYOUT_LIST_INSET;

        if (centered)
        {
            /* Titulo centrado (Ahora suena, Fecha...): identidad propia
             * de la pantalla, no el "nombre de menu" que MarqueeText
             * modela (PLAN.md T2.1/T2.6) -- se queda estatico, igual
             * que antes. */
            lcd_putsxy(text_x, text_y, (const unsigned char *)title);
        }
        else
        {
            /* MarqueeText (T2.1), primer consumidor real mientras
             * DynamicTitle (T2.6) todavia no existe -- mismo criterio
             * que T0.4 con la sombra: reutilizar una superficie real ya
             * en pantalla en vez de dejar el componente huerfano.
             * Reinicia su propio reloj cada vez que el titulo cambia
             * (comparacion por puntero: los titulos de este proyecto
             * son siempre literales de cadena estables, mismo patron
             * que el debounce del panel derecho). */
            static const char *s_shown_title = NULL;
            long elapsed_ms;

            if (title != s_shown_title)
            {
                s_shown_title = title;
                s_marquee_since = current_tick;
            }
            elapsed_ms = (current_tick - s_marquee_since) * 1000L / HZ;

            s_marquee_overflowing = aura_marquee_draw(
                text_x, text_y, right - text_x - A26_SPACING_SM, title, elapsed_ms);
        }

        if (!centered)
            title_right_edge = text_x + w;
    }

    /* Reloj al centro (doc SS5): solo cuando el titulo esta a la
     * izquierda (menus de navegacion) -- si el titulo esta centrado, ya
     * ocupa ese mismo espacio con la identidad propia de la pantalla
     * (Ahora suena, Fecha, etc.) y no hay lugar para los dos a la vez.
     *
     * Chequeo de colision antes de dibujar: en la barra dividida (160px,
     * SPLIT) el titulo puede ser largo ("Temporiz. reposo", "Restablecer
     * ajustes") y el centro matematico de la franja cae demasiado cerca
     * -- verificado a ojo en el simulador, no una suposicion. Se
     * requiere un margen minimo a cada lado (A26_SPACING_MD) contra el
     * titulo y contra el cluster de iconos; si no entra, se omite en vez
     * de superponerse. En pantallas de ancho completo (320px, sin nada
     * mas al centro) esto casi nunca se activa. */
    if (!centered)
    {
        char clock[16];
        int w, h, clock_x;

        aura_format_clock(clock, sizeof(clock));
        lcd_setfont(a26_font(A26_FONT_STYLE_CAPTION));
        lcd_getstringsize((const unsigned char *)clock, &w, &h);
        clock_x = x + (width - w) / 2;

        if (clock_x - A26_SPACING_MD >= title_right_edge
            && clock_x + w + A26_SPACING_MD <= right)
        {
            lcd_set_foreground(a26_color(A26_TEXT_SECONDARY));
            lcd_putsxy(clock_x, (A26_LAYOUT_STATUSBAR_HEIGHT - h) / 2,
                       (const unsigned char *)clock);
        }
    }
}
