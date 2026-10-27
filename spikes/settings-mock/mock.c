// Mockup of NextUI's System settings page in two styles, for choosing how to reskin Settings.
// Style 1 draws with the launcher's GL kit (lgfx); style 2 imitates a tidied version of NextUI's
// current flat drawing. The rows are NextUI's real System settings with example values.
//   DYLD_LIBRARY_PATH=/var/tmp/nextui/lib SDL_RENDER_VSYNC=0 ./build/mock out-prefix [icon-dir]
#include <msettings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "lgfx.h"

#define W 1024
#define H 768

static const char* rows[][2] = {
	{ "Volume", "70%" },           { "Screen timeout", "1 min" },
	{ "Suspend timeout", "30 s" }, { "Haptic feedback", "On" },
	{ "Default view", "Home" },    { "Show 24h time format", "Off" },
	{ "Show clock", "On" },        { "Set time and date automatically", "On" },
	{ "Time zone", "Hong Kong" },  { "Safe poweroff", "On" },
};
#define ROWS (int)(sizeof(rows) / sizeof(rows[0]))
#define FOCUS 2

static void save(const char* path) {
	int w, h;
	SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &w, &h);
	SDL_Surface* img = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
	Uint8* rgba = malloc(w * h * 4);
	glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	for (int y = 0; y < h; y++) memcpy((Uint8*)img->pixels + y * img->pitch, rgba + (h - 1 - y) * w * 4, w * 4);
	SDL_SaveBMP(img, path);
	SDL_FreeSurface(img);
	free(rgba);
}

static void text(int font, const char* s, float x, float y, float alpha, int right) {
	LTex* t = ltex_text(font, s, 0);
	if (!t) return;
	lgfx_draw_tex(t, right ? x - t->w : x, y, t->w, t->h, 0, alpha);
	ltex_free(t);
}

// Style 1: tvOS Settings. A large card for the section on the left, rounded rows on the right,
// the focused row lifted on a white card with dark text, all over the blurred wallpaper.
static int tvos_controls; // draw switches and a slider instead of On/Off and 70%
static const char* icon_dir; // pre-rendered Settings icons (argv[2]); none draws the old glyph

static LTex* icon(const char* name, int size) {
	char path[512];
	snprintf(path, sizeof(path), "%s/%s-%d.png", icon_dir, name, size);
	return ltex_load(path, 0);
}
static int is_toggle(const char* v);
static void draw_switch(float x, float y, int on, int dark);

