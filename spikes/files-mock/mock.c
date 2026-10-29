// Mockups of Bramley's Files app: a folder, the actions sheet, and the confirmation before changing
// saves. Drawn with the launcher's GL kit in the Settings layout. Made-up files. Not shipped.
//   DYLD_LIBRARY_PATH=/var/tmp/nextui/lib SDL_RENDER_VSYNC=0 ./build/mock <res-dir> <out-prefix>
#include <msettings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "lgfx.h"

#define W 1024
#define H 768

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
	lgfx_draw_tex(t, align == 1 ? x - t->w : align == 2 ? x - t->w * 0.5f : x, y, t->w, t->h, 0, alpha);
	ltex_free(t);
	return w;
}

// a simple glyph for each place and file kind, drawn, no icon set
static void glyph(char kind, float x, float y, float s, float a, int dark) {
	float c = dark ? 0.15f : 1;
	switch (kind) {
	case 'f': // folder
		lgfx_draw_fill(x + s * .08f, y + s * .22f, s * .4f, s * .16f, s * .05f, BRAMLEY_ACCENT, a);
		lgfx_draw_fill(x + s * .08f, y + s * .3f, s * .84f, s * .52f, s * .08f, BRAMLEY_ACCENT, a);
		break;
	case 'g': // game: a cartridge
		lgfx_draw_fill(x + s * .2f, y + s * .12f, s * .6f, s * .76f, s * .08f, c, c, c, a * .85f);
		lgfx_draw_fill(x + s * .3f, y + s * .22f, s * .4f, s * .3f, s * .04f, dark ? 1 : 0.1f, dark ? 1 : 0.1f, dark ? 1 : 0.12f, a);
		break;
	case 's': // save: a disk
		lgfx_draw_fill(x + s * .15f, y + s * .15f, s * .7f, s * .7f, s * .1f, c, c, c, a * .85f);
		lgfx_draw_fill(x + s * .3f, y + s * .15f, s * .4f, s * .22f, s * .02f, dark ? 1 : 0.1f, dark ? 1 : 0.1f, dark ? 1 : 0.12f, a);
		break;
	case 'm': // music: a note
		lgfx_draw_fill(x + s * .5f, y + s * .15f, s * .1f, s * .55f, 0, c, c, c, a);
		lgfx_draw_fill(x + s * .3f, y + s * .6f, s * .3f, s * .26f, s * .13f, c, c, c, a);
		break;
	case 'i': // image
		lgfx_draw_fill(x + s * .12f, y + s * .2f, s * .76f, s * .6f, s * .08f, c, c, c, a * .85f);
		lgfx_draw_fill(x + s * .2f, y + s * .5f, s * .6f, s * .22f, s * .05f, BRAMLEY_ACCENT, a);
		break;
	case 'c': // card
		lgfx_draw_fill(x + s * .2f, y + s * .12f, s * .6f, s * .76f, s * .06f, c, c, c, a * .85f);
		break;
	case 't': // trash
		lgfx_draw_fill(x + s * .25f, y + s * .3f, s * .5f, s * .56f, s * .06f, c, c, c, a * .85f);
		lgfx_draw_fill(x + s * .18f, y + s * .18f, s * .64f, s * .08f, s * .04f, c, c, c, a * .85f);
		break;
	}
}

// text in the alert red, for destructive actions
static void red_text(const char* str, float cx, float y) {
	TTF_Font* f = TTF_OpenFont("../../workspace/all/launcher/res/Inter-Regular.ttf", 23);
	SDL_Surface* sf = TTF_RenderUTF8_Blended(f, str, (SDL_Color){ 255, 105, 97, 255 });
	LTex* t = ltex_from_surface(sf, 0);
	lgfx_draw_tex(t, cx - t->w * 0.5f, y, t->w, t->h, 0, 1);
	ltex_free(t);
	SDL_FreeSurface(sf);
	TTF_CloseFont(f);
}

static void background(void) {
	lgfx_begin_frame();
	lgfx_draw_tex(backdrop, 0, 0, W, H, 0, 1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.30f);
	text(LFONT_LABEL, "Files", 48, 30, 0.55f, 0);
	text(LFONT_LABEL, "82%", W - 48, 30, 0.8f, 1);
	text(LFONT_LABEL, "10:24", W - 48 - 58, 30, 0.8f, 1);
}

