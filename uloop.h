// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "uloop_config.h"

#define ULOOP_LISTENER_NONE  ((uloop_listener_id_t) -1)
#define ULOOP_EVENT_NONE     ((uloop_event_t) -1)

#if ULOOP_LISTENER_TABLE_SIZE < 256
typedef uint8_t uloop_listener_lut_t;
#define ULOOP_LISTENER_LUT_TYPE uint8_t
#else
typedef uint16_t uloop_listener_lut_t;
#endif

#if ULOOP_DATA_QUEUE_SIZE > 0
typedef uint8_t uloop_event_t;
#else
typedef uint16_t uloop_event_t;
#endif

typedef uint8_t uloop_listener_id_t;

#if ULOOP_DATA_QUEUE_SIZE > 0
typedef void (*uloop_listener_t)(uloop_event_t event, const void* data, uint32_t size);
#else
typedef void (*uloop_listener_t)(uloop_event_t event);
#endif

#ifdef ULOOP_METADATA_ENABLED
typedef struct {
	char name[ULOOP_METADATA_NAME_SIZE];
} uloop_name_t;
#endif

typedef struct {
	uint32_t runs;
	uint32_t time_total;
	uint32_t time_max;
} uloop_listenter_stats_t;

typedef struct {
	uint32_t count;
} uloop_event_stats_t;

typedef struct {
	uloop_event_t id;
#if ULOOP_DATA_QUEUE_SIZE > 0
	uint8_t size;
#endif
} uloop_event_queue_item_t;

extern const uloop_listener_t uloop_listeners[ULOOP_LISTENER_COUNT];

#ifdef ULOOP_METADATA_ENABLED
extern const uloop_name_t uloop_listener_names[ULOOP_LISTENER_COUNT];
extern const uloop_name_t uloop_event_names[ULOOP_EVENT_COUNT];
#endif

#ifdef ULOOP_STATISTICS_ENABLED
extern uloop_event_stats_t uloop_event_stats[ULOOP_EVENT_COUNT];
extern uloop_listenter_stats_t uloop_listener_stats[ULOOP_LISTENER_COUNT];
#endif

extern const uloop_listener_id_t uloop_listener_table[ULOOP_LISTENER_TABLE_SIZE];
extern const uloop_listener_lut_t uloop_listener_lut[ULOOP_EVENT_COUNT];

extern uloop_listener_id_t uloop_listener_active;

bool uloop_run(void);
void uloop_publish(uloop_event_t event);
void uloop_init(void);

#if ULOOP_DATA_QUEUE_SIZE > 0
void uloop_publish_ex(uloop_event_t event, const void* data, uint32_t size);
#endif

uloop_event_queue_item_t uloop_event_queue_get(uint32_t offset);
