
#include <stdexcept>
#include <queue>
#include <stdint.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "uloop.h"
#include "uloop_config.h"

static void mock_listener(uloop_event_t id, const void* data, uint32_t size);

const uloop_listener_t uloop_listeners[1] = {mock_listener};

const uloop_listener_id_t uloop_listener_table[2] = {0, ULOOP_LISTENER_NONE};
const uint16_t uloop_listener_lut[256] = {0};

uint64_t xorshift64s(uint64_t* state) {
	uint64_t x = state[0];
	x ^= x >> 12; // a
	x ^= x << 25; // b
	x ^= x >> 27; // c
	state[0] = x;
	return x * 0x2545F4914F6CDD1DLL;
}

static void mock_listener(uloop_event_t event, const void* data, uint32_t size) {
	mock().actualCall(__FUNCTION__)
		.withParameter("event", event)
		.withMemoryBufferParameter("data", (const uint8_t*)data, size);
}

void mock_fail(const char* reason) {
	mock().actualCall(__FUNCTION__)
		.withStringParameter("reason", reason);
	throw std::exception();
}

void mock_fail_tmo(uint32_t duration) {
	mock().actualCall(__FUNCTION__)
		.withParameter("duration", duration);
	throw std::exception();
}

void mock_timer_start() {
	mock().actualCall(__FUNCTION__);
}

uint32_t mock_timer_stop() {
	return mock().actualCall(__FUNCTION__).returnUnsignedIntValue();
}

void mock_atomic_block_start() {
	mock().actualCall(__FUNCTION__);
}

void mock_atomic_block_stop() {
	mock().actualCall(__FUNCTION__);
}

void mock_dev_assert() {
	mock().actualCall(__FUNCTION__);
	throw std::exception();
}

TEST_GROUP(uloop)
{
	void setup() {
		uloop_init();
	}
	void teardown() {
		mock().clear();
	}
};

void push_event(uloop_event_t event, const uint8_t* data = NULL, uint32_t size = 0) {
	mock().strictOrder();
	mock().expectOneCall("mock_atomic_block_start");
	mock().expectOneCall("mock_atomic_block_stop");
	uloop_publish_ex(event, data, size);
	mock().checkExpectations();
	mock().clear();
}

void expect_event(uloop_event_t event, const uint8_t* data = NULL, uint32_t size = 0) {
	mock().strictOrder();
	mock().expectOneCall("mock_timer_start");
	mock().expectOneCall("mock_listener")
		.withParameter("event", event)
		.withMemoryBufferParameter("data", data, size);
	mock().expectOneCall("mock_timer_stop");
	if (size > 0) {
		mock().expectOneCall("mock_atomic_block_start");
		mock().expectOneCall("mock_atomic_block_stop");
	}
	CHECK_TRUE(uloop_run());
	mock().checkExpectations();
	mock().clear();
}

TEST(uloop, event_queue_fill) {
	for (uint32_t i = 0; i < (ULOOP_EVENT_QUEUE_SIZE - 1); i++) {
		push_event(i);
	}
	for (uint32_t i = 0; i < (ULOOP_EVENT_QUEUE_SIZE - 1); i++) {
		expect_event(i);
	}
	CHECK_FALSE(uloop_run());
}

TEST(uloop, event_queue_overflow) {
	for (uint32_t i = 0; i < (ULOOP_EVENT_QUEUE_SIZE - 1); i++) {
		push_event(i);
	}
	mock().expectOneCall("mock_fail")
		.withParameter("reason", "eqOVF");
	CHECK_THROWS(std::exception, push_event(ULOOP_EVENT_QUEUE_SIZE));
}

TEST(uloop, event_queue_empty) {
	CHECK_FALSE(uloop_run());
}

