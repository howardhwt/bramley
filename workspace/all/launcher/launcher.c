// Brick Hammer launcher: a tvOS-style home screen for NextUI.
// Milestone 1: Continue Playing, Consoles and Apps rows with spring focus, shadows and parallax.
// Launching goes through NextUI's /tmp/next contract, so minarch and MinUI.pak stay stock.
#include <math.h>
#include <msettings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "config.h"
#include "lgfx.h"
#include "ldata.h"

#define SCREEN_W 1024
#define SCREEN_H 768
#define MAX_ROWS 3
#define MARGIN_X 56.0f
#define CARD_GAP 28.0f
#define FOCUS_SCALE 1.12f

typedef struct {
	float x, v;
} Spring;

typedef struct {
	float card_w, card_h;
	float title_y, cards_y;
	Spring scroll;
	Spring* scale;
	LTex* title;
} RowView;

static Row rows[MAX_ROWS];
static RowView views[MAX_ROWS];
static int row_count;
static int focus_row, focus_col[MAX_ROWS];
static Spring parallax_x, parallax_y;
static LTex* wallpaper;
static LTex* caption;
static LTex* hint_open;
static LTex* hint_resume;
static LTex* toast;
static double toast_until;
static int caption_row = -1, caption_col = -1;

// Development aid: LAUNCHER_DEBUG_KEYS="R R D S" presses Right, Right, Down, then saves a
// screenshot (S) to $LAUNCHER_DEBUG_SHOT-<n>.bmp, one step every 0.6 s. Unused on the device.
static void debug_step(void) {
	static const char* keys;
	static Uint32 next;
	static int shot;
	static SDL_Keycode held;
	if (!keys) keys = getenv("LAUNCHER_DEBUG_KEYS");
	if (held && SDL_GetTicks() >= next - 500) {
		SDL_Event up = { 0 };
		up.type = SDL_KEYUP;
		up.key.keysym.sym = held;
		up.key.keysym.scancode = SDL_GetScancodeFromKey(held);
		SDL_PushEvent(&up);
		held = 0;
	}
	if (!keys || !*keys || SDL_GetTicks() < next) return;
	next = SDL_GetTicks() + 600;
	while (*keys == ' ') keys++;
	char k = *keys ? *keys++ : 0;
	SDL_Keycode code = k == 'R' ? SDLK_RIGHT : k == 'L' ? SDLK_LEFT : k == 'U' ? SDLK_UP : k == 'D' ? SDLK_DOWN : 0;
	if (code) {
		SDL_Event e = { 0 };
		e.type = SDL_KEYDOWN;
		e.key.keysym.sym = code;
		e.key.keysym.scancode = SDL_GetScancodeFromKey(code);
		SDL_PushEvent(&e);
		held = code;
	} else if (k == 'S') {
		int w, h;
		SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(), &w, &h);
		SDL_Surface* img = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
		Uint8* rgba = malloc(w * h * 4);
		glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
		for (int y = 0; y < h; y++) memcpy((Uint8*)img->pixels + y * img->pitch, rgba + (h - 1 - y) * w * 4, w * 4);
		char path[512];
		snprintf(path, sizeof(path), "%s-%d.bmp", getenv("LAUNCHER_DEBUG_SHOT") ? getenv("LAUNCHER_DEBUG_SHOT") : "/tmp/launcher", shot++);
		SDL_SaveBMP(img, path);
		SDL_FreeSurface(img);
		free(rgba);
	}
}

static int spring_step(Spring* s, float target, float dt) {
	const float k = 260.0f, c = 2.0f * sqrtf(k) * 0.72f;
	for (int i = 0; i < 4; i++) {
		float a = k * (target - s->x) - c * s->v;
		s->v += a * dt / 4;
		s->x += s->v * dt / 4;
	}
	if (fabsf(target - s->x) < 0.0005f && fabsf(s->v) < 0.0005f) {
		s->x = target;
		s->v = 0;
		return 0;
	}
	return 1;
}

static void hsv_to_rgb(float h, float s, float v, float* r, float* g, float* b) {
	float i = floorf(h * 6), f = h * 6 - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
	switch ((int)i % 6) {
	case 0: *r = v, *g = t, *b = p; break;
	case 1: *r = q, *g = v, *b = p; break;
	case 2: *r = p, *g = v, *b = t; break;
	case 3: *r = p, *g = q, *b = v; break;
	case 4: *r = t, *g = p, *b = v; break;
	default: *r = v, *g = p, *b = q; break;
	}
}

