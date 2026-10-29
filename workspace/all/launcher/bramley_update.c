// Software Update (see bramley_update.h).
#include <errno.h>
#include <openssl/evp.h>
#include <pthread.h>
#include <spawn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "defines.h"
#include "api.h"
#include "bramley.h"
#include "bramley_update.h"

#define MANIFEST_URL "https://github.com/howardhwt/bramley/releases/latest/download/bramley-update.txt"
#define RELEASE_URL "https://github.com/howardhwt/bramley/releases/download/v%s/%s"
#define CA_BUNDLE BRAMLEY_RES_DIR "/cacert.pem"
#define DIR BRAMLEY_STATE_DIR "/update"
#define STATE_FILE DIR "/state.txt"
#define NOTES_FILE DIR "/notes.txt"
#define MANIFEST_TMP DIR "/manifest.txt"
#define PART DIR "/download.part"
#define ZIP DIR "/download.zip"
#define TEST_SERVER DIR "/test-server.txt"
#define DAY (24 * 3600)

extern char** environ;

#ifdef __APPLE__
#define UNZIP "unzip" // the Mac's own, for the desktop build
#else
#define UNZIP SDCARD_PATH "/.tmp_update/" PLATFORM "/unzip"
#endif

// What installing may write: the system zip the install screen unpacks, the emulator and app
// Paks, and the notices at the top of the card. Nothing else in the zip is touched.
static const char* const allowed[] = {
	"MinUI.zip", "Emus/" PLATFORM "/*", "Tools/" PLATFORM "/*", "Licenses/*",
	"CORES-SOURCE.md", "README.txt", "LICENSE", "THIRD-PARTY-LICENSES.md", "Inter-LICENSE.txt",
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static UpdateInfo info;
static char sha256[65];
static int auto_check = 1;
static int loaded, busy, fake;
static pid_t curl_pid;
static int stopping;
static time_t last_try;

static void set_error(const char* msg) {
	snprintf(info.error, sizeof(info.error), "%s", msg);
	info.state = UPDATE_FAILED;
}

// 1 if version a is newer than b ("0.10.0" is newer than "0.9.2"). Only the numbers count.
static int newer(const char* a, const char* b) {
	for (int part = 0; part < 4; part++) {
		long x = strtol(a, NULL, 10), y = strtol(b, NULL, 10);
		if (x != y) return x > y;
		a = strchr(a, '.');
		b = strchr(b, '.');
		if (!a && !b) return 0;
		a = a ? a + 1 : "0";
		b = b ? b + 1 : "0";
	}
	return 0;
}

static void save_state(void) {
	mkdir(BRAMLEY_STATE_DIR, 0755);
	mkdir(DIR, 0755);
	FILE* f = fopen(STATE_FILE, "w");
	if (!f) return;
	fprintf(f, "checked %lld\nauto %d\nversion %s\nfile %s\nsize %lld\nsha256 %s\n", info.checked, auto_check,
	        info.version, info.file, info.size, sha256);
	fclose(f);
	f = fopen(NOTES_FILE, "w");
	if (f) {
		fputs(info.notes, f);
		fclose(f);
	}
}

static void read_file(const char* path, char* out, size_t size) {
	out[0] = 0;
	FILE* f = fopen(path, "r");
	if (!f) return;
	size_t n = fread(out, 1, size - 1, f);
	out[n] = 0;
	fclose(f);
}

static void trim(char* s) {
	size_t n = strlen(s);
	while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = 0;
}

// The known state after a check: newer, current, or downloaded already.
static void settle(void) {
	struct stat st;
	if (info.version[0] && newer(info.version, info.current)) {
		info.state = stat(ZIP, &st) == 0 ? UPDATE_READY : UPDATE_AVAILABLE;
		info.done = stat(PART, &st) == 0 ? st.st_size : 0;
	} else {
		info.state = info.checked ? UPDATE_CURRENT : UPDATE_UNKNOWN;
		unlink(ZIP);
		unlink(PART);
	}
}

// Development aid: BRAMLEY_FAKE_UPDATE=checking|current|available|downloading|ready|failed shows
// that state with made-up details and no network. Unused on the device.
static void load_fake(const char* s) {
	fake = 1;
	snprintf(info.version, sizeof(info.version), "0.3.0");
	snprintf(info.file, sizeof(info.file), "Bramley-0.3.0-card.zip");
	info.size = 58035159;
	info.checked = time(NULL) - 3 * 3600;
	snprintf(info.notes, sizeof(info.notes),
	         "Bramley now updates itself. Software Update in Settings finds new versions, "
	         "downloads them over Wi-Fi and installs them on restart.\n"
	         "Every emulator core is built by Bramley, with its source listed on the card.");
	if (!strcmp(s, "checking")) info.state = UPDATE_CHECKING;
	else if (!strcmp(s, "current")) {
		info.state = UPDATE_CURRENT;
		snprintf(info.version, sizeof(info.version), "%s", info.current);
	} else if (!strcmp(s, "downloading")) {
		info.state = UPDATE_DOWNLOADING;
		info.done = info.size * 37 / 100;
	} else if (!strcmp(s, "ready")) info.state = UPDATE_READY;
	else if (!strcmp(s, "failed")) {
		snprintf(info.version, sizeof(info.version), "%s", info.current);
		set_error("Connect to Wi-Fi to check for updates.");
	}
	else info.state = UPDATE_AVAILABLE;
}

void update_init(void) {
	pthread_mutex_lock(&lock);
	if (loaded) {
		pthread_mutex_unlock(&lock);
		return;
	}
	loaded = 1;
	read_file(BIN_PATH "/.bramley-version", info.current, sizeof(info.current));
	trim(info.current);
	if (!info.current[0]) snprintf(info.current, sizeof(info.current), "0");
	const char* f = getenv("BRAMLEY_FAKE_UPDATE");
	if (f) {
		load_fake(f);
		pthread_mutex_unlock(&lock);
		return;
	}
	FILE* s = fopen(STATE_FILE, "r");
	if (s) {
		char line[256];
		while (fgets(line, sizeof(line), s)) {
			trim(line);
			char* v = strchr(line, ' ');
			if (!v) continue;
			*v++ = 0;
			if (!strcmp(line, "checked")) info.checked = atoll(v);
			else if (!strcmp(line, "auto")) auto_check = atoi(v);
			else if (!strcmp(line, "version")) snprintf(info.version, sizeof(info.version), "%s", v);
			else if (!strcmp(line, "file")) snprintf(info.file, sizeof(info.file), "%s", v);
			else if (!strcmp(line, "size")) info.size = atoll(v);
			else if (!strcmp(line, "sha256")) snprintf(sha256, sizeof(sha256), "%s", v);
		}
		fclose(s);
	}
	read_file(NOTES_FILE, info.notes, sizeof(info.notes));
	settle();
	pthread_mutex_unlock(&lock);
}

int update_supported(void) {
	if (getenv("BRAMLEY_FAKE_UPDATE")) return 1;
	// a Bramley card writes its name into version.txt; the Pak on NextUI does not
	char v[64];
	read_file(ROOT_SYSTEM_PATH "/version.txt", v, sizeof(v));
	return !strncmp(v, "Bramley", 7);
}

void update_get(UpdateInfo* out) {
	update_init();
	pthread_mutex_lock(&lock);
	if (info.state == UPDATE_DOWNLOADING && !fake) {
		struct stat st;
		if (stat(PART, &st) == 0) info.done = st.st_size;
	}
	info.newer = info.version[0] && newer(info.version, info.current);
	*out = info;
	pthread_mutex_unlock(&lock);
}

// Runs curl with a verified connection and waits; the pid is kept so a download can be stopped.
static int curl(const char* out, const char* url, int resume, int max_seconds) {
	char timeout[16];
	snprintf(timeout, sizeof(timeout), "%d", max_seconds);
	const char* argv[20];
	int n = 0;
	const char* fixed[] = {"curl", "-s", "-f", "-L", "--cacert", CA_BUNDLE, "--connect-timeout", "15", "-m", timeout};
	for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++) argv[n++] = fixed[i];
	if (resume) {
		argv[n++] = "-C";
		argv[n++] = "-";
	}
	argv[n++] = "-o";
	argv[n++] = out;
	argv[n++] = url;
	argv[n] = NULL;
	pid_t pid = -1;
	// posix_spawn, not fork: forking a big process with GL and threads is fragile
	if (posix_spawnp(&pid, "curl", NULL, NULL, (char* const*)argv, environ) != 0) pid = -1;
	if (pid < 0) return -1;
	pthread_mutex_lock(&lock);
	curl_pid = pid;
	pthread_mutex_unlock(&lock);
	int status = 0;
	pid_t w;
	while ((w = waitpid(pid, &status, 0)) < 0 && errno == EINTR) {}
	if (w < 0) {
		LOG_info("bramley_update: waiting for curl failed (%s)\n", strerror(errno));
		status = 0xFF00; // treated as a failure
	}
	pthread_mutex_lock(&lock);
	curl_pid = 0;
	pthread_mutex_unlock(&lock);
	int rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	if (rc != 0) LOG_info("bramley_update: curl %s ended with %d\n", url, rc);
	return rc;
}

