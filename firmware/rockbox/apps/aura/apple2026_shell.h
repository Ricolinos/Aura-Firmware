/* Aplicacion en runtime del sistema de diseno Apple2026: colores por
 * tema y fuentes bitmap cargadas desde disco (generadas por
 * design-system/generate.py, ver apple2026_tokens.h). Nombres y valores
 * salen de docs/design/Reglas de diseno Apple2026 (v2).md SS2/SS3 -- este
 * archivo es la superficie `a26_palette` que el documento nombra. */
#ifndef APPLE2026_SHELL_H
#define APPLE2026_SHELL_H

#include <stdbool.h>

#include "lcd.h"
#include "aura_category.h"

typedef enum {
    /* Fondo de toda pantalla. */
    A26_SHELL_BG = 0,
    A26_TEXT_PRIMARY,
    A26_TEXT_SECONDARY,
    A26_TEXT_TERTIARY,
    /* Color de contraste de la marca: iconos de menu, estado activo,
     * contenido (texto/icono) de la fila seleccionada. NO es el fondo
     * de la seleccion, y NO es el relleno de barras de progreso/nivel
     * -- eso es siempre A26_PROGRESS_FILL, incluso mientras se edita
     * con la rueda (AUDITORIA-01 ambiguedad A-f, resuelta: un solo
     * criterio para toda barra de progreso/nivel del sistema, sin
     * excepcion por estado de edicion). Ver Principio 2. */
    A26_ACCENT,
    /* Separadores, bordes finos -- tambien el token que reemplaza tanto
     * al viejo "surface" (relleno neutro de controles no activos) como
     * al viejo "border": el documento no distingue esos dos casos, los
     * dos son SHELL_RAIL. */
    A26_SHELL_RAIL,
    A26_PROGRESS_FILL,
    A26_PROGRESS_TRACK,
    /* Fondo de la pastilla de seleccion de lista (gris neutro). El
     * contenido que va encima (texto/icono de la fila activa) usa
     * A26_ACCENT directamente -- el documento no define un token de
     * contraste separado, el acento ES el estado activo. */
    A26_SELECTION_FILL,
} a26_token_t;

typedef enum {
    A26_FONT_STYLE_TITLE = 0,
    A26_FONT_STYLE_BODY,
    A26_FONT_STYLE_CAPTION,
    /* Mismo tamano que CAPTION (13px) pero cara Semibold -- doc SS3:
     * "Semibold -> titulos de pantalla". CAPTION (Regular) sigue
     * existiendo para texto secundario real (artista en Ahora suena);
     * no se reemplaza, se agrega (AUDITORIA-01 A-04/A-a). */
    A26_FONT_STYLE_HEADER,
    A26_FONT_STYLE_MICRO,
    /* Fuentes SF Pro del sistema nuevo (docs/aura-design-system/,
     * fundamentos/02-tipografia.md -- PLAN.md T0.2 genero los .fnt,
     * T2.3 los carga por primera vez). Nombradas por ESTILO real
     * (tamano+peso), no por rol de componente -- varios roles
     * documentados comparten un mismo estilo (np_album/np_artist/
     * lyrics son los tres 12px Regular = DS_REG_12) y cargar un
     * archivo repetido en mas de un slot desperdiciaria presupuesto de
     * MAXUSERFONTS (hoy 14 = 9 estilos ds_* + las 5 de arriba, sin
     * hueco libre: nacio en 12 con D-086, D-263 lo subio a 13 para el
     * octavo estilo ds_bold_16 -- RETIRADO en D-267, ver abajo -- y
     * D-267 lo subio de nuevo a 14). El mapeo rol->estilo vive en
     * design-system/tokens.json -> aura_ds.type_scale_roles,
     * documentacion para humanos; el codigo C usa el estilo
     * directamente. */
    /* Los comentarios marcan los consumidores REALES de cada estilo,
     * confirmados por grep contra los .c de apps/aura/ (no por suposicion --
     * un comentario viejo aca decia que DS_BOLD_8 "paso a" otro estilo
     * cuando en realidad nunca tuvo consumidor propio; el encargo de
     * tipografia de 2026-08-14 lo destapo auditando de verdad antes de
     * tocar nada, ver DECISIONS.md). Varios estilos se REUTILIZAN entre
     * componentes sin agregar fuente nueva -- D-195 y ese mismo encargo
     * documentan cada reutilizacion en su propio punto de uso. */
    A26_FONT_STYLE_DS_REG_8,      /* riel A-Z, calendario, busqueda */
    A26_FONT_STYLE_DS_SEMIBOLD_15, /* menu_item (MenuList/LeftPanel) -- unico estilo Semibold del sistema. D-267: renombrado de DS_SEMIBOLD_14 (mismo unico consumidor, +1pt) */
    A26_FONT_STYLE_DS_REG_10,     /* alarmas, algunas pantallas de aura_screens.c -- ya NO selection_summary (D-267/D-271, ver DS_MEDIUM_16) */
    A26_FONT_STYLE_DS_BOLD_10,    /* np_counter */
    A26_FONT_STYLE_DS_REG_12,     /* np_album, np_artist, lyrics, statusbar_time (encargo 2026-08-14, +2px reusando este estilo) */
    A26_FONT_STYLE_DS_BOLD_12,    /* np_title, statusbar_title (encargo 2026-08-14, +2px reusando este estilo) */
    A26_FONT_STYLE_DS_BOLD_14,    /* lyrics_active, filas de listas de contenido (D-205) */
    A26_FONT_STYLE_DS_BOLD_18,    /* selection_summary texto superior (D-271, medido contra un mockup pixel-exacto -- reemplaza DS_BOLD_13 de D-267, que resultaba mas chico que el objetivo real) */
    A26_FONT_STYLE_DS_MEDIUM_16,  /* selection_summary texto inferior (D-271, reemplaza DS_MEDIUM_12 de D-267 por el mismo motivo -- primer consumidor real de la cara Medium desde D-267) */
    A26_FONT_STYLE_COUNT,
} a26_font_style_t;

