// Brick Hammer launcher: a tvOS-style home screen for NextUI.
// The top half is a Top Shelf showing the focused item large; the bottom half holds rows of
// wide icons (Continue Playing, Consoles, Apps). Launching goes through NextUI's /tmp/next
// contract, so minarch and MinUI.pak stay stock.
#include <math.h>
#include <msettings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

// Top Shelf
#define SHELF_ART_Y 74.0f

// Rows of icons
#define ROW_TITLE_H 38.0f
#define ROW_BOTTOM 36.0f
#define GRID_GAP_Y 26.0f
#define GRID_COLS 4
#define ICON_W 200.0f
#define ICON_H 120.0f
#define ICON_GAP 30.0f
#define ICON_RADIUS 16.0f
#define FOCUS_SCALE 1.12f
#define SHINE_SECONDS 0.8f

typedef struct {
	float x, v;
} Spring;

typedef struct {
	Spring scroll;
	Spring* scale;
	LTex* title;
} RowView;

// A large Top Shelf, one row in full view with the next one peeking in at the bottom edge, and
// Apps wrapping into lines of GRID_COLS like the tvOS app grid.
static const float rows_top = 500.0f, shelf_art_h = 340.0f, shelf_art_max_w = 500.0f;

static Row rows[MAX_ROWS];
static RowView views[MAX_ROWS];
static int row_count;
static int focus_row, focus_col[MAX_ROWS];
static Spring parallax_x, parallax_y, rows_scroll, shelf_fade;
static float shine_t = 1.0f;
static LTex* wallpaper;
static LTex* toast;
static double toast_until;

// Top Shelf texts, rebuilt when focus moves.
static struct {
	int row, col;
	LTex* kicker;
	LTex* title;
	LTex* detail;
	LTex* pill_a;
	LTex* pill_x;
	LTex* art_label; // shown in the art box when the item has no art
} shelf = { -1, -1 };

// Status, rebuilt when the minute or battery level changes.
static struct {
	int minute, battery;
	LTex* clock;
	LTex* percent;
} status = { -1, -1 };

// Development aid: LAUNCHER_DEBUG_KEYS="R R D S" presses Right, Right, Down, then saves a
// screenshot. Lower case presses buttons on the desktop key map: a = A, b = B, x = X, l = L1, r = R1,
// m = MENU.
// Upper case S saves a
// screenshot to $LAUNCHER_DEBUG_SHOT-<n>.bmp. One step every 0.6 s. Unused on the device.
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
	SDL_Keycode code = k == 'R' ? SDLK_RIGHT : k == 'L' ? SDLK_LEFT : k == 'U' ? SDLK_UP : k == 'D' ? SDLK_DOWN
	                 : k == 'a' ? SDLK_s : k == 'b' ? SDLK_a : k == 'x' ? SDLK_w : k == 'l' ? SDLK_TAB : k == 'r' ? SDLK_b : k == 'm' ? SDLK_SPACE : 0;
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

static Card* focused_card(void) {
	return &rows[focus_row].cards[focus_col[focus_row]];
}

static int row_is_grid(int r) {
	return rows[r].count > 0 && rows[r].cards[0].kind == CARD_TOOL;
}

static int row_lines(int r) {
	return row_is_grid(r) ? (rows[r].count + GRID_COLS - 1) / GRID_COLS : 1;
}

static float row_height(int r) {
	int lines = row_lines(r);
	return ROW_TITLE_H + lines * ICON_H + (lines - 1) * GRID_GAP_Y + ROW_BOTTOM;
}

// Top of row r before vertical scrolling.
static float row_top(int r) {
	float y = rows_top;
	for (int i = 0; i < r; i++) y += row_height(i);
	return y;
}

static float row_y(int r) {
	return row_top(r) - rows_scroll.x;
}

// Top-left of icon c in row r at rest (unscaled), after scrolling.
static void icon_pos(int r, int c, float* x, float* y) {
	if (row_is_grid(r)) {
		*x = MARGIN_X + (c % GRID_COLS) * (ICON_W + ICON_GAP);
		*y = row_y(r) + ROW_TITLE_H + (c / GRID_COLS) * (ICON_H + GRID_GAP_Y);
	} else {
		*x = MARGIN_X + c * (ICON_W + ICON_GAP) - views[r].scroll.x;
		*y = row_y(r) + ROW_TITLE_H;
	}
}

static float scroll_target(int r) {
	if (row_is_grid(r)) return 0;
	float pitch = ICON_W + ICON_GAP;
	int visible = (int)((SCREEN_W - 2 * MARGIN_X + ICON_GAP) / pitch);
	int first = focus_col[r] - (visible - 2);
	if (first < 0) first = 0;
	float max_scroll = rows[r].count * pitch - ICON_GAP - (SCREEN_W - 2 * MARGIN_X);
	if (max_scroll < 0) max_scroll = 0;
	float target = first * pitch;
	return target > max_scroll ? max_scroll : target;
}

// The focused row (or grid line) always takes the one full-view spot under the Top Shelf; rows
// above it scroll out of view and the next one peeks in at the bottom edge.
static float rows_scroll_target(void) {
	int c = focus_col[focus_row];
	float line_top = row_top(focus_row) + (row_is_grid(focus_row) ? (c / GRID_COLS) * (ICON_H + GRID_GAP_Y) : 0);
	float target = line_top - rows_top;
	return target < 0 ? 0 : target;
}

