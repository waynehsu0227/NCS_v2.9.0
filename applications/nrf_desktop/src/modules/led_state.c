/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>

#define MODULE led_state
#include <caf/events/module_state_event.h>

#include <caf/events/led_event.h>
#include <caf/events/ble_common_event.h>
#include "battery_event.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DESKTOP_LED_STATE_LOG_LEVEL);

#include "led_state.h"
#include CONFIG_DESKTOP_LED_STATE_DEF_PATH
//rex add
#if CONFIG_MULTI_LINK_PEER_EVENTS
#include "multi_link_peer_event.h"
#endif

#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT
#include "dpm_event.h"
#endif

static enum led_system_state system_state = LED_SYSTEM_STATE_IDLE;

static uint8_t connected;
static bool peer_search;
static enum peer_operation peer_op = PEER_OPERATION_CANCEL;
static uint8_t cur_peer_id;

static void turn_off_rf_led(void)
{
	LOG_ERR("%s", __func__);
	enum led_peer_state state = LED_PEER_STATE_DISCONNECTED;
	for(uint8_t peer_id=0;peer_id < LED_PEER_COUNT;peer_id++)
	{
#ifdef CONFIG_CY25
		if (peer_multi_link_led_map[peer_id] == LED_UNAVAILABLE) {
			return;
		}
#else
		if (led_map[LED_ID_PEER_STATE] == LED_UNAVAILABLE) {
			return;
		}
#endif
		__ASSERT_NO_MSG(peer_id < LED_PEER_COUNT);
		__ASSERT_NO_MSG(state < LED_PEER_STATE_COUNT);

	struct led_event *event = new_led_event();
#ifdef CONFIG_CY25
		event->led_id = peer_multi_link_led_map[peer_id];
		event->led_effect = &led_peer_state_effect[state];
#else
    	event->led_id = led_map[LED_ID_PEER_STATE];
		event->led_effect = &led_peer_state_effect[peer_id][state];
#endif

		APP_EVENT_SUBMIT(event);
	}
}

static void load_peer_state_led(void)
{
	LOG_ERR("%s cur_peer_id:%d peer_op: %d", __func__, cur_peer_id, peer_op);
	enum led_peer_state state = LED_PEER_STATE_DISCONNECTED;
	uint8_t peer_id = cur_peer_id;

	switch (peer_op) {
	case PEER_OPERATION_SELECT:
		state = LED_PEER_STATE_CONFIRM_SELECT;
		break;
	case PEER_OPERATION_ERASE:
		state = LED_PEER_STATE_CONFIRM_ERASE;
		break;
	case PEER_OPERATION_ERASE_ADV:
		state = LED_PEER_STATE_ERASE_ADV;
		break;
	//bill
	case PEER_OPERATION_ERASE_ADV_CANCEL:
		state=LED_PEER_STATE_DISCONNECTED;
		break;
	//bill
	case PEER_OPERATION_SELECTED:
	// case PEER_OPERATION_ERASE_ADV_CANCEL://bill
	case PEER_OPERATION_ERASED:
	case PEER_OPERATION_CANCEL:
	case PEER_OPERATION_SCAN_REQUEST:
	    turn_off_rf_led();
		if (peer_search) {
			state = LED_PEER_STATE_PEER_SEARCH;
		} else if (connected > 0) {
			state = LED_PEER_STATE_CONNECTED;
		}
#if CONFIG_DESKTOP_CY25_BLE_PEER_SELECT
    case PEER_OPERATION_SWITCH:
#endif
		break;
	default:
		__ASSERT_NO_MSG(false);
		break;
	}
#ifdef CONFIG_CY25
	if (peer_led_map[peer_id] == LED_UNAVAILABLE) {
		return;
	}
#else
	if (led_map[LED_ID_PEER_STATE] == LED_UNAVAILABLE) {
		return;
	}
#endif
	__ASSERT_NO_MSG(peer_id < LED_PEER_COUNT);
	__ASSERT_NO_MSG(state < LED_PEER_STATE_COUNT);

	struct led_event *event = new_led_event();
#ifdef CONFIG_CY25
	event->led_id = peer_led_map[peer_id];
	event->led_effect = &led_peer_state_effect[state];
#else
    event->led_id = led_map[LED_ID_PEER_STATE];
	event->led_effect = &led_peer_state_effect[peer_id][state];
#endif

	APP_EVENT_SUBMIT(event);
}
#if CONFIG_MULTI_LINK_PEER_EVENTS
static void turn_off_ble_led(void)
{
	LOG_ERR("%s", __func__);
	enum led_peer_state state = LED_PEER_STATE_DISCONNECTED;
	for(uint8_t peer_id=0;peer_id < LED_PEER_COUNT;peer_id++)
	{
#ifdef CONFIG_CY25
		if (peer_led_map[peer_id] == LED_UNAVAILABLE) {
			return;
		}
#else
		if (led_map[LED_ID_PEER_STATE] == LED_UNAVAILABLE) {
			return;
		}
#endif
		__ASSERT_NO_MSG(peer_id < LED_PEER_COUNT);
		__ASSERT_NO_MSG(state < LED_PEER_STATE_COUNT);

	struct led_event *event = new_led_event();
#ifdef CONFIG_CY25
		event->led_id = peer_led_map[peer_id];
		event->led_effect = &led_peer_state_effect[state];
#else
    	event->led_id = led_map[LED_ID_PEER_STATE];
		event->led_effect = &led_peer_state_effect[peer_id][state];
#endif

		APP_EVENT_SUBMIT(event);
	}
}

