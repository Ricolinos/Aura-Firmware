/* Carga de caratulas por album (no por pista actual -- eso ya lo
 * resuelve aura_nowplaying.c solo). Se usa desde el modo grafico
 * Completo: pantalla de inicio dividida y Coverflow (D-025). */
#ifndef AURA_ALBUMART_H
#define AURA_ALBUMART_H

#include <stdbool.h>
#include <stdint.h>

/* Bitmap cuadrado de lado `size` mas su reflejo (mismo ancho, la mitad
 * de alto) ya pre-renderizado -- ver aura_albumart_load(). */
typedef struct {
    int size;
    unsigned char *cover_data;      /* size*size, formato nativo del LCD */
    unsigned char *reflection_data; /* size*(size/2) */
    bool valid;
} aura_albumart_t;

/* Busca una pista cualquiera del album identificado por `album_seek`
 * (el result_seek de tag_album que ya se tenga de una busqueda previa,
 * ver aura_music.h) y le busca caratula con la misma logica que usa
 * Ahora Suena (find_albumart). Si la encuentra, la decodifica y genera
 * su reflejo. Devuelve false (out->valid queda en false) si no hay
 * caratula o no hay ninguna pista en ese album.
 *
 * El llamador es dueno de la memoria: antes de llamar debe fijar
 * out->size y out->cover_data/out->reflection_data apuntando a
 * buffers de al menos size*size*FB_DATA_SZ y size*(size/2)*FB_DATA_SZ
 * bytes respectivamente (para poder mantener varias caratulas
 * cacheadas a la vez, p.ej. en Coverflow). */
bool aura_albumart_load_for_album(int32_t album_seek, aura_albumart_t *out);

#endif /* AURA_ALBUMART_H */