// Moves focus with the D-pad. Grid rows move by line; other rows move along the row.
static void move_focus(int dx, int dy) {
	int r = focus_row, c = focus_col[r], n = rows[r].count;
	if (row_is_grid(r)) {
		if (dx > 0 && c + 1 < n && (c + 1) % GRID_COLS) c++;
		else if (dx < 0 && c % GRID_COLS) c--;
		else if (dy > 0 && c + GRID_COLS < n) c += GRID_COLS;
		else if (dy > 0 && c / GRID_COLS < (n - 1) / GRID_COLS) c = n - 1;
		else if (dy < 0 && c >= GRID_COLS) c -= GRID_COLS;
		else if (dy != 0) {
			if (dy > 0 && r < row_count - 1) focus_row++;
			if (dy < 0 && r > 0) focus_row--;
			return;
		}
		focus_col[r] = c;
		return;
	}
	if (dx > 0 && c < n - 1) focus_col[r]++;
	else if (dx < 0 && c > 0) focus_col[r]--;
	else if (dy > 0 && r < row_count - 1) focus_row++;
	else if (dy < 0 && r > 0) focus_row--;
}

static void card_colors(const Card* card, float top[4], float bottom[4]) {
	hsv_to_rgb(card->hue, 0.42f, 0.78f, &top[0], &top[1], &top[2]);
	hsv_to_rgb(fmodf(card->hue + 0.04f, 1.0f), 0.58f, 0.48f, &bottom[0], &bottom[1], &bottom[2]);
	top[3] = bottom[3] = 1.0f;
}

static void layout_rows(void) {
	for (int r = 0; r < row_count; r++) {
		views[r].title = ltex_text(LFONT_LABEL, rows[r].title, 0);
		views[r].scale = calloc(rows[r].count, sizeof(Spring));
		for (int c = 0; c < rows[r].count; c++) views[r].scale[c].x = 1.0f;
	}
}

// Loads at most one image or label per frame, for icons on screen, so startup stays fast.
static int load_next_art(void) {
	for (int r = 0; r < row_count; r++) {
		for (int c = 0; c < rows[r].count; c++) {
			Card* card = &rows[r].cards[c];
			float x, y;
			icon_pos(r, c, &x, &y);
			if (x > SCREEN_W || x + ICON_W < 0 || y > SCREEN_H || y + ICON_H < 0) continue;
			if (!card->art_tried && card->art_path[0]) {
				card->art = ltex_load(card->art_path, card->art_is_pixel);
				card->art_tried = 1;
				return 1;
			}
			if (!card->label && !card->art) {
				int console = card->kind == CARD_CONSOLE;
				card->label = ltex_text(console ? LFONT_TITLE : LFONT_LABEL, console ? card->tag : card->name,
				                        (int)ICON_W - 24);
				return 1;
			}
		}
	}
	return 0;
}

static const char* console_name_for(const char* tag) {
	for (int r = 0; r < row_count; r++)
		for (int c = 0; c < rows[r].count; c++)
			if (rows[r].cards[c].kind == CARD_CONSOLE && exactMatch(rows[r].cards[c].tag, tag)) return rows[r].cards[c].name;
	return tag;
}

static void free_shelf(void) {
	ltex_free(shelf.kicker);
	ltex_free(shelf.title);
	ltex_free(shelf.detail);
	ltex_free(shelf.pill_a);
	ltex_free(shelf.pill_x);
	ltex_free(shelf.art_label);
	shelf.kicker = shelf.title = shelf.detail = shelf.pill_a = shelf.pill_x = shelf.art_label = NULL;
}

static int update_shelf(void) {
	if (shelf.row == focus_row && shelf.col == focus_col[focus_row]) return 0;
	shelf.row = focus_row;
	shelf.col = focus_col[focus_row];
	free_shelf();

	Card* card = focused_card();
	char kicker[128], detail[128];
	const char* pill_a = "Open";
	if (card->kind == CARD_RECENT) {
		snprintf(kicker, sizeof(kicker), "%s", console_name_for(card->tag));
		snprintf(detail, sizeof(detail), "%s", card->can_resume ? "Saved game available" : "Continue where you left off");
		pill_a = "Play";
	} else if (card->kind == CARD_CONSOLE) {
		snprintf(kicker, sizeof(kicker), "Console");
		snprintf(detail, sizeof(detail), "%d %s", card->game_count, card->game_count == 1 ? "game" : "games");
	} else {
		snprintf(kicker, sizeof(kicker), "App");
		snprintf(detail, sizeof(detail), "Tools Pak");
	}
	for (char* p = kicker; *p; p++) *p = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;

	// The text column starts right of the widest art box, so long titles wrap onto a second line.
	int text_w = (int)(SCREEN_W - MARGIN_X - (MARGIN_X + shelf_art_max_w + 40));
	shelf.kicker = ltex_text(LFONT_LABEL, kicker, text_w);
	shelf.title = ltex_text_wrapped(LFONT_HERO, card->name, text_w, 2, 0);
	shelf.detail = ltex_text(LFONT_BODY, detail, text_w);
	char text[64];
	snprintf(text, sizeof(text), "A   %s", pill_a);
	shelf.pill_a = ltex_text(LFONT_LABEL, text, 0);
	if (card->kind == CARD_RECENT && card->can_resume) shelf.pill_x = ltex_text(LFONT_LABEL, "X   Resume save", 0);
	shelf.art_label = ltex_text(card->kind == CARD_CONSOLE ? LFONT_HERO : LFONT_TITLE,
	                            card->kind == CARD_CONSOLE ? card->tag : card->name, (int)shelf_art_max_w - 40);
	shine_t = 0.0f;
	shelf_fade.x = 0.0f;
	shelf_fade.v = 0.0f;
	return 1;
}

static int update_status(void) {
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	int minute = tm.tm_hour * 60 + tm.tm_min, battery = PWR_getBattery();
	if (minute == status.minute && battery == status.battery) return 0;
	status.minute = minute;
	status.battery = battery;
	ltex_free(status.clock);
	ltex_free(status.percent);
	char text[16];
	snprintf(text, sizeof(text), "%d:%02d", tm.tm_hour, tm.tm_min);
	status.clock = ltex_text(LFONT_LABEL, text, 0);
	snprintf(text, sizeof(text), "%d%%", battery);
	status.percent = ltex_text(LFONT_SMALL, text, 0);
	return 1;
}

