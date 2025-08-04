/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "led_state.h"
#include <caf/led_effect.h>

/* This configuration file is included only once from led_state module and holds
 * information about LED effect associated with each state.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} led_state_def_include_once;

#if CONFIG_LED_PWM

/** @def _DELL_FADE_SUBSTEPS
 *
 * @brief Substeps for color update for LED fading effect.
 */
#define _DELL_FADE_SUBSTEPS 100

/** Create LED fading effect initializer.
 *
 * LED color is smoothly, gradually changed between the LED turned off
 * and the selected color.
 *
 * @param _period	Period of time for single substep.
 * @param _color	Selected LED color.
 */
#define DELL_LED_EFFECT_LED_FADE(_on_time, _period, _color)						\
	{										\
		.steps = ((const struct led_effect_step[]) {				\
			{							\
				.color = _color,				\
				.substep_count = 1,				\
				.substep_time = 0,				\
			},							\
			{							\
				.color = _color,				\
				.substep_count = 1,				\
				.substep_time = (_on_time),			\
			},	                            \
			{								\
				.color = LED_NOCOLOR(),					\
				.substep_count = _DELL_FADE_SUBSTEPS,			\
				.substep_time = ((_period + (_DELL_FADE_SUBSTEPS - 1))	\
						/ _DELL_FADE_SUBSTEPS),			\
			},								\
		}),									\
		.step_count = 3,							\
		.loop_forever = false,							\
	}




static const uint8_t led_map[LED_ID_COUNT] = {
	[LED_ID_SYSTEM_STATE] = 0,
	//[LED_ID_PEER_STATE] = 2,
};
static const uint8_t peer_led_map[] = {
    2,//BLE CH1
	2,//BLE CH2
};

//Multi Link
#ifdef CONFIG_MULTI_LINK_PEER_EVENTS
static const uint8_t peer_multi_link_led_map[] = {
	1,//Multi CH
};
#endif
static const struct led_effect led_system_state_effect[LED_SYSTEM_STATE_COUNT] = {
	[LED_SYSTEM_STATE_IDLE]     = DELL_LED_EFFECT_LED_FADE(3000, 1000,LED_COLOR(255, 255, 255)),
	[LED_SYSTEM_STATE_CHARGING] = LED_EFFECT_LED_ON(LED_COLOR(255, 255, 255)),
	[LED_SYSTEM_STATE_ERROR]    = LED_EFFECT_LED_BLINK(200, LED_COLOR(255, 255, 255)),
};

static const struct led_effect led_peer_state_effect[LED_PEER_STATE_COUNT] = {

    [LED_PEER_STATE_DISCONNECTED]   = LED_EFFECT_LED_OFF(),
    [LED_PEER_STATE_CONNECTED]      = DELL_LED_EFFECT_LED_FADE(3000, 1000,LED_COLOR(255, 255, 255)),
    [LED_PEER_STATE_PEER_SEARCH]    = LED_EFFECT_LED_BREATH(1000, LED_COLOR(255, 255, 255)),
    [LED_PEER_STATE_CONFIRM_SELECT] = DELL_LED_EFFECT_LED_FADE(3000, 1000,LED_COLOR(255, 255, 255)),
    [LED_PEER_STATE_CONFIRM_ERASE]  = LED_EFFECT_LED_BLINK(25, LED_COLOR(255, 255, 255)),
    [LED_PEER_STATE_ERASE_ADV]      = LED_EFFECT_LED_BLINK(100, LED_COLOR(255, 255, 255)),

};



#else

/* Map function to LED ID */
static const uint8_t led_map[LED_ID_COUNT] = {
};
static const uint8_t peer_led_map[] = {
	//2,
	//3
};

//Multi Link
#ifdef CONFIG_MULTI_LINK_PEER_EVENTS
static const uint8_t peer_multi_link_led_map[] = {
	1,//Multi CH
};
#endif
static const struct led_effect led_system_state_effect[LED_SYSTEM_STATE_COUNT] = {
	[LED_SYSTEM_STATE_IDLE]     = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,3000 ,100),
	[LED_SYSTEM_STATE_CHARGING] = LED_EFFECT_LED_ON(LED_COLOR(255, 255, 255)),
	[LED_SYSTEM_STATE_ERROR]    = LED_EFFECT_LED_BLINK(200, LED_COLOR(255, 255, 255)),
};

static const struct led_effect led_peer_state_effect[LED_PEER_STATE_COUNT] = {

    [LED_PEER_STATE_DISCONNECTED]   = LED_EFFECT_LED_OFF(),
    [LED_PEER_STATE_CONNECTED]      = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,3000 ,100),
    [LED_PEER_STATE_PEER_SEARCH]    = LED_EFFECT_LED_BLINK(500, LED_COLOR(255, 255, 255)),
    [LED_PEER_STATE_CONFIRM_SELECT] = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,3000 ,100),
    [LED_PEER_STATE_CONFIRM_ERASE]  = LED_EFFECT_LED_BLINK(25, LED_COLOR(255, 255, 255)),
    [LED_PEER_STATE_ERASE_ADV]      = LED_EFFECT_LED_BLINK(100, LED_COLOR(255, 255, 255)),

};


#endif




