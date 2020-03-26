# uloop - a minimalistic event loop framework for embedded C

This framework aims to provide a bare-bone event loop implementation that focuses on:

* using only statically allocated memory
* computing all necessary data structures at compile-time
* limiting critical sections to an absolute minimum
* using as little cpu time as possible
* giving the user a modern and modular way of defining events and events listeners via JSON configuration files

The framework also includes an optional software timer implementation integrated with the event loop. The timer extension follows the same design philosophies as the event loop itself.

For code generation the framework uses the [ctemplete](https://github.com/md5crypt/ctemplate) tool.

## Configuration

The schema files for the ctemplete tool are located in the `schema` folder. The configuration incudes the following keys:

* `uloop.defines.eventQueueSize` - maximum amount of events that the event queue can hold, each entry requires **2 bytes** of storage, so the total memory required for the queue is `eventQueueSize` * 2 bytes
* `uloop.defines.dataQueueSize` - if set to zero disables event data support. Disabling data queue removes the `size` and `data` from listeners as well as the `uloop_publish_ex` function. When set a value > 0 defines the maximum size of the data queue in bytes.
* `uloop.defines.listenerTimeLimit` - when set to 0 disables the built-in listener execution time checks. When disabled the `ULOOP_TIMER_START()`, `ULOOP_TIMER_STOP()` and `ULOOP_ERROR_TMO()` macros can be undefined. When set to a value > 0 defines the maximum listener execution time in units returned by the `ULOOP_TIMER_STOP()` function (microsecunds are recommended)
* `uloop.defines.metadataNameSize` - the maximum size of metadata event and listener names, only relevant when `metadataEnabled` is set.
* `uloop.defines.metadataEnabled` - emit event and listener metadata data, when enabled `uloop_listener_names` and `uloop_event_names` are created and use (`metadataNameSize` * (event-count + listener-count)) bytes of program memory
* `uloop.defines.statisticsEnabled` - enable statistics, when enabled `uloop_event_stats` and `uloop_listener_stats` are available. This feature uses ((12 * listener-count) + (4 * event-count)) of memory and a small amount of extra cpu time.
* `uloop.prefix` - prefix to append to emitted event names
* `uloop.timer.prefix` - prefix to append to emitted timer names
* `uloop.events` - events definition array (see below)
* `uloop.listeners` - listeners definition array (see below)
* `uloop.timer.timers` - timers definition array (see below)

### Event definitions

A event is defined by an object with two fields:

* `name` - the name to be used in C code for the event. If `uloop.prefix` was defined it will be prepended to this name. Event names have to be unique.
* `metadata` - metadata name of this event. This field is optional. If skipped the framework will attempt to generate this filed from the `name` field.

### Listener definitions

A event listener is defined by an object with the following fields:

* `function` - the listener function to call. The definition of this function will be automatically generated and placed into `uloop_listeners.h`
* `metadata` - metadata name of this listener. This field is optional. If skipped the framework will attempt to generate this this filed from the `function` field.
* `events` - an array of event names this listener listens on. The event names must match the names defined in the event definition array.

### Timer definitions

A timer is defined by an object with the following fields:

* `name` - the name to be used in C code for the timer. If `uloop.timer.prefix` was defined it will be prepended to this name. Timer names have to be unique.
* `event` - event to be emitted by this timer. The value must be a name of one of the events defined in the event definition array.

> Note: the uloop_timer extension to work properly requires a listener and a event to be defined in the configuration. The event can have any name and needs to be passed to the `uloop_timer_update` function. The listener needs to listen for that event and have it's function set to `uloop_timer_listener`

### Example configuration

```javascript
({
	"uloop.defines": {
		eventQueueSize: 32,
		dataQueueSize: 1024,
		listenerTimeLimit: 1000,
		metadataNameSize: 12,
		metadataEnabled: true,
		statisticsEnabled: true
	},
	"uloop.prefix": "E_",
	"uloop.timer.prefix": "TIMER_",
	"uloop.events": [
		/* event for uloop_timer extension */
		{name: "ULOOP_TIMER_UPDATE"},
		/* user event */
		{name: "CRANKSHAFT_CRANK", metadata: "cs-crank"},
		/* event that will be emitted by a timer */
		{name: "CRANKSHAFT_TIMEOUT"}
	],
	"uloop.listeners": [
		/* listener for uloop_timer extension */
		{
			function: "uloop_timer_listener",
			events: ["ULOOP_TIMER_UPDATE"]
		},
		/* user listener */
		{
			function: "crankshaft_listener",
			events: [
				"CRANKSHAFT_CRANK",
				"CRANKSHAFT_TIMEOUT"
			]
		}
	],
	"uloop.timer.timers": [
		/* in C code the timer's name will be TIMER_CRANKSHAFT */
		{name: "CRANKSHAFT", event: "CRANKSHAFT_TIMEOUT"}
	]
})
```

## Limitations

* Max event count:
  * 256 (when data queue enabled)
  * 65536 (when data queue disabled)
* Max listener count: 256
* Max events per listener: no limit
* Max timer count: no limit


## Event and data queues

The event loop uses two static queues: a event queue and a data queue. Access to both queues is guarded by critical sections, if fact those are the only critical sections in the whole implementation.

### Event queue

The event queue is an cyclic list of event entries. The structure of the event entry depends on the `dataQueueSize` value.
* if set to zero, a event entry is simply a `uint16_t` id wrapped in a structure.
* otherwise it is a pair of two bytes: the event id and data size

In both cases the event queue consumes the same amount of memory. The size of the queue is static and defined at compile time by the `eventQueueSize` configuration value.

The queued events can be accessed using the `uloop_event_queue_get` function. This allows to print debug information about past and pending events after an error. See this function description for more details.

### Data queue

When enabled, the data queue provides a mechanism for attaching data to events. Event data is **copied onto an internal structure** and **deleted once the event is processed**.

The structure itself is static and it's size is defined by `dataQueueSize`. The exact amount of data that can be stored on this structure is in theory equal to its size but has several limitations:

* data stored on the data queue is **always aligned**. This allows to directly access structures stored in event data without fear of dealing with unaligned access but also uses extra space in many cases
* data on the queue is required to be stored in a continues block of memory. The exact consequences of this limitations are explained below

The data queue is implemented as a static cyclic list of variable length elements. This introduces problems when a element does not fit on the end of the list. Because the element needs to stored on a continues block of memory the remaining space on the end of the list will be wasted. Consider the following example:

`dataQueueSize` is 128 bytes

| Operation | Queue content |
| :----: | :---- |
| init | `T, H \| (128)` |
| push 32B | `T \| 32 \| H \| (96)` |
| push 64B | `T \| 32 \| 64 \| H \| (32)` |
| push 4B | `T \| 32 \| 64 \| 4 \| H \| (28)` |
| pop | `(32) \| T \| 64 \| 4 \| H \| (28)` |
| push 32B | `32 \| T, H \| 64 \| 4 \| (28)` |

In this example 28 bytes on the end of the queue were wasted as the element was too big to fit in the continues block.

Although this behavior can introduce unwanted and hard to predict edge cases, limiting the maximum amount of data that can be pushed to the queue (for example using the `ULOOP_HOOK_PUBLISH` hook) greatly simplifies the analyse.

## Listener lookup tables

Three constant lookup tables are generated during compilation time:

* `uloop_listeners` - array binding listener IDs with functions, consumes (4 * listener-count) of program memory. This extra level of indirection decreases the size of `uloop_listener_table` by a factor of 4.
* `uloop_listener_table` - a list of lists of listener IDs bound with each event. Lists are divided by a separator element (`ULOOP_LISTENER_NONE`). As listener IDs are 1B and the table consumes (sum(listener-events) + event-count) bytes of program memory.
* `uloop_listener_lut` - a table binding event IDs with offsets in `uloop_listener_table`, consumes event-count bytes of program memory if size of uloop_listener_table is below 256 and (2 * event-count) otherwise.

Simplified processing of an event `e` can be described as:

```c
int offset = uloop_listener_lut[e];
while (uloop_listener_table[offset] != ULOOP_LISTENER_NONE) {
	uloop_listeners[uloop_listener_table[offset]](e);
	offset += 1;
}
```

## Platform configuration

For the event loop to work a `uloop_platform.h` header must be provided. The `uloop_platform.example.h` file can be used as a starting point.

The following definitions must be provided in that file:

* `ULOOP_ERROR_EQOVF()` - macro called upon a event queue overflow
* `ULOOP_ERROR_DQOVF()` - macro called upon a data queue overflow
* `ULOOP_ERROR_DQCORR()` - macro called when data queue corruption is detected
* `ULOOP_ERROR_TMO(time)` - macro called when a listener exceed the it's time limit, can skipped if `listenerTimeLimit` is zero
* `ULOOP_DEV_ASSERT(cond)` - an assert macro used for non critical runtime sanity checks, should be a no-operation for release builds to not impact performance
* `ULOOP_ATOMIC_BLOCK_ENTER()` - macro for entering a critical section, the critical sections are never nested and always in the same scope (this means that this macro can create a local variable that will be visible in `ULOOP_ATOMIC_BLOCK_LEAVE`)
* `ULOOP_ATOMIC_BLOCK_LEAVE()` - macro for leaving a critical section, the critical sections are never nested and always in the same scope
* `ULOOP_TIMER_START()` - macro for starting time measurement. This macro can create a local variable that will be visible in `ULOOP_TIMER_STOP` as both are called from the same scope. This macro can be skipped if `listenerTimeLimit` is set to zero.
* `ULOOP_TIMER_STOP()` - macro for stopping time measurement, should return a `uint32_t` value with time elapsed from calling `ULOOP_TIMER_START`. This macro can be skipped if `listenerTimeLimit` is set to zero.
* `ULOOP_SYSTICK()` - macro for returning a `uint32_t` with current time in milliseconds, used by `uloop_timer`, can be skipped if that module is not included in compilation

### Hooks

For additional fine-tunning the following **optional** macros can be defined in `uloop_platform.h`. Each macro is called at a different stage of event processing and by default does nothing. Below is a list of available macros:

* `ULOOP_HOOK_INIT()`
* `ULOOP_HOOK_PUBLISH(event, data, size)`
* `ULOOP_HOOK_PRE_DISPATCH(event, data, size)`
* `ULOOP_HOOK_POST_DISPATCH(event, data, size)`
* `ULOOP_HOOK_PRE_EXECUTE(listener, event, data, size)`
* `ULOOP_HOOK_POST_EXECUTE(listener, event, data, size)`

For more information about these macros consult the soruce code

## Timer extension

The timer extension add a possibility to emit events at precise timings using as little cpu time inside as possible.

A single inline function is provided that should be called from inside a systick interrupt handler. This function computes a single integer comparison to decide if it should emit a timer update event. That event triggers the internal uloop timer listener to process all user defined timers outside of the interrupt space.

The trigger event is emitted only when one or more timers have expired as that single integer comparison made inside the interrupt compares current time with the minimum of all current expire times. Stopping and starting timers is O(1).

Four bytes of memory are consumed per timer, so this module uses in total (4 * timer-count) bytes of runtime memory.

> Note: The timer trigger event and the internal event listener must be added to the uloop configuration. See the configuration section for more details.

## Statistics

When enabled, two global variables with statistics are available for user access

Listener statistics are stored in the `uloop_listener_stats` structure with the following fields:

* `runs` - amount of times this listener was executed
* `time_total` - total time spent in this listener
* `time_max` - maximum execution time of this listener

Event statistics are stored in the `uloop_event_stats` structure with the following fields:

* `count` - amount of times this event was published

## Listener functions

Listener functions can be places in any application source file that includes `uloop_listeners.h`

If event data queue is **enabled** listeners should have the following definitions:

```c
void example_listener(uloop_event_t event, const void* data, uint32_t size)
```

If event queue is **disabled** listeners should have the following definitions:

```c
void example_listener(uloop_event_t event)
```


## Public API

### Uloop functions

The following functions are defined in `uloop.h`

#### `void uloop_init(void)`

Initialize internal data structures, must be called before any other uloop function is used

#### `void uloop_publish(uloop_event_t event)`

Publish an event without any data

> Note: all event ID are defined in the generated `uloop_config.h` file

#### `void uloop_publish_ex(uloop_event_t event, const void* data, uint32_t size)`

Publish an event with optional data. The data will be copied onto the internal data queue and deleted once the event is processed. This function is not available if `dataQueueSize` set to zero.

> Note: all event ID are defined in the generated `uloop_config.h` file

#### `bool uloop_run(void)`

Process a single event from the event queue. Returns `false` if the queue was empty. Should be called from the application main loop.

#### `uloop_event_queue_item_t uloop_event_queue_get(uint32_t offset)`

Function providing raw access to the event queue, `offset` is relative to the current event. Should be used only for generating error messages after a failure.

### Uloop global variables

* `uloop_listener_active` - currently running listener or `ULOOP_LISTENER_NONE`, should be **NEVER** written and read only for error reporting purposes
* `uloop_listener_names` - listener metadata, note that strings inside can fill the entire char table without including a null terminator.
* `uloop_event_names` - event metadata, note that strings inside can fill the entire char table without including a null terminator.
* `uloop_event_stats` - event statistics, see statistics section
* `uloop_listener_stats` - listener statistics, see statistics section

### Uloop timer functions

The following functions are defined in `uloop_timer.h`

#### `void uloop_timer_init(uint32_t systick)`

Initialize internal data structures, must be called before any other uloop timer function is used. The argument should be the current systick value.

#### `void uloop_timer_start(uloop_timer_t timer, uint32_t value)`

Set a timer to expire in `value` systick time units. If the timer was already running the expire time is overwritten.

> Note: all timer ID are defined in the generated `uloop_timer_config.h` file

> Note: this function is a wrapper for `uloop_timer_start_ex` with `relative` set to true

#### `void uloop_timer_start_ex(uloop_timer_t timer, uint32_t value, bool relative)`

* when `relative` is `false` set a timer to expire at the given `value` systick value
* when `relative` is `true` set a timer to expire in `value` systick time units

If the timer was already running the expire time is overwritten.

> Note: all timer ID are defined in the generated `uloop_timer_config.h` file

#### `void uloop_timer_stop(uloop_timer_t timer)`

Stop a running timer. If the timer was already stopped nothing will happen.

> Note: all timer ID are defined in the generated `uloop_timer_config.h` file

#### `bool uloop_timer_running(uloop_timer_t timer)`

Returns `true` if a timer is running.

> Note: all timer ID are defined in the generated `uloop_timer_config.h` file

#### `void uloop_timer_update(uint32_t systick, uloop_event_t event)`

This function should be called from the systick interrupt handler. The `systick` argument should be the current systick value and `event` the uloop timer trigger event id.

## Setting up a project

1. download the [ctemplete](https://github.com/md5crypt/ctemplate/releases) tool
2. write the ctemplate configuration file
3. add Makefile rules for processing `*.c.template` / `*.h.template` files into `*.c` / `*.h` files
4. add `uloop.c` and `uloop_config.c` to the project
5. add `uloop_timer.c` and `uloop_timer_config.c` to the project (if timers are to be used)
6. create `uloop_platform.h`
7. call `uloop_init` from main
8. call `uloop_timer_init` from main (if timers are to be used)
9. add `uloop_timer_update` to be called form the systick interrupt (if timers are to be used)
10. add `uloop_run` to the application main loop

## Unit tests

Some basic units tests are in the `utest` folder, they require cmake and cpputest to build