static void show_toast(const char* text) {
	ltex_free(toast);
	toast = ltex_text(LFONT_BODY, text, 0);
	toast_until = SDL_GetTicks() / 1000.0 + 1.8;
}

static void draw_icon(int r, int c, float row_alpha) {
	RowView* view = &views[r];
	Card* card = &rows[r].cards[c];
	int focused = r == focus_row && c == focus_col[r];
	float s = view->scale[c].x, lift = (s - 1.0f) / (FOCUS_SCALE - 1.0f);
	float w = ICON_W * s, h = ICON_H * s;
	float ix, iy;
	icon_pos(r, c, &ix, &iy);
	// Icons scrolling up under the Top Shelf fade out.
	if (iy < rows_top) row_alpha *= fmaxf(0.0f, 1.0f - (rows_top - iy) / 90.0f);
	if (row_alpha <= 0.01f) return;
	float cx = ix + ICON_W * 0.5f - parallax_x.x * 6;
	float cy = iy + ICON_H * 0.5f - parallax_y.x * 4 - lift * 6;
	float x = cx - w * 0.5f, y = cy - h * 0.5f, radius = ICON_RADIUS * s;
	if (x > SCREEN_W || x + w < 0) return;

	lgfx_draw_shadow(x, y, w, h, lift * 12, (0.45f + lift * 0.4f) * row_alpha);
	if (card->art) {
		lgfx_draw_tex(card->art, x, y, w, h, radius, row_alpha);
	} else {
		float top[4], bottom[4];
		card_colors(card, top, bottom);
		top[3] = bottom[3] = row_alpha;
		lgfx_draw_gradient(x, y, w, h, radius, top, bottom);
		if (card->label) {
			float lw = card->label->w * s, lh = card->label->h * s;
			lgfx_draw_tex(card->label, cx - lw * 0.5f, cy - lh * 0.5f, lw, lh, 0, row_alpha);
		}
	}
	if (focused && shine_t < 1.0f) lgfx_draw_shine(x, y, w, h, radius, shine_t);
}

static void draw_pill(LTex* text, float x, float y) {
	float w = text->w + 36, h = text->h + 16;
	lgfx_draw_fill(x, y, w, h, h * 0.5f, 1, 1, 1, 0.16f * shelf_fade.x);
	lgfx_draw_tex(text, x + 18, y + 8, text->w, text->h, 0, shelf_fade.x);
}

static void draw_shelf(void) {
	Card* card = focused_card();
	float a = shelf_fade.x;

	// Art box: landscape for screenshots and wide art, narrower for square or tall box art.
	float art_w = shelf_art_max_w;
	if (card->art) {
		float aspect = (float)card->art->w / card->art->h;
		art_w = fminf(fmaxf(shelf_art_h * aspect, 180), shelf_art_max_w);
	}
	float art_x = MARGIN_X, art_y = SHELF_ART_Y + (1.0f - a) * 10;
	lgfx_draw_shadow(art_x, art_y, art_w, shelf_art_h, 14, 0.7f * a);
	if (card->art) {
		lgfx_draw_tex(card->art, art_x, art_y, art_w, shelf_art_h, 22, a);
	} else {
		float top[4], bottom[4];
		card_colors(card, top, bottom);
		top[3] = bottom[3] = a;
		lgfx_draw_gradient(art_x, art_y, art_w, shelf_art_h, 22, top, bottom);
		LTex* label = shelf.art_label;
		if (label)
			lgfx_draw_tex(label, art_x + (art_w - label->w) * 0.5f, art_y + (shelf_art_h - label->h) * 0.5f, label->w,
			              label->h, 0, a);
	}

	float tx = art_x + art_w + 40, ty = SHELF_ART_Y + 26 + (1.0f - a) * 6;
	if (shelf.kicker) lgfx_draw_tex(shelf.kicker, tx, ty, shelf.kicker->w, shelf.kicker->h, 0, 0.6f * a);
	float detail_y = ty + 92;
	if (shelf.title) {
		lgfx_draw_tex(shelf.title, tx, ty + 30, shelf.title->w, shelf.title->h, 0, a);
		detail_y = ty + 30 + shelf.title->h + 6;
	}
	if (shelf.detail) lgfx_draw_tex(shelf.detail, tx, detail_y, shelf.detail->w, shelf.detail->h, 0, 0.72f * a);
	float px = tx, py = SHELF_ART_Y + shelf_art_h - 48;
	if (shelf.pill_a) {
		draw_pill(shelf.pill_a, px, py);
		px += shelf.pill_a->w + 36 + 14;
	}
	if (shelf.pill_x) draw_pill(shelf.pill_x, px, py);
}

static void draw_status(void) {
	float x = SCREEN_W - MARGIN_X;
	// Battery: outline, level and tip.
	float bw = 30, bh = 15, by = 26;
	x -= bw + 3;
	float level = status.battery < 0 ? 0 : status.battery > 100 ? 1 : status.battery / 100.0f;
	lgfx_draw_fill(x, by, bw, bh, 4, 1, 1, 1, 0.35f);
	lgfx_draw_fill(x + 2, by + 2, (bw - 4) * level, bh - 4, 3, 1, 1, 1, status.battery <= 15 ? 0.6f : 0.95f);
	lgfx_draw_fill(x + bw + 1, by + 4, 3, bh - 8, 1, 1, 1, 1, 0.35f);
	if (status.percent) {
		x -= status.percent->w + 8;
		lgfx_draw_tex(status.percent, x, by - 2, status.percent->w, status.percent->h, 0, 0.75f);
	}
	if (status.clock) {
		x -= status.clock->w + 20;
		lgfx_draw_tex(status.clock, x, by - 3, status.clock->w, status.clock->h, 0, 0.9f);
	}
}


