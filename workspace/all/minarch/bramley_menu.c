// Bramley: the in-game menu drawn with the GL kit. See bramley_menu.h.
#include "bramley_menu.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "defines.h"
#include "api.h"
#include "lgfx.h"

// minarch's screen size, set when it starts (ma_internal.h)
extern int DEVICE_WIDTH, DEVICE_HEIGHT;
#define W DEVICE_WIDTH
#define H DEVICE_HEIGHT

static int ready;      // lgfx started, once per minarch run
static LTex* frame;    // the paused game
static struct {
	LTex* tex;
	char path[256];
	time_t mtime;
} preview;

static void text(int font, const char* s, float x, float y, float alpha, int right) {
	LTex* t = ltex_text(font, s, 0);
	if (!t) return;
	lgfx_draw_tex(t, right ? x - t->w : x, y, t->w, t->h, 0, alpha);
	ltex_free(t);
}

int bramley_menu_begin(SDL_Surface* bitmap) {
	if (!bitmap) return 0;
	lgfx_save_gl_state();
	if (!ready) {
		lgfx_init(W, H);
		ready = 1;
	}
	frame = ltex_from_surface(bitmap, 0);
	if (frame) {
		// the blurred game behind the menu, made once
		lgfx_begin_snapshot();
		lgfx_draw_tex(frame, 0, 0, W, H, 0, 1);
		lgfx_end_snapshot();
	}
	lgfx_restore_gl_state();
	return frame != NULL;
}

void bramley_menu_end(void) {
	lgfx_save_gl_state();
	ltex_free(frame);
	frame = NULL;
	ltex_free(preview.tex);
	preview.tex = NULL;
	preview.path[0] = 0;
	lgfx_restore_gl_state();
}

// The console's name from the game's folder, "Game Boy Advance (GBA)" -> "GAME BOY ADVANCE".
static void console_name(const char* rom_path, char* out, size_t size) {
	out[0] = 0;
	if (!rom_path) return;
	const char* end = strrchr(rom_path, '/');
	if (!end) return;
	const char* start = end - 1;
	while (start > rom_path && *start != '/') start--;
	if (*start == '/') start++;
	size_t n = end - start;
	if (n >= size) n = size - 1;
	memcpy(out, start, n);
	out[n] = 0;
	char* paren = strstr(out, " (");
	if (paren) *paren = 0;
	for (char* c = out; *c; c++)
		if (*c >= 'a' && *c <= 'z') *c -= 32;
}

// The slot's screenshot, loaded again only when the file changes.
static LTex* slot_preview(const BramleyMenuView* v, time_t* when) {
	*when = 0;
	if (!v->preview_exists || !v->preview_path) return NULL;
	struct stat st;
	if (stat(v->preview_path, &st) != 0) return NULL;
	*when = st.st_mtime;
	if (preview.tex && !strcmp(preview.path, v->preview_path) && preview.mtime == st.st_mtime) return preview.tex;
	ltex_free(preview.tex);
	preview.tex = ltex_load(v->preview_path, 0);
	snprintf(preview.path, sizeof(preview.path), "%s", v->preview_path);
	preview.mtime = st.st_mtime;
	return preview.tex;
}

static void saved_when(time_t t, char* out, size_t size) {
	time_t now = time(NULL);
	struct tm a, b;
	localtime_r(&t, &a);
	localtime_r(&now, &b);
	char clock[16];
	strftime(clock, sizeof(clock), "%H:%M", &a);
	if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday)
		snprintf(out, size, "Saved today at %s", clock);
	else {
		char day[32];
		strftime(day, sizeof(day), "%b %e", &a);
		snprintf(out, size, "Saved %s at %s", day, clock);
	}
}

static void dots(float cx, float y, int n, int sel) {
	float gap = 16, x = cx - (n - 1) * gap * 0.5f;
	for (int i = 0; i < n; i++) {
		float s = i == sel ? 9 : 7;
		lgfx_draw_fill(x + i * gap - s * 0.5f, y - s * 0.5f, s, s, s * 0.5f, 1, 1, 1, i == sel ? 0.95f : 0.35f);
	}
}

