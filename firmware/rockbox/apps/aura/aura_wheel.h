/* Dinamica de rueda (Fase 29, PLAN-APPLE2026.md, doc de diseno SS7).
 * Modulo puro en C99, sin dependencias de Rockbox -- mismo criterio que
 * aura_nav.c/aura_motion.c, compila igual en el host que en el firmware.
 *
 * La velocidad angular real (grados/seg) NO se mide aca: el driver del
 * clickwheel de ipod6g ya la calcula y la suaviza en hardware
 * (firmware/target/arm/ipod/button-clickwheel.c, HAVE_SCROLLWHEEL) y la
 * manda como dato adjunto de cada BUTTON_SCROLL_FWD/BACK -- el llamador
 * la lee con `button_get_data() & 0xFFFFFF` despues de aura_main.c
 * normalizar el boton. Este modulo solo traduce esa velocidad a
 * decisiones de navegacion.
 */
#ifndef AURA_WHEEL_H
#define AURA_WHEEL_H

/* Umbral de hojeo por letras (doc SS7: ">420 grados/seg"). */
#define AURA_WHEEL_LETTER_HOP_THRESHOLD_DEG_S 420

/* Cuantos items avanzar por evento de scroll segun la velocidad angular.
 * Girar lento (o velocidad 0 -- el arnes de botones pautado y los
 * eventos sinteticos siempre la reportan asi) da precision absoluta: 1.
 * Aceleracion intermedia suave con v^2 hasta el umbral de hojeo, tope
 * x3 (doc: "x2-3 maximo") -- mas alla del umbral es un modo de
 * navegacion distinto (hojear por letras, ver
 * aura_wheel_should_hop_letters()), no "mas de lo mismo". */
int aura_wheel_step(int velocity_deg_s);

/* True si la velocidad supera el umbral de hojeo por letras. Sin
 * consumidor real todavia: el riel A-Z (IndexRail, componentes/
 * index-rail.md -- construido en D-155, redefinido en D-276) es hoy un
 * indicador pasivo sin salto por letra, por decision del dueno (D-276,
 * encargo aparte). La deteccion queda lista para cuando ese salto se
 * construya -- debe saltar solo entre letras presentes en la lista. */
int aura_wheel_should_hop_letters(int velocity_deg_s);

#endif /* AURA_WHEEL_H */