// bg.png on the card if present, otherwise a dark gradient made once.
static LTex* load_wallpaper(void) {
	char path[256];
	snprintf(path, sizeof(path), "%s/bg.png", SDCARD_PATH);
	if (exists(path)) {
		LTex* tex = ltex_load(path, 0);
		if (tex) return tex;
	}
	SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 512, 384, 32, SDL_PIXELFORMAT_ABGR8888);
	static const float blobs[3][5] = {
		{ 0.20f, 0.15f, 0.30f, 0.20f, 0.55f },
		{ 0.85f, 0.30f, 0.45f, 0.15f, 0.35f },
		{ 0.45f, 0.95f, 0.10f, 0.30f, 0.40f },
	};
	for (int y = 0; y < s->h; y++) {
		Uint32* row = (Uint32*)((Uint8*)s->pixels + y * s->pitch);
		for (int x = 0; x < s->w; x++) {
			float u = (float)x / s->w, v = (float)y / s->h, r = 0.04f, g = 0.04f, b = 0.07f;
			for (int i = 0; i < 3; i++) {
				float dx = u - blobs[i][0], dy = (v - blobs[i][1]) * 0.75f, k = expf(-(dx * dx + dy * dy) * 6.0f);
				r += blobs[i][2] * k;
				g += blobs[i][3] * k;
				b += blobs[i][4] * k;
			}
			int R = r > 1 ? 255 : (int)(r * 255), G = g > 1 ? 255 : (int)(g * 255), B = b > 1 ? 255 : (int)(b * 255);
			row[x] = 0xff000000u | (B << 16) | (G << 8) | R;
		}
	}
	LTex* tex = ltex_from_surface(s, 0);
	SDL_FreeSurface(s);
	return tex;
}

static void layout_rows(void) {
	float y = 24;
	for (int r = 0; r < row_count; r++) {
		RowView* view = &views[r];
		Card* first = &rows[r].cards[0];
		if (first->kind == CARD_RECENT) view->card_w = 240, view->card_h = 150;
		else if (first->kind == CARD_CONSOLE) view->card_w = 150, view->card_h = 150;
		else view->card_w = 120, view->card_h = 120;
		view->title_y = y;
		view->cards_y = y + 46;
		y = view->cards_y + view->card_h + 58;
		view->title = ltex_text(LFONT_TITLE, rows[r].title, 0);
		view->scale = calloc(rows[r].count, sizeof(Spring));
		for (int c = 0; c < rows[r].count; c++) view->scale[c].x = 1.0f;
	}
}

static float card_x(int r, int c) {
	return MARGIN_X + c * (views[r].card_w + CARD_GAP) - views[r].scroll.x;
}

static float scroll_target(int r) {
	float pitch = views[r].card_w + CARD_GAP;
	int visible = (int)((SCREEN_W - 2 * MARGIN_X + CARD_GAP) / pitch);
	int first = focus_col[r] - (visible - 2);
	if (first < 0) first = 0;
	float max_scroll = rows[r].count * pitch - CARD_GAP - (SCREEN_W - 2 * MARGIN_X);
	if (max_scroll < 0) max_scroll = 0;
	float target = first * pitch;
	return target > max_scroll ? max_scroll : target;
}

// Loads at most one image per frame, for cards on screen, so startup stays fast.
static int load_next_art(void) {
	for (int r = 0; r < row_count; r++) {
		for (int c = 0; c < rows[r].count; c++) {
			Card* card = &rows[r].cards[c];
			float x = card_x(r, c);
			if (x > SCREEN_W || x + views[r].card_w < 0) continue;
			if (!card->art_tried && card->art_path[0]) {
				card->art = ltex_load(card->art_path, card->art_is_pixel);
				card->art_tried = 1;
				return 1;
			}
			if (!card->label && !card->art) {
				const char* text = card->kind == CARD_CONSOLE ? card->tag : card->name;
				card->label = ltex_text(card->kind == CARD_CONSOLE ? LFONT_TITLE : LFONT_SMALL, text,
				                        (int)views[r].card_w - 20);
				return 1;
			}
		}
	}
	return 0;
}

