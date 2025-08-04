/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <sys/types.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define MODULE collabs_keys
#include <caf/events/module_state_event.h>
#include <caf/events/button_event.h>
#include "dpm_event.h"

#include <caf/key_id.h>

#include "collabs_key_id.h"
#include "collabs_keys_def.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);


static uint16_t fn_key_pressed[4];
static size_t fn_key_pressed_count;

#define SHARE_KEY_MASK      0x0001
#define CHAT_KEY_MASK       0x0002
#define VIDEO_KEY_MASK      0x0004
#define MIC_MUTE_KEY_MASK   0x0008

static uint16_t key_sw = 0;

static bool collabs_active = true;

static void *bsearch(const void *key, const uint8_t *base,
		     size_t elem_num, size_t elem_size,
		     int (*compare)(const void *, const void *))
{
	__ASSERT_NO_MSG(base);
	__ASSERT_NO_MSG(compare);
	__ASSERT_NO_MSG(elem_num <= SSIZE_MAX);

	if (!elem_num) {
		return NULL;
	}

	ssize_t lower = 0;
	ssize_t upper = elem_num - 1;

	while (upper >= lower) {
		ssize_t m = (lower + upper) / 2;
		int cmp = compare(key, base + (elem_size * m));

		if (cmp == 0) {
			return (void *)(base + (elem_size * m));
		} else if (cmp < 0) {
			upper = m - 1;
		} else {
			lower = m + 1;
		}
	}

	/* key not found */
	return NULL;
}

static int key_id_compare(const void *a, const void *b)
{
	const uint16_t *p_a = a;
	const uint16_t *p_b = b;

	return (*p_a - *p_b);
}

static bool fn_key_enabled(uint16_t key_id)
{
	uint16_t *p = bsearch(&key_id,
			   (uint8_t *)collabs_keys,
			   ARRAY_SIZE(collabs_keys),
			   sizeof(collabs_keys[0]),
			   key_id_compare);

	return (p != NULL);
}

static bool fn_key_switch_on(uint16_t key_id)
{
    for (int i = 0; i < ARRAY_SIZE(collabs_keys); i ++) {
        if (key_id == collabs_keys[i] && (key_sw & (0x00001 << i)) == 0)
            return false;
    }
    return true;
}

static bool button_event_handler(const struct button_event *event)
{
	if (unlikely(IS_COLLABS_KEY(event->key_id))) {
		return false;
	}

	if (likely(!fn_key_enabled(event->key_id))) {
		return false;
	}

    if (collabs_active == false)
        return false;

    if (likely(!fn_key_switch_on(event->key_id))) {
		return true;
	}

	if (event->pressed) {
		if (fn_key_pressed_count == ARRAY_SIZE(fn_key_pressed)) {
			LOG_WRN("No space to handle fn key remapping.");
			return false;
		}

		fn_key_pressed[fn_key_pressed_count] = event->key_id;
		fn_key_pressed_count++;

		struct button_event *new_event = new_button_event();
		new_event->pressed = true;
		new_event->key_id = COLLABS_KEY_ID(KEY_COL(event->key_id),
					      KEY_ROW(event->key_id));
		APP_EVENT_SUBMIT(new_event);

		return true;
	}

	if (!event->pressed) {
		bool key_was_remapped = false;

		for (size_t i = 0; i < fn_key_pressed_count; i++) {
			if (key_was_remapped) {
				fn_key_pressed[i - 1] = fn_key_pressed[i];
			} else if (fn_key_pressed[i] == event->key_id) {
				key_was_remapped = true;
			} else {
				/* skip */
			}
		}

		if (key_was_remapped) {
			fn_key_pressed_count--;

			struct button_event *new_event = new_button_event();
			new_event->pressed = false;
			new_event->key_id = COLLABS_KEY_ID(KEY_COL(event->key_id),
						      KEY_ROW(event->key_id));
			APP_EVENT_SUBMIT(new_event);

			return true;
		}
	}

	return false;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_module_state_event(aeh)) {
		const struct module_state_event *event =
			cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			module_set_state(MODULE_STATE_READY);
		}

		return false;
	}

    if (is_button_event(aeh)) {
		return button_event_handler(cast_button_event(aeh));
	}

    if (is_dpm_set_collabs_led_event(aeh)) {
        struct dpm_set_collabs_led_event *event = cast_dpm_set_collabs_led_event(aeh);

        if (event->led_sw.mic_mute ) {
            key_sw |= MIC_MUTE_KEY_MASK;
        } else {
            key_sw &= (~MIC_MUTE_KEY_MASK);
        }
        if (event->led_sw.chat) {
            key_sw |= CHAT_KEY_MASK;
        } else {
            key_sw &= (~CHAT_KEY_MASK);
        }
        if (event->led_sw.share) {
            key_sw |= SHARE_KEY_MASK;
        } else {
            key_sw &= (~SHARE_KEY_MASK);
        }
        if (event->led_sw.video) {
            key_sw |= VIDEO_KEY_MASK;
        } else {
            key_sw &= (~VIDEO_KEY_MASK);
        }
        return false;
    }

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, button_event);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, dpm_set_collabs_led_event);
