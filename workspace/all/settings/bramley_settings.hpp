// Bramley: draws NextUI's Settings with the launcher's GL kit (lgfx) as a split view, the
// sections on the left and the focused section's settings on the right. NextUI's menu model
// (menu.hpp) is read, never changed; only the drawing is ours. Design: TDD section 12.4.3.
#pragma once

#include <string>

#include "sdl.h"

#include <atomic>
#include <functional>
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

// Wi-Fi and Bluetooth switch on a background thread, because switching takes seconds. These are
// 0 when idle, 1 while turning on and 2 while turning off; the status lines read them.
extern std::atomic<int> bramley_wifi_switching, bramley_bt_switching;
// Runs work on a background thread with flag showing the direction. A press while a switch is
// still running is ignored.
void bramley_switch_radio(std::atomic<int> &flag, bool on, std::function<void()> work);

// The sections list, once Settings has drawn a frame; Left does not leave it.
MenuList *bramley_settings_root();

// True for a level, such as volume or brightness: evenly spaced numbers, shown as a slider.
// Only levels change with left and right.
bool bramley_is_level(const std::vector<std::string> &labels);

// Runs Settings: as its own program with host_screen null, or inside the launcher with the
// launcher's screen surface. Returns when Settings is closed.
int settings_run(SDL_Surface *host_screen);
// Inside the launcher, called once per Settings frame to run the launcher's background work.
void bramley_host_tick();
// Deletes root and every page reachable from it, except those in keep.
void bramley_delete_menu_tree(MenuList *root, const std::vector<MenuList *> &keep);
// For the launcher (C): opens Settings inside the launcher and returns when it closes. tick runs
// once per Settings frame.
extern "C" int bramley_open_settings(SDL_Surface *screen, void (*tick)(void));
