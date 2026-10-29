// Files: Bramley's file browser. See lfiles.h.
#include "lfiles.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "bramley.h"
#include "lgfx.h"
#include "lwallpaper.h"

#define W 1024
#define H 768
#define ROW_H 56
#define ROW_GAP 6

typedef struct {
	const char* name;
	char path[512];
	char kind;
} Place;

typedef struct {
	char name[256];
	int is_dir;
	long long size;
	time_t mtime;
	char kind;   // f folder, g game, s save, i image, m music, o other
	int items;   // folders: entries inside, -1 until counted
} Entry;

static struct {
	int open;
	int place;        // the focused place in the sidebar
	int in_pane;      // focus is in the folder, not the sidebar
	char path[1024];  // the folder shown
	Entry* entries;
	int count, cap;
	long long total;  // bytes of the files shown
	int focus, top;
	char console[128]; // the folder's console when it is one console's games, else ""
	long long free_bytes, card_bytes;
	LTex* backdrop;
	// image preview
	int previewing;
	LTex* preview;
	int preview_tried;
} fs;

static Place places[6];

///////////////////////////////////////////////////////////
// Text

#define TEXTS 200
static struct {
	int font;
	char s[200];
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

// align 0 left, 1 right, 2 centred; max_w > 0 cuts long text with an ellipsis
static float text(int font, const char* s, float x, float y, float alpha, int align) {
	LTex* t = text_tex(font, s);
	if (!t) return 0;
	float tx = align == 1 ? x - t->w : align == 2 ? x - t->w * 0.5f : x;
	lgfx_draw_tex(t, tx, y, t->w, t->h, 0, alpha);
	return t->w;
}

static void text_fit(int font, const char* s, float x, float y, float alpha, float max_w) {
	LTex* t = text_tex(font, s);
	if (!t) return;
	if (t->w <= max_w) {
		lgfx_draw_tex(t, x, y, t->w, t->h, 0, alpha);
		return;
	}
	// too long: cut in the middle so the extension stays visible ("Very Long Na….gba")
	char buf[200];
	size_t n = strlen(s);
	for (size_t keep = n > 8 ? n - 4 : n; keep > 6; keep -= 2) {
		size_t head = keep - 6, tail = 6;
		snprintf(buf, sizeof(buf), "%.*s\xE2\x80\xA6%s", (int)head, s, s + n - tail);
		LTex* c = text_tex(font, buf);
		if (c && c->w <= max_w) {
			lgfx_draw_tex(c, x, y, c->w, c->h, 0, alpha);
			return;
		}
	}
}

///////////////////////////////////////////////////////////
// Reading folders

static const char* ext_of(const char* name) {
	const char* dot = strrchr(name, '.');
	return dot && dot != name ? dot + 1 : "";
}

static int ext_in(const char* ext, const char* const* list) {
	for (; *list; list++)
		if (!strcasecmp(ext, *list)) return 1;
	return 0;
}

// Inside Roms/<console>/..., the console folder; "" elsewhere.
static void console_of(const char* path, char* out, size_t n) {
	out[0] = 0;
	size_t r = strlen(ROMS_PATH);
	if (strncmp(path, ROMS_PATH "/", r + 1)) return;
	const char* c = path + r + 1;
	const char* end = strchr(c, '/');
	size_t len = end ? (size_t)(end - c) : strlen(c);
	snprintf(out, n, "%.*s", (int)len, c);
	// "Game Boy Advance (GBA)" -> "Game Boy Advance"
	char* paren = strrchr(out, '(');
	if (paren && paren > out) {
		*paren = 0;
		size_t l = strlen(out);
		while (l && out[l - 1] == ' ') out[--l] = 0;
	}
}

static char kind_of(const char* dir, const char* name, int is_dir) {
	static const char* saves[] = { "sav", "srm", "dsv", "mcd", "mcr", "state", "st0", "st1", "st2", "st3", "st4", "st5", "st6", "st7", "st8", "st9", NULL };
	static const char* images[] = { "png", "jpg", "jpeg", "bmp", "gif", "webp", NULL };
	static const char* music[] = { "mp3", "flac", "ogg", "opus", "wav", "m4a", "m4b", "aac", NULL };
	static const char* parts[] = { "bin", "img", "sub", "txt", "cfg", "xml", "dat", NULL }; // not games on their own
	if (is_dir) return 'f';
	const char* ext = ext_of(name);
	if (ext_in(ext, saves) || !strncmp(dir, SDCARD_PATH "/Saves", strlen(SDCARD_PATH "/Saves"))) return 's';
	if (ext_in(ext, images)) return 'i';
	if (ext_in(ext, music)) return 'm';
	char console[128];
	console_of(dir, console, sizeof(console));
	if (console[0] && !ext_in(ext, parts) && !strstr(dir, "/.media")) return 'g';
	return 'o';
}

static int compare_entries(const void* a, const void* b) {
	const Entry *x = a, *y = b;
	if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir; // folders first
	return strcasecmp(x->name, y->name);
}

static int visible_name(const char* name) {
	if (!strcmp(name, ".") || !strcmp(name, "..")) return 0;
	if (name[0] == '.') return !strcmp(name, ".media"); // hidden, except the box art folders
	return 1;
}

static void read_folder(const char* path) {
	snprintf(fs.path, sizeof(fs.path), "%s", path);
	fs.count = 0;
	fs.total = 0;
	fs.focus = fs.top = 0;
	DIR* dh = opendir(path);
	if (dh) {
		struct dirent* dp;
		while ((dp = readdir(dh))) {
			if (!visible_name(dp->d_name)) continue;
			char full[1024];
			snprintf(full, sizeof(full), "%s/%s", path, dp->d_name);
			struct stat st;
			if (stat(full, &st)) continue;
			if (fs.count == fs.cap) {
				int cap = fs.cap ? fs.cap * 2 : 64;
				Entry* e = realloc(fs.entries, cap * sizeof(Entry));
				if (!e) break;
				fs.entries = e;
				fs.cap = cap;
			}
			Entry* e = &fs.entries[fs.count++];
			snprintf(e->name, sizeof(e->name), "%s", dp->d_name);
			e->is_dir = S_ISDIR(st.st_mode);
			e->size = e->is_dir ? 0 : (long long)st.st_size;
			e->mtime = st.st_mtime;
			e->kind = kind_of(path, dp->d_name, e->is_dir);
			e->items = -1;
			fs.total += e->size;
		}
		closedir(dh);
	}
	qsort(fs.entries, fs.count, sizeof(Entry), compare_entries);
	// one console's folder: its games need not repeat the console
	char console[128];
	console_of(path, console, sizeof(console));
	fs.console[0] = 0;
	if (console[0] && !strchr(path + strlen(ROMS_PATH) + 1, '/')) snprintf(fs.console, sizeof(fs.console), "%s", console);
}

static int count_items(const char* path) {
	int n = 0;
	DIR* dh = opendir(path);
	if (!dh) return 0;
	struct dirent* dp;
	while ((dp = readdir(dh)))
		if (visible_name(dp->d_name)) n++;
	closedir(dh);
	return n;
}

static void size_text(long long b, char* out, size_t n) {
	if (b < 1024) snprintf(out, n, "%lld B", b);
	else if (b < 1024 * 1024) snprintf(out, n, "%lld KB", (b + 512) / 1024);
	else if (b < 1024LL * 1024 * 1024) snprintf(out, n, "%.1f MB", b / 1048576.0);
	else snprintf(out, n, "%.1f GB", b / 1073741824.0);
}

static void date_text(time_t t, char* out, size_t n) {
	time_t now = time(NULL);
	struct tm a, b;
	localtime_r(&t, &a);
	localtime_r(&now, &b);
	struct tm m = b;
	m.tm_hour = m.tm_min = m.tm_sec = 0;
	time_t today = mktime(&m);
	if (t >= today) snprintf(out, n, "Today");
	else if (t >= today - 86400) snprintf(out, n, "Yesterday");
	else {
		strftime(out, n, a.tm_year == b.tm_year ? "%e %b" : "%e %b %Y", &a);
		if (out[0] == ' ') memmove(out, out + 1, strlen(out));
	}
}

static void read_space(void) {
	struct statvfs v;
	fs.free_bytes = fs.card_bytes = 0;
	if (!statvfs(SDCARD_PATH, &v)) {
		fs.free_bytes = (long long)v.f_bavail * v.f_frsize;
		fs.card_bytes = (long long)v.f_blocks * v.f_frsize;
	}
}

///////////////////////////////////////////////////////////
// Open and close

void lfiles_open(void) {
	memset(&fs, 0, sizeof(fs));
	Place p[6] = {
		{ "Games", ROMS_PATH, 'g' },
		{ "Saves", SDCARD_PATH "/Saves", 's' },
		{ "Music", SDCARD_PATH "/Music", 'm' },
		{ "Screenshots", SDCARD_PATH "/Screenshots", 'i' },
		{ "Whole card", SDCARD_PATH, 'c' },
		{ "Trash", BRAMLEY_STATE_DIR "/Trash", 't' },
	};
	memcpy(places, p, sizeof(p));
	fs.open = 1;
	read_space();
	read_folder(places[0].path);
	Wallpaper w;
	wallpaper_current(&w);
	if (w.blur[0]) fs.backdrop = ltex_load(w.blur, 0);
}

static void close_preview(void) {
	ltex_free(fs.preview);
	fs.preview = NULL;
	fs.previewing = fs.preview_tried = 0;
}

void lfiles_close(void) {
	if (!fs.open) return;
	close_preview();
	free(fs.entries);
	ltex_free(fs.backdrop);
	for (int i = 0; i < ntexts; i++) ltex_free(texts[i].tex);
	ntexts = 0;
	memset(&fs, 0, sizeof(fs));
}

int lfiles_is_open(void) {
	return fs.open;
}

///////////////////////////////////////////////////////////
// Input

static int place_root(void) {
	return !strcmp(fs.path, places[fs.place].path);
}

static int visible_rows(void) {
	// rows start under the title (and the back label inside a folder) and stop above the footer
	return (int)((H - 70 - (84 + 52 + 70) - 44) / (ROW_H + ROW_GAP));
}

static void keep_in_view(void) {
	int v = visible_rows();
	if (fs.focus < fs.top) fs.top = fs.focus;
	if (fs.focus >= fs.top + v) fs.top = fs.focus - v + 1;
}

int lfiles_input(Card* play) {
	if (fs.previewing) {
		if (PAD_justPressed(BTN_B) || PAD_justPressed(BTN_A)) {
			close_preview();
			return FILES_REDRAW;
		}
		return FILES_NONE;
	}
	if (!fs.in_pane) {
		if (PAD_justPressed(BTN_B)) return FILES_CLOSE;
		if (PAD_justRepeated(BTN_UP) && fs.place > 0) {
			fs.place--;
			read_folder(places[fs.place].path);
			return FILES_REDRAW;
		}
		if (PAD_justRepeated(BTN_DOWN) && fs.place < 5) {
			fs.place++;
			read_folder(places[fs.place].path);
			return FILES_REDRAW;
		}
		if ((PAD_justPressed(BTN_A) || PAD_justPressed(BTN_RIGHT)) && fs.count) {
			fs.in_pane = 1;
			return FILES_REDRAW;
		}
		return FILES_NONE;
	}
	if (PAD_justPressed(BTN_B) || (PAD_justPressed(BTN_LEFT))) {
		if (place_root()) fs.in_pane = 0;
		else {
			// up one folder, keeping the one we came from in focus
			char from[256];
			char* slash = strrchr(fs.path, '/');
			snprintf(from, sizeof(from), "%s", slash ? slash + 1 : "");
			if (slash) *slash = 0;
			char up[1024];
			snprintf(up, sizeof(up), "%s", fs.path);
			read_folder(up);
			for (int i = 0; i < fs.count; i++)
				if (!strcmp(fs.entries[i].name, from)) fs.focus = i;
			keep_in_view();
		}
		return FILES_REDRAW;
	}
	if (PAD_justRepeated(BTN_UP) && fs.focus > 0) {
		fs.focus--;
		keep_in_view();
		return FILES_REDRAW;
	}
	if (PAD_justRepeated(BTN_DOWN) && fs.focus < fs.count - 1) {
		fs.focus++;
		keep_in_view();
		return FILES_REDRAW;
	}
	if (PAD_justRepeated(BTN_L1) || PAD_justRepeated(BTN_R1)) {
		int v = visible_rows();
		fs.focus += PAD_justRepeated(BTN_R1) ? v : -v;
		if (fs.focus < 0) fs.focus = 0;
		if (fs.focus > fs.count - 1) fs.focus = fs.count - 1;
		keep_in_view();
		return FILES_REDRAW;
	}
	if (PAD_justPressed(BTN_A) && fs.count) {
		Entry* e = &fs.entries[fs.focus];
		char full[1024];
		snprintf(full, sizeof(full), "%s/%s", fs.path, e->name);
		if (e->is_dir) {
			read_folder(full);
			return FILES_REDRAW;
		}
		if (e->kind == 'i') {
			fs.previewing = 1;
			return FILES_REDRAW;
		}
		if (e->kind == 'g') {
			memset(play, 0, sizeof(*play));
			play->kind = CARD_GAME;
			snprintf(play->path, sizeof(play->path), "%s", full);
			getDisplayName(e->name, play->name);
			return FILES_PLAY;
		}
	}
	return FILES_NONE;
}

int lfiles_update(void) {
	if (!fs.open || !fs.previewing || fs.preview_tried) return 0;
	fs.preview_tried = 1;
	char full[1024];
	snprintf(full, sizeof(full), "%s/%s", fs.path, fs.entries[fs.focus].name);
	fs.preview = ltex_load(full, 0);
	return 1;
}

///////////////////////////////////////////////////////////
// Drawing

// The colored tiles (option C in spikes/files-mock): one color per kind, a white glyph on each.
static void tile(char kind, float x, float y, float s, float alpha) {
	float r = 0.56f, g = 0.56f, b = 0.58f;
	if (kind == 'f') r = 0.290f, g = 0.698f, b = 0.471f;      // folder: the accent
	else if (kind == 'g') r = 0.36f, g = 0.42f, b = 0.95f;    // game: blue
	else if (kind == 's') r = 1.0f, g = 0.62f, b = 0.04f;     // save: orange
	else if (kind == 'i') r = 0.85f, g = 0.45f, b = 0.65f;    // image: rose
	else if (kind == 'm') r = 0.65f, g = 0.50f, b = 0.90f;    // music: violet
	const float top[4] = { fminf(1, r * 1.1f), fminf(1, g * 1.1f), fminf(1, b * 1.1f), alpha }, bot[4] = { r * 0.75f, g * 0.75f, b * 0.75f, alpha };
	lgfx_draw_gradient(x, y, s, s, s * 0.24f, top, bot);
	float gx = x + s * 0.18f, gy = y + s * 0.18f, q = s * 0.64f;
	switch (kind) {
	case 'f':
		lgfx_draw_fill(gx + q * .1f, gy + q * .22f, q * .38f, q * .14f, q * .05f, 1, 1, 1, alpha);
		lgfx_draw_fill(gx + q * .1f, gy + q * .3f, q * .8f, q * .5f, q * .08f, 1, 1, 1, alpha);
		break;
	case 'g':
		lgfx_draw_fill(gx + q * .22f, gy + q * .1f, q * .56f, q * .8f, q * .08f, 1, 1, 1, alpha);
		lgfx_draw_fill(gx + q * .32f, gy + q * .2f, q * .36f, q * .3f, q * .04f, r * .75f, g * .75f, b * .75f, alpha);
		break;
	case 's':
		lgfx_draw_fill(gx + q * .15f, gy + q * .15f, q * .7f, q * .7f, q * .1f, 1, 1, 1, alpha);
		lgfx_draw_fill(gx + q * .3f, gy + q * .15f, q * .4f, q * .22f, q * .02f, r * .75f, g * .75f, b * .75f, alpha);
		break;
	case 'i':
		lgfx_draw_fill(gx + q * .1f, gy + q * .2f, q * .8f, q * .6f, q * .08f, 1, 1, 1, alpha);
		lgfx_draw_fill(gx + q * .2f, gy + q * .5f, q * .6f, q * .2f, q * .05f, r * .75f, g * .75f, b * .75f, alpha);
		break;
	case 'm':
		lgfx_draw_fill(gx + q * .5f, gy + q * .12f, q * .12f, q * .58f, 0, 1, 1, 1, alpha);
		lgfx_draw_fill(gx + q * .26f, gy + q * .56f, q * .34f, q * .3f, q * .15f, 1, 1, 1, alpha);
		break;
	case 'c':
		lgfx_draw_fill(gx + q * .22f, gy + q * .1f, q * .56f, q * .8f, q * .06f, 1, 1, 1, alpha);
		break;
	case 't':
		lgfx_draw_fill(gx + q * .25f, gy + q * .3f, q * .5f, q * .58f, q * .06f, 1, 1, 1, alpha);
		lgfx_draw_fill(gx + q * .16f, gy + q * .16f, q * .68f, q * .09f, q * .04f, 1, 1, 1, alpha);
		break;
	default:
		lgfx_draw_fill(gx + q * .22f, gy + q * .1f, q * .56f, q * .8f, q * .06f, 1, 1, 1, alpha);
	}
}

static void backdrop(void) {
	if (fs.backdrop) lgfx_draw_tex(fs.backdrop, 0, 0, W, H, 0, 1);
	else {
		const float top[4] = { 0.20f, 0.17f, 0.36f, 1 }, bottom[4] = { 0.10f, 0.12f, 0.22f, 1 };
		lgfx_draw_gradient(0, 0, W, H, 0, top, bottom);
	}
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.30f);
}

