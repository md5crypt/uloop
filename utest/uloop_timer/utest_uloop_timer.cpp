
#include <stdexcept>
#include <queue>
#include <stdint.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "uloop_timer.h"
#include "uloop_config.h"
#include "uloop_listeners.h"

#define TIM0 (1 << 0)
#define TIM1 (1 << 1)
#define TIM2 (1 << 2)
#define TIM3 (1 << 3)
#define TIM4 (1 << 4)
#define TIM5 (1 << 5)
#define TIM6 (1 << 6)
#define TIM7 (1 << 7)
#define TIM_NONE (1 << 8)

uint64_t xorshift64s(uint64_t* state) {
	uint64_t x = state[0];
	x ^= x >> 12; // a
	x ^= x << 25; // b
	x ^= x >> 27; // c
	state[0] = x;
	return x * 0x2545F4914F6CDD1DLL;
}

const uloop_event_t uloop_timer_events[] = {10, 11, 12, 13, 14, 15, 16, 17};

void mock_dev_assert() {
	mock().actualCall(__FUNCTION__);
	throw std::exception();
}

uint32_t mock_systick() {
	return mock().actualCall(__FUNCTION__).returnUnsignedIntValue();
}

void uloop_publish(uloop_event_t event) {
	mock().actualCall(__FUNCTION__).withParameter("event", event);
}

TEST_GROUP(uloop_timer)
{
	void setup() {
		uloop_timer_init(0);
	}
	void teardown() {
		mock().clear();
	}
};

void update(uint32_t systick, uint32_t bitmask) {
	mock().expectOneCall("mock_systick").andReturnValue(systick);
	for(uint32_t i = 0; i < 8; i++) {
		if (bitmask & (1 << i)) {
			mock().expectOneCall("uloop_publish").withParameter("event", i + 10);
		}
	}
	uloop_timer_listener(E_ULOOP_TIMER_UPDATE, NULL, 0);
	mock().checkExpectations();
}

void run_for(uint32_t& systick, uint32_t value, uint32_t bitmask = 0, bool exact = false) {
	if (!exact) {
		uint32_t step = value / 16;
		if (step > 0) {
			for (uint32_t i = 0; i < value - 2; i += step) {
				uloop_timer_update(systick + i);
			}
			uloop_timer_update(systick + value - 2);
			uloop_timer_update(systick + value - 1);
		} else if (value > 0) {
			for (uint32_t i = 0; i < value - 1; i++) {
				uloop_timer_update(systick + i);
			}
		}
		mock().checkExpectations();
	}
	if (bitmask) {
		mock().expectOneCall("uloop_publish").withParameter("event", E_ULOOP_TIMER_UPDATE);
	}
	uloop_timer_update(systick + value);
	mock().checkExpectations();
	systick += value;
	if (bitmask) {
		update(systick, bitmask);
	}
}

void start(uint32_t systick, uloop_timer_t timer, uint32_t value, bool realtive = true) {
	mock().expectOneCall("mock_systick").andReturnValue(systick);
	uloop_timer_start_ex(timer, value, realtive);
	mock().checkExpectations();
}

void stop(uint32_t systick, uloop_timer_t timer) {
	mock().expectOneCall("mock_systick").andReturnValue(systick);
	uloop_timer_stop(timer);
	mock().checkExpectations();
}

void running(uint32_t systick, uloop_timer_t timer, bool val) {
	mock().expectOneCall("mock_systick").andReturnValue(systick);
	CHECK_EQUAL(uloop_timer_running(timer), val);
	mock().checkExpectations();
}

TEST(uloop_timer, basic_test) {
	uint32_t systick = 0;
	run_for(systick, 100);
	start(systick, 0, 20);
	run_for(systick, 20, TIM0);
	run_for(systick, 100);
}

TEST(uloop_timer, basic_test2) {
	uint32_t systick = 0xFFFFFFF0;
	uloop_timer_init(systick);
	run_for(systick, 100);
	start(systick, 0, 20);
	run_for(systick, 20, TIM0);
	run_for(systick, 100);
}

TEST(uloop_timer, basic_test3) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	run_for(systick, 100);
	start(systick, 0, 20);
	run_for(systick, 10);
	start(systick, 1, 500);
	run_for(systick, 10, TIM0);
	run_for(systick, 490, TIM1);
	run_for(systick, 100);
}

