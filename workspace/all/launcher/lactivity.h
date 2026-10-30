// Activity: time played, per day and per game, like iOS Screen Time. It replaces NextUI's Game
// Tracker app and reads the same log (game_logs.sqlite in .userdata/shared), read only; minarch
// and gametimectl stay its only writers. Drawn inside the launcher. Design: mockup C in
// spikes/apps-mock.
#ifndef LACTIVITY_H
#define LACTIVITY_H

#include "ldata.h"

// Opens Activity at the week view and reads the log.
void lactivity_open(void);
void lactivity_close(void);
int lactivity_is_open(void);

enum { ACT_NONE, ACT_REDRAW, ACT_CLOSE, ACT_PLAY };
// Handles this frame's input. ACT_PLAY fills play with the game to launch.
int lactivity_input(Card* play);
// Loads a box art image if one is waiting. Returns 1 when the screen changed.
int lactivity_update(void);
// Draws Activity over the whole screen, except the status in the top right corner.
void lactivity_draw(void);
// When a game in this ROM folder was last played, as "played today", "played yesterday" or
// "played 3 Oct 2026"; empty if the log has none.
void lactivity_last_played(const char* rom_dir, char* out, size_t n);

#endif
