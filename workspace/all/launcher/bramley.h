// Bramley: names and paths shared by the launcher and Settings.
#ifndef BRAMLEY_H
#define BRAMLEY_H

#define BRAMLEY_NAME "Bramley"
// Saved state: the music queue, audiobook bookmarks, the shader cache and Bramley's settings.
#define BRAMLEY_STATE_DIR SHARED_USERDATA_PATH "/.bramley"
// Where the state lived before the rename on 2026-10-27; the launcher moves it once.
#define BRAMLEY_OLD_STATE_DIR SHARED_USERDATA_PATH "/.launcher"
// Icons and wallpapers installed with Bramley.
#define BRAMLEY_RES_DIR RES_PATH "/bramley"

#endif
