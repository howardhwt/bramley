// Activity: time played, like iOS Screen Time. See lactivity.h.
#include "lactivity.h"

#include <math.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "lgfx.h"
#include "lwallpaper.h"

#define W 1024
#define H 768
#define GAMETIME_LOG_FILE SHARED_USERDATA_PATH "/game_logs.sqlite"
#define MAX_GAMES 256
#define MAX_SESSIONS 8192
#define COLORS 5   // the most played games get their own colour; the rest share grey
#define ROWS_SHOWN 3
// Sessions logged before the clock was set start in 1970; they count for totals, not for days.
#define CLOCK_SET 1577836800 // 2020-01-01

typedef struct {
	char name[256], path[512], console[128], art_path[512];
	LTex* art;
	int art_tried;
	int total, plays;   // all time
	time_t first, last; // 0 if unknown
	int period;         // seconds in the week or day shown
	int color;          // 0..COLORS-1, or COLORS for grey
} Game;

typedef struct {
	int game;
	time_t start;
	int secs;
} Session;

static struct {
	int open;
	int day;  // 0 week, 1 day
	int detail; // showing one game
	Game* games;
	int count;
	Session* sessions;
	int nsessions;
	int order[MAX_GAMES]; // games played in the period, most first
	int ranked;
	int focus, top;
	int buckets;               // 7 days or 24 hours
	int bucket[24][COLORS + 1]; // seconds per bucket and colour
	int total, before;          // seconds in this period and the one before
	LTex* backdrop;
} act;

static const float palette[COLORS + 1][3] = {
	{ 0.04f, 0.52f, 1.00f }, { 0.20f, 0.78f, 0.35f }, { 1.00f, 0.62f, 0.04f },
	{ 1.00f, 0.22f, 0.37f }, { 0.75f, 0.35f, 0.95f }, { 0.56f, 0.56f, 0.58f },
};

// Text textures, kept while Activity is open.
#define TEXTS 160
static struct {
	int font;
	char s[160];
	LTex* tex;
} texts[TEXTS];
static int ntexts;

static LTex* text_tex(int font, const char* s) {
	for (int i = 0; i < ntexts; i++)
		if (texts[i].font == font && !strcmp(texts[i].s, s)) return texts[i].tex;
	if (ntexts == TEXTS) {
		for (int i = 0; i < ntexts; i++) ltex_free(texts[i].tex);
		ntexts = 0;
	}
	texts[ntexts].font = font;
	snprintf(texts[ntexts].s, sizeof(texts[ntexts].s), "%s", s);
	texts[ntexts].tex = ltex_text(font, s, 0);
	return texts[ntexts++].tex;
}

// Draws text; align 0 left, 1 right, 2 centred. Returns its width.
static float text(int font, const char* s, float x, float y, float alpha, int align) {
	LTex* t = text_tex(font, s);
	if (!t) return 0;
	float tx = align == 1 ? x - t->w : align == 2 ? x - t->w * 0.5f : x;
	lgfx_draw_tex(t, tx, y, t->w, t->h, 0, alpha);
	return t->w;
}

static void hours_minutes(int secs, char* out, size_t n) {
	int m = (secs + 30) / 60;
	if (m >= 60) snprintf(out, n, "%d h %d min", m / 60, m % 60);
	else if (m > 0) snprintf(out, n, "%d min", m);
	else snprintf(out, n, secs > 0 ? "Under a minute" : "0 min");
}

static time_t midnight(time_t t) {
	struct tm tm;
	localtime_r(&t, &tm);
	tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
	tm.tm_isdst = -1;
	return mktime(&tm);
}

static void when(time_t t, char* out, size_t n) {
	if (!t) {
		snprintf(out, n, "\xE2\x80\x94");
		return;
	}
	time_t today = midnight(time(NULL));
	if (t >= today) snprintf(out, n, "Today");
	else if (t >= today - 86400) snprintf(out, n, "Yesterday");
	else {
		struct tm tm;
		localtime_r(&t, &tm);
		strftime(out, n, "%e %b %Y", &tm);
		if (out[0] == ' ') memmove(out, out + 1, strlen(out));
	}
}

// "Game Boy Advance (GBA)" -> "Game Boy Advance"
static void console_of(const char* rel, char* out, size_t n) {
	snprintf(out, n, "%s", rel);
	char* slash = strchr(out, '/');
	if (slash) *slash = '\0';
	char* paren = strrchr(out, '(');
	if (paren && paren > out) {
		*paren = '\0';
		size_t l = strlen(out);
		while (l && out[l - 1] == ' ') out[--l] = '\0';
	}
}

