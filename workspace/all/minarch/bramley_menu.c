// Bramley: the in-game menu drawn with the GL kit. See bramley_menu.h.
#include "bramley_menu.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "defines.h"
#include "api.h"
#include "lgfx.h"
#include "ma_frontend_opts.h"

// minarch's screen size, set when it starts (ma_internal.h)
extern int DEVICE_WIDTH, DEVICE_HEIGHT;
#define W DEVICE_WIDTH
#define H DEVICE_HEIGHT

static int ready;      // lgfx started, once per minarch run
static int active;     // between bramley_menu_begin and bramley_menu_end
static LTex* frame;    // the paused game
static struct {
	LTex* tex;
	char path[256];
	time_t mtime;
} preview;

static void text(int font, const char* s, float x, float y, float alpha, int right) {
	LTex* t = ltex_text(font, s, 0);
	if (!t) return;
	lgfx_draw_tex(t, right ? x - t->w : x, y, t->w, t->h, 0, alpha);
	ltex_free(t);
}

int bramley_menu_begin(SDL_Surface* bitmap) {
	if (!bitmap) return 0;
	lgfx_save_gl_state();
	if (!ready) {
		lgfx_init(W, H);
		ready = 1;
	}
	frame = ltex_from_surface(bitmap, 0);
	if (frame) {
		// the blurred game behind the menu, made once
		lgfx_begin_snapshot();
		lgfx_draw_tex(frame, 0, 0, W, H, 0, 1);
		lgfx_end_snapshot();
	}
	lgfx_restore_gl_state();
	active = frame != NULL;
	return active;
}

int bramley_menu_active(void) {
	return active;
}

void bramley_menu_end(void) {
	active = 0;
	lgfx_save_gl_state();
	ltex_free(frame);
	frame = NULL;
	ltex_free(preview.tex);
	preview.tex = NULL;
	preview.path[0] = 0;
	lgfx_restore_gl_state();
}

// The console's name from the game's folder, "Game Boy Advance (GBA)" -> "GAME BOY ADVANCE".
static void console_name(const char* rom_path, char* out, size_t size) {
	out[0] = 0;
	if (!rom_path) return;
	const char* end = strrchr(rom_path, '/');
	if (!end) return;
	const char* start = end - 1;
	while (start > rom_path && *start != '/') start--;
	if (*start == '/') start++;
	size_t n = end - start;
	if (n >= size) n = size - 1;
	memcpy(out, start, n);
	out[n] = 0;
	char* paren = strstr(out, " (");
	if (paren) *paren = 0;
	for (char* c = out; *c; c++)
		if (*c >= 'a' && *c <= 'z') *c -= 32;
}

// The slot's screenshot, loaded again only when the file changes.
static LTex* slot_preview(const BramleyMenuView* v, time_t* when) {
	*when = 0;
	if (!v->preview_exists || !v->preview_path) return NULL;
	struct stat st;
	if (stat(v->preview_path, &st) != 0) return NULL;
	*when = st.st_mtime;
	if (preview.tex && !strcmp(preview.path, v->preview_path) && preview.mtime == st.st_mtime) return preview.tex;
	ltex_free(preview.tex);
	preview.tex = ltex_load(v->preview_path, 0);
	snprintf(preview.path, sizeof(preview.path), "%s", v->preview_path);
	preview.mtime = st.st_mtime;
	return preview.tex;
}

static void saved_when(time_t t, char* out, size_t size) {
	time_t now = time(NULL);
	struct tm a, b;
	localtime_r(&t, &a);
	localtime_r(&now, &b);
	char clock[16];
	strftime(clock, sizeof(clock), "%H:%M", &a);
	if (a.tm_year == b.tm_year && a.tm_yday == b.tm_yday)
		snprintf(out, size, "Saved today at %s", clock);
	else {
		char day[32];
		strftime(day, sizeof(day), "%b %e", &a);
		snprintf(out, size, "Saved %s at %s", day, clock);
	}
}

static void dots(float cx, float y, int n, int sel) {
	float gap = 16, x = cx - (n - 1) * gap * 0.5f;
	for (int i = 0; i < n; i++) {
		float s = i == sel ? 9 : 7;
		lgfx_draw_fill(x + i * gap - s * 0.5f, y - s * 0.5f, s, s, s * 0.5f, 1, 1, 1, i == sel ? 0.95f : 0.35f);
	}
}