/* Carga las 4 fuentes Apple2026 desde FONT_DIR. Si alguna falta (p. ej.
 * no se corrio design-system/generate.py + build_sim.sh), degrada a
 * FONT_SYSFIXED en vez de fallar: la UI sigue siendo usable. Llamar una
 * vez al arrancar, despues de aura_settings_load(). */
void a26_shell_init(void);

/* Color del token dado, resuelto contra aura_settings.theme, ya
 * empaquetado al formato nativo del LCD (listo para
 * lcd_set_foreground/background). */
unsigned a26_color(a26_token_t token);

/* Acento configurable del sistema docs/aura-design-system/ (PLAN.md T0.3,
 * fundamentos/01-color.md). Lee aura_settings.accent_rgb24 en runtime --
 * NUNCA usar AURA_DS_COLOR_ACCENT_DEFAULT directo en un componente, ese
 * define es solo el valor de fabrica. Empaquetado al formato nativo del
 * LCD, listo para lcd_set_foreground/background. */
unsigned aura_accent(void);

/* Version del acento vigente mezclada hacia blanco/negro en
 * AURA_DS_COLOR_ACCENT_DERIVED_LIGHTEN_PCT/_DARKEN_PCT (D-086, G9) --
 * los dos colores derivados del degradado de SelectionSummary. Se
 * recalculan cada vez que se llaman (no cachean): son baratos (aura_color.c
 * es aritmetica entera pura) y siempre reflejan el acento vigente aunque
 * el usuario lo haya cambiado sin reiniciar. */
unsigned aura_accent_light(void);
unsigned aura_accent_dark(void);

/* Jerarquia de color por categoria del Menu principal (encargo del dueno
 * 2026-08-14, design-system/tokens.json -> aura_ds.color.category):
 * Musica sigue el acento configurable (identico a aura_accent_light/
 * _dark de arriba); Ajustes/Video/Fotos son colores FIJOS (no siguen
 * tema ni acento, a proposito); Extras es el unico caso de degradado
 * entre DOS TONOS (amarillo -> acento) en vez de luz/sombra de un solo
 * tono. Los 3 colores que devuelve son los mismos 3 puntos que ya usaba
 * el degradado diagonal del tile de SelectionSummary (esquina clara,
 * centro, esquina oscura) -- `color_center` es el color "real" de la
 * categoria (para consumidores de un solo tono, p. ej. el degradado de
 * icono de 2 puntos en aura_widgets.c, que usa `color_a`/`color_b` y
 * descarta el centro). AURA_CATEGORY_NONE se resuelve igual que
 * AURA_CATEGORY_MUSIC (fallback seguro al comportamiento de siempre). */
