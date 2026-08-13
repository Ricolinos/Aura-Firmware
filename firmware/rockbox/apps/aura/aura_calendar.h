/* Calendario de Extras (encargo del dueno del diseno 2026-08-13):
 * rejilla del mes navegable -- la rueda recorre los dias, los botones
 * de avanzar/retroceder cambian de mes, SELECT entra al dia. */
#ifndef AURA_CALENDAR_H
#define AURA_CALENDAR_H

#include "aura_nav.h"

void aura_calendar_draw(void);
void aura_calendar_handle_button(aura_nav_t *nav, long button);
void aura_calendar_day_draw(void);
void aura_calendar_day_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_CALENDAR_H */
