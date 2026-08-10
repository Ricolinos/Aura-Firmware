/* Parser de letras sincronizadas (.lrc).
 *
 * Modulo puro en C99, sin dependencias de Rockbox: se compila igual
 * dentro del firmware que como binario de test en el host (ver
 * apps/aura/test/). Sin asignacion dinamica -- el llamador reserva
 * `aura_lrc_t` (tamano fijo) y lo pasa por referencia.
 *
 * Formato soportado (estandar .lrc, ver p.ej. LRCLIB):
 *   [00:12.34]Texto de la linea
 *   [00:12.34][00:45.00]Texto que se repite en dos momentos
 *   [ar:Artista]  -- metadatos con tag de 2-3 letras: se ignoran
 *   [00:12]Texto  -- centesimas opcionales
 */
#ifndef AURA_LRC_H
#define AURA_LRC_H

#define AURA_LRC_MAX_LINES    600
#define AURA_LRC_LINE_MAX_LEN 128

typedef struct {
    long timestamp_ms;
    char text[AURA_LRC_LINE_MAX_LEN];
} aura_lrc_line_t;

typedef struct {
    aura_lrc_line_t lines[AURA_LRC_MAX_LINES];
    int count;
} aura_lrc_t;

/* Parsea el contenido completo de un .lrc (NUL-terminado) ya cargado en
 * memoria. Ignora lineas de metadata sin timestamp (p.ej. "[ar:...]").
 * Una linea con varios timestamps ("[00:12.00][00:45.00]texto") genera
 * una entrada por cada uno. El resultado queda ordenado por timestamp
 * ascendente sin importar el orden de entrada. Devuelve out->count.
 * Si el archivo tiene mas de AURA_LRC_MAX_LINES lineas con timestamp,
 * las que sobran se descartan (no hay overflow de buffer). */
int aura_lrc_parse(const char *buf, aura_lrc_t *out);

/* Indice de la linea activa para `position_ms` (la ultima cuyo
 * timestamp <= position_ms), o -1 si position_ms es anterior a la
 * primera linea o si `lrc` esta vacio. */
int aura_lrc_find_active_line(const aura_lrc_t *lrc, long position_ms);

#endif /* AURA_LRC_H */