TEST(uloop, event_queue_rand) {
	uint64_t seed = 1;
	uint32_t events = 0;
	uint8_t head = 0;
	uint8_t tail = 0;
	for (uint32_t n = 0; n < 128; n++) {
		uint32_t push_count = xorshift64s(&seed) % (ULOOP_EVENT_QUEUE_SIZE - events);
		for (uint32_t i = 0; i < push_count; i++) {
			push_event(tail);
			tail += 1;
			events += 1;
		}
		uint32_t pop_count = xorshift64s(&seed) % (events + 1);
		for (uint32_t i = 0; i < pop_count; i++) {
			expect_event(head);
			head += 1;
			events -= 1;
		}
	}
	while(events > 0) {
		expect_event(head);
		head += 1;
		events -= 1;
	}
	CHECK_FALSE(uloop_run());
}

TEST(uloop, data_queue_rand) {
	std::queue<uint32_t> sizes;
	uint64_t seed = 1;
	uint32_t events = 0;
	uint32_t bytes = 0;
	uint8_t head = 0;
	uint8_t tail = 0;
	uint8_t data[64];
	for (uint32_t n = 0; n < 128; n++) {
		uint32_t push_count = xorshift64s(&seed) % (ULOOP_EVENT_QUEUE_SIZE - events);
		for (uint32_t i = 0; i < push_count; i++) {
			uint32_t bytes_left = ((ULOOP_DATA_QUEUE_SIZE - (2 *(sizeof(data) - 4))) - bytes);
			if (bytes_left > sizeof(data)) {
				bytes_left = sizeof(data);
			}
			if (bytes_left == 0) {
				break;
			}
			uint32_t size = xorshift64s(&seed) % (bytes_left + 1);
			memset(data, tail, size);
			bytes += (size + 3) & ~3;
			push_event(tail, data, size);
			sizes.push(size);
			tail += 1;
			events += 1;
		}
		uint32_t pop_count = xorshift64s(&seed) % (events + 1);
		for (uint32_t i = 0; i < pop_count; i++) {
			memset(data, head, sizes.front());
			expect_event(head, data, sizes.front());
			bytes -= (sizes.front() + 3) & ~3;
			sizes.pop();
			head += 1;
			events -= 1;
		}
	}
	while(events > 0) {
		memset(data, head, sizes.front());
		expect_event(head, data, sizes.front());
		sizes.pop();
		head += 1;
		events -= 1;
	}
	CHECK_FALSE(uloop_run());
}

TEST(uloop, data_queue_overflow1) {
	uint8_t data[256] = {0};
	uint32_t size = ULOOP_DATA_QUEUE_SIZE - 4;
	while (size >= 252) {
		push_event(0, data, 252);
		size -= 252;
	}
	push_event(0, data, size);
	mock().expectOneCall("mock_atomic_block_start");
	mock().expectOneCall("mock_atomic_block_stop");
	mock().expectOneCall("mock_fail").withParameter("reason", "dqOVF");
	CHECK_THROWS(std::exception, push_event(0, data, 1));
}

TEST(uloop, data_queue_overflow2) {
	uint8_t data[256] = {0};
	push_event(0, data, 60);
	uint32_t size = ULOOP_DATA_QUEUE_SIZE - 120;
	while (size >= 252) {
		push_event(0, data, 252);
		size -= 252;
	}
	push_event(0, data, size);
	expect_event(0, data, 60);
	mock().expectOneCall("mock_atomic_block_start");
	mock().expectOneCall("mock_atomic_block_stop");
	mock().expectOneCall("mock_fail").withParameter("reason", "dqOVF");
	CHECK_THROWS(std::exception, push_event(0, data, 64));
}

TEST(uloop, data_queue_overflow3) {
	uint8_t data[256] = {0};
	uint32_t size = ULOOP_DATA_QUEUE_SIZE - 4;
	while (size >= 64) {
		push_event(0, data, 64);
		size -= 64;
	}
	push_event(0, data, size);
	expect_event(0, data, 64);
	expect_event(0, data, 64);
	push_event(0, data, 64);
	mock().expectOneCall("mock_atomic_block_start");
	mock().expectOneCall("mock_atomic_block_stop");
	mock().expectOneCall("mock_fail").withParameter("reason", "dqOVF");
	CHECK_THROWS(std::exception, push_event(0, data, 64));
}

int main(int ac, char** av) {
	return CommandLineTestRunner::RunAllTests(ac, av);
}