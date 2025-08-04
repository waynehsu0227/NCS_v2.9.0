/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/sys/reboot.h>
#include <caf/events/power_manager_event.h>
#include <zephyr/drivers/retained_mem.h>
#include "multi_link_setting.h"
#include <zephyr/logging/log.h>

#include <caf/events/click_event.h>
#define MODULE multi_link
#include <caf/events/module_state_event.h>
#include <caf/events/module_suspend_event.h>
#include "test_mode_event.h"
#include "multi_link.h"

#include "multi_link_handler.h"
#if CONFIG_MULTI_LINK_PEER_EVENTS
#include "multi_link_peer_event.h"
#endif

#include "multi_link_crypto.h"
#include "multi_link_load_key.h"
#include "multi_link_basic.h"

#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);

#define GPIO_TEST false

#if GPIO_TEST
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
static const struct device * const gpio_devs[] = {
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio0)),
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio1)),
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio2)),
};
#endif
//============ clock control =============
#define CLOCK_CONTROL_TEST   false

//============ ram retention =============
#if CONFIG_RETAINED_MEM
const static struct device *retained_mem_device0 =	DEVICE_DT_GET(DT_ALIAS(retainedmemdevice0));
#define RAM_PAIRING_OFFSET    0
static uint8_t pairing_buffer[10];

static uint8_t RAM_PAIRING_FLAG[10] = {
	0x23, 0x82, 0xa8, 0x7b, 0xde, 0x18, 0x00, 0xff, 0x8e, 0xd6,
};

void ram_pairing_write(void)
{
	int32_t rc;

	rc = retained_mem_write(retained_mem_device0, RAM_PAIRING_OFFSET, RAM_PAIRING_FLAG, sizeof(RAM_PAIRING_FLAG));
}

void ram_pairing_empty(void)
{
	int32_t rc;

	rc = retained_mem_clear(retained_mem_device0);
}

void ram_pairing_read(void)
{
	int32_t rc;

	memset(pairing_buffer, 0, sizeof(pairing_buffer));

	rc = retained_mem_read(retained_mem_device0, RAM_PAIRING_OFFSET, pairing_buffer, sizeof(pairing_buffer));
}
#endif
//================ Gazell ================
//
// Constant holding base address part of the pairing address.
//
static const uint8_t gazell_host_pairing_base_address[4] = GAZELL_HOST_ADDRESS;

//
// Constant holding prefix byte of the pairing address.
//
static const uint8_t gazell_host_pairing_address_prefix_byte;

extern void fill_callback(multi_link_common_callbacks_t *cb);
/* Gazell Link Layer TX result structure */
struct gzll_tx_result
{
	bool success;
	uint32_t pipe;
	nrf_gzll_device_tx_info_t info;
};

/* Gazell Link Layer TX result message queue */
K_MSGQ_DEFINE(gzll_msgq,
			  sizeof(struct gzll_tx_result),
			  1,
			  sizeof(uint32_t));


static struct k_work gzll_work;
static void gzll_tx_result_handler(struct gzll_tx_result *tx_result);
static void gzll_work_handler(struct k_work *work);

//================ Multi Link ================
#ifdef CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE
uint8_t multi_link_device_id[MULTI_LINK_DEV_ID_WIDTH] = {0xFD, 0xFE, 0xFF};
uint8_t multi_link_device_name[10] = {'M', 'S', '3', '5', '5',  0x00,  0x00, 0x00, 0x00, 0x00};
#if CONFIG_DESKTOP_MOTION_SENSOR_CPI
uint16_t g_current_dpi_value = CONFIG_DESKTOP_MOTION_SENSOR_CPI;
#else
uint16_t g_current_dpi_value = 1200;
#endif
#else
uint8_t multi_link_device_id[MULTI_LINK_DEV_ID_WIDTH] = {0xFB, 0xFE, 0xFF};
uint8_t multi_link_device_name[10] = {'K', 'B', '9', '0', '0',0x00, 0x00, 0x00, 0x00, 0x00};//{'K', 'B', '_', 'G', '3', 0x00, 0x00, 0x00, 0x00, 0x00};
#endif


uint8_t g_host_address[4];
uint8_t g_host_id[5];
uint8_t g_challenge[5];
uint8_t g_response[5];
uint8_t g_pairing;

uint8_t g_packet_id_host_0 = MULTI_LINK_PACKET_DATA_PACKET_ID_MIN;
uint8_t g_packet_id_host_1 = MULTI_LINK_PACKET_DATA_PACKET_ID_MIN;

uint16_t g_current_fw_version;
uint8_t g_current_led_status;

int32_t g_current_ping_interval;
int32_t  g_current_ping_duration;
uint32_t g_last_ping_sent_time;

uint8_t ack_payload[NRF_GZLL_CONST_MAX_PAYLOAD_LENGTH];

/* callbacks for multi link common */
static multi_link_common_callbacks_t multi_link_common_callbacks;
/* callbacks for multi link device */
static multi_link_proto_device_callbacks_t multi_link_proto_device_callbacks;


//================ Darfon ================
#if CONFIG_MULTI_LINK_PEER_EVENTS
static void send_peer_disconnect_led_event(void);
static void send_peer_connect_led_event(void);
static void send_peer_pairing_led_event(void);
#endif
//================ Nordic ================
#define ON_START_CLICK_UPTIME_MAX	(10 * MSEC_PER_SEC) //n * second
/* Interval between subsequent keep alives will be up to twice as big as this define's value. */
#define KEEP_ALIVE_TIMEOUT_MS		(290 / 2) //see 294ms interval in bushound
#define MAX_HID_REPORT_LENGTH		(MAX(REPORT_SIZE_MOUSE + 1, REPORT_SIZE_KEYBOARD_KEYS + 1))

struct sent_hid_report {
	uint8_t packet_cnt;
	uint8_t id;
	bool keep_alive;
	bool error;
};

struct sent_hid_report sent_hid_report = {
	.packet_cnt = 0,
	.id = REPORT_ID_COUNT,
	.keep_alive = false,
	.error = false
};
static enum state state;
static struct k_work_delayable periodic_work;

/* Add an extra byte for HID report ID. */
uint8_t mouse_data[REPORT_SIZE_MOUSE + 1] = {REPORT_ID_MOUSE};
uint8_t keyboard_data[REPORT_SIZE_KEYBOARD_KEYS + 1] = {REPORT_ID_KEYBOARD_KEYS};
#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
uint8_t consumer_package[REPORT_SIZE_CONSUMER_CTRL + 1] = {REPORT_ID_CONSUMER_CTRL};
#endif
#if CONFIG_DPM_ENABLE
uint8_t dpm_input_data[SIZE_DPM_INPUT + 1] = {REPORT_ID_DPM_INPUT};
#endif
#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
#if  (CONFIG_CY25KB_RECHARGEABLE)  
uint8_t battery_level_data[REPORT_SIZE_BATTERY_LEVEL + 2] = {REPORT_ID_BATTERY_LEVEL};
#else
uint8_t battery_level_data[REPORT_SIZE_BATTERY_LEVEL + 1] = {REPORT_ID_BATTERY_LEVEL};
#endif
#endif
static uint8_t queued_report_id = REPORT_ID_COUNT;
static bool keep_alive_needed;
static uint8_t keep_alive_needed_report_id;
static bool pairing_erase_pending;
static bool suspend_pending;
static bool suspended = IS_ENABLED(CONFIG_DESKTOP_GZP_SUSPEND_ON_READY);
static uint8_t g_paired_type=0;
static void sent_cb(bool success);
static void finalize_hid_report_sent(struct sent_hid_report *report);
static void send_hid_report_buffered(uint8_t report_id, bool keep_alive);
static void periodic_work_fn(struct k_work *work);
static int module_init(void);
static bool gazell_state_update(bool enable);
static void broadcast_hid_report_sent(uint8_t report_id, bool error);
static void update_hid_report_data(const uint8_t *data, size_t size);
static bool hid_report_update_needed(uint8_t report_id);
static void update_hid_subscription(bool enable);
#if (CONFIG_CY25KB_RECHARGEABLE)
bool battery_meas_is_ready = false;
#endif
//==========================================================================================
//================================ Gazell ==================================================
//==========================================================================================
static void gazell_device_nrf_disable_gzll(void)
{
	LOG_DBG("%s", __func__);
	if (nrf_gzll_is_enabled())
	{
		unsigned int key = irq_lock();

		nrf_gzll_disable();

		while (nrf_gzll_is_enabled())
		{
			k_cpu_atomic_idle(key);
			key = irq_lock();
		}

		irq_unlock(key);
	}
}

