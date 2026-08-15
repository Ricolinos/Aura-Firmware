/* CoverDrift (PLAN.md T2.9, componentes/cover-drift.md): imagenes
 * (caratulas, fotos) que se desplazan hacia el centro en 8 direcciones
 * fijas, una a la vez, rotando entre las disponibles. Monta solo con
 * >=10 imagenes -- con menos, el llamador debe seguir mostrando
 * SelectionSummary (T2.8) en su lugar.
 *
 * Alcance real de la construccion original (ver DECISIONS.md D-098):
 * - Motor de rotacion/deriva completo: angulo aleatorio sin repetir el
 *   anterior (G10), distancia variable, cross-fade real de <0.5s entre
 *   imagenes -- para el caso de imagen NO cargada (placeholder solido),
 *   donde el cross-fade es un blend de color puro (a26_shell_blend, sin
 *   infraestructura nueva).
 * - Cross-fade de bitmaps REALES (`bmp` no NULL) es un corte directo
 *   sin fundido -- mezclar dos bitmaps arbitrarios pixel a pixel
 *   necesita un compositor offscreen que este sistema no tiene todavia
 *   (mismo tipo de brecha de arquitectura que dejo T1.2/Push-and-Drop
 *   sin conectar su render real). Diferido, no bloqueado -- sigue sin
 *   resolverse en D-251 (ver abajo), ningun consumidor real necesita
 *   fundido de bitmap todavia, un corte directo es aceptable a esta
 *   cadencia de 7s por imagen.
 *
 * D-251: conectado a un consumidor real -- las miniaturas de la lista
 * de Fotos (aura_photos.c), via aura_widgets_draw_list_with_art(). Los
 * getters de indice de abajo existen exactamente para permitir esa
 * decodificacion bajo demanda sin cargar el pool completo en memoria.
 *
 * D-252: conectado tambien a Musica -- las pantallas de
 * Canciones/Artistas/Generos/Compositores/etc. (todo
 * is_music_browse_screen() menos las de Albumes, que tienen su propio
 * renderizador de miniatura por fila) pasan a layout SPLIT
 * (screen_uses_split_layout(), aura_screens.c) para darle un hueco a
 * CoverDrift -- pierden el riel A-Z que tenian en FULL, tradeoff
 * aceptado explicitamente por el dueno del producto. Ver
 * ensure_drift_album_pool()/ensure_drift_albums_decoded() en
 * aura_screens.c: usan un pool GENERAL de todos los albumes de la
 * biblioteca (no el album exacto de cada fila -- D-242 encontro que
 * esa resolucion no tiene API publica).
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

/* D-251 (conexion a un consumidor real, Musica/Fotos): decodificar las
 * `count` imagenes de un pool de antemano es inviable en memoria (un
 * pool de musica puede tener hasta AURA_MUSIC_MAX_ITEMS=300 caratulas;
 * decodificarlas todas serian cientos de KB a MB). El llamador solo
 * necesita decodificar bajo demanda la imagen ACTIVA y la ANTERIOR
 * (para el cross-fade) -- estos getters exponen el indice que
 * aura_coverdrift_draw() va a leer en el PROXIMO cuadro, para que el
 * llamador pueda decodificar antes de esa llamada. Devuelven -1 si
 * CoverDrift todavia no se monto ninguna vez (antes de la primera
 * llamada a aura_coverdrift_draw() con count>0) -- en ese primer
 * cuadro se ve el placeholder solido (bmp==NULL para el indice que se
 * vaya a usar), y desde el segundo cuadro en adelante ya hay bitmap
 * real: degradacion de un solo cuadro, imperceptible. */
int aura_coverdrift_active_index(void);
int aura_coverdrift_prev_index(void);

#endif /* AURA_COVERDRIFT_H */
