/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/util.h>

#include <app_event_manager.h>
#include <caf/events/button_event.h>
#include "tilt_event.h"
#include "hid_event.h"

#define MODULE tilt
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_TILT_LOG_LEVEL);

#ifdef CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER
	typedef uint64_t tilt_ts;
#else
	typedef uint32_t tilt_ts;
#endif

enum state {
	STATE_IDLE,
	STATE_CONNECTED,
	STATE_PENDING
};

enum dir {
	DIR_LEFT,
	DIR_RIGHT,

	DIR_COUNT
};

BUILD_ASSERT(DIR_COUNT <= 8);
static uint8_t active_dir_bm;

static tilt_ts timestamp[DIR_COUNT];
static int32_t tilt_remainder;

static enum state state;


static tilt_ts get_timestamp(void)
{
	/* Use higher resolution if available. */
	if (IS_ENABLED(CONFIG_TIMER_HAS_64BIT_CYCLE_COUNTER)) {
		return k_cycle_get_64();
	} else {
		return k_cycle_get_32();
	}
}

static void tilt_event_send(int8_t tilt)
{
	struct tilt_event *event = new_tilt_event();

	event->tilt = tilt;

	APP_EVENT_SUBMIT(event);
}

static enum dir key_to_dir(uint16_t key_id)
{
	enum dir dir = DIR_COUNT;
	switch (key_id) {

	case CONFIG_DESKTOP_TILT_BUTTONS_LEFT_KEY_ID:
		dir = DIR_LEFT;
		break;

	case CONFIG_DESKTOP_TILT_BUTTONS_RIGHT_KEY_ID:
		dir = DIR_RIGHT;
		break;

	default:
		break;
	}

	return dir;
}

static int16_t ts_diff_to_tilt(int64_t diff, int32_t *reminder)
{
	int64_t res = diff * CONFIG_DESKTOP_TILT_BUTTONS_TILT_PER_SEC + *reminder;

	*reminder = res % sys_clock_hw_cycles_per_sec();
	res /= sys_clock_hw_cycles_per_sec();

	return CLAMP(res, INT8_MIN, INT8_MAX);
}

static int64_t tilt_to_ts_diff(int64_t m)
{
	return m * sys_clock_hw_cycles_per_sec() / CONFIG_DESKTOP_TILT_BUTTONS_TILT_PER_SEC;
}

static int64_t update_tilt_ts(enum dir dir, tilt_ts ts)
{
	if (!(active_dir_bm & BIT(dir))) {
		return 0;
	}

	int64_t ts_diff = ts - timestamp[dir];

	timestamp[dir] = ts;

	return ts_diff;
}

static void generate_tilt(int8_t *tilt)
{
	tilt_ts ts = get_timestamp();

	int64_t left = update_tilt_ts(DIR_LEFT, ts);
	int64_t right = update_tilt_ts(DIR_RIGHT, ts);

	*tilt = ts_diff_to_tilt(right - left, &tilt_remainder);

	LOG_INF("generate_tilt");
}

static void clear_accumulated_tilt(void)
{
	int8_t tilt;

	generate_tilt(&tilt);
	tilt_remainder = 0;
}

static void send_tilt(void)
{
	int8_t tilt;
    LOG_INF("send_tilt");
	generate_tilt(&tilt);
	tilt_event_send(tilt);
}

static bool handle_button_event(const struct button_event *event)
{
	enum dir dir = key_to_dir(event->key_id);

	if (dir == DIR_COUNT) {
		/* Unhandled button. */
		return false;
	}

	WRITE_BIT(active_dir_bm, dir, event->pressed);

	if (event->pressed) {
		/* Ensure tilt is generated right after button press. */
		tilt_ts ts_shift = tilt_to_ts_diff(1) + 1;

		update_tilt_ts(dir, get_timestamp() - ts_shift);

		if (state == STATE_CONNECTED) {
			send_tilt();
			state = STATE_PENDING;
		}
	}
	else{
		WRITE_BIT(active_dir_bm, dir, event->pressed);
	}

	return false;
}

static bool handle_module_state_event(const struct module_state_event *event)
{
	/* Replicate the state of buttons module */
	if (event->module_id == MODULE_ID(buttons)) {
		module_set_state(event->state);
	}

	return false;
}

static bool is_tilt_active(void)
{
	return active_dir_bm != 0;
}

static bool handle_hid_report_sent_event(const struct hid_report_sent_event *event)
{
	if ((event->report_id == REPORT_ID_MOUSE) ||
	    (event->report_id == REPORT_ID_BOOT_MOUSE)) {
		if (state == STATE_PENDING) {
			if (is_tilt_active()) {
				send_tilt();
			} else {
				state = STATE_CONNECTED;
			}
		}
	}

	return false;
}

static bool handle_hid_report_subscription_event(const struct hid_report_subscription_event *event)
{
	if ((event->report_id == REPORT_ID_MOUSE) ||
	    (event->report_id == REPORT_ID_BOOT_MOUSE)) {
		static uint8_t peer_count;

		if (event->enabled) {
			__ASSERT_NO_MSG(peer_count < UCHAR_MAX);
			peer_count++;
		} else {
			__ASSERT_NO_MSG(peer_count > 0);
			peer_count--;
		}

		bool is_connected = (peer_count != 0);

		if ((state == STATE_IDLE) && is_connected) {
			if (is_tilt_active()) {
				clear_accumulated_tilt();
				send_tilt();
				state = STATE_PENDING;
			} else {
				state = STATE_CONNECTED;
			}
			return false;
		}
		if ((state != STATE_IDLE) && !is_connected) {
			state = STATE_IDLE;
			return false;
		}
	}

	return false;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_hid_report_sent_event(aeh)) {
		return handle_hid_report_sent_event(
				cast_hid_report_sent_event(aeh));
	}

	if (is_button_event(aeh)) {
		return handle_button_event(cast_button_event(aeh));
	}

	if (is_module_state_event(aeh)) {
		return handle_module_state_event(cast_module_state_event(aeh));
	}

	if (is_hid_report_subscription_event(aeh)) {
		return handle_hid_report_subscription_event(
				cast_hid_report_subscription_event(aeh));
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}
APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, button_event);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, hid_report_sent_event);
APP_EVENT_SUBSCRIBE(MODULE, hid_report_subscription_event);
