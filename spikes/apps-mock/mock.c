// Mockups of two stock apps redrawn for Bramley, for choosing a design: Battery as a Settings
// page, and Game Tracker as an Activity app. Drawn with the launcher's GL kit in the real Settings
// layout, with made-up data and made-up game names. Not shipped.
//   DYLD_LIBRARY_PATH=/var/tmp/nextui/lib SDL_RENDER_VSYNC=0 ./build/mock <res-dir> <out-prefix>
#include <math.h>
#include <msettings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "lgfx.h"

#define W 1024
#define H 768

static const char* res;
static LTex* backdrop;

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

static float text(int font, const char* s, float x, float y, float alpha, int align) {
	LTex* t = ltex_text(font, s, 0);
	if (!t) return 0;
	float w = t->w;
	float tx = align == 1 ? x - t->w : align == 2 ? x - t->w * 0.5f : x;
	lgfx_draw_tex(t, tx, y, t->w, t->h, 0, alpha);
	ltex_free(t);
	return w;
}

static LTex* icon(const char* slug) {
	char path[512];
	snprintf(path, sizeof(path), "%s/settings/%s-36.png", res, slug);
	return ltex_load(path, 0);
}

// Accent blue and the green of a switch, as in Settings.
static const float BLUE[3] = { 0.04f, 0.52f, 1.0f }, GREEN[3] = { 0.20f, 0.78f, 0.35f };

static void top_bar(const char* name) {
	text(LFONT_LABEL, name, 48, 30, 0.55f, 0);
	text(LFONT_LABEL, "82%", W - 48, 30, 0.8f, 1);
	text(LFONT_LABEL, "15:42", W - 48 - 58, 30, 0.8f, 1);
}

static void background(void) {
	lgfx_begin_frame();
	lgfx_draw_tex(backdrop, 0, 0, W, H, 0, 1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.30f);
}

// A drawn stand-in for icons that do not exist yet (Battery, Lights).
static void glyph_icon(const char* slug, float x, float y, float s, float alpha) {
	if (!strcmp(slug, "battery")) {
		const float t[4] = { 0.30f, 0.85f, 0.42f, alpha }, b[4] = { 0.13f, 0.62f, 0.25f, alpha };
		lgfx_draw_gradient(x, y, s, s, s * 0.24f, t, b);
		float bw = s * 0.58f, bh = s * 0.32f, bx = x + (s - bw) * 0.5f - s * 0.03f, by = y + (s - bh) * 0.5f;
		lgfx_draw_ring(bx, by, bw, bh, bh * 0.28f, s * 0.05f, 1, 1, 1, alpha);
		lgfx_draw_fill(bx + bw + s * 0.02f, by + bh * 0.3f, s * 0.05f, bh * 0.4f, s * 0.02f, 1, 1, 1, alpha);
		lgfx_draw_fill(bx + s * 0.08f, by + s * 0.08f, (bw - s * 0.16f) * 0.8f, bh - s * 0.16f, s * 0.03f, 1, 1, 1, alpha);
	} else {
		const float t[4] = { 1.0f, 0.62f, 0.25f, alpha }, b[4] = { 0.93f, 0.33f, 0.40f, alpha };
		lgfx_draw_gradient(x, y, s, s, s * 0.24f, t, b);
		lgfx_draw_fill(x + s * 0.32f, y + s * 0.32f, s * 0.36f, s * 0.36f, s * 0.18f, 1, 1, 1, alpha);
		lgfx_draw_ring(x + s * 0.2f, y + s * 0.2f, s * 0.6f, s * 0.6f, s * 0.3f, s * 0.04f, 1, 1, 1, alpha * 0.6f);
	}
}

// The Settings sidebar as proposed: the stock apps folded in (Lights, Battery).
static void sidebar(int focus, int open) {
	static const char* names[] = { "Network", "Bluetooth", "Display", "Appearance", "Lights", "In-Game", "Buttons", "FN switch", "Battery", "System", "About" };
	static const char* slugs[] = { "network", "bluetooth", "display", "appearance", "lights", "in-game", "buttons", "fn-switch", "battery", "system", "about" };
	static const int groups[] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3 };
	float x = 40, w = 330, rh = 46, gap = 5, y = 84;
	for (int i = 0; i < 11; i++) {
		if (i && groups[i] != groups[i - 1]) y += 12;
		int f = i == focus;
		float fx = x, fw = w, fy = y, fh = rh;
		if (f && !open) {
			fx -= 8, fw += 16, fy -= 3, fh += 6;
			lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
			lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, 0.96f);
		} else
			lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, f ? 0.22f : 0.07f);
		float ia = open && !f ? 0.5f : 1, is = 32;
		LTex* ic = strcmp(slugs[i], "battery") && strcmp(slugs[i], "lights") ? icon(slugs[i]) : NULL;
		if (ic) {
			lgfx_draw_tex(ic, fx + 14, fy + (fh - is) * 0.5f, is, is, 0, ia);
			ltex_free(ic);
		} else
			glyph_icon(slugs[i], fx + 14 + 1, fy + (fh - is) * 0.5f + 1, is - 2, ia);
		if (f && !open) lgfx_set_dim(0.1f);
		text(LFONT_BODY, names[i], fx + 60, fy + (fh - 30) * 0.5f, open ? (f ? 0.95f : 0.45f) : (f ? 1 : 0.92f), 0);
		lgfx_set_dim(1);
		y += rh + gap;
	}
}

static void pane(void) {
	lgfx_draw_fill(410, 84, W - 450, H - 70 - 84, 22, 0, 0, 0, 0.18f);
}

// Made-up battery history: 48 half hours, oldest first, with a charge in the middle.
static int level[48], charging[48];
static void make_battery(void) {
	float l = 58;
	for (int i = 0; i < 48; i++) {
		charging[i] = i >= 30 && i < 38;
		l += charging[i] ? 9.0f : (i < 14 ? -0.4f : i < 30 ? -2.2f : i < 42 ? -0.6f : -0.8f);
		if (l > 100) l = 100;
		level[i] = (int)l;
	}
}

static void stat(float x, float y, const char* label, const char* value) {
	text(LFONT_LABEL, label, x, y, 0.55f, 0);
	text(LFONT_BODY, value, x, y + 24, 0.95f, 0);
}

