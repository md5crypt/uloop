#pragma once
#include "uloop.h"

extern void mock_dev_assert(void);
extern uint32_t mock_systick(void);

#define ULOOP_DEV_ASSERT(cond)     \
	do { \
		if (!(cond)) { \
			mock_dev_assert(); \
		} \
	} while (0)

#define ULOOP_SYSTICK()            mock_systick()
#define ULOOP_TIMER_COUNT          8
