/* Fotos: lista de imagenes en /Photos (JPG y BMP se decodifican; PNG y
 * GIF se listan pero muestran "formato no soportado" -- sus decoders
 * solo existen dentro del plugin imageviewer, no como funcion de app
 * reutilizable; portarlos queda fuera de alcance de esta fase, ver
 * D-028 en DECISIONS.md) y un visor de pantalla completa. */
#ifndef AURA_PHOTOS_H
#define AURA_PHOTOS_H

#include "aura_nav.h"

void aura_photos_draw(aura_nav_t *nav);
void aura_photos_handle_button(aura_nav_t *nav, long button);

void aura_photo_viewer_draw(aura_nav_t *nav);
void aura_photo_viewer_handle_button(aura_nav_t *nav, long button);

#endif /* AURA_PHOTOS_H */