static void range_row(float x, float y, float w, int focused) {
	float h = 48;
	if (focused) {
		x -= 6, w += 12, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.96f);
		lgfx_set_dim(0.1f);
	} else
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f);
	float ty = y + (h - 30) * 0.5f;
	text(LFONT_BODY, "Show", x + 20, ty, 1, 0);
	lgfx_set_dim(focused ? 0.45f : 1);
	text(LFONT_BODY, focused ? "\xE2\x80\xB9  Last 24 hours  \xE2\x80\xBA" : "Last 24 hours", x + w - 20, ty, focused ? 1 : 0.5f, 1);
	lgfx_set_dim(1);
}

// Battery as a Settings page. style 'A': bars like iOS; 'B': a filled line with charging shaded.
static void battery(char style) {
	background();
	top_bar("Settings");
	sidebar(8, 1);
	pane();
	float px = 410, pw = W - 450, py = 84, x = px + 28;
	text(LFONT_TITLE, "Battery", x, py + 20, 1, 0);

	// the level now, large, with the estimate beside it
	float hw = text(LFONT_HERO, "82%", x, py + 70, 1, 0);
	text(LFONT_BODY, "About 4 h 10 min left", x + hw + 20, py + 78, 0.9f, 0);
	text(LFONT_SMALL, "Not charging", x + hw + 20, py + 110, 0.55f, 0);

	// the chart
	float cx = px + 20, cw = pw - 40, cy = py + 160, ch = 250;
	lgfx_draw_fill(cx, cy, cw, ch, 16, 1, 1, 1, 0.06f);
	text(LFONT_LABEL, "BATTERY LEVEL", cx + 18, cy + 14, 0.55f, 0);
	float gx = cx + 18, gw = cw - 18 - 58, gy = cy + 50, gh = ch - 50 - 40;
	for (int p = 0; p <= 100; p += 50) {
		float ly = gy + gh * (1 - p / 100.0f);
		lgfx_draw_fill(gx, ly, gw, 1, 0, 1, 1, 1, 0.12f);
		char s[8];
		snprintf(s, sizeof(s), "%d%%", p);
		text(LFONT_SMALL, s, cx + cw - 16, ly - 11, 0.45f, 1);
	}
	float slot = gw / 48;
	if (style == 'A') {
		for (int i = 0; i < 48; i++) {
			float bh = gh * level[i] / 100.0f, bx = gx + i * slot + 1.5f;
			const float* c = charging[i] ? GREEN : NULL;
			float r = c ? c[0] : 1, g = c ? c[1] : 1, b = c ? c[2] : 1;
			if (!c && level[i] <= 20) r = 1, g = 0.27f, b = 0.23f;
			lgfx_draw_fill(bx, gy + gh - bh, slot - 3, bh, 2, r, g, b, c ? 1 : 0.85f);
		}
	} else {
		for (int i = 0; i < 48; i++)
			if (charging[i]) lgfx_draw_fill(gx + i * slot, gy, slot + 0.5f, gh, 0, GREEN[0], GREEN[1], GREEN[2], 0.16f);
		// a filled area under the line, column by column, then the line on top
		for (int i = 0; i < 47; i++) {
			for (int k = 0; k < 4; k++) {
				float t = k / 4.0f, l = level[i] + (level[i + 1] - level[i]) * t;
				float bx = gx + (i + 0.5f + t) * slot, bh = gh * l / 100.0f;
				lgfx_draw_fill(bx, gy + gh - bh, slot / 4 + 1.0f, bh, 0, 1, 1, 1, 0.10f);
				lgfx_draw_fill(bx, gy + gh - bh - 1.5f, slot / 4 + 0.5f, 3, 1.5f, charging[i] ? GREEN[0] : 1, charging[i] ? GREEN[1] : 1, charging[i] ? GREEN[2] : 1, 1);
			}
		}
		float ex = gx + 47.5f * slot, ey = gy + gh * (1 - level[47] / 100.0f);
		lgfx_draw_fill(ex - 6, ey - 6, 12, 12, 6, 1, 1, 1, 1);
	}
	static const char* ticks[] = { "24 h ago", "18 h", "12 h", "6 h", "Now" };
	for (int i = 0; i < 5; i++) text(LFONT_SMALL, ticks[i], gx + gw * i / 4.0f, gy + gh + 10, 0.45f, i == 0 ? 0 : i == 4 ? 1 : 2);

	// stats from the battery log, then the one setting
	float sy = cy + ch + 22, sw = (pw - 56) / 3;
	stat(x, sy, "SINCE CHARGE", "4 h 50 min");
	stat(x + sw, sy, "LAST CHARGED", "To 100%");
	stat(x + 2 * sw, sy, "LONGEST", "6 h 40 min");
	range_row(px + 20, sy + 74, pw - 40, 1);
	text(LFONT_SMALL, "\xE2\x80\xB9 \xE2\x80\xBA  Change range        B  Back", 40, H - 40, 0.6f, 0);
}

// Activity: made-up play history.
typedef struct { const char *name, *console; int minutes, plays; float hue[3]; } Game;
static Game games[] = {
	{ "Pixel Quest", "Game Boy Advance", 412, 23, { 0.36f, 0.50f, 0.95f } },
	{ "Moss Kingdom", "Super Nintendo", 265, 11, { 0.22f, 0.70f, 0.45f } },
	{ "Star Courier", "Game Boy", 148, 9, { 0.95f, 0.55f, 0.25f } },
	{ "Turbo Lane", "Mega Drive", 96, 7, { 0.90f, 0.30f, 0.40f } },
	{ "Deep Signal", "PlayStation", 51, 3, { 0.55f, 0.40f, 0.85f } },
};
#define GAMES 5
static const int week[7] = { 35, 72, 18, 0, 64, 128, 85 }; // minutes, Monday first; Sunday is today

static void art(const Game* g, float x, float y, float s, float alpha) {
	const float t[4] = { g->hue[0], g->hue[1], g->hue[2], alpha }, b[4] = { g->hue[0] * 0.45f, g->hue[1] * 0.45f, g->hue[2] * 0.45f, alpha };
	lgfx_draw_gradient(x, y, s, s, s * 0.16f, t, b);
	char ini[3] = { g->name[0], strchr(g->name, ' ') ? strchr(g->name, ' ')[1] : 0, 0 };
	LTex* tx = ltex_text(s > 100 ? LFONT_HERO : LFONT_TITLE, ini, 0);
	if (tx) {
		lgfx_draw_tex(tx, x + (s - tx->w) * 0.5f, y + (s - tx->h) * 0.5f, tx->w, tx->h, 0, alpha * 0.9f);
		ltex_free(tx);
	}
}

