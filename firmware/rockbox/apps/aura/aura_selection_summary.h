/* SelectionSummary (PLAN.md T2.8, componentes/selection-summary.md):
 * estado base/vacio del panel derecho -- icono sobre tile con degradado
 * del acento + hasta dos lineas de texto, cuando no hay contenido mas
 * rico (caratulas, fotos) para mostrar ahi.
 *
 * Alcance real de esta pasada (ver DECISIONS.md D-097):
 * - Icono ESTATICO sobre el tile de degradado, dos slots de texto con
 *   MarqueeText en overflow, cambio instantaneo de valor (sin
 *   Fade-Slide/Scroll-Slide, tal como pide el documento), sombra de
 *   LeftPanel -- completos y verificados.
 * - Variante DINAMICA del icono (reloj analogico, hoja de calendario
 *   para Ajustes > Fecha y Hora) NO implementada -- el propio documento
 *   deja sin resolver si es una variante de este componente o uno
 *   distinto ("todavia sin resolver"). Registrado en BLOCKED.md B-04,
 *   no es una simplificacion de alcance sino una pregunta de
 *   arquitectura que el documento mismo no contesta.
 * - Cross-fade con CoverDrift (T2.9) NO conectado -- CoverDrift no
 *   existe todavia. Diferido, no bloqueado: se conecta cuando T2.9
 *   exista, el mecanismo (a26_shell_blend hacia el color ya visible)
 *   ya esta disponible.
 */
#ifndef AURA_SELECTION_SUMMARY_H
#define AURA_SELECTION_SUMMARY_H

/* Dibuja en la franja [x, x+width) x [0, A26_SCREEN_HEIGHT) -- todo el
 * espacio vertical disponible del panel derecho (StatusBar en (split)
 * solo vive sobre el panel izquierdo, doc status-bar.md, asi que este
 * componente no necesita dejarle hueco arriba).
 *
 * `icon_name` nunca NULL (siempre hay un icono por item de menu, regla
 * de diseno cerrada en el documento). `top_text` puede ser NULL (slot
 * superior opcional); `bottom_text` casi siempre presente pero tambien
 * acepta NULL para cubrir el caso limite. Ninguno de los dos anima su
 * cambio de VALOR (icono y texto cambian de forma instantanea) -- solo
 * el loop de MarqueeText si el texto no cabe.
 */
void aura_selection_summary_draw(int x, int width,
                                  const char *icon_name,
                                  const char *top_text,
                                  const char *bottom_text);

/* Mismo par pending()/animating() que aura_statusbar_title_*() y
 * aura_menu_list_scroll_indicator_*() -- cubre AMBOS slots de texto (o
 * cualquiera de los dos desbordando cuenta). aura_main.c los consulta
 * con la misma cadencia gruesa/fina ya establecida. */
int aura_selection_summary_pending(void);
int aura_selection_summary_animating(void);

#endif /* AURA_SELECTION_SUMMARY_H */