static void draw_sidebar(void) {
	float x = 40, w = 330, rh = 46, gap = 5, y = 84;
	for (int i = 0; i < 6; i++) {
		if (i == 4) y += 12;
		int f = i == fs.place, open = fs.in_pane;
		float fx = x, fw = w, fy = y, fh = rh;
		if (f && !open) {
			fx -= 8, fw += 16, fy -= 3, fh += 6;
			lgfx_draw_shadow(fx, fy, fw, fh, 6, 0.32f);
			lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, 0.96f);
		} else
			lgfx_draw_fill(fx, fy, fw, fh, 14, 1, 1, 1, f ? 0.22f : 0.07f);
		tile(places[i].kind, fx + 14, fy + (fh - 32) * 0.5f, 32, open && !f ? 0.5f : 1);
		if (f && !open) lgfx_set_dim(0.1f);
		text(LFONT_BODY, places[i].name, fx + 60, fy + (fh - 30) * 0.5f, open ? (f ? 0.95f : 0.45f) : 1, 0);
		lgfx_set_dim(1);
		y += rh + gap;
	}
	if (fs.card_bytes > 0) {
		char a[32], b[32], line[96];
		size_text(fs.free_bytes, a, sizeof(a));
		size_text(fs.card_bytes, b, sizeof(b));
		snprintf(line, sizeof(line), "%s free of %s", a, b);
		text(LFONT_SMALL, "SD card", 48, y + 26, 0.55f, 0);
		text(LFONT_SMALL, line, 362, y + 26, 0.55f, 1);
		lgfx_draw_fill(48, y + 56, 314, 6, 3, 1, 1, 1, 0.15f);
		lgfx_draw_fill(48, y + 56, 314.0f * (fs.card_bytes - fs.free_bytes) / fs.card_bytes, 6, 3, BRAMLEY_ACCENT, 1);
	}
}

