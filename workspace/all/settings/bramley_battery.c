// Bramley: the battery history for Settings > Battery. See bramley_battery.h.
#include "bramley_battery.h"

#include <sqlite3.h>
#include <string.h>

#include "defines.h"
#include "api.h"

#define BATTERY_LOG_FILE SHARED_USERDATA_PATH "/battery_logs.sqlite"
// Too little time on battery gives a wild estimate, and more than 15 hours is not plausible
// (the same limits as NextUI's Battery app).
#define MIN_FOR_ESTIMATE 1200
#define MAX_ESTIMATE 54000

int battery_history(BatteryHistory* h, int range, int level_now) {
	memset(h, 0, sizeof(*h));
	h->since_charge = h->charged_to = h->remaining = -1;
	for (int i = 0; i < BATTERY_SLOTS; i++) h->slot[i].level = -1;

	sqlite3* db;
	if (sqlite3_open_v2(BATTERY_LOG_FILE, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return 0;
	}
	const char* model = PLAT_getModel();
	sqlite3_stmt* st;
	if (sqlite3_prepare_v2(db, "SELECT best_session FROM device_specifics WHERE device_serial = ? ORDER BY id LIMIT 1;", -1, &st, NULL) == SQLITE_OK) {
		sqlite3_bind_text(st, 1, model, -1, SQLITE_STATIC);
		if (sqlite3_step(st) == SQLITE_ROW) h->longest = sqlite3_column_int(st, 0);
		sqlite3_finalize(st);
	}
	if (sqlite3_prepare_v2(db, "SELECT bat_level, duration, is_charging FROM bat_activity WHERE device_serial = ? ORDER BY id DESC;", -1, &st, NULL) != SQLITE_OK) {
		sqlite3_close(db);
		return 0;
	}
	sqlite3_bind_text(st, 1, model, -1, SQLITE_STATIC);
	h->found = 1;

	// Walk back from now. back is how long ago, in seconds of use, the current row ended.
	int slot_len = range / BATTERY_SLOTS, back = 0, first = 1, in_session = 1, session = 0, session_top = -1;
	while (sqlite3_step(st) == SQLITE_ROW) {
		int level = sqlite3_column_int(st, 0), duration = sqlite3_column_int(st, 1), charging = sqlite3_column_int(st, 2);
		if (level > 100) level = 100;
		// the newest row may have only just started, so on the chart it covers at least a slot
		int span = first && duration < slot_len ? slot_len : duration;
		if (first) {
			h->charging = charging;
			first = 0;
		}
		if (in_session) {
			if (charging) {
				in_session = 0;
				if (!h->charging) {
					h->since_charge = session;
					h->charged_to = session_top;
				}
			} else {
				session += duration;
				session_top = level;
			}
		}
		// the slots whose middle falls inside this row take its level
		for (int s = 0; s < BATTERY_SLOTS; s++) {
			int mid = (BATTERY_SLOTS - 1 - s) * slot_len + slot_len / 2;
			if (mid >= back && mid < back + span) {
				h->slot[s].level = level;
				h->slot[s].charging = charging;
			}
		}
		back += span;
		if (back >= range && !in_session) break;
	}
	sqlite3_finalize(st);
	sqlite3_close(db);

	if (h->since_charge >= MIN_FOR_ESTIMATE && h->charged_to > level_now) {
		long left = (long)level_now * h->since_charge / (h->charged_to - level_now);
		if (left < MAX_ESTIMATE) h->remaining = (int)left;
	}
	if (h->since_charge > h->longest) h->longest = h->since_charge;
	return 1;
}
