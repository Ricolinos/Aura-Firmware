/* Utilidades de arte compartidas (Fase 29, PLAN-APPLE2026.md, doc de
 * diseno SS5.4): reflejo precalculado parametrico, hoy usado por Cover
 * Flow (aura_coverflow.c) y pensado tambien para Ahora suena (Fase 30).
 * Antes vivia solo dentro de aura_albumart.c, con la fraccion de alto
 * vieja (50%, D-057) -- extraido a un modulo propio para que ambos
 * consumidores compartan la misma implementacion en vez de
 * reimplementarla cada uno. Depende de lcd.h (fb_data, RGB_UNPACK_*):
 * no es un modulo host-testable como aura_nav/motion/wheel, mismo
 * perfil de dependencias que aura_albumart.c.
 */
#ifndef AURA_ART_H
#define AURA_ART_H

#include "lcd.h"

/* Alto del reflejo como numerador/100 del lado del cuadrado (doc SS5.4:
 * "35% alto"). Expuesto para que el llamador dimensione su buffer:
 * aura_art_reflection_height(size) filas de `size` columnas. */
#define AURA_ART_REFLECTION_HEIGHT_PCT 35

int aura_art_reflection_height(int size);

/* Genera el reflejo de `cover` (size x size, fb_data, formato nativo
 * del LCD) en `out` (size x aura_art_reflection_height(size), reservado
 * por el llamador -- Aura no hace malloc). Espejo vertical de las
 * primeras filas de la caratula, atenuado hacia `bg_color` a medida que
 * se aleja (mascara 35%->0%, doc SS5.4). */
void aura_art_generate_reflection(const fb_data *cover, fb_data *out,
                                   int size, unsigned bg_color);

#endif /* AURA_ART_H */
