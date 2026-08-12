#include "aura_art.h"

int aura_art_reflection_height(int size, int height_pct)
{
    return size * height_pct / 100;
}

void aura_art_generate_reflection(const fb_data *cover, fb_data *out,
                                   int size, int height_pct, unsigned bg_color)
{
    int refl_h = aura_art_reflection_height(size, height_pct);
    int bg_r = RGB_UNPACK_RED(bg_color);
    int bg_g = RGB_UNPACK_GREEN(bg_color);
    int bg_b = RGB_UNPACK_BLUE(bg_color);
    int y, x;

    for (y = 0; y < refl_h; y++)
    {
        /* Fila y del reflejo = fila (size-1-y) de la caratula, invertida
         * verticalmente, difuminada hacia el fondo segun se aleja del
         * borde -- mascara 35%->0% (doc SS5.4). */
        const fb_data *src_row = cover + (size_t)(size - 1 - y) * size;
        fb_data *dst_row = out + (size_t)y * size;
        int fade = 255 - (255 * y) / refl_h; /* 255=espejo nitido .. 0=fondo puro */

        for (x = 0; x < size; x++)
        {
            unsigned px = src_row[x];
            int r = RGB_UNPACK_RED(px);
            int g = RGB_UNPACK_GREEN(px);
            int b = RGB_UNPACK_BLUE(px);

            r = bg_r + ((r - bg_r) * fade) / 255;
            g = bg_g + ((g - bg_g) * fade) / 255;
            b = bg_b + ((b - bg_b) * fade) / 255;

            dst_row[x] = LCD_RGBPACK(r, g, b);
        }
    }
}
