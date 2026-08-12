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

#endif /* AURA_STATUS_BAR_V2_H */
