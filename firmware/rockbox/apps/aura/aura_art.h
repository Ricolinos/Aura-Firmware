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

#include <stdbool.h>

#include "lcd.h"

/* Alto del reflejo como numerador/100 del lado del cuadrado (doc SS5.4
 * del sistema viejo: "35% alto") -- valor por defecto de los
 * consumidores del sistema Apple2026 viejo (Cover Flow viejo,
 * aura_albumart.c). El sistema nuevo (componentes/now-playing.md,
 * cover-flow.md) pide una proporcion mas sutil (25%,
 * AURA_DS_METRICS_COVER_FLOW_REFLECTION_PCT_OF_SLIDE_HEIGHT, G16) --
 * por eso `height_pct` es un parametro, no una constante fija: cada
 * consumidor pasa la suya en vez de que este modulo elija por todos. */
#define AURA_ART_REFLECTION_HEIGHT_PCT 35

int aura_art_reflection_height(int size, int height_pct);

/* Genera el reflejo de `cover` (size x size, fb_data, formato nativo
 * del LCD) en `out` (size x aura_art_reflection_height(size, height_pct),
 * reservado por el llamador -- Aura no hace malloc). Espejo vertical de
 * las primeras filas de la caratula, atenuado hacia `bg_color` a medida
 * que se aleja (mascara height_pct%->0%).
 *
 * `transposed`=false: layout normal, fila contigua (cover[y*size+x],
 * out[y*size+x]) -- consumidores existentes (aura_albumart.c "flat",
 * aura_nowplaying.c). `transposed`=true: layout columna contigua
 * (cover[x*size+y], out[x*refl_h+y]) -- cache .pfraw de Cover Flow
 * (T3.2, componentes/cover-flow.md), para que el render por columnas
 * de aura_flow.c lea memoria contigua sin necesidad de transponer un
 * bitmap ya generado en el layout equivocado. */
void aura_art_generate_reflection(const fb_data *cover, fb_data *out,
                                   int size, int height_pct, unsigned bg_color,
                                   bool transposed);

#endif /* AURA_ART_H */