void aura_category_gradient(aura_category_t cat,
                             unsigned *color_a, unsigned *color_center,
                             unsigned *color_b);

/* Sombra de LeftPanel sobre el contenido a su derecha (PLAN.md T0.4,
 * efectos/01-sombras.md: "SelectionSummary y CoverDrift SIEMPRE
 * renderizan una sombra que simula que LeftPanel esta por encima").
 * Sin alfa real en este LCD: se aproxima oscureciendo una banda de
 * AURA_DS_METRICS_SHADOW_LEFT_PANEL_SHADOW_WIDTH columnas junto al
 * borde derecho del panel, con caida lineal de opacidad (mezcla contra
 * negro via a26_shell_blend(), mismo mecanismo que el fundido de la
 * barra de deslizamiento) -- funciona bien contra un fondo plano
 * conocido (SHELL_BG); sobre contenido rico (carratula, foto) es una
 * aproximacion, no compositing real. No-op si el usuario desactivo el
 * efecto desde Ajustes (aura_settings.left_panel_shadow). `x` es el
 * borde derecho del panel (la sombra crece HACIA la derecha desde ahi);
 * `y`/`height` acotan verticalmente la banda. */
void aura_shell_draw_left_panel_shadow(int x, int y, int height);

/* D-258: variante de COMPOSITING REAL para "contenido rico" (la
 * limitacion que el comentario de arriba ya dejaba anotada) -- en vez
 * de mezclar contra un fondo plano fijo (SHELL_BG), lee el pixel YA
 * dibujado en el framebuffer (lo que sea que este ahi -- una caratula
 * de CoverDrift, por ejemplo) y lo oscurece en el lugar. Debe llamarse
 * DESPUES de dibujar ese contenido, nunca antes (si se llama antes,
 * el contenido posterior la tapa por completo -- bug real de D-254/
 * D-256, corregido en D-258 moviendo la llamada al final en
 * aura_coverdrift_draw()). Mismos parametros/ancho/opacidad/caida que
 * la variante de fondo plano. */
void aura_shell_draw_left_panel_shadow_over_content(int x, int y, int height);

/* Font id (para lcd_setfont) del estilo dado. */
int a26_font(a26_font_style_t style);

/* lcd_set_background(fondo del tema) + lcd_clear_viewport(). */
void a26_shell_clear_screen(void);

/* Pinta las 4 esquinas de pantalla redondeadas (radio
 * A26_LAYOUT_CORNER_RADIUS_SCREEN, doc SS5) sobre lo que ya este
 * dibujado, con el fondo del tema activo. Idempotente: se puede llamar
 * una vez por cuadro sin llevar estado propio ni necesitar limpiar nada
 * antes. Se llama UNA vez desde el bucle principal (aura_main.c),
 * despues de dibujar la pantalla y antes de lcd_update() -- no en cada
 * cuadro de una transicion (aura_transitions.c), donde el contenido en
 * movimiento hace que el recorte de esquina sea mucho menos perceptible
 * y el costo extra por cuadro no se justifica. */
void a26_shell_stamp_corners(void);
void a26_shell_stamp_corners_left_only(void);

/* Interruptor global de sombras paralelas (encargo 2026-08-13, ajuste
 * "Mostrar sombras"): TODA sombra del sistema -- la del LeftPanel, la
 * difusa del album y la de la hoja del Modo 4, la del indicador de la
 * barra -- consulta esto antes de dibujarse. */
bool aura_shadows_enabled(void);

