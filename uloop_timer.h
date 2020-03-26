// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "uloop.h"
#include "uloop_timer_config.h"

#define ULOOP_TIMER_MAX_PERIOD     ((uint32_t)INT32_MAX + 1)
#define ULOOP_TIMER_UPDATE_PERIOD  (ULOOP_TIMER_MAX_PERIOD) / 2

typedef uint32_t uloop_timer_t;

extern volatile uint32_t uloop_timer_current;
extern const uloop_event_t uloop_timer_events[ULOOP_TIMER_COUNT];

void uloop_timer_start_ex(uloop_timer_t timer, uint32_t value, bool relative);
void uloop_timer_stop(uloop_timer_t timer);
bool uloop_timer_running(uloop_timer_t timer);
void uloop_timer_init(uint32_t systick);

static inline void uloop_timer_start(uloop_timer_t timer, uint32_t value) {
	uloop_timer_start_ex(timer, value, true);
}

static inline void uloop_timer_update(uint32_t systick) {
	if ((int32_t)(uloop_timer_current - systick) < 0) {
		uloop_timer_current += ULOOP_TIMER_UPDATE_PERIOD;
		uloop_publish(E_ULOOP_TIMER_UPDATE);
	}
}