static void hm(char* out, size_t n, int minutes) {
	if (minutes >= 60) snprintf(out, n, "%d h %d min", minutes / 60, minutes % 60);
	else snprintf(out, n, "%d min", minutes);
}

static void week_chart(float x, float y, float w, float h) {
	static const char* days[] = { "M", "T", "W", "T", "F", "S", "S" };
	float slot = w / 7, gh = h - 30;
	for (int i = 0; i < 7; i++) {
		float bw = slot * 0.46f, bx = x + i * slot + (slot - bw) * 0.5f, bh = gh * week[i] / 140.0f;
		lgfx_draw_fill(bx, y, bw, gh, bw * 0.3f, 1, 1, 1, 0.06f);
		if (bh > 0) lgfx_draw_fill(bx, y + gh - bh, bw, bh, bw * 0.3f, i == 6 ? BLUE[0] : 1, i == 6 ? BLUE[1] : 1, i == 6 ? BLUE[2] : 1, i == 6 ? 1 : 0.8f);
		text(LFONT_SMALL, days[i], bx + bw * 0.5f, y + gh + 6, i == 6 ? 0.95f : 0.5f, 2);
	}
}

// Activity 'A': a summary with the week on top, the most played games in a list below.
static void activity_a(void) {
	background();
	top_bar("Activity");
	int focus = 0;
	text(LFONT_LABEL, "THIS WEEK", 56, 76, 0.55f, 0);
	text(LFONT_HERO, "6 h 42 min", 56, 100, 1, 0);
	text(LFONT_BODY, "1 h 10 min more than last week", 56, 168, 0.6f, 0);
	lgfx_draw_fill(560, 76, 408, 150, 18, 1, 1, 1, 0.06f);
	week_chart(576, 92, 376, 120);

	text(LFONT_LABEL, "MOST PLAYED, ALL TIME", 56, 258, 0.55f, 0);
	float y = 288, x = 56, w = W - 112, rh = 76, gap = 8;
	for (int i = 0; i < 5; i++, y += rh + gap) {
		int f = i == focus;
		float fx = x, fy = y, fw = w, fh = rh;
		if (f) {
			fx -= 8, fw += 16, fy -= 3, fh += 6;
			lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
			lgfx_draw_fill(fx, fy, fw, fh, 16, 1, 1, 1, 0.96f);
		} else
			lgfx_draw_fill(fx, fy, fw, fh, 16, 1, 1, 1, 0.07f);
		art(&games[i], fx + 12, fy + 10, fh - 20, 1);
		if (f) lgfx_set_dim(0.1f);
		text(LFONT_BODY, games[i].name, fx + fh + 10, fy + 10, 1, 0);
		char t[32], sub[64];
		hm(t, sizeof(t), games[i].minutes);
		snprintf(sub, sizeof(sub), "%s  \xC2\xB7  played %d times", games[i].console, games[i].plays);
		text(LFONT_SMALL, sub, fx + fh + 10, fy + 44, f ? 0.7f : 0.55f, 0);
		text(LFONT_BODY, t, fx + fw - 24, fy + (fh - 30) * 0.5f, f ? 1 : 0.85f, 1);
		lgfx_set_dim(1);
		// a thin bar for each game's share of the most played
		float bw = 180, bx = fx + fw - 24 - 150 - bw, by = fy + fh * 0.5f - 3;
		lgfx_draw_fill(bx, by, bw, 6, 3, f ? 0 : 1, f ? 0 : 1, f ? 0 : 1, f ? 0.10f : 0.12f);
		lgfx_draw_fill(bx, by, bw * games[i].minutes / games[0].minutes, 6, 3, BLUE[0], BLUE[1], BLUE[2], 1);
	}
	text(LFONT_SMALL, "A  Play        L R  Week        B  Back", 40, H - 40, 0.6f, 0);
}

// Activity 'B': tvOS style. A shelf of large game cards, the focused one lifted, its details below.
static void activity_b(void) {
	background();
	top_bar("Activity");
	int focus = 1;
	text(LFONT_HERO, "6 h 42 min", 56, 70, 1, 0);
	text(LFONT_BODY, "played this week, 1 h 10 min more than last week", 56, 138, 0.6f, 0);
	lgfx_draw_fill(640, 64, 328, 110, 18, 1, 1, 1, 0.06f);
	week_chart(652, 74, 304, 92);

	float s = 200, gap = 36, x = 56, y = 250;
	for (int i = 0; i < GAMES; i++, x += s + gap) {
		int f = i == focus;
		float fx = x, fy = y, fs = s;
		if (f) {
			fx -= 14, fy -= 14, fs += 28;
			lgfx_draw_shadow(fx, fy, fs, fs, 14, 0.55f);
		}
		art(&games[i], fx, fy, fs, f ? 1 : 0.8f);
		char t[32];
		hm(t, sizeof(t), games[i].minutes);
		text(LFONT_BODY, t, x + s * 0.5f, y + s + (f ? 26 : 14), f ? 1 : 0.6f, 2);
	}
	// details of the focused game
	const Game* g = &games[focus];
	float dy = 560;
	lgfx_draw_fill(56, dy, W - 112, 120, 18, 1, 1, 1, 0.06f);
	text(LFONT_TITLE, g->name, 80, dy + 18, 1, 0);
	text(LFONT_SMALL, g->console, 80, dy + 66, 0.55f, 0);
	stat(440, dy + 26, "PLAYED", "11 times");
	stat(610, dy + 26, "LAST PLAYED", "Yesterday");
	stat(800, dy + 26, "FIRST PLAYED", "3 Oct");
	text(LFONT_SMALL, "A  Play        \xE2\x80\xB9 \xE2\x80\xBA  Game        B  Back", 40, H - 40, 0.6f, 0);
}

// Minutes per game per day (Monday first), for the stacked chart; sums match week[].
static const int per_day[GAMES][7] = {
	{ 20, 40, 18, 0, 30, 60, 40 }, { 15, 20, 0, 0, 24, 40, 25 }, { 0, 12, 0, 0, 10, 20, 12 }, { 0, 0, 0, 0, 0, 8, 8 }, { 0, 0, 0, 0, 0, 0, 0 },
};