static void load_log(void) {
	sqlite3* db;
	if (sqlite3_open_v2(GAMETIME_LOG_FILE, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return;
	}
	int ids[MAX_GAMES];
	sqlite3_stmt* st;
	if (sqlite3_prepare_v2(db, "SELECT id, name, file_path FROM rom;", -1, &st, NULL) == SQLITE_OK) {
		while (sqlite3_step(st) == SQLITE_ROW && act.count < MAX_GAMES) {
			const char* name = (const char*)sqlite3_column_text(st, 1);
			const char* rel = (const char*)sqlite3_column_text(st, 2);
			if (!rel || !rel[0]) continue; // a game the log could not place
			Game* g = &act.games[act.count];
			memset(g, 0, sizeof(*g));
			ids[act.count] = sqlite3_column_int(st, 0);
			snprintf(g->path, sizeof(g->path), "%s/%s", ROMS_PATH, rel);
			if (name && name[0]) snprintf(g->name, sizeof(g->name), "%s", name);
			else getDisplayName(g->path, g->name);
			console_of(rel, g->console, sizeof(g->console));
			act.count++;
		}
		sqlite3_finalize(st);
	}
	if (sqlite3_prepare_v2(db, "SELECT rom_id, play_time, created_at FROM play_activity WHERE play_time IS NOT NULL;", -1, &st, NULL) == SQLITE_OK) {
		while (sqlite3_step(st) == SQLITE_ROW && act.nsessions < MAX_SESSIONS) {
			int id = sqlite3_column_int(st, 0), g = -1;
			for (int i = 0; i < act.count; i++)
				if (ids[i] == id) {
					g = i;
					break;
				}
			if (g < 0) continue;
			Session* s = &act.sessions[act.nsessions++];
			s->game = g;
			s->secs = sqlite3_column_int(st, 1);
			s->start = (time_t)sqlite3_column_int64(st, 2);
			Game* gm = &act.games[g];
			gm->total += s->secs;
			gm->plays++;
			if (s->start >= CLOCK_SET) {
				if (!gm->first || s->start < gm->first) gm->first = s->start;
				if (s->start + s->secs > gm->last) gm->last = s->start + s->secs;
			}
		}
		sqlite3_finalize(st);
	}
	sqlite3_close(db);
}

// Seconds of a session inside [from, to).
static int overlap(const Session* s, time_t from, time_t to) {
	time_t a = s->start > from ? s->start : from, b = s->start + s->secs < to ? s->start + s->secs : to;
	return b > a ? (int)(b - a) : 0;
}

// Works out the period shown: the last 7 days ending today, or today by the hour.
static void tally(void) {
	time_t today = midnight(time(NULL));
	time_t from = act.day ? today : today - 6 * 86400, to = act.day ? today + 86400 : today + 86400;
	time_t prev = act.day ? today - 86400 : from - 7 * 86400;
	act.buckets = act.day ? 24 : 7;
	memset(act.bucket, 0, sizeof(act.bucket));
	act.total = act.before = 0;
	for (int i = 0; i < act.count; i++) act.games[i].period = 0;
	for (int i = 0; i < act.nsessions; i++) {
		Session* s = &act.sessions[i];
		if (s->start < CLOCK_SET) continue;
		act.games[s->game].period += overlap(s, from, to);
		act.before += overlap(s, prev, from);
	}
	act.ranked = 0;
	for (int i = 0; i < act.count; i++)
		if (act.games[i].period > 0) act.order[act.ranked++] = i;
	for (int i = 1; i < act.ranked; i++) // most played first
		for (int j = i; j > 0 && act.games[act.order[j]].period > act.games[act.order[j - 1]].period; j--) {
			int t = act.order[j];
			act.order[j] = act.order[j - 1];
			act.order[j - 1] = t;
		}
	for (int i = 0; i < act.nsessions; i++) {
		Session* s = &act.sessions[i];
		if (s->start < CLOCK_SET) continue;
		for (int b = 0; b < act.buckets; b++) {
			time_t bf = act.day ? from + b * 3600 : from + b * 86400;
			// days by midnight, so a change of summer time still lands on the right day
			time_t bt = act.day ? bf + 3600 : midnight(bf + 86400 + 7200);
			if (!act.day) bf = midnight(bf + 7200);
			int sec = overlap(s, bf, bt);
			act.bucket[b][act.games[s->game].color] += sec;
			act.total += sec;
		}
	}
	act.focus = act.top = 0;
}

void lactivity_open(void) {
	memset(&act, 0, sizeof(act));
	act.games = calloc(MAX_GAMES, sizeof(Game));
	act.sessions = calloc(MAX_SESSIONS, sizeof(Session));
	if (!act.games || !act.sessions) {
		free(act.games);
		free(act.sessions);
		return;
	}
	act.open = 1;
	load_log();
	// The five most played of all time keep their own colour in both views; the rest are grey.
	for (int i = 0; i < act.count; i++) {
		int above = 0;
		for (int j = 0; j < act.count; j++)
			if (act.games[j].total > act.games[i].total || (act.games[j].total == act.games[i].total && j < i)) above++;
		act.games[i].color = above < COLORS ? above : COLORS;
	}
	tally();
	Wallpaper w;
	wallpaper_current(&w);
	if (w.blur[0]) act.backdrop = ltex_load(w.blur, 0);
}

void lactivity_close(void) {
	if (!act.open) return;
	for (int i = 0; i < act.count; i++) ltex_free(act.games[i].art);
	free(act.games);
	free(act.sessions);
	ltex_free(act.backdrop);
	for (int i = 0; i < ntexts; i++) ltex_free(texts[i].tex);
	ntexts = 0;
	memset(&act, 0, sizeof(act));
}

int lactivity_is_open(void) {
	return act.open;
}

int lactivity_input(Card* play) {
	if (act.detail) {
		if (PAD_justPressed(BTN_B)) {
			act.detail = 0;
			return ACT_REDRAW;
		}
		if (PAD_justPressed(BTN_A)) {
			Game* g = &act.games[act.order[act.focus]];
			if (!exists(g->path)) return ACT_NONE;
			memset(play, 0, sizeof(*play));
			play->kind = CARD_GAME;
			snprintf(play->path, sizeof(play->path), "%s", g->path);
			snprintf(play->name, sizeof(play->name), "%s", g->name);
			return ACT_PLAY;
		}
		return ACT_NONE;
	}
	if (PAD_justPressed(BTN_B)) return ACT_CLOSE;
	if (PAD_justPressed(BTN_L1) || PAD_justPressed(BTN_R1)) {
		act.day = !act.day;
		tally();
		return ACT_REDRAW;
	}
	if (PAD_justRepeated(BTN_DOWN) && act.focus < act.ranked - 1) {
		act.focus++;
		if (act.focus >= act.top + ROWS_SHOWN) act.top = act.focus - ROWS_SHOWN + 1;
		return ACT_REDRAW;
	}
	if (PAD_justRepeated(BTN_UP) && act.focus > 0) {
		act.focus--;
		if (act.focus < act.top) act.top = act.focus;
		return ACT_REDRAW;
	}
	if (PAD_justPressed(BTN_A) && act.ranked) {
		act.detail = 1;
		return ACT_REDRAW;
	}
	return ACT_NONE;
}

static void art_rgb(const Game* g, float* r, float* gr, float* b) {
	const float* c = palette[g->color < COLORS ? g->color : COLORS];
	*r = c[0], *gr = c[1], *b = c[2];
}

static void find_art(Game* g) {
	g->art_tried = 1;
	char dir[512], base[256], art[1024];
	snprintf(dir, sizeof(dir), "%s", g->path);
	char* slash = strrchr(dir, '/');
	if (!slash) return;
	snprintf(base, sizeof(base), "%s", slash + 1);
	*slash = '\0';
	char* dot = strrchr(base, '.');
	if (dot) *dot = '\0';
	snprintf(art, sizeof(art), "%s/.media/%s.png", dir, base);
	if (exists(art)) g->art = ltex_load(art, 0);
}

int lactivity_update(void) {
	if (!act.open) return 0;
	// one image a frame, for the games on screen
	int want[ROWS_SHOWN + 1], n = 0;
	if (act.detail) want[n++] = act.order[act.focus];
	else
		for (int i = act.top; i < act.ranked && i < act.top + ROWS_SHOWN; i++) want[n++] = act.order[i];
	for (int i = 0; i < n; i++) {
		Game* g = &act.games[want[i]];
		if (!g->art_tried) {
			find_art(g);
			return g->art != NULL;
		}
	}
	return 0;
}

// The game's box art, or a tile in its colour with its first letter.
static void draw_art(Game* g, float x, float y, float s, float alpha) {
	if (g->art) {
		lgfx_draw_tex_fit(g->art, x, y, s, s, s * 0.12f, alpha);
		return;
	}
	float r, gr, b;
	art_rgb(g, &r, &gr, &b);
	const float top[4] = { r, gr, b, alpha }, bottom[4] = { r * 0.45f, gr * 0.45f, b * 0.45f, alpha };
	lgfx_draw_gradient(x, y, s, s, s * 0.16f, top, bottom);
	char first[8] = { 0 };
	int len = 1;
	unsigned char c = (unsigned char)g->name[0];
	if (c >= 0xF0) len = 4;
	else if (c >= 0xE0) len = 3;
	else if (c >= 0xC0) len = 2;
	memcpy(first, g->name, len);
	LTex* t = text_tex(s > 120 ? LFONT_HERO : LFONT_TITLE, first);
	if (t) lgfx_draw_tex(t, x + (s - t->w) * 0.5f, y + (s - t->h) * 0.5f, t->w, t->h, 0, alpha * 0.9f);
}

static void backdrop(void) {
	if (act.backdrop) lgfx_draw_tex(act.backdrop, 0, 0, W, H, 0, 1);
	else {
		const float top[4] = { 0.20f, 0.17f, 0.36f, 1 }, bottom[4] = { 0.10f, 0.12f, 0.22f, 1 };
		lgfx_draw_gradient(0, 0, W, H, 0, top, bottom);
	}
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.30f);
}

