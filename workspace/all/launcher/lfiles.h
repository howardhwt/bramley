// Files: Bramley's file browser, in the iPad Files layout. Places on the left (Games, Saves,
// Music, Screenshots, the whole card, Trash), the open folder on the right with colored tiles,
// sizes and dates. Drawn inside the launcher. It replaces NextCommander's card in Apps. Design:
// spikes/files-mock. This first version only reads: it changes no files.
#ifndef LFILES_H
#define LFILES_H

#include "ldata.h"

void lfiles_open(void);
void lfiles_close(void);
int lfiles_is_open(void);

enum { FILES_NONE, FILES_REDRAW, FILES_CLOSE, FILES_PLAY };
// Handles this frame's input. FILES_PLAY fills play with the game to launch.
int lfiles_input(Card* play);
// Loads an image preview if one is waiting. Returns 1 when the screen changed.
int lfiles_update(void);
// Draws Files over the whole screen, except the status in the top right corner.
void lfiles_draw(void);

#endif
