// Software Update: finds, downloads and installs new Bramley releases from GitHub
// (docs/plans/v1-standalone.md, step 4). Shared by the launcher, which checks quietly once a day
// on Home while Wi-Fi is already on, and Settings, where the user downloads and installs.
//
// Each release carries bramley-update.txt (made by card.sh): the version, the card zip's name,
// size and SHA-256, and short notes. The check reads it from the latest release. The download is
// verified against the SHA-256, and installing unpacks only the files Bramley owns (never Roms,
// Saves, Bios or .userdata), then the device restarts and the install screen finishes the job.
// Connections check GitHub's certificate against the bundle Bramley ships (res/cacert.pem).
#ifndef BRAMLEY_UPDATE_H
#define BRAMLEY_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	UPDATE_UNKNOWN,     // not checked yet
	UPDATE_CHECKING,
	UPDATE_CURRENT,     // the newest release is installed
	UPDATE_AVAILABLE,   // a newer release, not downloaded
	UPDATE_DOWNLOADING,
	UPDATE_READY,       // downloaded and verified
	UPDATE_INSTALLING,
	UPDATE_FAILED,      // the last step failed; error says why
} UpdateState;

typedef struct {
	UpdateState state;
	char current[32];    // the installed version
	char version[32];    // the newest version found
	char file[128];      // its card zip
	long long size;      // the zip's size in bytes
	long long done;      // bytes downloaded so far
	char notes[1024];    // what is new, plain text
	char error[160];
	long long checked;   // when the last check succeeded (Unix time), 0 if never
	int newer;           // version is newer than the installed one
} UpdateInfo;

// True when Bramley runs as a whole card, the only kind the updater can update.
int update_supported(void);
// Reads the saved state. Safe to call more than once.
void update_init(void);
void update_get(UpdateInfo* info);
// Each runs on its own thread and returns at once; update_get shows the progress.
void update_check(void);
void update_download(void);
// Cancels a download and deletes what was downloaded. (A download that breaks off by itself keeps
// its part, and the next download resumes it.)
void update_stop(void);
// Unpacks the downloaded release onto the card (a few seconds). Returns 0 when the device
// should restart to finish, or -1 with the error in update_get.
int update_install(void);
// Whether the launcher checks by itself once a day (on by default).
int update_auto(void);
void update_set_auto(int on);
// For the launcher's Home: starts the daily check when it is due and Wi-Fi is on.
void update_tick(void);
// True while an update waits for the user (available or downloaded): the dot on Settings.
int update_badge(void);

#ifdef __cplusplus
}
#endif

#endif