static void segmented(float cx, float y, const char* a, const char* b, int sel) {
	float w = 220, h = 36, x = cx - w * 0.5f;
	lgfx_draw_fill(x, y, w, h, h * 0.5f, 1, 1, 1, 0.10f);
	lgfx_draw_fill(x + 3 + sel * (w * 0.5f), y + 3, w * 0.5f - 6, h - 6, (h - 6) * 0.5f, 1, 1, 1, 0.28f);
	text(LFONT_SMALL, a, x + w * 0.25f, y + 7, 1, 2);
	text(LFONT_SMALL, b, x + w * 0.75f, y + 7, 1, 2);
}

// Activity 'C': like iOS Screen Time. Week or Day, the daily average, bars split by game, then
// the most played with a bar under each name.
static void activity_c(void) {
	background();
	top_bar("Activity");
	segmented(W * 0.5f, 26, "Week", "Day", 0);
	float x = 56, w = W - 112;
	text(LFONT_LABEL, "DAILY AVERAGE", x, 82, 0.55f, 0);
	float hw = text(LFONT_HERO, "57 min", x, 104, 1, 0);
	text(LFONT_BODY, "\xE2\x86\x91 18% from last week", x + hw + 22, 124, 0.6f, 0);
	// chart card
	float cy = 184, ch = 230;
	lgfx_draw_fill(x, cy, w, ch, 18, 1, 1, 1, 0.06f);
	float gx = x + 24, gw = w - 24 - 70, gy = cy + 22, gh = ch - 22 - 40;
	for (int m = 0; m <= 120; m += 60) {
		float ly = gy + gh * (1 - m / 140.0f);
		lgfx_draw_fill(gx, ly, gw, 1, 0, 1, 1, 1, 0.12f);
		text(LFONT_SMALL, m ? (m == 60 ? "1 h" : "2 h") : "0", x + w - 20, ly - 11, 0.45f, 1);
	}
	float avg = gy + gh * (1 - 57 / 140.0f);
	for (float dx = 0; dx < gw; dx += 14) lgfx_draw_fill(gx + dx, avg - 1, 8, 2, 1, GREEN[0], GREEN[1], GREEN[2], 0.9f);
	text(LFONT_SMALL, "avg", gx + 2, avg - 26, 0.8f, 0);
	static const char* days[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
	float slot = gw / 7;
	for (int d = 0; d < 7; d++) {
		float bw = slot * 0.42f, bx = gx + d * slot + (slot - bw) * 0.5f, base = gy + gh;
		for (int g = 0; g < GAMES; g++) {
			float bh = gh * per_day[g][d] / 140.0f;
			if (bh <= 0) continue;
			lgfx_draw_fill(bx, base - bh, bw, bh - 1.5f, 3, games[g].hue[0], games[g].hue[1], games[g].hue[2], 1);
			base -= bh;
		}
		text(LFONT_SMALL, days[d], bx + bw * 0.5f, gy + gh + 10, d == 6 ? 0.95f : 0.5f, 2);
	}
	// most played
	text(LFONT_LABEL, "MOST PLAYED", x, 436, 0.55f, 0);
	float y = 464, rh = 66, gap = 6;
	for (int i = 0; i < 3; i++, y += rh + gap) {
		int f = i == 0;
		float fx = x, fy = y, fw = w, fh = rh;
		if (f) {
			fx -= 8, fw += 16, fy -= 3, fh += 6;
			lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
			lgfx_draw_fill(fx, fy, fw, fh, 16, 1, 1, 1, 0.96f);
		} else
			lgfx_draw_fill(fx, fy, fw, fh, 16, 1, 1, 1, 0.07f);
		art(&games[i], fx + 10, fy + 9, fh - 18, 1);
		if (f) lgfx_set_dim(0.1f);
		text(LFONT_BODY, games[i].name, fx + fh + 8, fy + 6, 1, 0);
		char t[32];
		hm(t, sizeof(t), games[i].minutes / 3);
		text(LFONT_BODY, t, fx + fw - 50, fy + (fh - 30) * 0.5f, f ? 1 : 0.85f, 1);
		text(LFONT_BODY, "\xE2\x80\xBA", fx + fw - 22, fy + (fh - 30) * 0.5f, f ? 1 : 0.45f, 1);
		lgfx_set_dim(1);
		float bw = (fw - fh - 8 - 200) * games[i].minutes / games[0].minutes;
		lgfx_draw_fill(fx + fh + 8, fy + fh - 18, bw, 6, 3, games[i].hue[0], games[i].hue[1], games[i].hue[2], 1);
	}
	text(LFONT_SMALL, "A  Details        L R  Week or Day        B  Back", 40, H - 40, 0.6f, 0);
}

// Activity 'D': A's summary on top, B's cards for the most played below, details under them.
static void activity_d(void) {
	background();
	top_bar("Activity");
	text(LFONT_LABEL, "THIS WEEK", 56, 76, 0.55f, 0);
	text(LFONT_HERO, "6 h 42 min", 56, 100, 1, 0);
	text(LFONT_BODY, "1 h 10 min more than last week", 56, 168, 0.6f, 0);
	lgfx_draw_fill(560, 76, 408, 150, 18, 1, 1, 1, 0.06f);
	week_chart(576, 92, 376, 120);
	text(LFONT_LABEL, "MOST PLAYED", 56, 262, 0.55f, 0);
	int focus = 0;
	float s = 150, gap = 28, x = 56, y = 300;
	for (int i = 0; i < GAMES; i++, x += s + gap) {
		int f = i == focus;
		float fx = x, fy = y, fs = s;
		if (f) {
			fx -= 10, fy -= 10, fs += 20;
			lgfx_draw_shadow(fx, fy, fs, fs, 12, 0.55f);
		}
		art(&games[i], fx, fy, fs, f ? 1 : 0.8f);
		char t[32];
		hm(t, sizeof(t), games[i].minutes);
		text(LFONT_SMALL, t, x + s * 0.5f, y + s + (f ? 20 : 10), f ? 1 : 0.6f, 2);
	}
	const Game* g = &games[focus];
	float dy = 540;
	lgfx_draw_fill(56, dy, W - 112, 130, 18, 1, 1, 1, 0.06f);
	text(LFONT_TITLE, g->name, 80, dy + 20, 1, 0);
	text(LFONT_SMALL, g->console, 80, dy + 68, 0.55f, 0);
	stat(440, dy + 30, "PLAYED", "23 times");
	stat(610, dy + 30, "LAST PLAYED", "Today");
	stat(800, dy + 30, "FIRST PLAYED", "12 Sep");
	text(LFONT_SMALL, "A  Play        \xE2\x80\xB9 \xE2\x80\xBA  Game        B  Back", 40, H - 40, 0.6f, 0);
}

// A Settings row: a value on the right ("" none), a switch (sw 0 or 1), or a colour dot.
static void srow(float x, float y, float w, const char* name, const char* value, int sw, const float* dot, int focused) {
	float h = 48;
	if (focused) {
		x -= 6, w += 12, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.96f);
		lgfx_set_dim(0.1f);
	} else
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f);
	float ty = y + (h - 30) * 0.5f, right = x + w - 20;
	text(LFONT_BODY, name, x + 20, ty, 1, 0);
	lgfx_set_dim(1);
	if (sw >= 0) {
		float sw_w = 58, sh = 34, sy = y + (h - sh) * 0.5f;
		if (sw) lgfx_draw_fill(right - sw_w, sy, sw_w, sh, sh * 0.5f, GREEN[0], GREEN[1], GREEN[2], 1);
		else lgfx_draw_fill(right - sw_w, sy, sw_w, sh, sh * 0.5f, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, focused ? 0.15f : 0.22f);
		float k = sh - 6;
		lgfx_draw_fill(sw ? right - k - 3 : right - sw_w + 3, sy + 3, k, k, k * 0.5f, 1, 1, 1, 1);
	} else if (dot) {
		lgfx_draw_fill(right - 28, y + (h - 28) * 0.5f, 28, 28, 14, dot[0], dot[1], dot[2], 1);
		lgfx_draw_ring(right - 28, y + (h - 28) * 0.5f, 28, 28, 14, 1.5f, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, 0.3f);
	} else if (value && value[0]) {
		lgfx_set_dim(focused ? 0.45f : 1);
		text(LFONT_BODY, value, right, ty, focused ? 1 : 0.5f, 1);
		lgfx_set_dim(1);
	}
}