static void draw_row(Entry* e, float x, float y, float w, float h, int focused) {
	if (focused) {
		x -= 6, w += 12, y -= 3, h += 6;
		lgfx_draw_shadow(x, y, w, h, 6, 0.32f);
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.96f);
	} else
		lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f);
	tile(e->kind, x + 14, y + (h - 34) * 0.5f, 34, 1);
	float tx = x + 62;
	char right[48], detail[200], when[48];
	date_text(e->mtime, when, sizeof(when));
	if (e->is_dir) {
		if (e->items < 0) {
			char full[1024];
			snprintf(full, sizeof(full), "%s/%s", fs.path, e->name);
			e->items = count_items(full);
		}
		snprintf(right, sizeof(right), "Folder");
		if (e->items == 1) snprintf(detail, sizeof(detail), "1 item");
		else snprintf(detail, sizeof(detail), "%d items", e->items);
	} else {
		size_text(e->size, right, sizeof(right));
		char console[128];
		console_of(fs.path, console, sizeof(console));
		// the console, when this folder is not one console's own
		if (e->kind == 'g' && console[0] && !fs.console[0]) snprintf(detail, sizeof(detail), "%s  \xC2\xB7  %s", console, when);
		else snprintf(detail, sizeof(detail), "%s", when);
	}
	if (focused) lgfx_set_dim(0.1f);
	float rw = text(LFONT_BODY, right, x + w - 20, y + (h - 30) * 0.5f, focused ? 1 : 0.55f, 1);
	text_fit(LFONT_BODY, e->name, tx, y + 4, 1, w - (tx - x) - rw - 40);
	text_fit(LFONT_SMALL, detail, tx, y + 32, focused ? 0.7f : 0.5f, w - (tx - x) - rw - 40);
	lgfx_set_dim(1);
}

