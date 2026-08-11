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
     * relleno de progreso/sliders bajo edicion. NO es el fondo de la
     * seleccion (Principio 2: "el acento se gana"). */
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
    A26_FONT_STYLE_MICRO,
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

#endif /* APPLE2026_SHELL_H */