static void gazell_device_generate_channels(uint8_t *ch_dst, const uint8_t *system_address, uint8_t channel_tab_size)
{
	LOG_DBG("%s", __func__);
	uint8_t binsize, spacing, i;

	binsize = (GAZELL_HOST_CHANNEL_MAX - GAZELL_HOST_CHANNEL_MIN) / channel_tab_size;

	ch_dst[0] = GAZELL_HOST_CHANNEL_LOW;
	ch_dst[channel_tab_size - 1] = GAZELL_HOST_CHANNEL_HIGH;

	if (system_address != NULL)
	{
		for (i = 1; i < (channel_tab_size - 1); i++)
		{
			ch_dst[i] = (binsize * i) + (system_address[i % 4] % binsize);
		}
	}

	//
	// If channels are too close, shift them to better positions
	//
	for (i = 1; i < channel_tab_size; i++)
	{
		spacing = (ch_dst[i] - ch_dst[i - 1]);
		if (spacing < GAZELL_HOST_CHANNEL_SPACING_MIN)
		{
			ch_dst[i] += (GAZELL_HOST_CHANNEL_SPACING_MIN - spacing);
		}
	}
}

static bool gazell_device_update_radio_params(const uint8_t *system_address)
{
	LOG_ERR("%s", __func__);
	uint8_t i;
	uint8_t channels[NRF_GZLL_CONST_MAX_CHANNEL_TABLE_SIZE];
	uint32_t channel_table_size;
	uint32_t pairing_base_address_32, system_address_32;
	bool update_ok = true;
	bool gzll_enabled_state;

	//
	// Configure "global" pairing address
	//
	pairing_base_address_32 = (gazell_host_pairing_base_address[0]) +
							  ((uint32_t)gazell_host_pairing_base_address[1] << 8) +
							  ((uint32_t)gazell_host_pairing_base_address[2] << 16) +
							  ((uint32_t)gazell_host_pairing_base_address[3] << 24);
	if (system_address != NULL)
	{
		system_address_32 = (system_address[0]) +
							((uint32_t)system_address[1] << 8) +
							((uint32_t)system_address[2] << 16) +
							((uint32_t)system_address[3] << 24);
	}
	else
	{
		return false;
	}

	gzll_enabled_state = nrf_gzll_is_enabled();

	gazell_device_nrf_disable_gzll();
	update_ok &= nrf_gzll_set_base_address_0(pairing_base_address_32);
	update_ok &= nrf_gzll_set_address_prefix_byte(GAZELL_HOST_PARING_PIPE, gazell_host_pairing_address_prefix_byte);
	update_ok &= nrf_gzll_set_base_address_1(system_address_32);

	//
	// Configure address for pipe 1 - 7. Address byte set to equal pipe number.
	//
	for (i = 1; i < NRF_GZLL_CONST_PIPE_COUNT; i++)
	{
		update_ok &= nrf_gzll_set_address_prefix_byte(i, i);
	}

	channel_table_size = nrf_gzll_get_channel_table_size();
	gazell_device_generate_channels(&channels[0], system_address, channel_table_size);

	//
	// Write generated channel subset to Gazell Link Layer
	//
	update_ok &= nrf_gzll_set_channel_table(&channels[0], channel_table_size);
	if (gzll_enabled_state)
	{
		update_ok &= nrf_gzll_enable();
	}

	return update_ok;
}
uint8_t device_get_paired_type() {
	return g_paired_type;
}

void device_update_paired_type(uint8_t paired_type) {
	g_paired_type = paired_type;
}
static void gzll_device_report_tx(bool success,
								  uint32_t pipe,
								  nrf_gzll_device_tx_info_t *tx_info)
{
	int err;
	struct gzll_tx_result tx_result;

	tx_result.success = success;
	tx_result.pipe = pipe;
	tx_result.info = *tx_info;
	err = k_msgq_put(&gzll_msgq, &tx_result, K_NO_WAIT);
	if (!err)
	{
		/* Get work handler to run */
		k_work_submit(&gzll_work);
	}
	else
	{
		LOG_ERR("Cannot put TX result to message queue");
	}
}
static void device_data_send_status(bool is_success);//bill
void nrf_gzll_device_tx_success(uint32_t pipe, nrf_gzll_device_tx_info_t tx_info)
{
	#if CLOCK_CONTROL_TEST
	nrf_gzll_release_xosc();
	#endif
    if (pipe != GAZELL_HOST_OTA_PIPE)
	{
		gzll_device_report_tx(true, pipe, &tx_info);
	}
	else {
		LOG_INF("%s pip3", __func__);
		extern void gzp_device_tx_success(void);
		gzp_device_tx_success();
	}
	device_data_send_status(true);//bill
}

void nrf_gzll_device_tx_failed(uint32_t pipe, nrf_gzll_device_tx_info_t tx_info)
{
	LOG_DBG("%s", __func__);
    if (pipe != GAZELL_HOST_OTA_PIPE) {
		gzll_device_report_tx(false, pipe, &tx_info);
	} else {
		LOG_INF("%s pip3", __func__);
		extern void gzp_device_tx_failed(void);
		gzp_device_tx_failed();
	}
	device_data_send_status(false);//bill
}

void nrf_gzll_disabled(void)
{
	//LOG_DBG("%s", __func__);
}

void nrf_gzll_host_rx_data_ready(uint32_t pipe, nrf_gzll_host_rx_info_t rx_info)
{
}

static void gzll_work_handler(struct k_work *work)
{
	struct gzll_tx_result tx_result;

	/* Process message queue */
	while (!k_msgq_get(&gzll_msgq, &tx_result, K_NO_WAIT))
	{
		gzll_tx_result_handler(&tx_result);
	}

}

static void gzll_tx_result_handler(struct gzll_tx_result *tx_result)
{
	uint32_t ack_payload_length = NRF_GZLL_CONST_MAX_PAYLOAD_LENGTH;
	bool is_success = tx_result->success;
	if (is_success && tx_result->info.payload_received_in_ack)
	{
		if (nrf_gzll_get_rx_fifo_packet_count(tx_result->pipe) <= 0)
			return;

		is_success = nrf_gzll_fetch_packet_from_rx_fifo(tx_result->pipe,
														ack_payload,
														&ack_payload_length);

		if (!is_success)
		{
			ack_payload_length = 0;
			LOG_ERR("nrf_gzll_fetch_packet_from_rx_fifo failed, pipe = %d", tx_result->pipe);
		}
	}
	else
	{
		ack_payload_length = 0;
	}
   
	multi_link_proto_device_process_packet(is_success, tx_result->pipe, ack_payload, ack_payload_length);

	//Rex add
	if (state == STATE_ACTIVE && (sent_hid_report.packet_cnt > 0))
	{
		sent_cb(is_success);
	}
	return;
}

