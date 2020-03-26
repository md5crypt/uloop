#pragma once
#include "uloop.h"

extern void mock_fail(const char* reason);
extern void mock_fail_tmo(uint32_t duration);
extern void mock_timer_start(void);
extern uint32_t mock_timer_stop(void);
extern void mock_atomic_block_start(void);
extern void mock_atomic_block_stop(void);
extern void mock_dev_assert(void);
extern uint32_t mock_systick(void);

#define ULOOP_SYSTICK()             mock_systick

#define ULOOP_ERROR_EQOVF()         mock_fail("eqOVF");
#define ULOOP_ERROR_DQOVF()         mock_fail("dqOVF");
#define ULOOP_ERROR_DQCORR()        mock_fail("dqCORR");
#define ULOOP_ERROR_TMO(time_us)    mock_fail_tmo(time_us);

#define ULOOP_DEV_ASSERT(cond)     \
	do { \
		if (!(cond)) { \
			mock_dev_assert(); \
		} \
	} while (0)

#define ULOOP_TIMER_START()         mock_timer_start()
#define ULOOP_TIMER_STOP()          mock_timer_stop()
#define ULOOP_ATOMIC_BLOCK_ENTER()  mock_atomic_block_start()
#define ULOOP_ATOMIC_BLOCK_LEAVE()  mock_atomic_block_stop()
