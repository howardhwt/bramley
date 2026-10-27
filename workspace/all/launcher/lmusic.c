#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "defines.h"
#include "bramley.h"
#include "api.h"
#include "utils.h"
#include "lmuse.h"
#include "lmusic.h"

// What the launcher remembers between runs. The launcher exits for every game and Pak, so the
// queue is saved here and picked up again by the next launcher.
#define STATE_DIR BRAMLEY_STATE_DIR
#define STATE_PATH STATE_DIR "/music.txt"
#define BOOKS_PATH STATE_DIR "/books.txt"
#define MAX_BOOKS 64
// Positions are saved on every pause, seek and track change, and every 30 s while playing, since a
// power-off from the quick menu does not pass through lmusic_quit.
#define SAVE_MS 30000
#define MAX_FAILURES 8

static ml_lib lib;
static int scanned;
static int* shelf[2];
static int shelf_n[2];

static muq queue;
static int queue_album = -1;
static float speed = 1.0f;
static muq_mode music_mode = MUQ_IN_ORDER;
static int failures;
static Uint32 last_book_save;
static int game_pause;
static char title[256], next_title[256];
// A restored queue waits here, at its saved position, until the player presses play.
static float resume_at = -1;
// Set when the playing track changes; music.txt is saved once the daemon confirms it plays.
static int save_when_playing;

// Covers: 0 not looked for, 1 asked the daemon, 2 known to have none.
static unsigned char* cover_state;
static char cover_path[512];

typedef struct {
	char dir[512];
	char file[256];
	float at, speed;
} Bookmark;
static Bookmark books[MAX_BOOKS];
static int book_count;

static void full_path(const char* rel, char* out, size_t size) {
	snprintf(out, size, "%s/%s", SDCARD_PATH, rel);
}

static const char* file_of(const char* rel) {
	const char* slash = strrchr(rel, '/');
	return slash ? slash + 1 : rel;
}

// The folder an album's tracks are in, relative to the card.
static void album_dir(int al, char* out, size_t size) {
	snprintf(out, size, "%s", lib.tracks[lib.albums[al].first].path);
	char* slash = strrchr(out, '/');
	if (slash) *slash = '\0';
}

///////////////////////////////////////
// Audiobook bookmarks: books.txt holds dir, file, seconds and speed per book, tab separated.

static void load_books(void) {
	book_count = 0;
	FILE* f = fopen(BOOKS_PATH, "r");
	if (!f) return;
	char line[1024];
	while (book_count < MAX_BOOKS && fgets(line, sizeof(line), f)) {
		normalizeNewline(line);
		trimTrailingNewlines(line);
		char* parts[4];
		int n = 0;
		for (char* p = strtok(line, "\t"); p && n < 4; p = strtok(NULL, "\t")) parts[n++] = p;
		if (n < 4) continue;
		Bookmark* b = &books[book_count++];
		snprintf(b->dir, sizeof(b->dir), "%s", parts[0]);
		snprintf(b->file, sizeof(b->file), "%s", parts[1]);
		b->at = atof(parts[2]);
		b->speed = atof(parts[3]);
	}
	fclose(f);
}

static void save_books(void) {
	mkdir(STATE_DIR, 0755);
	FILE* f = fopen(BOOKS_PATH, "w");
	if (!f) return;
	for (int i = 0; i < book_count; i++)
		fprintf(f, "%s\t%s\t%.1f\t%.2f\n", books[i].dir, books[i].file, books[i].at, books[i].speed);
	fclose(f);
}

static Bookmark* bookmark_for(int al, int create) {
	char dir[512];
	album_dir(al, dir, sizeof(dir));
	for (int i = 0; i < book_count; i++)
		if (exactMatch(books[i].dir, dir)) return &books[i];
	if (!create) return NULL;
	if (book_count == MAX_BOOKS) {
		memmove(books, books + 1, sizeof(Bookmark) * (MAX_BOOKS - 1));
		book_count--;
	}
	Bookmark* b = &books[book_count++];
	memset(b, 0, sizeof(*b));
	snprintf(b->dir, sizeof(b->dir), "%s", dir);
	b->speed = 1.0f;
	return b;
}

static void save_bookmark(void) {
	if (queue_album < 0 || !lib.albums[queue_album].book) return;
	int t = muq_current(&queue);
	if (t < 0) return;
	Bookmark* b = bookmark_for(queue_album, 1);
	snprintf(b->file, sizeof(b->file), "%s", file_of(lib.tracks[lib.albums[queue_album].first + t].path));
	b->at = lmuse_status()->at;
	b->speed = speed;
	save_books();
	last_book_save = SDL_GetTicks();
}