/* Rellena un rectangulo con esquinas redondeadas: fillrect completo +
 * mascara de las 4 esquinas con `bg` (mismo corte por distancia que
 * a26_shell_stamp_corners, generalizado). `bg` es el color que rodea al
 * rectangulo -- normalmente A26_SHELL_BG, salvo que el rectangulo se
 * dibuje sobre otra superficie ya pintada. Radio de la tabla SS5.4:
 * nunca un valor suelto por componente. Primitiva compartida por la
 * pastilla de seleccion (SS5.1), el contenedor modal y la capsula de
 * progreso flotante (SS5.2). */
void a26_shell_fill_rounded_rect(int x, int y, int w, int h, int radius,
                                  unsigned fill, unsigned bg);

/* Recorta las 4 esquinas de un rectangulo YA DIBUJADO (p. ej. un
 * lcd_bitmap() de una imagen) al radio dado, pintandolas con `bg` --
 * mismo corte por distancia que a26_shell_stamp_corners()/
 * a26_shell_fill_rounded_rect(), pero sin rellenar el rectangulo
 * primero (el contenido interior ya existe). `bg` es el color que
 * rodea al rectangulo, igual que en las otras dos primitivas. Unico
 * consumidor hoy: la caratula de Ahora suena (doc "Reproductor - Ahora
 * suena.md" SS3, radio 8px -- AUDITORIA-01 A-07). */
void a26_shell_round_bitmap_corners(int x, int y, int w, int h, int radius,
                                     unsigned bg);

/* Variante de compositing REAL de la de arriba (D-267): en vez de un
 * color plano `bg`, restaura los pixeles REALES capturados en `saved`
 * (instantanea de w x h, fila-mayor, stride=w, tomada ANTES de dibujar
 * el bitmap encima) -- para cuando lo que rodea al bitmap es una imagen
 * rica, no un fondo solido conocido (mismo problema que D-258 resolvio
 * para la sombra del LeftPanel, aca para esquinas redondeadas). */
void a26_shell_round_bitmap_corners_over_content(int x, int y, int w, int h,
                                                   int radius,
                                                   const fb_data *saved, int saved_w);

/* Igual que a26_shell_fill_rounded_rect(), con un borde de 1px encima
 * (radio interior = radio-1, misma jerarquia concentrica que pide SS5.4
 * para hijo/padre). `bg` es, otra vez, lo que rodea al rectangulo entero
 * -- se usa para recortar las esquinas exteriores del borde. Usan esto
 * el contenedor modal y la capsula flotante de espera (SS5.2), las dos
 * unicas superficies del sistema con borde en vez de solo relleno. */
void a26_shell_outline_rounded_rect(int x, int y, int w, int h, int radius,
                                     unsigned fill, unsigned border, unsigned bg);

/* Interpola linealmente entre dos colores empaquetados del LCD.
 * `alpha_256` en [0, 256]: 0 = `from`, 256 = `to`. Unico uso hoy: el
 * fundido de aparicion/desvanecido de la barra de deslizamiento (SS5.3)
 * -- Aura no tiene compositor alfa por pixel, asi que un color
 * intermedio precalculado es la forma de simular la transparencia que
 * el documento pide sin tocar el framebuffer por debajo. */
unsigned a26_shell_blend(unsigned from, unsigned to, int alpha_256);

/* sqrt entera en punto fijo 24.8 (isqrt256(v) ~= sqrt(v)*256) --
 * compartida por los recortes de esquina antialiasados (stamp_corner
 * de este modulo, mask_corners_* de caratulas). */
unsigned a26_shell_isqrt256(unsigned v);

/* Capsula horizontal con extremos semicirculares EXACTOS (radio h/2
 * fraccionario, cobertura subpixel) -- para las piezas donde "puntas
 * completamente redondeadas" es requisito (barra del reproductor).
 * `bg` = color plano real debajo. */
void a26_shell_fill_capsule(int x, int y, int w, int h, unsigned fill, unsigned bg);

/* Igual pero mezclando contra el framebuffer (fondo heterogeneo) con
 * alpha global -- pildora del indicador y su sombra. */
void a26_shell_fill_capsule_over(int x, int y, int w, int h, unsigned fill, int alpha_256);

#endif /* APPLE2026_SHELL_H */
