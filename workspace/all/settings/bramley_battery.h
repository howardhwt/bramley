// Bramley: the battery history behind Settings > Battery, read from the log that NextUI's batmon
// daemon keeps (battery_logs.sqlite in .userdata/shared). Read only; batmon stays the only writer.
//
// The log has no clock times. Each row is a battery level, whether it was charging, and how many
// seconds the device was on at that level, so all times here are time switched on.
#ifndef BRAMLEY_BATTERY_H
#define BRAMLEY_BATTERY_H

#ifdef __cplusplus
extern "C" {
#endif

#define BATTERY_SLOTS 48

typedef struct {
	int level;    // percent, or -1 where the log has no data
	int charging;
} BatterySlot;

typedef struct {
	int found;        // whether the log could be read
	int charging;     // the newest row was charging
	int since_charge; // seconds on battery since the last charge ended, -1 if unknown
	int charged_to;   // the level when the last charge ended, -1 if unknown
	int longest;      // the longest time on one charge in seconds, 0 if unknown
	int remaining;    // estimated seconds left, -1 if there is not enough data yet
	BatterySlot slot[BATTERY_SLOTS]; // the last range seconds, oldest first
} BatteryHistory;

// Reads the log and fills h for a chart covering the last range seconds of use. Returns h->found.
int battery_history(BatteryHistory* h, int range, int level_now);

#ifdef __cplusplus
}
#endif

#endif
