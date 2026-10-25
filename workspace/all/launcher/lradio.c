#include <stdio.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "lradio.h"

static SDL_Thread* thread;
static SDL_mutex* lock;
static SDL_cond* wake;
static int running, polling, version;
static int pending[RADIO_COUNT];
static RadioStatus status[RADIO_COUNT];

static void set_status(int radio, RadioState state, const char* detail) {
	SDL_LockMutex(lock);
	if (status[radio].state != state || strcmp(status[radio].detail, detail ? detail : "")) {
		status[radio].state = state;
		snprintf(status[radio].detail, sizeof(status[radio].detail), "%s", detail ? detail : "");
		version++;
	}
	SDL_UnlockMutex(lock);
}

static void poll_wifi(void) {
	if (!WIFI_supported()) {
		set_status(RADIO_WIFI, RADIO_UNAVAILABLE, NULL);
		return;
	}
	if (!WIFI_enabled()) {
		set_status(RADIO_WIFI, RADIO_OFF, NULL);
		return;
	}
	struct WIFI_connection info = { 0 };
	WIFI_connectionInfo(&info);
	if (info.valid && info.ssid[0]) set_status(RADIO_WIFI, RADIO_CONNECTED, info.ssid);
	else set_status(RADIO_WIFI, RADIO_ON, NULL);
}

static void poll_bluetooth(void) {
	if (!BT_supported()) set_status(RADIO_BLUETOOTH, RADIO_UNAVAILABLE, NULL);
	else if (!BT_enabled()) set_status(RADIO_BLUETOOTH, RADIO_OFF, NULL);
	else if (PLAT_bluetoothConnected()) set_status(RADIO_BLUETOOTH, RADIO_CONNECTED, NULL);
	else set_status(RADIO_BLUETOOTH, RADIO_ON, NULL);
}

static void toggle(int radio) {
	int on = radio == RADIO_WIFI ? WIFI_enabled() : BT_enabled();
	set_status(radio, on ? RADIO_TURNING_OFF : RADIO_TURNING_ON, NULL);
	if (radio == RADIO_WIFI) WIFI_enable(!on);
	else BT_enable(!on);
}

static int worker(void* unused) {
	int first = 1;
	SDL_LockMutex(lock);
	while (running) {
		int work[RADIO_COUNT];
		for (int i = 0; i < RADIO_COUNT; i++) {
			work[i] = pending[i];
			pending[i] = 0;
		}
		int poll = polling || first;
		first = 0;
		SDL_UnlockMutex(lock);

		for (int i = 0; i < RADIO_COUNT; i++)
			if (work[i]) toggle(i);
		if (poll || work[RADIO_WIFI] || work[RADIO_BLUETOOTH]) {
			poll_wifi();
			poll_bluetooth();
		}

		SDL_LockMutex(lock);
		if (running && !pending[RADIO_WIFI] && !pending[RADIO_BLUETOOTH]) SDL_CondWaitTimeout(wake, lock, 1000);
	}
	SDL_UnlockMutex(lock);
	return 0;
}

void lradio_start(void) {
	lock = SDL_CreateMutex();
	wake = SDL_CreateCond();
	running = 1;
	// The first status check runs on the worker too, since reading the Wi-Fi link can block.
	for (int i = 0; i < RADIO_COUNT; i++) status[i].state = RADIO_OFF;
	thread = SDL_CreateThread(worker, "launcher-radio", NULL);
}

void lradio_stop(void) {
	if (!thread) return;
	SDL_LockMutex(lock);
	running = 0;
	SDL_CondSignal(wake);
	SDL_UnlockMutex(lock);
	// Waits for a toggle in progress, so a radio is never left half switched.
	SDL_WaitThread(thread, NULL);
	thread = NULL;
	SDL_DestroyCond(wake);
	SDL_DestroyMutex(lock);
}

void lradio_toggle(int radio) {
	SDL_LockMutex(lock);
	RadioState s = status[radio].state;
	if (s != RADIO_UNAVAILABLE && s != RADIO_TURNING_ON && s != RADIO_TURNING_OFF && !pending[radio]) {
		pending[radio] = 1;
		// Show the switch at once; the worker confirms it when the script finishes.
		int was_on = s == RADIO_ON || s == RADIO_CONNECTED;
		status[radio].state = was_on ? RADIO_TURNING_OFF : RADIO_TURNING_ON;
		status[radio].detail[0] = '\0';
		version++;
		SDL_CondSignal(wake);
	}
	SDL_UnlockMutex(lock);
}

void lradio_set_polling(int on) {
	SDL_LockMutex(lock);
	polling = on;
	if (on) SDL_CondSignal(wake);
	SDL_UnlockMutex(lock);
}

int lradio_get(int radio, RadioStatus* out) {
	SDL_LockMutex(lock);
	*out = status[radio];
	int v = version;
	SDL_UnlockMutex(lock);
	return v;
}
