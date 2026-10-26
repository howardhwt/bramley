// Launcher data: what the home rows show and how items are launched. Reads and writes the same
// files as NextUI's stock launcher (recent.txt, /tmp/next, /tmp/resume_slot.txt, auto_resume.txt).
#ifndef LDATA_H
#define LDATA_H

#include "lgfx.h"

typedef enum { CARD_RECENT, CARD_CONSOLE, CARD_TOOL, CARD_GAME, CARD_MUSIC, CARD_ALBUM } CardKind;

typedef struct {
	CardKind kind;
	char name[256];      // display name
	char path[512];      // full SD path of the ROM, ROM folder or Pak
	char tag[64];        // console short name, eg. GBA
	char art_path[512];  // image to show, "" if none
	int art_is_pixel;    // save-state screenshot rather than box art
	int can_resume;      // a manual save slot exists
	int game_count;      // consoles only
	int index;           // albums: album index in the music library
	float hue;           // colour of the title card
	LTex* art;
	LTex* label;
	int art_tried;
	int resume_checked;  // games: can_resume is looked up only when the game is focused
} Card;

typedef struct {
	char title[64];
	Card* cards;
	int count;
} Row;

// Loads Continue Playing, Consoles and Apps. Empty rows are left out.
int ldata_load_rows(Row* rows, int max_rows);
void ldata_free_rows(Row* rows, int count);

// Lists the games in a console folder, sorted by name, with box art paths. The caller frees
// the array with ldata_free_games(). Folders count as a game when they hold a .m3u or .cue.
Card* ldata_load_games(const Card* console, int* count);
void ldata_free_games(Card* games, int count);
// Looks up whether a game has a manual save, once.
void ldata_check_resume(Card* game);

// Each writes /tmp/next for MinUI.pak/launch.sh to run after the launcher exits.
// resume: 1 resumes the last manual save, 0 starts from the default state, as stock does.
void ldata_launch(Card* card, int resume);
int ldata_auto_resume(void);
int ldata_run_fn_action(int index);
// Launches the Tools Pak with this folder name (eg. "Settings"). Returns 1 if it exists.
int ldata_launch_tool(const char* name);

#endif
