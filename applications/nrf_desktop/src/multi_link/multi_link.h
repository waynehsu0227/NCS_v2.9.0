#ifndef _MULTI_LINK_H_
#define _MULTI_LINK_H_

#include "hid_event.h"
#include <zephyr/kernel.h>
#include <psa/crypto.h>
#include <psa/crypto_extra.h>
#include <nrf_gzll.h>
#include <gzll_glue.h>
#include "multi_link_proto_device.h"

#define SIZE_DPM_INPUT              2

#ifdef CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE
#include "multi_link_mouse.h"
#else
#include "multi_link_keyboard.h"
#endif

#if (CONFIG_DPM_ENABLE && CONFIG_DESKTOP_BATTERY_MEAS)
#include "battery_event.h"
#endif
/* Maximum number of transmission attempts */
#define MAX_TX_ATTEMPTS 100

//
// Definition of the static "global" pairing address.
//
#define GAZELL_HOST_ADDRESS \
	{                       \
		4, 5, 7, 11         \
	}

/** Must equal Gazell base address length. */
#define GAZELL_HOST_SYSTEM_ADDRESS_WIDTH 4

//
// Definition of the upper boundary for the channel range to be used.
//
#define GAZELL_HOST_CHANNEL_MAX 80

//
// Definition of the lowest boundary for the channel range to be used.
//
#define GAZELL_HOST_CHANNEL_MIN 2

//
// Definition of the first static selected pairing channel. Should be located in
// the lower Nth of the channel range, where N is the size if the channel subset
// selected by the application.
//
#define GAZELL_HOST_CHANNEL_LOW 2

//
// Definition of the second static selected pairing channel. Should be located in
// the upper Nth of the channel range, where N is the size if the channel subset
// selected by the application.
//
#define GAZELL_HOST_CHANNEL_HIGH 79

//
// Definition of the minimum channel spacing for the channel subset generated
// during pairing.
//
#define GAZELL_HOST_CHANNEL_SPACING_MIN 5

//add by rex
#define MULTI_LINK_DEV_ID_WIDTH         3

enum state {
	STATE_DISABLED, //0
	STATE_GET_SYSTEM_ADDRESS, //1
	STATE_GET_HOST_ID, //2
	STATE_ACTIVE, //3
	STATE_ERROR, //4
};

#ifdef CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE
extern uint16_t g_current_dpi_value;
#endif

#ifdef CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE
extern m_mouse_data_t g_multi_link_keepalive_mouse_pkt;
#else
extern m_keyboard_data_t g_multi_link_keepalive_keyboard_pkt;
#endif

extern uint8_t multi_link_device_id[MULTI_LINK_DEV_ID_WIDTH];
extern uint8_t multi_link_device_name[10];

extern uint8_t g_packet_id_host_0;
extern uint8_t g_packet_id_host_1;
extern int32_t g_current_ping_interval;
extern int32_t  g_current_ping_duration;
extern uint32_t g_last_ping_sent_time;
extern uint8_t g_current_led_status;

extern uint8_t g_host_address[4];
extern uint8_t g_host_id[5];
extern uint8_t g_challenge[5];
extern uint8_t g_response[5];
void multilink_send_kbd_leds_report(uint8_t led_status);
#if CONFIG_DPM_ENABLE
extern uint8_t g_battery;
#endif

extern uint8_t mouse_data[REPORT_SIZE_MOUSE + 1];
extern uint8_t keyboard_data[REPORT_SIZE_KEYBOARD_KEYS + 1];
#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
extern uint8_t consumer_package[REPORT_SIZE_CONSUMER_CTRL + 1];
#endif
extern uint8_t dpm_input_data[SIZE_DPM_INPUT + 1];

#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
#if  (CONFIG_CY25KB_RECHARGEABLE)  
extern uint8_t battery_level_data[REPORT_SIZE_BATTERY_LEVEL + 2];
#else
extern uint8_t battery_level_data[REPORT_SIZE_BATTERY_LEVEL + 1];
#endif
#endif

#endif /* _MULTI_LINK_H_ */