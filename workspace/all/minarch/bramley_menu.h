// Bramley: the in-game menu drawn with the launcher's GL kit (lgfx), in the style of tvOS Now
// Playing: the actions on the left, the save slot's screenshot large on the right, the paused
// game blurred behind. minarch's Menu_loop keeps its logic and calls these to draw. Design:
// TDD section 12, mockup 38.
#ifndef BRAMLEY_MENU_H
#define BRAMLEY_MENU_H

#include "sdl.h"

typedef struct {
	const char* title;     // the game's display name
	const char* rom_path;  // the game's path, for the console's name
	char* const* items;    // Continue, Save, Load, Options, Quit
	int count;
	int selected;
	int total_discs, disc; // disc picker on Continue when total_discs > 1
	int slot, save_exists, preview_exists;
	const char* preview_path; // the slot's screenshot
} BramleyMenuView;

// Starts the menu over the paused game's frame (a full-screen capture). Returns 0 if the kit
// could not start, in which case minarch draws its own menu.
int bramley_menu_begin(SDL_Surface* frame);
// Draws and presents one frame.
void bramley_menu_draw(const BramleyMenuView* v);
void bramley_menu_end(void);
// True while the Bramley menu is drawing (between begin and end).
int bramley_menu_active(void);

// Options, as a split view like Settings: the sections in a sidebar, the open section's settings
// on the right. Menu_options calls push when it opens a list and pop when it returns, and draw
// whenever something changed. list is minarch's MenuList (ma_frontend_opts.h).
struct MenuList;
void bramley_options_push(struct MenuList* list);
void bramley_options_pop(void);
void bramley_options_draw(struct MenuList* list, int selected, int await_input);

#endif
