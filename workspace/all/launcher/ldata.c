#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "config.h"
#include "ldata.h"

#define MAX_RECENTS 24
#define MAX_CARDS 128

static void card_name_from_path(const char* path, char* out) {
	char file[256];
	const char* slash = strrchr(path, '/');
	snprintf(file, sizeof(file), "%s", slash ? slash + 1 : path);
	getDisplayName(file, out);
}

static float hue_for(const char* s) {
	unsigned int h = 2166136261u;
	for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
	return (h % 360) / 360.0f;
}

static int has_emu(const char* emu_name) {
	char pak_path[256];
	getEmuPath((char*)emu_name, pak_path);
	return exists(pak_path);
}

// The .m3u for a disc of a multi-disc game, if the game folder has one.
static int find_m3u(const char* rom_path, char* m3u_path) {
	char dir[256];
	snprintf(dir, sizeof(dir), "%s", rom_path);
	char* slash = strrchr(dir, '/');
	if (!slash) return 0;
	*slash = '\0';
	const char* folder = strrchr(dir, '/');
	if (!folder) return 0;
	snprintf(m3u_path, 256, "%s%s.m3u", dir, folder);
	return exists(m3u_path);
}

static void first_disc(const char* m3u_path, char* disc_path) {
	FILE* f = fopen(m3u_path, "r");
	disc_path[0] = '\0';
	if (!f) return;
	char line[256];
	while (fgets(line, sizeof(line), f)) {
		normalizeNewline(line);
		trimTrailingNewlines(line);
		if (!line[0]) continue;
		snprintf(disc_path, 256, "%s", m3u_path);
		char* slash = strrchr(disc_path, '/');
		if (slash) strcpy(slash + 1, line);
		break;
	}
	fclose(f);
}

// Last manual save slot and its screenshot, the same lookup as stock readyResumePath().
static void find_resume(Card* card) {
	char path[256], rom_file[256], emu_name[256], slot_path[512];
	snprintf(path, sizeof(path), "%s", card->path);
	char m3u[256];
	if (!suffixMatch(".m3u", path) && find_m3u(path, m3u)) snprintf(path, sizeof(path), "%s", m3u);

	getEmuName(path, emu_name);
	snprintf(rom_file, sizeof(rom_file), "%s", strrchr(path, '/') + 1);
	snprintf(slot_path, sizeof(slot_path), "%s/.minui/%s/%s.txt", SHARED_USERDATA_PATH, emu_name, rom_file);
	card->can_resume = exists(slot_path);
	if (!card->can_resume) return;

	char slot[16];
	getFile(slot_path, slot, sizeof(slot));
	char preview[512];
	snprintf(preview, sizeof(preview), "%s/.minui/%s/%s.%d.bmp", SHARED_USERDATA_PATH, emu_name, rom_file, atoi(slot));
	if (exists(preview)) {
		snprintf(card->art_path, sizeof(card->art_path), "%s", preview);
		card->art_is_pixel = 1;
	}
}

// Box art: <rom folder>/.media/<rom name without extension>.png
static void find_box_art(Card* card) {
	char dir[512], base[256];
	snprintf(dir, sizeof(dir), "%s", card->path);
	char* slash = strrchr(dir, '/');
	if (!slash) return;
	snprintf(base, sizeof(base), "%s", slash + 1);
	*slash = '\0';
	char* dot = strrchr(base, '.');
	if (dot) *dot = '\0';
	char art[512];
	snprintf(art, sizeof(art), "%s/.media/%s.png", dir, base);
	if (exists(art)) snprintf(card->art_path, sizeof(card->art_path), "%s", art);
}

