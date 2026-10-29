// First-boot setup (see lsetup.h). Drawn with the GL kit to the motion in spikes/setup-mock.
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "defines.h"
#include "api.h"
#include "lgfx.h"
#include "lwallpaper.h"
#include "lsetup.h"
#include "bramley_clock.h"

extern int bramley_text_input(const char* app, const char* title, const char* initial, char* out, size_t size,
                              void (*backdrop)(void), void (*after)(void));
extern void launcher_debug_tick(void);

#define W 1024
#define H 768
#define GREEN 0.290f, 0.698f, 0.471f // BRAMLEY_ACCENT
#define MAX_NETS 24
#define WIFI_ROWS 5
#define ZONE_ROWS 7

enum { PAGE_WIFI, PAGE_ZONE, PAGE_CARD };

static struct {
	int open, page, prev_page;
	Uint32 intro_at;  // when the mark started to rise (0: still under the boot splash)
	Uint32 page_at;   // when the current page came in
	Uint32 closing_at; // when Get Started was pressed
	int sel, top;
	float fpos;       // focus position, following sel
	Uint32 last_tick;
	LTex *mark, *backdrop;
} S;

// --- Wi-Fi, on its own thread: switching on, scanning and joining all block for seconds ---
static pthread_t worker_thread;
static pthread_mutex_t wl = PTHREAD_MUTEX_INITIALIZER;
static volatile int worker_quit;
static struct {
	int state;                // 0 switching on, 1 on, 2 not available
	struct WIFI_network nets[MAX_NETS];
	int count;
	char connected[SSID_MAX]; // the joined network, or empty
	int join_request;
	struct WIFI_network join;
	char pass[128];
	int joining;              // 1 while joining
	int join_failed;
	char failed_ssid[SSID_MAX];
} wifi;

static int by_signal(const void* a, const void* b) {
	return ((const struct WIFI_network*)b)->rssi - ((const struct WIFI_network*)a)->rssi;
}

static void scan(void) {
	static struct WIFI_network found[64];
	int n = WIFI_scan(found, 64);
	if (n < 0) return;
	// one row per name, the strongest
	struct WIFI_network out[MAX_NETS];
	int m = 0;
	qsort(found, n, sizeof(found[0]), by_signal);
	for (int i = 0; i < n && m < MAX_NETS; i++) {
		if (!found[i].ssid[0] || found[i].security == SECURITY_UNSUPPORTED) continue;
		int dup = 0;
		for (int j = 0; j < m; j++) dup |= !strcmp(out[j].ssid, found[i].ssid);
		if (!dup) out[m++] = found[i];
	}
	struct WIFI_connection c;
	int ok = WIFI_connectionInfo(&c) >= 0 && c.valid && WIFI_connected();
	pthread_mutex_lock(&wl);
	memcpy(wifi.nets, out, sizeof(out[0]) * m);
	wifi.count = m;
	snprintf(wifi.connected, sizeof(wifi.connected), "%s", ok ? c.ssid : "");
	pthread_mutex_unlock(&wl);
}

static void* worker(void* arg) {
	(void)arg;
	if (!WIFI_supported()) {
		wifi.state = 2;
		return NULL;
	}
	if (!WIFI_enabled()) WIFI_enable(true);
	wifi.state = 1;
	Uint32 scanned = 0;
	while (!worker_quit) {
		pthread_mutex_lock(&wl);
		int req = wifi.join_request;
		struct WIFI_network n = wifi.join;
		char pass[128];
		snprintf(pass, sizeof(pass), "%s", wifi.pass);
		wifi.join_request = 0;
		pthread_mutex_unlock(&wl);
		if (req) {
			if (pass[0]) WIFI_connectPass(n.ssid, n.security, pass);
			else WIFI_connect(n.ssid, n.security);
			// joining finishes in the background; wait up to 20 s for it
			int ok = 0;
			for (int i = 0; i < 40 && !worker_quit && !ok; i++) {
				usleep(500000);
				struct WIFI_connection c;
				ok = WIFI_connected() && WIFI_connectionInfo(&c) >= 0 && c.valid && !strcmp(c.ssid, n.ssid);
			}
			pthread_mutex_lock(&wl);
			wifi.joining = 0;
			wifi.join_failed = !ok;
			snprintf(wifi.failed_ssid, sizeof(wifi.failed_ssid), "%s", ok ? "" : n.ssid);
			pthread_mutex_unlock(&wl);
			scan();
			scanned = SDL_GetTicks();
		}
		if (S.page == PAGE_WIFI && (!scanned || SDL_GetTicks() - scanned > 6000)) {
			scan();
			scanned = SDL_GetTicks();
		}
		usleep(100000);
	}
	return NULL;
}

