/* Bloqueo de pantalla GLOBAL, en Ajustes (encargo del dueno del diseno
 * 2026-08-13, reubicado desde Extras el 2026-08-14): candado y una
 * clave de 4 digitos que se configura con la rueda; despues de
 * escribirla, pide confirmarla. MENU restablece y cancela; SELECT al
 * final la establece.
 *
 * Tres estados segun aura_settings (screen_lock_active/_enabled), no
 * dos:
 *  - screen_lock_active: "desbloquear" (un solo paso, compara contra la
 *    clave guardada). aura_main.c llama a este mismo par de funciones
 *    directamente en este modo, interceptando el loop principal ANTES
 *    de aura_screens_draw()/aura_screens_handle_button() -- por eso el
 *    bloqueo alcanza a todo el aparato y no solo a esta pantalla.
 *    aura_main.c es tambien el UNICO lugar que enciende este estado (en
 *    cada arranque, a partir de screen_lock_enabled) -- este archivo ya
 *    no lo hace.
 *  - screen_lock_enabled (sin estar activo): "Desactivar" -- confirmar
 *    borra la clave guardada y el armado; reactivar exige configurar
 *    una clave nueva desde cero.
 *  - ninguno de los dos: "Activar" -- configurar una clave nueva (flujo
 *    de arriba). Al coincidir arma screen_lock_enabled; NO enciende
 *    screen_lock_active de inmediato (eso lo decide aura_main.c en el
 *    proximo arranque). */
#ifndef AURA_SCREENLOCK_H
#define AURA_SCREENLOCK_H

#include "aura_nav.h"

void aura_screenlock_draw(void);
void aura_screenlock_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_SCREENLOCK_H */
