#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#if CONFIG_BT
#include <zephyr/settings/settings.h>
#include <caf/events/ble_common_event.h>
#endif

#include "dpm_event.h"
#include "battery_event.h"

#define MODULE dpm_setting
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);

uint8_t g_battery=86;

#if CONFIG_BT
#define KEY_LEN_MAX       42
#define HOST_NAME_LEN_MAX 19
static char host_name[3][HOST_NAME_LEN_MAX] = {{"KB_DRYCELL"},{"KB_RECHARGEABLE"},{"MS_DRYCELL"}};
static uint8_t ble_host_select_id = 0;

#define DPM_DB_HOST_MAME_1  "host_name_1"
#define DPM_DB_HOST_MAME_2  "host_name_2"

static int settings_set(const char *key, size_t len_rd,
			settings_read_cb read_cb, void *cb_arg)
{
    LOG_INF("%s start", __func__);
	if (!strcmp(key, DPM_DB_HOST_MAME_1)) {

		ssize_t len = read_cb(cb_arg, host_name[1],
				      HOST_NAME_LEN_MAX);

		if (len > HOST_NAME_LEN_MAX) {
            LOG_WRN("host_name_1 invalid length (%u)", len);
            memset(host_name[1], 0, HOST_NAME_LEN_MAX);
            return -EINVAL;
		} else {
            LOG_INF("host_name_1=%s", host_name[1]);
		}
	}

    if (!strcmp(key, DPM_DB_HOST_MAME_2)) {

		ssize_t len = read_cb(cb_arg, host_name[2],
				      HOST_NAME_LEN_MAX);

		if (len > HOST_NAME_LEN_MAX) {
            LOG_WRN("host_name_2 invalid length (%u)", len);
            memset(host_name[2], 0, HOST_NAME_LEN_MAX);
            return -EINVAL;
		} else {
            LOG_INF("host_name_2=%s", host_name[2]);
		}
	}

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(dpm, MODULE_NAME, NULL, settings_set, NULL,
			       NULL);

static void store_host_name(uint8_t *ptr, uint8_t len)
{
    char key[KEY_LEN_MAX];
    
	int err = snprintf(key, sizeof(key), MODULE_NAME "/%s",
				ble_host_select_id == 0 ?  DPM_DB_HOST_MAME_1 : DPM_DB_HOST_MAME_2);
    if (err < 0 || err > sizeof(key)) {
        LOG_ERR("Option format err: %s",
            ble_host_select_id == 0 ?  DPM_DB_HOST_MAME_1 : DPM_DB_HOST_MAME_2);
        module_set_state(MODULE_STATE_ERROR);
        return;
    }
    
    err = settings_save_one(key, ptr, len);

    if (err) {
        LOG_ERR("Cannot store %s, err %d",
        ble_host_select_id == 0 ?  DPM_DB_HOST_MAME_1 : DPM_DB_HOST_MAME_2,
        err);
        module_set_state(MODULE_STATE_ERROR);
    }
}

static bool handle_dpm_save_host_name_event(const struct dpm_save_host_name_event *event)
{

    uint8_t len = event->dyndata.size;
    uint8_t name_buf[HOST_NAME_LEN_MAX] = {0};
    if (len > HOST_NAME_LEN_MAX-1)
        len = HOST_NAME_LEN_MAX-1;
    memcpy(name_buf, event->dyndata.data, len);
#if 1
    if (memcmp(host_name[ble_host_select_id+1], name_buf, len)) {
        memset(host_name[ble_host_select_id+1], 0, HOST_NAME_LEN_MAX);
        memcpy(host_name[ble_host_select_id+1], name_buf, len);
        store_host_name(name_buf, HOST_NAME_LEN_MAX);
        LOG_INF("%s:store host%d name:%s OK!", __func__,
                                            ble_host_select_id+1,
                                            event->dyndata.data);
    } else {
        LOG_WRN("%s:store host%d name:%s the same!",
                __func__,
                ble_host_select_id+1,
                event->dyndata.data);
    }
#endif
	return false;
}

static bool handle_ble_peer_operation_event(const struct ble_peer_operation_event *event)
{
    if (event->op == PEER_OPERATION_SELECTED) {
        ble_host_select_id = event->bt_app_id;
        LOG_DBG("%s: ble_host_select_id=%d", __func__, ble_host_select_id);
    }
    return false;
}
#endif
static bool state_is_cnahge = true;
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
#if CONFIG_BT
    if (is_dpm_save_host_name_event(aeh)) {
        return handle_dpm_save_host_name_event(cast_dpm_save_host_name_event(aeh));
	}

    if (is_ble_peer_operation_event(aeh)) {
        return handle_ble_peer_operation_event(cast_ble_peer_operation_event(aeh));
	}
#endif    
	if (is_battery_level_event(aeh)) {
		struct battery_level_event *event = cast_battery_level_event(aeh);
        extern uint8_t g_battery;
        if(event->level<=g_battery || event->is_charging || state_is_cnahge)
        {
		    g_battery = event->level;
            state_is_cnahge = false;
        }
		return false;
	}

#if (CONFIG_CY25KB_RECHARGEABLE)
	if (is_battery_state_event(aeh)) {
        struct battery_state_event *event = cast_battery_state_event(aeh);
        if(event->state==BATTERY_STATE_CHARGING || event->state==BATTERY_STATE_IDLE)
            state_is_cnahge = true;
		return false;
	}
#endif

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}










































































































APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, dpm_save_host_name_event);
#if CONFIG_BT
APP_EVENT_SUBSCRIBE(MODULE, ble_peer_operation_event);
#endif
APP_EVENT_SUBSCRIBE(MODULE, battery_level_event);
#if (CONFIG_CY25KB_RECHARGEABLE)
APP_EVENT_SUBSCRIBE(MODULE, battery_state_event);
#endif


char* get_host_name(uint8_t index)
{
#if CONFIG_BT
    if (index > 2)
        return NULL;
    return host_name[index];
#else
    return NULL;
#endif
}
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3232)
#include "motion_sensor_config_event.h"
static uint16_t current_dpi_value = CONFIG_DESKTOP_MOTION_SENSOR_CPI;

uint16_t get_current_dpi(void)
{
    return current_dpi_value;
}

void send_set_dpi_event(uint16_t dpi_value)
{
    current_dpi_value = dpi_value;
	struct motion_sensor_config_event *event = new_motion_sensor_config_event();
	event->opt = MOTION_SENSOR_OPTION_CPI;
	event->value = current_dpi_value;
	APP_EVENT_SUBMIT(event);
}

static const uint16_t support_report_rate[] =
{
    125,133,250,333
};
static uint16_t current_report_rate = 125;
void send_set_report_rate_event(uint16_t report_rate)
{
    for (int i = 0; i < (sizeof(support_report_rate) / 2); i ++) {
        if (report_rate == support_report_rate[i]) {
            current_report_rate = report_rate;
            struct dpm_set_report_rate_event *event = new_dpm_set_report_rate_event();
            event->report_interval = 1000 / report_rate;
            APP_EVENT_SUBMIT(event);
        }
    }
}
uint16_t* get_report_rate_current(void)
{
    return &current_report_rate;
}
uint16_t* get_report_rate_support(void)
{ 
    return support_report_rate;
}
#endif

#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT

static uint8_t collabs_keys_status[5] = {0};

void set_collabs_keys_status(uint8_t *buf)
{
    memcpy(collabs_keys_status, buf, sizeof(collabs_keys_status));
    LOG_HEXDUMP_INF(collabs_keys_status,
                    sizeof(collabs_keys_status),
                    __func__);
    struct dpm_set_collabs_led_event *event = new_dpm_set_collabs_led_event();
    event->led_sw.video = buf[0] & 0x01;
    event->led_sw.share = buf[0] & 0x02;
    event->led_sw.chat = buf[0] & 0x04;
    event->led_sw.mic_mute = buf[0] & 0x08;

    event->led_state.chat_led_blink = buf[1] & 0x01;

    event->led_lower_brightness = buf[2] & 0x01;

    event->led_state.camera_led = buf[4] & 0x01;
    event->led_state.camera_disable_led = buf[4] & 0x02;
    event->led_state.share_screen_led = buf[4] & 0x04;
    event->led_state.chat_led = buf[4] & 0x08;
    event->led_state.mic_led = buf[4] & 0x10;
    event->led_state.mic_disable_led = buf[4] & 0x20;

	APP_EVENT_SUBMIT(event);
}

uint8_t* get_collabs_keys_status(void)
{
    LOG_HEXDUMP_INF(collabs_keys_status,
                    sizeof(collabs_keys_status),
                    __func__);
    return collabs_keys_status;
}

#endif
