// Client for TortOS's Muse playback daemon (spikes/muse, THIRD-PARTY-LICENSES.md). Muse runs as
// its own process on /tmp/muse.sock so music keeps playing while the launcher restarts; this
// side only sends commands and reads its one-line replies, without ever blocking a frame.
#ifndef LMUSE_H
#define LMUSE_H

typedef enum { MUSE_STOPPED, MUSE_PLAYING, MUSE_PAUSED } MuseState;

typedef struct {
	int connected;
	MuseState state;
	char path[512];
	float at, len;
	char title[256], artist[256], album[256];
	int ended;       // set when a track ran out by itself; cleared by lmuse_take_end()
	int failed;      // set on an ERROR reply; cleared by lmuse_take_failed()
	char error[128]; // last ERROR reply
	int chapters;
	float chapter_at[64];
	char cover_base[512], cover_file[512]; // last COVER reply
	int cover_version;
} MuseStatus;

// Connects to the daemon, starting it first on the device if it is not running.
void lmuse_start(void);
void lmuse_quit(void);

// Reads whatever the daemon has sent. Returns 1 if the status changed.
int lmuse_poll(void);
const MuseStatus* lmuse_status(void);
int lmuse_take_end(void);
int lmuse_take_failed(void);

// Asks the daemon to write the picture embedded in path to base.jpg or base.png. The reply
// arrives later in cover_base and cover_file, with cover_file "" when there is no picture.
void lmuse_cover(const char* path, const char* base);

void lmuse_play(const char* path, float at, float speed);
void lmuse_pause(void);
void lmuse_resume(void);
void lmuse_stop(void);
void lmuse_seek(float at);
void lmuse_speed(float x);

#endif