///////////////////////////////////////
// Game library: a grid of covers for one console.

#define LIB_COLS 5
#define COVER_W 150.0f
#define COVER_H 200.0f
#define COVER_GAP 40.0f
#define LIB_TOP 132.0f
#define LIB_LINE (COVER_H + 64.0f)
#define LAST_FOCUS_PATH "/tmp/launcher_last"

static struct {
	int open;
	Card* console;
	Card* games;
	int count;
	int focus;
	Spring* scale;
	Spring scroll, fade;
	float scroll_goal;
	LTex* header;
	LTex* subtitle;
	LTex* caption;
	LTex* hints;
	int caption_for;
	LTex* letter;
	double letter_until;
} lib = { .caption_for = -1 };

static float lib_margin(void) {
	return (SCREEN_W - (LIB_COLS * COVER_W + (LIB_COLS - 1) * COVER_GAP)) * 0.5f;
}

static void cover_pos(int i, float* x, float* y) {
	*x = lib_margin() + (i % LIB_COLS) * (COVER_W + COVER_GAP);
	*y = LIB_TOP + (i / LIB_COLS) * LIB_LINE - lib.scroll.x;
}

static void close_library(void) {
	ldata_free_games(lib.games, lib.count);
	free(lib.scale);
	ltex_free(lib.header);
	ltex_free(lib.subtitle);
	ltex_free(lib.caption);
	ltex_free(lib.hints);
	ltex_free(lib.letter);
	memset(&lib, 0, sizeof(lib));
	lib.caption_for = -1;
	shine_t = 1.0f;
}

static void open_library(Card* console, int focus) {
	if (lib.open) close_library();
	lib.games = ldata_load_games(console, &lib.count);
	if (!lib.count) {
		ldata_free_games(lib.games, 0);
		lib.games = NULL;
		show_toast("No games in this folder");
		return;
	}
	lib.open = 1;
	lib.console = console;
	lib.focus = focus >= 0 && focus < lib.count ? focus : 0;
	lib.scale = calloc(lib.count, sizeof(Spring));
	for (int i = 0; i < lib.count; i++) lib.scale[i].x = 1.0f;
	lib.caption_for = -1;
	lib.header = ltex_text(LFONT_HERO, console->name, 640);
	char text[64];
	snprintf(text, sizeof(text), "%d %s", lib.count, lib.count == 1 ? "game" : "games");
	lib.subtitle = ltex_text(LFONT_BODY, text, 0);
	// Start with the focused line at the top of the grid.
	lib.scroll_goal = (lib.focus / LIB_COLS) * LIB_LINE;
	lib.scroll.x = lib.scroll_goal;
	lib.fade.x = 0.0f;
}

static char first_letter(const Card* game) {
	char c = game->name[0];
	if (c >= 'a' && c <= 'z') c -= 32;
	return (c >= 'A' && c <= 'Z') ? c : '#';
}

// R1 goes to the first game of the next letter, L1 to the start of this letter or the previous one.
static void jump_letter(int dir) {
	int i = lib.focus;
	char here = first_letter(&lib.games[i]);
	if (dir > 0) {
		while (i < lib.count - 1 && first_letter(&lib.games[i]) == here) i++;
	} else {
		while (i > 0 && first_letter(&lib.games[i - 1]) == here) i--;
		if (i == lib.focus && i > 0) {
			char prev = first_letter(&lib.games[i - 1]);
			i--;
			while (i > 0 && first_letter(&lib.games[i - 1]) == prev) i--;
		}
	}
	if (i == lib.focus) return;
	lib.focus = i;
	char text[4] = { first_letter(&lib.games[i]), 0 };
	ltex_free(lib.letter);
	lib.letter = ltex_text(LFONT_HERO, text, 0);
	lib.letter_until = SDL_GetTicks() / 1000.0 + 0.7;
}

static void lib_move(int dx, int dy) {
	int c = lib.focus, n = lib.count;
	if (dx > 0 && c + 1 < n && (c + 1) % LIB_COLS) c++;
	else if (dx < 0 && c % LIB_COLS) c--;
	else if (dy > 0 && c + LIB_COLS < n) c += LIB_COLS;
	else if (dy > 0 && c / LIB_COLS < (n - 1) / LIB_COLS) c = n - 1;
	else if (dy < 0 && c >= LIB_COLS) c -= LIB_COLS;
	lib.focus = c;
}

// Keeps the focused line between the header and the bottom edge, scrolling only when needed.
static float lib_scroll_target(void) {
	float line_top = LIB_TOP + (lib.focus / LIB_COLS) * LIB_LINE;
	if (line_top - lib.scroll_goal < LIB_TOP) lib.scroll_goal = line_top - LIB_TOP;
	if (line_top + COVER_H + 56 - lib.scroll_goal > SCREEN_H) lib.scroll_goal = line_top + COVER_H + 56 - SCREEN_H;
	if (lib.scroll_goal < 0) lib.scroll_goal = 0;
	return lib.scroll_goal;
}

static int lib_update_caption(void) {
	if (lib.caption_for == lib.focus) return 0;
	lib.caption_for = lib.focus;
	Card* game = &lib.games[lib.focus];
	ldata_check_resume(game);
	ltex_free(lib.caption);
	ltex_free(lib.hints);
	lib.caption = ltex_text(LFONT_LABEL, game->name, (int)(COVER_W + COVER_GAP) * 2);
	lib.hints = ltex_text(LFONT_SMALL, game->can_resume ? "A  Play     X  Resume save     L1 R1  A-Z     B  Back"
	                                                     : "A  Play     L1 R1  A-Z     B  Back", 0);
	shine_t = 0.0f;
	return 1;
}