//==========================================================================================
//================================ Multi Link ==============================================
//==========================================================================================
static void device_paring_status_changed(uint8_t device_paring_status)
{
	LOG_INF("device_paring_status = %d", device_paring_status);

	if (device_paring_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_DONE)
	{
		//
		// Save g_host_address, g_host_id, g_challenge and g_response in device flash
		//
		LOG_HEXDUMP_INF((uint8_t *)g_host_address, sizeof(g_host_address), "g_host_address :");
		LOG_HEXDUMP_INF((uint8_t *)g_host_id, sizeof(g_host_id), "g_host_id :");
		LOG_HEXDUMP_INF((uint8_t *)g_challenge, sizeof(g_challenge), "g_challenge :");
		LOG_HEXDUMP_INF((uint8_t *)g_response, sizeof(g_response), "g_response :");
		//store to flash
		multi_link_params_store();
		power_manager_restrict((size_t)NULL, POWER_MANAGER_LEVEL_OFF); //set power level to power off after power manager timeout
	}
	else if (device_paring_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_TIMEOUT || device_paring_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ERROR)
	{
		sys_reboot(SYS_REBOOT_WARM);
	}
	else if (device_paring_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ADDRESS_REQUEST)
	{
		power_manager_restrict((size_t)NULL, POWER_MANAGER_LEVEL_ALIVE); //keep device alive
	}
	
}

static void device_backoff(uint32_t pipe, uint32_t time_ms)
{
	nrf_gzll_disable();
	while (nrf_gzll_is_enabled())
	{
	}

	nrf_gzll_flush_rx_fifo(pipe);
	nrf_gzll_flush_tx_fifo(pipe);

	multi_link_device_sleep_ms(time_ms);

	nrf_gzll_enable();
}

static bool device_send_packet(uint32_t pipe, const uint8_t *payload, uint32_t payload_length)
{
#if CLOCK_CONTROL_TEST
	nrf_gzll_request_xosc();
#endif
	if (!nrf_gzll_add_packet_to_tx_fifo(pipe, payload, payload_length))
	{
		LOG_ERR("device_send_packet failed! = 0x%x, length = %d, count = %d, error = %d", payload[0], payload_length, nrf_gzll_get_tx_fifo_packet_count(pipe), nrf_gzll_get_error_code());

		return false;
	}

	return true;
}

static bool device_update_host_address(uint8_t (*host_address)[4])
{
    LOG_DBG("%s", __func__);
	if (!gazell_device_update_radio_params(host_address[0]))
	{
		LOG_ERR("gazell_device_update_radio_params failed");

		return false;
	}
	else
	{
		memcpy(g_host_address, &host_address[0], sizeof(g_host_address));
	}

	return true;
}


static void device_dynamic_key_status_changed(uint8_t device_dynamic_key_status)
{
	LOG_DBG("device_dynamic_key_status = %d", device_dynamic_key_status);

	//
	// Note : For demo testing send all value again after dynamic key lost. Real device should optimize only send when needed
	//
	g_packet_id_host_0 = MULTI_LINK_PACKET_DATA_PACKET_ID_MIN;
	g_packet_id_host_1 = MULTI_LINK_PACKET_DATA_PACKET_ID_MIN;


	//
	// In this callback only allow to call multi_link_proto_device_request_dynamic_key. Do not call an other APIs
	//

	if (device_dynamic_key_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_ERROR ||
		device_dynamic_key_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_TIMEOUT)
	{
		//
		// Start dynamic key request
		//
		multi_link_proto_device_request_dynamic_key();
	}
	else if (device_dynamic_key_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_DEVICE_NOT_PAIRED)
	{
#if CONFIG_MULTI_LINK_PEER_EVENTS
		send_peer_connect_led_event();
#endif
		LOG_DBG("Device is not paired do not try or send any data. All should stoped");
	}
	else if(device_dynamic_key_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE)
	{
#if CONFIG_MULTI_LINK_PEER_EVENTS
		send_peer_connect_led_event();
#endif
		state = STATE_ACTIVE;
		if (pairing_erase_pending || suspend_pending) {
			(void)k_work_reschedule(&periodic_work, K_NO_WAIT);
		}
		else
		{
			(void)k_work_reschedule(&periodic_work, K_MSEC(KEEP_ALIVE_TIMEOUT_MS));
#ifdef CONFIG_DPM_ENABLE
			multi_link_send_info_step_work_init();
			multi_link_send_info_step_work_submit(500);
#endif
		}
	}
}

bool multi_link_send_disconnect=false;//bill smart connect v2.0
bool is_multi_link_tx_success=false;
static void device_data_send_status(bool is_success)
{
	LOG_DBG("%s", __func__);
	//
	// Do not call any API from this function.
	//
	if (is_success)
	{
		//bill smart connect v2.0
		is_multi_link_tx_success=true;
		//bill smart connect v2.0
		multi_link_send_disconnect=false;//bill smart connect v2.0
	}
	else
	{
		//bill smart connect v2.0
		is_multi_link_tx_success=false;
		if(multi_link_send_disconnect==false)
		{
			multi_link_send_disconnect=true;
			struct multi_link_peer_event *event = new_multi_link_peer_event();
			event->state = MULTI_LINK_STATE_DISCONNECTED;
			APP_EVENT_SUBMIT(event);
		}
		//bill smart connect v2.0
		LOG_ERR("data sent failed");
		//
		// Sending last data failed after all retry device automaticaly go in dynamic key request again
		//
	}
}

