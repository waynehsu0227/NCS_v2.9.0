/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/reboot.h>

#define MODULE multi_link_ble_selector
#include <caf/events/module_state_event.h>
#include <caf/events/module_suspend_event.h>
#include <caf/events/click_event.h>
#include <zephyr/settings/settings.h>
#include <caf/events/ble_common_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);

#define ON_START_CLICK(CLICK)		(CLICK + CLICK_COUNT)
static enum module_state ble_adv_state = MODULE_STATE_COUNT;
static enum module_state ble_state_state = MODULE_STATE_COUNT;
static enum module_state ble_bond_state = MODULE_STATE_COUNT;
static enum module_state multilink_state = MODULE_STATE_COUNT;

enum protocol {
	PROTOCOL_BLE1,
    PROTOCOL_BLE2,
	PROTOCOL_MULTILINK,

	PROTOCOL_COUNT,
};
bool is_multi_link_selected = false;
static enum protocol selected_protocol = PROTOCOL_COUNT;
static uint8_t ble_host_select_id;

#define MULTI_LINK_DB_SLELECT_MODE  "select_mode"
//bill smart connect v2.0
enum protocol last_used_channel = PROTOCOL_COUNT;
#if CONFIG_MULTI_LINK_PEER_EVENTS
#include "multi_link_peer_event.h"
#endif
#include <caf/events/button_event.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#define IDLE_DETECT_CHECK_INTERVAL_SEC 1
#define IDLE_DETECT_CHECK_INTERVAL     K_SECONDS(IDLE_DETECT_CHECK_INTERVAL_SEC)
#define IDLE_TIMEOUT 10 //seconds
#define SMART_CONNECT_ACTIVE_INTERVAL K_MSEC(100) 
#define SMART_CONNECT_RF_TIMEOUT_TO_BLE K_SECONDS(10)
uint8_t disconnect_reason=0xff;
static bool keep_alive_flag;
bool is_smart_connect_active=false;
static struct k_work_delayable idle_detect_trigger;
static struct k_work_delayable smart_connect_v2;
static uint8_t idle_detect_counter=0;
static bool trigger_rf_scan=false;
bool is_connect=false;
int bill_err=0xff;
static bool is_manual_switch=false;
static void find_conn(struct bt_conn *conn, void *data)
{
	int err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if(err==0)
	{
		idle_detect_counter=0;
		trigger_rf_scan=true;
		k_work_reschedule(&smart_connect_v2, SMART_CONNECT_ACTIVE_INTERVAL);
	}
}
static void idle_detect_counter_reset(struct k_work *work)
{
	if(is_manual_switch)
		return;
	if(keep_alive_flag==true)
	{
		keep_alive_flag=false;
		if(is_connect==true && (selected_protocol == PROTOCOL_BLE1 || selected_protocol == PROTOCOL_BLE2))
		{
			idle_detect_counter = 0;
			is_smart_connect_active=false;
		}
		k_work_cancel_delayable(&smart_connect_v2);
		k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
	}
	else
	{
		idle_detect_counter++;
		if(idle_detect_counter==IDLE_TIMEOUT)
		{
			is_smart_connect_active=true;
			if (selected_protocol == PROTOCOL_BLE1 || selected_protocol == PROTOCOL_BLE2)
			{
				bt_conn_foreach(BT_CONN_TYPE_LE,find_conn,NULL);
				if(is_connect==false)
				{
					k_work_reschedule(&smart_connect_v2, SMART_CONNECT_ACTIVE_INTERVAL);
				}
			}
			else
			{
				k_work_reschedule(&smart_connect_v2, SMART_CONNECT_ACTIVE_INTERVAL);
			}
		}
		else
		{
			k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
		}
	}
	// k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
}
//bill smart connect v2.0
bool bt1_bond_valid=false;
bool bt2_bond_valid=false;
extern size_t power_down_interval_counter;//bill smart connect v2.0
void manual_switch_detected(void)
{
	is_manual_switch=true;
	NRF_POWER_S->GPREGRET[0]=0;
	NRF_POWER_S->GPREGRET[1]=0;
	power_down_interval_counter=0;
	idle_detect_counter=0;
}
static void suspend_req(const void *module_id)
{
	struct module_suspend_req_event *event = new_module_suspend_req_event();

	event->module_id = module_id;
	APP_EVENT_SUBMIT(event);
}