static int sha256_of(const char* path, char hex[65]) {
	FILE* f = fopen(path, "rb");
	if (!f) return -1;
	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
	static unsigned char buf[1 << 16];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) EVP_DigestUpdate(ctx, buf, n);
	unsigned char md[32];
	unsigned int len = 0;
	EVP_DigestFinal_ex(ctx, md, &len);
	EVP_MD_CTX_free(ctx);
	fclose(f);
	for (int i = 0; i < 32; i++) sprintf(hex + i * 2, "%02x", md[i]);
	hex[64] = 0;
	return 0;
}

// The manifest: "key value" lines, then "notes" and the notes to the end.
static int parse_manifest(const char* path, UpdateInfo* m, char sum[65]) {
	FILE* f = fopen(path, "r");
	if (!f) return -1;
	char line[512];
	int format = 0, in_notes = 0;
	m->version[0] = m->file[0] = m->notes[0] = 0;
	m->size = 0;
	sum[0] = 0;
	while (fgets(line, sizeof(line), f)) {
		if (in_notes) {
			strncat(m->notes, line, sizeof(m->notes) - strlen(m->notes) - 1);
			continue;
		}
		trim(line);
		char* v = strchr(line, ' ');
		if (!strcmp(line, "notes")) {
			in_notes = 1;
			continue;
		}
		if (!v) continue;
		*v++ = 0;
		if (!strcmp(line, "bramley-update")) format = atoi(v);
		else if (!strcmp(line, "version")) snprintf(m->version, sizeof(m->version), "%s", v);
		else if (!strcmp(line, "file")) snprintf(m->file, sizeof(m->file), "%s", v);
		else if (!strcmp(line, "size")) m->size = atoll(v);
		else if (!strcmp(line, "sha256")) snprintf(sum, 65, "%s", v);
	}
	fclose(f);
	trim(m->notes);
	// a name with a slash could point outside the release
	if (format != 1 || !m->version[0] || !m->file[0] || strchr(m->file, '/') || m->size <= 0 || strlen(sum) != 64)
		return -1;
	return 0;
}