static void segmented(void) {
	float w = 220, h = 36, x = (W - w) * 0.5f, y = 22;
	lgfx_draw_fill(x, y, w, h, h * 0.5f, 1, 1, 1, 0.10f);
	lgfx_draw_fill(x + 3 + act.day * (w * 0.5f), y + 3, w * 0.5f - 6, h - 6, (h - 6) * 0.5f, 1, 1, 1, 0.28f);
	text(LFONT_SMALL, "Week", x + w * 0.25f, y + 7, act.day ? 0.6f : 1, 2);
	text(LFONT_SMALL, "Day", x + w * 0.75f, y + 7, act.day ? 1 : 0.6f, 2);
}

// The axis top: the smallest step that fits the busiest bucket.
static int axis_max(void) {
	static const int steps[] = { 900, 1800, 3600, 7200, 10800, 14400, 21600, 28800, 43200, 86400 };
	int most = 0;
	for (int b = 0; b < act.buckets; b++) {
		int sum = 0;
		for (int c = 0; c <= COLORS; c++) sum += act.bucket[b][c];
		if (sum > most) most = sum;
	}
	if (act.day) return most <= 900 ? 900 : most <= 1800 ? 1800 : 3600;
	for (int i = 0; i < 10; i++)
		if (most <= steps[i]) return steps[i];
	return 86400;
}