void bramley_menu_draw(const BramleyMenuView* v) {
	lgfx_save_gl_state();
	lgfx_begin_frame();
	lgfx_draw_snapshot(1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.40f);

	// status and title
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	char status[32];
	strftime(status, sizeof(status), "%H:%M", &tm);
	char pct[16];
	snprintf(pct, sizeof(pct), "      %d%%", PWR_getBattery());
	strncat(status, pct, sizeof(status) - strlen(status) - 1);
	text(LFONT_LABEL, status, W - 48, 30, 0.8f, 1);
	char console[64];
	console_name(v->rom_path, console, sizeof(console));
	text(LFONT_LABEL, console, 72, 64, 0.55f, 0);
	LTex* t = ltex_text(LFONT_TITLE, v->title ? v->title : "", W - 72 - 72 - 200);
	if (t) {
		lgfx_draw_tex(t, 72, 90, t->w, t->h, 0, 1);
		ltex_free(t);
	}

	// the actions
	float y = 170;
	for (int i = 0; i < v->count; i++, y += 70) {
		int f = i == v->selected;
		float x = 72, w = 300, h = 58, yy = y;
		if (f) {
			x -= 6, w += 12, yy -= 3, h += 6;
			lgfx_draw_shadow(x, yy, w, h, 6, 0.35f);
			lgfx_draw_fill(x, yy, w, h, 16, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
		} else
			lgfx_draw_fill(x, yy, w, h, 16, 1, 1, 1, 0.08f);
		float ty = yy + (h - 30) * 0.5f;
		text(LFONT_BODY, v->items[i], x + 24, ty, 1, 0);
		if (i == 0 && v->total_discs > 1) {
			char disc[32];
			snprintf(disc, sizeof(disc), f ? "\xE2\x80\xB9  Disc %d  \xE2\x80\xBA" : "Disc %d", v->disc + 1);
			lgfx_set_dim(f ? 0.45f : 1);
			text(LFONT_BODY, disc, x + w - 22, ty, f ? 1 : 0.55f, 1);
		}
		lgfx_set_dim(1);
	}

	// the picture: the slot's screenshot on Save and Load, the paused game otherwise
	int slots = v->selected == 1 || v->selected == 2;
	float cw = 528, ch = 352, cx = 432, cy = 170;
	time_t when = 0;
	LTex* pic = slots ? slot_preview(v, &when) : frame;
	lgfx_draw_shadow(cx, cy, cw, ch, 12, 0.5f);
	if (pic)
		lgfx_draw_art(pic, cx, cy, cw, ch, 18, 1);
	else {
		lgfx_draw_fill(cx, cy, cw, ch, 18, 0.08f, 0.08f, 0.12f, 0.9f);
		LTex* m = ltex_text(LFONT_BODY, v->save_exists ? "No picture for this save" : "Empty slot", 0);
		if (m) {
			lgfx_draw_tex(m, cx + (cw - m->w) * 0.5f, cy + (ch - m->h) * 0.5f, m->w, m->h, 0, 0.5f);
			ltex_free(m);
		}
	}
	char line[64], label[16];
	if (slots) {
		snprintf(label, sizeof(label), "SLOT %d", v->slot + 1);
		if (when) saved_when(when, line, sizeof(line));
		else snprintf(line, sizeof(line), "%s", v->save_exists ? "Saved" : "Nothing saved here yet");
		dots(cx + cw - 70, cy + ch + 62, 8, v->slot);
	} else {
		snprintf(label, sizeof(label), "%s", "PAUSED");
		snprintf(line, sizeof(line), "%s", v->selected == 4 ? "Quit saves your place to resume" : "Your game is paused");
	}
	text(LFONT_LABEL, label, cx, cy + ch + 24, 0.55f, 0);
	text(LFONT_BODY, line, cx, cy + ch + 48, 0.9f, 0);

	// button hints for the focused action
	char hint[128];
	switch (v->selected) {
	case 1: snprintf(hint, sizeof(hint), "A  Save to slot %d        \xE2\x80\xB9 \xE2\x80\xBA  Slot        B  Back to game", v->slot + 1); break;
	case 2: snprintf(hint, sizeof(hint), "A  Load slot %d        \xE2\x80\xB9 \xE2\x80\xBA  Slot        B  Back to game", v->slot + 1); break;
	case 3: snprintf(hint, sizeof(hint), "%s", "A  Open        B  Back to game"); break;
	case 4: snprintf(hint, sizeof(hint), "%s", "A  Quit        B  Back to game"); break;
	default: snprintf(hint, sizeof(hint), "%s", v->total_discs > 1 ? "A  Continue        \xE2\x80\xB9 \xE2\x80\xBA  Disc        B  Back to game" : "A  Continue        B  Back to game");
	}
	text(LFONT_SMALL, hint, 72, H - 44, 0.6f, 0);

	lgfx_end_frame();
	lgfx_restore_gl_state();
}

// ---------------------------------------------------------------------------------------------
// Options

// Text textures kept between frames, since Options redraws whenever anything changes.
#define CACHE 384
static struct {
	char key[160];
	int font;
	LTex* tex;
} cache[CACHE];
static int cached;

static LTex* cached_text(int font, const char* s) {
	for (int i = 0; i < cached; i++)
		if (cache[i].font == font && !strcmp(cache[i].key, s)) return cache[i].tex;
	if (cached == CACHE) {
		for (int i = 0; i < cached; i++) ltex_free(cache[i].tex);
		cached = 0;
	}
	LTex* t = ltex_text(font, s, 0);
	if (strlen(s) >= sizeof(cache[0].key)) return t; // too long to cache; leaks one texture
	cache[cached].font = font;
	snprintf(cache[cached].key, sizeof(cache[cached].key), "%s", s);
	cache[cached].tex = t;
	cached++;
	return t;
}

static float text_width(int font, const char* s) {
	LTex* t = cached_text(font, s);
	return t ? t->w : 0;
}

static void ctext(int font, const char* s, float x, float y, float alpha, int right) {
	if (!s || !*s) return;
	LTex* t = cached_text(font, s);
	if (t) lgfx_draw_tex(t, right ? x - t->w : x, y, t->w, t->h, 0, alpha);
}

// Descriptions can hold line breaks; the kit wraps on its own, so they become spaces.
static LTex* wrapped(int font, const char* text, int width, int lines) {
	char buf[512];
	snprintf(buf, sizeof(buf), "%s", text);
	for (char* c = buf; *c; c++)
		if (*c == '\n') *c = ' ';
	return ltex_text_wrapped(font, buf, width, lines, 0);
}

#define DEPTH 6
static struct {
	MenuList* list;
	int selected, top;
} stack[DEPTH];
static int depth;

void bramley_options_push(MenuList* list) {
	if (depth < DEPTH) stack[depth].list = list, stack[depth].selected = 0, stack[depth].top = 0;
	depth++;
}

void bramley_options_pop(void) {
	if (depth > 0) depth--;
}

static int count_items(MenuList* l) {
	int n = 0;
	while (l->items[n].name) n++;
	return n;
}

static int number(const char* s, int* out) {
	char buf[32];
	snprintf(buf, sizeof(buf), "%s", s);
	size_t n = strlen(buf);
	if (n && buf[n - 1] == '%') buf[--n] = 0;
	if (!n) return 0;
	char* end;
	long v = strtol(buf, &end, 10);
	*out = (int)v;
	return *end == 0;
}

// Evenly spaced numbers (the first may be a word such as "Off"): drawn as a slider.
static int is_level(char** values, int n) {
	if (n < 11) return 0;
	int first, a, b, start;
	start = number(values[0], &first) ? 0 : 1;
	if (!number(values[start], &a) || !number(values[start + 1], &b) || a == b) return 0;
	for (int i = start; i < n; i++) {
		int v;
		if (!number(values[i], &v) || v != a + (i - start) * (b - a)) return 0;
	}
	return 1;
}

static void option_row(MenuItem* item, int is_input, float x, float y, float w, float h, int focused, int awaiting) {
	if (focused) {
		x -= 6, w += 12, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.96f);
		lgfx_set_dim(0.1f);
	} else
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f);
	float ty = y + (h - 30) * 0.5f, right = x + w - 20;
	ctext(LFONT_BODY, item->name, x + 20, ty, 1, 0);
	int n = 0;
	while (item->values && item->values[n]) n++;
	if (awaiting) {
		lgfx_set_dim(focused ? 0.45f : 1);
		ctext(LFONT_BODY, "Press a button\xE2\x80\xA6", right, ty, 1, 1);
	} else if (n == 2 && !is_input && !strcmp(item->values[0], "Off") && !strcmp(item->values[1], "On")) {
		lgfx_set_dim(1);
		int on = item->value == 1;
		float sw = 58, sh = 34, sy = y + (h - sh) * 0.5f;
		if (on) lgfx_draw_fill(right - sw, sy, sw, sh, sh * 0.5f, BRAMLEY_ACCENT, 1);
		else lgfx_draw_fill(right - sw, sy, sw, sh, sh * 0.5f, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, focused ? 0.15f : 0.22f);
		float k = sh - 6;
		lgfx_draw_fill(on ? right - k - 3 : right - sw + 3, sy + 3, k, k, k * 0.5f, 1, 1, 1, 1);
	} else if (n > 0 && !is_input && is_level(item->values, n)) {
		lgfx_set_dim(1);
		float frac = n > 1 ? (float)item->value / (n - 1) : 0, sw = 200, sx = right - sw, cy = y + h * 0.5f, c = focused ? 0.1f : 1;
		lgfx_draw_fill(sx, cy - 3, sw, 6, 3, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, 0.2f);
		lgfx_draw_fill(sx, cy - 3, sw * frac, 6, 3, c, c, c, 0.95f);
		lgfx_draw_fill(sx + sw * frac - 12, cy - 12, 24, 24, 12, 1, 1, 1, 1);
		if (focused) lgfx_draw_ring(sx + sw * frac - 12, cy - 12, 24, 24, 12, 1, 0, 0, 0, 0.25f);
	} else if (n > 0 && item->value >= 0 && item->value < n) {
		char label[160];
		if (focused && n > 1 && !is_input) snprintf(label, sizeof(label), "\xE2\x80\xB9  %s  \xE2\x80\xBA", item->values[item->value]);
		else snprintf(label, sizeof(label), "%s", item->values[item->value]);
		lgfx_set_dim(focused ? 0.45f : 1);
		ctext(LFONT_BODY, label, right, ty, focused ? 1 : 0.5f, 1);
	} else if (!item->values && (item->on_confirm || item->submenu)) {
		lgfx_set_dim(focused ? 0.35f : 1);
		ctext(LFONT_BODY, "\xE2\x80\xBA", right, ty, focused ? 1 : 0.4f, 1);
	}
	lgfx_set_dim(1);
}