// --- time zones ---
static char (*zones)[MAX_TZ_LENGTH];
static int zone_count, *order;
static char (*zone_times)[16];
static int times_minute = -1;

static const char* city(const char* zone) {
	const char* s = strrchr(zone, '/');
	return s ? s + 1 : zone;
}

static void region(const char* zone, char* out, size_t size) {
	const char* s = strchr(zone, '/');
	snprintf(out, size, "%.*s", s ? (int)(s - zone) : 5, s ? zone : "Other");
}

static int by_region_city(const void* a, const void* b) {
	const char *za = zones[*(const int*)a], *zb = zones[*(const int*)b];
	char ra[64], rb[64];
	region(za, ra, sizeof(ra));
	region(zb, rb, sizeof(rb));
	int c = strcmp(ra, rb);
	return c ? c : strcmp(city(za), city(zb));
}

static void load_zones(void) {
	if (zones) return;
	zones = malloc(sizeof(*zones) * MAX_TIMEZONES);
	TIME_getTimezones(zones, &zone_count);
	order = malloc(sizeof(int) * (zone_count ? zone_count : 1));
	zone_times = calloc(zone_count ? zone_count : 1, sizeof(*zone_times));
	for (int i = 0; i < zone_count; i++) order[i] = i;
	qsort(order, zone_count, sizeof(int), by_region_city);
}

// The time in a zone now, as the clock shows it.
static const char* zone_time(int z) {
	time_t now = time(NULL);
	int minute = (int)(now / 60);
	if (minute != times_minute) {
		for (int i = 0; i < zone_count; i++) zone_times[i][0] = 0;
		times_minute = minute;
	}
	if (!zone_times[z][0]) {
		char saved[128] = "";
		const char* tz = getenv("TZ");
		if (tz) snprintf(saved, sizeof(saved), "%s", tz);
		setenv("TZ", zones[z], 1);
		tzset();
		struct tm tm;
		localtime_r(&now, &tm);
		bramley_clock(&tm, zone_times[z], sizeof(zone_times[z]), 1);
		if (tz) setenv("TZ", saved, 1);
		else unsetenv("TZ");
		tzset();
	}
	return zone_times[z];
}

// The zone in use, read once: on the device the lookup runs a command and allocates.
static char current_tz[MAX_TZ_LENGTH];

static void read_current_zone(void) {
	char* now = TIME_getCurrentTimezone();
	snprintf(current_tz, sizeof(current_tz), "%s", now ? now : "");
	free(now);
}

static int current_zone_row(void) {
	for (int i = 0; i < zone_count; i++)
		if (!strcmp(zones[order[i]], current_tz)) return i;
	return 0;
}

// --- open and close ---
int lsetup_is_open(void) { return S.open; }
int lsetup_covers(void) { return S.open && S.page != PAGE_CARD; }

int lsetup_begin(void) {
	if (access(SETUP_PENDING, F_OK) != 0) return 0;
	memset(&S, 0, sizeof(S));
	S.open = 1;
	S.page = S.prev_page = PAGE_WIFI;
	S.mark = ltex_load(BRAMLEY_RES_DIR "/brand/mark-150.png", 0);
	Wallpaper w;
	wallpaper_current(&w);
	if (w.blur[0]) S.backdrop = ltex_load(w.blur, 0);
	S.last_tick = SDL_GetTicks();
	worker_quit = 0;
	pthread_create(&worker_thread, NULL, worker, NULL);
	return 1;
}

