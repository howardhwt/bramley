// Mockups of the last NextUI screens redrawn for Bramley: the volume and brightness HUD and toasts
// over a game, an in-game message, the achievements list and one achievement, and power off.
// Drawn with the launcher's GL kit over a made-up pixel-art frame. Not shipped.
//   DYLD_LIBRARY_PATH=/var/tmp/nextui/lib SDL_RENDER_VSYNC=0 ./build/mock <frames-dir> <out-prefix>
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

static LTex* game;

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
	lgfx_draw_tex(t, align == 1 ? x - t->w : align == 2 ? x - t->w * 0.5f : x, y, t->w, t->h, 0, alpha);
	ltex_free(t);
	return w;
}

static void draw_game(void) { lgfx_draw_tex(game, 0, 42, W, 683, 0, 1); }

static void blurred(float dim) {
	lgfx_begin_snapshot();
	draw_game();
	lgfx_end_snapshot();
	lgfx_begin_frame();
	lgfx_draw_snapshot(1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, dim);
}

// A frosted pill: dark glass with a hairline edge, as the launcher's quick menu.
static void glass(float x, float y, float w, float h, float r) {
	lgfx_draw_fill(x - 2, y + 3, w + 4, h + 4, r + 2, 0, 0, 0, 0.18f); // soft rounded shadow
	lgfx_draw_fill(x, y, w, h, r, 0.11f, 0.11f, 0.13f, 0.88f);
	lgfx_draw_ring(x, y, w, h, r, 1, 1, 1, 1, 0.10f);
}

// Speaker: a box and a cone, with waves for the level.
static void speaker(float x, float y, float s, int waves) {
	lgfx_draw_fill(x, y + s * 0.34f, s * 0.22f, s * 0.32f, 2, 1, 1, 1, 1);
	for (int i = 0; i < 6; i++) {
		float t = i / 5.0f, hh = s * (0.32f + 0.4f * t);
		lgfx_draw_fill(x + s * 0.2f + i * s * 0.04f, y + (s - hh) * 0.5f, s * 0.05f, hh, 1, 1, 1, 1, 1);
	}
	for (int i = 0; i < waves; i++)
		lgfx_draw_ring(x + s * 0.3f - i * s * 0.12f, y + s * 0.2f - i * s * 0.14f, s * (0.5f + i * 0.28f), s * (0.6f + i * 0.28f),
		               s * (0.3f + i * 0.14f), 2, 1, 1, 1, 0.9f);
}

static void sun(float cx, float cy, float s) {
	lgfx_draw_fill(cx - s * 0.2f, cy - s * 0.2f, s * 0.4f, s * 0.4f, s * 0.2f, 1, 1, 1, 1);
	for (int i = 0; i < 8; i++) {
		float a = i * 0.785f, r = s * 0.38f;
		lgfx_draw_fill(cx + cosf(a) * r - 2, cy + sinf(a) * r - 2, 4, 4, 2, 1, 1, 1, 1);
	}
}

// HUD A: a slim pill at the top centre, the icon then a level bar, like iOS 16's.
static void hud(int volume) {
	lgfx_begin_frame();
	draw_game();
	float w = 300, h = 52, x = (W - w) * 0.5f, y = 22;
	glass(x, y, w, h, h * 0.5f);
	if (volume) speaker(x + 18, y + 13, 26, 0);
	else sun(x + 32, y + 26, 26);
	float bx = x + 62, bw = w - 62 - 64, by = y + h * 0.5f - 4, level = volume ? 0.65f : 0.8f;
	lgfx_draw_fill(bx, by, bw, 8, 4, 1, 1, 1, 0.2f);
	lgfx_draw_fill(bx, by, bw * level, 8, 4, 1, 1, 1, 1);
	text(LFONT_LABEL, volume ? "65%" : "80%", x + w - 18, y + 14, 0.85f, 1);
}