static int load_recents(Card* cards) {
	FILE* f = fopen(RECENT_PATH, "r");
	if (!f) return 0;
	int count = 0;
	char line[512];
	while (fgets(line, sizeof(line), f) && count < MAX_RECENTS) {
		normalizeNewline(line);
		trimTrailingNewlines(line);
		if (!line[0]) continue;

		char* alias = NULL;
		char* tab = strchr(line, '\t');
		if (tab) {
			*tab = '\0';
			alias = tab + 1;
		}

		Card* card = &cards[count];
		memset(card, 0, sizeof(*card));
		card->kind = CARD_RECENT;
		snprintf(card->path, sizeof(card->path), "%s%s", SDCARD_PATH, line);
		if (!exists(card->path)) continue;

		char emu_name[256];
		getEmuName(card->path, emu_name);
		if (!has_emu(emu_name)) continue;

		card_name_from_path(card->path, card->name);
		if (alias && alias[0]) snprintf(card->name, sizeof(card->name), "%s", alias);
		snprintf(card->tag, sizeof(card->tag), "%s", emu_name);
		card->hue = hue_for(card->name);

		find_resume(card);
		if (!card->art_path[0]) find_box_art(card);
		count++;
	}
	fclose(f);
	return count;
}

// Number of games in a ROM folder, or 0 if its emulator Pak is missing.
static int rom_folder_game_count(const char* folder) {
	char emu_name[256];
	getEmuName(folder, emu_name);
	if (!has_emu(emu_name)) return 0;

	char path[512];
	snprintf(path, sizeof(path), "%s/%s", ROMS_PATH, folder);
	DIR* dh = opendir(path);
	if (!dh) return 0;
	int count = 0;
	struct dirent* dp;
	while ((dp = readdir(dh)))
		if (!hide(dp->d_name)) count++;
	closedir(dh);
	return count;
}

static void apply_map(const char* map_path, Card* cards, int count) {
	FILE* f = fopen(map_path, "r");
	if (!f) return;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		normalizeNewline(line);
		trimTrailingNewlines(line);
		char* tab = strchr(line, '\t');
		if (!tab) continue;
		*tab = '\0';
		for (int i = 0; i < count; i++) {
			const char* folder = strrchr(cards[i].path, '/') + 1;
			if (exactMatch(folder, line)) snprintf(cards[i].name, sizeof(cards[i].name), "%s", tab + 1);
		}
	}
	fclose(f);
}

static int compare_cards(const void* a, const void* b) {
	return strcasecmp(((const Card*)a)->name, ((const Card*)b)->name);
}

static int load_consoles(Card* cards) {
	DIR* dh = opendir(ROMS_PATH);
	if (!dh) return 0;
	int count = 0;
	struct dirent* dp;
	while ((dp = readdir(dh)) && count < MAX_CARDS) {
		if (hide(dp->d_name)) continue;
		int games = rom_folder_game_count(dp->d_name);
		if (!games) continue;

		Card* card = &cards[count];
		memset(card, 0, sizeof(*card));
		card->kind = CARD_CONSOLE;
		card->game_count = games;
		snprintf(card->path, sizeof(card->path), "%s/%s", ROMS_PATH, dp->d_name);
		getDisplayName(dp->d_name, card->name);
		getEmuName(dp->d_name, card->tag);
		card->hue = hue_for(card->tag);

		char art[512];
		snprintf(art, sizeof(art), "%s/.media/consoles/%s.png", SDCARD_PATH, card->tag);
		if (exists(art)) snprintf(card->art_path, sizeof(card->art_path), "%s", art);
		count++;
	}
	closedir(dh);

	char map_path[256];
	snprintf(map_path, sizeof(map_path), "%s/map.txt", ROMS_PATH);
	apply_map(map_path, cards, count);
	qsort(cards, count, sizeof(Card), compare_cards);

	// Folders such as "Game Boy (GB)" and "Game Boy (GB-alt)" can share a display name; keep the first.
	int unique = 0;
	for (int i = 0; i < count; i++) {
		if (unique && exactMatch(cards[unique - 1].name, cards[i].name)) continue;
		cards[unique++] = cards[i];
	}
	return unique;
}

static int load_tools(Card* cards) {
	DIR* dh = opendir(TOOLS_PATH);
	if (!dh) return 0;
	int count = 0;
	struct dirent* dp;
	while ((dp = readdir(dh)) && count < MAX_CARDS) {
		if (hide(dp->d_name) || !suffixMatch(".pak", dp->d_name)) continue;
		Card* card = &cards[count];
		memset(card, 0, sizeof(*card));
		card->kind = CARD_TOOL;
		snprintf(card->path, sizeof(card->path), "%s/%s", TOOLS_PATH, dp->d_name);
		snprintf(card->name, sizeof(card->name), "%s", dp->d_name);
		card->name[strlen(card->name) - 4] = '\0';
		card->hue = hue_for(card->name);
		count++;
	}
	closedir(dh);
	qsort(cards, count, sizeof(Card), compare_cards);
	return count;
}