static void page_title(const char* back, const char* title) {
	float px = 410, py = 84;
	if (back) {
		char b[64];
		snprintf(b, sizeof(b), "\xE2\x80\xB9  %s", back);
		text(LFONT_BODY, b, px + 28, py + 18, 0.5f, 0);
		text(LFONT_TITLE, title, px + 28, py + 52, 1, 0);
	} else
		text(LFONT_TITLE, title, px + 28, py + 20, 1, 0);
}

// Date & Time, under System. auto_on: set from the internet, so the date and time rows are dimmed.
// Otherwise the date row is open for changing, as three columns that move with up and down.
static void dim_row(float x, float y, float w, const char* name, const char* value) {
	lgfx_draw_fill(x, y, w, 48, 12, 1, 1, 1, 0.03f);
	text(LFONT_BODY, name, x + 20, y + 9, 0.4f, 0);
	text(LFONT_BODY, value, x + w - 20, y + 9, 0.3f, 1);
}

static void date_time(int auto_on) {
	background();
	top_bar("Settings");
	sidebar(9, 1);
	pane();
	page_title("System", "Date & Time");
	float x = 430, w = 534, y = 206, step = 54;
	srow(x, y, w, "Set automatically", NULL, auto_on, NULL, auto_on);
	srow(x, y += step, w, "Time zone", "Hong Kong", -1, NULL, 0);
	srow(x, y += step, w, "24-hour time", NULL, 1, NULL, 0);
	y += step;
	if (auto_on) {
		dim_row(x, y, w, "Date", "28 Oct 2026");
		dim_row(x, y + step, w, "Time", "16:12");
		LTex* d = ltex_text_wrapped(LFONT_SMALL, "Set from the internet when Wi-Fi is on. Turn this off to set the date and time yourself.", (int)w - 40, 2, 0);
		if (d) lgfx_draw_tex(d, x + 20, y + 2 * step + 16, d->w, d->h, 0, 0.5f), ltex_free(d);
		text(LFONT_SMALL, "A  Change        B  Back", 40, H - 40, 0.6f, 0);
		return;
	}
	float h = 200, rx = x - 6, rw = w + 12;
	lgfx_draw_shadow(rx, y - 3, rw, h, 6, 0.32f);
	lgfx_draw_fill(rx, y - 3, rw, h, 14, 1, 1, 1, 0.96f);
	lgfx_set_dim(0.1f);
	text(LFONT_BODY, "Date", rx + 26, y + 9, 1, 0);
	static const char* cols[3][3] = { { "27", "28", "29" }, { "Sep", "Oct", "Nov" }, { "2025", "2026", "2027" } };
	float cw = 120, cx = rx + rw - 26 - 3 * cw, cy = y + 52;
	for (int c = 0; c < 3; c++) {
		float mx = cx + c * cw + cw * 0.5f;
		lgfx_set_dim(1);
		if (c == 0) lgfx_draw_fill(cx + c * cw + 8, cy + 44, cw - 16, 48, 12, BLUE[0], BLUE[1], BLUE[2], 1);
		else lgfx_draw_fill(cx + c * cw + 8, cy + 44, cw - 16, 48, 12, 0, 0, 0, 0.07f);
		lgfx_set_dim(c == 0 ? 1 : 0.1f);
		text(LFONT_BODY, cols[c][1], mx, cy + 53, 1, 2);
		lgfx_set_dim(0.1f);
		text(LFONT_BODY, cols[c][0], mx, cy + 6, 0.22f, 2);
		text(LFONT_BODY, cols[c][2], mx, cy + 100, 0.22f, 2);
	}
	lgfx_set_dim(1);
	srow(x, y + h + 8, w, "Time", "16:12", -1, NULL, 0);
	text(LFONT_SMALL, "\xE2\x80\xB9 \xE2\x80\xBA  Day, month, year        \xE2\x86\x91 \xE2\x86\x93  Change        A  Done        B  Cancel", 40, H - 40, 0.6f, 0);
}