// Loads one cover or title card per frame, for covers on screen.
static int lib_load_next(void) {
	for (int i = 0; i < lib.count; i++) {
		float x, y;
		cover_pos(i, &x, &y);
		if (y > SCREEN_H || y + COVER_H < 0) continue;
		Card* game = &lib.games[i];
		if (!game->art_tried && game->art_path[0]) {
			game->art = ltex_load(game->art_path, game->art_is_pixel);
			game->art_tried = 1;
			return 1;
		}
		if (!game->art && !game->label) {
			game->label = ltex_text_wrapped(LFONT_LABEL, game->name, (int)COVER_W - 24, 3, 1);
			return 1;
		}
	}
	return 0;
}

static void draw_cover(int i, float alpha) {
	Card* game = &lib.games[i];
	float s = lib.scale[i].x, lift = (s - 1.0f) / (FOCUS_SCALE - 1.0f);
	float x0, y0;
	cover_pos(i, &x0, &y0);
	if (y0 < LIB_TOP) alpha *= fmaxf(0.0f, 1.0f - (LIB_TOP - y0) / 100.0f);
	if (alpha <= 0.01f || y0 > SCREEN_H) return;
	y0 += (1.0f - lib.fade.x) * 30;
	float w = COVER_W * s, h = COVER_H * s;
	float cx = x0 + COVER_W * 0.5f, cy = y0 + COVER_H * 0.5f - lift * 8;
	float x = cx - w * 0.5f, y = cy - h * 0.5f, radius = 14 * s;

	lgfx_draw_shadow(x, y, w, h, lift * 12, (0.45f + lift * 0.4f) * alpha);
	if (game->art) {
		lgfx_draw_tex(game->art, x, y, w, h, radius, alpha);
	} else {
		float top[4], bottom[4];
		card_colors(game, top, bottom);
		top[3] = bottom[3] = alpha;
		lgfx_draw_gradient(x, y, w, h, radius, top, bottom);
		if (game->label) {
			float lw = game->label->w * s, lh = game->label->h * s;
			lgfx_draw_tex(game->label, cx - lw * 0.5f, cy - lh * 0.5f, lw, lh, 0, alpha);
		}
	}
	if (i == lib.focus && shine_t < 1.0f) lgfx_draw_shine(x, y, w, h, radius, shine_t);
}

static void draw_library(void) {
	float a = lib.fade.x;
	const float shade_top[4] = { 0, 0, 0, 0.35f }, shade_bottom[4] = { 0, 0, 0, 0.55f };
	lgfx_draw_gradient(0, 0, SCREEN_W, SCREEN_H, 0, shade_top, shade_bottom);

	for (int i = 0; i < lib.count; i++)
		if (i != lib.focus) draw_cover(i, a);
	draw_cover(lib.focus, a);

	if (lib.caption) {
		float x, y;
		cover_pos(lib.focus, &x, &y);
		float cx = x + COVER_W * 0.5f - lib.caption->w * 0.5f;
		cx = fmaxf(MARGIN_X, fminf(cx, SCREEN_W - MARGIN_X - lib.caption->w));
		float cy = y + COVER_H * FOCUS_SCALE + 2 + (1.0f - a) * 30;
		if (cy + lib.caption->h < SCREEN_H) lgfx_draw_tex(lib.caption, cx, cy, lib.caption->w, lib.caption->h, 0, a);
	}

	// Header over the grid, on a soft band so covers scrolling under it stay out of the way.
	const float band_top[4] = { 0, 0, 0, 0.55f }, band_bottom[4] = { 0, 0, 0, 0 };
	lgfx_draw_gradient(0, 0, SCREEN_W, LIB_TOP + 40, 0, band_top, band_bottom);
	if (lib.header) lgfx_draw_tex(lib.header, MARGIN_X, 22, lib.header->w, lib.header->h, 0, a);
	if (lib.subtitle && lib.header)
		lgfx_draw_tex(lib.subtitle, MARGIN_X + lib.header->w + 18, 22 + lib.header->h - lib.subtitle->h - 6,
		              lib.subtitle->w, lib.subtitle->h, 0, 0.65f * a);
	if (lib.hints) lgfx_draw_tex(lib.hints, SCREEN_W - MARGIN_X - lib.hints->w, 76, lib.hints->w, lib.hints->h, 0, 0.6f * a);

	if (lib.letter && SDL_GetTicks() / 1000.0 < lib.letter_until) {
		float size = 120, x = (SCREEN_W - size) * 0.5f, y = (SCREEN_H - size) * 0.5f;
		lgfx_draw_fill(x, y, size, size, 28, 0.1f, 0.1f, 0.12f, 0.8f);
		lgfx_draw_tex(lib.letter, x + (size - lib.letter->w) * 0.5f, y + (size - lib.letter->h) * 0.5f, lib.letter->w,
		              lib.letter->h, 0, 1);
	}
}

// Where focus goes when the launcher starts again after something it launched. After a game the
// player lands on Continue Playing, where the game just played is first; after a Pak, focus
// returns to that Pak's icon.
static void remember_launch(const Card* card) {
	if (card->kind == CARD_TOOL) putFile(LAST_FOCUS_PATH, (char*)card->path);
	else putFile(LAST_FOCUS_PATH, "game");
}

static void restore_focus(void) {
	if (!exists(LAST_FOCUS_PATH)) return;
	char text[600];
	getFile(LAST_FOCUS_PATH, text, sizeof(text));
	unlink(LAST_FOCUS_PATH);
	for (int r = 0; r < row_count; r++) {
		for (int c = 0; c < rows[r].count; c++) {
			Card* card = &rows[r].cards[c];
			int match = exactMatch(text, "game") ? card->kind == CARD_RECENT : exactMatch(card->path, text);
			if (match) {
				focus_row = r;
				focus_col[r] = c;
				return;
			}
		}
	}
}

