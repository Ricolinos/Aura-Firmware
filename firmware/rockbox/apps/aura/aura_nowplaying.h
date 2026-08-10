/* Pantalla "Ahora suena": caratula, titulo/artista/album, progreso,
 * bateria, y letra sincronizada (.lrc) accesible con el boton central. */
#ifndef AURA_NOWPLAYING_H
#define AURA_NOWPLAYING_H

#include "aura_nav.h"

/* True si hay algo cargado para reproducir (pausado o sonando). Si es
 * false, aura_screens.c muestra el estado vacio en su lugar. */
bool aura_nowplaying_active(void);

void aura_nowplaying_draw(void);
void aura_nowplaying_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_NOWPLAYING_H */
