/* Aplicacion en runtime del sistema de diseno Apple2026: colores por
 * tema y fuentes bitmap cargadas desde disco (generadas por
 * design-system/generate.py, ver apple2026_tokens.h). Nombres y valores
 * salen de docs/design/Reglas de diseno Apple2026 (v2).md SS2/SS3 -- este
 * archivo es la superficie `a26_palette` que el documento nombra. */
#ifndef APPLE2026_SHELL_H
#define APPLE2026_SHELL_H

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
     * MAXUSERFONTS (12 exacto con estas 7 + las 5 de arriba, ver
     * D-086). El mapeo rol->estilo vive en
     * design-system/tokens.json -> aura_ds.type_scale_roles,
     * documentacion para humanos; el codigo C usa el estilo
     * directamente. */
    A26_FONT_STYLE_DS_REG_8,   /* statusbar_time */
    A26_FONT_STYLE_DS_BOLD_8,  /* statusbar_title */
    A26_FONT_STYLE_DS_REG_10,  /* menu_item */
    A26_FONT_STYLE_DS_BOLD_10, /* np_counter */
    A26_FONT_STYLE_DS_REG_12,  /* np_album, np_artist, lyrics */
    A26_FONT_STYLE_DS_BOLD_12, /* np_title */
    A26_FONT_STYLE_DS_BOLD_14, /* lyrics_active */
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

#endif /* APPLE2026_SHELL_H */