void bramley_menu_draw(const BramleyMenuView* v) {
	lgfx_save_gl_state();
	lgfx_begin_frame();
	lgfx_draw_snapshot(1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.40f);

	// status and title
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char status[32];
	strftime(status, sizeof(status), "%H:%M", &tm);
	char pct[16];
	snprintf(pct, sizeof(pct), "      %d%%", PWR_getBattery());
	strncat(status, pct, sizeof(status) - strlen(status) - 1);
	text(LFONT_LABEL, status, W - 48, 30, 0.8f, 1);
	char console[64];
	console_name(v->rom_path, console, sizeof(console));
	text(LFONT_LABEL, console, 72, 64, 0.55f, 0);
	LTex* t = ltex_text(LFONT_TITLE, v->title ? v->title : "", W - 72 - 72 - 200);
	if (t) {
		lgfx_draw_tex(t, 72, 90, t->w, t->h, 0, 1);
		ltex_free(t);
	}

	// the actions
	float y = 170;
	for (int i = 0; i < v->count; i++, y += 70) {
		int f = i == v->selected;
		float x = 72, w = 300, h = 58, yy = y;
		if (f) {
			x -= 6, w += 12, yy -= 3, h += 6;
			lgfx_draw_shadow(x, yy, w, h, 6, 0.35f);
			lgfx_draw_fill(x, yy, w, h, 16, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
		} else
			lgfx_draw_fill(x, yy, w, h, 16, 1, 1, 1, 0.08f);
		float ty = yy + (h - 30) * 0.5f;
		text(LFONT_BODY, v->items[i], x + 24, ty, 1, 0);
		if (i == 0 && v->total_discs > 1) {
			char disc[32];
			snprintf(disc, sizeof(disc), f ? "\xE2\x80\xB9  Disc %d  \xE2\x80\xBA" : "Disc %d", v->disc + 1);
			lgfx_set_dim(f ? 0.45f : 1);
			text(LFONT_BODY, disc, x + w - 22, ty, f ? 1 : 0.55f, 1);
		}
		lgfx_set_dim(1);
	}

	// the picture: the slot's screenshot on Save and Load, the paused game otherwise
	int slots = v->selected == 1 || v->selected == 2;
	float cw = 528, ch = 352, cx = 432, cy = 170;
	time_t when = 0;
	LTex* pic = slots ? slot_preview(v, &when) : frame;
	lgfx_draw_shadow(cx, cy, cw, ch, 12, 0.5f);
	if (pic)
		lgfx_draw_art(pic, cx, cy, cw, ch, 18, 1);
	else {
		lgfx_draw_fill(cx, cy, cw, ch, 18, 0.08f, 0.08f, 0.12f, 0.9f);
		LTex* m = ltex_text(LFONT_BODY, v->save_exists ? "No picture for this save" : "Empty slot", 0);
		if (m) {
			lgfx_draw_tex(m, cx + (cw - m->w) * 0.5f, cy + (ch - m->h) * 0.5f, m->w, m->h, 0, 0.5f);
			ltex_free(m);
		}
	}
	char line[64], label[16];
	if (slots) {
		snprintf(label, sizeof(label), "SLOT %d", v->slot + 1);
		if (when) saved_when(when, line, sizeof(line));
		else snprintf(line, sizeof(line), "%s", v->save_exists ? "Saved" : "Nothing saved here yet");
		dots(cx + cw - 70, cy + ch + 62, 8, v->slot);
	} else {
		snprintf(label, sizeof(label), "%s", "PAUSED");
		snprintf(line, sizeof(line), "%s", v->selected == 4 ? "Quit saves your place to resume" : "Your game is paused");
	}
	text(LFONT_LABEL, label, cx, cy + ch + 24, 0.55f, 0);
	text(LFONT_BODY, line, cx, cy + ch + 48, 0.9f, 0);

	// button hints for the focused action
	char hint[128];
	switch (v->selected) {
	case 1: snprintf(hint, sizeof(hint), "A  Save to slot %d        \xE2\x80\xB9 \xE2\x80\xBA  Slot        B  Back to game", v->slot + 1); break;
	case 2: snprintf(hint, sizeof(hint), "A  Load slot %d        \xE2\x80\xB9 \xE2\x80\xBA  Slot        B  Back to game", v->slot + 1); break;
	case 3: snprintf(hint, sizeof(hint), "%s", "A  Open        B  Back to game"); break;
	case 4: snprintf(hint, sizeof(hint), "%s", "A  Quit        B  Back to game"); break;
	default: snprintf(hint, sizeof(hint), "%s", v->total_discs > 1 ? "A  Continue        \xE2\x80\xB9 \xE2\x80\xBA  Disc        B  Back to game" : "A  Continue        B  Back to game");
	}
	text(LFONT_SMALL, hint, 72, H - 44, 0.6f, 0);

	lgfx_end_frame();
	lgfx_restore_gl_state();
}