static void finish(void) {
	// the worker may be inside a scan for seconds; it stops on its own, nobody waits for it
	worker_quit = 1;
	pthread_detach(worker_thread);
	unlink(SETUP_PENDING);
	ltex_free(S.mark);
	ltex_free(S.backdrop);
	S.mark = S.backdrop = NULL;
	S.open = 0;
}

static void scroll_to(int rows, int count);

static void go(int page) {
	S.prev_page = S.page;
	S.page = page;
	S.page_at = SDL_GetTicks();
	S.top = 0;
	if (page == PAGE_ZONE) {
		load_zones();
		read_current_zone();
		S.sel = current_zone_row();
		S.top = S.sel - ZONE_ROWS / 2;
		scroll_to(ZONE_ROWS, zone_count);
	} else S.sel = 0;
	S.fpos = S.sel;
}

// --- motion helpers ---
static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float ease_out(float x) { x = clamp01(x); return 1 - powf(1 - x, 3); }
static float ease_in_out(float x) { x = clamp01(x); return x < 0.5f ? 4 * x * x * x : 1 - powf(-2 * x + 2, 3) / 2; }
static float mix(float a, float b, float t) { return a + (b - a) * t; }
// seconds since a moment, 0 before it
static float since(Uint32 at) { return at ? (SDL_GetTicks() - at) / 1000.0f : 0; }

static float text(int font, const char* s, float x, float y, float alpha, int align) {
	if (alpha <= 0.01f || !s || !*s) return 0;
	LTex* t = ltex_text(font, s, 0);
	if (!t) return 0;
	float w = t->w;
	lgfx_draw_tex(t, align == 1 ? x - t->w : align == 2 ? x - t->w * 0.5f : x, y, t->w, t->h, 0, alpha);
	ltex_free(t);
	return w;
}

static float text_w(int font, const char* s) {
	LTex* t = ltex_text(font, s, 0);
	float w = t ? t->w : 0;
	ltex_free(t);
	return w;
}

static void wrapped(const char* s, float x, float y, float w, float alpha) {
	LTex* t = ltex_text_wrapped(LFONT_BODY, s, (int)w, 2, 1);
	if (!t) return;
	lgfx_draw_tex(t, x + (w - t->w) * 0.5f, y, t->w, t->h, 0, alpha);
	ltex_free(t);
}

static void spinner(float cx, float cy, float r, float alpha) {
	float t = SDL_GetTicks() / 1000.0f;
	for (int i = 0; i < 8; i++) {
		float a = i * 6.2831853f / 8;
		float k = (float)((i + (int)(t * 12)) % 8) / 8.0f;
		lgfx_draw_fill(cx + cosf(a) * r - 2, cy + sinf(a) * r - 2, 4, 4, 2, 1, 1, 1, alpha * (0.25f + 0.75f * k));
	}
}

static void bars(float right, float cy, int rssi, float alpha, int dark) {
	int level = rssi > -60 ? 3 : rssi > -72 ? 2 : 1;
	float c = dark ? 0.1f : 1;
	for (int i = 0; i < 3; i++) {
		float h = 7 + i * 5, x = right - 30 + i * 9;
		lgfx_draw_fill(x, cy + 8 - h, 5, h, 2, c, c, c, alpha * (i < level ? 0.9f : 0.25f));
	}
}

static void row(float x, float y, float w, float h, float focus, float alpha) {
	if (focus > 0.01f) {
		float g = 6 * focus;
		lgfx_draw_shadow(x - g, y - g * 0.5f, w + 2 * g, h + g, 6, 0.32f * alpha * focus);
		lgfx_draw_fill(x - g, y - g * 0.5f, w + 2 * g, h + g, 12, 1, 1, 1, 0.96f * alpha * focus);
	}
	lgfx_draw_fill(x, y, w, h, 12, 1, 1, 1, 0.06f * alpha * (1 - focus));
}

static void pill(const char* label, float cx, float y, float focus, float alpha) {
	float bw = 240, bh = 54, bx = cx - bw * 0.5f;
	if (focus > 0.5f) lgfx_draw_shadow(bx, y, bw, bh, 8, 0.3f * alpha);
	lgfx_draw_fill(bx, y, bw, bh, bh * 0.5f, GREEN, alpha * (0.55f + 0.45f * focus));
	text(LFONT_BODY, label, cx, y + 12, alpha, 2);
}

