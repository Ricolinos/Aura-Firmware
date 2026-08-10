/* Pantalla de inicio dividida del modo grafico Completo: carátulas en
 * rotación con crossfade arriba, menú raíz abajo. En Ultra/Minimalista
 * la raíz sigue siendo la lista simple de aura_screens.c. Ver D-025.
 */
#ifndef AURA_HOME_H
#define AURA_HOME_H

#include <stdbool.h>

#include "aura_nav.h"

/* True si hay al menos un album con caratula disponible -- si no, el
 * llamador debe usar la lista simple de siempre en su lugar (una
 * pantalla dividida con la mitad de arriba vacia no aporta nada). */
bool aura_home_has_content(void);

void aura_home_draw(aura_nav_t *nav);
void aura_home_handle_button(aura_nav_t *nav, long button);

/* True mientras la pantalla de inicio dividida necesita redibujarse
 * sola (animando un crossfade o esperando la proxima rotacion). Lo usa
 * aura_main.c para decidir si sondear con timeout. */
bool aura_home_needs_tick(void);

#endif /* AURA_HOME_H */