// Everything except the quick menu and overlays: the home screen or the library.
static void draw_scene(void) {
	lgfx_draw_wallpaper(wallpaper, parallax_x.x * 12, parallax_y.x * 8);

	// Darken the lower half so the rows read clearly over any wallpaper.
	const float clear[4] = { 0, 0, 0, 0 }, dark[4] = { 0, 0, 0, 0.45f };
	lgfx_draw_gradient(0, rows_top - 60, SCREEN_W, SCREEN_H - rows_top + 60, 0, clear, dark);

	if (lib.open) {
		draw_library();
	} else if (row_count > 0) {
		draw_shelf();
		for (int r = 0; r < row_count; r++) {
			float y = row_y(r);
			if (y > SCREEN_H || y + row_height(r) < rows_top - 90) continue;
			float title_alpha = y >= rows_top - 1 ? 1.0f : fmaxf(0.0f, 1.0f - (rows_top - y) / 60.0f);
			float row_alpha = 1.0f;
			RowView* view = &views[r];
			if (view->title && title_alpha > 0.01f)
				lgfx_draw_tex(view->title, MARGIN_X - parallax_x.x * 3, y, view->title->w, view->title->h, 0,
				              (r == focus_row ? 0.85f : 0.5f) * title_alpha);
			for (int c = 0; c < rows[r].count; c++)
				if (!(r == focus_row && c == focus_col[r])) draw_icon(r, c, row_alpha);
			if (r == focus_row) draw_icon(r, focus_col[r], row_alpha); // focused icon on top of its neighbours
		}
	}
	draw_status();
}

///////////////////////////////////////
// Quick menu: a Control Center panel over a snapshot blur of the screen behind it.

enum { QM_WIFI, QM_BLUETOOTH, QM_SETTINGS, QM_SLEEP, QM_RESTART, QM_SHUTDOWN, QM_COUNT };
static const char* qm_labels[QM_COUNT] = { "Wi-Fi", "Bluetooth", "Settings", "Sleep", "Restart", "Shut Down" };

#define QM_W 384.0f
#define QM_INSET 16.0f
#define QM_TILE_W 164.0f
#define QM_TILE_H 112.0f
#define QM_ITEM_H 58.0f

static struct {
	int open;
	int focus;
	int wifi, bluetooth;
	Spring slide;   // 0 closed, 1 open
	LTex* labels[QM_COUNT];
	LTex* states[2];
	LTex* clock;
	LTex* battery;
	int closing;
} qm;

static void qm_update_states(void) {
	qm.wifi = WIFI_supported() ? WIFI_enabled() : -1;
	qm.bluetooth = BT_supported() ? BT_enabled() : -1;
	for (int i = 0; i < 2; i++) ltex_free(qm.states[i]);
	int on[2] = { qm.wifi, qm.bluetooth };
	for (int i = 0; i < 2; i++) qm.states[i] = ltex_text(LFONT_SMALL, on[i] < 0 ? "Not available" : on[i] ? "On" : "Off", 0);
}

static void open_quick_menu(void) {
	// Freeze and blur what is on screen once; the panel then draws over that snapshot.
	lgfx_begin_snapshot();
	draw_scene();
	lgfx_end_snapshot();
	qm.open = 1;
	qm.closing = 0;
	qm.focus = QM_WIFI;
	qm.slide.x = 0;
	qm.slide.v = 0;
	for (int i = 0; i < QM_COUNT; i++)
		if (!qm.labels[i]) qm.labels[i] = ltex_text(i < 2 ? LFONT_LABEL : LFONT_BODY, qm_labels[i], 0);
	ltex_free(qm.clock);
	ltex_free(qm.battery);
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char text[32];
	snprintf(text, sizeof(text), "%d:%02d", tm.tm_hour, tm.tm_min);
	qm.clock = ltex_text(LFONT_HERO, text, 0);
	snprintf(text, sizeof(text), "Battery %d%%%s", PWR_getBattery(), PWR_isCharging() ? ", charging" : "");
	qm.battery = ltex_text(LFONT_BODY, text, 0);
	qm_update_states();
	shine_t = 0.0f;
}

static void free_quick_menu(void) {
	for (int i = 0; i < QM_COUNT; i++) ltex_free(qm.labels[i]);
	for (int i = 0; i < 2; i++) ltex_free(qm.states[i]);
	ltex_free(qm.clock);
	ltex_free(qm.battery);
	memset(&qm, 0, sizeof(qm));
}

static void qm_move(int dx, int dy) {
	int f = qm.focus;
	if (f <= QM_BLUETOOTH) {
		if (dx > 0) f = QM_BLUETOOTH;
		else if (dx < 0) f = QM_WIFI;
		else if (dy > 0) f = QM_SETTINGS;
	} else {
		if (dy > 0 && f < QM_COUNT - 1) f++;
		else if (dy < 0) f = f == QM_SETTINGS ? QM_WIFI : f - 1;
	}
	if (f != qm.focus) shine_t = 0.0f;
	qm.focus = f;
}

static float qm_panel_x(void) {
	return SCREEN_W - (QM_W + QM_INSET) * qm.slide.x;
}

// Returns the top-left of item i inside the panel.
static void qm_item_pos(int i, float px, float* x, float* y) {
	if (i <= QM_BLUETOOTH) {
		*x = px + 24 + i * (QM_TILE_W + 8);
		*y = 196;
	} else {
		*x = px + 24;
		*y = 336 + (i - QM_SETTINGS) * (QM_ITEM_H + 8);
	}
}