static void axis_label(int secs, char* out, size_t n) {
	if (secs == 0) snprintf(out, n, "0");
	else if (secs % 3600 == 0) snprintf(out, n, "%d h", secs / 3600);
	else snprintf(out, n, "%d min", secs / 60);
}

static void draw_chart(float x, float y, float w, float h) {
	lgfx_draw_fill(x, y, w, h, 18, 1, 1, 1, 0.06f);
	float gx = x + 24, gw = w - 24 - 80, gy = y + 22, gh = h - 22 - 40;
	int top = axis_max();
	for (int i = 0; i <= 2; i++) {
		int v = top * i / 2;
		float ly = gy + gh * (1 - (float)v / top);
		lgfx_draw_fill(gx, ly, gw, 1, 0, 1, 1, 1, 0.12f);
		char s[16];
		axis_label(v, s, sizeof(s));
		text(LFONT_SMALL, s, x + w - 20, ly - 11, 0.45f, 1);
	}
	if (!act.day && act.total > 0) {
		// a dashed line at the daily average
		float avg = gy + gh * (1 - (act.total / 7.0f) / top);
		for (float dx = 0; dx < gw; dx += 14) lgfx_draw_fill(gx + dx, avg - 1, 8, 2, 1, 0.20f, 0.78f, 0.35f, 0.9f);
		text(LFONT_SMALL, "avg", gx + 2, avg - 26, 0.8f, 0);
	}
	float slot = gw / act.buckets;
	time_t today = midnight(time(NULL));
	for (int b = 0; b < act.buckets; b++) {
		float bw = act.day ? slot * 0.6f : slot * 0.42f, bx = gx + b * slot + (slot - bw) * 0.5f, base = gy + gh;
		for (int c = 0; c <= COLORS; c++) {
			float bh = gh * act.bucket[b][c] / top;
			if (bh < 1) continue;
			lgfx_draw_fill(bx, base - bh, bw, bh - (bh > 3 ? 1.5f : 0), act.day ? 2 : 3, palette[c][0], palette[c][1], palette[c][2], 1);
			base -= bh;
		}
		if (act.day) {
			if (b % 6 == 0) {
				char s[8];
				snprintf(s, sizeof(s), "%02d", b);
				text(LFONT_SMALL, s, bx, gy + gh + 10, 0.5f, 0);
			}
		} else {
			time_t t = today - (6 - b) * 86400 + 7200;
			struct tm tm;
			localtime_r(&t, &tm);
			char s[8];
			strftime(s, sizeof(s), "%a", &tm);
			text(LFONT_SMALL, s, bx + bw * 0.5f, gy + gh + 10, b == 6 ? 0.95f : 0.5f, 2);
		}
	}
}

