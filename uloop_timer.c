// SPDX-License-Identifier: MIT

#include "uloop_timer.h"
#include "uloop.h"
#include "uloop_platform.h"
#include "uloop_listeners.h"

#define RUN_THRESHOLD ((int32_t)0xFFFF0000)

#if ULOOP_TIMER_COUNT > 1
static uint32_t timeouts[ULOOP_TIMER_COUNT];
#else
static uint32_t timeouts[1];
#endif

volatile uint32_t uloop_timer_current;

bool uloop_timer_running(uloop_timer_t timer) {
	ULOOP_DEV_ASSERT(timer < ULOOP_TIMER_COUNT);
	int32_t diff = timeouts[timer] - ULOOP_SYSTICK();
	return diff > RUN_THRESHOLD;
}

void uloop_timer_start_ex(uloop_timer_t timer, uint32_t value, bool relative) {
	uint32_t systick = ULOOP_SYSTICK();
	ULOOP_DEV_ASSERT(
		(timer < ULOOP_TIMER_COUNT) &&
		(!relative || (value <= ULOOP_TIMER_MAX_PERIOD))
	);
	uint32_t timeout = (relative ? (systick + value) : value) - 1;
	timeouts[timer] = timeout;
	if ((int32_t)(timeout - uloop_timer_current) < 0) {
		uloop_timer_current = timeout;
	}
}

void uloop_timer_stop(uloop_timer_t timer) {
	ULOOP_DEV_ASSERT(timer < ULOOP_TIMER_COUNT);
	timeouts[timer] = ULOOP_SYSTICK() + RUN_THRESHOLD;
}

#if ULOOP_DATA_QUEUE_SIZE > 0
void uloop_timer_listener(uloop_event_t event, const void* data, uint32_t size) {
	(void) data;
	(void) size;
	ULOOP_DEV_ASSERT(size == 0);
#else
void uloop_timer_listener(uloop_event_t event) {
#endif
	(void) event;
	ULOOP_DEV_ASSERT(event == E_ULOOP_TIMER_UPDATE);
	uint32_t systick = ULOOP_SYSTICK();
	uint32_t off_value = systick + RUN_THRESHOLD;
	uint32_t next = systick + ULOOP_TIMER_UPDATE_PERIOD - 1;
	for (uint32_t i = 0; i < ULOOP_TIMER_COUNT; i++) {
		int32_t diff = timeouts[i] - systick;
		if (diff < 0) {
			if (diff > RUN_THRESHOLD) {
				uloop_publish(uloop_timer_events[i]);
			}
			timeouts[i] = off_value;
		} else if ((int32_t)(next - timeouts[i]) > 0) {
			next = timeouts[i];
		}
	}
	uloop_timer_current = next;
}

void uloop_timer_init(uint32_t systick) {
	for (uint32_t i = 0; i < ULOOP_TIMER_COUNT; i++) {
		timeouts[i] = systick + RUN_THRESHOLD;
	}
	uloop_timer_current = systick + ULOOP_TIMER_UPDATE_PERIOD - 1;
}
