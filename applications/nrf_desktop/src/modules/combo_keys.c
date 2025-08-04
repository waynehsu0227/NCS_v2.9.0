#include <sys/types.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define MODULE combo_keys
#include <caf/events/module_state_event.h>
#include <caf/events/button_event.h>

#include <caf/key_id.h>

#include "fn_key_id.h"
#include "combo_keys_def.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_FN_KEYS_LOG_LEVEL);

#if CONFIG_DESKTOP_MAC_OS_KEYS_ENABLE
#include "mac_os_key_event.h"
static bool mac_os_mode = false;
#endif


static void validate_enabled_combo_keys(void)
{
	static bool done;

	if (IS_ENABLED(CONFIG_ASSERT) && !done) {
		/* Validate the order of key IDs on the combo_keys array. */
		for (size_t i = 1; i < ARRAY_SIZE(combo_keys); i++) {
			if (combo_keys[i - 1] >= combo_keys[i]) {
				__ASSERT(false, "The combo_keys array must be "
						"sorted by key_id!");
			}
		}

		done = true;
	}
}

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

static void *fn_key_enabled(uint16_t key_id)
{
	validate_enabled_combo_keys();

	uint8_t *p = bsearch(&key_id,
			   (uint8_t *)combo_keys,
			   ARRAY_SIZE(combo_keys),
			   sizeof(combo_keys[0]),
			   key_id_compare);

	return p;
}

static bool button_event_handler(const struct button_event *event)
{
	uint8_t *p = fn_key_enabled(event->key_id);

	if (likely(!p)) {
		return false;
	}

#if CONFIG_DESKTOP_MAC_OS_KEYS_ENABLE
	if (mac_os_mode && 
		event->key_id == MAC_MODE_NO_CONVER_KEY) {    /* application -> copilot */
		return false;
	}
#endif

	uint16_t cobmo_index = (p - (uint8_t *)combo_keys) / sizeof(combo_keys[0]);
	const uint16_t *combo_key_data = combo_keys_table[cobmo_index];

	if (event->pressed) {
		for (int16_t i = 1; i <= combo_key_data[0]; i ++) {
			struct button_event *new_event = new_button_event();
			new_event->pressed = true;
			new_event->key_id = combo_key_data[i];
			APP_EVENT_SUBMIT(new_event);
		}

		return true;
	} else {
		for (int16_t i = combo_key_data[0]; i > 0; i --) {
			struct button_event *new_event = new_button_event();
			new_event->pressed = false;
			new_event->key_id = combo_key_data[i];
			APP_EVENT_SUBMIT(new_event);
		}
		return true;
	}

	return false;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_button_event(aeh)) {
		return button_event_handler(cast_button_event(aeh));
	}

	if (is_module_state_event(aeh)) {
		const struct module_state_event *event =
			cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			module_set_state(MODULE_STATE_READY);
		}

		return false;
	}

#if CONFIG_DESKTOP_MAC_OS_KEYS_ENABLE
	if (is_mac_os_mode_on_event(aeh)) {
		const struct mac_os_mode_on_event *event =
			cast_mac_os_mode_on_event(aeh);

		mac_os_mode = event->sw;
		return false;
	}
#endif

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, button_event);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
#if CONFIG_DESKTOP_MAC_OS_KEYS_ENABLE
APP_EVENT_SUBSCRIBE(MODULE, mac_os_mode_on_event);
#endif