// Development aid: an address in test-server.txt (such as http://127.0.0.1:8000, reached through
// adb reverse) serves the manifest and the zip in place of GitHub, to test updates unpublished.
static int test_server(char* base, size_t size) {
	read_file(TEST_SERVER, base, size);
	trim(base);
	return base[0] != 0;
}

static void* check_thread(void* arg) {
	(void)arg;
	mkdir(BRAMLEY_STATE_DIR, 0755);
	mkdir(DIR, 0755);
	unlink(MANIFEST_TMP);
	char url[512], base[256];
	if (test_server(base, sizeof(base))) snprintf(url, sizeof(url), "%s/bramley-update.txt", base);
	else snprintf(url, sizeof(url), "%s", MANIFEST_URL);
	int rc = curl(MANIFEST_TMP, url, 0, 30);
	UpdateInfo m = {0};
	char sum[65];
	pthread_mutex_lock(&lock);
	if (rc != 0)
		set_error(rc == 22 ? "No update information was found for this version." : "Bramley couldn't reach the update server. Check the Wi-Fi connection.");
	else if (parse_manifest(MANIFEST_TMP, &m, sum) != 0)
		set_error("The update information could not be read.");
	else {
		if (strcmp(sum, sha256) != 0) {
			// a different release than the one partly downloaded
			unlink(PART);
			unlink(ZIP);
		}
		snprintf(info.version, sizeof(info.version), "%s", m.version);
		snprintf(info.file, sizeof(info.file), "%s", m.file);
		snprintf(info.notes, sizeof(info.notes), "%s", m.notes);
		snprintf(sha256, sizeof(sha256), "%s", sum);
		info.size = m.size;
		info.checked = time(NULL);
		info.error[0] = 0;
		settle();
		save_state();
	}
	unlink(MANIFEST_TMP);
	busy = 0;
	pthread_mutex_unlock(&lock);
	return NULL;
}

