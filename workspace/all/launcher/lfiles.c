// Files: Bramley's file browser. See lfiles.h.
#include "lfiles.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
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
	int selected;
	char from[512]; // in Trash: where it was, relative to the card
} Entry;

// What the actions do, chosen in the sheet or confirmed in the card.
enum { A_COPY, A_MOVE, A_RENAME, A_NEWFOLDER, A_TRASH, A_PUTBACK, A_DELETE, A_EMPTY };
#define TRASH_DIR BRAMLEY_STATE_DIR "/Trash"
#define ORIGINS TRASH_DIR "/.origins"
#define MAX_CLIP 256

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
	// the actions sheet (Y)
	int sheet, sheet_focus, sheet_n, sheet_ids[8];
	// a confirmation card: Cancel is focused first
	int confirm, confirm_ok_focused, confirm_action;
	char confirm_title[160], confirm_body[400], confirm_ok[48];
	// copy or move: the items, chosen before going to the folder to paste them in
	char (*clip)[1024];
	int nclip, clip_move;
	// a message card, dismissed with A or B
	int message;
	char message_title[160], message_body[400];
	// a short note at the bottom, for things that went as expected
	char toast[160];
	Uint32 toast_until;
	// copying, on its own thread
	SDL_Thread* copier;
	SDL_atomic_t copying, cancel;
	volatile long long copy_done, copy_total;
	volatile int copy_item, copy_items, copy_result; // result: 0 done, 1 cancelled, 2 failed
	char copy_name[256], copy_error[256], copy_dest[1024];
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

static int in_trash(const char* path) {
	return !strcmp(path, TRASH_DIR);
}

static int visible_name(const char* dir, const char* name) {
	if (!strcmp(name, ".") || !strcmp(name, "..")) return 0;
	if (in_trash(dir)) return strcmp(name, ".origins") != 0;
	if (name[0] == '.') return !strcmp(name, ".media"); // hidden, except the box art folders
	return 1;
}

// Trash remembers where each item came from: lines of "name<TAB>original path".
static int origin_of(const char* name, char* out, size_t n) {
	FILE* f = fopen(ORIGINS, "r");
	if (!f) return 0;
	char line[1600];
	int found = 0;
	while (fgets(line, sizeof(line), f)) {
		char* tab = strchr(line, '\t');
		if (!tab) continue;
		*tab = 0;
		if (strcmp(line, name)) continue;
		char* nl = strchr(tab + 1, '\n');
		if (nl) *nl = 0;
		snprintf(out, n, "%s", tab + 1);
		found = 1;
	}
	fclose(f);
	return found;
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
			if (!visible_name(path, dp->d_name)) continue;
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
			e->selected = 0;
			e->from[0] = 0;
			if (in_trash(path)) {
				char o[1024];
				if (origin_of(e->name, o, sizeof(o))) {
					const char* rel = !strncmp(o, SDCARD_PATH "/", strlen(SDCARD_PATH) + 1) ? o + strlen(SDCARD_PATH) + 1 : o;
					char* slash = strrchr(o, '/');
					snprintf(e->from, sizeof(e->from), "%.*s", slash ? (int)(slash - rel) : 0, rel);
					// the tile of what it was where it came from (a game in Trash is still a game)
					if (slash) {
						*slash = 0;
						e->kind = kind_of(o, e->name, e->is_dir);
					}
				}
			}
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
		if (visible_name(path, dp->d_name)) n++;
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
// Changing files. Nothing is ever overwritten: a name already taken gets " 2", " 3" and so on.
// Deleting means moving to Trash, which only the user empties.

extern int bramley_text_input(const char* app, const char* title, const char* initial, char* out, size_t size,
                              void (*backdrop)(void), void (*after)(void));

// Saves hold progress: changing them asks first.
static int is_protected(const char* path) {
	size_t n = strlen(SDCARD_PATH "/Saves");
	return !strncmp(path, SDCARD_PATH "/Saves", n) && (path[n] == '/' || path[n] == 0);
}

static int path_exists(const char* p) {
	struct stat st;
	return lstat(p, &st) == 0;
}

// dir/name, or dir/"name 2.ext" and so on when that is taken.
static void unique_path(const char* dir, const char* name, char* out, size_t n) {
	snprintf(out, n, "%s/%s", dir, name);
	if (!path_exists(out)) return;
	char base[256], ext[64] = "";
	snprintf(base, sizeof(base), "%s", name);
	char* dot = strrchr(base, '.');
	struct stat st;
	char probe[1024];
	snprintf(probe, sizeof(probe), "%s/%s", dir, name);
	int is_dir = !stat(probe, &st) && S_ISDIR(st.st_mode);
	if (dot && dot != base && !is_dir) {
		snprintf(ext, sizeof(ext), "%s", dot);
		*dot = 0;
	}
	for (int i = 2; i < 10000; i++) {
		snprintf(out, n, "%s/%s %d%s", dir, base, i, ext);
		if (!path_exists(out)) return;
	}
}

static long long tree_size(const char* p) {
	struct stat st;
	if (lstat(p, &st)) return 0;
	if (!S_ISDIR(st.st_mode)) return st.st_size;
	long long t = 0;
	DIR* dh = opendir(p);
	if (!dh) return 0;
	struct dirent* dp;
	while ((dp = readdir(dh))) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) continue;
		char c[1024];
		snprintf(c, sizeof(c), "%s/%s", p, dp->d_name);
		t += tree_size(c);
	}
	closedir(dh);
	return t;
}

