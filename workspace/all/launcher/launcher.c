// Bramley's launcher: a tvOS-style home screen.
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
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "config.h"
#include "lgfx.h"
#include "ldata.h"
#include "lactivity.h"
#include "lfiles.h"
#include "lradio.h"
#include "lmusic.h"
#include "bramley.h"
#include "bramley_update.h"
#include "lsetup.h"
#include "lwallpaper.h"

#define SCREEN_W 1024
#define SCREEN_H 768
#define MAX_ROWS 4
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
// screenshot. Lower case presses buttons on the desktop key map: a = A, b = B, x = X, y = Y,
// l = L1, r = R1, m = MENU, s = SELECT. Upper case P presses POWER.
// Upper case S saves a
// screenshot to $LAUNCHER_DEBUG_SHOT-<n>.bmp. One step every 0.6 s. Unused on the device.
static int debug_shot_wanted; // a screenshot step waits for the next frame to be drawn
static int debug_drawing;     // set while draw_frame runs

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
	if (debug_shot_wanted && !debug_drawing) return;
	if (!debug_shot_wanted && (!keys || !*keys || SDL_GetTicks() < next)) return;
	next = SDL_GetTicks() + 600;
	while (*keys == ' ') keys++;
	char k = debug_shot_wanted ? 'S' : *keys ? *keys++ : 0;
	if (k == 'S' && !debug_drawing) {
		debug_shot_wanted = 1;
		return;
	}
	debug_shot_wanted = 0;
	SDL_Keycode code = k == 'R' ? SDLK_RIGHT : k == 'L' ? SDLK_LEFT : k == 'U' ? SDLK_UP : k == 'D' ? SDLK_DOWN
	                 : k == 'a' ? SDLK_s : k == 'b' ? SDLK_a : k == 'x' ? SDLK_w : k == 'l' ? SDLK_TAB : k == 'r' ? SDLK_b : k == 'm' ? SDLK_SPACE
	                 : k == 's' ? SDLK_BACKQUOTE : k == 'y' ? SDLK_q : k == 'P' ? SDLK_BACKSPACE : 0;
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

