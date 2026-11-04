// Bramley: the time as text, following Settings > System > Date & Time > 24-hour time. Status
// bars leave out AM and PM in 12-hour time, as iOS does (suffix 0); sentences keep it (suffix 1).
#ifndef BRAMLEY_CLOCK_H
#define BRAMLEY_CLOCK_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "config.h"
#ifdef __cplusplus
}
#endif

static inline void bramley_clock(const struct tm* tm, char* out, size_t size, int suffix) {
	// BRAMLEY_DEBUG_CLOCK=9:41 shows that time everywhere, so screen recordings for screenshots and the
	// launch film agree. Desktop and recording use only; nothing sets it on the device.
	const char* fixed = getenv("BRAMLEY_DEBUG_CLOCK");
	if (fixed && *fixed) {
		snprintf(out, size, "%s", fixed);
		return;
	}
	if (CFG_getClock24H()) {
		snprintf(out, size, "%02d:%02d", tm->tm_hour, tm->tm_min);
		return;
	}
	int h = tm->tm_hour % 12;
	if (suffix) snprintf(out, size, "%d:%02d %s", h ? h : 12, tm->tm_min, tm->tm_hour >= 12 ? "PM" : "AM");
	else snprintf(out, size, "%d:%02d", h ? h : 12, tm->tm_min);
}

#endif