// The file a game folder launches: <folder>/<folder>.m3u, else the first .cue inside.
static int folder_game_file(const char* dir, char* out, size_t size) {
	const char* name = strrchr(dir, '/');
	snprintf(out, size, "%s%s.m3u", dir, name ? name : "/");
	if (exists(out)) return 1;
	DIR* dh = opendir(dir);
	if (!dh) return 0;
	struct dirent* dp;
	int found = 0;
	while ((dp = readdir(dh))) {
		if (!hide(dp->d_name) && suffixMatch(".cue", dp->d_name)) {
			snprintf(out, size, "%s/%s", dir, dp->d_name);
			found = 1;
			break;
		}
	}
	closedir(dh);
	return found;
}

Card* ldata_load_games(const Card* console, int* count) {
	*count = 0;
	DIR* dh = opendir(console->path);
	if (!dh) return NULL;
	int cap = 64;
	Card* games = calloc(cap, sizeof(Card));
	struct dirent* dp;
	while ((dp = readdir(dh))) {
		if (hide(dp->d_name)) continue;
		char path[512];
		snprintf(path, sizeof(path), "%s/%s", console->path, dp->d_name);

		char launch[512];
		snprintf(launch, sizeof(launch), "%s", path);
		// d_type is not reliable on every SD card file system, so ask stat().
		struct stat st;
		int is_dir = stat(path, &st) == 0 && S_ISDIR(st.st_mode);
		if (is_dir && !folder_game_file(path, launch, sizeof(launch))) continue;

		if (*count == cap) {
			cap *= 2;
			games = realloc(games, cap * sizeof(Card));
		}
		Card* game = &games[(*count)++];
		memset(game, 0, sizeof(*game));
		game->kind = CARD_GAME;
		snprintf(game->path, sizeof(game->path), "%s", launch);
		getDisplayName(dp->d_name, game->name);
		snprintf(game->tag, sizeof(game->tag), "%s", console->tag);
		game->hue = hue_for(game->name);

		// Box art is named after the file or folder in the console folder.
		char base[256];
		snprintf(base, sizeof(base), "%s", dp->d_name);
		char* dot = is_dir ? NULL : strrchr(base, '.');
		if (dot) *dot = '\0';
		char art[512];
		snprintf(art, sizeof(art), "%s/.media/%s.png", console->path, base);
		if (exists(art)) snprintf(game->art_path, sizeof(game->art_path), "%s", art);
	}
	closedir(dh);

	char map_path[600];
	snprintf(map_path, sizeof(map_path), "%s/map.txt", console->path);
	FILE* f = fopen(map_path, "r");
	if (f) {
		char line[512];
		while (fgets(line, sizeof(line), f)) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			char* tab = strchr(line, '\t');
			if (!tab) continue;
			*tab = '\0';
			for (int i = 0; i < *count; i++) {
				const char* file = strrchr(games[i].path, '/') + 1;
				if (exactMatch((char*)file, line)) snprintf(games[i].name, sizeof(games[i].name), "%s", tab + 1);
			}
		}
		fclose(f);
	}
	qsort(games, *count, sizeof(Card), compare_cards);
	return games;
}

void ldata_free_games(Card* games, int count) {
	for (int i = 0; i < count; i++) {
		ltex_free(games[i].art);
		ltex_free(games[i].label);
	}
	free(games);
}

void ldata_check_resume(Card* game) {
	if (game->resume_checked) return;
	game->resume_checked = 1;
	char art[512];
	snprintf(art, sizeof(art), "%s", game->art_path);
	int is_pixel = game->art_is_pixel;
	find_resume(game);
	// Keep box art in the library; find_resume() would swap in the save screenshot.
	if (art[0]) {
		snprintf(game->art_path, sizeof(game->art_path), "%s", art);
		game->art_is_pixel = is_pixel;
	}
}

