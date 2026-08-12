/* ClockIndicator (PLAN.md T2.5, componentes/clock-indicator.md):
 * elemento de hora dentro de StatusBar. Alcance real de esta pasada --
 * ver DECISIONS.md D-094 para el razonamiento completo:
 *
 * - Modo "persistente" (siempre visible): COMPLETO, con entrada/salida
 *   Drop-and-Lift al activarse/desactivarse el ajuste.
 * - Modo "auto-oculta": el propio documento deja abierto que la
 *   dispara -- "avisame si en realidad querias algo de easing" es la
 *   unica pregunta que el documento SI responde; "que dispara la
 *   revelacion inicial" queda como pregunta explicitamente sin
 *   contestar. Sin un disparador definido, no hay nada que implementar
 *   todavia -- BLOCKED.md.
 * - Atajo "mantener Select": depende de un gesto de "hold" que no
 *   existe en el vocabulario de botones de Aura (mismo bloqueo ya
 *   documentado para Quickscreen, D-077) -- BLOCKED.md.
 * - Interaccion con DynamicTitle en (full) (Push-and-Pull): depende de
 *   que DynamicTitle exista (T2.6) -- se conecta ahi.
 */
#ifndef AURA_CLOCK_INDICATOR_H
#define AURA_CLOCK_INDICATOR_H

/* Dibuja el reloj centrado en [x, x+width) x [y, y+AURA_DS_METRICS_
 * CLOCK_INDICATOR_HEIGHT), formato HH:MM (reusa global_settings.
 * timeformat, D-021 -- ningun formato propio inventado). `enter_progress_256`
 * es el progreso [0,256] de la animacion Drop-and-Lift (T1.1: un solo
 * paso lineal, aplicado aca por el llamador via aura_pattern_lerp) --
 * 256 = en su posicion final, 0 = fuera de pantalla por arriba, valores
 * intermedios interpolan la caida. Pasar 256 directo para "siempre
 * visible sin animar" (p. ej. el modo persistente ya asentado). */
void aura_clock_indicator_draw(int x, int width, int y, int enter_progress_256);

#endif /* AURA_CLOCK_INDICATOR_H */
