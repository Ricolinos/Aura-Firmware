/* Punto de entrada de Aura UI. Reemplaza a root_menu() al final de
 * apps/main.c: a partir de aqui la UI de Rockbox (arbol de archivos,
 * menu raiz, WPS) queda completamente inalcanzable para el usuario. */
#ifndef AURA_MAIN_H
#define AURA_MAIN_H

#include "gcc_extensions.h"

void aura_main(void) NORETURN_ATTR;

/* Velocidad angular (grados/seg) del ultimo BUTTON_SCROLL_FWD/BACK
 * procesado -- 0 para cualquier otro boton o para eventos sinteticos sin
 * dato real (doc SS7, aura_wheel.h la consume). */
long aura_main_wheel_velocity(void);

#endif /* AURA_MAIN_H */
