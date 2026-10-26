#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "defines.h"
#include "api.h"
#include "utils.h"
#include "lmuse.h"

#define MUSE_SOCK "/tmp/muse.sock"

static int fd = -1;
static char inbuf[8192];
static int inlen;
static Uint32 next_connect;
static MuseStatus st;

static void send_line(const char* line);

// Value of key in a tab-separated reply line, copied into out.
static int field(const char* line, const char* key, char* out, size_t size) {
	char pattern[32];
	snprintf(pattern, sizeof(pattern), "\t%s=", key);
	const char* p = strstr(line, pattern);
	if (!p) return 0;
	p += strlen(pattern);
	size_t n = strcspn(p, "\t");
	if (n >= size) n = size - 1;
	memcpy(out, p, n);
	out[n] = '\0';
	return 1;
}

static void handle(const char* line) {
	char v[512];
	if (!strncmp(line, "STATE", 5)) {
		if (field(line, "state", v, sizeof(v)))
			st.state = exactMatch(v, "playing") ? MUSE_PLAYING : exactMatch(v, "paused") ? MUSE_PAUSED : MUSE_STOPPED;
		if (field(line, "path", v, sizeof(v))) snprintf(st.path, sizeof(st.path), "%s", v);
	} else if (!strncmp(line, "POS", 3)) {
		if (field(line, "at", v, sizeof(v))) st.at = atof(v);
		if (field(line, "len", v, sizeof(v))) st.len = atof(v);
	} else if (!strncmp(line, "META", 4)) {
		field(line, "title", st.title, sizeof(st.title));
		field(line, "artist", st.artist, sizeof(st.artist));
		field(line, "album", st.album, sizeof(st.album));
		if (field(line, "len", v, sizeof(v))) st.len = atof(v);
		if (field(line, "chapters", v, sizeof(v))) st.chapters = atoi(v) > 64 ? 64 : atoi(v);
	} else if (!strncmp(line, "CHAPTER", 7)) {
		char at[32];
		if (field(line, "i", v, sizeof(v)) && field(line, "at", at, sizeof(at))) {
			int i = atoi(v);
			if (i >= 0 && i < 64) st.chapter_at[i] = atof(at);
		}
	} else if (!strncmp(line, "END", 3)) {
		st.ended = 1;
	} else if (!strncmp(line, "COVER", 5)) {
		field(line, "base", st.cover_base, sizeof(st.cover_base));
		if (!field(line, "file", st.cover_file, sizeof(st.cover_file))) st.cover_file[0] = '\0';
		st.cover_version++;
	} else if (!strncmp(line, "ERROR", 5)) {
		st.failed = 1;
		field(line, "why", st.error, sizeof(st.error));
		LOG_error("muse: %s\n", st.error);
	}
}

static void disconnect(void) {
	if (fd >= 0) close(fd);
	fd = -1;
	inlen = 0;
	st.connected = 0;
}

static int try_connect(void) {
	struct sockaddr_un sa = { .sun_family = AF_UNIX };
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", MUSE_SOCK);
	int s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) return 0;
	if (connect(s, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
		close(s);
		return 0;
	}
	fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
	fd = s;
	st.connected = 1;
	return 1;
}

// On the device the daemon ships next to the launcher; it needs the stock FFmpeg in /usr/lib,
// which spikes/muse showed works. On a desktop, run tools/fake-muse.py instead.
static void spawn_daemon(void) {
	if (exactMatch(PLATFORM, "desktop")) return;
	char muse[256];
	snprintf(muse, sizeof(muse), "%s/bin/muse", SYSTEM_PATH);
	if (!exists(muse)) {
		LOG_error("muse: %s is missing\n", muse);
		return;
	}
	// Detached the way TortOS does it (platform.c plat_spawn_detached): fork twice and start a
	// new session, so the daemon outlives the launcher and holds none of its files. The Brick's
	// BusyBox has no nohup, so this does not go through a shell.
	pid_t pid = fork();
	if (pid == 0) {
		setsid();
		if (fork() == 0) {
			for (int f = 3; f < 256; f++) close(f);
			int null = open("/dev/null", O_RDWR);
			if (null >= 0) {
				dup2(null, 0);
				dup2(null, 1);
				dup2(null, 2);
			}
			char libs[1024];
			const char* old = getenv("LD_LIBRARY_PATH");
			snprintf(libs, sizeof(libs), "/usr/lib%s%s", old && *old ? ":" : "", old ? old : "");
			setenv("LD_LIBRARY_PATH", libs, 1);
			execl(muse, muse, (char*)NULL);
			_exit(127);
		}
		_exit(0);
	}
	if (pid > 0) waitpid(pid, NULL, 0);
}