// Lights: on or off first, then whether all lights match, then the light strip and its settings.
static void lights(void) {
	background();
	top_bar("Settings");
	sidebar(4, 1);
	pane();
	page_title(NULL, "Lights");
	float px = 410, pw = W - 450, x = 430, w = 534, step = 54;
	srow(x, 150, w, "Lights", NULL, 1, NULL, 0);
	srow(x, 150 + step, w, "Same for all lights", NULL, 0, NULL, 0);
	static const char* zones[] = { "F1", "F2", "Top", "Triggers" };
	float sx = px + 28, sw = (pw - 56 - 3 * 10) / 4, sy = 272;
	const float purple[3] = { 0.55f, 0.20f, 0.85f };
	for (int i = 0; i < 4; i++) {
		int f = i == 2;
		float zx = sx + i * (sw + 10);
		lgfx_draw_fill(zx, sy, sw, 76, 14, 1, 1, 1, f ? 0.22f : 0.06f);
		if (f) lgfx_draw_ring(zx, sy, sw, 76, 14, 2.5f, 1, 1, 1, 0.9f);
		float lw = sw * 0.6f, lx = zx + (sw - lw) * 0.5f;
		lgfx_draw_fill(lx - 4, sy + 14, lw + 8, 18, 9, purple[0], purple[1], purple[2], 0.35f);
		lgfx_draw_fill(lx, sy + 18, lw, 10, 5, purple[0] * 1.3f, purple[1] * 1.3f, purple[2] * 1.3f, 1);
		text(LFONT_SMALL, zones[i], zx + sw * 0.5f, sy + 42, f ? 1 : 0.6f, 2);
	}
	float y = 366;
	srow(x, y, w, "Effect", "\xE2\x80\xB9  Breathe  \xE2\x80\xBA", -1, NULL, 1);
	srow(x, y += step, w, "Colour", NULL, -1, purple, 0);
	srow(x, y += step, w, "Brightness", "100%", -1, NULL, 0);
	srow(x, y += step, w, "Speed", "Slow", -1, NULL, 0);
	text(LFONT_SMALL, "Static, Breathe, Blink, Rainbow, Fire, Twinkle, More effects", x + 20, y + step + 10, 0.45f, 0);
	text(LFONT_SMALL, "L R  Light        \xE2\x80\xB9 \xE2\x80\xBA  Change        B  Back", 40, H - 40, 0.6f, 0);
}

// Test Buttons, under Buttons: the Brick drawn plainly. A held control lights blue; once pressed,
// it stays green, so the screen works as a checklist.
static void key(float x, float y, float w, float h, const char* label, int state) {
	// state 0 untested, 1 held, 2 tested
	float r = state == 1 ? BLUE[0] : state == 2 ? GREEN[0] : 1, g = state == 1 ? BLUE[1] : state == 2 ? GREEN[1] : 1,
	      b = state == 1 ? BLUE[2] : state == 2 ? GREEN[2] : 1, a = state == 1 ? 1 : state == 2 ? 0.75f : 0.14f;
	lgfx_draw_fill(x, y, w, h, h < w ? h * 0.5f : w * 0.5f, r, g, b, a);
	if (label) text(LFONT_SMALL, label, x + w * 0.5f, y + (h - 22) * 0.5f, state ? 1 : 0.7f, 2);
}

// The Brick's front, drawn as one shape: the shoulders are tabs behind the top edge (style 'A'),
// or left off for a separate top view (style 'B').
static void brick_front(float bx, float by, float bw, float bh, char style) {
	if (style == 'A') {
		// side by side tabs behind the top edge, each showing 30 px above it: L2 outside, L1 inside
		float tw = 76, th = 60, ty = by - 30;
		struct { float x; const char* n; int st; } t[4] = {
			{ bx + 20, "L2", 0 }, { bx + 20 + tw + 8, "L1", 2 }, { bx + bw - 20 - 2 * tw - 8, "R1", 1 }, { bx + bw - 20 - tw, "R2", 0 },
		};
		for (int i = 0; i < 4; i++) {
			const float* c = t[i].st == 1 ? BLUE : t[i].st == 2 ? GREEN : NULL;
			lgfx_draw_fill(t[i].x, ty, tw, th, 14, c ? c[0] : 1, c ? c[1] : 1, c ? c[2] : 1, c ? (t[i].st == 2 ? 0.75f : 1) : 0.16f);
			text(LFONT_SMALL, t[i].n, t[i].x + tw * 0.5f, ty + 4, t[i].st ? 1 : 0.7f, 2);
		}
	}
	// the body, opaque so the tabs show only above its edge
	lgfx_draw_fill(bx, by, bw, bh, 36, 0.16f, 0.15f, 0.22f, 1);
	lgfx_draw_fill(bx, by, bw, bh, 36, 1, 1, 1, 0.06f);
	lgfx_draw_ring(bx, by, bw, bh, 36, 1.5f, 1, 1, 1, 0.16f);
	lgfx_draw_fill(bx + 50, by + 24, bw - 100, 140, 10, 0, 0, 0, 0.45f);
	text(LFONT_BODY, "R1  \xC2\xB7  A", bx + bw * 0.5f, by + 78, 0.9f, 2);
	float dx = bx + 90, dy = by + 268, k = 38;
	lgfx_draw_fill(dx - k * 0.5f, dy - k * 1.5f, k, k * 3, 8, 1, 1, 1, 0.14f);
	lgfx_draw_fill(dx - k * 1.5f, dy - k * 0.5f, k * 3, k, 8, 1, 1, 1, 0.14f);
	lgfx_draw_fill(dx - k * 0.5f, dy - k * 1.5f, k, k, 8, GREEN[0], GREEN[1], GREEN[2], 0.75f);
	lgfx_draw_fill(dx + k * 0.5f, dy - k * 0.5f, k, k, 8, GREEN[0], GREEN[1], GREEN[2], 0.75f);
	float fx = bx + bw - 90, fy = by + 268, r = 40;
	key(fx - r * 0.5f, fy - r * 1.5f, r, r, "X", 0);
	key(fx - r * 0.5f, fy + r * 0.5f, r, r, "B", 2);
	key(fx - r * 1.5f, fy - r * 0.5f, r, r, "Y", 0);
	key(fx + r * 0.5f, fy - r * 0.5f, r, r, "A", 1);
	key(bx + bw * 0.5f - 60, by + 180, 52, 22, "FN1", 0);
	key(bx + bw * 0.5f + 8, by + 180, 52, 22, "FN2", 0);
	float my = by + bh - 46;
	key(bx + bw * 0.5f - 130, my, 76, 24, "SELECT", 0);
	key(bx + bw * 0.5f - 38, my, 76, 24, "MENU", 0);
	key(bx + bw * 0.5f + 54, my, 76, 24, "START", 0);
}

