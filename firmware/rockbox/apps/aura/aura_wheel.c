#include "aura_wheel.h"

int aura_wheel_step(int velocity_deg_s)
{
    long v, thr, scaled;

    if (velocity_deg_s <= 0)
        return 1;

    thr = AURA_WHEEL_LETTER_HOP_THRESHOLD_DEG_S;
    v = velocity_deg_s;
    if (v > thr)
        v = thr; /* mas alla del umbral es el modo de hojeo, no mas paso */

    /* step = 1 + round(2 * (v/thr)^2), en enteros: redondeo por +mitad
     * del divisor antes de la division. */
    scaled = 1 + (2 * v * v + (thr * thr) / 2) / (thr * thr);

    if (scaled > 3)
        scaled = 3;
    if (scaled < 1)
        scaled = 1;
    return (int)scaled;
}

int aura_wheel_should_hop_letters(int velocity_deg_s)
{
    return velocity_deg_s > AURA_WHEEL_LETTER_HOP_THRESHOLD_DEG_S;
}