static void draw_quick_menu(void) {
	lgfx_draw_snapshot(1);
	const float dim[4] = { 0, 0, 0, 0.25f * qm.slide.x };
	lgfx_draw_gradient(0, 0, SCREEN_W, SCREEN_H, 0, dim, dim);

	float px = qm_panel_x(), py = QM_INSET, ph = SCREEN_H - 2 * QM_INSET;
	lgfx_draw_shadow(px, py, QM_W, ph, 10, 0.6f);
	const float top[4] = { 0.16f, 0.16f, 0.2f, 0.72f }, bottom[4] = { 0.1f, 0.1f, 0.13f, 0.8f };
	lgfx_draw_gradient(px, py, QM_W, ph, 32, top, bottom);

	if (qm.clock) lgfx_draw_tex(qm.clock, px + 24, 44, qm.clock->w, qm.clock->h, 0, 1);
	if (qm.battery) lgfx_draw_tex(qm.battery, px + 24, 44 + 62, qm.battery->w, qm.battery->h, 0, 0.7f);

	for (int i = 0; i < QM_COUNT; i++) {
		float x, y, w = i <= QM_BLUETOOTH ? QM_TILE_W : QM_W - 48, h = i <= QM_BLUETOOTH ? QM_TILE_H : QM_ITEM_H;
		qm_item_pos(i, px, &x, &y);
		int focused = i == qm.focus;
		int on = i == QM_WIFI ? qm.wifi == 1 : i == QM_BLUETOOTH ? qm.bluetooth == 1 : 0;
		float s = focused ? 1.04f : 1.0f;
		float cx = x + w * 0.5f, cy = y + h * 0.5f;
		w *= s;
		h *= s;
		x = cx - w * 0.5f;
		y = cy - h * 0.5f;
		if (on) lgfx_draw_fill(x, y, w, h, 18, 0.18f, 0.48f, 1.0f, 0.95f);
		else lgfx_draw_fill(x, y, w, h, 18, 1, 1, 1, focused ? 0.24f : 0.08f);
		LTex* label = qm.labels[i];
		if (i <= QM_BLUETOOTH) {
			if (label) lgfx_draw_tex(label, x + 18, y + h - 58, label->w, label->h, 0, 1);
			LTex* state = qm.states[i];
			if (state) lgfx_draw_tex(state, x + 18, y + h - 32, state->w, state->h, 0, 0.75f);
		} else if (label) {
			lgfx_draw_tex(label, x + 20, y + (h - label->h) * 0.5f, label->w, label->h, 0, 1);
		}
		if (focused && shine_t < 1.0f) lgfx_draw_shine(x, y, w, h, 18, shine_t);
	}
}

// Brightness or volume level while the player changes it (show_setting from PWR_update).
static void draw_setting_overlay(int show_setting) {
	if (!show_setting) return;
	static LTex* labels[3];
	if (!labels[1]) labels[1] = ltex_text(LFONT_LABEL, "Brightness", 0);
	if (!labels[2]) labels[2] = ltex_text(LFONT_LABEL, "Volume", 0);
	int brightness = show_setting == 1;
	float level = brightness ? (float)GetBrightness() / BRIGHTNESS_MAX : (float)GetVolume() / VOLUME_MAX;
	float w = 360, h = 52, x = (SCREEN_W - w) * 0.5f, y = 22;
	lgfx_draw_fill(x, y, w, h, h * 0.5f, 0.1f, 0.1f, 0.13f, 0.85f);
	LTex* label = labels[brightness ? 1 : 2];
	if (label) lgfx_draw_tex(label, x + 24, y + (h - label->h) * 0.5f, label->w, label->h, 0, 0.9f);
	float bx = x + 150, bw = w - 150 - 26, by = y + h * 0.5f - 4;
	lgfx_draw_fill(bx, by, bw, 8, 4, 1, 1, 1, 0.2f);
	lgfx_draw_fill(bx, by, bw * fmaxf(0.0f, fminf(1.0f, level)), 8, 4, 1, 1, 1, 0.95f);
}

