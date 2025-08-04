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
	[LED_ID_SYSTEM_STATE] = 10,
	[LED_ID_PEER_STATE] = 7,
};
//BLE
static const uint8_t peer_led_map[] = {
//  8,//BLE CH1
//	9,//BLE CH2
    9,
    7,
};
//Multi Link
#ifdef CONFIG_MULTI_LINK_PEER_EVENTS
static const uint8_t peer_multi_link_led_map[] = {
//	7,//Multi CH   //
    8,
};
#endif

static const struct led_effect led_system_state_effect[LED_SYSTEM_STATE_COUNT] = {
	[LED_SYSTEM_STATE_IDLE]     = LED_EFFECT_LED_OFF(),
	[LED_SYSTEM_STATE_CHARGING] = LED_EFFECT_LED_BREATH(500, LED_COLOR(255, 255, 255)),
	[LED_SYSTEM_STATE_ERROR]    = LED_EFFECT_LED_BLINK(200, LED_COLOR(255, 255, 255)),
#if CONFIG_CY25KB_RECHARGEABLE    
    [LED_SYSTEM_STATE_FULL]     = LED_EFFECT_LED_ON(LED_COLOR(255, 255, 255)),
    [LED_SYSTEM_STATE_LOW_BAT]     = LED_EFFECT_LED_ON(LED_COLOR(255, 0, 0)),
#endif
};

static const struct led_effect led_peer_state_effect[LED_PEER_STATE_COUNT] = {

    [LED_PEER_STATE_DISCONNECTED]   = LED_EFFECT_LED_OFF(),
    [LED_PEER_STATE_CONNECTED]      = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,5000 ,100),
    [LED_PEER_STATE_PEER_SEARCH]    = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,3000 ,10), //LED_EFFECT_LED_BLINK(500, LED_COLOR(255, 255, 255)),
    [LED_PEER_STATE_CONFIRM_SELECT] = LED_EFFECT_LED_ON_GO_OFF(LED_COLOR(255, 255, 255) ,5000 ,100),
    [LED_PEER_STATE_CONFIRM_ERASE]  = LED_EFFECT_LED_BLINK(25, LED_COLOR(255, 255, 255)),
    [LED_PEER_STATE_ERASE_ADV]      = LED_EFFECT_LED_BLINK(100, LED_COLOR(255, 255, 255)),

};

#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT
static const uint8_t collabs_led_map[] = {
    0,//share w CH
	1,//camera w CH
    2,//camera r CH
    3,//mic w CH
    4,//mic r CH
    11,//chat w CH
};

static const struct led_effect led_collabs_state_effect[2][LED_COLLABS_STATE_COUNT] = {
    {
        [LED_COLLABS_STATE_OFF]     = LED_EFFECT_LED_OFF(),
        [LED_COLLABS_STATE_ON]      = LED_EFFECT_LED_ON(LED_COLOR(255, 255, 255)),
        [LED_COLLABS_STATE_BLINK]   = LED_EFFECT_LED_BLINK(500, LED_COLOR(255, 255, 255)),
    },
    {
        [LED_COLLABS_STATE_OFF]     = LED_EFFECT_LED_OFF(),
        [LED_COLLABS_STATE_ON]      = LED_EFFECT_LED_ON(LED_COLOR(127, 127, 127)),
        [LED_COLLABS_STATE_BLINK]   = LED_EFFECT_LED_BLINK(500, LED_COLOR(127, 127, 127)),
    },
};
#endif