void bramley_options_draw(MenuList* list, int selected, int await_input) {
	// remember where this level is, for the levels drawn around it
	int level = -1;
	for (int i = 0; i < depth && i < DEPTH; i++)
		if (stack[i].list == list) level = i;
	if (level < 0) return;
	stack[level].selected = selected;

	lgfx_save_gl_state();
	lgfx_begin_frame();
	lgfx_draw_snapshot(1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.45f);

	ctext(LFONT_LABEL, "Options", 48, 30, 0.55f, 0);

	// sidebar: the sections
	MenuList* root = stack[0].list;
	int n = count_items(root), open = level > 0;
	float x = 40, w = 330, rh = 48, gap = 6, y = 84;
	for (int i = 0; i < n; i++, y += rh + gap) {
		int f = i == stack[0].selected;
		float fx = x, fw = w, fy = y, fh = rh;
		if (f && !open) {
			fx -= 8, fw += 16, fy -= 3, fh += 6;
			lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
			lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
		} else
			lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, f ? 0.22f : 0.07f);
		ctext(LFONT_BODY, root->items[i].name, fx + 20, fy + (fh - 30) * 0.5f, open ? (f ? 0.95f : 0.45f) : 1, 0);
		lgfx_set_dim(1);
	}

	// pane
	float px = 410, pw = W - px - 40, py = 84, pb = H - 70;
	lgfx_draw_fill(px, py, pw, pb - py, 22, 0, 0, 0, 0.18f);
	MenuItem* section = &root->items[stack[0].selected];
	if (!open) {
		ctext(LFONT_TITLE, section->name, px + 28, py + 20, 1, 0);
		if (section->desc) {
			LTex* d = wrapped(LFONT_BODY, section->desc, (int)pw - 56, 4);
			if (d) {
				lgfx_draw_tex(d, px + 28, py + 76, d->w, d->h, 0, 0.6f);
				ltex_free(d);
			}
		}
		ctext(LFONT_SMALL, "A  Open        B  Back", 40, H - 40, 0.6f, 0);
	} else {
		MenuList* l = list;
		float ty = py + 20;
		if (level >= 2) {
			char back[160];
			snprintf(back, sizeof(back), "\xE2\x80\xB9  %s", stack[level - 1].list->items[stack[level - 1].selected].name);
			ctext(LFONT_BODY, back, px + 28, py + 18, 0.5f, 0);
			ty = py + 52;
		}
		const char* title = level >= 1 ? stack[level - 1].list->items[stack[level - 1].selected].name : section->name;
		if (l->category && *l->category) title = l->category;
		ctext(LFONT_TITLE, title, px + 28, ty, 1, 0);
		int count = count_items(l), is_input = l->type == MENU_INPUT;
		float ry = ty + 70, orh = 48, ogap = 6;
		int visible = (int)((pb - ry - 64) / (orh + ogap));
		if (visible < 1) visible = 1;
		int* top = &stack[level].top;
		if (selected < *top) *top = selected;
		if (selected >= *top + visible) *top = selected - visible + 1;
		float yy = ry;
		for (int i = *top; i < count && i < *top + visible; i++, yy += orh + ogap)
			option_row(&l->items[i], is_input, px + 20, yy, pw - 40, orh, i == selected, await_input && i == selected);
		if (*top > 0) ctext(LFONT_SMALL, "\xE2\x96\xB2", px + pw * 0.5f, ry - 26, 0.35f, 0);
		if (*top + visible < count) ctext(LFONT_SMALL, "\xE2\x96\xBC", px + pw * 0.5f, yy - 6, 0.35f, 0);
		const char* desc = selected >= 0 && selected < count && l->items[selected].desc ? l->items[selected].desc : l->desc;
		if (desc) {
			LTex* d = wrapped(LFONT_SMALL, desc, (int)pw - 80, 2);
			if (d) {
				lgfx_draw_tex(d, px + 40, yy + 20, d->w, d->h, 0, 0.5f);
				ltex_free(d);
			}
		}
		ctext(LFONT_SMALL,
		      is_input ? "A  Set button        X  Clear        B  Back" : "A  Select        \xE2\x80\xB9 \xE2\x80\xBA  Change        B  Back", 40, H - 40, 0.6f, 0);
	}

	lgfx_end_frame();
	lgfx_restore_gl_state();
}

