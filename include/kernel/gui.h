#ifndef BONFIRE_GUI_H
#define BONFIRE_GUI_H

/**
 * Run the optional 1980s-style graphical interface.
 * Switches to Mode 13h, draws the GUI, and returns when the user presses a key.
 * Only available when the kernel is built with ENABLE_GUI=1.
 */
void gui_run(void);

#endif /* BONFIRE_GUI_H */
