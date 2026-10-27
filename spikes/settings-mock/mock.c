// Mockup of NextUI's System settings page in two styles, for choosing how to reskin Settings.
// Style 1 draws with the launcher's GL kit (lgfx); style 2 imitates a tidied version of NextUI's
// current flat drawing. The rows are NextUI's real System settings with example values.
//   DYLD_LIBRARY_PATH=/var/tmp/nextui/lib SDL_RENDER_VSYNC=0 ./build/mock out-prefix
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
static void style_tvos(void) {
	lgfx_begin_frame();
	const float top[4] = { 0.20f, 0.17f, 0.36f, 1 }, bottom[4] = { 0.10f, 0.12f, 0.22f, 1 };
	lgfx_draw_gradient(0, 0, W, H, 0, top, bottom);

	text(LFONT_LABEL, "Settings", 56, 30, 0.55f, 0);
	text(LFONT_LABEL, "12:47      100%", W - 56, 30, 0.8f, 1);

	// Section card
	float cx = 56, cy = 150, cs = 300;
	lgfx_draw_shadow(cx, cy, cs, cs, 14, 0.6f);
	const float ct[4] = { 0.55f, 0.58f, 0.66f, 1 }, cb[4] = { 0.30f, 0.32f, 0.40f, 1 };
	lgfx_draw_gradient(cx, cy, cs, cs, 40, ct, cb);
	// A simple drawn glyph: three slider lines, since no Apple symbols ship.
	for (int i = 0; i < 3; i++) {
		float ly = cy + 100 + i * 50;
		lgfx_draw_fill(cx + 70, ly, 160, 8, 4, 1, 1, 1, 0.85f);
		lgfx_draw_fill(cx + 90 + i * 45, ly - 12, 32, 32, 16, 1, 1, 1, 1);
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
			lgfx_set_dim(0.45f);
			text(LFONT_BODY, rows[i][1], x + w - 26, y + 14, 1, 1);
			lgfx_set_dim(1);
		} else {
			lgfx_draw_fill(x, y, w, h, 14, 1, 1, 1, 0.07f);
			text(LFONT_BODY, rows[i][0], x + 26, y + 13, 0.92f, 0);
			text(LFONT_BODY, rows[i][1], x + w - 26, y + 13, 0.5f, 1);
		}
	}
	text(LFONT_SMALL, "A  Change     < >  Adjust     B  Back", rx, H - 40, 0.55f, 0);
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
	return 0;
}