// Connects if the daemon is already running, and never waits: the launcher starts after every
// game and Pak, so music must not cost it time. The daemon is started only when something is
// sent to it (ensure_daemon), which is when the player plays music or a cover is needed.
void lmuse_start(void) {
	if (try_connect()) send_line("STATUS\n");
	next_connect = SDL_GetTicks() + 1000;
}

static int ensure_daemon(void) {
	if (fd >= 0 || try_connect()) return 1;
	spawn_daemon();
	// The socket appears within about 100 ms (spikes/muse).
	for (int i = 0; i < 10; i++) {
		usleep(50000);
		if (try_connect()) return 1;
	}
	return 0;
}

void lmuse_quit(void) {
	// Closing the socket leaves the daemon running, so music continues while a Pak or the next
	// launcher instance is up.
	disconnect();
}

int lmuse_poll(void) {
	if (fd < 0) {
		if (SDL_GetTicks() < next_connect) return 0;
		next_connect = SDL_GetTicks() + 1000;
		if (!try_connect()) return 0;
		send_line("STATUS\n"); // catch up with whatever the daemon is doing
	}
	int changed = 0;
	if (fd < 0) return 0;
	for (;;) {
		ssize_t n = read(fd, inbuf + inlen, sizeof(inbuf) - 1 - inlen);
		if (n > 0) {
			inlen += n;
			inbuf[inlen] = '\0';
			char* start = inbuf;
			char* nl;
			while ((nl = strchr(start, '\n'))) {
				*nl = '\0';
				if (*start) {
					handle(start);
					changed = 1;
				}
				start = nl + 1;
			}
			inlen = (int)strlen(start);
			memmove(inbuf, start, inlen + 1);
			if (inlen == (int)sizeof(inbuf) - 1) inlen = 0; // a line this long is not Muse's
			continue;
		}
		if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
			disconnect();
			changed = 1;
		}
		break;
	}
	return changed;
}

const MuseStatus* lmuse_status(void) {
	return &st;
}

int lmuse_take_failed(void) {
	int f = st.failed;
	st.failed = 0;
	return f;
}

void lmuse_cover(const char* path, const char* base) {
	char line[1200];
	snprintf(line, sizeof(line), "COVER\tpath=%s\tbase=%s\n", path, base);
	send_line(line);
}

int lmuse_take_end(void) {
	int e = st.ended;
	st.ended = 0;
	return e;
}

static void send_line(const char* line) {
	if (!ensure_daemon()) return;
	size_t len = strlen(line);
	if (write(fd, line, len) != (ssize_t)len) disconnect();
}

void lmuse_play(const char* path, float at, float speed) {
	char line[700];
	snprintf(line, sizeof(line), "PLAY\tpath=%s\tat=%.1f\tspeed=%.2f\n", path, at, speed);
	st.title[0] = st.artist[0] = st.album[0] = '\0';
	st.chapters = 0;
	st.at = at;
	send_line(line);
}

void lmuse_pause(void) {
	send_line("PAUSE\n");
}

void lmuse_resume(void) {
	send_line("RESUME\n");
}

void lmuse_stop(void) {
	send_line("STOP\n");
}

void lmuse_seek(float at) {
	char line[64];
	snprintf(line, sizeof(line), "SEEK\tat=%.1f\n", at < 0 ? 0 : at);
	send_line(line);
}

void lmuse_speed(float x) {
	char line[64];
	snprintf(line, sizeof(line), "SPEED\tx=%.2f\n", x);
	send_line(line);
}
