#include "lcd.h"

#include "aura_shutdown_screen.h"
#include "apple2026_shell.h"
#include "apple2026_tokens.h"
#include "aura_widgets.h"

void aura_shutdown_screen_draw(void)
{
    int size = A26_ICON_SIZE_SHUTDOWN_ICON;
    int x = (A26_SCREEN_WIDTH - size) / 2;
    int y = (A26_SCREEN_HEIGHT - size) / 2;

    /* Negro/blanco crudos a proposito -- el dueno pidio que esta
     * pantalla NO respete el tema (ver aura_shutdown_screen.h). */
    lcd_set_background(LCD_BLACK);
    lcd_set_foreground(LCD_WHITE);
    lcd_clear_display();

    aura_widgets_draw_icon_ink("poweroff", size, x, y, LCD_WHITE, 256);

    lcd_update();
}
