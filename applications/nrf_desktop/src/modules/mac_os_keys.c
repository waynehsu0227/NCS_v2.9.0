#include <sys/types.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#define MODULE mac_os_keys
#include <caf/events/module_state_event.h>
#include <caf/events/button_event.h>
#include <caf/events/click_event.h>

#include <caf/key_id.h>

#include "mac_os_key_id.h"
#include "mac_os_keys_def.h"
#include "mac_os_key_event.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_FN_KEYS_LOG_LEVEL);

static bool mac_os_mode = false;
static uint16_t mac_key_pressed[CONFIG_DESKTOP_MAC_KEYS_MAX_ACTIVE];
static size_t mac_key_pressed_count;

static void validate_enabled_mac_os_keys(void)
{
	static bool done;

	if (IS_ENABLED(CONFIG_ASSERT) && !done) {
		/* Validate the order of key IDs on the mac_os_keys array. */
		for (size_t i = 1; i < ARRAY_SIZE(mac_os_keys); i++) {
			if (mac_os_keys[i - 1] >= mac_os_keys[i]) {
				__ASSERT(false, "The mac_os_keys array must be "
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

static void *mac_os_key_enabled(uint16_t key_id)
{
	validate_enabled_mac_os_keys();

	uint8_t *p = bsearch(&key_id,
			   (uint8_t *)mac_os_keys,
			   ARRAY_SIZE(mac_os_keys),
			   sizeof(mac_os_keys[0]),
			   key_id_compare);

	return p;
}

static bool button_event_handler(const struct button_event *event)
{
	if (mac_os_mode == false)
		return false;
	uint8_t *p = mac_os_key_enabled(event->key_id);

	if (likely(!p)) {
		return false;
	}

	if (event->pressed) {
		if (mac_key_pressed_count == ARRAY_SIZE(mac_key_pressed)) {
			LOG_WRN("No space to handle mac key remapping.");
			return false;
		}
		mac_key_pressed[mac_key_pressed_count] = event->key_id;
		mac_key_pressed_count++;

		struct button_event *new_event = new_button_event();
		new_event->pressed = true;
		new_event->key_id = MAC_OS_KEY_ID(KEY_COL(event->key_id),
					      KEY_ROW(event->key_id));
		APP_EVENT_SUBMIT(new_event);

		return true;
	}

	if (!event->pressed) {
		bool key_was_remapped = false;

		for (size_t i = 0; i < mac_key_pressed_count; i++) {
			if (key_was_remapped) {
				mac_key_pressed[i - 1] = mac_key_pressed[i];
			} else if (mac_key_pressed[i] == event->key_id) {
				key_was_remapped = true;
			} else {
				/* skip */
			}
		}

		if (key_was_remapped) {
			mac_key_pressed_count--;

			struct button_event *new_event = new_button_event();
			new_event->pressed = false;
			new_event->key_id = MAC_OS_KEY_ID(KEY_COL(event->key_id),
						      KEY_ROW(event->key_id));
			APP_EVENT_SUBMIT(new_event);

			return true;
		}
	}

	return false;
}

static void send_mac_os_mode_event(void)
{
	struct mac_os_mode_on_event *new_event = new_mac_os_mode_on_event();
	new_event->sw = mac_os_mode;
	LOG_INF("set to %s key mode.", mac_os_mode ? "Mac" : "Win");
	APP_EVENT_SUBMIT(new_event);
}

static bool click_event_handler(const struct click_event *event)
{
	LOG_INF("%s", __func__);
	if (event->click != CLICK_LONG)
		return false;
	switch(event->key_id) {
		case CONFIG_MAC_MODE_SELECTOR_BUTTON:
			if (mac_os_mode != true) {
				mac_os_mode = true;				
				send_mac_os_mode_event();
			}
			break;
		case CONFIG_WIN_MODE_SELECTOR_BUTTON:
			if (mac_os_mode != false) {
				mac_os_mode = false;
				send_mac_os_mode_event();
			}
		default:
			break;
	}

	return false;
}

#define CONFIG_MAC_OS_SELECT_PORT	0
#define CONFIG_MAC_OS_SELECT_PIN	1
#include <zephyr/drivers/gpio.h>
static const struct device * const gpio_devs[] = {
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio0)),
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio1)),
#if IS_ENABLED(CONFIG_SOC_NRF54L15)
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio2)),
#endif
};
static struct gpio_callback gpio_cb[ARRAY_SIZE(gpio_devs)];

static void MAC_OS_mode_isr(const struct device *gpio_dev,
							struct gpio_callback *cb,
							uint32_t pins)
{
	int val = gpio_pin_get_raw(gpio_devs[CONFIG_MAC_OS_SELECT_PORT],
								CONFIG_MAC_OS_SELECT_PIN);

	if (val < 0) {
		LOG_ERR("Cannot get pin");
		return;
	}
	val > 0 ? (mac_os_mode = true) : (mac_os_mode = false);
	send_mac_os_mode_event();
}

static void init_fn(void)
{
	if (!device_is_ready(gpio_devs[CONFIG_MAC_OS_SELECT_PORT])) {
		LOG_ERR("GPIO device not ready");
		goto error;
	}
	int err = gpio_pin_configure(gpio_devs[CONFIG_MAC_OS_SELECT_PORT],
					    		CONFIG_MAC_OS_SELECT_PIN,
								GPIO_INPUT);

	if (err) {
		LOG_ERR("Cannot configure cols");
		goto error;
	}

	gpio_init_callback(&gpio_cb[CONFIG_MAC_OS_SELECT_PORT], MAC_OS_mode_isr, BIT(CONFIG_MAC_OS_SELECT_PIN));
	err = gpio_add_callback(gpio_devs[CONFIG_MAC_OS_SELECT_PORT], &gpio_cb[CONFIG_MAC_OS_SELECT_PORT]);
	if (err) {
		LOG_ERR("Cannot add callback");
		goto error;
	}

	int val = gpio_pin_get_raw(gpio_devs[CONFIG_MAC_OS_SELECT_PORT],
								CONFIG_MAC_OS_SELECT_PIN);

	if (val < 0) {
		LOG_ERR("Cannot get pin");
		goto error;
	}
	val > 0 ? (mac_os_mode = true) : (mac_os_mode = false);
	

	err = gpio_pin_interrupt_configure(gpio_devs[CONFIG_MAC_OS_SELECT_PORT],
										CONFIG_MAC_OS_SELECT_PIN,
										GPIO_INT_EDGE_BOTH);
	if (err) {
		LOG_ERR("Cannot configure rows");
		goto error;
	}
error:
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_module_state_event(aeh)) {
		const struct module_state_event *event =
			cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			module_set_state(MODULE_STATE_READY);
			init_fn();
			send_mac_os_mode_event();
		}

		return false;
	}

	if (is_button_event(aeh)) {
		return button_event_handler(cast_button_event(aeh));
	}

	if (is_click_event(aeh)) {
		return click_event_handler(cast_click_event(aeh));
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE_FINAL(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, button_event);
APP_EVENT_SUBSCRIBE(MODULE, click_event);