static void draw_frame(int show_setting) {
	lgfx_begin_frame();
	if (qm.open) draw_quick_menu();
	else draw_scene();
	draw_setting_overlay(show_setting);

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

	row_count = ldata_load_rows(rows, MAX_ROWS);
	layout_rows();
	if (exists(GAME_SWITCHER_PERSIST_PATH)) {
		// The in-game switcher brings the player to Continue Playing, which is row 0 when it exists.
		unlink(GAME_SWITCHER_PERSIST_PATH);
		focus_row = 0;
	}
	if (row_count == 0) show_toast("No games or Paks found");
	restore_focus();

	int quit = 0, dirty = 1, show_setting = 0;
	Uint32 last = SDL_GetTicks();
	while (!quit) {
		Uint32 now = SDL_GetTicks();
		float dt = (now - last) / 1000.0f;
		if (dt > 0.05f) dt = 0.05f;
		last = now;

		PAD_poll();
		PWR_update(&dirty, &show_setting, NULL, NULL);

		// Development: a new build is installed by creating this file, so the launcher exits cleanly
		// and MinUI.pak's loop starts the new binary without killing the process.
		if (exists("/tmp/launcher_reload")) {
			unlink("/tmp/launcher_reload");
			break;
		}

		if (!show_setting) {
			if (PAD_justPressed(BTN_FN1)) quit = ldata_run_fn_action(0);
			else if (PAD_justPressed(BTN_FN2)) quit = ldata_run_fn_action(1);
			else if (PAD_justPressed(BTN_FN3)) quit = ldata_run_fn_action(2);
		}

		if (qm.open && !qm.closing && !quit) {
			if (PAD_justRepeated(BTN_RIGHT)) qm_move(1, 0);
			else if (PAD_justRepeated(BTN_LEFT)) qm_move(-1, 0);
			else if (PAD_justRepeated(BTN_DOWN)) qm_move(0, 1);
			else if (PAD_justRepeated(BTN_UP)) qm_move(0, -1);
			else if (PAD_justPressed(BTN_B) || PAD_tappedMenu(SDL_GetTicks())) qm.closing = 1;
			else if (PAD_justPressed(BTN_A)) {
				switch (qm.focus) {
				case QM_WIFI:
					if (qm.wifi >= 0) WIFI_enable(!WIFI_enabled());
					qm_update_states();
					break;
				case QM_BLUETOOTH:
					if (qm.bluetooth >= 0) BT_enable(!BT_enabled());
					qm_update_states();
					break;
				case QM_SETTINGS:
					if (ldata_launch_tool("Settings")) quit = 1;
					else show_toast("Settings Pak not found");
					break;
				case QM_SLEEP:
					qm.closing = 1;
					qm.slide.x = 0;
					PWR_sleep(); // returns after wake
					dirty = 1;
					break;
				case QM_RESTART:
					PWR_powerOff(1);
					break;
				case QM_SHUTDOWN:
					PWR_powerOff(0);
					break;
				}
			}
		} else if (qm.open) {
			// Closing: ignore input until the panel has slid away.
		} else if (!quit && !show_setting && PAD_tappedMenu(SDL_GetTicks())) {
			open_quick_menu();
		} else if (lib.open && !quit) {
			Card* game = &lib.games[lib.focus];
			if (PAD_justRepeated(BTN_RIGHT)) lib_move(1, 0);
			else if (PAD_justRepeated(BTN_LEFT)) lib_move(-1, 0);
			else if (PAD_justRepeated(BTN_DOWN)) lib_move(0, 1);
			else if (PAD_justRepeated(BTN_UP)) lib_move(0, -1);
			else if (PAD_justRepeated(BTN_R1)) jump_letter(1);
			else if (PAD_justRepeated(BTN_L1)) jump_letter(-1);
			else if (PAD_justPressed(BTN_B)) {
				close_library();
				shelf.row = -1; // rebuild the Top Shelf for the console card
			} else if (PAD_justPressed(BTN_A)) {
				remember_launch(game);
				ldata_launch(game, 0);
				quit = 1;
			} else if (PAD_justPressed(BTN_X) && game->can_resume) {
				remember_launch(game);
				ldata_launch(game, 1);
				quit = 1;
			}
		} else if (row_count > 0 && !quit) {
			if (PAD_justRepeated(BTN_RIGHT)) move_focus(1, 0);
			else if (PAD_justRepeated(BTN_LEFT)) move_focus(-1, 0);
			else if (PAD_justRepeated(BTN_DOWN)) move_focus(0, 1);
			else if (PAD_justRepeated(BTN_UP)) move_focus(0, -1);

			Card* card = focused_card();
			if (PAD_justPressed(BTN_A)) {
				if (card->kind == CARD_CONSOLE) open_library(card, 0);
				else {
					remember_launch(card);
					ldata_launch(card, 0);
					quit = 1;
				}
			} else if (PAD_justPressed(BTN_X) && card->kind == CARD_RECENT && card->can_resume) {
				remember_launch(card);
				ldata_launch(card, 1);
				quit = 1;
			}
		}
		if (quit) break;

		int animating = 0;
		if (qm.open) {
			animating |= spring_step(&qm.slide, qm.closing ? 0.0f : 1.0f, dt);
			if (shine_t < 1.0f) {
				shine_t = fminf(1.0f, shine_t + dt / SHINE_SECONDS);
				animating = 1;
			}
			if (qm.closing && qm.slide.x < 0.02f) {
				free_quick_menu();
				dirty = 1;
			}
		} else if (lib.open) {
			animating |= lib_update_caption();
			for (int i = 0; i < lib.count; i++)
				animating |= spring_step(&lib.scale[i], i == lib.focus ? FOCUS_SCALE : 1.0f, dt);
			animating |= spring_step(&lib.scroll, lib_scroll_target(), dt);
			animating |= spring_step(&lib.fade, 1.0f, dt);
			if (shine_t < 1.0f) {
				shine_t = fminf(1.0f, shine_t + dt / SHINE_SECONDS);
				animating = 1;
			}
			if (lib.letter && SDL_GetTicks() / 1000.0 < lib.letter_until + 0.1) animating = 1;
			animating |= lib_load_next();
		} else if (row_count > 0) {
			animating |= update_shelf();
			for (int r = 0; r < row_count; r++) {
				animating |= spring_step(&views[r].scroll, scroll_target(r), dt);
				for (int c = 0; c < rows[r].count; c++)
					animating |= spring_step(&views[r].scale[c], r == focus_row && c == focus_col[r] ? FOCUS_SCALE : 1.0f, dt);
			}
			animating |= spring_step(&rows_scroll, rows_scroll_target(), dt);
			animating |= spring_step(&shelf_fade, 1.0f, dt);
			float px = rows[focus_row].count > 1 ? (float)focus_col[focus_row] / (rows[focus_row].count - 1) * 2 - 1 : 0;
			float py = row_count > 1 ? (float)focus_row / (row_count - 1) * 2 - 1 : 0;
			animating |= spring_step(&parallax_x, px, dt);
			animating |= spring_step(&parallax_y, py, dt);
			if (shine_t < 1.0f) {
				shine_t = fminf(1.0f, shine_t + dt / SHINE_SECONDS);
				animating = 1;
			}
			animating |= load_next_art();
		}
		animating |= update_status();
		if (toast && SDL_GetTicks() / 1000.0 < toast_until + 0.1) animating = 1;

		// Draw only while something moves, so the GPU and battery rest on a still screen.
		if (animating || dirty || show_setting) {
			draw_frame(show_setting);
			dirty = 0;
		} else {
			debug_step();
			SDL_Delay(16);
		}
	}

	if (lib.open) close_library();
	free_quick_menu();
	ldata_free_rows(rows, row_count);
	for (int r = 0; r < row_count; r++) {
		ltex_free(views[r].title);
		free(views[r].scale);
	}
	free_shelf();
	ltex_free(status.clock);
	ltex_free(status.percent);
	ltex_free(wallpaper);
	ltex_free(toast);
	lgfx_quit();

	PWR_quit();
	PAD_quit();
	GFX_quit();
	QuitSettings();
	return 0;
}
