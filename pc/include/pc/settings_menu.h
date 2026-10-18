#pragma once

// In-game settings menu (pc/src/ui/settings_menu.cpp).

// Once Aurora is initialized (font, style).
void pc_settings_menu_init();

// While it is open, the game gets no controller or mouse input.
bool pc_settings_menu_open();

void pc_settings_menu_toggle();

// The player chose "Quitter le jeu" (the platform exits after the frame).
bool pc_settings_menu_quit_requested();

// Between aurora_begin_frame and aurora_end_frame.
void pc_settings_menu_draw();
