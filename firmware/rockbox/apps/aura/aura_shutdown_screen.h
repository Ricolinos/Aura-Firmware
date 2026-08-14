/* Pantalla de apagado (encargo del dueno, 2026-08-14): "al apagar el
 * dispositivo, deberia mostrarse al centro el icono de apagado, en una
 * pantalla negra (icono blanco) independientemente del tema claro u
 * oscuro". Excepcion deliberada al sistema de temas -- por eso no usa
 * a26_color()/a26_shell_clear_screen() como el resto de las pantallas,
 * sino LCD_BLACK/LCD_WHITE crudos. aura_main.c la dibuja UNA vez al
 * interceptar SYS_POWEROFF, antes de dejar que default_event_handler()
 * siga con el apagado real de Rockbox (que ya no dibuja su propio
 * splash de texto -- ver aura_settings_apply_core_defaults(),
 * show_shutdown_message). */
#ifndef AURA_SHUTDOWN_SCREEN_H
#define AURA_SHUTDOWN_SCREEN_H

void aura_shutdown_screen_draw(void);

#endif