static void start(void* (*fn)(void*)) {
	pthread_t t;
	if (pthread_create(&t, NULL, fn, NULL) == 0)
		pthread_detach(t);
	else
		busy = 0;
}

void update_check(void) {
	update_init();
	pthread_mutex_lock(&lock);
	if (busy || fake || info.state == UPDATE_INSTALLING) {
		pthread_mutex_unlock(&lock);
		return;
	}
	if (!WIFI_connected()) {
		set_error("Connect to Wi-Fi to check for updates.");
		pthread_mutex_unlock(&lock);
		return;
	}
	busy = 1;
	last_try = time(NULL);
	info.state = UPDATE_CHECKING;
	pthread_mutex_unlock(&lock);
	start(check_thread);
}

static void* download_thread(void* arg) {
	(void)arg;
	char url[512], file[128], sum[65];
	pthread_mutex_lock(&lock);
	snprintf(file, sizeof(file), "%s", info.file);
	char base[256];
	if (test_server(base, sizeof(base))) snprintf(url, sizeof(url), "%s/%s", base, file);
	else snprintf(url, sizeof(url), RELEASE_URL, info.version, file);
	snprintf(sum, sizeof(sum), "%s", sha256);
	pthread_mutex_unlock(&lock);

	int rc = curl(PART, url, 1, 3600);
	char got[65] = "";
	pthread_mutex_lock(&lock);
	if (stopping) {
		stopping = 0;
		settle(); // keeps the part for next time
	} else if (rc != 0) {
		set_error("The download stopped. Check the Wi-Fi connection and try again.");
	} else {
		pthread_mutex_unlock(&lock);
		int ok = sha256_of(PART, got) == 0 && !strcmp(got, sum);
		pthread_mutex_lock(&lock);
		if (ok && rename(PART, ZIP) == 0) {
			info.state = UPDATE_READY;
			info.error[0] = 0;
		} else {
			unlink(PART);
			set_error("The download was damaged, so it was deleted. Try again.");
		}
	}
	busy = 0;
	pthread_mutex_unlock(&lock);
	return NULL;
}

