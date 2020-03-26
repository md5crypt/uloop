#pragma once
#include "uloop.h"

/* optional hooks */
// #define ULOOP_HOOK_INIT()
// #define ULOOP_HOOK_PUBLISH(event, data, size)
// #define ULOOP_HOOK_PRE_DISPATCH(event, data, size)
// #define ULOOP_HOOK_POST_DISPATCH(event, data, size)
// #define ULOOP_HOOK_PRE_EXECUTE(listener, event, data, size)
// #define ULOOP_HOOK_POST_EXECUTE(listener, event, data, size)

/* error reporting */
#define ULOOP_ERROR_EQOVF()        do {} while (1)
#define ULOOP_ERROR_DQOVF()        do {} while (1)
#define ULOOP_ERROR_DQCORR()       do {} while (1)
#define ULOOP_ERROR_TMO(time_us)   do {} while (1)

/* development assert macro */
#define ULOOP_DEV_ASSERT(cond)     do { if (cond) { while (1) {} } } while (0)

/* atomic block macros */
#define ULOOP_ATOMIC_BLOCK_ENTER() do {} while (0)
#define ULOOP_ATOMIC_BLOCK_LEAVE() do {} while (0)

/* microsecond timer macros */
#define ULOOP_TIMER_START()        do {} while (0)
#define ULOOP_TIMER_STOP()         0 /* should return us from start to stop */

/* systick access macro */
#define ULOOP_SYSTICK()            0 /* should return 32-bit ms from system init */