// On a crash, append the signal and the call stack to the crash log, which survives restarts,
// then crash as before so the safety net sees it. Map the addresses with addr2line on the
// unstripped build.
static void crash_report(int sig) {
	void* frames[64];
	int n = backtrace(frames, 64);
	int fd = open(SDCARD_PATH "/.userdata/" PLATFORM "/logs/bramley-crashes.txt", O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (fd >= 0) {
		char head[64];
		int len = snprintf(head, sizeof(head), "signal %d in thread %ld, stack:\n", sig, (long)getpid());
		write(fd, head, len);
		backtrace_symbols_fd(frames, n, fd);
		close(fd);
	}
	signal(sig, SIG_DFL);
	raise(sig);
}

// NextUI's seven theme colors, set to Bramley's palette. Bramley draws its own interface and no
// longer offers these in Settings; this keeps Paks that still draw with NextUI's code, and its
// overlays, in Bramley's colors. Written only when something differs.
// Shutting down or restarting: the wordmark and PWR_powerOff's message, drawn with the GL kit.
static void power_screen(const char* message) {
	lgfx_begin_frame();
	lgfx_draw_fill(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, 0, 1);
	// the boot screen's layout (docs/brand): the icon, "Bramley" in ivory, then the message
	LTex* icon = ltex_load(BRAMLEY_RES_DIR "/brand/icon-176.png", 0);
	// the wordmark: Inter Regular in ivory (docs/brand)
	LTex* t = NULL;
	TTF_Font* f = TTF_OpenFont(RES_PATH "/Inter-Regular.ttf", 44);
	if (f) {
		SDL_Surface* s = TTF_RenderUTF8_Blended(f, BRAMLEY_NAME, (SDL_Color){ 243, 235, 214, 255 });
		if (s) t = ltex_from_surface(s, 0), SDL_FreeSurface(s);
		TTF_CloseFont(f);
	}
	LTex* m = ltex_text(LFONT_BODY, message, 0);
	float y = SCREEN_H * 0.5f - 150;
	if (icon) lgfx_draw_tex(icon, (SCREEN_W - 176) * 0.5f, y, 176, 176, 0, 1);
	if (t) lgfx_draw_tex(t, (SCREEN_W - t->w) * 0.5f, y + 196, t->w, t->h, 0, 1);
	if (m) lgfx_draw_tex(m, (SCREEN_W - m->w) * 0.5f, y + 270, m->w, m->h, 0, 0.6f);
	ltex_free(icon);
	ltex_free(t);
	ltex_free(m);
	lgfx_end_frame();
}

static void apply_bramley_palette(void) {
	static const uint32_t palette[7] = {
		0xFFFFFFFFu, // main: white
		0x4AB278FFu, // accent: Bramley green (docs/brand)
		0x2C2C2EFFu, // second accent: dark gray
		0xFFFFFFFFu, // list text
		0x1C1C1EFFu, // selected list text, on the white highlight
		0xAEAEB2FFu, // hints
		0x101018FFu, // background without a wallpaper
	};
	int changed = 0;
	for (int i = 0; i < 7; i++)
		if (CFG_getColor(i + 1) != palette[i]) {
			CFG_setColor(i + 1, palette[i]);
			changed = 1;
		}
	if (changed) {
		CFG_clearPalette();
		CFG_sync();
		LOG_info("launcher: theme colors set to Bramley's palette\n");
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

// Settings lives in the launcher (../settings): it opens as a screen of its own and returns here
// when closed, instead of starting a separate program, which took most of a second each way.
int bramley_open_settings(SDL_Surface* screen, void (*tick)(void));
void bramley_close_settings(int keep_place);
int bramley_settings_typing(void);
static SDL_Surface* screen;
// SELECT in Settings closes it and shows Now Playing; closing Now Playing then goes back to it.
static int settings_wants_music, back_to_settings;
// Set when Settings has left its own frame in the blur snapshot for Now Playing to use.
static int np_over_settings;

// Set while Software Update has an update waiting (bramley_update.h); read every half minute.
static int update_waiting;

static int is_settings(const Card* card) {
	return card->kind == CARD_TOOL && suffixMatch("/Settings.pak", (char*)card->path);
}

// Files' card opens Bramley's Files, drawn here, instead of NextCommander.
static int is_files(const Card* card) {
	return card->kind == CARD_TOOL && suffixMatch("/Files.pak", (char*)card->path);
}

// Game Tracker's card opens Activity, drawn here, instead of the old app.
static int is_activity(const Card* card) {
	return card->kind == CARD_TOOL && suffixMatch("/Game Tracker.pak", (char*)card->path);
}

static void open_settings(void);

// The full wallpaper is decoded on a thread (about 70 ms for a 1024x768 JPEG on the Brick), so the
// first frame shows at once over its small blurred copy, or the gradient, and the sharp picture
// replaces it a moment later (wallpaper_arrived()).
static SDL_Surface* wall_decoded;
static SDL_atomic_t wall_done;
static SDL_Thread* wall_thread;

static int decode_wallpaper(void* path) {
	wall_decoded = IMG_Load((char*)path);
	free(path);
	SDL_AtomicSet(&wall_done, 1);
	return 0;
}

// Swaps in the decoded wallpaper once it is ready. Returns 1 when it did.
static int wallpaper_arrived(void) {
	if (!wall_thread || !SDL_AtomicGet(&wall_done)) return 0;
	SDL_WaitThread(wall_thread, NULL);
	wall_thread = NULL;
	if (!wall_decoded) return 0;
	LTex* tex = ltex_from_surface(wall_decoded, 0);
	SDL_FreeSurface(wall_decoded);
	wall_decoded = NULL;
	if (!tex) return 0;
	ltex_free(wallpaper);
	wallpaper = tex;
	return 1;
}

// The wallpaper chosen in Settings (lwallpaper.c), otherwise a dark gradient made once.
static LTex* load_wallpaper(void) {
	Wallpaper w;
	wallpaper_current(&w);
	if (w.path[0]) {
		if (wall_thread) SDL_WaitThread(wall_thread, NULL); // an earlier load still running
		if (wall_decoded) SDL_FreeSurface(wall_decoded);
		wall_decoded = NULL;
		SDL_AtomicSet(&wall_done, 0);
		wall_thread = SDL_CreateThread(decode_wallpaper, "wallpaper", strdup(w.path));
		LTex* tex = w.blur[0] ? ltex_load(w.blur, 0) : NULL;
		if (tex) return tex;
	}
	// Smooth colour fields, so a small image scaled up looks the same and takes a quarter of the time.
	SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, 256, 192, 32, SDL_PIXELFORMAT_ABGR8888);
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
	return rows[r].count > 0 && (rows[r].cards[0].kind == CARD_TOOL || rows[r].cards[0].kind == CARD_MUSIC);
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
	} else if (card->kind == CARD_COLLECTION) {
		snprintf(kicker, sizeof(kicker), "Collection");
		snprintf(detail, sizeof(detail), "%d %s", card->game_count, card->game_count == 1 ? "item" : "items");
	} else if (card->kind == CARD_MUSIC) {
		snprintf(kicker, sizeof(kicker), "App");
		int albums = lmusic_shelf_count(0), books = lmusic_shelf_count(1);
		if (books) snprintf(detail, sizeof(detail), "%d %s  ·  %d %s", albums, albums == 1 ? "album" : "albums", books,
		                    books == 1 ? "audiobook" : "audiobooks");
		else snprintf(detail, sizeof(detail), "%d %s", albums, albums == 1 ? "album" : "albums");
	} else {
		snprintf(kicker, sizeof(kicker), "App");
		snprintf(detail, sizeof(detail), "%s", is_activity(card) ? "Time played, day by day" : is_files(card) ? "Browse the card" : is_settings(card) ? "Network, display, sound and more" : "Tools Pak");
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

static void show_toast_for(const char* text, double seconds) {
	ltex_free(toast);
	toast = ltex_text(LFONT_BODY, text, 0);
	toast_until = SDL_GetTicks() / 1000.0 + seconds;
}

static void show_toast(const char* text) {
	show_toast_for(text, 1.8);
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
		lgfx_draw_art(card->art, x, y, w, h, radius, row_alpha);
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
	// Settings carries a red dot while a Bramley update waits, as iOS badges an app
	if (update_waiting && is_settings(card)) {
		float d = 22 * s;
		lgfx_draw_fill(x + w - d * 0.7f, y - d * 0.3f, d, d, d * 0.5f, BRAMLEY_ALERT, row_alpha);
	}
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

static void draw_eq_bars(float x, float y, float h, float alpha);

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
	if (lmusic_active()) draw_eq_bars(x - 36, by - 2, 18, lmusic_playing() ? 0.9f : 0.45f);
}


///////////////////////////////////////
// Library: a grid of covers. Game covers for one console are 3:4; the Music app uses the same
// grid with square album covers.

#define LIB_COLS 5
#define COVER_W 150.0f
#define COVER_GAP 40.0f
#define LIB_TOP 132.0f
#define LIB_LINE (lib.cover_h + 64.0f)
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
	float cover_h;
	int music, books; // the Music app, showing albums or audiobooks
	int collection;   // a collection: the player's own order, so no A to Z
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
	int collection = console->kind == CARD_COLLECTION;
	lib.games = collection ? ldata_load_collection(console, &lib.count) : ldata_load_games(console, &lib.count);
	if (!lib.count) {
		ldata_free_games(lib.games, 0);
		lib.games = NULL;
		show_toast("No games in this folder");
		return;
	}
	lib.open = 1;
	lib.cover_h = 200.0f;
	lib.collection = collection;
	lib.console = console;
	lib.focus = focus >= 0 && focus < lib.count ? focus : 0;
	lib.scale = calloc(lib.count, sizeof(Spring));
	for (int i = 0; i < lib.count; i++) lib.scale[i].x = 1.0f;
	lib.caption_for = -1;
	lib.header = ltex_text(LFONT_HERO, console->name, 640);
	char text[64];
	if (collection) snprintf(text, sizeof(text), "%d %s", lib.count, lib.count == 1 ? "item" : "items");
	else snprintf(text, sizeof(text), "%d %s", lib.count, lib.count == 1 ? "game" : "games");
	lib.subtitle = ltex_text(LFONT_BODY, text, 0);
	// Start with the focused line at the top of the grid.
	lib.scroll_goal = (lib.focus / LIB_COLS) * LIB_LINE;
	lib.scroll.x = lib.scroll_goal;
	lib.fade.x = 0.0f;
}

// One colour per album for covers without a picture, the same in the grid, the album page and
// Now Playing.
static float album_hue(int al) {
	unsigned h = 2166136261u;
	for (const char* c = lmusic_lib()->albums[al].name; *c; c++) h = (h ^ (unsigned char)*c) * 16777619u;
	return (h % 360) / 360.0f;
}

// The Music app: albums or audiobooks from lmusic's library, as cards for the cover grid.
static void open_music(int books) {
	if (lib.open) close_library();
	int n = lmusic_shelf_count(books);
	const ml_lib* ml = lmusic_lib();
	lib.games = calloc(n > 0 ? n : 1, sizeof(Card));
	for (int k = 0; k < n; k++) {
		int al = lmusic_shelf_album(books, k);
		Card* card = &lib.games[k];
		card->kind = CARD_ALBUM;
		card->index = al;
		snprintf(card->name, sizeof(card->name), "%s", ml->albums[al].name);
		snprintf(card->tag, sizeof(card->tag), "%s", lmusic_artist_of(al));
		card->hue = album_hue(al);
	}
	lib.count = n;
	lib.open = 1;
	lib.music = 1;
	lib.books = books;
	lib.cover_h = COVER_W;
	lib.focus = 0;
	lib.scale = calloc(n > 0 ? n : 1, sizeof(Spring));
	for (int i = 0; i < n; i++) lib.scale[i].x = 1.0f;
	lib.caption_for = -1;
	lib.header = ltex_text(LFONT_HERO, books ? "Audiobooks" : "Music", 640);
	char text[64];
	if (books) snprintf(text, sizeof(text), "%d %s", n, n == 1 ? "book" : "books");
	else snprintf(text, sizeof(text), "%d %s", n, n == 1 ? "album" : "albums");
	lib.subtitle = ltex_text(LFONT_BODY, text, 0);
	lib.scroll_goal = lib.scroll.x = 0;
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
	if (line_top + lib.cover_h + 56 - lib.scroll_goal > SCREEN_H) lib.scroll_goal = line_top + lib.cover_h + 56 - SCREEN_H;
	if (lib.scroll_goal < 0) lib.scroll_goal = 0;
	return lib.scroll_goal;
}

static int lib_update_caption(void) {
	if (lib.caption_for == lib.focus) return 0;
	lib.caption_for = lib.focus;
	ltex_free(lib.caption);
	ltex_free(lib.hints);
	if (!lib.count) {
		lib.caption = NULL;
		lib.hints = ltex_text(LFONT_SMALL, lib.books ? "L1 R1  Music     B  Back" : "L1 R1  Audiobooks     B  Back", 0);
		return 1;
	}
	Card* game = &lib.games[lib.focus];
	if (lib.music) {
		char text[300];
		snprintf(text, sizeof(text), "%s%s%s", game->name, game->tag[0] ? "  ·  " : "", game->tag);
		lib.caption = ltex_text(LFONT_LABEL, text, (int)(COVER_W + COVER_GAP) * 3);
		lib.hints = ltex_text(LFONT_SMALL, lib.books ? "A  Resume     L1 R1  Music     SELECT  Now Playing     B  Back"
		                                             : "A  Open     L1 R1  Audiobooks     SELECT  Now Playing     B  Back", 0);
		shine_t = 0.0f;
		return 1;
	}
	if (game->kind == CARD_GAME) ldata_check_resume(game);
	lib.caption = ltex_text(LFONT_LABEL, game->name, (int)(COVER_W + COVER_GAP) * 2);
	const char* hints = game->can_resume ? (lib.collection ? "A  Play     X  Resume save     B  Back"
	                                                       : "A  Play     X  Resume save     L1 R1  A-Z     B  Back")
	                                     : (lib.collection ? "A  Play     B  Back" : "A  Play     L1 R1  A-Z     B  Back");
	lib.hints = ltex_text(LFONT_SMALL, game->kind == CARD_TOOL ? (lib.collection ? "A  Open     B  Back" : "A  Open") : hints, 0);
	shine_t = 0.0f;
	return 1;
}

// Loads one cover or title card per frame, for covers on screen.
static int lib_load_next(void) {
	for (int i = 0; i < lib.count; i++) {
		float x, y;
		cover_pos(i, &x, &y);
		if (y > SCREEN_H || y + lib.cover_h < 0) continue;
		Card* game = &lib.games[i];
		if (lib.music && !game->art_tried && !game->art_path[0]) {
			// Covers may come from the daemon a moment later, so keep asking until it has answered.
			const char* cover = lmusic_cover(game->index);
			if (cover) snprintf(game->art_path, sizeof(game->art_path), "%s", cover);
		}
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
	float w = COVER_W * s, h = lib.cover_h * s;
	float cx = x0 + COVER_W * 0.5f, cy = y0 + lib.cover_h * 0.5f - lift * 8;
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
	if (lib.count) draw_cover(lib.focus, a);

	if (lib.caption && lib.count) {
		float x, y;
		cover_pos(lib.focus, &x, &y);
		float cx = x + COVER_W * 0.5f - lib.caption->w * 0.5f;
		cx = fmaxf(MARGIN_X, fminf(cx, SCREEN_W - MARGIN_X - lib.caption->w));
		float cy = y + lib.cover_h * FOCUS_SCALE + 2 + (1.0f - a) * 30;
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
	else {
		putFile(LAST_FOCUS_PATH, "game");
		// Games pause the music; the emulator is not changed to share the speaker.
		lmusic_pause_for_game();
	}
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


///////////////////////////////////////
// Album page: the cover on the left and the track list on the right.

#define ALB_COVER 300.0f
#define ALB_ROW_H 54.0f
#define ALB_LIST_X (MARGIN_X + ALB_COVER + 48)
#define ALB_LIST_TOP 200.0f

static struct {
	int open;
	int album, focus, count;
	Spring scroll, fade;
	float scroll_goal;
	LTex* cover;
	LTex* title;
	LTex* artist;
	LTex** tracks;
	LTex* hints;
} alb;

static void close_album(void) {
	ltex_free(alb.cover);
	ltex_free(alb.title);
	ltex_free(alb.artist);
	ltex_free(alb.hints);
	for (int i = 0; i < alb.count; i++) ltex_free(alb.tracks[i]);
	free(alb.tracks);
	memset(&alb, 0, sizeof(alb));
	shine_t = 1.0f;
}

static void open_album(int al) {
	if (alb.open) close_album();
	const ml_lib* ml = lmusic_lib();
	alb.open = 1;
	alb.album = al;
	alb.count = ml->albums[al].n;
	alb.focus = lmusic_album() == al && lmusic_track() >= 0 ? lmusic_track() : 0;
	const char* cover = lmusic_cover(al);
	if (cover) alb.cover = ltex_load(cover, 0);
	alb.title = ltex_text_wrapped(LFONT_TITLE, ml->albums[al].name, (int)(SCREEN_W - ALB_LIST_X - MARGIN_X), 2, 0);
	alb.artist = ltex_text(LFONT_BODY, lmusic_artist_of(al), (int)(SCREEN_W - ALB_LIST_X - MARGIN_X));
	alb.tracks = calloc(alb.count, sizeof(LTex*));
	for (int i = 0; i < alb.count; i++) {
		char text[200];
		snprintf(text, sizeof(text), "%d   %s", i + 1, ml->tracks[ml->albums[al].first + i].name);
		alb.tracks[i] = ltex_text(LFONT_BODY, text, (int)(SCREEN_W - ALB_LIST_X - MARGIN_X - 40));
	}
	alb.hints = ltex_text(LFONT_SMALL, "A  Play from here     SELECT  Now Playing     B  Back", 0);
	alb.fade.x = 0;
	shine_t = 0;
}

static float alb_scroll_target(void) {
	float row = alb.focus * ALB_ROW_H;
	float visible = SCREEN_H - ALB_LIST_TOP - 40;
	if (row - alb.scroll_goal < 0) alb.scroll_goal = row;
	if (row + ALB_ROW_H - alb.scroll_goal > visible) alb.scroll_goal = row + ALB_ROW_H - visible;
	return alb.scroll_goal;
}

static void draw_eq_bars(float x, float y, float h, float alpha); // the playing indicator, below

static void draw_album(void) {
	float a = alb.fade.x;
	const float shade[4] = { 0, 0, 0, 0.5f };
	lgfx_draw_gradient(0, 0, SCREEN_W, SCREEN_H, 0, shade, shade);

	Card fake = { .hue = album_hue(alb.album) };
	float cy = 120 + (1 - a) * 20;
	lgfx_draw_shadow(MARGIN_X, cy, ALB_COVER, ALB_COVER, 14, 0.7f * a);
	if (alb.cover) lgfx_draw_tex(alb.cover, MARGIN_X, cy, ALB_COVER, ALB_COVER, 20, a);
	else {
		float top[4], bottom[4];
		card_colors(&fake, top, bottom);
		top[3] = bottom[3] = a;
		lgfx_draw_gradient(MARGIN_X, cy, ALB_COVER, ALB_COVER, 20, top, bottom);
	}
	if (alb.title) lgfx_draw_tex(alb.title, ALB_LIST_X, 96, alb.title->w, alb.title->h, 0, a);
	if (alb.artist && alb.title)
		lgfx_draw_tex(alb.artist, ALB_LIST_X, 96 + alb.title->h + 2, alb.artist->w, alb.artist->h, 0, 0.7f * a);
	if (alb.hints) lgfx_draw_tex(alb.hints, MARGIN_X, SCREEN_H - 40, alb.hints->w, alb.hints->h, 0, 0.6f * a);

	int playing_here = lmusic_album() == alb.album;
	for (int i = 0; i < alb.count; i++) {
		float y = ALB_LIST_TOP + i * ALB_ROW_H - alb.scroll.x;
		if (y < ALB_LIST_TOP - ALB_ROW_H || y > SCREEN_H - 50) continue;
		float row_a = a * (y < ALB_LIST_TOP ? fmaxf(0.0f, 1.0f - (ALB_LIST_TOP - y) / ALB_ROW_H) : 1.0f);
		float w = SCREEN_W - ALB_LIST_X - MARGIN_X + 16;
		if (i == alb.focus) {
			lgfx_draw_fill(ALB_LIST_X - 16, y, w, ALB_ROW_H - 6, 14, 1, 1, 1, 0.18f * row_a);
			lgfx_draw_ring(ALB_LIST_X - 16, y, w, ALB_ROW_H - 6, 14, 2, 1, 1, 1, 0.9f * row_a);
			if (shine_t < 1.0f) lgfx_draw_shine(ALB_LIST_X - 16, y, w, ALB_ROW_H - 6, 14, shine_t);
		}
		LTex* t = alb.tracks[i];
		if (t) lgfx_draw_tex(t, ALB_LIST_X, y + (ALB_ROW_H - 6 - t->h) * 0.5f, t->w, t->h, 0, row_a * (i == alb.focus ? 1 : 0.8f));
		if (playing_here && i == lmusic_track())
			draw_eq_bars(SCREEN_W - MARGIN_X - 26, y + 14, 20, row_a * (lmusic_playing() ? 1 : 0.4f));
	}
}

///////////////////////////////////////
// Now Playing: SELECT from anywhere, over a snapshot blur of the screen behind.

#define NP_COVER 340.0f
#define NP_TEXT_X (MARGIN_X + 24 + NP_COVER + 48)
#define NP_TEXT_W (SCREEN_W - NP_TEXT_X - MARGIN_X)

static struct {
	int open, closing;
	Spring slide;
	int album, track;
	char shown_title[256];
	LTex* cover;
	LTex* title;
	LTex* sub;
	LTex* next;
	LTex* times[2];
	int times_for;
	LTex* mode;
	int mode_for;
	LTex* hints;
	int hints_book;
} np = { .album = -1, .track = -1, .times_for = -1, .mode_for = -1, .hints_book = -1 };

static void np_free_texts(void) {
	ltex_free(np.cover);
	ltex_free(np.title);
	ltex_free(np.sub);
	ltex_free(np.next);
	ltex_free(np.times[0]);
	ltex_free(np.times[1]);
	ltex_free(np.mode);
	ltex_free(np.hints);
	np.cover = np.title = np.sub = np.next = np.times[0] = np.times[1] = np.mode = np.hints = NULL;
	np.album = np.track = np.times_for = np.mode_for = np.hints_book = -1;
	np.shown_title[0] = '\0';
}

static void draw_scene(void);

static void open_now_playing(void) {
	if (!lmusic_active()) {
		np_over_settings = 0;
		show_toast("Nothing playing. Open Music in Apps.");
		return;
	}
	if (!np_over_settings) {
		lgfx_begin_snapshot();
		draw_scene();
		lgfx_end_snapshot();
	}
	np_over_settings = 0;
	np.open = 1;
	np.closing = 0;
	np.slide.x = 0;
	np.slide.v = 0;
	toast_until = 0; // a "Music paused" banner has done its job once Now Playing is open
}

// Runs while Settings is open, so a track that ends still moves the music queue on.
static void settings_tick(void) {
	lmusic_update();
	if (PAD_justPressed(BTN_SELECT) && !bramley_settings_typing()) {
		settings_wants_music = 1;
		bramley_close_settings(1);
	}
}

static void open_settings(void) {
	settings_wants_music = 0;
	bramley_open_settings(screen, settings_tick);
	// the wallpaper may have been changed, and the clock has moved on
	ltex_free(wallpaper);
	wallpaper = load_wallpaper();
	status.minute = -1;
	if (settings_wants_music) {
		settings_wants_music = 0;
		np_over_settings = 1;
		open_now_playing();
		back_to_settings = np.open; // nothing playing: a toast shows instead, and home stays
	}
}


static void fmt_time(float t, char* out, size_t size) {
	int s = t < 0 ? 0 : (int)t;
	if (s >= 3600) snprintf(out, size, "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
	else snprintf(out, size, "%d:%02d", s / 60, s % 60);
}

// Rebuilds Now Playing's texts when the track, title, time or mode changes. Returns 1 if any did.
static int np_update(void) {
	if (!lmusic_active()) return 0;
	int changed = 0;
	const ml_lib* ml = lmusic_lib();
	int al = lmusic_album(), tr = lmusic_track();
	if (al != np.album) {
		ltex_free(np.cover);
		const char* cover = lmusic_cover(al);
		np.cover = cover ? ltex_load(cover, 0) : NULL;
		ltex_free(np.sub);
		char text[300];
		const char* artist = lmusic_artist_of(al);
		snprintf(text, sizeof(text), "%s%s%s", artist, artist[0] ? "  ·  " : "", ml->albums[al].name);
		np.sub = ltex_text(LFONT_BODY, text, (int)NP_TEXT_W);
		np.album = al;
		changed = 1;
	} else if (!np.cover) {
		const char* cover = lmusic_cover(al);
		if (cover) {
			np.cover = ltex_load(cover, 0);
			changed = 1;
		}
	}
	const char* title = lmusic_title();
	if (tr != np.track || strcmp(title, np.shown_title)) {
		snprintf(np.shown_title, sizeof(np.shown_title), "%s", title);
		ltex_free(np.title);
		np.title = ltex_text_wrapped(LFONT_HERO, title, (int)NP_TEXT_W, 2, 0);
		ltex_free(np.next);
		char text[300];
		if (lmusic_is_book()) {
			int count, c = lmusic_chapter(&count);
			if (count > 1) snprintf(text, sizeof(text), "Chapter %d of %d", c + 1, count);
			else snprintf(text, sizeof(text), "Part %d of %d", tr + 1, ml->albums[al].n);
		} else if (lmusic_next_title()[0])
			snprintf(text, sizeof(text), "Next: %s", lmusic_next_title());
		else
			snprintf(text, sizeof(text), "Track %d of %d", tr + 1, ml->albums[al].n);
		np.next = ltex_text(LFONT_BODY, text, (int)NP_TEXT_W);
		np.track = tr;
		changed = 1;
	}
	int second = (int)lmusic_at();
	if (second != np.times_for) {
		char a[16], b[24];
		fmt_time(lmusic_at(), a, sizeof(a));
		b[0] = '-';
		fmt_time(lmusic_len() - lmusic_at(), b + 1, sizeof(b) - 1);
		ltex_free(np.times[0]);
		ltex_free(np.times[1]);
		np.times[0] = ltex_text(LFONT_SMALL, a, 0);
		np.times[1] = ltex_text(LFONT_SMALL, b, 0);
		np.times_for = second;
		changed = 1;
	}
	int mode = lmusic_is_book() ? 100 + (int)(lmusic_speed() * 100) : (int)lmusic_mode();
	if (mode != np.mode_for) {
		static const char* names[] = { "In order", "Repeat all", "Repeat one", "Shuffle" };
		char text[32];
		if (lmusic_is_book()) snprintf(text, sizeof(text), "%.2gx speed", lmusic_speed());
		else snprintf(text, sizeof(text), "%s", names[lmusic_mode() % MUQ_MODES]);
		ltex_free(np.mode);
		np.mode = ltex_text(LFONT_LABEL, text, 0);
		np.mode_for = mode;
		changed = 1;
	}
	if (np.hints_book != lmusic_is_book()) {
		np.hints_book = lmusic_is_book();
		ltex_free(np.hints);
		np.hints = ltex_text(LFONT_SMALL,
		                     np.hints_book ? "A  Play/Pause     L1 R1  Chapter     < >  10 s     Y  Speed     X  Lock     B  Close"
		                                   : "A  Play/Pause     L1 R1  Track     < >  10 s     Y  Mode     X  Lock     B  Close",
		                     0);
	}
	return changed;
}

static void draw_now_playing(void) {
	lgfx_draw_snapshot(1);
	float a = np.slide.x;
	const float dim[4] = { 0, 0, 0, 0.55f * a };
	lgfx_draw_gradient(0, 0, SCREEN_W, SCREEN_H, 0, dim, dim);
	float dy = (1 - a) * 40;

	float cx = MARGIN_X + 24, cy = 150 + dy;
	lgfx_draw_shadow(cx, cy, NP_COVER, NP_COVER, 16, 0.75f * a);
	if (np.cover) lgfx_draw_tex(np.cover, cx, cy, NP_COVER, NP_COVER, 22, a);
	else {
		Card fake = { .hue = album_hue(lmusic_album()) };
		float top[4], bottom[4];
		card_colors(&fake, top, bottom);
		top[3] = bottom[3] = a;
		lgfx_draw_gradient(cx, cy, NP_COVER, NP_COVER, 22, top, bottom);
	}

	float tx = NP_TEXT_X, y = 170 + dy;
	if (np.title) {
		lgfx_draw_tex(np.title, tx, y, np.title->w, np.title->h, 0, a);
		y += np.title->h + 8;
	}
	if (np.sub) {
		lgfx_draw_tex(np.sub, tx, y, np.sub->w, np.sub->h, 0, 0.75f * a);
		y += np.sub->h + 30;
	}
	// Progress
	float bar_y = fmaxf(y, 360 + dy), bar_w = NP_TEXT_W;
	float p = lmusic_len() > 0 ? fminf(1.0f, lmusic_at() / lmusic_len()) : 0;
	lgfx_draw_fill(tx, bar_y, bar_w, 6, 3, 1, 1, 1, 0.22f * a);
	lgfx_draw_fill(tx, bar_y, bar_w * p, 6, 3, 1, 1, 1, 0.95f * a);
	if (np.times[0]) lgfx_draw_tex(np.times[0], tx, bar_y + 12, np.times[0]->w, np.times[0]->h, 0, 0.7f * a);
	if (np.times[1])
		lgfx_draw_tex(np.times[1], tx + bar_w - np.times[1]->w, bar_y + 12, np.times[1]->w, np.times[1]->h, 0, 0.7f * a);

	float row_y = bar_y + 54;
	// Play state: a pause or play glyph drawn as shapes, so no font is needed.
	if (lmusic_playing()) {
		lgfx_draw_fill(tx, row_y, 8, 26, 2, 1, 1, 1, a);
		lgfx_draw_fill(tx + 14, row_y, 8, 26, 2, 1, 1, 1, a);
	} else {
		// A play triangle from one-pixel columns, narrowing to the tip.
		for (int i = 0; i < 22; i++) lgfx_draw_fill(tx + i, row_y + i * 0.59f, 1, 26 - i * 1.18f, 0, 1, 1, 1, a);
	}
	if (np.mode) {
		float mw = np.mode->w + 28, mh = np.mode->h + 12;
		lgfx_draw_fill(tx + 48, row_y - 4, mw, mh, mh * 0.5f, 1, 1, 1, 0.16f * a);
		lgfx_draw_tex(np.mode, tx + 62, row_y + 2, np.mode->w, np.mode->h, 0, 0.9f * a);
	}
	if (np.next) lgfx_draw_tex(np.next, tx, row_y + 52, np.next->w, np.next->h, 0, 0.65f * a);
	if (np.hints) lgfx_draw_tex(np.hints, (SCREEN_W - np.hints->w) * 0.5f, SCREEN_H - 46, np.hints->w, np.hints->h, 0, 0.6f * a);
}


// Three animated bars: shown next to the clock, and on the playing track, while music plays.
static void draw_eq_bars(float x, float y, float h, float alpha) {
	float t = SDL_GetTicks() / 1000.0f;
	int moving = lmusic_playing();
	for (int i = 0; i < 3; i++) {
		float k = moving ? 0.35f + 0.65f * fabsf(sinf(t * (3.1f + i * 1.7f) + i)) : 0.35f;
		float bh = h * k;
		lgfx_draw_fill(x + i * 7, y + h - bh, 4, bh, 2, 1, 1, 1, alpha);
	}
}

// Everything except the quick menu and overlays: the home screen or the library.
static void draw_scene(void) {
	if (lactivity_is_open()) {
		lactivity_draw();
		draw_status();
		return;
	}
	if (lfiles_is_open()) {
		lfiles_draw();
		draw_status();
		return;
	}
	lgfx_draw_wallpaper(wallpaper, parallax_x.x * 12, parallax_y.x * 8);

	// Darken the lower half so the rows read clearly over any wallpaper.
	const float clear[4] = { 0, 0, 0, 0 }, dark[4] = { 0, 0, 0, 0.45f };
	lgfx_draw_gradient(0, rows_top - 60, SCREEN_W, SCREEN_H - rows_top + 60, 0, clear, dark);

	if (alb.open) {
		draw_album();
	} else if (lib.open) {
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
	RadioStatus radio[RADIO_COUNT];
	int radio_version;
	Spring slide;   // 0 closed, 1 open
	LTex* labels[QM_COUNT];
	LTex* states[RADIO_COUNT];
	LTex* clock;
	LTex* battery;
	LTex* version;
	int closing;
} qm;

static int radio_lit(RadioState s) {
	return s == RADIO_TURNING_ON || s == RADIO_ON || s == RADIO_CONNECTED;
}

// Rebuilds the tile subtitles when the radio worker reports a change. Returns 1 if it did.
static int qm_update_states(int force) {
	RadioStatus now[RADIO_COUNT];
	int version = 0;
	for (int i = 0; i < RADIO_COUNT; i++) version = lradio_get(i, &now[i]);
	if (!force && version == qm.radio_version) return 0;
	qm.radio_version = version;
	for (int i = 0; i < RADIO_COUNT; i++) {
		qm.radio[i] = now[i];
		char text[96];
		switch (now[i].state) {
		case RADIO_UNAVAILABLE: snprintf(text, sizeof(text), "Not available"); break;
		case RADIO_OFF: snprintf(text, sizeof(text), "Off"); break;
		case RADIO_TURNING_ON: snprintf(text, sizeof(text), "Turning On..."); break;
		case RADIO_TURNING_OFF: snprintf(text, sizeof(text), "Turning Off..."); break;
		// A lit tile already says the radio is on, so the second line only adds the link state.
		case RADIO_ON: snprintf(text, sizeof(text), i == RADIO_WIFI ? "Not connected" : "On"); break;
		case RADIO_CONNECTED:
			if (now[i].detail[0]) snprintf(text, sizeof(text), "%s", now[i].detail);
			else snprintf(text, sizeof(text), "Connected");
			break;
		}
		ltex_free(qm.states[i]);
		qm.states[i] = ltex_text(LFONT_SMALL, text, (int)QM_TILE_W - 36);
	}
	return 1;
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
	ltex_free(qm.version);
	qm.version = ltex_text(LFONT_SMALL, BRAMLEY_NAME " " LAUNCHER_VERSION, 0);
	lradio_set_polling(1);
	qm_update_states(1);
	shine_t = 0.0f;
}

static void free_quick_menu(void) {
	lradio_set_polling(0);
	for (int i = 0; i < QM_COUNT; i++) ltex_free(qm.labels[i]);
	for (int i = 0; i < RADIO_COUNT; i++) ltex_free(qm.states[i]);
	ltex_free(qm.clock);
	ltex_free(qm.battery);
	ltex_free(qm.version);
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
	if (qm.version) lgfx_draw_tex(qm.version, px + 24, py + ph - 24 - qm.version->h, qm.version->w, qm.version->h, 0, 0.45f);

	for (int i = 0; i < QM_COUNT; i++) {
		float x, y, w = i <= QM_BLUETOOTH ? QM_TILE_W : QM_W - 48, h = i <= QM_BLUETOOTH ? QM_TILE_H : QM_ITEM_H;
		qm_item_pos(i, px, &x, &y);
		int focused = i == qm.focus;
		RadioState rs = i <= QM_BLUETOOTH ? qm.radio[i].state : RADIO_OFF;
		int on = i <= QM_BLUETOOTH && radio_lit(rs);
		// A radio that is switching pulses until the worker reports it done.
		int switching = rs == RADIO_TURNING_ON || rs == RADIO_TURNING_OFF;
		float pulse = switching ? 0.75f + 0.25f * sinf(SDL_GetTicks() / 160.0f) : 1.0f;
		float s = focused ? 1.08f : 1.0f;
		// Unfocused tiles step back a little so the focused one reads at a glance, even when
		// both radios are on and both tiles are blue.
		float dim = focused ? 1.0f : 0.72f;
		float cx = x + w * 0.5f, cy = y + h * 0.5f;
		w *= s;
		h *= s;
		x = cx - w * 0.5f;
		y = cy - h * 0.5f;
		if (focused) lgfx_draw_shadow(x, y, w, h, 8, 0.55f);
		if (on) lgfx_draw_fill(x, y, w, h, 18, 0.18f * dim, 0.48f * dim, 1.0f * dim, 0.95f * pulse);
		else lgfx_draw_fill(x, y, w, h, 18, 1, 1, 1, (focused ? 0.24f : 0.08f) * (switching ? pulse * 1.6f : 1.0f));
		if (focused) lgfx_draw_ring(x, y, w, h, 18, 3, 1, 1, 1, 0.95f);
		LTex* label = qm.labels[i];
		if (i <= QM_BLUETOOTH) {
			if (label) lgfx_draw_tex(label, x + 18, y + h - 58, label->w, label->h, 0, dim);
			LTex* state = qm.states[i];
			if (state) lgfx_draw_tex(state, x + 18, y + h - 32, state->w, state->h, 0, 0.75f * dim);
		} else if (label) {
			lgfx_draw_tex(label, x + 20, y + (h - label->h) * 0.5f, label->w, label->h, 0, dim);
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

///////////////////////////////////////
// Lock screen: music keeps playing with the screen off, safe in a pocket.
//
// While music plays, a short power press, holding L3 and R3 (FN1 and FN2) for a second, or X in
// Now Playing turns the screen off; NextUI's sleep is held off meanwhile, because it would stop
// the music. Dark, only power and the chord do anything: power shows the lock screen (clock,
// cover, track, A to play or pause), and power again or the chord unlocks. A long power press
// still powers off, after saving the music's place. The chord's arming (it locks once per hold,
// and only after both keys were let go) is adapted from TortOS v1.6.0 src/main.c lock_chord,
// MIT, Copyright (c) 2026 Eric Reinsmidt.

#define LOCK_SHOWN_MS 8000
#define LOCK_IDLE_SLEEP_MS 10000 // dark with no music: sleep properly after this

enum { LOCK_NONE, LOCK_DARK, LOCK_SHOWN };
static int lock_state, sleep_held, chord_armed = 1, just_woke;
static Uint32 lock_shown_at, power_down_at, chord_since, lock_idle_since;
static LTex* lock_clock;
static LTex* lock_hint;
static int lock_clock_minute = -1;

// Holds NextUI's sleep and auto-sleep off while music plays or the screen is locked, and gives
// them back otherwise.
static void update_sleep_hold(void) {
	int hold = lmusic_playing() || lock_state != LOCK_NONE;
	if (hold == sleep_held) return;
	sleep_held = hold;
	if (hold) {
		PWR_disableSleep();
		PWR_disableAutosleep();
	} else {
		if (HAS_POWER_BUTTON) PWR_enableSleep();
		PWR_enableAutosleep();
	}
}

static void before_power_change(void) {
	lmusic_save();
}

// NextUI slept (music had stopped); it lights the screen itself on wake.
static void after_sleep(void) {
	lock_state = LOCK_NONE;
	just_woke = 1;
	power_down_at = 0;
}

// While music plays the launcher handles the power button itself, before NextUI's PWR_update:
// with sleep disabled, PWR_update keeps a short press pending and powers off a second later
// (api.c, power_pressed_at is only cleared on sleep). So a short press returns 1 (lock), a press
// held for a second saves the music's place and powers off as NextUI would, and the button is
// then hidden from PWR_update. With no music, NextUI handles power as always.
static int power_tap(void) {
	if (!sleep_held) {
		power_down_at = 0;
		return 0;
	}
	Uint32 now = SDL_GetTicks();
	int tap = 0;
	if (PAD_justPressed(BTN_POWER) && !power_down_at) power_down_at = now ? now : 1;
	if (PAD_justReleased(BTN_POWER) && power_down_at) {
		tap = now - power_down_at < 1000;
		power_down_at = 0;
	}
	if (power_down_at && now - power_down_at >= 1000) {
		lmusic_save();
		system("gametimectl.elf stop_all");
		PWR_powerOff(0);
	}
	// Only the edges are hidden: PWR_update reads nothing else for power, and clearing is_pressed
	// too would make PAD_poll treat the key's repeat events as new presses (api.c PAD_poll), which
	// restarted the hold timer so a long press never powered off.
	pad.just_pressed &= ~BTN_POWER;
	pad.just_released &= ~BTN_POWER;
	return tap;
}

static int lock_chord(void) {
	int one = PAD_isPressed(BTN_FN1), two = PAD_isPressed(BTN_FN2);
	if (!one && !two) chord_armed = 1;
	if (!one || !two) {
		chord_since = 0;
		return 0;
	}
	if (!chord_since) chord_since = SDL_GetTicks();
	if (!chord_armed || SDL_GetTicks() - chord_since < 1000) return 0;
	chord_armed = 0;
	return 1;
}

static void lock_go_dark(void) {
	lock_idle_since = 0;
	// One black frame, so the lock screen never lights up showing a stale picture.
	lgfx_begin_frame();
	lgfx_end_frame();
	PLAT_enableBacklight(0);
	lock_state = LOCK_DARK;
	LOG_info("launcher lock: dark\n");
}

static void lock_now(void) {
	if (!lmusic_playing()) return;
	if (np.open) np.open = 0;
	if (qm.open) free_quick_menu();
	lock_go_dark();
}

static void lock_show(void) {
	lock_state = LOCK_SHOWN;
	lock_shown_at = SDL_GetTicks();
	LOG_info("launcher lock: shown\n");
	PLAT_enableBacklight(1);
}

static void unlock(void) {
	if (lock_state == LOCK_DARK) PLAT_enableBacklight(1);
	lock_state = LOCK_NONE;
	PAD_reset();
	LOG_info("launcher lock: unlocked\n");
}

static void draw_lock_screen(void) {
	np_update();
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	if (tm.tm_hour * 60 + tm.tm_min != lock_clock_minute) {
		lock_clock_minute = tm.tm_hour * 60 + tm.tm_min;
		char text[16];
		snprintf(text, sizeof(text), "%d:%02d", tm.tm_hour, tm.tm_min);
		ltex_free(lock_clock);
		lock_clock = ltex_text(LFONT_HERO, text, 0);
	}
	if (!lock_hint) lock_hint = ltex_text(LFONT_SMALL, "A  Play/Pause     L1 R1  Track     POWER  Unlock", 0);

	lgfx_begin_frame();
	lgfx_draw_wallpaper(wallpaper, 0, 0);
	const float shade[4] = { 0, 0, 0, 0.6f };
	lgfx_draw_gradient(0, 0, SCREEN_W, SCREEN_H, 0, shade, shade);
	if (lock_clock) lgfx_draw_tex(lock_clock, (SCREEN_W - lock_clock->w) * 0.5f, 40, lock_clock->w, lock_clock->h, 0, 0.95f);

	float size = 280, cx = (SCREEN_W - size) * 0.5f, cy = 128;
	lgfx_draw_shadow(cx, cy, size, size, 14, 0.7f);
	if (np.cover) lgfx_draw_tex(np.cover, cx, cy, size, size, 22, 1);
	else {
		Card fake = { .hue = album_hue(lmusic_album()) };
		float top[4], bottom[4];
		card_colors(&fake, top, bottom);
		lgfx_draw_gradient(cx, cy, size, size, 22, top, bottom);
	}
	float y = cy + size + 26;
	if (np.title) {
		lgfx_draw_tex(np.title, (SCREEN_W - np.title->w) * 0.5f, y, np.title->w, np.title->h, 0, 1);
		y += np.title->h + 4;
	}
	if (np.sub) {
		lgfx_draw_tex(np.sub, (SCREEN_W - np.sub->w) * 0.5f, y, np.sub->w, np.sub->h, 0, 0.75f);
		y += np.sub->h + 22;
	}
	float bar_w = 440, bx = (SCREEN_W - bar_w) * 0.5f;
	float p = lmusic_len() > 0 ? fminf(1.0f, lmusic_at() / lmusic_len()) : 0;
	lgfx_draw_fill(bx, y, bar_w, 6, 3, 1, 1, 1, 0.22f);
	lgfx_draw_fill(bx, y, bar_w * p, 6, 3, 1, 1, 1, 0.95f);
	if (!lmusic_playing()) {
		// Paused: a play triangle under the progress bar.
		for (int i = 0; i < 22; i++) lgfx_draw_fill(SCREEN_W * 0.5f - 8 + i, y + 24 + i * 0.59f, 1, 26 - i * 1.18f, 0, 1, 1, 1, 0.9f);
	}
	if (lock_hint) lgfx_draw_tex(lock_hint, (SCREEN_W - lock_hint->w) * 0.5f, SCREEN_H - 44, lock_hint->w, lock_hint->h, 0, 0.6f);
	lgfx_end_frame();
}

// Input while locked. Returns 1 when the lock was lifted.
static int lock_input(int tap) {
	if (lock_chord()) {
		unlock();
		return 1;
	}
	if (lock_state == LOCK_DARK) {
		if (tap) lock_show();
		else if (lmusic_playing()) lock_idle_since = 0;
		else if (!lock_idle_since) lock_idle_since = SDL_GetTicks();
		else if (SDL_GetTicks() - lock_idle_since > LOCK_IDLE_SLEEP_MS) {
			// The music ended or was paused in the pocket: sleep for real, so the battery lasts.
			lock_idle_since = 0;
			lmusic_save();
			PLAT_enableBacklight(1);
			lock_state = LOCK_NONE;
			PWR_sleep();
			after_sleep();
			return 1;
		}
		return 0;
	}
	if (tap) {
		unlock();
		return 1;
	}
	int touched = 1;
	if (PAD_justPressed(BTN_A)) lmusic_toggle();
	else if (PAD_justRepeated(BTN_R1)) lmusic_next();
	else if (PAD_justRepeated(BTN_L1)) lmusic_prev();
	else touched = 0;
	if (touched) lock_shown_at = SDL_GetTicks();
	if (SDL_GetTicks() - lock_shown_at > LOCK_SHOWN_MS) lock_go_dark();
	return 0;
}

///////////////////////////////////////
// Low battery. NextUI only blinks the LED, and the device dies abruptly when the battery runs
// out. The launcher warns at 5 percent and at 2 percent saves the music's place and powers off
// cleanly. A single low reading can be wrong, so power-off waits for two at least 5 s apart.

#define LOW_BATTERY_WARN 5
#define LOW_BATTERY_OFF 2

static void check_battery(void) {
	static Uint32 next_check, low_since;
	static int warned;
	Uint32 now = SDL_GetTicks();
	if (now < next_check) return;
	next_check = now + 5000;
	int pct = PWR_getBattery(), charging = PWR_isCharging();
	// Development: LAUNCHER_DEBUG_BATTERY=<percent> simulates a battery that is not charging.
	const char* fake = getenv("LAUNCHER_DEBUG_BATTERY");
	if (fake) {
		pct = atoi(fake);
		charging = 0;
	}
	if (charging || pct < 0) {
		low_since = 0;
		warned = 0;
		return;
	}
	if (pct <= LOW_BATTERY_WARN && !warned) {
		show_toast_for("Battery low  ·  plug in soon", 4);
		warned = 1;
	}
	if (pct > LOW_BATTERY_OFF) {
		low_since = 0;
		return;
	}
	if (!low_since) {
		low_since = now;
		return;
	}
	if (now - low_since < 5000) return;
	LOG_info("launcher: battery at %d%%, powering off\n", pct);
	lmusic_save();
	if (lock_state == LOCK_DARK) PLAT_enableBacklight(1); // so the power-off message is seen
	system("gametimectl.elf stop_all");
	PWR_powerOff(0);
}

// The start-up splash after a cold boot: the boot logo's mark, in the same place, over black, with a
// thin bar that fills while the home screen gets ready, then fades into it, as Apple's devices
// start. /tmp is empty after every boot, so a flag there tells a cold boot from a return to home.
#define SPLASH_FLAG "/tmp/bramley_booted"
static struct {
	int on;
	Uint32 start, fade_at;
	float progress;
	LTex* mark;
} splash;

static void splash_begin(void) {
	if (exists(SPLASH_FLAG)) return;
	touch(SPLASH_FLAG);
	splash.mark = ltex_load(BRAMLEY_RES_DIR "/brand/mark-150.png", 0);
	if (!splash.mark) return;
	splash.on = 1;
	splash.start = SDL_GetTicks();
}

// Moves the bar on. It fills over about a second, waits at 85% until the wallpaper is in, then
// finishes and fades out over 0.4 s. Returns 1 while it is still showing.
static int splash_update(void) {
	if (!splash.on) return 0;
	Uint32 now = SDL_GetTicks();
	float t = (now - splash.start) / 1000.0f;
	int ready = !wall_thread || SDL_AtomicGet(&wall_done) || t > 3;
	float goal = fminf(1.0f, t / 1.0f) * (ready ? 1.0f : 0.85f);
	if (goal > splash.progress) splash.progress = goal;
	if (splash.progress >= 1.0f && !splash.fade_at) splash.fade_at = now + 150;
	if (splash.fade_at && now > splash.fade_at + 400) {
		splash.on = 0;
		ltex_free(splash.mark);
		splash.mark = NULL;
	}
	return 1;
}

static void draw_splash(void) {
	float a = 1;
	if (splash.fade_at && SDL_GetTicks() > splash.fade_at) a = fmaxf(0, 1 - (SDL_GetTicks() - splash.fade_at) / 400.0f);
	lgfx_draw_fill(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, 0, a);
	// the boot logo's mark: 150 px, centred 20 px above the middle (docs/brand)
	lgfx_draw_tex(splash.mark, (SCREEN_W - 150) / 2, (SCREEN_H - 150) / 2 - 20, 150, 150, 0, a);
	float bw = 200, bx = (SCREEN_W - bw) * 0.5f, by = SCREEN_H * 0.5f + 110;
	lgfx_draw_fill(bx, by, bw, 6, 3, 0.24f, 0.24f, 0.25f, a);
	lgfx_draw_fill(bx, by, fmaxf(6, bw * splash.progress), 6, 3, 0.92f, 0.92f, 0.92f, a);
}

// How much of the boot splash still shows, 1 to 0.
static float splash_alpha(void) {
	if (!splash.on) return 0;
	if (splash.fade_at && SDL_GetTicks() > splash.fade_at) return fmaxf(0, 1 - (SDL_GetTicks() - splash.fade_at) / 400.0f);
	return 1;
}

static void draw_frame(int show_setting) {
	lgfx_begin_frame();
	if (lsetup_covers()) lsetup_draw(splash_alpha());
	else if (np.open) draw_now_playing();
	else if (qm.open) draw_quick_menu();
	else {
		draw_scene();
		if (lsetup_is_open()) lsetup_draw(0); // the Home card
	}
	draw_setting_overlay(show_setting);
	if (splash.on) draw_splash();

	if (toast && SDL_GetTicks() / 1000.0 < toast_until) {
		float w = toast->w + 48, h = toast->h + 24, x = (SCREEN_W - w) * 0.5f, y = SCREEN_H - h - 40;
		lgfx_draw_fill(x, y, w, h, h * 0.5f, 0.1f, 0.1f, 0.12f, 0.85f);
		lgfx_draw_tex(toast, x + 24, y + 12, toast->w, toast->h, 0, 1);
	}
	debug_drawing = 1;
	debug_step();
	debug_drawing = 0;
	lgfx_end_frame();
}

// For screens that run their own loop (Files' keyboard): keeps the development key script going.
void launcher_debug_tick(void) {
	debug_drawing = 1;
	debug_step();
	debug_drawing = 0;
}

static double now_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char* argv[]) {
	if (ldata_auto_resume()) return 0;
	// Startup cost, step by step, from a monotonic clock (SDL's starts only in GFX_init).
	double t[10];
	int nt = 0;
	t[nt++] = now_ms();

	LOG_info("Bramley launcher " LAUNCHER_VERSION "\n");
	// Saved state moved from .launcher to .bramley with the rename; move an old card's state
	// across once. A rename within the card keeps every file, so nothing is copied or lost.
	if (!exists(BRAMLEY_STATE_DIR) && exists(BRAMLEY_OLD_STATE_DIR)) {
		if (rename(BRAMLEY_OLD_STATE_DIR, BRAMLEY_STATE_DIR) == 0)
			LOG_info("moved saved state from %s to %s\n", BRAMLEY_OLD_STATE_DIR, BRAMLEY_STATE_DIR);
		else
			LOG_error("could not move %s to %s\n", BRAMLEY_OLD_STATE_DIR, BRAMLEY_STATE_DIR);
	}
	InitSettings();
	t[nt++] = now_ms();
	GFX_keepScreenOnInit = 1;
	screen = GFX_init(MODE_MAIN);
	apply_bramley_palette();
	t[nt++] = now_ms();
	PAD_init();
	VIB_init();
	PWR_init();
	if (!HAS_POWER_BUTTON) PWR_disableSleep();
	// after SDL, whose own crash handler would otherwise catch the signal first and hide the stack
	signal(SIGSEGV, crash_report);
	signal(SIGBUS, crash_report);
	signal(SIGABRT, crash_report);
	signal(SIGFPE, crash_report);
	t[nt++] = now_ms();
	system("gametimectl.elf stop_all &"); // in the background: nothing here waits on it
	t[nt++] = now_ms();
	GFX_setVsync(VSYNC_STRICT);
	PWR_setCPUSpeed(CPU_SPEED_AUTO);
	PAD_reset();

	lgfx_init(SCREEN_W, SCREEN_H);
	PWR_drawPowerOffScreen = power_screen; // shutting down from the launcher draws with the kit
	splash_begin();
	lsetup_begin(); // first start of a new card
	t[nt++] = now_ms();
	lradio_start();
	lmusic_init();
	t[nt++] = now_ms();
	wallpaper = load_wallpaper();
	t[nt++] = now_ms();

	row_count = ldata_load_rows(rows, MAX_ROWS);
	t[nt++] = now_ms();
	layout_rows();
	if (exists(GAME_SWITCHER_PERSIST_PATH)) {
		// The in-game switcher brings the player to Continue Playing, which is row 0 when it exists.
		unlink(GAME_SWITCHER_PERSIST_PATH);
		focus_row = 0;
	}
	if (row_count == 0) show_toast("No games or Paks found");
	restore_focus();
	if (lmusic_take_game_pause()) show_toast_for("Music paused  ·  SELECT to resume", 5);

	int quit = 0, dirty = 1, show_setting = 0;
	Uint32 last = SDL_GetTicks();
	while (!quit) {
		Uint32 now = SDL_GetTicks();
		float dt = (now - last) / 1000.0f;
		if (dt > 0.05f) dt = 0.05f;
		last = now;

		PAD_poll();
		int tap = power_tap();
		PWR_update(&dirty, &show_setting, before_power_change, after_sleep);
		update_sleep_hold();
		check_battery();
		if (just_woke) {
			just_woke = 0;
			tap = 0;
			dirty = 1;
		}

		// Development: a new build is installed by creating this file, so the launcher exits cleanly
		// and MinUI.pak's loop starts the new binary without killing the process.
		if (exists("/tmp/launcher_reload")) {
			unlink("/tmp/launcher_reload");
			break;
		}

		// Software Update: the quiet daily check while Wi-Fi is on, and the dot on Settings
		static Uint32 update_at;
		if (!update_at || now - update_at > 30000) {
			update_at = now | 1;
			if (!lfiles_is_open() && !lactivity_is_open()) update_tick();
			int waiting = update_badge();
			if (waiting != update_waiting) {
				update_waiting = waiting;
				dirty = 1;
			}
		}

		if (lock_state != LOCK_NONE) {
			lmusic_update();
			if (lock_input(tap)) dirty = 1;
			else {
				if (lock_state == LOCK_SHOWN) draw_lock_screen();
				else SDL_Delay(30);
				debug_step();
				continue;
			}
		}
		if (lock_chord() || tap) lock_now();
		if (lock_state != LOCK_NONE) continue;

		// FN1 and FN2 run their shortcut on release, and not when they were held together as the
		// lock chord.
		static int fn_chorded;
		if (PAD_isPressed(BTN_FN1) && PAD_isPressed(BTN_FN2)) fn_chorded = 1;
		if (!show_setting && !fn_chorded) {
			if (PAD_justReleased(BTN_FN1)) quit = ldata_run_fn_action(0);
			else if (PAD_justReleased(BTN_FN2)) quit = ldata_run_fn_action(1);
		}
		if (!show_setting && PAD_justPressed(BTN_FN3)) quit = ldata_run_fn_action(2);
		if (!PAD_isPressed(BTN_FN1) && !PAD_isPressed(BTN_FN2)) fn_chorded = 0;

		if (np.open && !np.closing && !quit) {
			if (PAD_justPressed(BTN_A)) lmusic_toggle();
			else if (PAD_justRepeated(BTN_R1)) lmusic_next();
			else if (PAD_justRepeated(BTN_L1)) lmusic_prev();
			else if (PAD_justRepeated(BTN_RIGHT)) lmusic_seek_by(10);
			else if (PAD_justRepeated(BTN_LEFT)) lmusic_seek_by(-10);
			else if (PAD_justPressed(BTN_Y)) {
				if (lmusic_is_book()) lmusic_cycle_speed();
				else lmusic_cycle_mode();
			} else if (PAD_justPressed(BTN_X)) lock_now();
			else if (PAD_justPressed(BTN_B) || PAD_justPressed(BTN_SELECT)) np.closing = 1;
		} else if (np.open) {
			// Closing: ignore input until Now Playing has slid away.
		} else if (qm.open && !qm.closing && !quit) {
			if (PAD_justRepeated(BTN_RIGHT)) qm_move(1, 0);
			else if (PAD_justRepeated(BTN_LEFT)) qm_move(-1, 0);
			else if (PAD_justRepeated(BTN_DOWN)) qm_move(0, 1);
			else if (PAD_justRepeated(BTN_UP)) qm_move(0, -1);
			else if (PAD_justPressed(BTN_B) || PAD_tappedMenu(SDL_GetTicks())) qm.closing = 1;
			else if (PAD_justPressed(BTN_A)) {
				switch (qm.focus) {
				case QM_WIFI:
					lradio_toggle(RADIO_WIFI);
					break;
				case QM_BLUETOOTH:
					lradio_toggle(RADIO_BLUETOOTH);
					break;
				case QM_SETTINGS:
					free_quick_menu();
					qm.open = 0;
					open_settings();
					dirty = 1;
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
		} else if (lsetup_is_open() && !quit) {
			if (lsetup_input()) dirty = 1;
		} else if (!quit && !show_setting && PAD_justPressed(BTN_SELECT)) {
			open_now_playing();
		} else if (!quit && !show_setting && PAD_tappedMenu(SDL_GetTicks())) {
			open_quick_menu();
		} else if (lfiles_is_open() && !quit) {
			Card play;
			int r = lfiles_input(&play);
			if (r == FILES_CLOSE) lfiles_close();
			else if (r == FILES_PLAY) {
				lfiles_close();
				ldata_launch(&play, 0);
				quit = 1;
			}
			if (r != FILES_NONE) dirty = 1;
		} else if (lactivity_is_open() && !quit) {
			Card play;
			int r = lactivity_input(&play);
			if (r == ACT_CLOSE) lactivity_close();
			else if (r == ACT_PLAY) {
				lactivity_close();
				ldata_launch(&play, 0);
				quit = 1;
			}
			if (r != ACT_NONE) dirty = 1;
		} else if (alb.open && !quit) {
			if (PAD_justRepeated(BTN_DOWN) && alb.focus < alb.count - 1) alb.focus++, shine_t = 0;
			else if (PAD_justRepeated(BTN_UP) && alb.focus > 0) alb.focus--, shine_t = 0;
			else if (PAD_justPressed(BTN_B)) close_album();
			else if (PAD_justPressed(BTN_A)) {
				lmusic_play(alb.album, alb.focus);
				open_now_playing();
			}
		} else if (lib.open && lib.music && !quit) {
			if (PAD_justRepeated(BTN_RIGHT)) lib_move(1, 0);
			else if (PAD_justRepeated(BTN_LEFT)) lib_move(-1, 0);
			else if (PAD_justRepeated(BTN_DOWN)) lib_move(0, 1);
			else if (PAD_justRepeated(BTN_UP)) lib_move(0, -1);
			else if (PAD_justPressed(BTN_R1) || PAD_justPressed(BTN_L1)) open_music(!lib.books);
			else if (PAD_justPressed(BTN_B)) {
				close_library();
				shelf.row = -1;
			} else if (PAD_justPressed(BTN_A) && lib.count) {
				Card* album = &lib.games[lib.focus];
				if (lib.books) {
					lmusic_play(album->index, -1); // a book resumes at its bookmark
					open_now_playing();
				} else
					open_album(album->index);
			}
		} else if (lib.open && !quit) {
			Card* game = &lib.games[lib.focus];
			if (PAD_justRepeated(BTN_RIGHT)) lib_move(1, 0);
			else if (PAD_justRepeated(BTN_LEFT)) lib_move(-1, 0);
			else if (PAD_justRepeated(BTN_DOWN)) lib_move(0, 1);
			else if (PAD_justRepeated(BTN_UP)) lib_move(0, -1);
			else if (PAD_justRepeated(BTN_R1) && !lib.collection) jump_letter(1);
			else if (PAD_justRepeated(BTN_L1) && !lib.collection) jump_letter(-1);
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
				if (card->kind == CARD_CONSOLE || card->kind == CARD_COLLECTION) open_library(card, 0);
				else if (card->kind == CARD_MUSIC) open_music(0);
				else if (is_settings(card)) {
					open_settings();
					dirty = 1;
				} else if (is_activity(card)) {
					lactivity_open();
					dirty = 1;
				} else if (is_files(card)) {
					lfiles_open();
					dirty = 1;
				} else {
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

		if (wallpaper_arrived()) dirty = 1;
		int animating = lmusic_update();
		if (np.open) {
			animating |= spring_step(&np.slide, np.closing ? 0.0f : 1.0f, dt);
			animating |= np_update();
			if (np.closing && np.slide.x < 0.02f) {
				np.open = 0;
				dirty = 1;
				if (back_to_settings) {
					back_to_settings = 0;
					open_settings();
				}
			}
		} else if (lactivity_is_open()) {
			animating |= lactivity_update();
		} else if (lfiles_is_open()) {
			animating |= lfiles_update();
		} else if (alb.open) {
			animating |= spring_step(&alb.scroll, alb_scroll_target(), dt);
			animating |= spring_step(&alb.fade, 1.0f, dt);
			if (shine_t < 1.0f) {
				shine_t = fminf(1.0f, shine_t + dt / SHINE_SECONDS);
				animating = 1;
			}
		} else if (qm.open) {
			animating |= spring_step(&qm.slide, qm.closing ? 0.0f : 1.0f, dt);
			animating |= qm_update_states(0);
			for (int i = 0; i < RADIO_COUNT; i++)
				if (qm.radio[i].state == RADIO_TURNING_ON || qm.radio[i].state == RADIO_TURNING_OFF) animating = 1;
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
		animating |= splash_update();
		animating |= lsetup_update();
		if (toast && SDL_GetTicks() / 1000.0 < toast_until + 0.1) animating = 1;

		// The playing indicator moves too, but 10 times a second is enough for three bars.
		static Uint32 last_draw;
		if (!animating && lmusic_playing() && !np.open && SDL_GetTicks() - last_draw >= 100) animating = 1;

		// Draw only while something moves, so the GPU and battery rest on a still screen.
		if (animating || dirty || show_setting || debug_shot_wanted) {
			last_draw = SDL_GetTicks();
			draw_frame(show_setting);
			static int first_frame = 1;
			if (first_frame) {
				// Startup cost, step by step: the launcher runs again after every game and Pak.
				LOG_info("launcher startup: settings %.0f, GFX_init %.0f, input+power %.0f, gametimectl %.0f, "
				         "lgfx_init %.0f, music %.0f, wallpaper %.0f, rows %.0f, first frame at %.0f ms\n",
				         t[1] - t[0], t[2] - t[1], t[3] - t[2], t[4] - t[3], t[5] - t[4], t[6] - t[5], t[7] - t[6],
				         t[8] - t[7], now_ms() - t[0]);
				first_frame = 0;
				lgfx_warm_up(); // after the first frame is on screen
			}
			dirty = 0;
		} else {
			debug_step();
			SDL_Delay(16);
		}
	}

	if (lib.open) close_library();
	lactivity_close();
	lfiles_close();
	free_quick_menu();
	if (alb.open) close_album();
	np_free_texts();
	lmusic_quit();
	lradio_stop();
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
	// Not GFX_quit(): its PLAT_quitVideo() blanks the screen and then zeroes the whole 64 MB
	// framebuffer, which took 450 ms on every exit and left the screen black while the next
	// program started. The system frees the GL context when we exit, and the last frame stays on
	// screen until the next program draws. CFG_quit() is the one part of GFX_quit() that saves.
	CFG_quit();
	QuitSettings();
	return 0;
}