static void draw_pane(void) {
	float px = 410, pw = W - 450, py = 84, pb = H - 70;
	lgfx_draw_fill(px, py, pw, pb - py, 22, 0, 0, 0, 0.18f);
	// title: the folder's name, and the folder above it as a back label
	const Place* p = &places[fs.place];
	const char* name = place_root() ? p->name : strrchr(fs.path, '/') + 1;
	char shown[160];
	snprintf(shown, sizeof(shown), "%s", name);
	char* paren = strrchr(shown, '(');
	if (paren && paren > shown && strchr(paren, ')')) { // "Game Boy Advance (GBA)" -> "Game Boy Advance"
		*paren = 0;
		size_t l = strlen(shown);
		while (l && shown[l - 1] == ' ') shown[--l] = 0;
	}
	float ty = py + 20;
	if (!place_root()) {
		char back[200], up[1024];
		snprintf(up, sizeof(up), "%s", fs.path);
		char* s = strrchr(up, '/');
		if (s) *s = 0;
		const char* upname = !strcmp(up, p->path) ? p->name : (strrchr(up, '/') ? strrchr(up, '/') + 1 : up);
		snprintf(back, sizeof(back), "\xE2\x80\xB9  %s", upname);
		text(LFONT_BODY, back, px + 28, py + 18, 0.5f, 0);
		ty = py + 52;
	}
	text(LFONT_TITLE, shown, px + 28, ty, 1, 0);
	float y = ty + 70;
	if (!fs.count) {
		const char* empty = fs.place == 5 ? "Trash is empty" : "Nothing here";
		text(LFONT_BODY, empty, px + pw * 0.5f, py + 260, 0.5f, 2);
		return;
	}
	int v = visible_rows();
	for (int i = fs.top; i < fs.count && i < fs.top + v; i++, y += ROW_H + ROW_GAP)
		draw_row(&fs.entries[i], px + 20, y, pw - 40, ROW_H, fs.in_pane && i == fs.focus);
	if (fs.top > 0) text(LFONT_SMALL, "\xE2\x96\xB2", px + pw * 0.5f, py + 96, 0.35f, 2);
	if (fs.top + v < fs.count) text(LFONT_SMALL, "\xE2\x96\xBC", px + pw * 0.5f, y - 4, 0.35f, 2);
	char foot[96], total[32];
	size_text(fs.total, total, sizeof(total));
	char items[32];
	if (fs.count == 1) snprintf(items, sizeof(items), "1 item");
	else snprintf(items, sizeof(items), "%d items", fs.count);
	if (fs.total > 0) snprintf(foot, sizeof(foot), "%s  \xC2\xB7  %s", items, total);
	else snprintf(foot, sizeof(foot), "%s", items);
	text(LFONT_SMALL, foot, px + 28, pb - 34, 0.5f, 0);
}

