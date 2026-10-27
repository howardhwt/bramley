// Music for the launcher: the library on the card, the play queue, covers, audiobook bookmarks,
// and what survives a launcher restart. Playback itself is TortOS's Muse daemon (lmuse.c); the
// library scan and queue order are TortOS's muselib.c and musequeue.c, unchanged.
#ifndef LMUSIC_H
#define LMUSIC_H

#include "tortos/muselib.h"
#include "tortos/musequeue.h"

void lmusic_init(void);
void lmusic_quit(void);
// Saves the queue, position and book bookmark now, as before a power-off.
void lmusic_save(void);

// Reads the daemon, moves the queue on when a track ends, and saves state now and then.
// Returns 1 when anything shown on screen may have changed.
int lmusic_update(void);

const ml_lib* lmusic_lib(void);
// Albums of one kind (music or books) in shelf order: index k of count.
int lmusic_shelf_count(int books);
int lmusic_shelf_album(int books, int k);
const char* lmusic_artist_of(int album);

// An image for the album's cover, or NULL. With no image on the card it asks the daemon once
// for the picture embedded in the first track; a later call returns it once written.
const char* lmusic_cover(int album);

// Plays album from track (0-based within the album). For a book, track -1 resumes its bookmark.
void lmusic_play(int album, int track);

int lmusic_active(void);           // a queue exists
int lmusic_playing(void);          // playing rather than paused or stopped
int lmusic_album(void);            // album of the queue, or -1
int lmusic_track(void);            // track playing, 0-based within the album, or -1
int lmusic_is_book(void);
float lmusic_at(void);
float lmusic_len(void);
const char* lmusic_title(void);    // tag title, else the track's name
const char* lmusic_next_title(void);
muq_mode lmusic_mode(void);
float lmusic_speed(void);
int lmusic_chapter(int* count);    // current chapter (0-based) and how many, for books

void lmusic_toggle(void);          // play or pause
void lmusic_next(void);            // next track, or next chapter in a book
void lmusic_prev(void);            // restart, or previous track or chapter
void lmusic_seek_by(float seconds);
void lmusic_cycle_mode(void);      // music: in order, repeat all, repeat one, shuffle
void lmusic_cycle_speed(void);     // books: 1, 1.25, 1.5, 1.75, 2, 0.75

// Games: pause before a game starts; after the launcher comes back, lmusic_take_game_pause()
// is 1 once, so the launcher can offer to resume.
void lmusic_pause_for_game(void);
int lmusic_take_game_pause(void);

#endif
