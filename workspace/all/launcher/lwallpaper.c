// Wallpapers for the launcher and Settings. See lwallpaper.h.
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "bramley.h"
#include "lwallpaper.h"

#define CHOICE_PATH BRAMLEY_STATE_DIR "/wallpaper.txt"
#define FOLDER SDCARD_PATH "/Wallpapers"

static const char* ours[][2] = {
	{ "bloom", "Bloom" }, { "dusk-hills", "Dusk Hills" }, { "dunes", "Dunes" }, { "aurora", "Aurora" },
	{ "graphite", "Graphite" }, { "ember", "Ember" }, { "tide", "Tide" }, { "lagoon", "Lagoon" },
};
#define OURS (int)(sizeof(ours) / sizeof(ours[0]))

static int picture(const char* file) {
	const char* dot = strrchr(file, '.');
	return file[0] != '.' && dot && (!strcasecmp(dot, ".png") || !strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg"));
}

int wallpaper_list(Wallpaper* out, int max) {
	int n = 0;
	for (int i = 0; i < OURS && n < max; i++) {
		Wallpaper* w = &out[n];
		snprintf(w->path, sizeof(w->path), "%s/wallpapers/%s.jpg", BRAMLEY_RES_DIR, ours[i][0]);
		if (!exists(w->path)) continue;
		snprintf(w->id, sizeof(w->id), "bramley:%s", ours[i][0]);
		snprintf(w->name, sizeof(w->name), "%s", ours[i][1]);
		snprintf(w->blur, sizeof(w->blur), "%s/wallpapers/%s-blur.jpg", BRAMLEY_RES_DIR, ours[i][0]);
		if (!exists(w->blur)) w->blur[0] = 0;
		n++;
	}
	if (n < max) {
		Wallpaper* w = &out[n];
		snprintf(w->path, sizeof(w->path), "%s/bg.png", SDCARD_PATH);
		if (exists(w->path)) {
			snprintf(w->id, sizeof(w->id), "card:bg.png");
			snprintf(w->name, sizeof(w->name), "Your bg.png");
			w->blur[0] = 0;
			n++;
		}
	}
	DIR* dir = opendir(FOLDER);
	if (dir) {
		struct dirent* e;
		while ((e = readdir(dir)) && n < max) {
			if (!picture(e->d_name) || strlen(e->d_name) > 120) continue;
			Wallpaper* w = &out[n++];
			snprintf(w->id, sizeof(w->id), "folder:%s", e->d_name);
			snprintf(w->name, sizeof(w->name), "%.*s", (int)(strrchr(e->d_name, '.') - e->d_name), e->d_name);
			snprintf(w->path, sizeof(w->path), "%s/%s", FOLDER, e->d_name);
			w->blur[0] = 0;
		}
		closedir(dir);
	}
	return n;
}

void wallpaper_current(Wallpaper* out) {
	char id[160] = "";
	FILE* f = fopen(CHOICE_PATH, "r");
	if (f) {
		if (fgets(id, sizeof(id), f)) id[strcspn(id, "\r\n")] = 0;
		fclose(f);
	}
	Wallpaper all[64];
	int n = wallpaper_list(all, 64);
	int pick = -1, bg = -1;
	for (int i = 0; i < n; i++) {
		if (id[0] && !strcmp(all[i].id, id)) pick = i;
		if (!strcmp(all[i].id, "card:bg.png")) bg = i;
	}
	if (pick < 0) pick = bg >= 0 ? bg : 0;
	if (n > 0) *out = all[pick];
	else memset(out, 0, sizeof(*out)); // nothing installed: the caller draws its own fallback
}

void wallpaper_choose(const char* id) {
	mkdir(BRAMLEY_STATE_DIR, 0755);
	FILE* f = fopen(CHOICE_PATH, "w");
	if (!f) return;
	fprintf(f, "%s\n", id);
	fclose(f);
}