static bool multi_link_init(void)
{
	LOG_DBG("%s", __func__);
	bool result_value;

	//multi_link_handler_init();

	//k_work_init(&gzll_work, gzll_work_handler);

	/* Initialize Gazell Link Layer glue */
	result_value = gzll_glue_init();
	if (!result_value)
	{
		LOG_ERR("Cannot initialize GZLL glue code");
		return false;
	}

	/* Initialize the Gazell Link Layer */
	result_value = nrf_gzll_init(NRF_GZLL_MODE_DEVICE);
	if (!result_value)
	{
		LOG_ERR("Cannot initialize GZLL");
		return false;
	}

	// CHANGES: Use 600 time slot period for new devices
	result_value = nrf_gzll_set_timeslot_period(NRF_GZLL_DEFAULT_TIMESLOT_PERIOD);
	if (!result_value)
	{
		LOG_ERR("nrf_gzll_set_timeslot_period failed. Error : %d", nrf_gzll_get_error_code());
		return false;
	}

	//
	// Set up radio parameters (addresses and channel subset) from system_address
	// For this example we do not store system address so pass any but this should follow original old flow in device (gzp_update_radio_params)
	// 
	uint8_t system_address[GAZELL_HOST_SYSTEM_ADDRESS_WIDTH] = {4, 5, 7, 11};
	result_value = gazell_device_update_radio_params(system_address);
	if (!result_value)
	{
		LOG_ERR("gazell_device_update_radio_params failed");
		return false;
	}
	
	result_value = nrf_gzll_set_device_channel_selection_policy(NRF_GZLL_DEVICE_CHANNEL_SELECTION_POLICY_USE_CURRENT);
	if (!result_value)
	{
		LOG_ERR("nrf_gzll_set_device_channel_selection_policy failed");
		return false;
	}

	result_value = nrf_gzll_set_sync_lifetime(20);
	if (!result_value)
	{
		LOG_ERR("nrf_gzll_set_sync_lifetime failed");
		return false;
	}


	nrf_gzll_set_max_tx_attempts(MAX_TX_ATTEMPTS);

	

	/* ----- must call for this example for Zephyr's based crypto --- */
	//
	// Initialize PSA Crypto
	//
	psa_status_t status = psa_crypto_init();
	if (status != PSA_SUCCESS)
	{
		LOG_ERR("psa_crypto_init failed, status = %d", status);
		return false;
	}

	/* --------------------------------------- */

	result_value = nrf_gzll_enable();
	if (!result_value)
	{
		LOG_ERR("Cannot enable GZLL");
		return false;
	}
	/* --------------------------------------- */

	LOG_INF("Dell Dongle device example started.");

	/* ----- must call to start paring for new device for paring --- */
	//
	// All callback must required
	//
	//multi_link_common_callbacks.generate_random = &multi_link_generate_random;
	//multi_link_common_callbacks.hmac_sha_256 = &multi_link_hmac_sha_256;
	//multi_link_common_callbacks.ecb_128 = &multi_link_ecb_128;
	//multi_link_common_callbacks.load_key = &multi_link_load_key;

    fill_callback(&multi_link_common_callbacks);

	multi_link_proto_device_callbacks.device_paring_status_changed = &device_paring_status_changed;
	multi_link_proto_device_callbacks.device_send_packet = &device_send_packet;
	multi_link_proto_device_callbacks.device_sleep_ms = &multi_link_device_sleep_ms;
	multi_link_proto_device_callbacks.device_get_uptime_ms = &multi_link_device_get_uptime_ms;
	multi_link_proto_device_callbacks.device_backoff = &device_backoff;
	multi_link_proto_device_callbacks.device_update_host_address = &device_update_host_address;
	multi_link_proto_device_callbacks.device_update_host_id = &multi_link_device_update_host_id;
	multi_link_proto_device_callbacks.device_get_device_id = &multi_link_device_get_device_id;
	multi_link_proto_device_callbacks.device_get_device_name = &multi_link_device_get_device_name;
	multi_link_proto_device_callbacks.device_get_device_type = &multi_link_device_get_device_type;
	multi_link_proto_device_callbacks.device_get_vendor_id = &multi_link_device_get_vendor_id;
	multi_link_proto_device_callbacks.device_get_paring_timout_ms = &multi_link_device_get_paring_timout_ms;
	multi_link_proto_device_callbacks.device_get_device_firmware_version = &multi_link_device_get_device_firmware_version;
	multi_link_proto_device_callbacks.device_get_device_capability = &multi_link_device_get_device_capability;
	multi_link_proto_device_callbacks.device_get_device_kb_layout = &multi_link_device_get_device_kb_layout;
	multi_link_proto_device_callbacks.device_update_challenge = &multi_link_device_update_challenge;
	multi_link_proto_device_callbacks.device_update_response = &multi_link_device_update_response;
	multi_link_proto_device_callbacks.device_get_host_address = &multi_link_device_get_host_address;
	multi_link_proto_device_callbacks.device_get_host_id = &multi_link_device_get_host_id;
	multi_link_proto_device_callbacks.device_get_challenge = &multi_link_device_get_challenge;
	multi_link_proto_device_callbacks.device_get_response = &multi_link_device_get_response;
	multi_link_proto_device_callbacks.device_dynamic_key_status_changed = &device_dynamic_key_status_changed;
	multi_link_proto_device_callbacks.device_get_dynamic_key_prepare_timout_ms = &multi_link_device_get_dynamic_key_prepare_timout_ms;
	multi_link_proto_device_callbacks.device_data_send_status = &device_data_send_status;
	multi_link_proto_device_callbacks.device_set_settings = &multi_link_device_set_settings;
	
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
	multi_link_proto_device_callbacks.device_get_paired_type = &device_get_paired_type;
	multi_link_proto_device_callbacks.device_update_paired_type = &device_update_paired_type;
#endif
	//
	// Start all. Must be called after all Gazell setup done and nrf_gzll_enable
	// Depend on device state will start paring or if device provides host_address, host_id, challenge and response, it will start dynamic key request
	//
	return true;
}

//==========================================================================================
//=================================== Darfon ===============================================
//==========================================================================================
#if CONFIG_MULTI_LINK_PEER_EVENTS
static void send_peer_disconnect_led_event(void)
{
	struct multi_link_peer_event *event = new_multi_link_peer_event();
	event->state = MULTI_LINK_STATE_DISCONNECTED;
	APP_EVENT_SUBMIT(event);
}

static void send_peer_connect_led_event(void)
{
	struct multi_link_peer_event *event = new_multi_link_peer_event();
	event->state = MULTI_LINK_STATE_CONNECTED;
	APP_EVENT_SUBMIT(event);
}

static void send_peer_pairing_led_event(void)
{
	struct multi_link_peer_event *event = new_multi_link_peer_event();
	event->state = MULTI_LINK_STATE_PAIRING;
	APP_EVENT_SUBMIT(event);
}
#endif

static bool multi_link_erase_pairing_data(void)
{
	memset(g_host_address, 0x00, sizeof(g_host_address));
	memset(g_host_id, 0x00, sizeof(g_host_id));
	memset(g_challenge, 0x00, sizeof(g_challenge));
	memset(g_response, 0x00, sizeof(g_response));

	return true;		
}

static int8_t multi_link_get_pairing_status(void)
{
	LOG_HEXDUMP_INF((uint8_t *)g_host_address, sizeof(g_host_address), "g_host_address :");
	LOG_HEXDUMP_INF((uint8_t *)g_host_id, sizeof(g_host_id), "g_host_id :");
	LOG_HEXDUMP_INF((uint8_t *)g_challenge, sizeof(g_challenge), "g_challenge :");
	LOG_HEXDUMP_INF((uint8_t *)g_response, sizeof(g_response), "g_response :");
	LOG_HEXDUMP_INF((uint8_t *)&g_pairing, sizeof(g_pairing), "g_pairing :");
#if CONFIG_RETAINED_MEM
	if(memcmp(pairing_buffer ,RAM_PAIRING_FLAG ,sizeof(pairing_buffer)) == 0)
	{
		multi_link_erase_pairing_data();
		ram_pairing_empty();
	}
#else
	if(g_pairing == MULTI_LINK_PAIRING_FLAG)
	{
		g_pairing = 0xFF; 
		multi_link_pairing_store();//erase flag. 
		multi_link_erase_pairing_data();
	}
#endif
	int8_t db_index;
    if (!multi_link_proto_common_check_array_is_empty((const uint8_t *)&g_host_address[0], sizeof(g_host_address[0])) &&
        !multi_link_proto_common_check_array_is_empty((const uint8_t *)&g_host_id[0], sizeof(g_host_id[0])) &&
        !multi_link_proto_common_check_array_is_empty((const uint8_t *)&g_challenge[0], sizeof(g_challenge[0])) &&
        !multi_link_proto_common_check_array_is_empty((const uint8_t *)&g_response[0], sizeof(g_response[0])))
    {
    	db_index = 0;//not empty
    }
    else
    {
    	db_index = -2; //empty
    }
    return db_index;
}

static enum state multi_link_get_pairing_module_state(void)
{
	LOG_DBG("%s", __func__);
	enum state ret;
	int8_t pairing_status = multi_link_get_pairing_status();