// --- pages ---
// How much of a page shows, and how far it has slid: pages push to the left, as on iOS.
static float page_alpha(int page, float* dx) {
	float t = since(S.page_at);
	if (page == S.page) {
		float in = S.page_at ? ease_out((t - 0.3f) / 0.55f) : 1;
		*dx = (1 - in) * 320;
		return in;
	}
	if (page == S.prev_page && S.page_at) {
		float out = ease_in_out(t / 0.35f);
		*dx = -out * 320;
		return 1 - out;
	}
	*dx = 0;
	return 0;
}

static int wifi_rows(void) {
	pthread_mutex_lock(&wl);
	int n = wifi.count;
	pthread_mutex_unlock(&wl);
	return n;
}

static void draw_wifi(float intro) {
	float dx, a = page_alpha(PAGE_WIFI, &dx);
	if (a <= 0.01f) return;
	float title = ease_out((intro - 0.4f) / 0.7f) * a, rise = (1 - ease_out((intro - 0.4f) / 0.7f)) * 24;
	text(LFONT_TITLE, "Welcome to Bramley", W * 0.5f + dx, 176 + rise, title, 2);
	wrapped("Choose a Wi-Fi network. Bramley uses it to set the clock and to find updates.", 232 + dx, 228 + rise, 560, 0.6f * title);

	pthread_mutex_lock(&wl);
	int n = wifi.count, state = wifi.state, joining = wifi.joining;
	struct WIFI_network nets[MAX_NETS];
	memcpy(nets, wifi.nets, sizeof(nets[0]) * n);
	char connected[SSID_MAX], failed[SSID_MAX], joining_ssid[SSID_MAX];
	snprintf(connected, sizeof(connected), "%s", wifi.connected);
	snprintf(failed, sizeof(failed), "%s", wifi.failed_ssid);
	snprintf(joining_ssid, sizeof(joining_ssid), "%s", wifi.join.ssid);
	pthread_mutex_unlock(&wl);

	float x = 212 + dx, w = 600, rh = 52, gap = 8, y0 = 312;
	float list = ease_out((intro - 0.75f) / 0.5f) * a;
	if (n == 0) {
		const char* msg = state == 2 ? "Wi-Fi is not available on this device." : state == 0 ? "Turning on Wi-Fi\xE2\x80\xA6" : "Looking for networks\xE2\x80\xA6";
		float tw = text_w(LFONT_BODY, msg);
		if (state != 2) spinner(W * 0.5f + dx - tw * 0.5f - 18, y0 + 20, 8, list);
		text(LFONT_BODY, msg, W * 0.5f + dx + (state != 2 ? 8 : 0), y0 + 5, 0.6f * list, 2);
	}
	for (int k = 0; k < WIFI_ROWS && S.top + k < n; k++) {
		int i = S.top + k;
		float st = ease_out((intro - 0.75f - k * 0.08f) / 0.5f) * a;
		float y = y0 + k * (rh + gap) + (1 - st) * 18;
		float focus = fmaxf(0, 1 - fabsf(S.fpos - i));
		row(x, y, w, rh, focus, st);
		int dark = focus > 0.5f;
		if (dark) lgfx_set_dim(0.1f);
		text(LFONT_BODY, nets[i].ssid, x + 20, y + 11, st * (dark ? 1 : 0.92f), 0);
		float right = x + w - 20;
		if (joining && !strcmp(joining_ssid, nets[i].ssid)) {
			lgfx_set_dim(1);
			spinner(right - 50, y + rh * 0.5f, 8, st * (dark ? 0.0f : 1));
			if (dark) lgfx_set_dim(0.1f);
			text(LFONT_BODY, "Joining\xE2\x80\xA6", right - 70, y + 11, st * 0.6f, 1);
		} else if (!strcmp(connected, nets[i].ssid)) {
			float cw = text_w(LFONT_BODY, "Connected");
			text(LFONT_BODY, "Connected", right - 40, y + 11, st * (dark ? 1 : 0.7f), 1);
			lgfx_set_dim(1);
			lgfx_draw_fill(right - 40 - cw - 22, y + rh * 0.5f - 6, 12, 12, 6, GREEN, st);
			if (dark) lgfx_set_dim(0.1f);
		} else if (!strcmp(failed, nets[i].ssid)) {
			text(LFONT_BODY, "Couldn\xE2\x80\x99t join", right - 40, y + 11, st * 0.6f, 1);
		} else if (nets[i].security != SECURITY_NONE && !dark) {
			text(LFONT_SMALL, "Secured", right - 40, y + 15, st * 0.4f, 1);
		}
		bars(right, y + rh * 0.5f, nets[i].rssi, st, dark);
		lgfx_set_dim(1);
	}
	if (connected[0]) {
		float shown = (float)(n < WIFI_ROWS ? n : WIFI_ROWS);
		float focus = fmaxf(0, 1 - fabsf(S.fpos - n));
		pill("Continue", W * 0.5f + dx, y0 + shown * (rh + gap) + 28, focus, list);
	}
	const char* hint = S.sel >= n && connected[0] ? "A  Continue        B  Skip" : "A  Join        B  Set up later";
	text(LFONT_SMALL, hint, 40, H - 40, 0.6f * a * title, 0);
}

