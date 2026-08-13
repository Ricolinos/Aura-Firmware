/* Cronometro de Extras (encargo del dueno del diseno 2026-08-13).
 *
 * Replica el del firmware original: contador HH:MM:SS:CC con las
 * centesimas mas chicas, SELECT guarda un registro (los tres ultimos
 * visibles bajo el contador, todos conservados), y PLAY/PAUSA detiene y
 * regresa al menu conservando el tiempo, con un icono de pausa que
 * indica que se puede reanudar.
 */
#ifndef AURA_STOPWATCH_H
#define AURA_STOPWATCH_H

#include <stdbool.h>
#include "aura_nav.h"

void aura_stopwatch_draw(void);
void aura_stopwatch_handle_button(aura_nav_t *nav, long button);
/* true mientras el cronometro esta contando -- la puerta de energia lo
 * consulta para redibujar a la cadencia de las centesimas. */
bool aura_stopwatch_running(void);

#endif /* AURA_STOPWATCH_H */