static void sidebar(int focus, int open) {
	static const char* names[] = { "Games", "Saves", "Music", "Screenshots", "Whole card", "Trash" };
	static const char kinds[] = { 'g', 's', 'm', 'i', 'c', 't' };
	float x = 40, w = 330, rh = 46, gap = 5, y = 84;
	for (int i = 0; i < 6; i++) {
		if (i == 4) y += 12;
		int f = i == focus;
		float fx = x, fw = w, fy = y, fh = rh;
		if (f && !open) {
			fx -= 8, fw += 16, fy -= 3, fh += 6;
			lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
			lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, 0.96f);
		} else lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, f ? 0.22f : 0.07f);
		glyph(kinds[i], fx + 14, fy + (fh - 32) * 0.5f, 32, open && !f ? 0.5f : 1, f && !open);
		if (f && !open) lgfx_set_dim(0.1f);
		text(LFONT_BODY, names[i], fx + 60, fy + (fh - 30) * 0.5f, open ? (f ? 0.95f : 0.45f) : 1, 0);
		lgfx_set_dim(1);
		y += rh + gap;
	}
	// space on the card, under the places
	text(LFONT_SMALL, "SD card", 48, 470, 0.55f, 0);
	text(LFONT_SMALL, "38.2 GB free of 59.4 GB", 362, 470, 0.55f, 1);
	lgfx_draw_fill(48, 500, 314, 6, 3, 1, 1, 1, 0.15f);
	lgfx_draw_fill(48, 500, 314 * 0.36f, 6, 3, BRAMLEY_ACCENT, 1);
}

typedef struct { char kind; const char *name, *size, *when; int selected; } Row;

static void file_row(const Row* r, float x, float y, float w, float h, int focused, int selecting) {
	if (focused) {
		x -= 6, w += 12, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.96f);
	} else lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f);
	float tx = x + 16;
	if (selecting) {
		float c = 22, cy = y + (h - c) * 0.5f;
		if (r->selected) lgfx_draw_fill(tx, cy, c, c, c * 0.5f, BRAMLEY_ACCENT, 1);
		else lgfx_draw_ring(tx, cy, c, c, c * 0.5f, 2, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, 0.4f);
		tx += 34;
	}
	glyph(r->kind, tx, y + (h - 34) * 0.5f, 34, 1, focused);
	if (focused) lgfx_set_dim(0.1f);
	text(LFONT_BODY, r->name, tx + 46, y + 4, 1, 0);
	text(LFONT_SMALL, r->when, tx + 46, y + 32, focused ? 0.7f : 0.5f, 0);
	text(LFONT_BODY, r->size, x + w - 20, y + (h - 30) * 0.5f, focused ? 1 : 0.55f, 1);
	lgfx_set_dim(1);
}

static Row rows[] = {
	{ 'f', ".media", "Folder", "Box art  \xC2\xB7  32 items", 0 },
	{ 'g', "Moss Kingdom.gba", "4.0 MB", "Played yesterday", 1 },
	{ 'g', "Pixel Quest.gba", "8.0 MB", "Played today", 1 },
	{ 'g', "Star Courier.gba", "8.0 MB", "Added 3 Oct", 0 },
	{ 'g', "Tiny Tactics.gba", "16.0 MB", "Added 12 Sep", 0 },
	{ 'g', "Turbo Lane.gba", "8.0 MB", "Added 12 Sep", 0 },
};

static void pane(const char* back, const char* title, int focus, int selecting) {
	float px = 410, pw = W - 450, py = 84;
	lgfx_draw_fill(px, py, pw, H - 70 - py, 22, 0, 0, 0, 0.18f);
	char b[64];
	snprintf(b, sizeof(b), "\xE2\x80\xB9  %s", back);
	text(LFONT_BODY, b, px + 28, py + 18, 0.5f, 0);
	text(LFONT_TITLE, title, px + 28, py + 52, 1, 0);
	float y = py + 120;
	for (int i = 0; i < 6; i++, y += 62) file_row(&rows[i], px + 20, y, pw - 40, 56, i == focus, selecting);
	text(LFONT_SMALL, selecting ? "2 selected  \xC2\xB7  12.0 MB" : "32 items  \xC2\xB7  412 MB", px + 28, y + 8, 0.5f, 0);
}

static void list_screen(void) {
	background();
	sidebar(0, 1);
	pane("Games", "Game Boy Advance", 2, 0);
	text(LFONT_SMALL, "A  Open        Y  Actions        X  Select        B  Back", 40, H - 40, 0.6f, 0);
}

