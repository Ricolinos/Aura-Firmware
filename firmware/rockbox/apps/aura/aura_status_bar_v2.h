/* StatusBar v2 (PLAN.md T2.7, componentes/status-bar.md): ensambla
 * DynamicTitle (T2.6) + ClockIndicator (T2.5) + iconos de estado
 * (bateria/candado/play) en las posiciones y orden del sistema nuevo.
 *
 * Alcance real de esta pasada (ver DECISIONS.md D-096): geometria,
 * orden, tipografia con opacidad simulada y montaje condicional (split/
 * full) completos y verificados. La transicion Push-and-Drop real
 * (T1.2 ya tiene la coreografia de tiempo lista) NO se conecta todavia
 * -- exige separar el render de "contenido" del de "chrome" en el
 * motor de transiciones offscreen existente, cambio de arquitectura
 * real que T1.2 ya identifico y que esta pasada tampoco alcanza a
 * hacer con el rigor que merece. DynamicTitle se dibuja siempre
 * estatico (transition=NONE) por ahora -- disparar la transicion real
 * en cada navegacion es follow-up, anotado, no bloqueado (a diferencia
 * de B-01/B-02 en BLOCKED.md, esto SI tiene toda la informacion que
 * necesita, solo falta el trabajo de conectarlo).
 */
#ifndef AURA_STATUS_BAR_V2_H
#define AURA_STATUS_BAR_V2_H

#include <stdbool.h>

/* Dibuja la franja [x, x+width) x [0, AURA_DS_METRICS_STATUSBAR_HEIGHT).
 * `title` es el nombre de menu/seccion actual (nunca NULL ni vacio --
 * a diferencia del sistema viejo, StatusBar v2 siempre tiene algo que
 * mostrar en ese slot). `is_playing`/`is_paused`/`is_hold` describen el
 * estado del reproductor -- mismos campos que ya resuelve el sistema
 * viejo, pasados por el llamador en vez de que este modulo vuelva a
 * consultar audio_status()/button_hold() (mas facil de verificar sin
 * depender de estado global oculto). */
void aura_status_bar_v2_draw(int x, int width, const char *title,
                              bool is_playing, bool is_paused, bool is_hold);

/* ClockIndicator por atajo (componentes/clock-indicator.md, B-01 en
 * BLOCKED.md ya resuelto: "se revela manteniendo presionado SELECT").
 * Dispara la revelacion temporal -- no hace nada si el reloj ya esta
 * en modo persistente (aura_settings.clock_visible, siempre visible,
 * nada que revelar) ni si ya estaba revelado (reinicia el conteo de
 * 10s en vez de duplicar la animacion). El llamador es
 * aura_main.c, que intercepta AURA_BUTTON_HOLD (aura_main.h, B-02)
 * para SELECT de forma centralizada -- StatusBar es la unica duena de
 * este gesto, ninguna pantalla necesita saber de el. */
void aura_status_bar_v2_reveal_clock(void);

/* Mismo par pending()/animating() del resto del sistema -- cubre tanto
 * la animacion de entrada/salida (Drop-and-Lift en (split),
 * Push-and-Pull en (full)) como la espera de los 10s visible antes de
 * empezar a ocultarse (pending() sin animating(), cadencia gruesa,
 * mismo criterio que MarqueeText/ScrollIndicator). */
int aura_status_bar_v2_clock_pending(void);
int aura_status_bar_v2_clock_animating(void);

#endif /* AURA_STATUS_BAR_V2_H */