static void draw_card(int r, int c) {
	RowView* view = &views[r];
	Card* card = &rows[r].cards[c];
	float s = view->scale[c].x, lift = (s - 1.0f) / (FOCUS_SCALE - 1.0f);
	float w = view->card_w * s, h = view->card_h * s;
	float cx = card_x(r, c) + view->card_w * 0.5f - parallax_x.x * 6;
	float cy = view->cards_y + view->card_h * 0.5f - parallax_y.x * 4 - lift * 6;
	float x = cx - w * 0.5f, y = cy - h * 0.5f, radius = 18 * s;
	if (x > SCREEN_W || x + w < 0) return;

	lgfx_draw_shadow(x, y, w, h, lift * 10, 0.55f + lift * 0.35f);
	if (card->art) {
		lgfx_draw_tex(card->art, x, y, w, h, radius, 1);
	} else {
		float red, green, blue;
		hsv_to_rgb(card->hue, 0.45f, 0.55f, &red, &green, &blue);
		lgfx_draw_fill(x, y, w, h, radius, red, green, blue, 1);
		if (card->label) {
			float lw = card->label->w * s, lh = card->label->h * s;
			lgfx_draw_tex(card->label, cx - lw * 0.5f, cy - lh * 0.5f, lw, lh, 0, 1);
		}
	}
}

static void update_caption(void) {
	if (caption_row == focus_row && caption_col == focus_col[focus_row]) return;
	caption_row = focus_row;
	caption_col = focus_col[focus_row];
	ltex_free(caption);
	caption = ltex_text(LFONT_BODY, rows[focus_row].cards[caption_col].name, SCREEN_W - 2 * (int)MARGIN_X);
}

static void show_toast(const char* text) {
	ltex_free(toast);
	toast = ltex_text(LFONT_BODY, text, 0);
	toast_until = SDL_GetTicks() / 1000.0 + 1.8;
}

static void draw_frame(void) {
	lgfx_begin_frame();
	lgfx_draw_wallpaper(wallpaper, parallax_x.x * 12, parallax_y.x * 8);

	for (int r = 0; r < row_count; r++) {
		RowView* view = &views[r];
		float alpha = r == focus_row ? 1.0f : 0.6f;
		if (view->title) lgfx_draw_tex(view->title, MARGIN_X - parallax_x.x * 3, view->title_y, view->title->w, view->title->h, 0, alpha);
		for (int c = 0; c < rows[r].count; c++)
			if (!(r == focus_row && c == focus_col[r])) draw_card(r, c);
		if (r == focus_row) draw_card(r, focus_col[r]); // focused card draws on top of its neighbours
	}

	if (caption) {
		RowView* view = &views[focus_row];
		float x = card_x(focus_row, focus_col[focus_row]) - parallax_x.x * 6;
		if (x + caption->w > SCREEN_W - MARGIN_X) x = SCREEN_W - MARGIN_X - caption->w;
		if (x < MARGIN_X) x = MARGIN_X;
		float y = view->cards_y + view->card_h * FOCUS_SCALE + 2;
		if (y + caption->h > SCREEN_H - 4) y = SCREEN_H - 4 - caption->h;
		lgfx_draw_tex(caption, x, y, caption->w, caption->h, 0, 1);
	}

	Card* card = &rows[focus_row].cards[focus_col[focus_row]];
	float hx = SCREEN_W - MARGIN_X;
	if (card->kind == CARD_RECENT && card->can_resume && hint_resume) {
		hx -= hint_resume->w;
		lgfx_draw_tex(hint_resume, hx, 22, hint_resume->w, hint_resume->h, 0, 0.75f);
		hx -= 24;
	}
	if (hint_open) lgfx_draw_tex(hint_open, hx - hint_open->w, 22, hint_open->w, hint_open->h, 0, 0.75f);

	if (toast && SDL_GetTicks() / 1000.0 < toast_until) {
		float w = toast->w + 48, h = toast->h + 24, x = (SCREEN_W - w) * 0.5f, y = SCREEN_H - h - 40;
		lgfx_draw_fill(x, y, w, h, h * 0.5f, 0.1f, 0.1f, 0.12f, 0.85f);
		lgfx_draw_tex(toast, x + 24, y + 12, toast->w, toast->h, 0, 1);
	}
	debug_step();
	lgfx_end_frame();
}