// Y opens an action sheet over the folder, for the focused file or the selection
static void actions_screen(void) {
	background();
	sidebar(0, 1);
	pane("Games", "Game Boy Advance", 2, 1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.45f);
	float w = 420, x = (W - w) * 0.5f, y = 170;
	const char* items[] = { "Copy", "Move", "Rename", "New Folder", "Move to Trash" };
	float h = 96 + 5 * 58 + 20;
	lgfx_draw_shadow(x, y, w, h, 14, 0.5f);
	lgfx_draw_fill(x, y, w, h, 24, 0.13f, 0.13f, 0.16f, 0.97f);
	text(LFONT_BODY, "2 items  \xC2\xB7  12.0 MB", x + w * 0.5f, y + 26, 0.6f, 2);
	float iy = y + 76;
	for (int i = 0; i < 5; i++, iy += 58) {
		int f = i == 1;
		if (f) {
			lgfx_draw_fill(x + 14, iy, w - 28, 50, 12, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
		}
		if (i == 4) { // destructive: in the alert red, as Apple marks them
			LTex* t = ltex_text(LFONT_BODY, items[i], 0);
			red_text(items[i], x + 34 + t->w * 0.5f, iy + 10);
			ltex_free(t);
		} else text(LFONT_BODY, items[i], x + 34, iy + 10, 1, 0);
		lgfx_set_dim(1);
	}
	text(LFONT_SMALL, "A  Choose        B  Cancel", 40, H - 40, 0.6f, 0);
}

// Changing anything in Saves asks first, and says what could be lost
static void confirm_screen(void) {
	background();
	sidebar(1, 1);
	float px = 410, pw = W - 450, py = 84;
	lgfx_draw_fill(px, py, pw, H - 70 - py, 22, 0, 0, 0, 0.18f);
	text(LFONT_TITLE, "Game Boy Advance", px + 28, py + 52, 1, 0);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.45f);
	float w = 560, h = 300, x = (W - w) * 0.5f, y = (H - h) * 0.5f - 20;
	lgfx_draw_shadow(x, y, w, h, 14, 0.5f);
	lgfx_draw_fill(x, y, w, h, 26, 0.13f, 0.13f, 0.16f, 0.97f);
	text(LFONT_TITLE, "Move this save to Trash?", x + w * 0.5f, y + 34, 1, 2);
	LTex* d = ltex_text_wrapped(LFONT_BODY, "Pixel Quest.gba.sav holds your progress in Pixel Quest. You can bring it back from Trash until you empty it.", (int)w - 80, 3, 1);
	lgfx_draw_tex(d, x + (w - d->w) * 0.5f, y + 92, d->w, d->h, 0, 0.7f);
	ltex_free(d);
	float bw = 220, bh = 52, by = y + h - bh - 28;
	// Cancel is focused first: the safe choice is the default
	lgfx_draw_fill(x + w * 0.5f - bw - 10, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.96f);
	lgfx_set_dim(0.1f);
	text(LFONT_BODY, "Cancel", x + w * 0.5f - 10 - bw * 0.5f, by + 11, 1, 2);
	lgfx_set_dim(1);
	lgfx_draw_fill(x + w * 0.5f + 10, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.14f);
	red_text("Move to Trash", x + w * 0.5f + 10 + bw * 0.5f, by + 11);
	text(LFONT_SMALL, "\xE2\x80\xB9 \xE2\x80\xBA  Choose        A  Confirm        B  Cancel", 40, H - 40, 0.6f, 0);
}

// Icon styles for the file rows, side by side: A drawn glyphs, B the game's box art, C colored
// tiles like the Settings icons, D outline glyphs, E an extension badge.
static LTex* art[3];

static void tile_icon(char kind, float x, float y, float s) {
	float r = 0.56f, g = 0.56f, b = 0.58f;
	if (kind == 'f') r = 0.290f, g = 0.698f, b = 0.471f;
	else if (kind == 'g') r = 0.36f, g = 0.42f, b = 0.95f;
	else if (kind == 's') r = 1.0f, g = 0.62f, b = 0.04f;
	else if (kind == 'i') r = 0.85f, g = 0.45f, b = 0.65f;
	const float top[4] = { r * 1.1f > 1 ? 1 : r * 1.1f, g * 1.1f > 1 ? 1 : g * 1.1f, b * 1.1f > 1 ? 1 : b * 1.1f, 1 }, bot[4] = { r * 0.75f, g * 0.75f, b * 0.75f, 1 };
	lgfx_draw_gradient(x, y, s, s, s * 0.24f, top, bot);
	glyph(kind == 'f' ? 'c' : kind, x + s * 0.18f, y + s * 0.18f, s * 0.64f, 1, 0);
}

static void outline_icon(char kind, float x, float y, float s, int dark) {
	float c = dark ? 0.15f : 1, w = 2.2f;
	if (kind == 'f') {
		lgfx_draw_ring(x + s * .08f, y + s * .28f, s * .84f, s * .56f, s * .1f, w, c, c, c, 0.9f);
		lgfx_draw_ring(x + s * .08f, y + s * .18f, s * .36f, s * .2f, s * .06f, w, c, c, c, 0.9f);
	} else if (kind == 'g') {
		lgfx_draw_ring(x + s * .2f, y + s * .1f, s * .6f, s * .8f, s * .1f, w, c, c, c, 0.9f);
		lgfx_draw_ring(x + s * .32f, y + s * .22f, s * .36f, s * .28f, s * .04f, w, c, c, c, 0.9f);
	} else {
		lgfx_draw_ring(x + s * .15f, y + s * .15f, s * .7f, s * .7f, s * .12f, w, c, c, c, 0.9f);
	}
}

