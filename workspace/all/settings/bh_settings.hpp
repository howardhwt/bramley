// Brick Hammer: draws NextUI's Settings with the launcher's GL kit (lgfx) as a split view, the
// sections on the left and the focused section's settings on the right. NextUI's menu model
// (menu.hpp) is read, never changed; only the drawing is ours. Design: TDD section 12.4.3.
#pragma once

class MenuList;

// Draws one frame of Settings for the root menu. Returns -1 when this screen should be drawn by
// NextUI's own code instead (custom pages such as the keyboard and the colour picker, and
// overlays), 1 when it wants another frame soon (an animation or a debug script is running),
// otherwise 0.
int bh_settings_frame(MenuList *root);