static void draw_preview(void) {
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.85f);
	Entry* e = &fs.entries[fs.focus];
	if (fs.preview) {
		float mw = W - 120, mh = H - 180, s = fminf(mw / fs.preview->w, mh / fs.preview->h);
		if (s > 4) s = 4;
		float w = fs.preview->w * s, h = fs.preview->h * s;
		lgfx_draw_shadow((W - w) * 0.5f, 70 + (mh - h) * 0.5f, w, h, 10, 0.5f);
		lgfx_draw_tex(fs.preview, (W - w) * 0.5f, 70 + (mh - h) * 0.5f, w, h, 8, 1);
	} else if (fs.preview_tried)
		text(LFONT_BODY, "This picture cannot be shown", W * 0.5f, H * 0.5f - 15, 0.6f, 2);
	text_fit(LFONT_BODY, e->name, 60, H - 92, 0.9f, W - 120);
	text(LFONT_SMALL, "B  Close", 40, H - 40, 0.6f, 0);
}

void lfiles_draw(void) {
	backdrop();
	text(LFONT_LABEL, "Files", 48, 30, 0.55f, 0);
	draw_sidebar();
	draw_pane();
	const char* hint = "A or \xE2\x80\xBA  Open        B  Back";
	if (fs.in_pane && fs.count) {
		char k = fs.entries[fs.focus].kind;
		int dir = fs.entries[fs.focus].is_dir;
		hint = dir ? "A  Open        L1 R1  Page        B  Back" : k == 'g' ? "A  Play        L1 R1  Page        B  Back"
		                     : k == 'i' ? "A  View        L1 R1  Page        B  Back" : "L1 R1  Page        B  Back";
	}
	if (fs.previewing) draw_preview(); // it has its own hint
	else text(LFONT_SMALL, hint, 40, H - 40, 0.6f, 0);
}