static void draw_zone(void) {
	float dx, a = page_alpha(PAGE_ZONE, &dx);
	if (a <= 0.01f || !zones) return;
	text(LFONT_TITLE, "Time Zone", W * 0.5f + dx, 104, a, 2);
	pthread_mutex_lock(&wl);
	int online = wifi.connected[0] != 0;
	pthread_mutex_unlock(&wl);
	wrapped(online ? "The clock is set from the internet. Choose your city for the right hour."
	                         : "Choose your city. Connect to Wi-Fi later to set the clock from the internet.",
	        232 + dx, 156, 560, 0.6f * a);
	char reg[64];
	region(zones[order[S.sel]], reg, sizeof(reg));
	for (char* c = reg; *c; c++) *c = (*c >= 'a' && *c <= 'z') ? *c - 32 : *c;
	text(LFONT_LABEL, reg, 232 + dx, 236, 0.55f * a, 0);
	text(LFONT_LABEL, "L1  R1  Region", 792 + dx, 236, 0.4f * a, 1);
	float x = 212 + dx, w = 600, rh = 52, gap = 8, y0 = 268;
	const char* now = current_tz;
	for (int k = 0; k < ZONE_ROWS && S.top + k < zone_count; k++) {
		int i = S.top + k, z = order[i];
		float st = ease_out((since(S.page_at) - 0.45f - k * 0.05f) / 0.5f) * a;
		float y = y0 + k * (rh + gap);
		float focus = fmaxf(0, 1 - fabsf(S.fpos - i));
		row(x, y, w, rh, focus, st);
		int dark = focus > 0.5f;
		if (dark) lgfx_set_dim(0.1f);
		char name[MAX_TZ_LENGTH];
		snprintf(name, sizeof(name), "%s", city(zones[z]));
		for (char* c = name; *c; c++) if (*c == '_') *c = ' ';
		text(LFONT_BODY, name, x + 20, y + 11, st * (dark ? 1 : 0.92f), 0);
		const char* t = zone_time(z);
		float right = x + w - 20;
		text(LFONT_BODY, t, right, y + 11, st * (dark ? 0.6f : 0.45f), 1);
		lgfx_set_dim(1);
		if (!strcmp(now, zones[z]))
			lgfx_draw_fill(right - text_w(LFONT_BODY, t) - 30, y + rh * 0.5f - 6, 12, 12, 6, GREEN, st);
	}
	text(LFONT_SMALL, "A  Choose        L1 R1  Region        B  Skip", 40, H - 40, 0.6f * a, 0);
}

