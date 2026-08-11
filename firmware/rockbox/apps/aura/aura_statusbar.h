/* Barra de estado propia de Aura (Fase 13, PLAN-UX.md, L5): reemplaza a
 * la barra de estado clasica de Rockbox (que Fase 12/D-051 ya
 * desactivo por completo via STATUSBAR_OFF). Bateria siempre visible a
 * la derecha; icono de reproduccion (o el candado de Hold, que le gana
 * el lugar) a su izquierda; titulo alineado a la izquierda en menus o
 * centrado en pantallas con interfaz propia. */
#ifndef AURA_STATUSBAR_H
#define AURA_STATUSBAR_H

/* Dibuja la franja [x, x+width) x [0, A26_LAYOUT_STATUSBAR_HEIGHT).
 * `title` puede ser NULL (sin texto, solo bateria/reproduccion/hold).
 * `centered` = 0 alinea a la izquierda (menus), 1 centra (pantallas
 * con interfaz propia: Ahora suena, reloj, etc.). No llama a
 * lcd_update(). */
void aura_statusbar_draw(int x, int width, const char *title, int centered);

#endif /* AURA_STATUSBAR_H */
