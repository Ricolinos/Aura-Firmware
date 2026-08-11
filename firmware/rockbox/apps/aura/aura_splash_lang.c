#include <string.h>
#include <stdio.h>

#include "aura_splash_lang.h"
#include "aura_settings.h"

typedef struct {
    const char *match; /* texto (o prefijo) original en ingles de Rockbox */
    int exact;          /* 1 = todo el texto debe coincidir, 0 = solo el prefijo */
    const char *es;
    const char *en;
} splash_rule_t;

/* Textos fuente extraidos literal de apps/lang/english.lang (el unico
 * idioma que Rockbox realmente carga -- Aura no usa su sistema de
 * idiomas, D-013). Las reglas de prefijo preservan lo que venga
 * despues (un sufijo dinamico como "[3/9]" o "PLAY/PAUSE to abort)").
 * El orden importa: los prefijos mas largos van antes que sus
 * variantes mas cortas. */
static const splash_rule_t s_rules[] = {
    { "Loading... (",                              0,
      "Cargando… (",                               "Loading... (" },
    { "Loading...",                                1,
      "Cargando…",                                  "Loading..." },
    { "Scanning disk...",                          1,
      "Preparando el disco…",                       "Preparing storage..." },
    { "Shutting down...",                          1,
      "Apagando…",                                   "Shutting down..." },
    { "Database is not ready",                     1,
      "Terminando de preparar la biblioteca…",       "Finishing up your library..." },
    { "WARNING! Low Battery! Shutting down...",    1,
      "Batería baja. Apagando…",                     "Low battery. Shutting down..." },
    { "Battery empty! RECHARGE! Shutting down...", 1,
      "Batería agotada. Conecta el cargador.",       "Battery empty. Plug in your charger." },
    { "Committing database [",                     0,
      "Preparando la biblioteca [",                  "Preparing your library [" },
};

void aura_splash_translate(char *buf, size_t bufsz)
{
    size_t i;

    for (i = 0; i < sizeof(s_rules) / sizeof(s_rules[0]); i++)
    {
        const splash_rule_t *r = &s_rules[i];
        size_t mlen = strlen(r->match);
        int matches = r->exact ? !strcmp(buf, r->match)
                                : !strncmp(buf, r->match, mlen);
        const char *translated;

        if (!matches)
            continue;

        translated = (aura_settings.language == AURA_LANG_EN) ? r->en : r->es;

        if (r->exact)
        {
            snprintf(buf, bufsz, "%s", translated);
        }
        else
        {
            char rest[64];
            snprintf(rest, sizeof(rest), "%s", buf + mlen);
            snprintf(buf, bufsz, "%s%s", translated, rest);
        }
        return;
    }
}