// Over Home: how to get around, once.
static void draw_card(void) {
	float t = since(S.page_at);
	// a soft spring, as the launcher's focus moves
	float x = clamp01((t - 0.5f) / 0.7f);
	float c = 1 - expf(-6.5f * x) * cosf(7.5f * x);
	if (S.closing_at) c *= 1 - ease_out(since(S.closing_at) / 0.22f); // leaves at once on the press
	if (c <= 0.01f) return;
	float cw = 560, ch = 236, cx = (W - cw) * 0.5f, cy = H - ch - 64 + (1 - c) * 40;
	lgfx_draw_shadow(cx, cy, cw, ch, 18, 0.5f * c);
	lgfx_draw_fill(cx, cy, cw, ch, 26, 0.11f, 0.11f, 0.13f, 0.92f * fminf(1, c));
	lgfx_draw_ring(cx, cy, cw, ch, 26, 1, 1, 1, 1, 0.10f * fminf(1, c));
	text(LFONT_TITLE, "You\xE2\x80\x99re all set", W * 0.5f, cy + 24, fminf(1, c), 2);
	const char* keys[] = { "MENU", "SELECT" };
	const char* what[] = { "Quick settings, from anywhere", "Now Playing, for music and books" };
	for (int i = 0; i < 2; i++) {
		float ry = cy + 84 + i * 46;
		float kw = text_w(LFONT_LABEL, keys[i]) + 20, col = text_w(LFONT_LABEL, "SELECT") + 20; // one column for both lines
		lgfx_draw_fill(cx + 48, ry, kw, 30, 8, 1, 1, 1, 0.14f * fminf(1, c));
		text(LFONT_LABEL, keys[i], cx + 58, ry + 5, 0.9f * fminf(1, c), 0);
		text(LFONT_BODY, what[i], cx + 48 + col + 16, ry, 0.85f * fminf(1, c), 0);
	}
	float bw = 200, bh = 46;
	lgfx_draw_fill(W * 0.5f - bw * 0.5f, cy + ch - 66, bw, bh, bh * 0.5f, GREEN, fminf(1, c));
	text(LFONT_BODY, "Get Started", W * 0.5f, cy + ch - 58, fminf(1, c), 2);
}

static void keyboard_backdrop(void) {
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 1);
	if (S.backdrop) lgfx_draw_tex(S.backdrop, 0, 0, W, H, 0, 1);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.25f);
}

void lsetup_draw(float splash_alpha) {
	if (!S.open) return;
	if (S.page == PAGE_CARD) {
		// the setup pages clear away over Home, then the card rises
		float f = 1 - ease_out(since(S.page_at) / 0.45f);
		if (f > 0.01f) {
			if (S.backdrop) lgfx_draw_tex(S.backdrop, 0, 0, W, H, 0, f);
			lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.25f * f);
			draw_zone();
		}
		draw_card();
		return;
	}
	// the mark rises out of the boot splash once the splash starts to fade
	if (!S.intro_at && splash_alpha < 0.999f) S.intro_at = SDL_GetTicks() | 1;
	float intro = since(S.intro_at);
	float bd = ease_out(intro / 0.8f);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 1);
	if (S.backdrop) lgfx_draw_tex(S.backdrop, 0, 0, W, H, 0, bd);
	lgfx_draw_fill(0, 0, W, H, 0, 0, 0, 0, 0.25f * bd);
	if (S.mark) {
		float k = ease_in_out((intro - 0.05f) / 0.8f), dx;
		float out = page_alpha(PAGE_WIFI, &dx);
		float s = mix(150, 84, k), cx = W * 0.5f + dx, cy = mix(H * 0.5f - 20, 112, k);
		lgfx_draw_tex(S.mark, cx - s * 0.5f, cy - s * 0.5f, s, s, 0, out);
	}
	draw_wifi(intro);
	draw_zone();
}

int lsetup_update(void) {
	if (!S.open) return 0;
	if (S.closing_at && since(S.closing_at) > 0.25f) {
		finish();
		return 1;
	}
	Uint32 now = SDL_GetTicks();
	float dt = (now - S.last_tick) / 1000.0f;
	S.last_tick = now;
	S.fpos += (S.sel - S.fpos) * fminf(1, dt * 16);
	if (fabsf(S.sel - S.fpos) < 0.002f) S.fpos = S.sel;
	return 1; // the spinner, the Wi-Fi list and the motion all change on their own
}

