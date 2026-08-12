/* Nucleo matematico de los patrones de transicion con nombre propio
 * (docs/aura-design-system/transiciones/00-vocabulario.md, PLAN.md
 * T1.1). Modulo puro en C99, sin dependencias de Rockbox ni de HZ --
 * mismo criterio que aura_motion.c/aura_wheel.c/aura_flow.c/aura_color.c:
 * compila igual en el host (apps/aura/test/) que en el firmware. No
 * dibuja nada ni toca hardware ni conoce ticks reales -- el llamador
 * convierte su unidad de tiempo real (HZ) a ms antes de llamar aca, y
 * aplica el resultado a coordenadas de pantalla reales.
 *
 * Un patron = una funcion (o una combinacion de esta funcion +
 * aura_motion_linear() ya existente, para los patrones que son
 * "un paso lineal" aplicado a una posicion/tamano por el llamador --
 * Fade-Slide, Scroll-Slide, Drop-and-Lift, Push-and-Pull, Morph Directo
 * son todos ESE caso, con la direccion/eje decidida por cada componente,
 * no por este modulo). Los patrones con forma propia real (Marquee Loop,
 * Fade-on-Idle, la mecanica "termina en el centro" de CoverDrift) tienen
 * su funcion dedicada abajo.
 */
#ifndef AURA_PATTERNS_H
#define AURA_PATTERNS_H

/* -- Utilidad generica: interpolacion lineal entera -----------------------
 * Usada por los patrones "un paso" del vocabulario (Fade-Slide,
 * Scroll-Slide, Drop-and-Lift, Push-and-Pull, Morph Directo) junto con
 * aura_motion_linear() (progreso [0,256]) ya existente en aura_motion.h:
 * el llamador calcula el progreso ahi y aplica esta funcion a cada
 * propiedad numerica (posicion, tamano) que necesite interpolar. */
int aura_pattern_lerp(int a, int b, int progress_256);

/* -- Marquee Loop -----------------------------------------------------
 * transiciones/00-vocabulario.md: ciclo continuo estático->movimiento,
 * sin fase de entrada separada. `elapsed_ms` es el tiempo transcurrido
 * desde que el texto empezo a mostrarse (no desde el inicio del ciclo
 * actual -- esta funcion hace el modulo internamente). Devuelve el
 * desplazamiento horizontal actual en px, [0, scroll_distance_px]:
 * 0 durante la fase estatica, avanza linealmente durante la fase de
 * movimiento, vuelve a 0 al reiniciar el ciclo. `scroll_distance_px`
 * es tipicamente ancho_del_texto + espacio_entre_vueltas (el llamador
 * lo calcula, depende de una medida de texto real via lcd_getstringsize). */
int aura_pattern_marquee_offset(long elapsed_ms, long static_ms, long scroll_ms,
                                  int scroll_distance_px);

/* -- Fade-on-Idle -------------------------------------------------------
 * transiciones/00-vocabulario.md: aparece con actividad, se desvanece
 * tras idle. Misma forma que ya probo el fundido de la barra de
 * deslizamiento del sistema viejo (D-073), generalizada aca para
 * cualquier consumidor futuro del patron. `elapsed_ms` es el tiempo
 * desde el ULTIMO evento de actividad (se reinicia a 0 en cada uno).
 * Devuelve alpha en [0, 256]. */
int aura_pattern_fade_on_idle_alpha_256(long elapsed_ms, long fade_in_ms,
                                          long hold_ms, long fade_out_ms);

typedef struct {
    int dx, dy; /* offset desde el centro, px */
} aura_pattern_point_t;

/* -- CoverDrift: movimiento que TERMINA en el centro ---------------------
 * componentes/cover-drift.md: arranca descentrado en una de 8 direcciones
 * fijas (0/60/90/120/180/240/270/300 grados, AURA_DS_METRICS_COVER_DRIFT_
 * DIRECTIONS_DEG en tokens.json) y se desplaza HACIA el centro a
 * velocidad constante (interpretacion confirmada en PLAN.md: lineal, sin
 * easing). Devuelve el offset (dx,dy) desde el centro en el instante
 * `elapsed_ms` de un movimiento de duracion `duration_ms` que arranca a
 * `distance_px` del centro -- (0,0) cuando elapsed_ms >= duration_ms.
 * Angulo distinto a los 8 valores documentados devuelve (0,0) siempre
 * (mismo criterio defensivo que aura_wheel_step con velocidad invalida:
 * degrada a "sin movimiento", nunca a un valor inventado). Sin
 * trigonometria en punto flotante -- tabla fija de los 8 angulos reales
 * que CoverDrift usa, no una funcion seno/coseno de proposito general
 * que este sistema no necesita. */
aura_pattern_point_t aura_pattern_drift_pos(int angle_deg, int distance_px,
                                              long elapsed_ms, long duration_ms);

/* -- Push-and-Drop (coreografia, PLAN.md T1.2) ---------------------------
 * componentes/status-bar.md + transiciones/00-vocabulario.md: 3 fases
 * secuenciales -- push del contenido (sin su chrome, "hueco
 * intencional") seguido de drop del chrome nuevo cayendo desde arriba.
 * Esta funcion es SOLO la maquina de tiempo/fases (que fase, que tan
 * avanzada) -- el renderizado real (offscreen del contenido SIN su
 * banda de status bar durante la Fase A, blit del chrome cayendo en la
 * Fase B) necesita un chrome separable del contenido, que el sistema
 * viejo no tiene: aura_screens_draw() dibuja la barra de estado inline
 * como parte de cada pantalla, no como una capa aparte. Esa separacion
 * es el trabajo real de T2.7 (StatusBar v2, --layer-chrome como
 * concepto de verdad) -- esta funcion queda lista y probada para
 * cuando exista ese consumidor, en vez de construir un renderizado a
 * medias contra una arquitectura que todavia no lo permite.
 *
 * `reverse`=0 (split->full, el caso documentado): Fase A = empuje del
 * contenido (duracion `push_ms`), Fase B = caida del chrome (duracion
 * `drop_ms`). `reverse`=1 ((full)->(split), G6 -- "no definida
 * todavia", espejo provisional del patron): se invierte el ORDEN de
 * las fases ademas de la duracion de cada una (Fase A pasa a durar
 * `drop_ms`, Fase B a durar `push_ms`) -- el llamador interpreta el
 * significado geometrico exacto de cada fase segun `reverse` (en
 * split->full, Fase A mueve contenido y Fase B mueve chrome; en la
 * inversa, Fase A es el chrome "subiendo" -- espejo de caer -- y Fase B
 * el contenido "jalado" -- espejo de empujar). */
typedef enum {
    AURA_PUSH_DROP_PHASE_A = 0,
    AURA_PUSH_DROP_PHASE_B,
    AURA_PUSH_DROP_DONE,
} aura_push_drop_phase_t;

typedef struct {
    aura_push_drop_phase_t phase;
    int progress_256; /* progreso DENTRO de la fase actual, [0,256] */
} aura_push_drop_state_t;

aura_push_drop_state_t aura_pattern_push_and_drop(long elapsed_ms, long push_ms,
                                                     long drop_ms, int reverse);

#endif /* AURA_PATTERNS_H */
