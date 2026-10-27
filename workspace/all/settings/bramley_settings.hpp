// Bramley: draws NextUI's Settings with the launcher's GL kit (lgfx) as a split view, the
// sections on the left and the focused section's settings on the right. NextUI's menu model
// (menu.hpp) is read, never changed; only the drawing is ours. Design: TDD section 12.4.3.
#pragma once

#include <string>

class MenuList;

// Draws one frame of Settings for the root menu. Returns -1 when this screen should be drawn by
// NextUI's own code instead (custom pages such as the keyboard and the colour picker), 1 when
// it wants another frame soon (a debug script is running), otherwise 0.
int bramley_settings_frame(MenuList *root);

// Called by MenuList::showOverlay. Draws the message over the current page at once when this
// code is drawing Settings, and returns true; returns false to leave it to NextUI's drawing.
bool bramley_settings_overlay(const std::string &message, int mode);
