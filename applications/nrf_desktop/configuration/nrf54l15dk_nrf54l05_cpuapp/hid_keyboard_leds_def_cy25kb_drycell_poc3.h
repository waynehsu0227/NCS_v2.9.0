/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "hid_keyboard_leds.h"

/* This configuration file is included only once from hid_state module and holds
 * information about LEDs associated with HID keyboard LEDs report.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} hid_keyboard_leds_def_include_once;

static const struct led_effect keyboard_led_on = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255), 5000, 10);
static const struct led_effect keyboard_led_off = LED_EFFECT_LED_OFF();

/* Map HID keyboard LEDs to application LED IDs. */
static const uint8_t keyboard_led_map[] = {
	[HID_KEYBOARD_LEDS_NUM_LOCK] = 5,
	[HID_KEYBOARD_LEDS_CAPS_LOCK] = 4,
};
