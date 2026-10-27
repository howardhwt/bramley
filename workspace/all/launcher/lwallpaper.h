// Wallpapers: Bramley's own (installed in .system/res/bramley/wallpapers), the card's bg.png, and
// pictures the user puts in a Wallpapers folder at the card root. The choice is saved in
// Bramley's state folder; bg.png itself is never written. Shared by the launcher and Settings.
#ifndef LWALLPAPER_H
#define LWALLPAPER_H

typedef struct {
	char id[160];    // what is saved: "bramley:bloom", "card:bg.png" or "folder:<file name>"
	char name[64];   // shown in Settings
	char path[512];  // the full picture
	char blur[512];  // a small pre-blurred copy, or empty when the picture must be blurred here
} Wallpaper;

#ifdef __cplusplus
extern "C" {
#endif

// Fills out with every wallpaper available, ours first, and returns how many.
int wallpaper_list(Wallpaper* out, int max);
// The chosen wallpaper. With no choice saved, or a choice no longer on the card, it is bg.png
// if the card has one (as in stock NextUI), else Bloom.
void wallpaper_current(Wallpaper* out);
void wallpaper_choose(const char* id);

#ifdef __cplusplus
}
#endif

#endif