// Toasts: bottom centre, one line, an icon dot on the left.
static void toast(void) {
	lgfx_begin_frame();
	draw_game();
	const char* msgs[2] = { "Saved to slot 3", "Achievement unlocked: First Steps" };
	float y = H - 88 - 52; // clear of the letterbox and the screen edge
	for (int i = 1; i >= 0; i--, y -= 62) {
		LTex* t = ltex_text(LFONT_BODY, msgs[i], 0);
		float w = t->w + 76, h = 52, x = (W - w) * 0.5f;
		glass(x, y, w, h, h * 0.5f);
		if (i == 0) lgfx_draw_fill(x + 20, y + 16, 20, 20, 10, 0.20f, 0.78f, 0.35f, 1);
		else lgfx_draw_fill(x + 20, y + 16, 20, 20, 5, 1.0f, 0.75f, 0.2f, 1);
		lgfx_draw_tex(t, x + 54, y + (h - t->h) * 0.5f, t->w, t->h, 0, 1);
		ltex_free(t);
	}
}

// Message: a card over the blurred game, title, text, and one button.
static void message(void) {
	blurred(0.45f);
	float w = 560, h = 260, x = (W - w) * 0.5f, y = (H - h) * 0.5f - 20;
	lgfx_draw_shadow(x, y, w, h, 14, 0.5f);
	lgfx_draw_fill(x, y, w, h, 26, 0.13f, 0.13f, 0.16f, 0.96f);
	text(LFONT_TITLE, "No shaders available", x + w * 0.5f, y + 36, 1, 2);
	LTex* d = ltex_text_wrapped(LFONT_BODY, "Add shader files to the Shaders folder on the card, then open this menu again.", (int)w - 80, 3, 1);
	lgfx_draw_tex(d, x + (w - d->w) * 0.5f, y + 96, d->w, d->h, 0, 0.7f);
	ltex_free(d);
	float bw = 200, bh = 52, bx = x + (w - bw) * 0.5f, by = y + h - bh - 28;
	lgfx_draw_fill(bx, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.96f);
	lgfx_set_dim(0.1f);
	text(LFONT_BODY, "OK", bx + bw * 0.5f, by + 11, 1, 2);
	lgfx_set_dim(1);
}

// Achievements: progress at the top, then rows with a badge, title, description and points.
static void badge(float x, float y, float s, int unlocked, float hue) {
	float r = 0.5f + 0.5f * cosf(hue), g = 0.5f + 0.5f * cosf(hue + 2.1f), b = 0.5f + 0.5f * cosf(hue + 4.2f);
	if (!unlocked) r = g = b = 0.35f;
	const float t[4] = { r, g, b, 1 }, bt[4] = { r * 0.5f, g * 0.5f, b * 0.5f, 1 };
	lgfx_draw_gradient(x, y, s, s, 10, t, bt);
	lgfx_draw_fill(x + s * 0.3f, y + s * 0.3f, s * 0.4f, s * 0.4f, s * 0.2f, 1, 1, 1, unlocked ? 0.9f : 0.35f);
}

static void achievements(void) {
	blurred(0.55f);
	text(LFONT_LABEL, "PIXEL QUEST", 72, 52, 0.55f, 0);
	text(LFONT_TITLE, "Achievements", 72, 76, 1, 0);
	// progress
	text(LFONT_BODY, "12 of 40 unlocked", W - 72, 66, 0.85f, 1);
	lgfx_draw_fill(W - 72 - 260, 104, 260, 8, 4, 1, 1, 1, 0.18f);
	lgfx_draw_fill(W - 72 - 260, 104, 260 * 0.3f, 8, 4, 0.20f, 0.78f, 0.35f, 1);
	struct { const char *t, *d; int pts, un; } rows[] = {
		{ "First Steps", "Finish the first level", 5, 1 },
		{ "Coin Hoarder", "Collect 500 coins in one run", 10, 1 },
		{ "Untouchable", "Beat World 2 without taking damage", 25, 0 },
		{ "Speed Runner", "Finish World 1 in under 4 minutes", 25, 0 },
		{ "Secret Door", "Find the hidden room in the castle", 10, 0 },
	};
	float x = 72, w = W - 144, y = 150, rh = 86, gap = 8;
	for (int i = 0; i < 5; i++, y += rh + gap) {
		int f = i == 2;
		float fx = x, fy = y, fw = w, fh = rh;
		if (f) {
			fx -= 8, fw += 16, fy -= 3, fh += 6;
			lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
			lgfx_draw_fill(fx, fy, fw, fh, 18, 1, 1, 1, 0.96f);
		} else lgfx_draw_fill(fx, fy, fw, fh, 18, 1, 1, 1, 0.07f);
		badge(fx + 14, fy + 12, fh - 24, rows[i].un, i * 1.3f);
		if (f) lgfx_set_dim(0.1f);
		text(LFONT_BODY, rows[i].t, fx + fh + 6, fy + 12, rows[i].un || f ? 1 : 0.75f, 0);
		text(LFONT_SMALL, rows[i].d, fx + fh + 6, fy + 48, f ? 0.7f : 0.55f, 0);
		char p[16];
		snprintf(p, sizeof(p), "%d pts", rows[i].pts);
		text(LFONT_BODY, p, fx + fw - 24, fy + (fh - 30) * 0.5f, f ? 1 : 0.6f, 1);
		if (rows[i].un && !f) lgfx_draw_fill(fx + fw - 130, fy + fh * 0.5f - 6, 12, 12, 6, 0.20f, 0.78f, 0.35f, 1);
		lgfx_set_dim(1);
	}
	text(LFONT_SMALL, "A  Details        Y  Show locked        X  Mute        B  Back", 40, H - 40, 0.6f, 0);
}