	if (pairing_status == -2) {
		LOG_ERR("Pairing database is empty");
		ret = STATE_GET_SYSTEM_ADDRESS;
	} else if (pairing_status >= 0) {
		LOG_ERR("Has system address ,Host ID ,challeng and response");
		ret = STATE_ACTIVE;
	} else {
		LOG_ERR("multi_link_get_pairing_status returned unhandled value: %" PRIi8, pairing_status);
		ret = STATE_ERROR;
	}

	return ret;
}

static void multi_link_pairing_start(void)
{
	LOG_DBG("%s", __func__);
	__ASSERT_NO_MSG(!k_is_in_isr());
	__ASSERT_NO_MSG(!k_is_preempt_thread());

	__ASSERT_NO_MSG(state == STATE_GET_SYSTEM_ADDRESS);
#if CONFIG_MULTI_LINK_PEER_EVENTS
	send_peer_pairing_led_event();
#endif
    if (!multi_link_proto_device_start(&multi_link_common_callbacks,&multi_link_proto_device_callbacks, 1))
   	{
        LOG_ERR("multi_link_pairing_start failed");
		/* Retry after some time. */
		(void)k_work_reschedule(&periodic_work, K_MSEC(500));
    }
}



static void multi_link_pairing_erase(void)
{
	__ASSERT_NO_MSG(sent_hid_report.packet_cnt == 0);

	multi_link_erase_pairing_data();

	if (multi_link_get_pairing_status() == -2) {
		LOG_INF("Erased Gazell pairing data");
		state = STATE_GET_SYSTEM_ADDRESS;
	} else {
		LOG_ERR("Failed to erase Gazell pairing data");
		state = STATE_ERROR;
	}

}

void multilink_send_kbd_leds_report(uint8_t led_status)
{
	LOG_ERR("%s", __func__);
	struct hid_report_event *event = new_hid_report_event(2);

	event->source = &state;
	/* Subscriber is not specified for HID output report. */
	event->subscriber = NULL;
	event->dyndata.data[0] = REPORT_ID_KEYBOARD_LEDS;
	memcpy(&event->dyndata.data[1], &led_status, 1);

	APP_EVENT_SUBMIT(event);
}

//==========================================================================================
//=================================== HID ==================================================
//==========================================================================================
static void keep_alive_send_dummy_report(void)
{
	/* A Gazell nRF Desktop Device may be either HID mouse or HID keyboard, but not both. */
	BUILD_ASSERT(IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_MOUSE_SUPPORT) !=
		     IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_KEYBOARD_SUPPORT));

	send_hid_report_buffered(keep_alive_needed_report_id, true);
}

static void keep_alive(void)
{
	if (sent_hid_report.packet_cnt > 0) {
		/* Cannot send at this point, the work will rerun later. */
	} else if (keep_alive_needed) {
		keep_alive_send_dummy_report();
	}

	keep_alive_needed = (hid_report_update_needed(REPORT_ID_MOUSE) ||
			     hid_report_update_needed(REPORT_ID_KEYBOARD_KEYS) ||
				 hid_report_update_needed(REPORT_ID_CONSUMER_CTRL));
				 //hid_report_update_needed(REPORT_ID_DPM_INPUT));

	(void)k_work_reschedule(&periodic_work, K_MSEC(KEEP_ALIVE_TIMEOUT_MS));
}
static void sent_cb(bool success)
{
	LOG_DBG("%s", __func__);
	__ASSERT_NO_MSG(!k_is_in_isr());
	__ASSERT_NO_MSG(!k_is_preempt_thread());

	struct sent_hid_report *report = &sent_hid_report;

	report->error = (report->error || !success);
	__ASSERT_NO_MSG(report->packet_cnt > 0);
	report->packet_cnt--;

	if (report->packet_cnt == 0) {
		if (report->error) {
			if (report->keep_alive) {
				LOG_ERR("Keep alive transmission failed");
			} else {
				LOG_ERR("sent_cb reported failure for report ID: %" PRIu8,
					report->id);
			}
		}

		finalize_hid_report_sent(report);
		keep_alive_needed = false;
		/* Send subsequent HID report as quickly as possible. */
		if (queued_report_id != REPORT_ID_COUNT) {
			send_hid_report_buffered(queued_report_id, false);
			queued_report_id = REPORT_ID_COUNT;
		}

		if (pairing_erase_pending || suspend_pending) {
			(void)k_work_reschedule(&periodic_work, K_NO_WAIT);
		}
	}
}

static bool hid_report_event_handler(const struct hid_report_event *event)
{
	LOG_ERR("%s", __func__);
	__ASSERT_NO_MSG(!k_is_in_isr());
	__ASSERT_NO_MSG(!k_is_preempt_thread());

	uint8_t report_id = event->dyndata.data[0];

	if (&state != event->subscriber) {
		/* It's not us */
		return false;
	}

	if ((state != STATE_ACTIVE) || pairing_erase_pending || suspend_pending || suspended) {
		/* Gazell is not active, report error. */
		broadcast_hid_report_sent(report_id, true);
		return false;
	}

	__ASSERT_NO_MSG((sent_hid_report.packet_cnt == 0) || (sent_hid_report.keep_alive));

	/* Buffer HID report internally. */
	update_hid_report_data(event->dyndata.data, event->dyndata.size);

	if (sent_hid_report.packet_cnt == 0) {
		send_hid_report_buffered(report_id, false);
	} else {
		queued_report_id = report_id;
	}
	return false;
}

static void broadcast_hid_subscriber(bool enable)
{
	LOG_DBG("%s", __func__);
	struct hid_report_subscriber_event *event = new_hid_report_subscriber_event();

	event->subscriber = &state;
	event->params.pipeline_size = 1;
	event->params.priority = CONFIG_DESKTOP_GZP_SUBSCRIBER_PRIORITY;
	event->params.report_max = 1;
	event->connected = enable;

	APP_EVENT_SUBMIT(event);
}

static void broadcast_hid_subscription(bool enable)
{
	LOG_DBG("%s", __func__);
	if (IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_MOUSE_SUPPORT)) {
		struct hid_report_subscription_event *event = new_hid_report_subscription_event();

		event->report_id  = REPORT_ID_MOUSE;
		event->enabled    = enable;
		event->subscriber = &state;

		APP_EVENT_SUBMIT(event);
	}

	if (IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_KEYBOARD_SUPPORT)) {
		struct hid_report_subscription_event *event = new_hid_report_subscription_event();

		event->report_id  = REPORT_ID_KEYBOARD_KEYS;
		event->enabled    = enable;
		event->subscriber = &state;

		APP_EVENT_SUBMIT(event);
	}

	if (IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT)) {
		struct hid_report_subscription_event *event = new_hid_report_subscription_event();

		event->report_id  = REPORT_ID_CONSUMER_CTRL;
		event->enabled    = enable;
		event->subscriber = &state;

		APP_EVENT_SUBMIT(event);
	}
#if CONFIG_DPM_ENABLE
	if (IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_KEYBOARD_SUPPORT )) {
		struct hid_report_subscription_event *event = new_hid_report_subscription_event();

		event->report_id  = REPORT_ID_DPM_INPUT;
		event->enabled    = enable;
		event->subscriber = &state;

		APP_EVENT_SUBMIT(event);
	}
#endif
}