///////////////////////////////////////////////////////////
// Messages

int bramley_message(const char* message) {
	char title[160] = "", body[512] = "";
	const char* nl = strchr(message, '\n');
	if (nl) {
		snprintf(title, sizeof(title), "%.*s", (int)(nl - message), message);
		snprintf(body, sizeof(body), "%s", nl + 1);
		for (char* c = body; *c; c++)
			if (*c == '\n') *c = ' ';
	} else
		snprintf(title, sizeof(title), "%s", message);
	int dirty = 1;
	for (;;) {
		GFX_startFrame();
		PAD_poll();
		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) break;
		PWR_update(&dirty, NULL, NULL, NULL);
		if (!dirty) {
			SDL_Delay(16);
			continue;
		}
		lgfx_save_gl_state();
		lgfx_begin_frame();
		lgfx_draw_snapshot(1);
		lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.45f);
		LTex* t = ltex_text_wrapped(LFONT_TITLE, title, 480, 2, 1);
		LTex* d = body[0] ? ltex_text_wrapped(LFONT_BODY, body, 480, 5, 1) : NULL;
		float w = 560, bh = 52, h = 36 + (t ? t->h : 0) + (d ? 16 + d->h : 0) + 28 + bh + 28;
		float x = (W - w) * 0.5f, y = (H - h) * 0.5f - 20, cy = y + 36;
		lgfx_draw_shadow(x, y, w, h, 14, 0.5f);
		lgfx_draw_fill(x, y, w, h, 26, 0.13f, 0.13f, 0.16f, 0.96f);
		if (t) {
			lgfx_draw_tex(t, x + (w - t->w) * 0.5f, cy, t->w, t->h, 0, 1);
			cy += t->h;
			ltex_free(t);
		}
		if (d) {
			cy += 16;
			lgfx_draw_tex(d, x + (w - d->w) * 0.5f, cy, d->w, d->h, 0, 0.7f);
			cy += d->h;
			ltex_free(d);
		}
		float bw = 200, bx = x + (w - bw) * 0.5f, by = cy + 28;
		lgfx_draw_fill(bx, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.96f);
		lgfx_set_dim(0.1f);
		LTex* ok = cached_text(LFONT_BODY, "OK");
		if (ok) lgfx_draw_tex(ok, bx + (bw - ok->w) * 0.5f, by + (bh - ok->h) * 0.5f, ok->w, ok->h, 0, 1);
		lgfx_set_dim(1);
		lgfx_end_frame();
		lgfx_restore_gl_state();
		dirty = 0;
	}
	return 0; // MENU_CALLBACK_NOP
}