///////////////////////////////////////
// Queue state across launcher runs: album folder, track, mode, position, playing, game pause.

static void save_state(void) {
	mkdir(STATE_DIR, 0755);
	FILE* f = fopen(STATE_PATH, "w");
	if (!f) return;
	if (queue_album >= 0) {
		char dir[512];
		album_dir(queue_album, dir, sizeof(dir));
		fprintf(f, "album=%s\ntrack=%d\nmode=%d\nat=%.1f\nplaying=%d\ngame_pause=%d\n", dir, muq_current(&queue),
		        music_mode, lmuse_status()->at, lmusic_playing(), game_pause);
	}
	fclose(f);
}

static void start_queue(int al, int track, float at, int play);

static void load_state(void) {
	FILE* f = fopen(STATE_PATH, "r");
	if (!f) return;
	char line[700], dir[512] = "";
	int track = 0, playing = 0;
	float at = 0;
	while (fgets(line, sizeof(line), f)) {
		normalizeNewline(line);
		trimTrailingNewlines(line);
		char* eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		const char* v = eq + 1;
		if (exactMatch(line, "album")) snprintf(dir, sizeof(dir), "%s", v);
		else if (exactMatch(line, "track")) track = atoi(v);
		else if (exactMatch(line, "mode")) music_mode = (muq_mode)(atoi(v) % MUQ_MODES);
		else if (exactMatch(line, "at")) at = atof(v);
		else if (exactMatch(line, "playing")) playing = atoi(v);
		else if (exactMatch(line, "game_pause")) game_pause = atoi(v);
	}
	fclose(f);
	if (!dir[0]) return;

	for (int al = 0; al < lib.nalbums; al++) {
		char d[512];
		album_dir(al, d, sizeof(d));
		if (!exactMatch(d, dir)) continue;
		if (track < 0 || track >= lib.albums[al].n) track = 0;
		// Restored paused. Never start music by itself; if the daemon turns out to be still on
		// this track (music went on while a Pak was open), lmusic_update adopts it.
		(void)playing;
		start_queue(al, track, at, 0);
		return;
	}
}

///////////////////////////////////////

static void update_titles(void) {
	title[0] = next_title[0] = '\0';
	if (queue_album < 0) return;
	const ml_album* a = &lib.albums[queue_album];
	int t = muq_current(&queue);
	if (t >= 0) snprintf(title, sizeof(title), "%s", lib.tracks[a->first + t].name);
	int up = muq_upcoming(&queue);
	if (up >= 0 && up != t) snprintf(next_title, sizeof(next_title), "%s", lib.tracks[a->first + up].name);
}

static void play_current(float at) {
	int t = muq_current(&queue);
	if (queue_album < 0 || t < 0) return;
	char path[600];
	full_path(lib.tracks[lib.albums[queue_album].first + t].path, path, sizeof(path));
	lmuse_play(path, at, speed);
	update_titles();
	save_when_playing = 1;
}

// play: 1 starts playback, 0 only sets the queue up to start at `at` on play, -1 adopts what
// the daemon is already playing.
static void start_queue(int al, int track, float at, int play) {
	muq_free(&queue);
	queue_album = al;
	int book = lib.albums[al].book;
	speed = 1.0f;
	if (book) {
		Bookmark* b = bookmark_for(al, 0);
		if (b) speed = b->speed > 0 ? b->speed : 1.0f;
	}
	muq_init(&queue, lib.albums[al].n, track, book ? MUQ_IN_ORDER : music_mode, SDL_GetTicks());
	failures = 0;
	resume_at = -1;
	if (play > 0) {
		play_current(at);
		save_state();
	} else {
		// Restoring from music.txt: nothing changed, and saving now would overwrite the saved
		// place with 0 before the daemon has reported where it is.
		if (play == 0) resume_at = at;
		update_titles();
	}
}

void lmusic_play(int al, int track) {
	if (al < 0 || al >= lib.nalbums) return;
	if (queue_album >= 0 && lib.albums[queue_album].book) save_bookmark();
	float at = 0;
	if (track < 0) {
		track = 0;
		Bookmark* b = lib.albums[al].book ? bookmark_for(al, 0) : NULL;
		if (b) {
			for (int i = 0; i < lib.albums[al].n; i++)
				if (exactMatch((char*)file_of(lib.tracks[lib.albums[al].first + i].path), b->file)) {
					track = i;
					at = b->at;
					break;
				}
		}
	}
	game_pause = 0;
	start_queue(al, track, at, 1);
}

