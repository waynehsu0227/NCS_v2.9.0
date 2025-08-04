#ifndef _MULTI_LINK_PROTO_DEVICE_H_
#define _MULTI_LINK_PROTO_DEVICE_H_

#if defined ( __CC_ARM   )
#pragma anon_unions
#endif

#include <string.h>
#include "multi_link_proto_common.h"

#ifdef __cplusplus
extern "C"
{
#endif
	#define COMMAND_PPID_GET (1<<0)
	#define COMMAND_DPI_GET	(1<<1)
	#define COMMAND_REPORT_RATE_GET	(1<<2)
	#define COMMAND_FACTORY_COMMAND_GET	(1<<3)
	#define COMMAND_BATTERY_GET	(1<<4)
	// new for 2026 DellHIDFeatureControlSupplement_CY2025_v1a_20042025
	#define COMMAND_DPI_LIST_1_GET (1<<5)
	#define COMMAND_DPI_LIST_2_GET (1<<6)
	#define COMMAND_SCROLL_OPTION_GET (1<<7)
	#define COMMAND_FN_ROW_OPTION_GET (1<<8)
	#define COMMAND_MACRO_KEY_GET (1<<9)
	#define COMMAND_SMOOTH_SCROLL_OPTION_GET (1<<10)

	#define DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_FIRMWARE_UPDATE_SUPPORTED 0x01
	#define DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_FN_KEY_SUPPORTED 0x02
	#define DONGLE_CONNECTED_DEVICE_CAPABILITY_MOUSE_DPI_SUPPORTED 0x02
	#define DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_GET_PAIRED_DEVICE_HOST_NAME_SUPPORTED 0x04
	#define DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_GET_BATTERY_INDICATOR_SUPPORTED 0x10 
	#define DONGLE_CONNECTED_DEVICE_CAPABILITY_FLAG_NEW_DEVICE_SUPPORTED 0x20 
	#define DONGLE_CONNECTED_DEVICE_CAPABILITY_MOUSE_SMARTPOINTER_SUPPORTED 0x40

	typedef uint8_t (*p_device_id_t)[3];
	typedef uint8_t (*p_device_name_t)[10];
	typedef uint8_t (*p_host_address_t)[4];
	typedef uint8_t (*p_host_id_t)[5];
	typedef uint8_t (*p_challenge_t)[5];
	typedef uint8_t (*p_response_t)[5];
	typedef void (*p_device_paring_status_changed_t)(uint8_t device_paring_status);
	typedef bool (*p_device_send_packet_t)(uint32_t pipe, const uint8_t *payload, uint32_t payload_length);
	typedef void (*p_device_sleep_ms_t)(uint32_t time_ms);
	typedef uint32_t (*p_device_get_uptime_ms_t)();
	typedef void (*p_device_backoff_t)(uint32_t pipe, uint32_t time_ms);
	typedef bool (*p_device_update_host_address_t)(uint8_t (*host_address)[4]);
	typedef void (*p_device_update_host_id_t)(uint8_t (*host_id)[5]);
	typedef p_device_id_t (*p_device_get_device_id_t)();
	typedef p_device_name_t (*p_device_get_device_name_t)();
	typedef uint8_t (*p_device_get_device_type_t)();
	typedef uint8_t (*p_device_get_vendor_id_t)();
	typedef uint32_t (*p_device_get_paring_timout_ms_t)();
	typedef uint16_t (*p_device_get_device_firmware_version_t)();
	typedef uint16_t (*p_device_get_device_capability_t)();
	typedef uint16_t (*p_device_get_device_kb_layout_t)();
	typedef void (*p_device_update_challenge_t)(uint8_t (*challenge)[5]);
	typedef void (*p_device_update_response_t)(uint8_t (*response)[5]);
	typedef p_host_address_t (*p_device_get_host_address_t)();
	typedef p_host_id_t (*p_device_get_host_id_t)();
	typedef p_challenge_t (*p_device_get_challenge_t)();
	typedef p_response_t (*p_device_get_response_t)();
	typedef void (*p_device_dynamic_key_status_changed_t)(uint8_t device_dynamic_key_status);
	typedef uint32_t (*p_device_get_dynamic_key_prepare_timout_ms_t)();
	typedef void (*p_device_data_send_status_t)(bool is_success);
	typedef bool (*p_device_set_settings_t)(uint8_t settings_type, uint8_t* data, uint8_t data_length);
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT	
	typedef uint8_t (*p_device_get_paired_type_t)();
	typedef void (*p_device_update_paired_type_t)(uint8_t paired_type);
#endif

	enum multi_link_proto_device_paring_status
	{
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_UNKNOW,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ERROR,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ADDRESS_REQUEST,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ID_REQUEST,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_DEVICE_INFO_SEND,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_DONE,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_TIMEOUT,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ALREADY_PAIRED,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_REJECTED,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_MAX = 63,
#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_V2_PAIRING_REQUEST=0x11,
		MULTI_LINK_PROTO_DEVICE_PARING_STATUS_V2_KEY_EXCHANGE_FETCH=0x12,
#endif
	};
	enum multi_link_proto_device_dynamic_key_status
	{
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_UNKNOW = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_MAX + 1,
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_ERROR,
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_PREPARE,
#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE_STEP0,
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE_STEP1,
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE_STEP2,
#endif
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE,
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_DEVICE_NOT_PAIRED,
		MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_TIMEOUT,
	};

	enum multi_link_proto_device_send_data_status
	{
		MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE,
		MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING,
		MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_ENCODING_ERROR,
		MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_SEND_PACKET_ERROR,
		MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_NOT_READY_ERROR,
	};

	typedef struct
	{
		p_device_paring_status_changed_t device_paring_status_changed;
		p_device_send_packet_t device_send_packet;
		p_device_sleep_ms_t device_sleep_ms;
		p_device_get_uptime_ms_t device_get_uptime_ms;
		p_device_backoff_t device_backoff;
		p_device_update_host_address_t device_update_host_address;
		p_device_update_host_id_t device_update_host_id;
		p_device_get_device_id_t device_get_device_id;
		p_device_get_device_name_t device_get_device_name;
		p_device_get_device_type_t device_get_device_type;
		p_device_get_vendor_id_t device_get_vendor_id;
		p_device_get_paring_timout_ms_t device_get_paring_timout_ms;
		p_device_get_device_firmware_version_t device_get_device_firmware_version;
		p_device_get_device_capability_t device_get_device_capability;
		p_device_get_device_kb_layout_t device_get_device_kb_layout;
		p_device_update_challenge_t device_update_challenge;
		p_device_update_response_t device_update_response;
		p_device_get_host_address_t device_get_host_address;
		p_device_get_host_id_t device_get_host_id;
		p_device_get_challenge_t device_get_challenge;
		p_device_get_response_t device_get_response;
		p_device_dynamic_key_status_changed_t device_dynamic_key_status_changed;
		p_device_get_dynamic_key_prepare_timout_ms_t device_get_dynamic_key_prepare_timout_ms;
		p_device_data_send_status_t device_data_send_status;
		p_device_set_settings_t device_set_settings;
	#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
		p_device_get_paired_type_t device_get_paired_type;
		p_device_update_paired_type_t device_update_paired_type;
	#endif
	

	} multi_link_proto_device_callbacks_t;

	bool multi_link_proto_device_start(multi_link_common_callbacks_t *multi_link_common_callbacks,
											  multi_link_proto_device_callbacks_t *multi_link_proto_device_callbacks,
											  bool start_with_g3p5);

	void multi_link_proto_device_process_packet(bool is_success, uint32_t pipe, const uint8_t *payload, uint32_t payload_length);

	int multi_link_proto_device_send_data(uint8_t * data, uint32_t data_length);
	bool multi_link_proto_device_is_send_data_ready();
	void multi_link_proto_device_request_dynamic_key();
	bool multi_link_proto_device_key_seed_reflash(void);

#ifdef __cplusplus
}
#endif

#endif /* _MULTI_LINK_PROTO_DEVICE_H_ */