///////////////////////////////////////////////////////////
// Achievements

// The badge, or a grey tile with a dot while it downloads.
static void draw_badge(SDL_Surface* badge, float x, float y, float s, int unlocked) {
	LTex* t = badge ? ltex_from_surface(badge, 0) : NULL;
	if (t) {
		lgfx_draw_tex(t, x, y, s, s, s * 0.16f, 1);
		ltex_free(t);
		return;
	}
	const float a[4] = { 0.35f, 0.35f, 0.38f, 1 }, b[4] = { 0.2f, 0.2f, 0.22f, 1 };
	lgfx_draw_gradient(x, y, s, s, s * 0.16f, a, b);
	lgfx_draw_fill(x + s * 0.3f, y + s * 0.3f, s * 0.4f, s * 0.4f, s * 0.2f, 1, 1, 1, unlocked ? 0.9f : 0.35f);
}

static void ach_top(const char* game) {
	lgfx_begin_frame();
	lgfx_draw_snapshot(1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.55f);
	if (game) {
		char up[128];
		snprintf(up, sizeof(up), "%s", game);
		for (char* c = up; *c; c++)
			if (*c >= 'a' && *c <= 'z') *c -= 32;
		ctext(LFONT_LABEL, up, 72, 52, 0.55f, 0);
	}
}

