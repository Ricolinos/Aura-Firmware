/* Alarmas de Extras (encargo del dueno del diseno 2026-08-13):
 * lista de alarmas + editor con Alarma (on/off), Hora (con reloj
 * analogico que se mueve con la configuracion), Repetir, Sonido,
 * Etiqueta y Eliminar. */
#ifndef AURA_ALARMS_H
#define AURA_ALARMS_H

#include "aura_nav.h"

void aura_alarms_draw(void);
void aura_alarms_handle_button(aura_nav_t *nav, long button);
void aura_alarm_edit_draw(void);
void aura_alarm_edit_handle_button(aura_nav_t *nav, long button);
void aura_alarm_time_draw(void);
void aura_alarm_time_handle_button(aura_nav_t *nav, long button);
void aura_alarm_choice_draw(void);
void aura_alarm_choice_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_ALARMS_H */