TEST(uloop_timer, basic_test4) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	run_for(systick, 100);
	start(systick, 0, 20);
	run_for(systick, 10);
	start(systick, 1, 10);
	run_for(systick, 10, TIM0 | TIM1);
	run_for(systick, 100);
}

TEST(uloop_timer, basic_longrun) {
	uint32_t systick = 0;
	uloop_timer_init(systick);
	for (uint32_t i = 0; i < 32; i++) {
		run_for(systick, ULOOP_TIMER_UPDATE_PERIOD, TIM_NONE);
	}
}

TEST(uloop_timer, basic_longrun2) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	for (uint32_t i = 0; i < 32; i++) {
		run_for(systick, ULOOP_TIMER_UPDATE_PERIOD, TIM_NONE);
	}
}

TEST(uloop_timer, basic_longrun3) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	for (uint32_t i = 0; i < 32; i++) {
		run_for(systick, ULOOP_TIMER_UPDATE_PERIOD + 1024, TIM_NONE, true);
	}
}

TEST(uloop_timer, basic_jitter) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	run_for(systick, 1000);
	start(systick, 0, 200);
	run_for(systick, 100);
	start(systick, 1, 400);
	run_for(systick, 300, TIM0, true);
	run_for(systick, 800, TIM1, true);
	run_for(systick, ULOOP_TIMER_UPDATE_PERIOD, TIM_NONE);
}

TEST(uloop_timer, max_period) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	start(systick, 0, ULOOP_TIMER_MAX_PERIOD);
	uint32_t n = ULOOP_TIMER_UPDATE_PERIOD;
	while (n < ULOOP_TIMER_MAX_PERIOD) {
		run_for(systick, ULOOP_TIMER_UPDATE_PERIOD, TIM_NONE);
		n += ULOOP_TIMER_UPDATE_PERIOD;
	}
	run_for(systick, ULOOP_TIMER_MAX_PERIOD - (n - ULOOP_TIMER_UPDATE_PERIOD), TIM0);
	run_for(systick, ULOOP_TIMER_UPDATE_PERIOD, TIM_NONE);
}

TEST(uloop_timer, max_period_jitter) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	start(systick, 0, ULOOP_TIMER_MAX_PERIOD);
	uint32_t n = ULOOP_TIMER_UPDATE_PERIOD;
	while (n < ULOOP_TIMER_MAX_PERIOD) {
		run_for(systick, ULOOP_TIMER_UPDATE_PERIOD, TIM_NONE);
		n += ULOOP_TIMER_UPDATE_PERIOD;
	}
	run_for(systick, ULOOP_TIMER_MAX_PERIOD - (n - ULOOP_TIMER_UPDATE_PERIOD) + 1024, TIM0, true);
	run_for(systick, ULOOP_TIMER_UPDATE_PERIOD, TIM_NONE);
}

TEST(uloop_timer, zero_period_run) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	start(systick, 0, 0);
	run_for(systick, 0, TIM0);
}

TEST(uloop_timer, all_at_once) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	for (uint32_t i = 0; i < 8; i++) {
		start(systick, i, 0);
	}
	run_for(systick, 0, 0xFF);
}

TEST(uloop_timer, stop_simple) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	running(systick, 0, false);
	start(systick, 0, 100);
	running(systick, 0, true);
	run_for(systick, 50);
	running(systick, 0, true);
	stop(systick, 0);
	running(systick, 0, false);
	run_for(systick, 50, TIM_NONE);
	running(systick, 0, false);
	for (uint32_t i = 0; i < 8; i++) {
		run_for(systick, ULOOP_TIMER_UPDATE_PERIOD + 1024, TIM_NONE, true);
		running(systick, 0, false);
	}
	start(systick, 0, 100);
	running(systick, 0, true);
	run_for(systick, 100, TIM0);
	running(systick, 0, false);
}

TEST(uloop_timer, start_absolute) {
	uint32_t systick = 0xFFFFFF00;
	uloop_timer_init(systick);
	start(systick, 0, 0, false);
	run_for(systick, 0x100, TIM0);
}

int main(int ac, char** av) {
	return CommandLineTestRunner::RunAllTests(ac, av);
}