static void draw_row(Game* g, float x, float y, float w, float h, int focused) {
	if (focused) {
		x -= 8, w += 16, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 16, 1, 1, 1, 0.96f);
	} else
		lgfx_draw_fill(x, y, w, h, 16, 1, 1, 1, 0.07f);
	draw_art(g, x + 10, y + 9, h - 18, 1);
	if (focused) lgfx_set_dim(0.1f);
	text(LFONT_BODY, g->name, x + h + 8, y + 6, 1, 0);
	char t[32];
	hours_minutes(g->period, t, sizeof(t));
	text(LFONT_BODY, t, x + w - 50, y + (h - 30) * 0.5f, focused ? 1 : 0.85f, 1);
	text(LFONT_BODY, "\xE2\x80\xBA", x + w - 22, y + (h - 30) * 0.5f, focused ? 1 : 0.45f, 1);
	lgfx_set_dim(1);
	int most = act.games[act.order[0]].period;
	float bw = (w - h - 8 - 220) * g->period / (most ? most : 1);
	const float* c = palette[g->color];
	lgfx_draw_fill(x + h + 8, y + h - 18, bw < 6 ? 6 : bw, 6, 3, c[0], c[1], c[2], 1);
}

static void draw_overview(void) {
	segmented();
	float x = 56, w = W - 112;
	char big[32], sub[96];
	if (act.day) {
		hours_minutes(act.total, big, sizeof(big));
		char y[32];
		hours_minutes(act.before, y, sizeof(y));
		snprintf(sub, sizeof(sub), "Yesterday: %s", y);
	} else {
		hours_minutes(act.total / 7, big, sizeof(big));
		if (act.before > 0) {
			int pct = (int)lroundf((act.total - act.before) * 100.0f / act.before);
			snprintf(sub, sizeof(sub), "%s %d%% from last week", pct >= 0 ? "\xE2\x86\x91" : "\xE2\x86\x93", abs(pct));
		} else
			sub[0] = '\0';
	}
	text(LFONT_LABEL, act.day ? "TODAY" : "DAILY AVERAGE", x, 82, 0.55f, 0);
	float hw = text(LFONT_HERO, big, x, 104, 1, 0);
	if (sub[0]) text(LFONT_BODY, sub, x + hw + 22, 124, 0.6f, 0);
	draw_chart(x, 184, w, 230);

	text(LFONT_LABEL, "MOST PLAYED", x, 436, 0.55f, 0);
	if (!act.ranked) {
		text(LFONT_BODY, act.day ? "No games played today." : "No games played this week.", x, 474, 0.6f, 0);
		if (!act.count) text(LFONT_SMALL, "Play a game and your time shows here.", x, 510, 0.45f, 0);
	}
	float y = 464, rh = 66, gap = 6;
	for (int i = act.top; i < act.ranked && i < act.top + ROWS_SHOWN; i++, y += rh + gap)
		draw_row(&act.games[act.order[i]], x, y, w, rh, i == act.focus);
	if (act.top + ROWS_SHOWN < act.ranked) text(LFONT_SMALL, "\xE2\x96\xBC", W * 0.5f, y - 4, 0.35f, 2);
	text(LFONT_SMALL, act.ranked ? "A  Details        L R  Week or Day        B  Back" : "L R  Week or Day        B  Back", 40, H - 40, 0.6f, 0);
}