static void style_tvos(void) {
	lgfx_begin_frame();
	const float top[4] = { 0.20f, 0.17f, 0.36f, 1 }, bottom[4] = { 0.10f, 0.12f, 0.22f, 1 };
	lgfx_draw_gradient(0, 0, W, H, 0, top, bottom);

	text(LFONT_LABEL, "Settings", 56, 30, 0.55f, 0);
	text(LFONT_LABEL, "12:47      100%", W - 56, 30, 0.8f, 1);

	// Section card
	float cx = 56, cy = 150, cs = 300;
	lgfx_draw_shadow(cx, cy, cs, cs, 14, 0.6f);
	LTex* ic = icon_dir ? icon("system", 300) : NULL;
	if (ic) {
		lgfx_draw_tex(ic, cx, cy, cs, cs, 0, 1);
		ltex_free(ic);
	} else {
		const float ct[4] = { 0.55f, 0.58f, 0.66f, 1 }, cb[4] = { 0.30f, 0.32f, 0.40f, 1 };
		lgfx_draw_gradient(cx, cy, cs, cs, 40, ct, cb);
		// A simple drawn glyph: three slider lines, since no Apple symbols ship.
		for (int i = 0; i < 3; i++) {
			float ly = cy + 100 + i * 50;
			lgfx_draw_fill(cx + 70, ly, 160, 8, 4, 1, 1, 1, 0.85f);
			lgfx_draw_fill(cx + 90 + i * 45, ly - 12, 32, 32, 16, 1, 1, 1, 1);
		}
	}
	text(LFONT_HERO, "System", cx, cy + cs + 24, 1, 0);
	text(LFONT_BODY, "Sound, sleep, clock and saves", cx, cy + cs + 84, 0.6f, 0);

	// Rows
	float rx = 400, rw = W - rx - 56, rh = 54, gap = 8, ry = 110;
	for (int i = 0; i < ROWS; i++) {
		float y = ry + i * (rh + gap), x = rx, w = rw, h = rh;
		int focused = i == FOCUS;
		if (focused) {
			x -= 8;
			w += 16;
			y -= 3;
			h += 6;
			lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
			lgfx_draw_fill(x, y, w, h, 16, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
			text(LFONT_BODY, rows[i][0], x + 26, y + 14, 1, 0);
			lgfx_set_dim(1);
		} else {
			lgfx_draw_fill(x, y, w, h, 14, 1, 1, 1, 0.07f);
			text(LFONT_BODY, rows[i][0], x + 26, y + 13, 0.92f, 0);
		}
		const char* v = rows[i][1];
		float vx = x + w - 26;
		if (tvos_controls && is_toggle(v)) {
			draw_switch(vx, y + (h - 34) * 0.5f, !strcmp(v, "On"), focused);
		} else if (tvos_controls && !strcmp(rows[i][0], "Volume")) {
			float sw = 220, sx = vx - sw, sy = y + h * 0.5f - 3;
			lgfx_draw_fill(sx, sy, sw, 6, 3, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, 0.2f);
			lgfx_draw_fill(sx, sy, sw * 0.7f, 6, 3, focused ? 0.1f : 1, focused ? 0.1f : 1, focused ? 0.1f : 1, 0.95f);
			lgfx_draw_fill(sx + sw * 0.7f - 12, sy - 9, 24, 24, 12, 1, 1, 1, 1);
		} else {
			lgfx_set_dim(focused ? 0.45f : 1);
			text(LFONT_BODY, v, vx, y + (focused ? 14 : 13), focused ? 1 : 0.5f, 1);
			lgfx_set_dim(1);
		}
	}
	text(LFONT_SMALL, "A  Change     < >  Adjust     B  Back", rx, H - 40, 0.55f, 0);
}


// Variations on style 1. variant: 'A' true tvOS (plain rows, title centred at the top, chevrons),
// 'B' full-width list with a large title and no section card, 'C' like A with switches and a
// slider instead of text values.
static int is_toggle(const char* v) {
	return !strcmp(v, "On") || !strcmp(v, "Off");
}

static void draw_switch(float x, float y, int on, int dark) {
	float w = 58, h = 34;
	if (on) lgfx_draw_fill(x - w, y, w, h, h * 0.5f, 0.20f, 0.78f, 0.35f, 1);
	else lgfx_draw_fill(x - w, y, w, h, h * 0.5f, dark ? 0 : 1, dark ? 0 : 1, dark ? 0 : 1, dark ? 0.15f : 0.22f);
	float k = h - 6;
	lgfx_draw_fill(on ? x - k - 3 : x - w + 3, y + 3, k, k, k * 0.5f, 1, 1, 1, 1);
}

static void style_variant(char variant) {
	lgfx_begin_frame();
	const float top[4] = { 0.20f, 0.17f, 0.36f, 1 }, bottom[4] = { 0.10f, 0.12f, 0.22f, 1 };
	lgfx_draw_gradient(0, 0, W, H, 0, top, bottom);
	text(LFONT_LABEL, "12:47      100%", W - 56, 30, 0.8f, 1);

	float rx, rw, rh = 54, gap = 6, ry;
	if (variant == 'B') {
		text(LFONT_LABEL, "Settings", 56, 30, 0.55f, 0);
		text(LFONT_HERO, "System", 56, 66, 1, 0);
		rx = 56;
		rw = W - 112;
		ry = 140;
	} else {
		LTex* t = ltex_text(LFONT_TITLE, "System", 0);
		if (t) {
			lgfx_draw_tex(t, (W - t->w) * 0.5f, 34, t->w, t->h, 0, 1);
			ltex_free(t);
		}
		float cx = 70, cs = 270, cy = (H - cs) * 0.5f;
		lgfx_draw_shadow(cx, cy, cs, cs, 14, 0.55f);
		const float ct[4] = { 0.55f, 0.58f, 0.66f, 1 }, cb[4] = { 0.30f, 0.32f, 0.40f, 1 };
		lgfx_draw_gradient(cx, cy, cs, cs, 36, ct, cb);
		for (int i = 0; i < 3; i++) {
			float ly = cy + 90 + i * 45;
			lgfx_draw_fill(cx + 63, ly, 144, 7, 3.5f, 1, 1, 1, 0.85f);
			lgfx_draw_fill(cx + 81 + i * 40, ly - 11, 29, 29, 14.5f, 1, 1, 1, 1);
		}
		rx = 400;
		rw = W - rx - 56;
		ry = 104;
	}

	for (int i = 0; i < ROWS; i++) {
		float y = ry + i * (rh + gap), x = rx, w = rw, h = rh;
		int focused = i == FOCUS;
		int chevron = !strcmp(rows[i][0], "Time zone");
		const char* v = rows[i][1];
		if (focused) {
			x -= 8, w += 16, y -= 3, h += 6;
			lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
			lgfx_draw_fill(x, y, w, h, 16, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
		}
		float ty = y + (h - 30) * 0.5f;
		text(LFONT_BODY, rows[i][0], x + 26, ty, focused ? 1 : 0.92f, 0);
		float vx = x + w - 26;
		if (chevron) {
			lgfx_set_dim(focused ? 0.35f : 1);
			text(LFONT_BODY, "\xE2\x80\xBA", vx, ty, focused ? 1 : 0.45f, 1);
			vx -= 26;
		}
		if (variant == 'C' && is_toggle(v)) {
			lgfx_set_dim(1);
			draw_switch(vx, y + (h - 34) * 0.5f, !strcmp(v, "On"), focused);
		} else if (variant == 'C' && !strcmp(rows[i][0], "Volume")) {
			lgfx_set_dim(1);
			float sw = 220, sx = vx - sw, sy = y + h * 0.5f - 3;
			lgfx_draw_fill(sx, sy, sw, 6, 3, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, 0.2f);
			lgfx_draw_fill(sx, sy, sw * 0.7f, 6, 3, focused ? 0.1f : 1, focused ? 0.1f : 1, focused ? 0.1f : 1, 0.95f);
			lgfx_draw_fill(sx + sw * 0.7f - 12, sy - 9, 24, 24, 12, 1, 1, 1, 1);
		} else {
			lgfx_set_dim(focused ? 0.45f : 1);
			text(LFONT_BODY, v, vx, ty, focused ? 1 : 0.5f, 1);
		}
		lgfx_set_dim(1);
	}
	text(LFONT_SMALL, "A  Change     < >  Adjust     B  Back", rx, H - 40, 0.55f, 0);
}

// The top level of Settings: the focused section's icon large on the left, the sections as rows
// with small icons on the right.
static const char* sections[][2] = {
	{ "Appearance", "appearance" }, { "Display", "display" },     { "System", "system" },
	{ "Network", "network" },       { "Bluetooth", "bluetooth" }, { "In-Game", "in-game" },
	{ "FN Switch", "fn-switch" },   { "Buttons", "buttons" },     { "Notifications", "notifications" },
	{ "Achievements", "achievements" }, { "About", "about" },
};
#define SECTIONS (int)(sizeof(sections) / sizeof(sections[0]))

static void style_sections(int focus) {
	lgfx_begin_frame();
	const float top[4] = { 0.20f, 0.17f, 0.36f, 1 }, bottom[4] = { 0.10f, 0.12f, 0.22f, 1 };
	lgfx_draw_gradient(0, 0, W, H, 0, top, bottom);
	text(LFONT_LABEL, "Settings", 56, 30, 0.55f, 0);
	text(LFONT_LABEL, "12:47      100%", W - 56, 30, 0.8f, 1);

	float cx = 56, cy = 150, cs = 300;
	lgfx_draw_shadow(cx, cy, cs, cs, 14, 0.6f);
	LTex* big = icon(sections[focus][1], 300);
	if (big) {
		lgfx_draw_tex(big, cx, cy, cs, cs, 0, 1);
		ltex_free(big);
	}
	text(LFONT_HERO, sections[focus][0], cx, cy + cs + 24, 1, 0);
	text(LFONT_BODY, "Wi-Fi and time sync", cx, cy + cs + 84, 0.6f, 0);

	float rx = 400, rw = W - rx - 56, rh = 50, gap = 6, ry = 84;
	for (int i = 0; i < SECTIONS; i++) {
		float y = ry + i * (rh + gap), x = rx, w = rw, h = rh;
		int focused = i == focus;
		if (focused) {
			x -= 8, w += 16, y -= 3, h += 6;
			lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
			lgfx_draw_fill(x, y, w, h, 16, 1, 1, 1, 0.96f);
		} else {
			lgfx_draw_fill(x, y, w, h, 14, 1, 1, 1, 0.07f);
		}
		LTex* ic = icon(sections[i][1], 36);
		if (ic) {
			lgfx_draw_tex(ic, x + 14, y + (h - 36) * 0.5f, 36, 36, 0, 1);
			ltex_free(ic);
		}
		float ty = y + (h - 30) * 0.5f;
		if (focused) lgfx_set_dim(0.1f);
		text(LFONT_BODY, sections[i][0], x + 64, ty, focused ? 1 : 0.92f, 0);
		lgfx_set_dim(focused ? 0.35f : 1);
		text(LFONT_BODY, "\xE2\x80\xBA", x + w - 26, ty, focused ? 1 : 0.45f, 1);
		lgfx_set_dim(1);
	}
	text(LFONT_SMALL, "A  Open     B  Back", rx, H - 40, 0.55f, 0);
}

// Alternatives to the top-level page. Each section's current value, shown on its row.
static const char* sec_value[] = { "Dark", "60%", "", "Home Wi-Fi", "Off", "", "Sleep", "", "On", "12 of 40", "v6.4" };

static void top_bar(const char* title) {
	lgfx_begin_frame();
	const float top[4] = { 0.20f, 0.17f, 0.36f, 1 }, bottom[4] = { 0.10f, 0.12f, 0.22f, 1 };
	lgfx_draw_gradient(0, 0, W, H, 0, top, bottom);
	if (title) text(LFONT_LABEL, title, 56, 30, 0.55f, 0);
	text(LFONT_LABEL, "12:47      100%", W - 56, 30, 0.8f, 1);
}

static void section_row(int i, float x, float y, float w, float h, int focused, int card, int value) {
	if (focused) {
		x -= 8, w += 16, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 14, 1, 1, 1, 0.96f);
	} else if (card) {
		lgfx_draw_fill(x, y, w, h, 14, 1, 1, 1, 0.07f);
	}
	LTex* ic = icon(sections[i][1], 36);
	if (ic) {
		lgfx_draw_tex(ic, x + 14, y + (h - 36) * 0.5f, 36, 36, 0, 1);
		ltex_free(ic);
	}
	float ty = y + (h - 30) * 0.5f;
	if (focused) lgfx_set_dim(0.1f);
	text(LFONT_BODY, sections[i][0], x + 64, ty, focused ? 1 : 0.92f, 0);
	lgfx_set_dim(focused ? 0.35f : 1);
	text(LFONT_BODY, "\xE2\x80\xBA", x + w - 22, ty, focused ? 1 : 0.4f, 1);
	if (value && sec_value[i][0]) {
		lgfx_set_dim(focused ? 0.45f : 1);
		text(LFONT_BODY, sec_value[i], x + w - 46, ty, focused ? 1 : 0.5f, 1);
	}
	lgfx_set_dim(1);
}

// A: iOS grouped list. One centred column, related sections grouped on shared cards, each row
// showing its current value, no large icon.
static void alt_grouped(int focus) {
	top_bar(NULL);
	LTex* t = ltex_text(LFONT_TITLE, "Settings", 0);
	if (t) {
		lgfx_draw_tex(t, (W - t->w) * 0.5f, 34, t->w, t->h, 0, 1);
		ltex_free(t);
	}
	static const int order[] = { 3, 4, -1, 1, 0, -1, 5, 7, 6, -1, 8, 9, -1, 2, 10 };
	float w = 640, x = (W - w) * 0.5f, y = 100, rh = 46;
	int n = sizeof(order) / sizeof(order[0]), start = 0;
	for (int k = 0; k <= n; k++) {
		if (k == n || order[k] < 0) {
			// one rounded card behind the group, rows divided by hairlines
			float gh = (k - start) * rh;
			lgfx_draw_fill(x, y, w, gh, 14, 1, 1, 1, 0.07f);
			for (int j = start; j < k; j++) {
				float ry = y + (j - start) * rh;
				if (j > start) lgfx_draw_fill(x + 64, ry, w - 64, 1, 0, 1, 1, 1, 0.08f);
				section_row(order[j], x, ry, w, rh, order[j] == focus, 0, 1);
			}
			y += gh + 14;
			start = k + 1;
		}
	}
}

// C: a grid of large icons, like the Apps row on the home screen.
static void alt_grid(int focus) {
	top_bar(NULL);
	LTex* t = ltex_text(LFONT_TITLE, "Settings", 0);
	if (t) {
		lgfx_draw_tex(t, (W - t->w) * 0.5f, 34, t->w, t->h, 0, 1);
		ltex_free(t);
	}
	float s = 120, px = 210, py = 190, x0 = (W - 3 * px - s) * 0.5f, y0 = 130;
	for (int i = 0; i < SECTIONS; i++) {
		int focused = i == focus;
		float x = x0 + (i % 4) * px, y = y0 + (i / 4) * py, z = focused ? 1.12f : 1;
		float zs = s * z, zx = x - (zs - s) * 0.5f, zy = y - (zs - s) * 0.5f;
		lgfx_draw_shadow(zx, zy, zs, zs, focused ? 14 : 5, focused ? 0.55f : 0.3f);
		LTex* ic = icon(sections[i][1], 132);
		if (ic) {
			lgfx_draw_tex(ic, zx, zy, zs, zs, 0, 1);
			ltex_free(ic);
		}
		LTex* l = ltex_text(LFONT_BODY, sections[i][0], 0);
		if (l) {
			lgfx_draw_tex(l, x + (s - l->w) * 0.5f, y + s + (focused ? 18 : 10), l->w, l->h, 0, focused ? 1 : 0.7f);
			ltex_free(l);
		}
	}
	text(LFONT_SMALL, "Wi-Fi and time sync.  Connected to Home Wi-Fi.", 56, H - 44, 0.6f, 0);
}

// D: split view, as on iPad. The sections on the left, the focused section's own settings live
// on the right, so the second page is only a move to the right.
static void alt_split(int focus) {
	top_bar("Settings");
	float lx = 40, lw = 330, rh = 50, gap = 6, ly = 84;
	for (int i = 0; i < SECTIONS; i++)
		section_row(i, lx, ly + i * (rh + gap), lw, rh, i == focus, 1, 0);
	float rx = 410, rw = W - rx - 40, y = 84;
	lgfx_draw_fill(rx, y, rw, H - y - 70, 22, 0, 0, 0, 0.18f);
	if (focus == 3 || focus == 4) {
		// Network and Bluetooth build their lists from a scan, so the pane shows a summary and
		// the scan starts only when the section is opened.
		int wifi = focus == 3;
		float is = 150, ix = rx + (rw - is) * 0.5f, iy = y + 110;
		lgfx_draw_shadow(ix, iy, is, is, 12, 0.5f);
		LTex* ic = icon(sections[focus][1], 300);
		if (ic) {
			lgfx_draw_tex(ic, ix, iy, is, is, 0, 1);
			ltex_free(ic);
		}
		const char* lines[3] = { sections[focus][0], wifi ? "Connected to Home Wi-Fi" : "Bluetooth is off",
								 wifi ? "Strong signal.  Clock set from the network." : "Turn it on to connect headphones or a controller." };
		const int fonts[3] = { LFONT_TITLE, LFONT_BODY, LFONT_SMALL };
		const float alphas[3] = { 1, 0.8f, 0.5f };
		float ty = iy + is + 28;
		for (int i = 0; i < 3; i++) {
			LTex* t = ltex_text(fonts[i], lines[i], (int)rw - 60);
			if (!t) continue;
			lgfx_draw_tex(t, rx + (rw - t->w) * 0.5f, ty, t->w, t->h, 0, alphas[i]);
			ty += t->h + (i ? 8 : 10);
			ltex_free(t);
		}
		// the one action, as a button
		const char* b = wifi ? "Choose a network" : "Turn on Bluetooth";
		LTex* bt = ltex_text(LFONT_BODY, b, 0);
		if (bt) {
			float bw = bt->w + 64, bh = 52, bx = rx + (rw - bw) * 0.5f, by = ty + 26;
			lgfx_draw_fill(bx, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.14f);
			lgfx_draw_tex(bt, bx + 32, by + (bh - bt->h) * 0.5f, bt->w, bt->h, 0, 0.95f);
			ltex_free(bt);
		}
		text(LFONT_SMALL, wifi ? "A  Open Network     B  Back" : "A  Open Bluetooth     B  Back", lx, H - 40, 0.55f, 0);
		return;
	}
	text(LFONT_TITLE, sections[focus][0], rx + 28, y + 20, 1, 0);
	float r2h = 48;
	for (int i = 0; i < 9; i++) {
		float yy = y + 90 + i * (r2h + 6);
		lgfx_draw_fill(rx + 20, yy, rw - 40, r2h, 12, 1, 1, 1, 0.06f);
		float ty = yy + (r2h - 30) * 0.5f;
		text(LFONT_BODY, rows[i][0], rx + 40, ty, 0.85f, 0);
		const char* v = rows[i][1];
		if (is_toggle(v)) draw_switch(rx + rw - 40, yy + (r2h - 34) * 0.5f, !strcmp(v, "On"), 0);
		else text(LFONT_BODY, v, rx + rw - 40, ty, 0.5f, 1);
	}
	text(LFONT_SMALL, "A  Open     >  Into System     B  Back", lx, H - 40, 0.55f, 0);
}

// Q6: a page one level down (In-Game, then Notifications) and three ways to show where you are.
static int nested_style; // 0 off, 1 back label above the title, 2 path as the title, 3 title only
static const char* notif_rows[][2] = {
	{ "Save states", "On" }, { "Load states", "On" }, { "Screenshots", "On" },
	{ "Vol / Display Adjustments", "On" }, { "Duration", "2 s" }, { "Reset to defaults", "" },
};

static void draw_nested(float rx, float py, float rw, int pane_focus) {
	float ty = py + 20;
	if (nested_style == 1) {
		text(LFONT_BODY, "\xE2\x80\xB9  In-Game", rx + 28, py + 18, 0.5f, 0);
		text(LFONT_TITLE, "Notifications", rx + 28, py + 52, 1, 0);
		ty = py + 52;
	} else if (nested_style == 2) {
		LTex* a = ltex_text(LFONT_TITLE, "In-Game  \xE2\x80\xBA  ", 0);
		float x = rx + 28;
		if (a) {
			lgfx_draw_tex(a, x, py + 20, a->w, a->h, 0, 0.45f);
			x += a->w;
			ltex_free(a);
		}
		text(LFONT_TITLE, "Notifications", x, py + 20, 1, 0);
	} else {
		text(LFONT_TITLE, "Notifications", rx + 28, py + 20, 1, 0);
	}
	float r2h = 48, y0 = ty + 70;
	for (int i = 0; i < 6; i++) {
		float yy = y0 + i * (r2h + 6), x = rx + 20, w = rw - 40, h = r2h;
		int f = i == pane_focus;
		if (f) {
			x -= 6, w += 12, yy -= 3, h += 6;
			lgfx_draw_shadow(x, yy, w, h, 6, 0.32f);
			lgfx_draw_fill(x, yy, w, h, 12, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
		} else {
			lgfx_draw_fill(x, yy, w, h, 12, 1, 1, 1, 0.06f);
		}
		float tty = yy + (h - 30) * 0.5f;
		text(LFONT_BODY, notif_rows[i][0], x + 20, tty, f ? 1 : 0.9f, 0);
		const char* v = notif_rows[i][1];
		if (is_toggle(v)) {
			lgfx_set_dim(1);
			draw_switch(x + w - 20, yy + (h - 34) * 0.5f, !strcmp(v, "On"), f);
		} else if (v[0]) {
			lgfx_set_dim(f ? 0.45f : 1);
			text(LFONT_BODY, v, x + w - 20, tty, f ? 1 : 0.5f, 1);
		}
		lgfx_set_dim(1);
	}
}

// The split view with NextUI's real top level on the Brick (settings.cpp:1140). order lists
// section indexes, -1 is a gap between groups. pane_focus is -1 while the sidebar has focus,
// else the focused row inside the pane.
static void split_real(const int* order, int n, int focus, int pane_focus, const char* hint, int rename) {
	top_bar("Settings");
	float lx = 40, lw = 330, rh = 52, gap = 6, y = 84;
	int inpane = pane_focus >= 0;
	for (int k = 0; k < n; k++) {
		int i = order[k];
		if (i < 0) {
			y += 14;
			continue;
		}
		const char* name = (i == 7 && !rename) ? "Assignments" : sections[i][0];
		float x = lx, w = lw, h = rh, yy = y;
		int focused = i == focus;
		if (focused && !inpane) {
			x -= 8, w += 16, yy -= 3, h += 6;
			lgfx_draw_shadow(x, yy, w, h, 6, 0.32f);
			lgfx_draw_fill(x, yy, w, h, 14, 1, 1, 1, 0.96f);
		} else if (focused) {
			lgfx_draw_fill(x, yy, w, h, 14, 1, 1, 1, 0.22f);
		} else {
			lgfx_draw_fill(x, yy, w, h, 14, 1, 1, 1, 0.07f);
		}
		LTex* ic = icon(sections[i][1], 36);
		if (ic) {
			lgfx_draw_tex(ic, x + 14, yy + (h - 36) * 0.5f, 36, 36, 0, inpane && !focused ? 0.5f : 1);
			ltex_free(ic);
		}
		float ty = yy + (h - 30) * 0.5f;
		if (focused && !inpane) lgfx_set_dim(0.1f);
		text(LFONT_BODY, name, x + 64, ty, inpane ? (focused ? 0.95f : 0.45f) : (focused ? 1 : 0.92f), 0);
		lgfx_set_dim(1);
		y += rh + gap;
	}
	float rx = 410, rw = W - rx - 40, py = 84;
	lgfx_draw_fill(rx, py, rw, H - py - 70, 22, 0, 0, 0, 0.18f);
	if (nested_style) {
		draw_nested(rx, py, rw, pane_focus);
	} else if (focus == 3) {
		float is = 150, ix = rx + (rw - is) * 0.5f, iy = py + 110;
		lgfx_draw_shadow(ix, iy, is, is, 12, 0.5f);
		LTex* ic = icon("network", 300);
		if (ic) {
			lgfx_draw_tex(ic, ix, iy, is, is, 0, 1);
			ltex_free(ic);
		}
		const char* lines[2] = { "Network", "Connected to Home Wi-Fi" };
		float ty = iy + is + 28;
		for (int i = 0; i < 2; i++) {
			LTex* t = ltex_text(i ? LFONT_BODY : LFONT_TITLE, lines[i], 0);
			if (!t) continue;
			lgfx_draw_tex(t, rx + (rw - t->w) * 0.5f, ty, t->w, t->h, 0, i ? 0.8f : 1);
			ty += t->h + 10;
			ltex_free(t);
		}
		LTex* bt = ltex_text(LFONT_BODY, "A   Choose a network", 0);
		if (bt) {
			float bw = bt->w + 64, bh = 52, bx = rx + (rw - bw) * 0.5f, by = ty + 26;
			lgfx_draw_fill(bx, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.14f);
			lgfx_draw_tex(bt, bx + 32, by + (bh - bt->h) * 0.5f, bt->w, bt->h, 0, 0.95f);
			ltex_free(bt);
		}
	} else {
		text(LFONT_TITLE, sections[focus][0], rx + 28, py + 20, 1, 0);
		float r2h = 48;
		for (int i = 0; i < 9; i++) {
			float yy = py + 90 + i * (r2h + 6), x = rx + 20, w = rw - 40, h = r2h;
			int f = i == pane_focus;
			if (f) {
				x -= 6, w += 12, yy -= 3, h += 6;
				lgfx_draw_shadow(x, yy, w, h, 6, 0.32f);
				lgfx_draw_fill(x, yy, w, h, 12, 1, 1, 1, 0.96f);
				lgfx_set_dim(0.1f);
			} else {
				lgfx_draw_fill(x, yy, w, h, 12, 1, 1, 1, 0.06f);
			}
			float ty = yy + (h - 30) * 0.5f;
			text(LFONT_BODY, rows[i][0], x + 20, ty, f ? 1 : (inpane ? 0.9f : 0.6f), 0);
			const char* v = rows[i][1];
			if (is_toggle(v)) {
				lgfx_set_dim(1);
				draw_switch(x + w - 20, yy + (h - 34) * 0.5f, !strcmp(v, "On"), f);
			} else {
				lgfx_set_dim(f ? 0.45f : 1);
				text(LFONT_BODY, v, x + w - 20, ty, f ? 1 : 0.5f, 1);
			}
			lgfx_set_dim(1);
		}
	}
	text(LFONT_SMALL, hint, lx, H - 40, 0.6f, 0);
}

// Text in NextUI's own font, for the restyled version.
static LTex* nextui_text(const char* s, int size) {
	char path[256];
	snprintf(path, sizeof(path), "%s/font1.ttf", RES_PATH);
	TTF_Font* f = TTF_OpenFont(path, size);
	if (!f) return NULL;
	SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s, (SDL_Color){ 255, 255, 255, 255 });
	LTex* t = ltex_from_surface(surf, 0);
	SDL_FreeSurface(surf);
	TTF_CloseFont(f);
	return t;
}

static void ntext(const char* s, int size, float x, float y, float alpha, int right, float dim) {
	LTex* t = nextui_text(s, size);
	if (!t) return;
	lgfx_set_dim(dim);
	lgfx_draw_tex(t, right ? x - t->w : x, y, t->w, t->h, 0, alpha);
	lgfx_set_dim(1);
	ltex_free(t);
}

// Style 2: NextUI's current structure (title pill, full-width rows, the selected row as a light
// pill with the name in a dark pill) with tidier spacing and rounding. Flat: no blur, no lift.
static void style_restyled(void) {
	lgfx_begin_frame();
	lgfx_draw_fill(0, 0, W, H, 0, 0.09f, 0.09f, 0.10f, 1);

	LTex* title = nextui_text("System", 30);
	if (title) {
		lgfx_draw_fill(32, 26, title->w + 44, 52, 26, 1, 1, 1, 0.12f);
		lgfx_draw_tex(title, 54, 26 + (52 - title->h) * 0.5f, title->w, title->h, 0, 1);
		ltex_free(title);
	}
	ntext("12:47  100%", 24, W - 40, 38, 0.8f, 1, 1);

	float y0 = 108, rh = 56;
	for (int i = 0; i < ROWS; i++) {
		float y = y0 + i * rh;
		if (i == FOCUS) {
			lgfx_draw_fill(32, y, W - 64, rh - 6, (rh - 6) * 0.5f, 1, 1, 1, 0.92f);
			LTex* name = nextui_text(rows[i][0], 28);
			if (name) {
				lgfx_draw_fill(38, y + 4, name->w + 36, rh - 14, (rh - 14) * 0.5f, 0.12f, 0.12f, 0.13f, 1);
				lgfx_draw_tex(name, 56, y + (rh - 6 - name->h) * 0.5f, name->w, name->h, 0, 1);
				ltex_free(name);
			}
			ntext(rows[i][1], 26, W - 60, y + 12, 1, 1, 0.12f);
		} else {
			ntext(rows[i][0], 28, 56, y + 10, 0.9f, 0, 1);
			ntext(rows[i][1], 26, W - 60, y + 12, 0.55f, 1, 1);
		}
	}
	ntext("A  CHANGE    B  BACK", 22, W - 40, H - 48, 0.6f, 1, 1);
}

int main(int argc, char* argv[]) {
	const char* prefix = argc > 1 ? argv[1] : "/tmp/settings-mock";
	char path[512];
	InitSettings();
	GFX_init(MODE_MAIN);
	lgfx_init(W, H);

	void (*styles[2])(void) = { style_tvos, style_restyled };
	for (int i = 0; i < 2; i++) {
		styles[i]();
		snprintf(path, sizeof(path), "%s-%d.bmp", prefix, i + 1);
		save(path);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	}
	tvos_controls = 1;
	style_tvos();
	snprintf(path, sizeof(path), "%s-1D.bmp", prefix);
	save(path);
	SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	tvos_controls = 0;
	if (argc > 2) {
		icon_dir = argv[2];
		tvos_controls = 1;
		style_tvos();
		snprintf(path, sizeof(path), "%s-1E.bmp", prefix);
		save(path);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
		tvos_controls = 0;
		style_sections(3);
		snprintf(path, sizeof(path), "%s-top.bmp", prefix);
		save(path);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
		// The Q5 options with the real nine sections. Indexes into sections[]: 0 Appearance,
		// 1 Display, 2 System, 3 Network, 4 Bluetooth, 5 In-Game, 6 FN Switch, 7 Buttons, 10 About.
		static const int nextui_order[] = { 0, 1, -1, 2, -1, 6, 7, 5, -1, 3, 4, -1, 10 };
		static const int new_order[] = { 3, 4, -1, 1, 0, -1, 5, 7, 6, -1, 2, 10 };
		struct { const int* o; int n, focus, pane, rename; const char* hint; char tag; } q5[] = {
			{ nextui_order, 13, 2, -1, 0, "A or >  Into System     B  Back", '1' },
			{ nextui_order, 13, 2, 2, 0, "A  Change     < >  Adjust     B  Back to sections", '2' },
			{ new_order, 12, 3, -1, 1, "A or >  Choose a network     B  Back", '3' },
			{ nextui_order, 13, 2, -1, 0, "A  Into System     B  Back", '4' },
		};
		for (int i = 0; i < 4; i++) {
			split_real(q5[i].o, q5[i].n, q5[i].focus, q5[i].pane, q5[i].hint, q5[i].rename);
			snprintf(path, sizeof(path), "%s-q5-%c.bmp", prefix, q5[i].tag);
			save(path);
			SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
		}
		for (int i = 1; i <= 3; i++) {
			nested_style = i;
			split_real(new_order, 12, 5, 1, "A  Change     B  Back to In-Game", 1);
			snprintf(path, sizeof(path), "%s-q6-%d.bmp", prefix, i);
			save(path);
			SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
		}
		nested_style = 0;
		void (*alts[5])(int) = { alt_grouped, alt_grid, alt_split, alt_split, alt_split };
		const int focus[5] = { 3, 3, 2, 3, 4 };
		for (int i = 0; i < 5; i++) {
			alts[i](focus[i]);
			snprintf(path, sizeof(path), "%s-top%c.bmp", prefix, "ACDEF"[i]);
			save(path);
			SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
		}
	}
	const char variants[] = "ABC";
	for (int i = 0; i < 3; i++) {
		style_variant(variants[i]);
		snprintf(path, sizeof(path), "%s-1%c.bmp", prefix, variants[i]);
		save(path);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	}
	return 0;
}
