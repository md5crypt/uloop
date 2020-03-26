// SPDX-License-Identifier: MIT

#include <string.h>

#include "uloop.h"
#include "uloop_platform.h"

#define ULOOP_HOOK_NULL  do { /* empty */ } while (false)

#ifndef ULOOP_HOOK_INIT
#define ULOOP_HOOK_INIT()                                    ULOOP_HOOK_NULL
#endif

#ifndef ULOOP_HOOK_PUBLISH
#define ULOOP_HOOK_PUBLISH(event, data, size)                ULOOP_HOOK_NULL
#endif

#ifndef ULOOP_HOOK_PRE_DISPATCH
#define ULOOP_HOOK_PRE_DISPATCH(event, data, size)           ULOOP_HOOK_NULL
#endif

#ifndef ULOOP_HOOK_POST_DISPATCH
#define ULOOP_HOOK_POST_DISPATCH(event, data, size)          ULOOP_HOOK_NULL
#endif

#ifndef ULOOP_HOOK_PRE_EXECUTE
#define ULOOP_HOOK_PRE_EXECUTE(listener, event, data, size)  ULOOP_HOOK_NULL
#endif

#ifndef ULOOP_HOOK_POST_EXECUTE
#define ULOOP_HOOK_POST_EXECUTE(listener, event, data, size) ULOOP_HOOK_NULL
#endif

#if ULOOP_DATA_QUEUE_SIZE > 0
#define EVENT_QUEUE_ITEM(event, _size) (uloop_event_queue_item_t) {.id = event, .size = (uint8_t) _size}
#else
#define EVENT_QUEUE_ITEM(event, size) (uloop_event_queue_item_t) {.id = event}
#endif

#if ULOOP_DATA_QUEUE_SIZE > 0
static struct {
	uint32_t head;
	uint32_t tail;
	uint32_t end;
	uint8_t data[ULOOP_DATA_QUEUE_SIZE];
} data_queue;
#endif

static struct {
	uint32_t head;
	uint32_t tail;
	uloop_event_queue_item_t data[ULOOP_EVENT_QUEUE_SIZE];
} event_queue;

uloop_listener_id_t uloop_listener_active;

#ifdef ULOOP_STATISTICS_ENABLED
uloop_event_stats_t uloop_event_stats[ULOOP_EVENT_COUNT] = {0};
uloop_listenter_stats_t uloop_listener_stats[ULOOP_LISTENER_COUNT] = {0};
#endif

static inline bool event_queue_empty() {
	return event_queue.head == event_queue.tail;
}

static inline void event_queue_push(uloop_event_t event, uint32_t size) {
	ULOOP_DEV_ASSERT((ULOOP_DATA_QUEUE_SIZE == 0) || (size < 256));
	event_queue.data[event_queue.tail] = EVENT_QUEUE_ITEM(event, size);
	uint32_t tail = (event_queue.tail + 1) % ULOOP_EVENT_QUEUE_SIZE;
	if (tail == event_queue.head) {
		ULOOP_ERROR_EQOVF();
	}
	event_queue.tail = tail;
}

static inline uloop_event_queue_item_t event_queue_top() {
	return event_queue.data[event_queue.head];
}

static inline void event_queue_pop() {
	event_queue.head = (event_queue.head + 1) % ULOOP_EVENT_QUEUE_SIZE;
}

#if ULOOP_DATA_QUEUE_SIZE > 0
static inline const uint8_t* data_queue_top() {
	return &data_queue.data[data_queue.head];
}

static uint8_t* data_queue_push(uint32_t unaligned_size) {
	uint8_t* ret;
	uint32_t size = (unaligned_size + 3) & (~3);
	if (data_queue.head == data_queue.tail) {
		ULOOP_DEV_ASSERT(size < ULOOP_DATA_QUEUE_SIZE);
		ret = data_queue.data;
		data_queue.head = 0;
		data_queue.tail = size;
	} else if (data_queue.tail > data_queue.head) {
		uint32_t end_size = ULOOP_DATA_QUEUE_SIZE - data_queue.tail;
		if (size < end_size) {
			ret = &data_queue.data[data_queue.tail];
			data_queue.tail += size;
		} else if ((size == end_size) && (data_queue.head > 0)) {
			ret = &data_queue.data[data_queue.tail];
			data_queue.tail = 0;
		} else if (size < data_queue.head) {
			ret = data_queue.data;
			data_queue.end = data_queue.tail;
			data_queue.tail = size;
		} else {
			ret = NULL;
		}
	} else {
		if (size < (data_queue.head - data_queue.tail)) {
			ret = &data_queue.data[data_queue.tail];
			data_queue.tail += size;
		} else {
			ret = NULL;
		}
	}
	return ret;
}

