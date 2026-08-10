/* Videos: lista de archivos MPEG-1/2 en /Videos (el unico formato que
 * reproduce el dispositivo -- lo genera Aura Studio al sincronizar,
 * ver PLAN.md) y reproduccion delegada al plugin mpegplayer del fork
 * (D-029 en DECISIONS.md: portar un decoder de video propio queda
 * fuera de alcance). */
#ifndef AURA_VIDEO_H
#define AURA_VIDEO_H

#include "aura_nav.h"

void aura_video_draw(aura_nav_t *nav);
void aura_video_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_VIDEO_H */