static void add_row(Row* rows, int* count, int max_rows, const char* title, Card* cards, int n) {
	if (n <= 0 || *count >= max_rows) {
		free(cards);
		return;
	}
	Row* row = &rows[(*count)++];
	snprintf(row->title, sizeof(row->title), "%s", title);
	row->cards = cards;
	row->count = n;
}

int ldata_load_rows(Row* rows, int max_rows) {
	int count = 0;
	Card* recents = calloc(MAX_RECENTS, sizeof(Card));
	add_row(rows, &count, max_rows, "Continue Playing", recents, load_recents(recents));
	Card* consoles = calloc(MAX_CARDS, sizeof(Card));
	add_row(rows, &count, max_rows, "Consoles", consoles, load_consoles(consoles));
	Card* tools = calloc(MAX_CARDS, sizeof(Card));
	add_row(rows, &count, max_rows, "Apps", tools, load_tools(tools));
	return count;
}

void ldata_free_rows(Row* rows, int count) {
	for (int r = 0; r < count; r++) {
		for (int c = 0; c < rows[r].count; c++) {
			ltex_free(rows[r].cards[c].art);
			ltex_free(rows[r].cards[c].label);
		}
		free(rows[r].cards);
	}
}

///////////////////////////////////////

// Escapes single quotes for a shell command, as stock escapeSingleQuotes() does.
static void shell_quote(const char* in, char* out, size_t size) {
	size_t o = 0;
	for (; *in && o + 5 < size; in++) {
		if (*in == '\'') {
			memcpy(out + o, "'\\''", 4);
			o += 4;
		} else
			out[o++] = *in;
	}
	out[o] = '\0';
}

static void queue_next(const char* cmd) {
	LOG_info("launcher cmd: %s\n", cmd);
	putFile("/tmp/next", (char*)cmd);
}

// Moves path (SD-relative) to the top of recent.txt, as stock addRecent() does.
static void add_recent(const char* sd_path) {
	const char* rel = sd_path + strlen(SDCARD_PATH);
	char lines[MAX_RECENTS][512];
	int count = 0;

	FILE* f = fopen(RECENT_PATH, "r");
	if (f) {
		char line[512];
		while (fgets(line, sizeof(line), f) && count < MAX_RECENTS) {
			normalizeNewline(line);
			trimTrailingNewlines(line);
			if (!line[0]) continue;
			char path_only[512];
			snprintf(path_only, sizeof(path_only), "%s", line);
			char* tab = strchr(path_only, '\t');
			if (tab) *tab = '\0';
			if (exactMatch(path_only, rel)) continue;
			snprintf(lines[count++], sizeof(lines[0]), "%s", line);
		}
		fclose(f);
	}

	f = fopen(RECENT_PATH, "w");
	if (!f) return;
	fprintf(f, "%s\n", rel);
	for (int i = 0; i < count && i < MAX_RECENTS - 1; i++) fprintf(f, "%s\n", lines[i]);
	fclose(f);
}