int main(int argc, char* argv[]) {
	if (ldata_auto_resume()) return 0;

	LOG_info("Brick Hammer launcher\n");
	InitSettings();
	GFX_init(MODE_MAIN);
	PAD_init();
	VIB_init();
	PWR_init();
	if (!HAS_POWER_BUTTON) PWR_disableSleep();
	system("gametimectl.elf stop_all");
	GFX_setVsync(VSYNC_STRICT);
	PWR_setCPUSpeed(CPU_SPEED_AUTO);
	PAD_reset();

	lgfx_init(SCREEN_W, SCREEN_H);
	wallpaper = load_wallpaper();
	hint_open = ltex_text(LFONT_SMALL, "A  Open", 0);
	hint_resume = ltex_text(LFONT_SMALL, "X  Resume save", 0);

	row_count = ldata_load_rows(rows, MAX_ROWS);
	layout_rows();
	if (exists(GAME_SWITCHER_PERSIST_PATH)) {
		// The in-game switcher brings the player to Continue Playing, which is row 0 when it exists.
		unlink(GAME_SWITCHER_PERSIST_PATH);
		focus_row = 0;
	}
	if (row_count == 0) show_toast("No games or Paks found");

	int quit = 0, dirty = 1, show_setting = 0;
	Uint32 last = SDL_GetTicks();
	while (!quit) {
		Uint32 now = SDL_GetTicks();
		float dt = (now - last) / 1000.0f;
		if (dt > 0.05f) dt = 0.05f;
		last = now;

		PAD_poll();
		PWR_update(&dirty, &show_setting, NULL, NULL);

		if (!show_setting) {
			if (PAD_justPressed(BTN_FN1)) quit = ldata_run_fn_action(0);
			else if (PAD_justPressed(BTN_FN2)) quit = ldata_run_fn_action(1);
			else if (PAD_justPressed(BTN_FN3)) quit = ldata_run_fn_action(2);
		}

		if (row_count > 0 && !quit) {
			int old_row = focus_row, old_col = focus_col[focus_row];
			if (PAD_justRepeated(BTN_RIGHT) && focus_col[focus_row] < rows[focus_row].count - 1) focus_col[focus_row]++;
			else if (PAD_justRepeated(BTN_LEFT) && focus_col[focus_row] > 0) focus_col[focus_row]--;
			else if (PAD_justRepeated(BTN_DOWN) && focus_row < row_count - 1) focus_row++;
			else if (PAD_justRepeated(BTN_UP) && focus_row > 0) focus_row--;
			if (focus_row != old_row || focus_col[focus_row] != old_col) dirty = 1;

			Card* card = &rows[focus_row].cards[focus_col[focus_row]];
			if (PAD_justPressed(BTN_A)) {
				if (card->kind == CARD_CONSOLE) show_toast("Game library arrives in Milestone 2");
				else {
					ldata_launch(card, 0);
					quit = 1;
				}
			} else if (PAD_justPressed(BTN_X) && card->kind == CARD_RECENT && card->can_resume) {
				ldata_launch(card, 1);
				quit = 1;
			}
		}
		if (quit) break;

		int animating = 0;
		if (row_count > 0) {
			for (int r = 0; r < row_count; r++) {
				animating |= spring_step(&views[r].scroll, scroll_target(r), dt);
				for (int c = 0; c < rows[r].count; c++)
					animating |= spring_step(&views[r].scale[c], r == focus_row && c == focus_col[r] ? FOCUS_SCALE : 1.0f, dt);
			}
			float px = rows[focus_row].count > 1 ? (float)focus_col[focus_row] / (rows[focus_row].count - 1) * 2 - 1 : 0;
			float py = row_count > 1 ? (float)focus_row / (row_count - 1) * 2 - 1 : 0;
			animating |= spring_step(&parallax_x, px, dt);
			animating |= spring_step(&parallax_y, py, dt);
			update_caption();
			animating |= load_next_art();
		}
		if (toast && SDL_GetTicks() / 1000.0 < toast_until + 0.1) animating = 1;

		// Draw only while something moves, so the GPU and battery rest on a still screen.
		if (animating || dirty || show_setting) {
			draw_frame();
			dirty = 0;
		} else {
			debug_step();
			SDL_Delay(16);
		}
	}

	ldata_free_rows(rows, row_count);
	for (int r = 0; r < row_count; r++) {
		ltex_free(views[r].title);
		free(views[r].scale);
	}
	ltex_free(wallpaper);
	ltex_free(caption);
	ltex_free(hint_open);
	ltex_free(hint_resume);
	ltex_free(toast);
	lgfx_quit();

	PWR_quit();
	PAD_quit();
	GFX_quit();
	QuitSettings();
	return 0;
}