static bool multi_link_connected;
static bool multi_link_pairing;
static enum multi_link_operation multi_link_op = PEER_OPERATION_CANCEL; //RF
extern bool is_connect;//bill20250513 
static void load_multi_link_peer_state_led(void)
{
	LOG_ERR("%s", __func__);
	enum led_peer_state state = LED_PEER_STATE_DISCONNECTED;
	uint8_t peer_id = cur_peer_id; //led_state_def.h

	switch (multi_link_op) {
	case MULTI_LINK_OPERATION_SELECT:
		state = LED_PEER_STATE_CONFIRM_SELECT;
		break;
	case MULTI_LINK_OPERATION_ERASE:
		state = LED_PEER_STATE_CONFIRM_ERASE;
		break;
	case MULTI_LINK_OPERATION_ERASE_ADV:
		state = LED_PEER_STATE_ERASE_ADV;
		break;
	case MULTI_LINK_OPERATION_SELECTED:
	case MULTI_LINK_OPERATION_ERASE_ADV_CANCEL:
	case MULTI_LINK_OPERATION_ERASED:
	case MULTI_LINK_OPERATION_CANCEL:
	case MULTI_LINK_OPERATION_SCAN_REQUEST:
		//bill
		// if (multi_link_pairing) {
		// 	state = LED_PEER_STATE_ERASE_ADV;
		// } else if(multi_link_connected){
		// 	state = LED_PEER_STATE_CONNECTED;
		turn_off_ble_led();
		// }
		if (multi_link_pairing) {
			state = LED_PEER_STATE_ERASE_ADV;
		} else if(is_connect){
			state = LED_PEER_STATE_CONNECTED;
			// turn_off_ble_led();
		}
		//bill
		else if(is_connect==false)
		{
			state=LED_PEER_STATE_PEER_SEARCH;
		}
		else
		{

		}
		//bill
    case MULTI_LINK_OPERATION_SWITCH:
		break;
	default:
		__ASSERT_NO_MSG(false);
		break;
	}
#ifdef CONFIG_CY25
	if (peer_multi_link_led_map[peer_id] == LED_UNAVAILABLE) {
		return;
	}
#else
	if (led_map[LED_ID_PEER_STATE] == LED_UNAVAILABLE) {
		return;
	}
#endif
	LOG_ERR("load_multi_link_peer_state_led :%d",state);
	//__ASSERT_NO_MSG(peer_id < LED_PEER_COUNT);
	__ASSERT_NO_MSG(state < LED_PEER_STATE_COUNT);

	struct led_event *event = new_led_event();
#ifdef CONFIG_CY25
	event->led_id = peer_multi_link_led_map[peer_id];
	event->led_effect = &led_peer_state_effect[state];
#else
   event->led_id = led_map[LED_ID_PEER_STATE];
	event->led_effect = &led_peer_state_effect[peer_id][state];
#endif

	APP_EVENT_SUBMIT(event);
}
#endif