static void resume_req(const void *module_id)
{
	struct module_resume_req_event *event = new_module_resume_req_event();

	event->module_id = module_id;
	APP_EVENT_SUBMIT(event);
}
void smart_connect_reset_handler(void)
{
			//bill reset gen3 and select ble mode
			if(NRF_POWER_S->GPREGRET[0]==0xBE)
			{
				selected_protocol=PROTOCOL_BLE1;
				last_used_channel=PROTOCOL_BLE1;
				NRF_POWER_S->GPREGRET[0]=0;
			}
			else if(NRF_POWER_S->GPREGRET[0]==0xAE)
			{
				selected_protocol=PROTOCOL_BLE2;
				last_used_channel=PROTOCOL_BLE2;
				NRF_POWER_S->GPREGRET[0]=0;
			}
			else if(NRF_POWER_S->GPREGRET[0]==0xBA)
			{
				manual_switch_detected();
			}
			else
			{
				/* code */
			}
			
			//bill reset gen3 and select ble mode
}
static int settings_set(const char *key, size_t len_rd,
			settings_read_cb read_cb, void *cb_arg)
{
    LOG_INF("%s start", __func__);
	if (!strcmp(key, MULTI_LINK_DB_SLELECT_MODE)) {

		ssize_t len = read_cb(cb_arg, &selected_protocol,
				      sizeof(selected_protocol));

		if (len == sizeof(selected_protocol)) {
            LOG_INF("select mode=%u", selected_protocol);
		} else {
			LOG_WRN("select mode invalid length (%u)", len);

			return -EINVAL;
		}
	}
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(multi_link_ble_selector, MODULE_NAME, NULL, settings_set, NULL,
			       NULL);

static void store_mode(void)
{
    char key[] = MODULE_NAME "/" MULTI_LINK_DB_SLELECT_MODE;

    int err = settings_save_one(key, &selected_protocol,
                    sizeof(selected_protocol));

    if (err) {
        LOG_ERR("Cannot store select mode, err %d", err);
        module_set_state(MODULE_STATE_ERROR);
    }
}

static void update_radio_protocol(void)
{
	static enum protocol used_protocol = PROTOCOL_COUNT;

	if ((ble_state_state == MODULE_STATE_COUNT) ||
	    (ble_adv_state == MODULE_STATE_COUNT) ||
	    (multilink_state == MODULE_STATE_COUNT)) {
		return;
	}

	/* Do not perform any operations before ble_bond module is ready. Bluetooth must be enabled
	 * to ensure proper load of Bluetooth settings from non-volatile memory.
	 * TODO: For now, ble_bond suspend is not yet supported, feature will be added later.
	 */
	if (ble_bond_state == MODULE_STATE_COUNT) {
		return;
	}
    LOG_DBG("%s:%d", __func__, selected_protocol);
	if (selected_protocol == PROTOCOL_BLE1 || selected_protocol == PROTOCOL_BLE2) {
		if (multilink_state != MODULE_STATE_SUSPENDED) {
			suspend_req(MODULE_ID(multi_link));
		} else if (ble_state_state != MODULE_STATE_READY) {
			resume_req(MODULE_ID(ble_state));
		} else if (ble_adv_state != MODULE_STATE_READY) {
			resume_req(MODULE_ID(ble_adv));
		 }
	} else if (selected_protocol == PROTOCOL_MULTILINK) {
		if (ble_adv_state != MODULE_STATE_SUSPENDED) {
			suspend_req(MODULE_ID(ble_adv));
		} else if (ble_state_state != MODULE_STATE_SUSPENDED) {
			suspend_req(MODULE_ID(ble_state));
		} else if (multilink_state != MODULE_STATE_READY) {
			resume_req(MODULE_ID(multi_link));
		}
	}

	if ((selected_protocol == PROTOCOL_BLE1) &&
	    (multilink_state == MODULE_STATE_SUSPENDED) &&
	    (ble_state_state == MODULE_STATE_READY) &&
	    (ble_adv_state == MODULE_STATE_READY)) {
		used_protocol = PROTOCOL_BLE1;
    } else if ((selected_protocol == PROTOCOL_BLE2) &&
	    (multilink_state == MODULE_STATE_SUSPENDED) &&
	    (ble_state_state == MODULE_STATE_READY) &&
	    (ble_adv_state == MODULE_STATE_READY)) {
		used_protocol = PROTOCOL_BLE2;
	} else if ((selected_protocol == PROTOCOL_MULTILINK) &&
		   (ble_adv_state == MODULE_STATE_SUSPENDED) &&
		   (ble_state_state == MODULE_STATE_SUSPENDED) &&
		   (multilink_state == MODULE_STATE_READY)) {
		used_protocol = PROTOCOL_MULTILINK;
	} else {
		used_protocol = PROTOCOL_COUNT;
	}
}

static bool handle_module_state_event(const struct module_state_event *event)
{
	if (event->module_id == MODULE_ID(ble_adv)) {
		ble_adv_state = event->state;
		update_radio_protocol();
	} else if (event->module_id == MODULE_ID(multi_link)) {
		multilink_state = event->state;
		update_radio_protocol();
	} else if (event->module_id == MODULE_ID(ble_state)) {
		ble_state_state = event->state;
		update_radio_protocol();
	} else if (event->module_id == MODULE_ID(ble_bond)) {
		ble_bond_state = event->state;
		update_radio_protocol();
	} else if (event->module_id == MODULE_ID(settings_loader)) {
        if (selected_protocol >= PROTOCOL_COUNT) {
            LOG_INF("%s:protocol=%d, set default protocol", __func__, selected_protocol);
            selected_protocol = PROTOCOL_MULTILINK;//PROTOCOL_BLE1;PROTOCOL_MULTILINK//
			if(selected_protocol == PROTOCOL_MULTILINK)
			{
				is_multi_link_selected = true;
			}
			
        }
        update_radio_protocol();
    }
    
	return false;
}

static void handle_click_long(void)
{
	//bill
	idle_detect_counter=0;
	k_work_cancel_delayable(&idle_detect_trigger);
	//bill
	struct click_event *event = new_click_event();
    switch(selected_protocol) {
        case PROTOCOL_BLE1:
        case PROTOCOL_BLE2:
#if 1
            event->key_id = CONFIG_DESKTOP_BLE_PEER_CONTROL_BUTTON;
            event->click = CLICK_LONG;
            APP_EVENT_SUBMIT(event);
#endif
            break;
        case PROTOCOL_MULTILINK:
#if 1
            event->key_id = CONFIG_MULTI_LINK_PEER_CONTROL_BUTTON;
            event->click = CLICK_LONG;
            APP_EVENT_SUBMIT(event);
#endif
            break;
        default:
            break;
    }
}

static void handle_click(void)
{
	enum protocol current_protocol = selected_protocol;
    selected_protocol = (selected_protocol + 1) % PROTOCOL_COUNT;
    store_mode();

    struct click_event *event = new_click_event();
    LOG_INF("%s switch to %d", __func__, selected_protocol);

    if (selected_protocol == PROTOCOL_BLE2) {
        if (ble_host_select_id != PROTOCOL_BLE2) {
            event->key_id = CONFIG_DESKTOP_BLE_PEER_CONTROL_BUTTON;
            event->click = CLICK_SHORT;
            APP_EVENT_SUBMIT(event);
        }
    } else {
		is_multi_link_selected = true;
        update_radio_protocol();
        if (selected_protocol == PROTOCOL_BLE1) {
            if (ble_host_select_id != PROTOCOL_BLE1) {
                event->key_id = CONFIG_DESKTOP_BLE_PEER_CONTROL_BUTTON;
                event->click = CLICK_SHORT;
                APP_EVENT_SUBMIT(event);
            }
			else if(current_protocol == PROTOCOL_MULTILINK)
			{
				sys_reboot(SYS_REBOOT_WARM);
			}
        }
    }
}

#if	!CONFIG_CY25MS_DRYCELL
static void handle_direct_set_mode_click(uint16_t key_id)
{
	enum protocol current_protocol = selected_protocol;
	switch (key_id) {
		case CONFIG_BT1_MODE_SELECTOR_BUTTON:
			selected_protocol = PROTOCOL_BLE1;
			break;
		case CONFIG_BT2_MODE_SELECTOR_BUTTON:
			selected_protocol = PROTOCOL_BLE2;
			break;
		case CONFIG_MULTILINK_MODE_SELECTOR_BUTTON:
			selected_protocol = PROTOCOL_MULTILINK;
		default:
			break;
	}
	if (current_protocol == selected_protocol)
		return;
	LOG_INF("%s switch to %d", __func__, selected_protocol);
	store_mode();
	manual_switch_detected();//bill smart connect v2.0
	struct click_event *event = new_click_event();
	switch (selected_protocol) {
		case PROTOCOL_BLE1:
		case PROTOCOL_BLE2:
			if (current_protocol == PROTOCOL_MULTILINK)
				update_radio_protocol();
			if (ble_host_select_id != selected_protocol) {
				ble_host_select_id=selected_protocol;//bill
				event->key_id = CONFIG_DESKTOP_BLE_PEER_CONTROL_BUTTON;
				event->click = CLICK_SHORT;
				APP_EVENT_SUBMIT(event);
				//bill
				struct ble_peer_operation_event *peer_event = new_ble_peer_operation_event();
				peer_event->op = PEER_OPERATION_SELECT;
				if(selected_protocol==PROTOCOL_BLE1)
					peer_event->bt_app_id = 0;
				else if(selected_protocol==PROTOCOL_BLE2)
					peer_event->bt_app_id = 1;
				else
				{

				}
				APP_EVENT_SUBMIT(peer_event);
				//bill
			}
			else if(current_protocol == PROTOCOL_MULTILINK)
			{
				__NOP();
				__NOP();
				__NOP();
				NRF_POWER_S->GPREGRET[0]=0xBA;//bill smart connect v2.0
				k_sleep(K_MSEC(3));//bill smart connect v2.0
				sys_reboot(SYS_REBOOT_WARM);
			}
			break;
		case PROTOCOL_MULTILINK:
			is_multi_link_selected = true;
       		update_radio_protocol();
		default:
			break;
	}
}
#endif

#if	!CONFIG_CY25MS_DRYCELL
static void handle_direct_set_mode_click_long(uint16_t key_id)
{
	enum protocol current_protocol = selected_protocol;
	switch (key_id) {
		case CONFIG_BT1_MODE_SELECTOR_BUTTON:
			current_protocol = PROTOCOL_BLE1;
			break;
		case CONFIG_BT2_MODE_SELECTOR_BUTTON:
			current_protocol = PROTOCOL_BLE2;
			break;
		case CONFIG_MULTILINK_MODE_SELECTOR_BUTTON:
			current_protocol = PROTOCOL_MULTILINK;
		default:
			break;
	}
	if (current_protocol != selected_protocol)
		return;
	handle_click_long();
}
#endif

static bool click_event_handler(const struct click_event *event)
{
	LOG_INF("%s", __func__);
#if	CONFIG_CY25MS_DRYCELL
	if (event->key_id != CONFIG_MULTILINK_BLE_SELECTOR_BUTTON) {
		return false;
	}
	if (event->key_id == CONFIG_MULTILINK_BLE_SELECTOR_BUTTON) {
		if (event->click == CLICK_LONG) {
			handle_click_long();
		} else {
			handle_click();
		}
	}
#else
	if (event->key_id != CONFIG_MULTILINK_BLE_SELECTOR_BUTTON &&
		event->key_id != CONFIG_BT1_MODE_SELECTOR_BUTTON &&
		event->key_id != CONFIG_BT2_MODE_SELECTOR_BUTTON &&
		event->key_id != CONFIG_MULTILINK_MODE_SELECTOR_BUTTON) {
		return false;
	}
	if (event->key_id == CONFIG_MULTILINK_BLE_SELECTOR_BUTTON) {
		if (event->click == CLICK_LONG) {
			handle_click_long();
		} else {
			handle_click();
		}
	} else {
		if (event->click == CLICK_LONG) {
			handle_direct_set_mode_click_long(event->key_id);
		} else {
			handle_direct_set_mode_click(event->key_id);
		}
	}
#endif
	return false;
}

static bool handle_ble_peer_operation_event(const struct ble_peer_operation_event *event)
{
    if (event->op == PEER_OPERATION_SELECTED) {
        ble_host_select_id = event->bt_app_id;
    }
    return false;
}
//bill samrt connect v2.0
extern uint8_t bt_stack_id_lut[3];//bill
static uint8_t check_index=0;
// uint8_t check_bt_id=0;
static void show_single_peer(const struct bt_bond_info *info, void *user_data)
{
	if(check_index==0)
		bt1_bond_valid=true;
	if(check_index==1)
		bt2_bond_valid=true;
}
extern void restart_power_down_counter(void);//bill
static void check_bt_bond_valid(void)
{
	// check_bt_id=bt_stack_id_lut[0];
	// temp_id=check_bond_index[check_bt_id];
	check_index=0;
	bt_foreach_bond(bt_stack_id_lut[check_index], show_single_peer, NULL);
	// check_bt_id=bt_stack_id_lut[1];
	// temp_id=check_bond_index[check_bt_id];
	check_index=1;
	bt_foreach_bond(bt_stack_id_lut[check_index], show_single_peer, NULL);
}
extern bool disconnect_from_dpm;
// extern void call_reconnect_work(void);

static bool ble_peer_event_handler(const struct ble_peer_event *event)
{
	if (event->state == PEER_STATE_DISCONNECTED && disconnect_from_dpm==false) {
		if(is_smart_connect_active)
		{
			NRF_POWER_S->GPREGRET[1]=power_down_interval_counter|0x80;//prevent ble can't enter power down mode
		}
		check_bt_bond_valid();
		is_connect=false;
		disconnect_reason=event->reason;
		idle_detect_counter=0;
		k_work_cancel_delayable(&idle_detect_trigger);
		k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
	}
	else if(event->state == PEER_STATE_DISCONNECTED && disconnect_from_dpm==true)
	{
		is_connect=false;
		// call_reconnect_work();
		// disconnect_from_dpm=false;
		check_bt_bond_valid();
		idle_detect_counter=0;
		k_work_cancel_delayable(&idle_detect_trigger);
	}
	else if(event->state == PEER_STATE_SECURED){
		is_connect=true;
		idle_detect_counter=0;
		last_used_channel=selected_protocol;
		is_manual_switch=false;
		disconnect_from_dpm=false;
		// if(is_manual_switch==false)
		//{
			k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);

		//}
		if(is_smart_connect_active)
		{
			NRF_POWER_S->GPREGRET[1]=power_down_interval_counter|0x80;//prevent ble can't enter power down mode
		}
		// restart_power_down_counter();
		k_work_cancel_delayable(&smart_connect_v2);
	} 
	else{
		
	}

	return false;
}

