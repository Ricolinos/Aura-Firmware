/* Bloqueo de pantalla de Extras (encargo del dueno del diseno
 * 2026-08-13): candado y una clave de 4 digitos que se configura con la
 * rueda; despues de escribirla, pide confirmarla. MENU restablece y
 * cancela; SELECT al final la establece. */
#ifndef AURA_SCREENLOCK_H
#define AURA_SCREENLOCK_H

#include "aura_nav.h"

void aura_screenlock_draw(void);
void aura_screenlock_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_SCREENLOCK_H */
