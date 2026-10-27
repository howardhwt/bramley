// Bramley: draws NextUI's Settings with the launcher's GL kit (lgfx) as a split view, the
// sections on the left and the focused section's settings on the right. NextUI's menu model
// (menu.hpp) is read, never changed; only the drawing is ours. Design: TDD section 12.4.3.
#pragma once

#include <string>

#include "sdl.h"

#include <vector>

class MenuList;
class AbstractMenuItem;

// Draws and presents one frame of Settings for the root menu. Pages that NextUI draws itself (the
// keyboard, the colour picker) are drawn by NextUI's code into screen, with the list and title
// rects NextUI lays out, and shown from there. Returns 1 when it wants another frame soon (a
// debug script is running), 0 otherwise, and -1 only without a menu or screen.
int bramley_settings_frame(MenuList *root, SDL_Surface *screen, const SDL_Rect &list, const SDL_Rect &title);

// Called by MenuList::showOverlay. Draws the message over the current page at once when this
// code is drawing Settings, and returns true; returns false to leave it to NextUI's drawing.
bool bramley_settings_overlay(const std::string &message, int mode);

// Puts the Settings sections in Bramley's order. Called once from settings.cpp on the list of
// sections before the main menu is built; sections it does not know keep their place after ours.
void bramley_order_sections(std::vector<AbstractMenuItem *> &items);

// Adds Bramley's rows to Appearance (Wallpaper). Called once from settings.cpp.
void bramley_add_appearance_items(std::vector<AbstractMenuItem *> &items);
