/* CoverDrift (PLAN.md T2.9, componentes/cover-drift.md): imagenes
 * (caratulas, fotos) que se desplazan hacia el centro en 8 direcciones
 * fijas, una a la vez, rotando entre las disponibles. Monta solo con
 * >=10 imagenes -- con menos, el llamador debe seguir mostrando
 * SelectionSummary (T2.8) en su lugar.
 *
 * Alcance real de esta pasada (ver DECISIONS.md D-098):
 * - Motor de rotacion/deriva completo: angulo aleatorio sin repetir el
 *   anterior (G10), distancia variable, cross-fade real de <0.5s entre
 *   imagenes -- para el caso de imagen NO cargada (placeholder solido),
 *   donde el cross-fade es un blend de color puro (a26_shell_blend, sin
 *   infraestructura nueva).
 * - Cross-fade de bitmaps REALES (`bmp` no NULL) es un corte directo
 *   sin fundido -- mezclar dos bitmaps arbitrarios pixel a pixel
 *   necesita un compositor offscreen que este sistema no tiene todavia
 *   (mismo tipo de brecha de arquitectura que dejo T1.2/Push-and-Drop
 *   sin conectar su render real). Diferido, no bloqueado: se resuelve
 *   cuando algun consumidor real (Musica/Fotos) necesite bitmaps reales
 *   aqui, hoy ninguno lo hace todavia.
 * - Ningun consumidor real: Musica/Fotos no invocan esto todavia (esa
 *   integracion es trabajo de Etapa 3, T3.1/T3.2, que ya construyen su
 *   propia carga real de caratulas). Verificado con overlay temporal.
 */
#ifndef AURA_COVERDRIFT_H
#define AURA_COVERDRIFT_H

#include "lcd.h"

typedef struct {
    const struct bitmap *bmp; /* NULL = sin imagen real cargada todavia (placeholder) */
} aura_coverdrift_image_t;

/* True si hay imagenes suficientes para montar CoverDrift en vez de
 * SelectionSummary (componentes/cover-drift.md, "Montaje"). */
int aura_coverdrift_should_mount(int image_count);

/* Dibuja en la franja [x, x+width) x [0, A26_SCREEN_HEIGHT), mismo
 * espacio vertical completo que SelectionSummary (T2.8) -- ambos
 * ocupan el mismo hueco del panel derecho, nunca a la vez.
 * `images`/`count` son las imagenes disponibles (>=10, verificar con
 * aura_coverdrift_should_mount() antes de llamar); la imagen ACTIVA
 * rota sola entre ellas, nunca varias a la vez (G10: "una imagen
 * visible a la vez"). */
void aura_coverdrift_draw(int x, int width,
                           const aura_coverdrift_image_t *images, int count);

/* Mismo par pending()/animating() del resto del sistema -- animating()
 * es practicamente siempre true mientras este montado (el movimiento es
 * continuo, a diferencia de MarqueeText/ScrollIndicator que tienen
 * tramos estaticos reales), pero se expone el par completo por
 * consistencia con la puerta de energia central. */
int aura_coverdrift_pending(void);
int aura_coverdrift_animating(void);

#endif /* AURA_COVERDRIFT_H */