#if !NRF_POWER_HAS_RESETREAS
#include <hal/nrf_reset.h>
#endif
static void print_reset_reason(void)
{
	uint32_t reas;

#if NRF_POWER_HAS_RESETREAS

	reas = nrf_power_resetreas_get(NRF_POWER);
	nrf_power_resetreas_clear(NRF_POWER, reas);
	if (reas & NRF_POWER_RESETREAS_NFC_MASK) {
		printk("Wake up by NFC field detect\n");
	} else if (reas & NRF_POWER_RESETREAS_RESETPIN_MASK) {
		printk("Reset by pin-reset\n");
	} else if (reas & NRF_POWER_RESETREAS_SREQ_MASK) {
		printk("Reset by soft-reset\n");
	} else if (reas) {
		printk("Reset by a different source (0x%08X)\n", reas);
	} else {
		printk("Power-on-reset\n");
	}

#else

	reas = nrf_reset_resetreas_get(NRF_RESET);
	nrf_reset_resetreas_clear(NRF_RESET, reas);
	if (reas & NRF_RESET_RESETREAS_NFC_MASK) {
		LOG_INF("Wake up by NFC field detect\n");
	} else if (reas & NRF_RESET_RESETREAS_RESETPIN_MASK) {
		LOG_INF("Reset by pin-reset\n");
	} else if (reas & NRF_RESET_RESETREAS_SREQ_MASK) {
		LOG_INF("Reset by soft-reset\n");
	} else if (reas) {
		LOG_INF("Reset by a different source (0x%08X)\n", reas);
	} else {
		LOG_INF("Power-on-reset\n");
	}

#endif
}

static void load_system_state_led(void)
{

	uint32_t reas;
	reas = nrf_reset_resetreas_get(NRF_RESET);
	nrf_reset_resetreas_clear(NRF_RESET, reas);
	if (reas & NRF_RESET_RESETREAS_SREQ_MASK) 
	{
		LOG_ERR("Reset by soft-reset, ignor power on led\n");		
		return;
	}

	if (led_map[LED_ID_SYSTEM_STATE] == LED_UNAVAILABLE) {
		return;
	}

	struct led_event *event = new_led_event();

	event->led_id = led_map[LED_ID_SYSTEM_STATE];
	event->led_effect = &led_system_state_effect[system_state];
	APP_EVENT_SUBMIT(event);
}

static void set_system_state_led(enum led_system_state state)
{
	__ASSERT_NO_MSG(state < LED_SYSTEM_STATE_COUNT);

	if (system_state != LED_SYSTEM_STATE_ERROR) {
		system_state = state;
		load_system_state_led();
	}
}

#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT
static void load_collabs_share_led(bool led_lower_brightness, enum led_collabs_state state)
{
    struct led_event *event = new_led_event();
    LOG_DBG("%s:%d", __func__, state);
    event->led_id = collabs_led_map[COLLABS_LED_ID_SHARE];
	event->led_effect = &led_collabs_state_effect[led_lower_brightness][state];
	APP_EVENT_SUBMIT(event);
}

static void load_collabs_camera_led(bool led_lower_brightness, enum led_collabs_state state)
{
    struct led_event *event = new_led_event();
    LOG_DBG("%s:%d", __func__, state);
    event->led_id = collabs_led_map[COLLABS_LED_ID_CAMERA_W];
	event->led_effect = &led_collabs_state_effect[led_lower_brightness][state];
	APP_EVENT_SUBMIT(event);
}

static void load_collabs_camera_disable_led(bool led_lower_brightness, enum led_collabs_state state)
{
    struct led_event *event = new_led_event();
    LOG_DBG("%s:%d", __func__, state);
    event->led_id = collabs_led_map[COLLABS_LED_ID_CAMERA_R];
	event->led_effect = &led_collabs_state_effect[led_lower_brightness][state];
	APP_EVENT_SUBMIT(event);
}

