// Wi-Fi and Bluetooth state for the quick menu, kept by a background thread. Turning a radio on
// runs NextUI's init scripts and takes seconds, so the launcher never calls those on its own
// thread; it asks this worker and reads back the latest status.
#ifndef LRADIO_H
#define LRADIO_H

enum { RADIO_WIFI, RADIO_BLUETOOTH, RADIO_COUNT };

typedef enum {
	RADIO_UNAVAILABLE,
	RADIO_OFF,
	RADIO_TURNING_ON,
	RADIO_TURNING_OFF,
	RADIO_ON,        // on, not connected
	RADIO_CONNECTED, // detail holds the network name for Wi-Fi
} RadioState;

typedef struct {
	RadioState state;
	char detail[64];
} RadioStatus;

void lradio_start(void);
void lradio_stop(void);

// Flips the radio on or off in the background. Ignored while that radio is already switching.
void lradio_toggle(int radio);

// While polling is on, status is refreshed about once a second.
void lradio_set_polling(int on);

// Copies the latest status and returns a counter that changes whenever any status changes.
int lradio_get(int radio, RadioStatus* out);

#endif