bool smart_connect_active=false;
static void smart_connect_handler(struct k_work *work)//bill if smart connect handler is triggered, that means bt1 or bt2 has bond
{
	bool need_to_send_evt=false;
	if(trigger_rf_scan)
	{
		trigger_rf_scan=false;
		selected_protocol=PROTOCOL_MULTILINK;
	}
	else if (selected_protocol == PROTOCOL_BLE1 || selected_protocol == PROTOCOL_BLE2)
	{
		if(last_used_channel!=selected_protocol)
		{
			if(last_used_channel==PROTOCOL_MULTILINK)
			{
				if((selected_protocol==PROTOCOL_BLE1) && (bt2_bond_valid==true))
				{
					need_to_send_evt=true;
					selected_protocol=PROTOCOL_BLE2;
				}
				else
					selected_protocol=PROTOCOL_MULTILINK;
			}
			else
				selected_protocol=PROTOCOL_MULTILINK;
		}
		else if(last_used_channel==PROTOCOL_BLE1)
		{
			if(bt2_bond_valid)
			{
				selected_protocol=PROTOCOL_BLE2;
				need_to_send_evt=true;
			}
			else
				selected_protocol=PROTOCOL_MULTILINK;
		}
		else
		{
			if(bt1_bond_valid)
			{
				selected_protocol=PROTOCOL_BLE1;
				need_to_send_evt=true;
			}
			else
				selected_protocol=PROTOCOL_MULTILINK;
		}
		
		if(is_smart_connect_active)
		{
			NRF_POWER_S->GPREGRET[1]=power_down_interval_counter|0x80;//store smart connect flag in msb. if CONFIG_CAF_POWER_MANAGER_TIMEOUT>127, this part should be re-design
		}
		else
		{
			NRF_POWER_S->GPREGRET[1]=power_down_interval_counter;
		}
		// NRF_POWER_S->GPREGRET[0]=0x0E;
	}
	else
	{
		if(last_used_channel==PROTOCOL_BLE1)
		{
			selected_protocol=PROTOCOL_BLE1;
			NRF_POWER_S->GPREGRET[0]=0xBE;
		}
		else if(last_used_channel==PROTOCOL_BLE2)
		{
			selected_protocol=PROTOCOL_BLE2;
			NRF_POWER_S->GPREGRET[0]=0xAE;
		}
		else
		{
			NRF_POWER_S->GPREGRET[0]=0xCE;
		}
		if(is_smart_connect_active)
		{
			NRF_POWER_S->GPREGRET[1]=power_down_interval_counter|0x80;//store smart connect flag in msb. if CONFIG_CAF_POWER_MANAGER_TIMEOUT>127, this part should be re-design
		}
		else
		{
			NRF_POWER_S->GPREGRET[1]=power_down_interval_counter;
		}
		k_sleep(K_MSEC(3));
		sys_reboot(SYS_REBOOT_WARM);
	}
	update_radio_protocol();
	idle_detect_counter=0;
	if(need_to_send_evt)
	{
		struct click_event *event = new_click_event();
		event->key_id = CONFIG_DESKTOP_BLE_PEER_CONTROL_BUTTON;
		event->click = CLICK_SHORT;
		APP_EVENT_SUBMIT(event);
	}
	// k_work_reschedule(&smart_connect_v2, SMART_CONNECT_RF_TIMEOUT_TO_BLE);
	// k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
}
extern bool is_multi_link_tx_success;
//bill smart connect v2.0
static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_module_state_event(aeh)) {
		//bill smart connect v2.0
		const struct module_state_event *event = cast_module_state_event(aeh);
		if (check_state(event, MODULE_ID(settings_loader),MODULE_STATE_READY))
		{
			smart_connect_reset_handler();
		}
		if (check_state(event, MODULE_ID(ble_bond),MODULE_STATE_READY)){

			check_bt_bond_valid();
			// k_work_init_delayable(&idle_detect_trigger, idle_detect_counter_reset);
			// k_work_init_delayable(&smart_connect_v2, smart_connect_handler);
			if(selected_protocol==PROTOCOL_BLE1 && bt1_bond_valid==true)
				k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
			if(selected_protocol==PROTOCOL_BLE2 && bt2_bond_valid==true)
				k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
			// if((NRF_POWER_S->GPREGRET[1]&0x80)!=0)
			// {
			// 	is_smart_connect_active=true;
			// }
			if(NRF_POWER_S->GPREGRET[0]==0xCE)
			{
				if(bt1_bond_valid==true)
					selected_protocol=PROTOCOL_BLE1;
				else if(bt2_bond_valid==true)
					selected_protocol=PROTOCOL_BLE2;
				else
					selected_protocol=PROTOCOL_MULTILINK;//bill should not happen
				last_used_channel=PROTOCOL_MULTILINK;
				k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
			}
			else if(NRF_POWER_S->GPREGRET[0]==0xDE)
			{
				last_used_channel=PROTOCOL_MULTILINK;
				selected_protocol=PROTOCOL_MULTILINK;
				k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
			}
			else
			{
				__NOP();
				__NOP();
				__NOP();
			}
		}
		if(check_state(event, MODULE_ID(main),MODULE_STATE_READY))
		{
			k_work_init_delayable(&idle_detect_trigger, idle_detect_counter_reset);
			k_work_init_delayable(&smart_connect_v2, smart_connect_handler);
			if((NRF_POWER_S->GPREGRET[1]&0x80)!=0)
			{
				is_smart_connect_active=true;
			}
			if((NRF_POWER_S->GPREGRET[0]==0xAE) || (NRF_POWER_S->GPREGRET[0]==0xBE))
			{
				// NRF_POWER_S->GPREGRET[0]=0;
				k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
			}
		}
		//bill smart connect v2.0
		return handle_module_state_event(cast_module_state_event(aeh));
	}
	#if CONFIG_MULTI_LINK_PEER_EVENTS
	if (is_multi_link_peer_event(aeh)) {
		LOG_ERR("is_multi_link_peer_event");
		struct multi_link_peer_event *event = cast_multi_link_peer_event(aeh);
		switch (event->state)  {
		case MULTI_LINK_STATE_CONNECTED:
			if(is_multi_link_tx_success)//bill use flag to prevent PROTOCOL_MULTILINK resume generate MULTI_LINK_STATE_CONNECTED event
			{
				is_connect=true;
				idle_detect_counter=0;
				is_manual_switch=false;
				is_smart_connect_active=false;
				k_work_cancel_delayable(&idle_detect_trigger);
				k_work_cancel_delayable(&smart_connect_v2);
				last_used_channel=selected_protocol;
			}
			// if(is_manual_switch==false)
			// {
			// 	k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
			// }
			break;
		case MULTI_LINK_STATE_DISCONNECTED:
			is_connect=false;
			idle_detect_counter=0;
			k_work_cancel_delayable(&idle_detect_trigger);
			k_work_reschedule(&idle_detect_trigger, IDLE_DETECT_CHECK_INTERVAL);
			// k_work_reschedule(&smart_connect_v2, SMART_CONNECT_RF_TIMEOUT_TO_BLE);
			break;
		case MULTI_LINK_STATE_SECURED:
		case MULTI_LINK_STATE_CONN_FAILED:
		case MULTI_LINK_STATE_DISCONNECTING:
			/* Ignore */
		    break;
		case MULTI_LINK_STATE_PAIRING:

			break;
		default:
			__ASSERT_NO_MSG(false);
			break;
		}
		return false;
	}
	#endif
	//bill smart connect v2.0
	if (is_button_event(aeh)) {
		keep_alive_flag = true;

		return false;
	}
	if (IS_ENABLED(CONFIG_DESKTOP_BT_PERIPHERAL) &&
	is_ble_peer_event(aeh)) {
		return ble_peer_event_handler(cast_ble_peer_event(aeh));
	}
	//bill smart connect v2.0
	if (is_click_event(aeh)) {
		return click_event_handler(cast_click_event(aeh));
	}

    if (is_ble_peer_operation_event(aeh)) {
        return handle_ble_peer_operation_event(cast_ble_peer_operation_event(aeh));
	}
	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE(MODULE, click_event);
APP_EVENT_SUBSCRIBE(MODULE, button_event);//bill
// APP_EVENT_SUBSCRIBE(MODULE, ble_peer_operation_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, ble_peer_operation_event);//bill store power down counter in smart connect active mode
#if IS_ENABLED(CONFIG_DESKTOP_BT_PERIPHERAL)
APP_EVENT_SUBSCRIBE(MODULE, ble_peer_event);
#endif
// #if IS_ENABLED(CONFIG_CAF_KEEP_ALIVE_EVENTS)
// APP_EVENT_SUBSCRIBE(MODULE, keep_alive_event);
// #endif
#if CONFIG_MULTI_LINK_PEER_EVENTS
APP_EVENT_SUBSCRIBE(MODULE, multi_link_peer_event);
#endif