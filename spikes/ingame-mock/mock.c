// Mockups of a Bramley in-game menu in three layouts, drawn with the launcher's GL kit over a
// made-up pixel-art game frame (no real game art). Not shipped; for choosing a design.
//   DYLD_LIBRARY_PATH=/var/tmp/nextui/lib SDL_RENDER_VSYNC=0 ./build/mock <frames-dir> <out-prefix>
#include <msettings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "lgfx.h"

#define W 1024
#define H 768

static LTex *game, *slots[3];
static const char* actions[] = { "Continue", "Save", "Load", "Options", "Quit" };
#define ACTIONS 5
#define FOCUS 1 // Save
#define SLOT 2  // slot 3

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

static void text(int font, const char* s, float x, float y, float alpha, int align) {
	LTex* t = ltex_text(font, s, 0);
	if (!t) return;
	float tx = align == 1 ? x - t->w : align == 2 ? x - t->w * 0.5f : x;
	lgfx_draw_tex(t, tx, y, t->w, t->h, 0, alpha);
	ltex_free(t);
}

// The paused game, letterboxed as a GBA screen (3:2) on the 4:3 display.
static void draw_game(void) { lgfx_draw_tex(game, 0, 42, W, 683, 0, 1); }

static void blurred_game(float dim) {
	lgfx_begin_snapshot();
	draw_game();
	lgfx_end_snapshot();
	lgfx_begin_frame();
	lgfx_draw_snapshot(1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, dim);
}

static void status(void) {
	text(LFONT_LABEL, "12:47      100%", W - 48, 30, 0.8f, 1);
}

static void title_block(float x, float y) {
	text(LFONT_LABEL, "GAME BOY ADVANCE", x, y, 0.55f, 0);
	text(LFONT_TITLE, "Pixel Quest", x, y + 26, 1, 0);
}

static void dots(float cx, float y, int n, int sel) {
	float gap = 16, x = cx - (n - 1) * gap * 0.5f;
	for (int i = 0; i < n; i++) {
		float s = i == sel ? 9 : 7;
		lgfx_draw_fill(x + i * gap - s * 0.5f, y - s * 0.5f, s, s, s * 0.5f, 1, 1, 1, i == sel ? 0.95f : 0.35f);
	}
}

static void row(const char* name, float x, float y, float w, float h, int focused, const char* value) {
	if (focused) {
		x -= 6, w += 12, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.35f);
		lgfx_draw_fill(x, y, w, h, 16, 1, 1, 1, 0.96f);
		lgfx_set_dim(0.1f);
	} else
		lgfx_draw_fill(x, y, w, h, 16, 1, 1, 1, 0.08f);
	float ty = y + (h - 30) * 0.5f;
	text(LFONT_BODY, name, x + 24, ty, 1, 0);
	if (value) {
		lgfx_set_dim(focused ? 0.45f : 1);
		text(LFONT_BODY, value, x + w - 22, ty, focused ? 1 : 0.55f, 1);
	}
	lgfx_set_dim(1);
}

static void slot_card(float x, float y, float w, float h) {
	lgfx_draw_shadow(x, y, w, h, 12, 0.5f);
	lgfx_draw_tex(slots[SLOT], x, y, w, h, 18, 1);
}

// A: like tvOS Now Playing. The actions on the left, the slot's screenshot large on the right.
static void layout_a(void) {
	blurred_game(0.40f);
	status();
	title_block(72, 64);
	float y = 170;
	for (int i = 0; i < ACTIONS; i++, y += 70) row(actions[i], 72, y, 300, 58, i == FOCUS, NULL);
	float cw = 528, ch = 352, cx = 432, cy = 170;
	slot_card(cx, cy, cw, ch);
	text(LFONT_LABEL, "SLOT 3", cx, cy + ch + 24, 0.55f, 0);
	text(LFONT_BODY, "Saved today at 12:41", cx, cy + ch + 48, 0.9f, 0);
	dots(cx + cw - 70, cy + ch + 62, 8, SLOT);
	text(LFONT_SMALL, "A  Save to slot 3        \xE2\x80\xB9 \xE2\x80\xBA  Slot        B  Back to game", 72, H - 44, 0.6f, 0);
}

