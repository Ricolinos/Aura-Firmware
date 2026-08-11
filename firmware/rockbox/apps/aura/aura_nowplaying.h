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

/* True mientras el overlay de volumen (Fase 17, PLAN-UX.md) sigue
 * visible y necesita un redibujo propio para desaparecer solo, aunque
 * el usuario no vuelva a tocar el clickwheel. */
bool aura_nowplaying_needs_tick(void);

/* True mientras el icono de modo recien activado sigue en pleno resorte
 * (doc "Reproductor - Ahora suena.md" SS5, Fase 30) -- aura_main.c pide
 * la cadencia fina de 20fps mientras dure, mismo patron que
 * aura_widgets_pill_animating(). */
int aura_nowplaying_wheel_animating(void);

#endif /* AURA_NOWPLAYING_H */
