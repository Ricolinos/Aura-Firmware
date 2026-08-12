#include "aura_art.h"

int aura_art_reflection_height(int size, int height_pct)
{
    return size * height_pct / 100;
}

void aura_art_generate_reflection(const fb_data *cover, fb_data *out,
                                   int size, int height_pct, unsigned bg_color,
                                   bool transposed)
{
    int refl_h = aura_art_reflection_height(size, height_pct);
    int bg_r = RGB_UNPACK_RED(bg_color);
    int bg_g = RGB_UNPACK_GREEN(bg_color);
    int bg_b = RGB_UNPACK_BLUE(bg_color);
    int y, x;

    /* `transposed` (PLAN.md T3.2(a), componentes/cover-flow.md: cache
     * .pfraw guarda las caratulas TRANSPUESTAS -- columna contigua en
     * memoria, igual que pictureflow.c, regla dura 7) -- el espejo
     * vertical + atenuacion es una operacion por indices LOGICOS
     * (fila/columna), el layout de memoria solo cambia COMO se
     * calculan los offsets, no la formula. Nunca se transpone un
     * bitmap ya generado: se genera directo en el layout que hace
     * falta. */
    for (y = 0; y < refl_h; y++)
    {
        /* Fila y del reflejo = fila (size-1-y) de la caratula, invertida
         * verticalmente, difuminada hacia el fondo segun se aleja del
         * borde. */
        int source_row = size - 1 - y;
        int fade = 255 - (255 * y) / refl_h; /* 255=espejo nitido .. 0=fondo puro */

        for (x = 0; x < size; x++)
        {
            unsigned px = transposed ? cover[(size_t)x * size + source_row]
                                      : cover[(size_t)source_row * size + x];
            int r = RGB_UNPACK_RED(px);
            int g = RGB_UNPACK_GREEN(px);
            int b = RGB_UNPACK_BLUE(px);
            fb_data result;

            r = bg_r + ((r - bg_r) * fade) / 255;
            g = bg_g + ((g - bg_g) * fade) / 255;
            b = bg_b + ((b - bg_b) * fade) / 255;
            result = LCD_RGBPACK(r, g, b);

            if (transposed)
                out[(size_t)x * refl_h + y] = result;
            else
                out[(size_t)y * size + x] = result;
        }
    }
}