static void test_buttons(char style) {
	background();
	top_bar("Settings");
	sidebar(6, 1);
	pane();
	page_title("Buttons", "Test Buttons");
	text(LFONT_SMALL, "7 of 18 tested", 438 + 520, 168, 0.6f, 1);
	float px = 410, pw = W - 450, bw = 400, bx = px + (pw - bw) * 0.5f;
	if (style == 'A') {
		brick_front(bx, 268, bw, 400, 'A');
	} else {
		// the top edge as its own strip, then the front
		float ty = 206, tw = bw, th = 40;
		lgfx_draw_fill(bx, ty, tw, th, 12, 1, 1, 1, 0.06f);
		text(LFONT_SMALL, "TOP", bx - 12, ty + 9, 0.4f, 1);
		const char* names[4] = { "L2", "L1", "R1", "R2" };
		int states[4] = { 0, 2, 1, 0 };
		float kw = 72, gap = 10, kx[4] = { bx + 12, bx + 12 + kw + gap, bx + tw - 12 - 2 * kw - gap, bx + tw - 12 - kw };
		for (int i = 0; i < 4; i++) key(kx[i], ty + 7, kw, th - 14, names[i], states[i]);
		brick_front(bx, 266, bw, 400, 'B');
	}
	text(LFONT_SMALL, "Hold SELECT and START to leave", 40, H - 40, 0.6f, 0);
}

// Hybrid: the earlier glass body (translucent, outlined, round keys) with A's shoulder tabs, which
// sit flush on the top edge as part of the shape.
static void test_buttons_hybrid(void) {
	background();
	top_bar("Settings");
	sidebar(6, 1);
	pane();
	page_title("Buttons", "Test Buttons");
	text(LFONT_SMALL, "7 of 18 tested", 438, 196, 0.6f, 0);
	float px = 410, pw = W - 450, bw = 440, bx = px + (pw - bw) * 0.5f, by = 276, bh = 396;
	// shoulder tabs: rounded on top, square where they meet the body
	float tw = 84, th = 26, ty = by - th - 2;
	struct { float x; const char* n; int st; } t[4] = {
		{ bx + 24, "L2", 0 }, { bx + 24 + tw + 8, "L1", 2 }, { bx + bw - 24 - 2 * tw - 8, "R1", 1 }, { bx + bw - 24 - tw, "R2", 0 },
	};
	for (int i = 0; i < 4; i++) {
		const float* c = t[i].st == 1 ? BLUE : t[i].st == 2 ? GREEN : NULL;
		float r = c ? c[0] : 1, g = c ? c[1] : 1, b = c ? c[2] : 1, a = c ? (t[i].st == 2 ? 0.75f : 1) : 0.14f;
		lgfx_draw_fill(t[i].x, ty, tw, th, 7, r, g, b, a);
		text(LFONT_SMALL, t[i].n, t[i].x + tw * 0.5f, ty + 2, t[i].st ? 1 : 0.7f, 2);
	}
	lgfx_draw_fill(bx, by, bw, bh, 36, 1, 1, 1, 0.05f);
	lgfx_draw_ring(bx, by, bw, bh, 36, 2, 1, 1, 1, 0.18f);
	lgfx_draw_fill(bx + 56, by + 26, bw - 112, 140, 10, 0, 0, 0, 0.35f);
	text(LFONT_BODY, "R1  \xC2\xB7  A", bx + bw * 0.5f, by + 80, 0.9f, 2);
	key(bx + bw * 0.5f - 62, by + 182, 54, 24, "FN1", 0);
	key(bx + bw * 0.5f + 8, by + 182, 54, 24, "FN2", 0);
	float dx = bx + 100, dy = by + 262, k = 40;
	key(dx - k * 0.5f, dy - k * 1.5f, k, k, NULL, 2);
	key(dx - k * 0.5f, dy + k * 0.5f, k, k, NULL, 2);
	key(dx - k * 1.5f, dy - k * 0.5f, k, k, NULL, 0);
	key(dx + k * 0.5f, dy - k * 0.5f, k, k, NULL, 2);
	float fx = bx + bw - 100, fy = by + 262, r = 40;
	key(fx - r * 0.5f, fy - r * 1.5f, r, r, "X", 0);
	key(fx - r * 0.5f, fy + r * 0.5f, r, r, "B", 2);
	key(fx - r * 1.5f, fy - r * 0.5f, r, r, "Y", 0);
	key(fx + r * 0.5f, fy - r * 0.5f, r, r, "A", 1);
	float my = by + bh - 44;
	key(bx + bw * 0.5f - 130, my, 76, 26, "SELECT", 0);
	key(bx + bw * 0.5f - 38, my, 76, 26, "MENU", 0);
	key(bx + bw * 0.5f + 54, my, 76, 26, "START", 0);
	text(LFONT_SMALL, "Hold SELECT and START to leave", 40, H - 40, 0.6f, 0);
}