static int remove_tree(const char* p) {
	struct stat st;
	if (lstat(p, &st)) return -1;
	if (!S_ISDIR(st.st_mode)) return unlink(p);
	DIR* dh = opendir(p);
	if (dh) {
		struct dirent* dp;
		while ((dp = readdir(dh))) {
			if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) continue;
			char c[1024];
			snprintf(c, sizeof(c), "%s/%s", p, dp->d_name);
			remove_tree(c);
		}
		closedir(dh);
	}
	return rmdir(p);
}

// Copies a file or folder to dst, which must not exist. Stops when cancel is set.
static int copy_tree(const char* src, const char* dst) {
	struct stat st;
	if (lstat(src, &st)) return -1;
	if (S_ISDIR(st.st_mode)) {
		if (mkdir(dst, 0755)) return -1;
		DIR* dh = opendir(src);
		if (!dh) return -1;
		struct dirent* dp;
		int r = 0;
		while (!r && (dp = readdir(dh))) {
			if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")) continue;
			char a[1024], b[1024];
			snprintf(a, sizeof(a), "%s/%s", src, dp->d_name);
			snprintf(b, sizeof(b), "%s/%s", dst, dp->d_name);
			r = copy_tree(a, b);
		}
		closedir(dh);
		return r;
	}
	int in = open(src, O_RDONLY);
	if (in < 0) return -1;
	int out = open(dst, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (out < 0) {
		close(in);
		return -1;
	}
	static char buf[1 << 20];
	ssize_t got;
	int r = 0;
	while ((got = read(in, buf, sizeof(buf))) > 0) {
		if (SDL_AtomicGet(&fs.cancel)) {
			r = 1;
			break;
		}
		if (write(out, buf, got) != got) {
			r = -1;
			break;
		}
		fs.copy_done += got;
	}
	if (got < 0) r = -1;
	if (fsync(out)) r = r ? r : -1;
	close(out);
	close(in);
	return r;
}

static int copy_thread(void* unused) {
	(void)unused;
	fs.copy_result = 0;
	for (int i = 0; i < fs.nclip; i++) {
		fs.copy_item = i + 1;
		const char* src = fs.clip[i];
		const char* name = strrchr(src, '/') ? strrchr(src, '/') + 1 : src;
		snprintf(fs.copy_name, sizeof(fs.copy_name), "%s", name);
		char dst[1024];
		unique_path(fs.copy_dest, name, dst, sizeof(dst));
		int r = copy_tree(src, dst);
		if (r) {
			remove_tree(dst); // the item copied only in part is removed; finished ones stay
			fs.copy_result = r == 1 ? 1 : 2;
			if (r != 1) snprintf(fs.copy_error, sizeof(fs.copy_error), "%s", strerror(errno));
			break;
		}
	}
	sync();
	SDL_AtomicSet(&fs.copying, 0);
	return 0;
}

static void show_message(const char* title, const char* body) {
	fs.message = 1;
	snprintf(fs.message_title, sizeof(fs.message_title), "%s", title);
	snprintf(fs.message_body, sizeof(fs.message_body), "%s", body ? body : "");
}

static void show_toast(const char* t) {
	snprintf(fs.toast, sizeof(fs.toast), "%s", t);
	fs.toast_until = SDL_GetTicks() + 2200;
}

static void items_text(int n, char* out, size_t size) {
	if (n == 1) snprintf(out, size, "1 item");
	else snprintf(out, size, "%d items", n);
}

// The files an action works on: the selection, or the focused file.
static int targets(int* idx, int max) {
	int n = 0;
	for (int i = 0; i < fs.count && n < max; i++)
		if (fs.entries[i].selected) idx[n++] = i;
	if (!n && fs.count) idx[n++] = fs.focus;
	return n;
}

static void full_path(int i, char* out, size_t n) {
	snprintf(out, n, "%s/%s", fs.path, fs.entries[i].name);
}

static void reread(const char* keep) {
	char path[1024];
	snprintf(path, sizeof(path), "%s", fs.path);
	int focus = fs.focus, top = fs.top;
	read_folder(path);
	fs.focus = focus < fs.count ? focus : fs.count - 1;
	if (fs.focus < 0) fs.focus = 0;
	fs.top = top;
	if (keep)
		for (int i = 0; i < fs.count; i++)
			if (!strcmp(fs.entries[i].name, keep)) fs.focus = i;
	if (fs.top > fs.focus) fs.top = fs.focus;
	read_space();
}

static void trash_targets(void) {
	int idx[MAX_CLIP], n = targets(idx, MAX_CLIP), done = 0;
	mkdir(BRAMLEY_STATE_DIR, 0755);
	mkdir(TRASH_DIR, 0755);
	FILE* o = fopen(ORIGINS, "a");
	for (int k = 0; k < n; k++) {
		char src[1024], dst[1024];
		full_path(idx[k], src, sizeof(src));
		unique_path(TRASH_DIR, fs.entries[idx[k]].name, dst, sizeof(dst));
		if (rename(src, dst)) continue;
		if (o) fprintf(o, "%s\t%s\n", strrchr(dst, '/') + 1, src);
		done++;
	}
	if (o) fclose(o);
	sync();
	char t[96], it[32];
	items_text(done, it, sizeof(it));
	if (done == n) snprintf(t, sizeof(t), "Moved %s to Trash", it);
	else snprintf(t, sizeof(t), "Moved %d of %d to Trash", done, n);
	reread(NULL);
	if (done == n) show_toast(t);
	else show_message("Some items stayed", "They could not be moved to Trash. The card may be read only.");
}

// Rewrites the origins list without the given names.
static void forget_origins(int* idx, int n) {
	FILE* f = fopen(ORIGINS, "r");
	if (!f) return;
	FILE* o = fopen(ORIGINS ".new", "w");
	char line[1600];
	while (o && fgets(line, sizeof(line), f)) {
		char name[512];
		char* tab = strchr(line, '\t');
		if (!tab) continue;
		snprintf(name, sizeof(name), "%.*s", (int)(tab - line), line);
		int drop = 0;
		for (int k = 0; k < n; k++)
			if (!strcmp(fs.entries[idx[k]].name, name)) drop = 1;
		if (!drop) fputs(line, o);
	}
	fclose(f);
	if (o) {
		fclose(o);
		rename(ORIGINS ".new", ORIGINS);
	}
}

static void make_parents(const char* path) {
	char p[1024];
	snprintf(p, sizeof(p), "%s", path);
	for (char* c = p + 1; *c; c++)
		if (*c == '/') {
			*c = 0;
			mkdir(p, 0755);
			*c = '/';
		}
}

static void put_back(void) {
	int idx[MAX_CLIP], n = targets(idx, MAX_CLIP), done = 0;
	for (int k = 0; k < n; k++) {
		char src[1024], orig[1024], dst[1024];
		full_path(idx[k], src, sizeof(src));
		if (!origin_of(fs.entries[idx[k]].name, orig, sizeof(orig))) continue;
		char dir[1024];
		snprintf(dir, sizeof(dir), "%s", orig);
		char* slash = strrchr(dir, '/');
		if (!slash) continue;
		*slash = 0;
		make_parents(orig);
		unique_path(dir, slash + 1, dst, sizeof(dst));
		if (!rename(src, dst)) done++;
	}
	forget_origins(idx, n);
	sync();
	char t[96], it[32];
	items_text(done, it, sizeof(it));
	snprintf(t, sizeof(t), "Put back %s", it);
	reread(NULL);
	show_toast(t);
}

static void delete_targets(int all) {
	int idx[MAX_CLIP], n;
	if (all) {
		n = 0;
		for (int i = 0; i < fs.count && n < MAX_CLIP; i++) idx[n++] = i;
	} else
		n = targets(idx, MAX_CLIP);
	for (int k = 0; k < n; k++) {
		char p[1024];
		full_path(idx[k], p, sizeof(p));
		remove_tree(p);
	}
	if (all) unlink(ORIGINS);
	else forget_origins(idx, n);
	sync();
	reread(NULL);
	show_toast(all ? "Trash emptied" : "Deleted");
}

void launcher_debug_tick(void);

// Behind the keyboard: the wallpaper, as in Files.
static void backdrop(void);
static void keyboard_backdrop(void) {
	backdrop();
}

static void rename_target(void) {
	Entry* e = &fs.entries[fs.focus];
	char name[256];
	if (!bramley_text_input("Files", "Rename", e->name, name, sizeof(name), keyboard_backdrop, launcher_debug_tick)) return;
	if (!name[0] || !strcmp(name, e->name)) return;
	if (strchr(name, '/') || !strcmp(name, ".") || !strcmp(name, "..")) {
		show_message("That name cannot be used", "Names cannot contain a slash.");
		return;
	}
	char src[1024], dst[1024];
	full_path(fs.focus, src, sizeof(src));
	snprintf(dst, sizeof(dst), "%s/%s", fs.path, name);
	if (path_exists(dst) && strcasecmp(name, e->name)) {
		show_message("That name is taken", "Another file in this folder already has it.");
		return;
	}
	if (rename(src, dst)) {
		show_message("Could not rename", strerror(errno));
		return;
	}
	sync();
	reread(name);
}

static void new_folder(void) {
	char name[256];
	if (!bramley_text_input("Files", "New Folder", "New Folder", name, sizeof(name), keyboard_backdrop, launcher_debug_tick)) return;
	if (!name[0] || strchr(name, '/')) return;
	char dst[1024];
	unique_path(fs.path, name, dst, sizeof(dst));
	if (mkdir(dst, 0755)) {
		show_message("Could not make the folder", strerror(errno));
		return;
	}
	sync();
	reread(strrchr(dst, '/') + 1);
}

static void start_clip(int move) {
	int idx[MAX_CLIP], n = targets(idx, MAX_CLIP);
	free(fs.clip);
	fs.clip = calloc(n, sizeof(*fs.clip));
	if (!fs.clip) return;
	for (int k = 0; k < n; k++) full_path(idx[k], fs.clip[k], sizeof(fs.clip[k]));
	fs.nclip = n;
	fs.clip_move = move;
	for (int i = 0; i < fs.count; i++) fs.entries[i].selected = 0;
}

static void end_clip(void) {
	free(fs.clip);
	fs.clip = NULL;
	fs.nclip = 0;
}

// Pastes the clip into the folder shown: a move renames in place, a copy runs on its thread.
static void paste(void) {
	if (in_trash(fs.path)) {
		show_message("Not into Trash", "Use Move to Trash instead.");
		return;
	}
	for (int k = 0; k < fs.nclip; k++) {
		size_t l = strlen(fs.clip[k]);
		if (!strncmp(fs.path, fs.clip[k], l) && (fs.path[l] == '/' || fs.path[l] == 0)) {
			show_message("Not into itself", "A folder cannot go inside itself. Choose another folder.");
			return;
		}
	}
	if (fs.clip_move) {
		int done = 0;
		for (int k = 0; k < fs.nclip; k++) {
			const char* name = strrchr(fs.clip[k], '/') + 1;
			char dst[1024], here[1024];
			snprintf(here, sizeof(here), "%s/%s", fs.path, name);
			if (!strcmp(here, fs.clip[k])) { // already here
				done++;
				continue;
			}
			unique_path(fs.path, name, dst, sizeof(dst));
			if (!rename(fs.clip[k], dst)) done++;
		}
		sync();
		char t[96], it[32];
		items_text(done, it, sizeof(it));
		snprintf(t, sizeof(t), "Moved %s", it);
		end_clip();
		reread(NULL);
		if (done) show_toast(t);
		else show_message("Nothing was moved", "The card may be read only.");
		return;
	}
	long long need = 0;
	for (int k = 0; k < fs.nclip; k++) need += tree_size(fs.clip[k]);
	read_space();
	if (fs.card_bytes && need + (1 << 20) > fs.free_bytes) {
		char a[32], b[32], body[200];
		size_text(need, a, sizeof(a));
		size_text(fs.free_bytes, b, sizeof(b));
		snprintf(body, sizeof(body), "This needs %s and the card has %s free. Nothing was copied.", a, b);
		show_message("Not enough space", body);
		return;
	}
	snprintf(fs.copy_dest, sizeof(fs.copy_dest), "%s", fs.path);
	fs.copy_total = need;
	fs.copy_done = 0;
	fs.copy_items = fs.nclip;
	fs.copy_item = 0;
	SDL_AtomicSet(&fs.cancel, 0);
	SDL_AtomicSet(&fs.copying, 1);
	fs.copier = SDL_CreateThread(copy_thread, "files-copy", NULL);
	if (!fs.copier) {
		SDL_AtomicSet(&fs.copying, 0);
		show_message("Could not start copying", NULL);
	}
}

// Called when the copy thread has finished.
static void copy_finished(void) {
	SDL_WaitThread(fs.copier, NULL);
	fs.copier = NULL;
	char it[32], t[128];
	items_text(fs.copy_items, it, sizeof(it));
	int result = fs.copy_result;
	end_clip();
	reread(NULL);
	if (result == 0) {
		snprintf(t, sizeof(t), "Copied %s", it);
		show_toast(t);
	} else if (result == 1) {
		snprintf(t, sizeof(t), "Copying stopped. %d of %d finished.", fs.copy_item - 1, fs.copy_items);
		show_toast(t);
	} else {
		char body[400];
		snprintf(body, sizeof(body), "%s could not be copied (%s). Items before it were copied; the part copied of it was removed.",
		         fs.copy_name, fs.copy_error);
		show_message("Copying stopped", body);
	}
}

static void ask(int action, const char* title, const char* body, const char* ok) {
	fs.confirm = 1;
	fs.confirm_ok_focused = 0;
	fs.confirm_action = action;
	snprintf(fs.confirm_title, sizeof(fs.confirm_title), "%s", title);
	snprintf(fs.confirm_body, sizeof(fs.confirm_body), "%s", body);
	snprintf(fs.confirm_ok, sizeof(fs.confirm_ok), "%s", ok);
}

static void run(int action) {
	switch (action) {
	case A_COPY: start_clip(0); break;
	case A_MOVE: start_clip(1); break;
	case A_RENAME: rename_target(); break;
	case A_NEWFOLDER: new_folder(); break;
	case A_TRASH: trash_targets(); break;
	case A_PUTBACK: put_back(); break;
	case A_DELETE: delete_targets(0); break;
	case A_EMPTY: delete_targets(1); break;
	}
}

// A chosen action: anything that changes a save, or deletes for good, asks first.
static void choose(int action) {
	int idx[MAX_CLIP], n = targets(idx, MAX_CLIP), prot = 0;
	for (int k = 0; k < n; k++) {
		char p[1024];
		full_path(idx[k], p, sizeof(p));
		prot |= is_protected(p);
	}
	char it[32], body[400];
	items_text(n, it, sizeof(it));
	const char* one = n == 1 ? fs.entries[idx[0]].name : NULL;
	if (action == A_DELETE) {
		snprintf(body, sizeof(body), "%s will be deleted for good. This cannot be undone.", one ? one : it);
		ask(action, n == 1 ? "Delete this now?" : "Delete these now?", body, "Delete");
	} else if (action == A_EMPTY) {
		items_text(fs.count, it, sizeof(it));
		snprintf(body, sizeof(body), "The %s in Trash will be deleted for good. This cannot be undone.", it);
		ask(action, "Empty Trash?", body, "Empty Trash");
	} else if (prot && (action == A_TRASH || action == A_MOVE || action == A_RENAME)) {
		const char* what = action == A_TRASH ? "Move to Trash" : action == A_MOVE ? "Move" : "Rename";
		if (one)
			snprintf(body, sizeof(body), "%s holds your progress in a game. %s", one,
			         action == A_TRASH ? "You can bring it back from Trash until you empty it." : "The game finds it only by its name and place.");
		else
			snprintf(body, sizeof(body), "These hold your progress in games. %s",
			         action == A_TRASH ? "You can bring them back from Trash until you empty it." : "A game finds its save only by its name and place.");
		char title[96];
		snprintf(title, sizeof(title), "%s %s?", action == A_TRASH ? "Move" : what, n == 1 ? (action == A_TRASH ? "this save to Trash" : "this save") : (action == A_TRASH ? "these saves to Trash" : "these saves"));
		ask(action, title, body, what);
	} else
		run(action);
}

static void open_sheet(void) {
	fs.sheet = 1;
	fs.sheet_focus = 0;
	fs.sheet_n = 0;
	if (in_trash(fs.path)) {
		if (fs.count) {
			fs.sheet_ids[fs.sheet_n++] = A_PUTBACK;
			fs.sheet_ids[fs.sheet_n++] = A_DELETE;
			fs.sheet_ids[fs.sheet_n++] = A_EMPTY;
		}
	} else {
		int idx[MAX_CLIP], n = fs.count ? targets(idx, MAX_CLIP) : 0;
		if (n) {
			fs.sheet_ids[fs.sheet_n++] = A_COPY;
			fs.sheet_ids[fs.sheet_n++] = A_MOVE;
			if (n == 1) fs.sheet_ids[fs.sheet_n++] = A_RENAME;
		}
		fs.sheet_ids[fs.sheet_n++] = A_NEWFOLDER;
		if (n) fs.sheet_ids[fs.sheet_n++] = A_TRASH;
	}
	if (!fs.sheet_n) fs.sheet = 0;
}

static const char* action_name(int a) {
	static const char* names[] = { "Copy", "Move", "Rename", "New Folder", "Move to Trash", "Put Back", "Delete Now", "Empty Trash" };
	return names[a];
}

static int destructive(int a) {
	return a == A_TRASH || a == A_DELETE || a == A_EMPTY;
}

static int selected_count(void) {
	int n = 0;
	for (int i = 0; i < fs.count; i++) n += fs.entries[i].selected;
	return n;
}

static void free_reds(void);

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
	if (fs.copier) {
		SDL_AtomicSet(&fs.cancel, 1);
		SDL_WaitThread(fs.copier, NULL);
		fs.copier = NULL;
	}
	end_clip();
	close_preview();
	free_reds();
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
	if (fs.copier) {
		if (PAD_justPressed(BTN_B)) SDL_AtomicSet(&fs.cancel, 1);
		return FILES_NONE;
	}
	if (fs.message) {
		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) {
			fs.message = 0;
			return FILES_REDRAW;
		}
		return FILES_NONE;
	}
	if (fs.confirm) {
		if (PAD_justPressed(BTN_LEFT) || PAD_justPressed(BTN_RIGHT)) {
			fs.confirm_ok_focused = !fs.confirm_ok_focused;
			return FILES_REDRAW;
		}
		if (PAD_justPressed(BTN_B) || (PAD_justPressed(BTN_A) && !fs.confirm_ok_focused)) {
			fs.confirm = 0;
			return FILES_REDRAW;
		}
		if (PAD_justPressed(BTN_A)) {
			fs.confirm = 0;
			run(fs.confirm_action);
			return FILES_REDRAW;
		}
		return FILES_NONE;
	}
	if (fs.sheet) {
		if (PAD_justRepeated(BTN_UP) && fs.sheet_focus > 0) fs.sheet_focus--;
		else if (PAD_justRepeated(BTN_DOWN) && fs.sheet_focus < fs.sheet_n - 1) fs.sheet_focus++;
		else if (PAD_justPressed(BTN_B) || PAD_justPressed(BTN_Y)) fs.sheet = 0;
		else if (PAD_justPressed(BTN_A)) {
			fs.sheet = 0;
			choose(fs.sheet_ids[fs.sheet_focus]);
		} else
			return FILES_NONE;
		return FILES_REDRAW;
	}
	// copy or move chosen: X pastes into the folder shown, Y cancels
	if (fs.clip && PAD_justPressed(BTN_X)) {
		paste();
		return FILES_REDRAW;
	}
	if (fs.clip && PAD_justPressed(BTN_Y)) {
		show_toast(fs.clip_move ? "Move cancelled" : "Copy cancelled");
		end_clip();
		return FILES_REDRAW;
	}
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
		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_RIGHT)) {
			fs.in_pane = 1;
			return FILES_REDRAW;
		}
		return FILES_NONE;
	}
	if (PAD_justPressed(BTN_B) && selected_count()) {
		for (int i = 0; i < fs.count; i++) fs.entries[i].selected = 0;
		return FILES_REDRAW;
	}
	if (PAD_justPressed(BTN_X) && fs.count) {
		fs.entries[fs.focus].selected = !fs.entries[fs.focus].selected;
		if (fs.focus < fs.count - 1) fs.focus++, keep_in_view();
		return FILES_REDRAW;
	}
	if (PAD_justPressed(BTN_Y)) {
		open_sheet();
		return FILES_REDRAW;
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
	if (!fs.open) return 0;
	if (fs.copier) {
		if (!SDL_AtomicGet(&fs.copying)) copy_finished();
		return 1;
	}
	if (fs.toast[0] && SDL_GetTicks() < fs.toast_until + 100) return 1;
	if (!fs.previewing || fs.preview_tried) return 0;
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
	float ix = x + 14;
	if (selected_count()) {
		float c = 22, cy = y + (h - c) * 0.5f;
		if (e->selected) {
			lgfx_draw_fill(ix, cy, c, c, c * 0.5f, BRAMLEY_ACCENT, 1);
			lgfx_draw_fill(ix + 7, cy + 7, 8, 8, 4, 1, 1, 1, 1);
		} else
			lgfx_draw_ring(ix, cy, c, c, c * 0.5f, 2, focused ? 0 : 1, focused ? 0 : 1, focused ? 0 : 1, 0.4f);
		ix += 34;
	}
	tile(e->kind, ix, y + (h - 34) * 0.5f, 34, 1);
	float tx = ix + 48;
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
	if (e->from[0]) { // in Trash: where it came from
		char d[300];
		snprintf(d, sizeof(d), "From %s", e->from);
		snprintf(detail, sizeof(detail), "%s", d);
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
	int sel = selected_count();
	if (sel) snprintf(foot, sizeof(foot), "%d selected", sel);
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


static void card(float w, float h, float* x, float* y) {
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.45f);
	*x = (W - w) * 0.5f, *y = (H - h) * 0.5f - 20;
	lgfx_draw_shadow(*x, *y, w, h, 14, 0.5f);
	lgfx_draw_fill(*x, *y, w, h, 26, 0.13f, 0.13f, 0.16f, 0.97f);
}

// Text in the alert red, for destructive actions, as Apple marks them. Kept until Files closes.
static struct {
	char s[48];
	LTex* tex;
} reds[8];

static void free_reds(void) {
	for (int i = 0; i < 8; i++) ltex_free(reds[i].tex);
	memset(reds, 0, sizeof(reds));
}

static void red_text(const char* str, float x, float y, int centre) {
	LTex* t = NULL;
	for (int i = 0; i < 8 && reds[i].tex; i++)
		if (!strcmp(reds[i].s, str)) t = reds[i].tex;
	if (!t) {
		int slot = 0;
		while (slot < 7 && reds[slot].tex) slot++;
		ltex_free(reds[slot].tex);
		TTF_Font* f = TTF_OpenFont(RES_PATH "/Inter-Regular.ttf", 23);
		SDL_Surface* sf = f ? TTF_RenderUTF8_Blended(f, str, (SDL_Color){ 255, 105, 97, 255 }) : NULL;
		t = sf ? ltex_from_surface(sf, 0) : NULL;
		if (sf) SDL_FreeSurface(sf);
		if (f) TTF_CloseFont(f);
		snprintf(reds[slot].s, sizeof(reds[slot].s), "%s", str);
		reds[slot].tex = t;
	}
	if (t) lgfx_draw_tex(t, centre ? x - t->w * 0.5f : x, y, t->w, t->h, 0, 1);
}

static void draw_sheet(void) {
	float w = 420, h = 96 + fs.sheet_n * 58 + 6, x, y;
	card(w, h, &x, &y);
	int idx[MAX_CLIP], n = in_trash(fs.path) ? targets(idx, MAX_CLIP) : (fs.count ? targets(idx, MAX_CLIP) : 0);
	char head[160], it[32], sz[32];
	long long b = 0;
	for (int k = 0; k < n; k++) b += fs.entries[idx[k]].is_dir ? 0 : fs.entries[idx[k]].size;
	items_text(n, it, sizeof(it));
	size_text(b, sz, sizeof(sz));
	if (n == 1) snprintf(head, sizeof(head), "%s", fs.entries[idx[0]].name);
	else if (n) snprintf(head, sizeof(head), "%s  \xC2\xB7  %s", it, sz);
	else snprintf(head, sizeof(head), "This folder");
	text_fit(LFONT_BODY, head, x + 30, y + 26, 0.6f, w - 60);
	float iy = y + 76;
	for (int i = 0; i < fs.sheet_n; i++, iy += 58) {
		int f = i == fs.sheet_focus, a = fs.sheet_ids[i];
		if (f) {
			lgfx_draw_fill(x + 14, iy, w - 28, 50, 12, 1, 1, 1, 0.96f);
			lgfx_set_dim(0.1f);
		}
		if (destructive(a)) {
			lgfx_set_dim(1);
			red_text(action_name(a), x + 34, iy + 10, 0);
		} else
			text(LFONT_BODY, action_name(a), x + 34, iy + 10, 1, 0);
		lgfx_set_dim(1);
	}
}

static void draw_confirm(void) {
	LTex* d = ltex_text_wrapped(LFONT_BODY, fs.confirm_body, 480, 5, 1);
	float w = 560, bh = 52, h = 36 + 40 + 16 + (d ? d->h : 0) + 28 + bh + 28, x, y;
	card(w, h, &x, &y);
	text(LFONT_TITLE, fs.confirm_title, x + w * 0.5f, y + 32, 1, 2);
	float cy = y + 92;
	if (d) {
		lgfx_draw_tex(d, x + (w - d->w) * 0.5f, cy, d->w, d->h, 0, 0.7f);
		cy += d->h;
		ltex_free(d);
	}
	float bw = 220, by = cy + 28;
	for (int b = 0; b < 2; b++) {
		int f = b == fs.confirm_ok_focused;
		float bx = b ? x + w * 0.5f + 10 : x + w * 0.5f - bw - 10;
		lgfx_draw_fill(bx, by, bw, bh, bh * 0.5f, 1, 1, 1, f ? 0.96f : 0.14f);
		if (b && destructive(fs.confirm_action)) red_text(fs.confirm_ok, bx + bw * 0.5f, by + 11, 1);
		else {
			if (f) lgfx_set_dim(0.1f);
			text(LFONT_BODY, b ? fs.confirm_ok : "Cancel", bx + bw * 0.5f, by + 11, 1, 2);
			lgfx_set_dim(1);
		}
	}
}

static void draw_message(void) {
	LTex* d = fs.message_body[0] ? ltex_text_wrapped(LFONT_BODY, fs.message_body, 480, 5, 1) : NULL;
	float w = 560, bh = 52, h = 36 + 40 + (d ? 16 + d->h : 0) + 28 + bh + 28, x, y;
	card(w, h, &x, &y);
	text(LFONT_TITLE, fs.message_title, x + w * 0.5f, y + 32, 1, 2);
	float cy = y + 76;
	if (d) {
		cy += 16;
		lgfx_draw_tex(d, x + (w - d->w) * 0.5f, cy, d->w, d->h, 0, 0.7f);
		cy += d->h;
		ltex_free(d);
	}
	float bw = 200, bx = x + (w - bw) * 0.5f, by = cy + 28;
	lgfx_draw_fill(bx, by, bw, bh, bh * 0.5f, 1, 1, 1, 0.96f);
	lgfx_set_dim(0.1f);
	text(LFONT_BODY, "OK", bx + bw * 0.5f, by + 11, 1, 2);
	lgfx_set_dim(1);
}

static void draw_progress(void) {
	float w = 560, h = 200, x, y;
	card(w, h, &x, &y);
	char t[96];
	snprintf(t, sizeof(t), fs.copy_items > 1 ? "Copying %d of %d" : "Copying", fs.copy_item, fs.copy_items);
	text(LFONT_TITLE, t, x + w * 0.5f, y + 30, 1, 2);
	text_fit(LFONT_BODY, fs.copy_name, x + 40, y + 84, 0.7f, w - 80);
	float f = fs.copy_total > 0 ? (float)fs.copy_done / fs.copy_total : 0;
	if (f > 1) f = 1;
	lgfx_draw_fill(x + 40, y + 134, w - 80, 8, 4, 1, 1, 1, 0.15f);
	lgfx_draw_fill(x + 40, y + 134, fmaxf(8, (w - 80) * f), 8, 4, BRAMLEY_ACCENT, 1);
	char a[32], b[32], line[96];
	size_text(fs.copy_done, a, sizeof(a));
	size_text(fs.copy_total, b, sizeof(b));
	snprintf(line, sizeof(line), "%s of %s", a, b);
	text(LFONT_SMALL, line, x + w - 40, y + 154, 0.55f, 1);
}

static void draw_toast(void) {
	if (!fs.toast[0] || SDL_GetTicks() > fs.toast_until) return;
	LTex* t = text_tex(LFONT_BODY, fs.toast);
	if (!t) return;
	float w = t->w + 76, h = 52, x = (W - w) * 0.5f, y = H - 88 - h;
	lgfx_draw_fill(x, y, w, h, h * 0.5f, 0.11f, 0.11f, 0.13f, 0.92f);
	lgfx_draw_fill(x + 20, y + 16, 20, 20, 10, BRAMLEY_ACCENT, 1);
	lgfx_draw_tex(t, x + 54, y + (h - t->h) * 0.5f, t->w, t->h, 0, 1);
}

// While copying or moving: a bar along the bottom saying what is waiting to be pasted.
static void draw_clip_bar(void) {
	char it[32], t[160];
	items_text(fs.nclip, it, sizeof(it));
	snprintf(t, sizeof(t), "%s %s: open the folder to put %s in", fs.clip_move ? "Moving" : "Copying", it, fs.nclip == 1 ? "it" : "them");
	float w = W - 450 - 40, x = 430, y = H - 70 - 58;
	lgfx_draw_fill(x, y, w, 46, 14, BRAMLEY_ACCENT, 0.95f);
	text_fit(LFONT_BODY, t, x + 18, y + 8, 1, w - 36);
}

void lfiles_draw(void) {
	backdrop();
	text(LFONT_LABEL, "Files", 48, 30, 0.55f, 0);
	draw_sidebar();
	draw_pane();
	char hint[200];
	snprintf(hint, sizeof(hint), "A or \xE2\x80\xBA  Open        B  Back");
	if (fs.in_pane) {
		const char* a = "";
		if (fs.count) {
			char k = fs.entries[fs.focus].kind;
			a = fs.entries[fs.focus].is_dir ? "A  Open        " : k == 'g' ? "A  Play        " : k == 'i' ? "A  View        " : "";
		}
		if (fs.clip) snprintf(hint, sizeof(hint), "%sX  Put here        Y  Cancel        B  Back", a);
		else if (selected_count()) snprintf(hint, sizeof(hint), "X  Select        Y  Actions        B  Clear");
		else snprintf(hint, sizeof(hint), "%sX  Select        Y  Actions        B  Back", a);
	} else if (fs.clip)
		snprintf(hint, sizeof(hint), "A or \xE2\x80\xBA  Open        X  Put here        Y  Cancel");
	if (fs.clip && !fs.copier) draw_clip_bar();
	if (fs.previewing) draw_preview(); // it has its own hint
	else if (fs.copier) {
		draw_progress();
		text(LFONT_SMALL, "B  Stop", 40, H - 40, 0.6f, 0);
	} else if (fs.message) {
		draw_message();
		text(LFONT_SMALL, "A  OK", 40, H - 40, 0.6f, 0);
	} else if (fs.confirm) {
		draw_confirm();
		text(LFONT_SMALL, "\xE2\x80\xB9 \xE2\x80\xBA  Choose        A  Confirm        B  Cancel", 40, H - 40, 0.6f, 0);
	} else if (fs.sheet) {
		draw_sheet();
		text(LFONT_SMALL, "A  Choose        B  Cancel", 40, H - 40, 0.6f, 0);
	} else {
		text(LFONT_SMALL, hint, 40, H - 40, 0.6f, 0);
		draw_toast();
	}
}