static void data_queue_pop(uint32_t unaligned_size) {
	uint32_t size = (unaligned_size + 3) & (~3);
	ULOOP_ATOMIC_BLOCK_ENTER();
	ULOOP_DEV_ASSERT(data_queue.head != data_queue.tail);
	data_queue.head += size;
	if (data_queue.head > data_queue.end) {
		data_queue.head -= size;
		ULOOP_ERROR_DQCORR();
	} else if (data_queue.head == data_queue.end) {
		data_queue.head = 0;
		data_queue.end = ULOOP_DATA_QUEUE_SIZE;
	} else {
		// do nothing
	}
	ULOOP_ATOMIC_BLOCK_LEAVE();
}
#endif

static inline void update_listener_stats(uloop_listenter_stats_t* stats, uint32_t duration) {
	stats->runs += 1;
	stats->time_total += duration;
	if (stats->time_max < duration) {
		stats->time_max = duration;
	}
}

static inline void dispatch(uloop_event_t event, const void* data, uint32_t size) {
#if ULOP_DATA_QUEUE_SIZE == 0
	(void) data;
	(void) size;
#endif
	const uloop_listener_id_t* ptr = &uloop_listener_table[uloop_listener_lut[event]];
	while (ptr[0] != ULOOP_LISTENER_NONE) {
		uint32_t listener = ptr[0];
		uloop_listener_active = listener;
		ULOOP_TIMER_START();
		ULOOP_HOOK_PRE_EXECUTE(listener, event, data, size);
#if ULOOP_DATA_QUEUE_SIZE > 0
		uloop_listeners[listener](event, data, size);
#else
		uloop_listeners[listener](event);
#endif
		ULOOP_HOOK_POST_EXECUTE(listener, event, data, size);
		uint32_t duration = ULOOP_TIMER_STOP();
#ifdef ULOOP_STATISTICS_ENABLED
		update_listener_stats(&uloop_listener_stats[listener], duration);
#endif
#if ULOOP_LISTENER_TIME_LIMIT > 0
		if (duration > ULOOP_LISTENER_TIME_LIMIT) {
			ULOOP_ERROR_TMO(duration);
		}
#else
	(void) duration;
#endif
		uloop_listener_active = ULOOP_LISTENER_NONE;
		ptr += 1;
	}
}

void uloop_publish(uloop_event_t event) {
	ULOOP_HOOK_PUBLISH(event, NULL, 0);
	ULOOP_ATOMIC_BLOCK_ENTER();
	event_queue_push(event, 0);
	ULOOP_ATOMIC_BLOCK_LEAVE();
}

#if ULOOP_DATA_QUEUE_SIZE > 0
void uloop_publish_ex(uloop_event_t event, const void* data, uint32_t size) {
	ULOOP_HOOK_PUBLISH(event, data, size);
	ULOOP_ATOMIC_BLOCK_ENTER();
	event_queue_push(event, size);
	if (size > 0) {
		ULOOP_DEV_ASSERT(data != NULL);
		uint8_t* ptr = data_queue_push(size);
		ULOOP_ATOMIC_BLOCK_LEAVE();
		if (ptr == NULL) {
			ULOOP_ERROR_DQOVF();
		}
		memcpy(ptr, data, size);
	} else {
		ULOOP_ATOMIC_BLOCK_LEAVE();
	}
}
#endif

bool uloop_run() {
	bool executed;
	if (!event_queue_empty()) {
		uloop_event_queue_item_t event = event_queue_top();
#ifdef ULOOP_STATISTICS_ENABLED
		uloop_event_stats[event.id].count += 1;
#endif
#if ULOOP_DATA_QUEUE_SIZE > 0
		const uint8_t* data = (event.size > 0) ? data_queue_top() : NULL;
		ULOOP_HOOK_PRE_DISPATCH(event.id, data, event.size);
		dispatch(event.id, data, event.size);
		ULOOP_HOOK_POST_DISPATCH(event.id, data, event.size);
		if (event.size > 0) {
			data_queue_pop(event.size);
		}
#else
		ULOOP_HOOK_PRE_DISPATCH(event.id, NULL, 0);
		dispatch(event.id, NULL, 0);
		ULOOP_HOOK_POST_DISPATCH(event.id, NULL, 0);
#endif
		event_queue_pop();
		executed = true;
	} else {
		executed = false;
	}
	return executed;
}

void uloop_init() {
	event_queue.head = 0;
	event_queue.tail = 0;
#if ULOOP_DATA_QUEUE_SIZE > 0
	data_queue.head = 0;
	data_queue.tail = 0;
	data_queue.end = ULOOP_DATA_QUEUE_SIZE;
#endif
	ULOOP_HOOK_INIT();
}

uloop_event_queue_item_t uloop_event_queue_get(uint32_t offset) {
	uloop_event_queue_item_t item = EVENT_QUEUE_ITEM(ULOOP_EVENT_NONE, 0);
	uint32_t size = (event_queue.tail - event_queue.head) & (ULOOP_EVENT_QUEUE_SIZE - 1);
	if (size > offset) {
		item = event_queue.data[(event_queue.head + offset) & (ULOOP_EVENT_QUEUE_SIZE - 1)];
	}
	return item;
}
