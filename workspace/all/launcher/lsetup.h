// First-boot setup (docs/plans/v1-standalone.md, step 5): "Welcome to Bramley" with the Wi-Fi
// networks, then Time Zone, then Home with a card that explains MENU and SELECT. Each step skips
// with B. It runs once on a new Bramley card: the install screen leaves SETUP_PENDING behind on a
// fresh install (card.sh adds that to tg5040.sh), and finishing or skipping removes it.
// Design and motion: spikes/setup-mock (shots/setup.mp4).
#ifndef LSETUP_H
#define LSETUP_H

#include "bramley.h"

#define SETUP_PENDING BRAMLEY_STATE_DIR "/setup-pending"

// Opens setup when SETUP_PENDING exists. Returns 1 when it opened.
int lsetup_begin(void);
int lsetup_is_open(void);
// True while setup covers the whole screen (Wi-Fi and Time Zone); false for the Home card, which
// is drawn over the home screen.
int lsetup_covers(void);
// Handles this frame's input. Returns 1 when the screen changed.
int lsetup_input(void);
// Returns 1 while something moves (animations, Wi-Fi work).
int lsetup_update(void);
// splash_alpha: how much of the boot splash still shows on top (1 to 0); the mark starts where
// the splash leaves it.
void lsetup_draw(float splash_alpha);

#endif
