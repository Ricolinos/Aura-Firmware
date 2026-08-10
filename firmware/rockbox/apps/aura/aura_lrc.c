#include <string.h>

#include "aura_lrc.h"

#define MAX_TAGS_PER_LINE 8

static int is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/* Intenta parsear "mm:ss" o "mm:ss.frac" a partir de `s` (que no
 * incluye los corchetes). Devuelve 1 y deja el valor en *out_ms si el
 * contenido completo (hasta `len`) es un timestamp valido; 0 si no
 * (entonces es un tag de metadata como "ar:Artista" y se ignora). */
static int parse_timestamp(const char *s, int len, long *out_ms)
{
    int i = 0;
    long mm = 0, ss = 0, frac = 0;
    int frac_digits = 0;

    if (len < 3)
        return 0;

    while (i < len && is_digit(s[i]))
    {
        mm = mm * 10 + (s[i] - '0');
        i++;
    }
    if (i == 0 || i >= len || s[i] != ':')
        return 0;
    i++;

    int ss_start = i;
    while (i < len && is_digit(s[i]))
    {
        ss = ss * 10 + (s[i] - '0');
        i++;
    }
    if (i == ss_start)
        return 0;

    if (i < len && s[i] == '.')
    {
        i++;
        int frac_start = i;
        while (i < len && is_digit(s[i]))
        {
            frac = frac * 10 + (s[i] - '0');
            i++;
        }
        frac_digits = i - frac_start;
        if (frac_digits == 0)
            return 0;
    }

    if (i != len)
        return 0; /* sobra basura: no es un timestamp puro */

    /* Normaliza la fraccion a milisegundos segun cuantos digitos traia. */
    if (frac_digits == 1)
        frac *= 100;
    else if (frac_digits == 2)
        frac *= 10;
    else if (frac_digits >= 3)
    {
        for (int d = frac_digits; d > 3; d--)
            frac /= 10;
    }

    *out_ms = mm * 60000 + ss * 1000 + frac;
    return 1;
}

static void trim(const char **s, int *len)
{
    while (*len > 0 && ((*s)[0] == ' ' || (*s)[0] == '\t'))
    {
        (*s)++;
        (*len)--;
    }
    while (*len > 0 && ((*s)[*len - 1] == ' ' || (*s)[*len - 1] == '\t'
                        || (*s)[*len - 1] == '\r'))
        (*len)--;
}

static void parse_line(const char *line, int line_len, aura_lrc_t *out)
{
    long pending[MAX_TAGS_PER_LINE];
    int pending_count = 0;
    const char *p = line;
    int remaining = line_len;

    trim(&p, &remaining);

    while (remaining > 0 && p[0] == '[')
    {
        const char *close = memchr(p, ']', remaining);
        if (!close)
            break; /* corchete sin cerrar: se deja de leer tags */

        int tag_len = (int)(close - p) - 1;
        long ms;
        if (pending_count < MAX_TAGS_PER_LINE && parse_timestamp(p + 1, tag_len, &ms))
            pending[pending_count++] = ms;
        /* si no es timestamp (p.ej. "ar:Artista"), se ignora */

        int consumed = (int)(close - p) + 1;
        p += consumed;
        remaining -= consumed;
    }

    if (pending_count == 0)
        return; /* linea de metadata pura, o sin ningun tag valido */

    trim(&p, &remaining);
    if (remaining >= AURA_LRC_LINE_MAX_LEN)
        remaining = AURA_LRC_LINE_MAX_LEN - 1;

    for (int t = 0; t < pending_count; t++)
    {
        if (out->count >= AURA_LRC_MAX_LINES)
            return;
        aura_lrc_line_t *dst = &out->lines[out->count];
        dst->timestamp_ms = pending[t];
        memcpy(dst->text, p, remaining);
        dst->text[remaining] = '\0';
        out->count++;
    }
}

static void sort_by_timestamp(aura_lrc_t *out)
{
    /* Insertion sort: simple, sin dependencias, y O(n) en el caso
     * habitual de un .lrc ya casi ordenado. */
    for (int i = 1; i < out->count; i++)
    {
        aura_lrc_line_t key = out->lines[i];
        int j = i - 1;
        while (j >= 0 && out->lines[j].timestamp_ms > key.timestamp_ms)
        {
            out->lines[j + 1] = out->lines[j];
            j--;
        }
        out->lines[j + 1] = key;
    }
}

int aura_lrc_parse(const char *buf, aura_lrc_t *out)
{
    const char *p = buf;

    out->count = 0;

    while (*p)
    {
        const char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);

        parse_line(p, len, out);

        if (!nl)
            break;
        p = nl + 1;
    }

    sort_by_timestamp(out);
    return out->count;
}

int aura_lrc_find_active_line(const aura_lrc_t *lrc, long position_ms)
{
    if (lrc->count == 0 || position_ms < lrc->lines[0].timestamp_ms)
        return -1;

    int lo = 0, hi = lrc->count - 1, result = -1;
    while (lo <= hi)
    {
        int mid = lo + (hi - lo) / 2;
        if (lrc->lines[mid].timestamp_ms <= position_ms)
        {
            result = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return result;
}