static void badge_icon(const char* name, char kind, float x, float y, float s, int dark) {
	const char* dot = strrchr(name, '.');
	char ext[8] = "DIR";
	if (kind != 'f' && dot) {
		int n = 0;
		for (const char* p = dot + 1; *p && n < 4; p++) ext[n++] = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
		ext[n] = 0;
	}
	lgfx_draw_fill(x, y + s * 0.18f, s * 1.3f, s * 0.64f, s * 0.14f, kind == 'f' ? 0.290f : (dark ? 0 : 1), kind == 'f' ? 0.698f : (dark ? 0 : 1),
	               kind == 'f' ? 0.471f : (dark ? 0 : 1), kind == 'f' ? 1 : 0.14f);
	if (dark && kind != 'f') lgfx_set_dim(0.1f);
	text(LFONT_SMALL, ext, x + s * 0.65f, y + s * 0.5f - 11, kind == 'f' ? 1 : 0.85f, 2);
	lgfx_set_dim(1);
}

static void styled_row(char style, const Row* r, int idx, float x, float y, float w, float h, int focused) {
	if (focused) {
		x -= 6, w += 12, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.96f);
	} else lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f);
	float s = 38, ix = x + 14, iy = y + (h - s) * 0.5f, tx = ix + s + 14;
	switch (style) {
	case 'A': glyph(r->kind, ix, iy, s, 1, focused); break;
	case 'B':
		if (r->kind == 'g' && art[idx % 3]) lgfx_draw_tex(art[idx % 3], ix - 2, y + 5, s * 0.75f + 4, h - 10, 4, 1), tx = ix + s * 0.75f + 16;
		else glyph(r->kind, ix, iy, s, 1, focused);
		break;
	case 'C': tile_icon(r->kind, ix, iy, s); break;
	case 'D': outline_icon(r->kind, ix, iy, s, focused); break;
	case 'E': badge_icon(r->name, r->kind, ix, iy, s, focused), tx = ix + s * 1.3f + 14; break;
	}
	if (focused) lgfx_set_dim(0.1f);
	text(LFONT_BODY, r->name, tx, y + 4, 1, 0);
	text(LFONT_SMALL, r->when, tx, y + 32, focused ? 0.7f : 0.5f, 0);
	text(LFONT_BODY, r->size, x + w - 20, y + (h - 30) * 0.5f, focused ? 1 : 0.55f, 1);
	lgfx_set_dim(1);
}

static void icon_board(void) {
	lgfx_begin_frame();
	lgfx_draw_tex(backdrop, 0, 0, W, H, 0, 1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.45f);
	static const char* names[] = { "A. Drawn glyphs", "B. Box art", "C. Colored tiles", "D. Outline", "E. Extension badge" };
	Row sample[4] = { rows[0], rows[1], rows[2], { 's', "Pixel Quest.gba.sav", "128 KB", "Saved today", 0 } };
	for (int st = 0; st < 5; st++) {
		float x = st < 3 ? 20 + st * 335 : 190 + (st - 3) * 335, y = st < 3 ? 20 : 400, w = 320;
		text(LFONT_LABEL, names[st], x + 4, y, 0.85f, 0);
		for (int i = 0; i < 4; i++) styled_row('A' + st, &sample[i], i, x, y + 34 + i * 82, w, 72, i == 2);
	}
}

int main(int argc, char* argv[]) {
	const char* res = argc > 1 ? argv[1] : "../../workspace/all/launcher/res";
	const char* prefix = argc > 2 ? argv[2] : "/tmp/files-mock";
	char path[512];
	InitSettings();
	GFX_init(MODE_MAIN);
	lgfx_init(W, H);
	snprintf(path, sizeof(path), "%s/wallpapers/bloom-blur.jpg", res);
	backdrop = ltex_load(path, 0);
	for (int i = 0; i < 3; i++) {
		snprintf(path, sizeof(path), "/tmp/bhl/ingame/game%d.png", i);
		art[i] = ltex_load(path, 1);
	}
	void (*screens[4])(void) = { list_screen, actions_screen, confirm_screen, icon_board };
	const char* names[4] = { "folder", "actions", "confirm", "icons" };
	for (int i = 0; i < 4; i++) {
		screens[i]();
		snprintf(path, sizeof(path), "%s-%s.bmp", prefix, names[i]);
		save(path);
		SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
	}
	return 0;
}