static void scroll_to(int rows, int count) {
	if (S.sel < S.top) S.top = S.sel;
	if (S.sel >= S.top + rows) S.top = S.sel - rows + 1;
	if (S.top > count - rows) S.top = count - rows;
	if (S.top < 0) S.top = 0;
}

static void join(int i) {
	pthread_mutex_lock(&wl);
	struct WIFI_network n = wifi.nets[i];
	int busy = wifi.joining;
	pthread_mutex_unlock(&wl);
	if (busy) return;
	char pass[128] = "";
	if (n.security != SECURITY_NONE && !WIFI_isKnown(n.ssid, n.security)) {
		char title[SSID_MAX + 32];
		snprintf(title, sizeof(title), "Password for %s", n.ssid);
		if (!bramley_text_input("Wi-Fi", title, "", pass, sizeof(pass), keyboard_backdrop, launcher_debug_tick)) return;
	}
	pthread_mutex_lock(&wl);
	wifi.join = n;
	snprintf(wifi.pass, sizeof(wifi.pass), "%s", pass);
	wifi.join_request = 1;
	wifi.joining = 1;
	wifi.failed_ssid[0] = 0;
	pthread_mutex_unlock(&wl);
}

int lsetup_input(void) {
	if (!S.open) return 0;
	if (S.page == PAGE_CARD) {
		if (!S.closing_at && since(S.page_at) > 0.6f && (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B))) {
			S.closing_at = SDL_GetTicks() | 1;
			return 1;
		}
		return 0;
	}
	if (!S.intro_at || since(S.intro_at) < 0.9f) return 0; // the intro plays first
	if (S.page == PAGE_WIFI) {
		pthread_mutex_lock(&wl);
		int connected = wifi.connected[0] != 0;
		pthread_mutex_unlock(&wl);
		int n = wifi_rows(), count = n + (connected ? 1 : 0);
		static int was_connected;
		if (connected && !was_connected) S.sel = n; // joined: Continue takes the focus
		was_connected = connected;
		if (S.sel >= count) S.sel = count ? count - 1 : 0;
		if (PAD_justRepeated(BTN_DOWN) && S.sel < count - 1) S.sel++;
		else if (PAD_justRepeated(BTN_UP) && S.sel > 0) S.sel--;
		else if (PAD_justPressed(BTN_A)) {
			if (connected && S.sel == n) {
				go(PAGE_ZONE);
				return 1;
			}
			if (S.sel < n) join(S.sel);
		} else if (PAD_justPressed(BTN_B)) {
			go(PAGE_ZONE);
			return 1;
		} else return 0;
		scroll_to(WIFI_ROWS, n);
		return 1;
	}
	// time zone
	if (PAD_justRepeated(BTN_DOWN) && S.sel < zone_count - 1) S.sel++;
	else if (PAD_justRepeated(BTN_UP) && S.sel > 0) S.sel--;
	else if (PAD_justRepeated(BTN_R1) || PAD_justRepeated(BTN_L1)) {
		char here[64], r[64];
		region(zones[order[S.sel]], here, sizeof(here));
		int i = S.sel;
		if (PAD_justRepeated(BTN_R1)) {
			while (i < zone_count - 1 && (region(zones[order[i]], r, sizeof(r)), !strcmp(r, here))) i++;
		} else {
			// to the start of this region, or of the one before when already there
			if (i > 0) {
				region(zones[order[i - 1]], r, sizeof(r));
				if (strcmp(r, here)) {
					i--;
					region(zones[order[i]], here, sizeof(here));
				}
			}
			while (i > 0 && (region(zones[order[i - 1]], r, sizeof(r)), !strcmp(r, here))) i--;
		}
		S.sel = i;
		S.top = i;
	} else if (PAD_justPressed(BTN_A)) {
		TIME_setCurrentTimezone(zones[order[S.sel]]);
		snprintf(current_tz, sizeof(current_tz), "%s", zones[order[S.sel]]);
		if (WIFI_connected()) TIME_setNetworkTimeSync(true);
		times_minute = -1;
		go(PAGE_CARD);
		return 1;
	} else if (PAD_justPressed(BTN_B)) {
		go(PAGE_CARD);
		return 1;
	} else return 0;
	scroll_to(ZONE_ROWS, zone_count);
	return 1;
}