static void broadcast_hid_report_sent(uint8_t report_id, bool error)
{
	LOG_DBG("%s", __func__);
	__ASSERT_NO_MSG(report_id != REPORT_ID_COUNT);

	struct hid_report_sent_event *event = new_hid_report_sent_event();

	event->report_id = report_id;
	event->subscriber = &state;
	event->error = error;

	APP_EVENT_SUBMIT(event);
}

static void update_hid_report_data(const uint8_t *data, size_t size)
{
	LOG_DBG("%s", __func__);
	uint8_t report_id = data[0];

	switch (report_id) {
	case REPORT_ID_MOUSE:
		__ASSERT_NO_MSG(sizeof(mouse_data) == size);
		memcpy(mouse_data, data, size);
		break;

	case REPORT_ID_KEYBOARD_KEYS:
		__ASSERT_NO_MSG(sizeof(keyboard_data) == size);
		memcpy(keyboard_data, data, size);
		break;
#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
	case REPORT_ID_CONSUMER_CTRL:
		memcpy(consumer_package, data, sizeof(consumer_package));
		break;
#endif
#if CONFIG_DPM_ENABLE
	case REPORT_ID_DPM_INPUT:
		memcpy(dpm_input_data, data, sizeof(dpm_input_data));
		break;
#endif
	default:
		LOG_ERR("Unhandled report %" PRIu8, report_id);
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
		break;
	}
}

static void clear_hid_report_data(uint8_t report_id, bool relative_only)
{
	LOG_DBG("%s", __func__);
	switch (report_id) {
	case REPORT_ID_MOUSE:
		if (relative_only) {
			/* Clear mouse wheel. */
			mouse_data[2] = 0x00;
			/* Clear mouse motion. */
			memset(&mouse_data[3], 0x00, 3);
		} else {
			/* Leave only report ID. */
			memset(&mouse_data[1], 0x00, sizeof(mouse_data) - 1);
		}
		break;

	case REPORT_ID_KEYBOARD_KEYS:
		if (relative_only) {
			/* Nothing to do. */
		} else {
			/* Leave only report ID. */
			memset(&keyboard_data[1], 0x00, sizeof(keyboard_data) - 1);
		}
		break;
#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
	case REPORT_ID_CONSUMER_CTRL:
		if (relative_only) {
			/* Nothing to do. */
		} else {
			/* Leave only report ID. */
			memset(&consumer_package[1], 0x00, sizeof(consumer_package) - 1);
		}
		break;
#endif
#if CONFIG_DPM_ENABLE
	case REPORT_ID_DPM_INPUT:
		if (relative_only) {
			/* Nothing to do. */
		} else {
			/* Leave only report ID. */
			memset(&dpm_input_data[1], 0x00, sizeof(dpm_input_data) - 1);
		}
		break;
#endif

	default:
		LOG_ERR("Unhandled report %" PRIu8, report_id);
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
		break;
	}
}

static bool hid_report_update_needed(uint8_t report_id)
{
	uint8_t zeros[MAX_HID_REPORT_LENGTH];
	bool update_needed = false;

	memset(zeros, 0x00, sizeof(zeros));

	switch (report_id) {
	case REPORT_ID_MOUSE:
		__ASSERT_NO_MSG((sizeof(mouse_data) - 1) <= sizeof(zeros));
		/* Omit HID report ID in the comparison. */
		update_needed = (memcmp(zeros, &mouse_data[1], sizeof(mouse_data) - 1) != 0);
		break;

	case REPORT_ID_KEYBOARD_KEYS:
		__ASSERT_NO_MSG((sizeof(keyboard_data) - 1) <= sizeof(zeros));
		/* Omit HID report ID in the comparison. */
		update_needed = (memcmp(zeros, &keyboard_data[1], sizeof(keyboard_data) - 1) != 0);
		break;
#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT	
	case REPORT_ID_CONSUMER_CTRL:
		__ASSERT_NO_MSG((sizeof(consumer_package) - 1) <= sizeof(zeros));
		/* Omit HID report ID in the comparison. */
		update_needed = (memcmp(zeros, &consumer_package[1], sizeof(consumer_package) - 1) != 0);
		break;
#endif
#if CONFIG_DPM_ENABLE
	case REPORT_ID_DPM_INPUT:
		__ASSERT_NO_MSG((sizeof(dpm_input_data) - 1) <= sizeof(zeros));
		/* Omit HID report ID in the comparison. */
		update_needed = (memcmp(zeros, &dpm_input_data[1], sizeof(dpm_input_data) - 1) != 0);
		break;
#endif
	default:
		LOG_ERR("Unhandled report %" PRIu8, report_id);
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
		break;
	}
	if(update_needed)
	{
		keep_alive_needed_report_id = report_id;
	}

	return update_needed;
}

static void update_hid_subscription(bool enable)
{
	LOG_DBG("%s", __func__);
	if (enable) {
		broadcast_hid_subscriber(true);
		broadcast_hid_subscription(true);
	} else {
		broadcast_hid_subscription(false);
		broadcast_hid_subscriber(false);
		/* Clear remembered report data. */
		if (IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_MOUSE_SUPPORT)) {
			clear_hid_report_data(REPORT_ID_MOUSE, false);
		}
		if (IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_MOUSE_SUPPORT)) {
			clear_hid_report_data(REPORT_ID_KEYBOARD_KEYS, false);
		}
		if (IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT)) {
			clear_hid_report_data(REPORT_ID_CONSUMER_CTRL, false);
		}
#if CONFIG_DPM_ENABLE
		if (IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_KEYBOARD_SUPPORT)) {
			clear_hid_report_data(REPORT_ID_DPM_INPUT, false);
		}
#endif
	}
}
static void finalize_hid_report_sent(struct sent_hid_report *report)
{
	LOG_DBG("%s", __func__);
	__ASSERT_NO_MSG(report->packet_cnt == 0);

	if (!report->keep_alive) {
		broadcast_hid_report_sent(report->id, report->error);
		/* Clear all data on error to align with HID state's approach. */
		clear_hid_report_data(report->id, !report->error);
	}

	/* Clear internal state. */
	report->id = REPORT_ID_COUNT;
	report->error = false;
	report->keep_alive = false;
}



static bool send_hid_report_multi_link(const uint8_t *data, size_t size, uint8_t *packet_cnt)
{
	LOG_DBG("%s", __func__);
	bool success = true;

#if CONFIG_RING_BUFFER	
	add_data_to_queue(data ,size);
#else
	uint8_t report_id = data[0];

	switch (report_id) {
	case REPORT_ID_MOUSE:
		success = send_hid_report_multi_link_mouse(data, size, packet_cnt);
		break;
	case REPORT_ID_KEYBOARD_KEYS:
		success = send_hid_report_multi_link_keyboard_keys(data, size);
		break;
#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
	case REPORT_ID_CONSUMER_CTRL:
		success = send_hid_report_multi_link_consumer_keys(data, size);
		break;
#endif
	case REPORT_ID_DPM_INPUT:
		success = send_hid_report_multi_link_dpm_input(data, size);
		break;

	default:
		LOG_ERR("Unhandled report %" PRIu8, report_id);
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
		break;
	}
#endif
	return success;
}


