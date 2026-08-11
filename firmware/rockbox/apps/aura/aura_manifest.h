/* Fase 24 (PLAN-UX.md): contadores/bytes por tipo que Aura Studio deja
 * en el dispositivo en cada sync, para que "Acerca de" pueda mostrar
 * cuanto hay realmente sincronizado. Ver aura_manifest.c y
 * CatalogSummaryWriter (Aura Studio, Services/CatalogSummary.swift).
 */
#ifndef AURA_MANIFEST_H
#define AURA_MANIFEST_H

#include <stdbool.h>

typedef struct {
    int music_count;
    long long music_bytes;
    int video_count;
    long long video_bytes;
    int photo_count;
    long long photo_bytes;
    int playlist_count;
} aura_manifest_t;

/* Lee el resumen que Aura Studio escribe en cada sync a *out. Devuelve
 * false si el archivo todavia no existe (el dispositivo nunca se
 * sincronizo desde Studio) -- *out queda en cero en ese caso, nunca sin
 * inicializar. */
bool aura_manifest_load(aura_manifest_t *out);

#endif /* AURA_MANIFEST_H */
