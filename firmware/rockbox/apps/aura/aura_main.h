/* Punto de entrada de Aura UI. Reemplaza a root_menu() al final de
 * apps/main.c: a partir de aqui la UI de Rockbox (arbol de archivos,
 * menu raiz, WPS) queda completamente inalcanzable para el usuario. */
#ifndef AURA_MAIN_H
#define AURA_MAIN_H

#include "gcc_extensions.h"

void aura_main(void) NORETURN_ATTR;

#endif /* AURA_MAIN_H */
