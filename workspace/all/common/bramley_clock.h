// Bramley: the time as text, following Settings > System > Date & Time > 24-hour time. Status
// bars leave out AM and PM in 12-hour time, as iOS does (suffix 0); sentences keep it (suffix 1).
#ifndef BRAMLEY_CLOCK_H
#define BRAMLEY_CLOCK_H

#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "config.h"
#ifdef __cplusplus
}
#endif

static inline void bramley_clock(const struct tm* tm, char* out, size_t size, int suffix) {
	if (CFG_getClock24H()) {
		snprintf(out, size, "%02d:%02d", tm->tm_hour, tm->tm_min);
		return;
	}
	int h = tm->tm_hour % 12;
	if (suffix) snprintf(out, size, "%d:%02d %s", h ? h : 12, tm->tm_min, tm->tm_hour >= 12 ? "PM" : "AM");
	else snprintf(out, size, "%d:%02d", h ? h : 12, tm->tm_min);
}

#endif