void bramley_achievements_draw(const char* game, unsigned unlocked, unsigned total, const BramleyAchievement* rows,
                               int count, int selected, int top, int locked_only, int* visible) {
	lgfx_save_gl_state();
	ach_top(game);
	ctext(LFONT_TITLE, locked_only ? "Locked Achievements" : "Achievements", 72, 76, 1, 0);
	char p[48];
	snprintf(p, sizeof(p), "%u of %u unlocked", unlocked, total);
	ctext(LFONT_BODY, p, W - 72, 66, 0.85f, 1);
	lgfx_draw_fill(W - 72 - 260, 104, 260, 8, 4, 1, 1, 1, 0.18f);
	if (total) lgfx_draw_fill(W - 72 - 260, 104, 260.0f * unlocked / total, 8, 4, BRAMLEY_ACCENT, 1);

	float x = 72, w = W - 144, y = 150, rh = 86, gap = 8;
	int fit = (int)((H - 70 - y) / (rh + gap));
	if (visible) *visible = fit;
	for (int i = top; i < count && i < top + fit; i++, y += rh + gap) {
		const BramleyAchievement* a = &rows[i];
		int f = i == selected;
		float fx = x, fy = y, fw = w, fh = rh;
		if (f) {
			fx -= 8, fw += 16, fy -= 3, fh += 6;
			lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
			lgfx_draw_fill(fx, fy, fw, fh, 18, 1, 1, 1, 0.96f);
		} else
			lgfx_draw_fill(fx, fy, fw, fh, 18, 1, 1, 1, 0.07f);
		draw_badge(a->badge, fx + 14, fy + 12, fh - 24, a->unlocked);
		if (f) lgfx_set_dim(0.1f);
		char t[200];
		snprintf(t, sizeof(t), "%s%s%s", a->challenge ? "\xE2\x97\x8F " : "", a->muted ? "(muted) " : "", a->title ? a->title : "");
		LTex* tt = ltex_text(LFONT_BODY, t, (int)(fw - fh - 170));
		if (tt) {
			lgfx_draw_tex(tt, fx + fh + 6, fy + 12, tt->w, tt->h, 0, a->unlocked || f ? 1 : 0.75f);
			ltex_free(tt);
		}
		LTex* dt = a->description ? ltex_text(LFONT_SMALL, a->description, (int)(fw - fh - 170)) : NULL;
		if (dt) {
			lgfx_draw_tex(dt, fx + fh + 6, fy + 48, dt->w, dt->h, 0, f ? 0.7f : 0.55f);
			ltex_free(dt);
		}
		snprintf(p, sizeof(p), "%u pts", a->points);
		ctext(LFONT_BODY, p, fx + fw - 24, fy + (fh - 30) * 0.5f, f ? 1 : 0.6f, 1);
		lgfx_set_dim(1);
		if (a->unlocked) lgfx_draw_fill(fx + fw - 130, fy + fh * 0.5f - 6, 12, 12, 6, BRAMLEY_ACCENT, 1);
	}
	if (top > 0) ctext(LFONT_SMALL, "\xE2\x96\xB2", W * 0.5f, 126, 0.35f, 0);
	if (top + fit < count) ctext(LFONT_SMALL, "\xE2\x96\xBC", W * 0.5f, y - 6, 0.35f, 0);
	int muted = selected >= 0 && selected < count && rows[selected].muted;
	char hint[160];
	snprintf(hint, sizeof(hint), "A  Details        Y  %s        X  %s        B  Back", locked_only ? "Show all" : "Show locked",
	         muted ? "Unmute" : "Mute");
	ctext(LFONT_SMALL, hint, 40, H - 40, 0.6f, 0);
	lgfx_end_frame();
	lgfx_restore_gl_state();
}

