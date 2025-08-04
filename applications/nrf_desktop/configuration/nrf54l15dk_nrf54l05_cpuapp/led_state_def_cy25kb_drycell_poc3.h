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


/* Map function to LED ID */
static const uint8_t led_map[LED_ID_COUNT] = {
	[LED_ID_SYSTEM_STATE] = 0,
	[LED_ID_PEER_STATE] = 3,
};
static const uint8_t peer_led_map[] = {
    2,//BLE CH1
	3,//BLE CH2
};

//Multi Link
#ifdef CONFIG_MULTI_LINK_PEER_EVENTS
static const uint8_t peer_multi_link_led_map[] = {
	1,//Multi CH
};
#endif

static const struct led_effect led_system_state_effect[LED_SYSTEM_STATE_COUNT] = {
	[LED_SYSTEM_STATE_IDLE]     = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,3000 ,500),
    [LED_SYSTEM_STATE_LOW_BAT]  = LED_EFFECT_LED_LOW_BAT(500, LED_COLOR(255, 255, 255) ,300000),
};

static const struct led_effect led_peer_state_effect[LED_PEER_STATE_COUNT] = {

    [LED_PEER_STATE_DISCONNECTED]   = LED_EFFECT_LED_OFF(),
    [LED_PEER_STATE_CONNECTED]      = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,3000 ,10),
    [LED_PEER_STATE_PEER_SEARCH]    = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,3000 ,10),
    [LED_PEER_STATE_CONFIRM_SELECT] = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,3000 ,10),
    [LED_PEER_STATE_CONFIRM_ERASE]  = LED_EFFECT_LED_OFF(),
    [LED_PEER_STATE_ERASE_ADV]      = LED_EFFECT_LED_BLINK(500, LED_COLOR(255, 255, 255)),
};