// B: a shelf along the bottom. The game stays visible above; the actions are tiles, and the
// eight slots sit in a strip above them while Save or Load is focused.
static void layout_b(void) {
	lgfx_begin_frame();
	draw_game();
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.35f);
	// frosted shelf: the game blurred, only in the lower part
	lgfx_begin_snapshot();
	draw_game();
	lgfx_end_snapshot();
	lgfx_begin_frame();
	draw_game();
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.30f);
	status();
	title_block(56, 40);
	float sy = 440;
	lgfx_draw_fill(0, sy, W, H - sy, 0, 0.06f, 0.06f, 0.10f, 0.78f);
	lgfx_draw_fill(0, sy, W, 1.5f, 0, 1, 1, 1, 0.18f);
	// slots strip
	float tw = 104, th = 69, gap = 14, x = (W - 8 * tw - 7 * gap) * 0.5f, y = sy + 26;
	for (int i = 0; i < 8; i++, x += tw + gap) {
		int f = i == SLOT;
		float fx = x, fy = y, fw = tw, fh = th;
		if (f) fx -= 8, fy -= 6, fw += 16, fh += 11;
		if (i < 3 || i == 5) {
			if (f) lgfx_draw_shadow(fx, fy, fw, fh, 8, 0.5f);
			lgfx_draw_tex(slots[i % 3], fx, fy, fw, fh, 10, i == SLOT ? 1 : 0.75f);
			if (f) lgfx_draw_ring(fx, fy, fw, fh, 10, 3, 1, 1, 1, 1);
		} else {
			lgfx_draw_fill(fx, fy, fw, fh, 10, 1, 1, 1, 0.06f);
			lgfx_draw_ring(fx, fy, fw, fh, 10, 1.5f, 1, 1, 1, 0.18f);
		}
		char n[4];
		snprintf(n, sizeof(n), "%d", i + 1);
		text(LFONT_SMALL, n, x + tw * 0.5f, y + th + 10, f ? 0.95f : 0.45f, 2);
	}
	// action tiles
	float bw = 168, bh = 72, bg = 16, bx = (W - 5 * bw - 4 * bg) * 0.5f, by = sy + 152;
	for (int i = 0; i < ACTIONS; i++, bx += bw + bg) {
		int f = i == FOCUS;
		float fx = bx, fy = by, fw = bw, fh = bh;
		if (f) {
			fx -= 6, fy -= 5, fw += 12, fh += 10;
			lgfx_draw_shadow(fx, fy, fw, fh, 8, 0.4f);
			lgfx_draw_fill(fx, fy, fw, fh, 18, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
		} else
			lgfx_draw_fill(fx, fy, fw, fh, 18, 1, 1, 1, 0.10f);
		text(LFONT_BODY, actions[i], fx + fw * 0.5f, fy + (fh - 30) * 0.5f, 1, 2);
		lgfx_set_dim(1);
	}
	text(LFONT_SMALL, "Slot 3  \xC2\xB7  Saved today at 12:41", W * 0.5f, sy + 260 - 10, 0.6f, 2);
}

// C: a panel down the right side, like the launcher's quick menu, over the blurred game.
static void layout_c(void) {
	blurred_game(0.25f);
	float pw = 440, px = W - pw - 24, py = 24, ph = H - 48;
	lgfx_draw_shadow(px, py, pw, ph, 16, 0.5f);
	lgfx_draw_fill(px, py, pw, ph, 28, 0.10f, 0.10f, 0.14f, 0.86f);
	title_block(px + 32, py + 30);
	float cw = pw - 64, ch = cw * 2 / 3, cx = px + 32, cy = py + 104;
	slot_card(cx, cy, cw, ch);
	text(LFONT_SMALL, "Slot 3  \xC2\xB7  Saved today at 12:41", cx, cy + ch + 14, 0.6f, 0);
	dots(cx + cw * 0.5f, cy + ch + 52, 8, SLOT);
	float y = cy + ch + 72;
	for (int i = 0; i < ACTIONS; i++, y += 56) row(actions[i], cx, y, cw, 48, i == FOCUS, NULL);
	text(LFONT_LABEL, "12:47   100%", 40, 36, 0.8f, 0);
}

int main(int argc, char* argv[]) {
	const char* dir = argc > 1 ? argv[1] : "/tmp/bhl/ingame";
	const char* prefix = argc > 2 ? argv[2] : "/tmp/bhl/ingame/mock";
	char path[512];
	InitSettings();
	GFX_init(MODE_MAIN);
	lgfx_init(W, H);
	snprintf(path, sizeof(path), "%s/game0-big.png", dir);
	game = ltex_load(path, 1);
	for (int i = 0; i < 3; i++) {
		snprintf(path, sizeof(path), "%s/game%d.png", dir, i);
		slots[i] = ltex_load(path, 1);
	}
	void (*layouts[3])(void) = { layout_a, layout_b, layout_c };
	for (int i = 0; i < 3; i++) {
		layouts[i]();
		snprintf(path, sizeof(path), "%s-%c.bmp", prefix, 'A' + i);
		save(path);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	}
	return 0;
}