static void send_hid_report(const uint8_t *data, size_t size, bool keep_alive)
{
	LOG_DBG("%s", __func__);
	__ASSERT_NO_MSG(data && (size > 0));

	uint8_t report_id = data[0];
	struct sent_hid_report *report = &sent_hid_report;

	__ASSERT_NO_MSG(report->packet_cnt == 0);
	__ASSERT_NO_MSG((report->id == REPORT_ID_COUNT) && !report->keep_alive && !report->error);

	/* Packet counter needs to be increased further if a HID report is sent as multiple
	 * Gazell packets.
	 */
	report->packet_cnt = 1;
	report->keep_alive = keep_alive;
	report->id = report_id;

	bool success = false;
	success = send_hid_report_multi_link(data, size, &report->packet_cnt);

	if (!success) {
		/* Instantly propagate information about Gazell transmission error only in case
		 * there are no packets queued to be sent. Otherwise, the information will be
		 * propagated as soon as the queued packets are sent.
		 */
		report->error = !success;
		if (report->packet_cnt == 0) {
			finalize_hid_report_sent(report);
		}

		if (keep_alive) {
			LOG_ERR("Failed to send keep alive");
		} else {
			LOG_ERR("Failed to send HID report (ID: %" PRIu8, report->id);
		}
	}
}

static void send_hid_report_buffered(uint8_t report_id, bool keep_alive)
{
	LOG_DBG("%s", __func__);
	const uint8_t *data;
	size_t size;

	switch (report_id) {
	case REPORT_ID_MOUSE:
		data = mouse_data;
		size = sizeof(mouse_data);
		break;

	case REPORT_ID_KEYBOARD_KEYS:
		data = keyboard_data;
		size = sizeof(keyboard_data);
		break;
#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
	case REPORT_ID_CONSUMER_CTRL:
		data = consumer_package;
		size = sizeof(consumer_package);
		break;
#endif
#if CONFIG_DPM_ENABLE
	case REPORT_ID_DPM_INPUT:
		data = dpm_input_data;
		size = sizeof(dpm_input_data);
		break;
#endif
	default:
		LOG_ERR("Unhandled report %" PRIu8, report_id);
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
		return;
	}

	send_hid_report(data, size, keep_alive);
}

//==========================================================================================
//================================ Module state ============================================
//==========================================================================================
static bool gazell_state_update(bool enable)
{
	LOG_DBG("%s", __func__);
	if (enable) {
		if (!gzll_glue_init()) {
			LOG_ERR("gzll_glue_uninit failed");
			return false;
		}

		if (!nrf_gzll_enable()) {
			LOG_ERR("nrf_gzll_enable failed");
			return false;
		};
	} else {
		nrf_gzll_disable();
        while (nrf_gzll_is_enabled())
        {
        }
		if (!gzll_glue_uninit()) {
			LOG_ERR("gzll_glue_uninit failed");
			return false;
		}
        
	}

	return true;
}

static void periodic_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	__ASSERT_NO_MSG(!k_is_in_isr());
	__ASSERT_NO_MSG(!k_is_preempt_thread());

	if (pairing_erase_pending) {
		multi_link_pairing_erase();
		pairing_erase_pending = false;

		if (state == STATE_ERROR) {
			return;
		}
		gazell_state_update(true);
		update_hid_subscription(true);
	}

	if (suspend_pending) {
		suspend_pending = false;
		suspended = true;
		gazell_state_update(false);
		module_set_state(MODULE_STATE_SUSPENDED);
	}

	/* Do not perform further actions if suspended. */
	if (suspended) {
		return;
	}

	if (state == STATE_ACTIVE) {
		keep_alive();
	} else if (state == STATE_GET_SYSTEM_ADDRESS) {
		multi_link_pairing_start();
	}
}

static int module_init(void)
{
	LOG_DBG("%s", __func__);
#if CONFIG_RETAINED_MEM
	ram_pairing_read();
#endif
	if (suspended || suspend_pending) {
		return 0;
	}
#if CONFIG_RING_BUFFER
    my_task_queue_init();
#endif
	k_work_init_delayable(&periodic_work, periodic_work_fn);
	k_work_init(&gzll_work, gzll_work_handler);
	//initial gzll and multi link
	if(!multi_link_init())
	{
		return -EIO;
	}

	if (pairing_erase_pending) {
		periodic_work_fn(NULL);
		return 0;
	}

    state = multi_link_get_pairing_module_state();
    
    int res = 0;
 	switch (state) {
	case STATE_GET_SYSTEM_ADDRESS:
		update_hid_subscription(true);
		/* The work finalizes initialization. */
		(void)k_work_reschedule(&periodic_work, K_NO_WAIT);
		break;
	case STATE_ACTIVE:
	    update_hid_subscription(true);
		/* The work finalizes initialization. */
    	if (!multi_link_proto_device_start(&multi_link_common_callbacks,&multi_link_proto_device_callbacks, 1))
   		{
        	LOG_ERR("multi_link_proto_device_start failed");
			/* Retry after some time. */
			(void)k_work_reschedule(&periodic_work, K_MSEC(500));
    	} else {
			(void)k_work_reschedule(&periodic_work, K_MSEC(KEEP_ALIVE_TIMEOUT_MS));
    	}
		break;
	case STATE_ERROR:
		/* Propagate information about an error. */
		res = -EIO;
		break;

	default:
		/* Should not happen. */
		__ASSERT_NO_MSG(false);
		break;
	}   
    LOG_ERR("module_init state :%d ,res :%d",state ,res);
	return res;
}

static bool module_state_event_handler(const struct module_state_event *event)
{
	LOG_DBG("%s", __func__);
	if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) 
	{		
		//this must be called before settings_loader module, otherwise, it will crash.
		multi_link_get_pairing_setting(g_host_address,
                                	   g_host_id,
                                       g_challenge,
                                       g_response,
									   &g_pairing);
	}
	if (check_state(event, MODULE_ID(settings_loader), MODULE_STATE_READY) &&
	    (state == STATE_DISABLED)) {

		int err = module_init();

		if (err) {
			module_set_state(MODULE_STATE_ERROR);
		} else {
			module_set_state(MODULE_STATE_READY);
			if (suspended) {
				module_set_state(MODULE_STATE_SUSPENDED);
			}
		}
	}
#if (CONFIG_CY25KB_RECHARGEABLE)
	if (check_state(event, MODULE_ID(battery_meas), MODULE_STATE_READY)) 
	{		
		battery_meas_is_ready = true;
	}
#endif
	return false;
}

static void handle_click_long(void)
{
	LOG_ERR("%s", __func__);
	//not finish. need to change to fn + rf key
	if(multi_link_get_pairing_status() == 0)//empty
	{
#if CONFIG_RETAINED_MEM
		ram_pairing_write();
		k_sleep(K_MSEC(10));
		sys_reboot(SYS_REBOOT_WARM);
#else	
		g_pairing = MULTI_LINK_PAIRING_FLAG;
		if(multi_link_pairing_store())
		{
			k_sleep(K_MSEC(10));
			sys_reboot(SYS_REBOOT_WARM);		
		}
#endif
	}	
}

static bool click_event_handler(const struct click_event *event)
{
	LOG_ERR("%s", __func__);
	#if 1
	if (event->key_id != CONFIG_MULTILINK_BLE_SELECTOR_BUTTON &&
		event->key_id != CONFIG_MULTILINK_MODE_SELECTOR_BUTTON) {
		return false;
	}
	#endif
	if ((k_uptime_get() < ON_START_CLICK_UPTIME_MAX)) {
		LOG_INF("Pairing data erase button pressed too early");
		return false;
	}

    if (event->click == CLICK_LONG) {
        handle_click_long();
    }
	return false;
}
//==========================================================================================
//================================ Suspend and Resume ======================================
//==========================================================================================
static bool handle_module_suspend_req_event(const struct module_suspend_req_event *event)
{
	LOG_DBG("%s", __func__);
    if (event->module_id != MODULE_ID(multi_link)) {
		/* Not us. */
		return false;
	}
	if (!suspended && !suspend_pending) {
		update_hid_subscription(false);
		suspend_pending = true;
#if CONFIG_MULTI_LINK_PEER_EVENTS
		send_peer_disconnect_led_event();
#endif
		if ((state == STATE_ACTIVE) && (sent_hid_report.packet_cnt == 0)) {
			/* Finalize suspend. */
			periodic_work_fn(NULL);
		} else if (state == STATE_ACTIVE) {
			/* Make sure that work would not be triggered before suspend is done. */
			(void)k_work_cancel_delayable(&periodic_work);
		} else {
			/* Suspend will be finalized on system address (or Host ID) get retry or
			 * during module's initialization.
			 */
		}
	}

	return false;
}