// The hybrid drawn to the Brick Hammer's real layout (from TrimUI's product photo): a tall body,
// the screen across the top half, F1 and F2 as pills under it, three round buttons at the bottom
// (MENU, SELECT, START), the shoulders as tabs on the top edge, volume on the left side, power on the right.
static void test_buttons_real(void) {
	background();
	top_bar("Settings");
	sidebar(6, 1);
	pane();
	page_title("Buttons", "Test Buttons");
	text(LFONT_SMALL, "7 of 18 tested", 956, 146, 0.6f, 1);
	float px = 410, pw = W - 450, bh = 466, bw = bh * 0.657f, bx = px + (pw - bw) * 0.5f, by = 214;
	#define RX(v) (bx + (v) * bw)
	#define RY(v) (by + (v) * bh)
	// shoulders on the top edge
	float tw = bw * 0.2f, th = 22, ty = by - th - 2;
	struct { float x; const char* n; int st; } t[4] = {
		{ RX(0.04f), "L2", 0 }, { RX(0.04f) + tw + 6, "L1", 2 }, { RX(0.96f) - 2 * tw - 6, "R1", 1 }, { RX(0.96f) - tw, "R2", 0 },
	};
	for (int i = 0; i < 4; i++) {
		const float* c = t[i].st == 1 ? BLUE : t[i].st == 2 ? GREEN : NULL;
		lgfx_draw_fill(t[i].x, ty, tw, th, 7, c ? c[0] : 1, c ? c[1] : 1, c ? c[2] : 1, c ? (t[i].st == 2 ? 0.75f : 1) : 0.14f);
		text(LFONT_SMALL, t[i].n, t[i].x + tw * 0.5f, ty, t[i].st ? 1 : 0.7f, 2);
	}
	// side buttons: volume on the left, power on the right
	key(bx - 9, RY(0.06f), 6, bh * 0.07f, NULL, 0);
	key(bx - 9, RY(0.16f), 6, bh * 0.07f, NULL, 0);
	key(bx + bw + 3, RY(0.19f), 6, bh * 0.06f, NULL, 0);
	// the body and screen
	lgfx_draw_fill(bx, by, bw, bh, 22, 1, 1, 1, 0.05f);
	lgfx_draw_ring(bx, by, bw, bh, 22, 2, 1, 1, 1, 0.18f);
	lgfx_draw_fill(RX(0.03f), RY(0.02f), bw * 0.94f, bh * 0.48f, 8, 0, 0, 0, 0.40f);
	text(LFONT_BODY, "R1  \xC2\xB7  A", bx + bw * 0.5f, RY(0.24f) - 14, 0.9f, 2);
	// F1, F2
	float fw = bw * 0.11f, fh = 10;
	key(RX(0.424f) - fw * 0.5f, RY(0.579f) - fh * 0.5f, fw, fh, NULL, 0);
	key(RX(0.572f) - fw * 0.5f, RY(0.579f) - fh * 0.5f, fw, fh, NULL, 0);
	text(LFONT_SMALL, "F1", RX(0.424f), RY(0.579f) + 8, 0.45f, 2);
	text(LFONT_SMALL, "F2", RX(0.572f), RY(0.579f) + 8, 0.45f, 2);
	// d-pad as one cross, each arm lighting on its own
	float cx = RX(0.225f), cy = RY(0.658f), k = bw * 0.09f;
	lgfx_draw_fill(cx - k * 0.5f, cy - k * 1.5f, k, k * 3, 5, 1, 1, 1, 0.14f);
	lgfx_draw_fill(cx - k * 1.5f, cy - k * 0.5f, k * 3, k, 5, 1, 1, 1, 0.14f);
	lgfx_draw_fill(cx - k * 0.5f, cy - k * 1.5f, k, k, 5, GREEN[0], GREEN[1], GREEN[2], 0.75f);
	lgfx_draw_fill(cx - k * 0.5f, cy + k * 0.5f, k, k, 5, GREEN[0], GREEN[1], GREEN[2], 0.75f);
	lgfx_draw_fill(cx + k * 0.5f, cy - k * 0.5f, k, k, 5, GREEN[0], GREEN[1], GREEN[2], 0.75f);
	// face buttons
	float fx = RX(0.779f), fy = RY(0.658f), sp = bw * 0.105f, d = bw * 0.108f;
	key(fx - d * 0.5f, fy - sp - d * 0.5f, d, d, "X", 0);
	key(fx - sp - d * 0.5f, fy - d * 0.5f, d, d, "Y", 0);
	key(fx + sp - d * 0.5f, fy - d * 0.5f, d, d, "A", 1);
	key(fx - d * 0.5f, fy + sp - d * 0.5f, d, d, "B", 2);
	// MENU, SELECT, START
	float rd = bw * 0.075f, ry = RY(0.905f);
	const char* bn[3] = { "MENU", "SELECT", "START" };
	float bxs[3] = { 0.361f, 0.499f, 0.637f };
	(void)bn;
	for (int i = 0; i < 3; i++) {
		float c = RX(bxs[i]);
		key(c - rd * 0.5f, ry - rd * 0.5f, rd, rd, NULL, 0);
		// the symbol printed on each button: three dots, a square, a play mark
		if (i == 0)
			for (int j = 0; j < 3; j++) {
				float a = j * 2.094f - 1.571f, dx = cosf(a) * 4.5f, dy = sinf(a) * 4.5f;
				lgfx_draw_fill(c + dx - 2.5f, ry + dy - 2.5f, 5, 5, 2.5f, 1, 1, 1, 0.7f);
			}
		else if (i == 1) lgfx_draw_fill(c - 5, ry - 4, 10, 8, 1.5f, 1, 1, 1, 0.7f);
		else text(LFONT_SMALL, "\xE2\x96\xB6", c + 1, ry - 11, 0.7f, 2);
	}
	#undef RX
	#undef RY
	text(LFONT_SMALL, "Hold SELECT and START to leave", 40, H - 40, 0.6f, 0);
}

int main(int argc, char* argv[]) {
	res = argc > 1 ? argv[1] : "../../workspace/all/launcher/res";
	const char* prefix = argc > 2 ? argv[2] : "/tmp/apps-mock";
	char path[512];
	InitSettings();
	GFX_init(MODE_MAIN);
	lgfx_init(W, H);
	snprintf(path, sizeof(path), "%s/wallpapers/bloom-blur.jpg", res);
	backdrop = ltex_load(path, 0);
	make_battery();
	struct { void (*draw)(char); char arg; const char* name; } shots[] = {
		{ battery, 'A', "battery-A" }, { battery, 'B', "battery-B" },
	};
	for (int i = 0; i < 2; i++) {
		shots[i].draw(shots[i].arg);
		snprintf(path, sizeof(path), "%s-%s.bmp", prefix, shots[i].name);
		save(path);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	}
	activity_a();
	snprintf(path, sizeof(path), "%s-activity-A.bmp", prefix);
	save(path);
	SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	activity_b();
	snprintf(path, sizeof(path), "%s-activity-B.bmp", prefix);
	save(path);
	SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	activity_c();
	snprintf(path, sizeof(path), "%s-activity-C.bmp", prefix);
	save(path);
	SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	activity_d();
	snprintf(path, sizeof(path), "%s-activity-D.bmp", prefix);
	save(path);
	const char* names[7] = { "date-time", "date-time-auto", "lights", "test-buttons-A", "test-buttons-B", "test-buttons-hybrid", "test-buttons-real" };
	for (int i = 0; i < 7; i++) {
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
		if (i < 2) date_time(i);
		else if (i == 2) lights();
		else if (i < 5) test_buttons(i == 3 ? 'A' : 'B');
		else if (i == 5) test_buttons_hybrid();
		else test_buttons_real();
		snprintf(path, sizeof(path), "%s-%s.bmp", prefix, names[i]);
		save(path);
	}
	return 0;
}
