/* Motor de movimiento (Fase 28, PLAN-APPLE2026.md): tablas de easing
 * precomputadas, un modulo central en vez de animacion ad-hoc por
 * pantalla. Modulo puro en C99, sin dependencias de Rockbox ni de HZ --
 * se compila igual dentro del firmware que como binario de test en el
 * host (ver apps/aura/test/), mismo criterio que aura_nav.c/.h. No
 * dibuja nada ni toca hardware: el llamador convierte ticks reales
 * (HZ, definido solo en firmware) a las unidades que estas funciones
 * reciben.
 */
#ifndef AURA_MOTION_H
#define AURA_MOTION_H

/* Duraciones de referencia en ms (doc de diseno SS6). El llamador las
 * convierte a ticks con su propio HZ -- este modulo no depende de
 * Rockbox, asi que no puede hacerlo el mismo. */
#define AURA_MOTION_FADE_MS   330
#define AURA_MOTION_SPRING_MS 380

/* Divisores de HZ para las cadencias con nombre del doc SS6/SS7 -- el
 * llamador arma su propio "HZ / AURA_MOTION_*_HZ_DIV" donde HZ si esta
 * disponible (aura_main.c y companeros). Un unico lugar con estos
 * numeros, no una constante suelta reinventada por pantalla. */
#define AURA_MOTION_FADE_FPS            20 /* fundidos: HZ/20 */
#define AURA_MOTION_PAN_HZ_DIV_MIN       6 /* paneos lentos: HZ/6..HZ/8 */
#define AURA_MOTION_PAN_HZ_DIV_MAX       8
#define AURA_MOTION_LETTER_HOP_HZ_DIV   10 /* hojear por letras, doc SS7 */
#define AURA_MOTION_MINIPLAYER_HZ_DIV   12 /* scroll de texto del mini-reproductor */

/* Fundido lineal: interpolacion directa, sin tabla (es lineal por
 * definicion -- SS9.2 reserva la tabla precomputada para el resorte,
 * "caratulas, fundidos de pantalla... siguen en fundido lineal").
 * Devuelve un progreso en punto fijo [0, 256]; clampa fuera de rango. */
int aura_motion_linear(long elapsed, long duration);

/* Resorte corto con sobrepaso (doc SS6, decidido en SS9.2: "tabla de
 * easing precomputada, no fisica de resorte con estado"). Curva
 * easeOutBack estandar (s=1.70158) muestreada en 24 pasos y
 * reescalada a punto fijo /256 -- reservada a la capa de controles
 * (pastilla de seleccion, progreso si se anima su aparicion), nunca al
 * contenido. Puede devolver algo MAS de 256 en el tramo de sobrepaso
 * antes de asentarse en 256 -- es la forma de la curva, no un error;
 * el llamador que la usa para interpolar una posicion debe esperar
 * (y permitir) ese breve exceso. Clampa a 0 antes de `elapsed<=0` y a
 * 256 despues de `elapsed>=duration`. */
int aura_motion_spring(long elapsed, long duration);

#endif /* AURA_MOTION_H */
