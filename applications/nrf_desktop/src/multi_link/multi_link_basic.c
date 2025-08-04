#include "multi_link.h"
#include "multi_link_basic.h"
#include "../modules/dpm_config.h"

#ifdef __ZEPHYR__

#define MODULE multi_link_basic
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);
#else
#define LOG_DBG(...)
#define LOG_ERR(...)
#define LOG_WRN(...)
#define LOG_HEXDUMP_DBG(...)
#define LOG_HEXDUMP_INF(...)
#endif

void multi_link_device_sleep_ms(uint32_t time_ms)
{
	k_msleep(time_ms);
}

uint32_t multi_link_device_get_uptime_ms()
{
	return k_uptime_get_32();
}

uint32_t multi_link_device_get_dynamic_key_prepare_timout_ms()
{
	return 500; // Key change as required. After this timeout will stop trying dynamic key request and device_dynamic_key_status_changed will called with MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_TIMEOUT
}

void multi_link_device_update_host_id(uint8_t (*host_id)[5])
{
	memcpy(g_host_id, &host_id[0], sizeof(g_host_id));
}

p_device_id_t multi_link_device_get_device_id()
{
	return &multi_link_device_id;
}

p_device_name_t multi_link_device_get_device_name()
{
	return &multi_link_device_name;
}

uint8_t multi_link_device_get_device_type()
{
#ifdef CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE
	return DEVICE_TYPE_MOUSE;
#else
	return DEVICE_TYPE_KEYBOARD;
#endif
}

uint8_t multi_link_device_get_vendor_id()
{
	return VENDOR_ID_RESEVERED_4;
}

uint32_t multi_link_device_get_paring_timout_ms()
{
	return 180000; //ms
}

uint16_t multi_link_device_get_device_firmware_version()
{
	return DPM_FW_VER;
}

uint16_t multi_link_device_get_device_capability()
{
	return (DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_FIRMWARE_UPDATE_SUPPORTED |
			DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_FN_KEY_SUPPORTED |
			DONGLE_CONNECTED_DEVICE_CAPABILITY_MOUSE_DPI_SUPPORTED |
			DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_GET_PAIRED_DEVICE_HOST_NAME_SUPPORTED |
			DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_GET_BATTERY_INDICATOR_SUPPORTED |
			DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_NEW_DEVICE_SUPPORTED);
}

uint16_t multi_link_device_get_device_kb_layout()
{
	return 0;
}

void multi_link_device_update_challenge(uint8_t (*challenge)[5])
{
	memcpy(g_challenge, &challenge[0], sizeof(g_challenge));
}

void multi_link_device_update_response(uint8_t (*response)[5])
{
	memcpy(g_response, &response[0], sizeof(g_response));
}

p_host_address_t multi_link_device_get_host_address()
{
	//
	// g_host_address should get from flash if paring was success else all pass 0
	//
	return &g_host_address;
}

p_host_id_t multi_link_device_get_host_id()
{
	//
	// g_host_id should get from flash if paring was success else all pass 0
	//
	return &g_host_id;
}

p_challenge_t multi_link_device_get_challenge()
{
	//
	// g_challenge should get from flash if paring was success else all pass 0
	//
	return &g_challenge;
}

p_response_t multi_link_device_get_response()
{
	//
	// g_response should get from flash if paring was success else all pass 0
	//
	return &g_response;
}
