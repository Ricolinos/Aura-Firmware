/* Carga de caratulas por album (no por pista actual -- eso lo resuelve
 * aura_nowplaying.c solo), con cache en disco `.pfraw` (PLAN.md
 * T3.2(a), componentes/cover-flow.md) -- unico consumidor real:
 * Coverflow (aura_coverflow.c).
 *
 * El bitmap queda TRANSPUESTO en memoria (columna contigua, no fila)
 * con las esquinas ya redondeadas horneadas -- mismo formato conceptual
 * que el cache de apps/plugins/pictureflow/pictureflow.c (regla dura 7:
 * extender, no reimplementar), personalizado con el enmascarado de
 * esquinas que pictureflow.c no tiene (ver aura_albumart.c). El
 * consumidor (draw_slide_perspective en aura_coverflow.c) lee este
 * layout directo -- ningun otro modulo debe asumir fila-contigua aqui.
 */
#ifndef AURA_ALBUMART_H
#define AURA_ALBUMART_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int size;
    int radius;                     /* esquina redondeada a hornear/verificar contra el cache */
    unsigned char *cover_data;      /* size*size, TRANSPUESTO, formato nativo del LCD */
    unsigned char *reflection_data; /* size*aura_art_reflection_height(size,...), tambien transpuesto */
    bool valid;
} aura_albumart_t;

/* Busca una pista cualquiera del album identificado por `album_seek`
 * (el result_seek de tag_album que ya se tenga de una busqueda previa,
 * ver aura_music.h) y le busca caratula con la misma logica que usa
 * Ahora Suena (find_albumart).
 *
 * Primero intenta el cache `.pfraw` en disco (clave: album_seek + size
 * + radius -- cambiar cualquiera de los dos invalida el cache de ese
 * album sin tocar los demas). Si no existe, decodifica el JPEG/BMP
 * real, transpone, hornea las esquinas y ESCRIBE el cache para la
 * proxima vez -- "cero decodificacion JPEG durante la animacion"
 * (doc) solo aplica una vez cada caratula ya paso por aqui.
 *
 * Devuelve false (out->valid queda en false) si no hay caratula o no
 * hay ninguna pista en ese album. El llamador es dueno de la memoria:
 * antes de llamar debe fijar out->size, out->radius y
 * out->cover_data/out->reflection_data apuntando a buffers de al menos
 * size*size*FB_DATA_SZ y size*aura_art_reflection_height(size,...)*FB_DATA_SZ
 * bytes respectivamente. */
bool aura_albumart_load_for_album(int32_t album_seek, aura_albumart_t *out);

/* Caratula "Default" para un album sin arte real (aura_albumart_load_for_album()
 * devolvio false) -- mismo lenguaje visual que SelectionSummary
 * (componentes/selection-summary.md: degradado diagonal del acento,
 * tres puntos claro/centro/oscuro) en vez del recuadro vacio que
 * dibujaba antes el llamador para este caso. Mismo formato (transpuesto,
 * esquinas horneadas al `out->radius` ya fijado, reflejo generado) que
 * una caratula real -- el consumidor (draw_slide_perspective) no
 * necesita distinguir entre los dos casos. Nunca falla (computo puro,
 * sin disco ni decodificacion) -- `out->valid` siempre queda en true. */
void aura_albumart_load_default(aura_albumart_t *out);

#endif /* AURA_ALBUMART_H */