void bramley_achievement_draw(const BramleyAchievement* a, int index, int count) {
	lgfx_save_gl_state();
	ach_top(NULL);
	float s = 200, bx = (W - s) * 0.5f, by = 90;
	lgfx_draw_shadow(bx, by, s, s, 14, 0.5f);
	draw_badge(a->badge, bx, by, s, a->unlocked);
	lgfx_draw_fill(W * 0.5f - 320, by + s + 14, 640, 270, 24, 0, 0, 0, 0.35f);
	LTex* t = ltex_text_wrapped(LFONT_TITLE, a->title ? a->title : "", 580, 2, 1);
	float y = by + s + 34;
	if (t) {
		lgfx_draw_tex(t, (W - t->w) * 0.5f, y, t->w, t->h, 0, 1);
		y += t->h + 8;
		ltex_free(t);
	}
	LTex* d = a->description ? ltex_text_wrapped(LFONT_BODY, a->description, 580, 3, 1) : NULL;
	if (d) {
		lgfx_draw_tex(d, (W - d->w) * 0.5f, y, d->w, d->h, 0, 0.7f);
		y += d->h;
		ltex_free(d);
	}
	// three facts: points, status, and how many players have it
	char pts[16], rare[32];
	snprintf(pts, sizeof(pts), "%u", a->points);
	if (a->rarity > 0) snprintf(rare, sizeof(rare), "%.1f%%", a->rarity);
	else snprintf(rare, sizeof(rare), "\xE2\x80\x94");
	const char* lab[3] = { "POINTS", a->unlocked ? "UNLOCKED" : "STATUS", "PLAYERS" };
	const char* val[3] = { pts, a->status ? a->status : (a->unlocked ? "Yes" : "Locked"), rare };
	float cy = by + s + 196, pw = 190, gap = 14, px = (W - 3 * pw - 2 * gap) * 0.5f;
	for (int i = 0; i < 3; i++) {
		float x = px + i * (pw + gap);
		lgfx_draw_fill(x, cy, pw, 72, 16, 1, 1, 1, 0.07f);
		LTex* l = cached_text(LFONT_LABEL, lab[i]);
		if (l) lgfx_draw_tex(l, x + (pw - l->w) * 0.5f, cy + 12, l->w, l->h, 0, 0.55f);
		LTex* v = ltex_text(LFONT_BODY, val[i], (int)pw - 16);
		if (v) {
			lgfx_draw_tex(v, x + (pw - v->w) * 0.5f, cy + 34, v->w, v->h, 0, 0.95f);
			ltex_free(v);
		}
	}
	char foot[96];
	if (a->offline) snprintf(foot, sizeof(foot), "Unlocked offline. It will sync when Wi-Fi is on.");
	else if (a->kind) snprintf(foot, sizeof(foot), "%s", a->kind);
	else foot[0] = 0;
	if (foot[0]) ctext(LFONT_SMALL, foot, W * 0.5f - text_width(LFONT_SMALL, foot) * 0.5f, cy + 92, 0.55f, 0);
	char hint[128];
	snprintf(hint, sizeof(hint), "\xE2\x80\xB9 \xE2\x80\xBA  %d of %d        X  %s        B  Back", index + 1, count, a->muted ? "Unmute" : "Mute");
	ctext(LFONT_SMALL, hint, 40, H - 40, 0.6f, 0);
	lgfx_end_frame();
	lgfx_restore_gl_state();
}