void lmusic_init(void) {
	lmuse_start();
	scanned = ml_scan_card(SDCARD_PATH, &lib);
	for (int b = 0; b < 2; b++) {
		shelf[b] = calloc(lib.nalbums + 1, sizeof(int));
		shelf_n[b] = scanned ? ml_shelf_order(&lib, ML_BY_ARTIST, b, shelf[b]) : 0;
	}
	cover_state = calloc(lib.nalbums + 1, 1);
	load_books();
	if (scanned) load_state();
}

void lmusic_save(void) {
	save_bookmark();
	save_state();
}

void lmusic_quit(void) {
	save_bookmark();
	save_state();
	muq_free(&queue);
	ml_free(&lib);
	free(shelf[0]);
	free(shelf[1]);
	free(cover_state);
	lmuse_quit();
}

// A restored queue waits at resume_at; when the daemon reports that it is still on that track,
// playing or paused, the launcher follows it instead of offering to start the track over.
static int adopt_running(void) {
	if (resume_at < 0 || queue_album < 0) return 0;
	const MuseStatus* st = lmuse_status();
	if (!st->connected || st->state == MUSE_STOPPED || !st->path[0]) return 0;
	char want[600];
	full_path(lib.tracks[lib.albums[queue_album].first + muq_current(&queue)].path, want, sizeof(want));
	if (!exactMatch((char*)st->path, want)) return 0;
	resume_at = -1;
	save_when_playing = 1;
	return 1;
}

int lmusic_update(void) {
	int changed = lmuse_poll();
	if (queue_album < 0) return changed;
	changed |= adopt_running();

	if (lmuse_take_end()) {
		int t = muq_ended(&queue);
		if (t >= 0) play_current(0);
		else {
			// The album or book ran out; a finished book starts over next time.
			update_titles();
			if (lib.albums[queue_album].book) {
				Bookmark* b = bookmark_for(queue_album, 1);
				b->file[0] = '\0';
				b->at = 0;
				save_books();
			}
		}
		save_state();
		changed = 1;
	}
	if (lmuse_take_failed()) {
		int t = ++failures < MAX_FAILURES ? muq_failed(&queue) : -1;
		if (t >= 0) play_current(0);
		changed = 1;
	}
	if (save_when_playing && lmusic_playing() && lmuse_status()->at > 0) {
		save_when_playing = 0;
		save_state();
	}
	if (lmusic_playing() && SDL_GetTicks() - last_book_save > SAVE_MS) {
		save_bookmark();
		save_state();
		last_book_save = SDL_GetTicks();
	}
	return changed;
}

const ml_lib* lmusic_lib(void) {
	return &lib;
}

int lmusic_shelf_count(int books) {
	return shelf_n[books ? 1 : 0];
}

int lmusic_shelf_album(int books, int k) {
	return shelf[books ? 1 : 0][k];
}

// A folder of tracks with no artist above it is its own artist in muselib, so the artist is then
// the album's own name; "" in that case, so screens do not show the name twice.
const char* lmusic_artist_of(int al) {
	for (int i = 0; i < lib.nartists; i++)
		if (al >= lib.artists[i].first && al < lib.artists[i].first + lib.artists[i].n)
			return exactMatch(lib.artists[i].name, lib.albums[al].name) ? "" : lib.artists[i].name;
	return "";
}

const char* lmusic_cover(int al) {
	if (al < 0 || al >= lib.nalbums || cover_state[al] == 2) return NULL;
	char base[512];
	ml_cover_base(SDCARD_PATH, &lib, al, base, sizeof(base));
	if (base[0]) {
		static const char* exts[] = { ".jpg", ".png" };
		for (int i = 0; i < 2; i++) {
			snprintf(cover_path, sizeof(cover_path), "%s%s", base, exts[i]);
			if (exists(cover_path)) return cover_path;
		}
	}
	if (ml_folder_image(SDCARD_PATH, &lib, al, cover_path, sizeof(cover_path))) {
		if (cover_path[0] != '/') {
			char rel[512];
			snprintf(rel, sizeof(rel), "%s", cover_path);
			full_path(rel, cover_path, sizeof(cover_path));
		}
		if (exists(cover_path)) return cover_path;
	}
	const MuseStatus* st = lmuse_status();
	if (cover_state[al] == 1) {
		// Asked already: the daemon answered for this base with no file, so there is none.
		if (st->cover_version && exactMatch((char*)st->cover_base, base) && !st->cover_file[0]) cover_state[al] = 2;
		return NULL;
	}
	if (base[0]) {
		char dir[512], track[600];
		snprintf(dir, sizeof(dir), "%s", base);
		char* slash = strrchr(dir, '/');
		if (slash) {
			*slash = '\0';
			mkdir(dir, 0755);
		}
		full_path(lib.tracks[lib.albums[al].first].path, track, sizeof(track));
		lmuse_cover(track, base);
		cover_state[al] = 1;
	} else
		cover_state[al] = 2;
	return NULL;
}