// One achievement: large badge, title, description, points, status.
static void achievement(void) {
	blurred(0.55f);
	float s = 200, bx = (W - s) * 0.5f, by = 110;
	lgfx_draw_shadow(bx, by, s, s, 14, 0.5f);
	badge(bx, by, s, 0, 2.6f);
	lgfx_draw_fill(W * 0.5f - 300, by + s + 14, 600, 230, 24, 0, 0, 0, 0.35f);
	text(LFONT_TITLE, "Untouchable", W * 0.5f, by + s + 34, 1, 2);
	text(LFONT_BODY, "Beat World 2 without taking damage", W * 0.5f, by + s + 84, 0.7f, 2);
	float cy = by + s + 140, pw = 180, gap = 14, px = (W - 3 * pw - 2 * gap) * 0.5f;
	const char *lab[3] = { "POINTS", "STATUS", "UNLOCKED BY" }, *val[3] = { "25", "Locked", "4.2%" };
	for (int i = 0; i < 3; i++) {
		float x = px + i * (pw + gap);
		lgfx_draw_fill(x, cy, pw, 72, 16, 1, 1, 1, 0.07f);
		text(LFONT_LABEL, lab[i], x + pw * 0.5f, cy + 12, 0.55f, 2);
		text(LFONT_BODY, val[i], x + pw * 0.5f, cy + 34, 0.95f, 2);
	}
	text(LFONT_SMALL, "\xE2\x80\xB9 \xE2\x80\xBA  Previous, next        X  Mute        B  Back", 40, H - 40, 0.6f, 0);
}

// Power off: black, the wordmark, and what is happening underneath.
static void power_off(void) {
	lgfx_begin_frame();
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 1);
	text(LFONT_HERO, "Bramley", W * 0.5f, H * 0.5f - 50, 1, 2);
	text(LFONT_BODY, "Game saved. Shutting down\xE2\x80\xA6", W * 0.5f, H * 0.5f + 30, 0.6f, 2);
}

int main(int argc, char* argv[]) {
	const char* dir = argc > 1 ? argv[1] : "/tmp/bhl/ingame";
	const char* prefix = argc > 2 ? argv[2] : "/tmp/system-mock";
	char path[512];
	InitSettings();
	GFX_init(MODE_MAIN);
	lgfx_init(W, H);
	snprintf(path, sizeof(path), "%s/game0-big.png", dir);
	game = ltex_load(path, 1);
	const char* names[] = { "hud-volume", "hud-brightness", "toast", "message", "achievements", "achievement", "power-off" };
	for (int i = 0; i < 7; i++) {
		if (i == 0) hud(1);
		else if (i == 1) hud(0);
		else if (i == 2) toast();
		else if (i == 3) message();
		else if (i == 4) achievements();
		else if (i == 5) achievement();
		else power_off();
		snprintf(path, sizeof(path), "%s-%s.bmp", prefix, names[i]);
		save(path);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	}
	return 0;
}
