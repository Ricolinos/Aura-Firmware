#include "aura_motion.h"

int aura_motion_linear(long elapsed, long duration)
{
    if (elapsed <= 0 || duration <= 0)
        return 0;
    if (elapsed >= duration)
        return 256;
    return (int)(elapsed * 256 / duration);
}

/* easeOutBack de Penner (s=1.70158, la constante estandar de la
 * industria para este tipo de curva), 24 pasos, /256. Generada una
 * sola vez fuera de linea (no en el dispositivo: no hay FPU) con:
 *
 *   f(t) = 1 + (s+1)*(t-1)^3 + s*(t-1)^2,  t = i/24
 *
 * y redondeada a punto fijo. El pico (282, ~10% de sobrepaso) cae
 * alrededor del 60% del recorrido -- sobrepaso perceptible pero no "de
 * juguete" (riesgo senalado en PLAN-APPLE2026.md SS3), fiel al resorte
 * de iOS. */
static const int spring_table[25] = {
      0,  47,  89, 126, 158, 186, 209, 229, 245, 257, 267, 274, 278,
    281, 282, 281, 279, 276, 272, 269, 265, 261, 259, 257, 256,
};
#define SPRING_TABLE_STEPS 24 /* indices 0..24 */

int aura_motion_spring(long elapsed, long duration)
{
    long pos, idx, rem;
    int a, b;

    if (elapsed <= 0 || duration <= 0)
        return 0;
    if (elapsed >= duration)
        return 256;

    /* Posicion fraccionaria dentro de la tabla, en 1/256 de paso, para
     * interpolar linealmente entre las dos muestras mas cercanas -- 24
     * muestras alcanzan de sobra para un movimiento de ~380ms a la
     * cadencia de fundidos (20fps son ~7-8 cuadros), pero interpolar
     * evita un escalon perceptible si el llamador pide mas resolucion
     * (p. ej. el simulador a 60fps de host). */
    pos = elapsed * SPRING_TABLE_STEPS * 256 / duration;
    idx = pos / 256;
    rem = pos % 256;

    if (idx >= SPRING_TABLE_STEPS)
        return 256;

    a = spring_table[idx];
    b = spring_table[idx + 1];
    return a + (int)((b - a) * rem / 256);
}