static void stat(float x, float y, const char* label, const char* value) {
	text(LFONT_LABEL, label, x, y, 0.55f, 0);
	text(LFONT_BODY, value, x, y + 24, 0.95f, 0);
}

// One game: its art, its numbers, and its time on each of the last seven days.
static void draw_detail(void) {
	Game* g = &act.games[act.order[act.focus]];
	float s = 300, ax = 72, ay = 110;
	lgfx_draw_shadow(ax, ay, s, s, 14, 0.55f);
	draw_art(g, ax, ay, s, 1);
	float x = 420;
	text(LFONT_LABEL, g->console, x, 110, 0.55f, 0);
	LTex* name = ltex_text_wrapped(LFONT_TITLE, g->name, W - 56 - (int)x, 2, 0);
	float ny = 136;
	if (name) {
		lgfx_draw_tex(name, x, ny, name->w, name->h, 0, 1);
		ny += name->h;
		ltex_free(name);
	}
	char v[64];
	float sy = ny + 30, col = 190;
	hours_minutes(g->total, v, sizeof(v));
	stat(x, sy, "TOTAL", v);
	snprintf(v, sizeof(v), g->plays == 1 ? "1 time" : "%d times", g->plays);
	stat(x + col, sy, "PLAYED", v);
	hours_minutes(g->plays ? g->total / g->plays : 0, v, sizeof(v));
	stat(x + 2 * col, sy, "AVERAGE", v);
	when(g->last, v, sizeof(v));
	stat(x, sy + 76, "LAST PLAYED", v);
	when(g->first, v, sizeof(v));
	stat(x + col, sy + 76, "FIRST PLAYED", v);
	if (exists(g->path)) {
		LTex* t = text_tex(LFONT_BODY, "A   Play");
		if (t) {
			float bw = t->w + 64, bh = 52, by = sy + 156;
			lgfx_draw_fill(x, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.16f);
			lgfx_draw_tex(t, x + 32, by + (bh - t->h) * 0.5f, t->w, t->h, 0, 0.95f);
		}
	}

	// this game's last seven days
	float cx = 72, cy = 470, cw = W - 144, ch = 200;
	lgfx_draw_fill(cx, cy, cw, ch, 18, 1, 1, 1, 0.06f);
	text(LFONT_LABEL, "LAST 7 DAYS", cx + 20, cy + 16, 0.55f, 0);
	time_t today = midnight(time(NULL));
	int day[7] = { 0 }, most = 0;
	for (int i = 0; i < act.nsessions; i++) {
		Session* se = &act.sessions[i];
		if (se->start < CLOCK_SET || &act.games[se->game] != g) continue;
		for (int b = 0; b < 7; b++) {
			time_t bf = midnight(today - (6 - b) * 86400 + 7200);
			day[b] += overlap(se, bf, midnight(bf + 86400 + 7200));
		}
	}
	for (int b = 0; b < 7; b++)
		if (day[b] > most) most = day[b];
	float gx = cx + 24, gw = cw - 48, gy = cy + 52, gh = ch - 52 - 40, slot = gw / 7;
	const float* c = palette[g->color];
	for (int b = 0; b < 7; b++) {
		float bw = slot * 0.36f, bx = gx + b * slot + (slot - bw) * 0.5f, bh = most ? gh * day[b] / most : 0;
		lgfx_draw_fill(bx, gy, bw, gh, 4, 1, 1, 1, 0.06f);
		if (bh >= 1) lgfx_draw_fill(bx, gy + gh - bh, bw, bh, 4, c[0], c[1], c[2], 1);
		time_t t = today - (6 - b) * 86400 + 7200;
		struct tm tm;
		localtime_r(&t, &tm);
		char d[8];
		strftime(d, sizeof(d), "%a", &tm);
		text(LFONT_SMALL, d, bx + bw * 0.5f, gy + gh + 10, b == 6 ? 0.95f : 0.5f, 2);
	}
	text(LFONT_SMALL, exists(g->path) ? "A  Play        B  Back" : "This game is no longer on the card        B  Back", 40, H - 40, 0.6f, 0);
}

void lactivity_draw(void) {
	backdrop();
	text(LFONT_LABEL, "Activity", 48, 30, 0.55f, 0);
	if (act.detail) draw_detail();
	else draw_overview();
}