void update_download(void) {
	update_init();
	pthread_mutex_lock(&lock);
	if (busy || fake || !info.version[0] || !newer(info.version, info.current)) {
		pthread_mutex_unlock(&lock);
		return;
	}
	if (!WIFI_connected()) {
		set_error("Connect to Wi-Fi to download the update.");
		pthread_mutex_unlock(&lock);
		return;
	}
	// room for the zip and for what it unpacks
	struct statvfs fs;
	if (statvfs(SDCARD_PATH, &fs) == 0 && (long long)fs.f_bavail * fs.f_frsize < info.size * 2 + (50LL << 20)) {
		char msg[160];
		snprintf(msg, sizeof(msg), "There is not enough space on the card. The update needs %lld MB free.",
		         (info.size * 2 >> 20) + 50);
		set_error(msg);
		pthread_mutex_unlock(&lock);
		return;
	}
	mkdir(BRAMLEY_STATE_DIR, 0755);
	mkdir(DIR, 0755);
	busy = 1;
	info.state = UPDATE_DOWNLOADING;
	info.error[0] = 0;
	pthread_mutex_unlock(&lock);
	start(download_thread);
}

void update_stop(void) {
	pthread_mutex_lock(&lock);
	if (curl_pid > 0 && info.state == UPDATE_DOWNLOADING) {
		stopping = 1;
		kill(curl_pid, SIGTERM);
	}
	pthread_mutex_unlock(&lock);
}

int update_install(void) {
	update_init();
	pthread_mutex_lock(&lock);
	if (busy || fake || info.state != UPDATE_READY) {
		pthread_mutex_unlock(&lock);
		return -1;
	}
	info.state = UPDATE_INSTALLING;
	char sum[65];
	snprintf(sum, sizeof(sum), "%s", sha256);
	pthread_mutex_unlock(&lock);

	// checked again: the file sat on the card since it was downloaded
	char got[65] = "";
	if (sha256_of(ZIP, got) != 0 || strcmp(got, sum) != 0) {
		unlink(ZIP);
		pthread_mutex_lock(&lock);
		set_error("The downloaded update was damaged, so it was deleted. Download it again.");
		pthread_mutex_unlock(&lock);
		return -1;
	}
	char cmd[1024];
	int n = snprintf(cmd, sizeof(cmd), "'%s' -o -q '%s'", UNZIP, ZIP);
	for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++)
		n += snprintf(cmd + n, sizeof(cmd) - n, " '%s'", allowed[i]);
	snprintf(cmd + n, sizeof(cmd) - n, " -d '%s' >/dev/null 2>&1", SDCARD_PATH);
	int rc = system(cmd);
	pthread_mutex_lock(&lock);
	// 0 is success; 11 means one of the patterns matched nothing, which is fine
	if (rc != 0 && !(WIFEXITED(rc) && WEXITSTATUS(rc) == 11)) {
		set_error("The update could not be unpacked. Nothing was changed; try again.");
		pthread_mutex_unlock(&lock);
		return -1;
	}
	unlink(ZIP);
	sync();
	pthread_mutex_unlock(&lock);
	return 0;
}

int update_auto(void) {
	update_init();
	return auto_check;
}

void update_set_auto(int on) {
	update_init();
	pthread_mutex_lock(&lock);
	auto_check = on;
	if (!fake) save_state();
	pthread_mutex_unlock(&lock);
}

void update_tick(void) {
	if (!update_supported()) return;
	update_init();
	time_t now = time(NULL);
	pthread_mutex_lock(&lock);
	// once a day after a good check; after a failed try, not again within the hour
	int due = auto_check && !busy && !fake && now - info.checked > DAY && now - last_try > 3600 &&
	          (info.state == UPDATE_UNKNOWN || info.state == UPDATE_CURRENT || info.state == UPDATE_AVAILABLE ||
	           info.state == UPDATE_FAILED);
	pthread_mutex_unlock(&lock);
	if (due && WIFI_connected()) update_check();
}

int update_badge(void) {
	if (!update_supported()) return 0;
	UpdateInfo i;
	update_get(&i);
	return i.newer && i.state != UPDATE_CHECKING && i.state != UPDATE_INSTALLING;
}