static bool handle_module_resume_req_event(const struct module_resume_req_event *event)
{
	LOG_DBG("%s", __func__);
	if (event->module_id != MODULE_ID(multi_link)) {
		/* Not us. */
		return false;
	}

	int err = 0;

	if (suspend_pending) {
		/* TODO: Improve. */
		LOG_ERR("Cannot resume while suspending. Event ignored");
		return false;
	}

	if (suspended) {
		suspended = false;
#if CONFIG_MULTI_LINK_PEER_EVENTS
		send_peer_connect_led_event();
#endif
		if (state == STATE_DISABLED) {
			err = module_init();
		} else if ((state == STATE_GET_HOST_ID) || (state == STATE_GET_SYSTEM_ADDRESS)) {
			gazell_state_update(true);
			/* The work finalizes initialization. */
			(void)k_work_reschedule(&periodic_work, K_NO_WAIT);
		} else if (state == STATE_ACTIVE) {
			gazell_state_update(true);
			update_hid_subscription(true);
			(void)k_work_reschedule(&periodic_work, K_MSEC(KEEP_ALIVE_TIMEOUT_MS));
		}

		if (err) {
			LOG_ERR("Resume failed");
			module_set_state(MODULE_STATE_ERROR);
		} else {
			module_set_state(MODULE_STATE_READY);
		}
	}

	return false;
}
//==========================================================================================
//================================ Test mode ===============================================
//==========================================================================================
static void test_per_tx(uint32_t count)
{
    if (multi_link_proto_device_is_send_data_ready())
    {
        LOG_DBG("%s in", __func__);
        multi_link_battery_data_ex_t multi_link_battery_data_ex;
        memset(&multi_link_battery_data_ex, 0x00, sizeof(multi_link_battery_data_ex_t));
        multi_link_battery_data_ex.type = MULTI_LINK_DEVICE_DATA_TYPE_BATTERY_LEVEL;
        //generate_random(&multi_link_battery_data_ex.level, sizeof(multi_link_battery_data_ex.level));
        multi_link_battery_data_ex.level = 100;
        multi_link_battery_data_ex.level = (multi_link_battery_data_ex.level * 100) / 255;
        multi_link_battery_data_ex.mAH = 0x3FFF; // only for testing
        multi_link_battery_data_ex.multiplier = 0x02; // only for testing
        multi_link_battery_data_ex.charge_cycles_counter = count; // for per test


        if(!multi_link_proto_device_send_data((uint8_t *)&multi_link_battery_data_ex, sizeof(multi_link_battery_data_ex_t)) == MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE)
        {
            LOG_ERR("multi_link_proto_device_send_data failed for MULTI_LINK_DEVICE_DATA_TYPE_BATTERY_LEVEL");
        }
    }
}
static bool test_mode_handler(const struct test_mode_event *event)
{
    switch(event->test_id) {
        case  test_id_PER_tx:
            uint32_t *tmp = (uint32_t*)event->dyndata.data;
            test_per_tx(*tmp);
            break;
        default:
            break;
	}
	return false;
}
//==========================================================================================
//====================== Battery level event handler =======================================
 
#if (CONFIG_CY25KB_RECHARGEABLE)
static bool battery_state_event_handler(const struct battery_state_event *event)
{
	battery_level_data[0] = REPORT_ID_BATTERY_LEVEL;
	battery_level_data[1] = g_battery ;
	battery_level_data[2] = (event->state == BATTERY_STATE_CHARGING)?true:false;
	send_multi_link_battery_level(battery_level_data, sizeof(battery_level_data));
	 
	return false;
}
#endif
//==========================================================================================
#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
static bool battery_level_event_handler(const struct battery_level_event *event)
{
	battery_level_data[0] = REPORT_ID_BATTERY_LEVEL;
	battery_level_data[1] = g_battery = event->level;
#if (CONFIG_CY25KB_RECHARGEABLE)
	battery_level_data[2] = event->is_charging;
	send_multi_link_battery_level(battery_level_data, sizeof(battery_level_data));
#else
#if CONFIG_RING_BUFFER	
	add_data_to_queue(battery_level_data ,sizeof(battery_level_data));
#else
	send_multi_link_battery_level(battery_level_data, sizeof(battery_level_data));
#endif
#endif
	return false;
}
#endif

//==========================================================================================
//================================ App event handler =======================================
//==========================================================================================
static bool app_event_handler(const struct app_event_header *aeh)
{

	if (is_hid_report_event(aeh)) {
		return hid_report_event_handler(cast_hid_report_event(aeh));
	}

	if (is_module_state_event(aeh)) {
		return module_state_event_handler(cast_module_state_event(aeh));
	}

	if (is_test_mode_event(aeh)) {
		return test_mode_handler(cast_test_mode_event(aeh));
	}

    if (IS_ENABLED(CONFIG_DESKTOP_GZP_MODULE_SUSPEND_EVENTS) &&
	    is_module_suspend_req_event(aeh)) {
		return handle_module_suspend_req_event(cast_module_suspend_req_event(aeh));
	}

	if (IS_ENABLED(CONFIG_DESKTOP_GZP_MODULE_SUSPEND_EVENTS) &&
	    is_module_resume_req_event(aeh)) {
		return handle_module_resume_req_event(cast_module_resume_req_event(aeh));
	}

	if (is_click_event(aeh)) {
		if (state != STATE_ACTIVE)
		return false;

		return click_event_handler(cast_click_event(aeh));
	}
#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
	if (is_battery_level_event(aeh)) {
		if (state != STATE_ACTIVE)
		return false;

		return battery_level_event_handler(cast_battery_level_event(aeh));
	}
#endif
#if (CONFIG_CY25KB_RECHARGEABLE)
	if (is_battery_state_event(aeh)) {
		if (state != STATE_ACTIVE)
			return false;

		return battery_state_event_handler(cast_battery_state_event(aeh));
	}
#endif

	

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}
APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, hid_report_event);
APP_EVENT_SUBSCRIBE(MODULE, test_mode_event);
#ifdef CONFIG_DESKTOP_GZP_MODULE_SUSPEND_EVENTS
APP_EVENT_SUBSCRIBE(MODULE, module_suspend_req_event);
APP_EVENT_SUBSCRIBE(MODULE, module_resume_req_event);
#endif /* CONFIG_DESKTOP_GZP_MODULE_SUSPEND_EVENTS */
//Henry add end
APP_EVENT_SUBSCRIBE(MODULE, click_event);
#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS) 
APP_EVENT_SUBSCRIBE(MODULE, battery_level_event);
#endif
#if (CONFIG_CY25KB_RECHARGEABLE)
APP_EVENT_SUBSCRIBE(MODULE, battery_state_event);
#endif

