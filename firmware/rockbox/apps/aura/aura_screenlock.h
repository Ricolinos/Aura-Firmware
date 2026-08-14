/* Bloqueo de pantalla de Extras (encargo del dueno del diseno
 * 2026-08-13): candado y una clave de 4 digitos que se configura con la
 * rueda; despues de escribirla, pide confirmarla. MENU restablece y
 * cancela; SELECT al final la establece.
 *
 * D-197: dos modos segun aura_settings.screen_lock_active --
 * "configurar" (arriba, sin cambios) cuando esta apagado, "desbloquear"
 * (un solo paso, compara contra la clave guardada) cuando esta
 * encendido. aura_main.c llama a este mismo par de funciones
 * directamente en modo desbloqueo, interceptando el loop principal
 * ANTES de aura_screens_draw()/aura_screens_handle_button() -- por eso
 * el bloqueo alcanza a todo el aparato y no solo a esta pantalla. */
#ifndef AURA_SCREENLOCK_H
#define AURA_SCREENLOCK_H

#include "aura_nav.h"

void aura_screenlock_draw(void);
void aura_screenlock_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_SCREENLOCK_H */