static void load_collabs_mic_led(bool led_lower_brightness, enum led_collabs_state state)
{
    struct led_event *event = new_led_event();
    LOG_DBG("%s:%d", __func__, state);
    event->led_id = collabs_led_map[COLLABS_LED_ID_MIC_W];
	event->led_effect = &led_collabs_state_effect[led_lower_brightness][state];
	APP_EVENT_SUBMIT(event);
}

static void load_collabs_mic_disable_led(bool led_lower_brightness, enum led_collabs_state state)
{
    struct led_event *event = new_led_event();
    LOG_DBG("%s:%d", __func__, state);
    event->led_id = collabs_led_map[COLLABS_LED_ID_MIC_R];
	event->led_effect = &led_collabs_state_effect[led_lower_brightness][state];
	APP_EVENT_SUBMIT(event);
}

static void load_collabs_chat_led(bool led_lower_brightness, enum led_collabs_state state)
{
    struct led_event *event = new_led_event();
    LOG_DBG("%s:%d", __func__, state);
    event->led_id = collabs_led_map[COLLABS_LED_ID_CHAT];
	event->led_effect = &led_collabs_state_effect[led_lower_brightness][state];
	APP_EVENT_SUBMIT(event);
}
#endif

static bool app_event_handler(const struct app_event_header *aeh)
{
#if CONFIG_MULTI_LINK_PEER_EVENTS
	if (is_multi_link_peer_event(aeh)) {
		LOG_ERR("is_multi_link_peer_event");
		struct multi_link_peer_event *event = cast_multi_link_peer_event(aeh);
		multi_link_pairing = false;
		switch (event->state)  {
		case MULTI_LINK_STATE_CONNECTED:
		    multi_link_connected = true;
			break;
		case MULTI_LINK_STATE_DISCONNECTED:
		    multi_link_connected = false;
			break;
		case MULTI_LINK_STATE_SECURED:
		case MULTI_LINK_STATE_CONN_FAILED:
		case MULTI_LINK_STATE_DISCONNECTING:
			/* Ignore */
		    break;
		case MULTI_LINK_STATE_PAIRING:
			multi_link_pairing = true;
			break;
		default:
			__ASSERT_NO_MSG(false);
			break;
		}
		load_multi_link_peer_state_led();

		return false;
	}
#endif
	if (IS_ENABLED(CONFIG_CAF_BLE_COMMON_EVENTS) && is_ble_peer_event(aeh)) {
		struct ble_peer_event *event = cast_ble_peer_event(aeh);

		switch (event->state)  {
		case PEER_STATE_CONNECTED:
			__ASSERT_NO_MSG(connected < UINT8_MAX);
			connected++;
			break;
		case PEER_STATE_DISCONNECTED:
			__ASSERT_NO_MSG(connected > 0);
			connected--;
			break;
		case PEER_STATE_SECURED:
		case PEER_STATE_CONN_FAILED:
		case PEER_STATE_DISCONNECTING:
			/* Ignore */
			break;
		default:
			__ASSERT_NO_MSG(false);
			break;
		}
		load_peer_state_led();

		return false;
	}

	if (IS_ENABLED(CONFIG_CAF_BLE_COMMON_EVENTS) && is_ble_peer_search_event(aeh)) {
		struct ble_peer_search_event *event = cast_ble_peer_search_event(aeh);

		peer_search = event->active;
		load_peer_state_led();

		return false;
	}

	if (IS_ENABLED(CONFIG_CAF_BLE_COMMON_EVENTS) && is_ble_peer_operation_event(aeh)) {
		struct ble_peer_operation_event *event =
			cast_ble_peer_operation_event(aeh);

		cur_peer_id = event->bt_app_id;
		peer_op = event->op;
		load_peer_state_led();

		return false;
	}

	if (is_battery_state_event(aeh)) {
		struct battery_state_event *event = cast_battery_state_event(aeh);

		switch (event->state) {
		case BATTERY_STATE_CHARGING:
			set_system_state_led(LED_SYSTEM_STATE_CHARGING);
			break;
		case BATTERY_STATE_IDLE:
			set_system_state_led(LED_SYSTEM_STATE_IDLE);
			break;
		case BATTERY_STATE_ERROR:
			set_system_state_led(LED_SYSTEM_STATE_ERROR);
			break;
#if CONFIG_CY25KB_RECHARGEABLE
		case BATTERY_STATE_FULL:
			set_system_state_led(LED_SYSTEM_STATE_FULL);
			break;
#endif
#if CONFIG_CY25
		case BATTERY_STATE_LOW_BAT:
			set_system_state_led(LED_SYSTEM_STATE_LOW_BAT);
			break;
#endif
		default:
			__ASSERT_NO_MSG(false);
			break;
		}

		return false;
	}

#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT
    if (is_dpm_set_collabs_led_event(aeh)) {
        struct dpm_set_collabs_led_event *event = cast_dpm_set_collabs_led_event(aeh);

        enum led_collabs_state state;
        if (event->led_sw.share == true) {
            if (event->led_state.share_screen_led == true) {
                state = LED_COLLABS_STATE_ON;
            } else {
                state = LED_COLLABS_STATE_OFF;
            }
        } else {
            state = LED_COLLABS_STATE_OFF;
        }
        load_collabs_share_led(event->led_lower_brightness, state);

        if (event->led_sw.video == true) {
            if (event->led_state.camera_led == true) {
                state = LED_COLLABS_STATE_ON;
            } else {
                state = LED_COLLABS_STATE_OFF;
            }
        } else {
            state = LED_COLLABS_STATE_OFF;
        }
        load_collabs_camera_led(event->led_lower_brightness, state);

        if (event->led_sw.video == true) {
            if (event->led_state.camera_disable_led == true) {
                state =  LED_COLLABS_STATE_ON;
            } else {
                state =  LED_COLLABS_STATE_OFF;
            }
        } else {
            state =  LED_COLLABS_STATE_OFF;
        }
        load_collabs_camera_disable_led(event->led_lower_brightness, state);

        if (event->led_sw.mic_mute == true) {
            if (event->led_state.mic_led == true) {
                state = LED_COLLABS_STATE_ON;
            } else {
                state = LED_COLLABS_STATE_OFF;
            }
        } else {
            state = LED_COLLABS_STATE_OFF;
        }
        load_collabs_mic_led(event->led_lower_brightness, state);

        if (event->led_sw.mic_mute == true) {
            if (event->led_state.mic_disable_led == true) {
                state = LED_COLLABS_STATE_ON;
            } else {
                state = LED_COLLABS_STATE_OFF;
            }
        } else {
            state = LED_COLLABS_STATE_OFF;
        }
        load_collabs_mic_disable_led(event->led_lower_brightness, state);

        if (event->led_sw.chat == true) {
            if (event->led_state.chat_led_blink == true) {
                state = LED_COLLABS_STATE_BLINK;
            } else {
                if (event->led_state.chat_led == true) {
                    state = LED_COLLABS_STATE_ON;
                } else {
                    state = LED_COLLABS_STATE_OFF;
                }
            }
        } else {
            state = LED_COLLABS_STATE_OFF;
        }
        load_collabs_chat_led(event->led_lower_brightness, state);

        return false;
    }
#endif

	if (is_module_state_event(aeh)) {
		struct module_state_event *event = cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			load_system_state_led();
			if (!IS_ENABLED(CONFIG_CAF_BLE_COMMON_EVENTS)) {
				/* BLE peer state will not be notified. Preload the LED. */
				load_peer_state_led();
			}
		} else if (event->state == MODULE_STATE_ERROR) {
			set_system_state_led(LED_SYSTEM_STATE_ERROR);
		}
		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
#ifdef CONFIG_CAF_BLE_COMMON_EVENTS
APP_EVENT_SUBSCRIBE(MODULE, ble_peer_event);
APP_EVENT_SUBSCRIBE(MODULE, ble_peer_search_event);
APP_EVENT_SUBSCRIBE(MODULE, ble_peer_operation_event);
#endif /* CONFIG_CAF_BLE_COMMON_EVENTS */
APP_EVENT_SUBSCRIBE(MODULE, battery_state_event);
//Rex add
#if CONFIG_MULTI_LINK_PEER_EVENTS
APP_EVENT_SUBSCRIBE(MODULE, multi_link_peer_event);
#endif

#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT
APP_EVENT_SUBSCRIBE(MODULE, dpm_set_collabs_led_event);
#endif