static void launch_rom(const char* rom_path, int resume) {
	char sd_path[256], m3u_path[256], recent_path[256], emu_name[256], emu_path[256];
	snprintf(sd_path, sizeof(sd_path), "%s", rom_path);

	int has_m3u = find_m3u(sd_path, m3u_path);
	snprintf(recent_path, sizeof(recent_path), "%s", has_m3u ? m3u_path : sd_path);
	if (has_m3u && suffixMatch(".m3u", sd_path)) first_disc(m3u_path, sd_path);

	getEmuName(sd_path, emu_name);

	if (resume) {
		char rom_file[256], slot_path[512], slot[16];
		snprintf(rom_file, sizeof(rom_file), "%s", strrchr(recent_path, '/') + 1);
		snprintf(slot_path, sizeof(slot_path), "%s/.minui/%s/%s.txt", SHARED_USERDATA_PATH, emu_name, rom_file);
		getFile(slot_path, slot, sizeof(slot));
		putFile(RESUME_SLOT_PATH, slot);

		if (has_m3u) {
			// A multi-disc save remembers which disc it was made on.
			char disc_path_path[512], disc_path[256];
			snprintf(disc_path_path, sizeof(disc_path_path), "%s/.minui/%s/%s.%s.txt", SHARED_USERDATA_PATH, emu_name,
			         rom_file, slot);
			if (exists(disc_path_path)) {
				getFile(disc_path_path, disc_path, sizeof(disc_path));
				if (disc_path[0] == '/')
					snprintf(sd_path, sizeof(sd_path), "%s", disc_path);
				else {
					snprintf(sd_path, sizeof(sd_path), "%s", m3u_path);
					char* slash = strrchr(sd_path, '/');
					if (slash) snprintf(slash + 1, sizeof(sd_path) - (slash + 1 - sd_path), "%s", disc_path);
				}
			}
		}
	} else
		putInt(RESUME_SLOT_PATH, 8); // the hidden default state, as stock does

	getEmuPath(emu_name, emu_path);
	add_recent(recent_path);

	char q_rom[512], q_emu[512], cmd[1200];
	shell_quote(sd_path, q_rom, sizeof(q_rom));
	shell_quote(emu_path, q_emu, sizeof(q_emu));
	snprintf(cmd, sizeof(cmd), "gametimectl.elf start '%s'", q_rom);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "'%s' '%s'", q_emu, q_rom);
	queue_next(cmd);
}

static void launch_pak(const char* pak_path) {
	char q[512], cmd[600];
	shell_quote(pak_path, q, sizeof(q));
	snprintf(cmd, sizeof(cmd), "'%s/launch.sh'", q);
	queue_next(cmd);
}

void ldata_launch(Card* card, int resume) {
	if (card->kind == CARD_TOOL) launch_pak(card->path);
	else if (card->kind == CARD_RECENT || card->kind == CARD_GAME) launch_rom(card->path, resume && card->can_resume);
}

// Relaunches the game minarch was in when the device slept or powered off.
int ldata_auto_resume(void) {
	if (!exists(AUTO_RESUME_PATH)) return 0;

	char path[256];
	getFile(AUTO_RESUME_PATH, path, sizeof(path));
	unlink(AUTO_RESUME_PATH);
	sync();

	char sd_path[512];
	snprintf(sd_path, sizeof(sd_path), "%s%s", SDCARD_PATH, path);
	if (!exists(sd_path)) return 0;

	char emu_name[256], emu_path[256];
	getEmuName(sd_path, emu_name);
	getEmuPath(emu_name, emu_path);
	if (!exists(emu_path)) return 0;

	char q_rom[512], q_emu[512], cmd[1200];
	shell_quote(sd_path, q_rom, sizeof(q_rom));
	shell_quote(emu_path, q_emu, sizeof(q_emu));
	snprintf(cmd, sizeof(cmd), "gametimectl.elf start '%s'", q_rom);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "'%s' '%s'", q_emu, q_rom);
	putInt(RESUME_SLOT_PATH, AUTO_RESUME_SLOT);
	queue_next(cmd);
	return 1;
}

// Runs the Tools Pak bound to FN1, FN2 or FN3 in Settings. Returns 1 if one was launched.
int ldata_run_fn_action(int index) {
	const char* action = CFG_getFnAction(index);
	if (!action || !action[0]) return 0;
	size_t prefix_len = strlen(FN_ACTION_PAK_PREFIX);
	if (strncmp(action, FN_ACTION_PAK_PREFIX, prefix_len) != 0 || !action[prefix_len]) return 0;

	char pak_path[256], launch_path[300];
	snprintf(pak_path, sizeof(pak_path), "%s/%s", TOOLS_PATH, action + prefix_len);
	snprintf(launch_path, sizeof(launch_path), "%s/launch.sh", pak_path);
	if (!exists(launch_path)) return 0;
	launch_pak(pak_path);
	return 1;
}

int ldata_launch_tool(const char* name) {
	char pak_path[256], launch_path[300];
	snprintf(pak_path, sizeof(pak_path), "%s/%s.pak", TOOLS_PATH, name);
	snprintf(launch_path, sizeof(launch_path), "%s/launch.sh", pak_path);
	if (!exists(launch_path)) return 0;
	launch_pak(pak_path);
	return 1;
}