int lmusic_active(void) {
	return queue_album >= 0 && muq_current(&queue) >= 0;
}

int lmusic_playing(void) {
	return lmusic_active() && resume_at < 0 && lmuse_status()->state == MUSE_PLAYING;
}

int lmusic_album(void) {
	return queue_album;
}

int lmusic_track(void) {
	return lmusic_active() ? muq_current(&queue) : -1;
}

int lmusic_is_book(void) {
	return queue_album >= 0 && lib.albums[queue_album].book;
}

float lmusic_at(void) {
	return resume_at >= 0 ? resume_at : lmuse_status()->at;
}

float lmusic_len(void) {
	return lmuse_status()->len;
}

const char* lmusic_title(void) {
	const MuseStatus* st = lmuse_status();
	return st->title[0] ? st->title : title;
}

const char* lmusic_next_title(void) {
	return next_title;
}

muq_mode lmusic_mode(void) {
	return music_mode;
}

float lmusic_speed(void) {
	return speed;
}

int lmusic_chapter(int* count) {
	const MuseStatus* st = lmuse_status();
	*count = st->chapters;
	int c = 0;
	for (int i = 0; i < st->chapters; i++)
		if (st->at >= st->chapter_at[i]) c = i;
	return c;
}

void lmusic_toggle(void) {
	if (!lmusic_active()) return;
	const MuseStatus* st = lmuse_status();
	if (st->state == MUSE_PLAYING) {
		lmuse_pause();
		save_bookmark();
		save_state();
	} else if (st->state == MUSE_PAUSED && resume_at < 0) lmuse_resume();
	else {
		play_current(resume_at >= 0 ? resume_at : st->at < st->len - 1 ? st->at : 0);
		resume_at = -1;
	}
	game_pause = 0;
}

void lmusic_next(void) {
	if (!lmusic_active()) return;
	int count, c = lmusic_chapter(&count);
	if (lmusic_is_book() && count > 1 && c + 1 < count) {
		lmuse_seek(lmuse_status()->chapter_at[c + 1]);
		return;
	}
	if (muq_next(&queue) >= 0) play_current(0);
	save_state();
}

void lmusic_prev(void) {
	if (!lmusic_active()) return;
	const MuseStatus* st = lmuse_status();
	int count, c = lmusic_chapter(&count);
	if (lmusic_is_book() && count > 1) {
		float start = st->chapter_at[c];
		lmuse_seek(st->at - start > 3 || c == 0 ? start : st->chapter_at[c - 1]);
		return;
	}
	// Past 3 seconds L1 restarts the track; otherwise it goes back one.
	if (st->at > 3 || muq_prev(&queue) < 0) lmuse_seek(0);
	else play_current(0);
	save_state();
}

void lmusic_seek_by(float seconds) {
	if (!lmusic_active()) return;
	const MuseStatus* st = lmuse_status();
	float at = st->at + seconds;
	if (at < 0) at = 0;
	if (st->len > 0 && at > st->len - 1) at = st->len - 1;
	lmuse_seek(at);
	if (lmusic_is_book()) {
		// Saved with the new position before the daemon confirms it.
		Bookmark* b = bookmark_for(queue_album, 1);
		snprintf(b->file, sizeof(b->file), "%s", file_of(lib.tracks[lib.albums[queue_album].first + muq_current(&queue)].path));
		b->at = at;
		b->speed = speed;
		save_books();
	}
}

void lmusic_cycle_mode(void) {
	music_mode = (muq_mode)((music_mode + 1) % MUQ_MODES);
	if (lmusic_active() && !lmusic_is_book()) {
		muq_set_mode(&queue, music_mode);
		update_titles();
	}
	save_state();
}

void lmusic_cycle_speed(void) {
	static const float steps[] = { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 0.75f };
	int n = sizeof(steps) / sizeof(steps[0]), i = 0;
	while (i < n && (steps[i] < speed - 0.01f || steps[i] > speed + 0.01f)) i++;
	speed = steps[(i + 1) % n];
	lmuse_speed(speed);
	save_bookmark();
}

void lmusic_pause_for_game(void) {
	if (!lmusic_playing()) return;
	lmuse_pause();
	save_bookmark();
	game_pause = 1;
	save_state();
}

int lmusic_take_game_pause(void) {
	int g = game_pause;
	if (g) {
		game_pause = 0;
		save_state();
	}
	return g;
}
