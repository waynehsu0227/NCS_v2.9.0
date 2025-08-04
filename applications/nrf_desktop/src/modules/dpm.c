#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#if 1
#include <caf/events/module_suspend_event.h>
#endif
#define MODULE dpm
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);

#include "dpm_config.h"
static uint8_t get_report_buf[15] = {0};
extern uint8_t g_battery;

static void reconnect_work_handler(struct k_work *work)
{
    LOG_INF("%s", __func__);
#if 1
    struct module_resume_req_event *event = new_module_resume_req_event();
	event->module_id = MODULE_ID(ble_adv);//MODULE_ID(ble_adv);//MODULE_ID(ble_state);//
	APP_EVENT_SUBMIT(event);
#endif
}
static K_WORK_DELAYABLE_DEFINE(reconnect_work, reconnect_work_handler);


static uint8_t keep_conn_interval = 0;
static uint8_t keep_conn_remained_duration = 0;
static void keep_connection_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(keep_connection_work, keep_connection_work_handler);
static void keep_connection_work_handler(struct k_work *work)
{
    if (keep_conn_remained_duration > 0) {			
        if (keep_conn_remained_duration <= keep_conn_interval)
        {
            keep_conn_remained_duration = 0;
            //is_exec_keep_conn_cmd = false;
        }
        else
        {
            keep_conn_remained_duration -= keep_conn_interval;
            k_work_reschedule(&keep_connection_work, K_MSEC(keep_conn_interval));
        }
    }
    else {
        //is_exec_keep_conn_cmd = false;
    } 
}

static void make_get_report_package(uint8_t *buf, uint8_t size)
{
    memset(get_report_buf, 0, sizeof(get_report_buf));
    memcpy(get_report_buf, buf, size);
}

static void Get_device_FW_version(void)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[]= {0x01,
                            DPM_FW_VER_LOW_BYTE,                                    //Byte 2
                            DPM_FW_VER_HIGH_BYTE,                                   //Byte 3
                            CAPABILITY_FLAG,                                        //Byte 4
                             0x3F, 0x52, 0xC5, 0x86, 0xB7, 0xB8,                    //Byte 5 ~ 10 : Dell Device ID
                             0x00, 0x00, 0x00,                                      //Byte 11 ~ 13 : RF paired ID
                             Vendor_ID, Color_code};                                //Byte 14 ~ 15
    make_get_report_package((uint8_t*)package, sizeof(package));
}

static void Set_software_mode(bool sw)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x0D};
    static bool software_mode_enable = false;
    software_mode_enable = sw;
    make_get_report_package((uint8_t*)package, sizeof(package));
}

static void Get_battery_status(void)
{
    extern int battery_meas_bat_charger_state;
    uint8_t charge_state = (battery_meas_bat_charger_state==1)?BAT_CHARGER_INFO:0;
    

    LOG_INF("%s\n", __func__);
    uint8_t package[] = {0x0E,
                        g_battery,              //Byte 1 : Battery level is percentage (0x00 ~ 0x64)
                        charge_state,           //Byte 2 : Battery and Charger Information
                        0x00, 0x00,             //Byte 3 ~ 4 : charge_cycles_counter
                        0x00, 0x00,             //Byte 5 ~ 6 : loww_batt_counter
                        BAT_CAP_INFO_LOW_BYTE , //Byte 7 : Numeric value in mAH
                        BAT_CAP_INFO_HIGH_BYTE };//Byte 8 : Multiplier(b7~b6)/Numeric value in mAH(b5~b0)
    make_get_report_package((uint8_t*)package, sizeof(package));
}
//bill
// #include <caf/events/ble_common_event.h>
#include <zephyr/bluetooth/conn.h>
bool disconnect_from_dpm=false;
uint8_t recoonect_delay_fn_time=0;
void call_reconnect_work(void)
{
    k_work_reschedule(&reconnect_work, K_SECONDS(recoonect_delay_fn_time));
}
static void find_conn(struct bt_conn *conn, void *data)
{
	int err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if(err==0)
	{
        // uint8_t reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;
        // struct ble_peer_event *event = new_ble_peer_event();

		// event->id = conn;
		// event->state = PEER_STATE_DISCONNECTING;
		// event->reason = reason;
		// APP_EVENT_SUBMIT(event);
	}
}
//bill

static void Device_disconnect_and_reconnect(uint8_t reconnect_delay_time)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x0F};
    disconnect_from_dpm=true;//bill
    recoonect_delay_fn_time=reconnect_delay_time;
    LOG_INF("reconnect_delay_time=%d", reconnect_delay_time);
#if CONFIG_CAF_MODULE_SUSPEND_EVENTS
    //bill don't suspend adv
    bt_conn_foreach(BT_CONN_TYPE_LE,find_conn,NULL);
    // struct module_suspend_req_event *event = new_module_suspend_req_event();
	// event->module_id = MODULE_ID(ble_adv);////MODULE_ID(ble_adv);//
	// APP_EVENT_SUBMIT(event);
    // k_work_reschedule(&reconnect_work, K_SECONDS(reconnect_delay_time));//bill:commented in henry's code
#endif
    make_get_report_package((uint8_t*)package, sizeof(package));        
}
#if CONFIG_BT_GATT_CLIENT && CONFIG_DESKTOP_BLE_DISCOVERY_ENABLE
static void Get_paired_device_host_name(uint8_t index)
{
    #define NAME_MAX_PACKAGE_LEN 12
    LOG_INF("%s\n", __func__);
    static bool long_name = false;
    uint8_t index_trans = index - 1;
    char *host_name = get_host_name(index_trans);
    uint8_t name_len = strlen(host_name) + 1;
    uint8_t package[16] = {0x10,        //Byte 0
                            index,      //Byte 1 : Switch Index
                            name_len};  //Byte 2 : Total length of host name string (max 19 bytes)
    if(name_len > NAME_MAX_PACKAGE_LEN) {
        if(long_name) {
            long_name = false;
            memcpy(&package[3], host_name + NAME_MAX_PACKAGE_LEN, name_len - NAME_MAX_PACKAGE_LEN);
        } else {
            long_name = true;
            memcpy(&package[3], host_name, NAME_MAX_PACKAGE_LEN);
        }
    } else {
        long_name = false;
        memcpy(&package[3], host_name, name_len);
    }
    make_get_report_package((uint8_t*)package, sizeof(package));
}
#endif

static void Set_Touch_Scroll_Sensitivity(void)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x15};
    make_get_report_package((uint8_t*)package, sizeof(package));
//Byte 2 : Sensity level (1 to 3)
//1: Fast
//2: Medium
//3: Slow
//Byte 3 ~ 15 : Reserved, all must set to 0
//InpFeatureCmd[0] = 0x15;
}

static void Get_Touch_Scroll_Sensitivity(void)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x16};
    make_get_report_package((uint8_t*)package, sizeof(package));
//Byte 2 : Sensity level (1 to 3)
//1: Fast
//2: Medium
//3: Slow
//Byte 3 ~ 15 : Reserved, all must set to 0
}

static void set_report_polling_rate(uint16_t report_rate)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x31};
    make_get_report_package((uint8_t*)package, sizeof(package));
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3212)
    send_set_report_rate_event(report_rate);
#endif
//Byte 2 ~ 3 : Report rate
//250 to n. in Hz. In Little-Endian (LSB-MSB)
//Byte 4 ~ 15 : Reserved, all must set to 0
}

static void get_report_polling_rate(void)
{
    LOG_INF("%s\n", __func__);
    uint8_t package[11]={0};
    package[0] = 0x32;
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3212)
    memcpy(package + 1, get_report_rate_current(), 2);
    memcpy(package + 3, get_report_rate_support(), 6);
#endif
    make_get_report_package((uint8_t*)package, sizeof(package));
//supported_report_rates_hz : should be liner Ex. 100Hz, 200Hz, 300Hz, 500Hz.
//If array is < 4, last element must be terminated by 0x00.
}

static void Set_Keep_Connection(uint8_t *buf)
{
    static const uint8_t package[] = {0x23};

    keep_conn_interval = (uint16_t)(buf[1] << 8) + buf[0];
    keep_conn_remained_duration = ((uint16_t)((buf[3] & 0x7F) << 8) + buf[2]) * 1000;
    LOG_INF("%s interval:%d, duration:%d\n", __func__, keep_conn_interval, keep_conn_remained_duration);
    if ((buf[3] & 0x80) == 0x80) {
        keep_conn_interval *= 8;
    }
    k_work_reschedule(&keep_connection_work, K_MSEC(keep_conn_interval));

    make_get_report_package((uint8_t*)package, sizeof(package));
}

static void Set_Collabs_keys_setting(uint8_t *buf)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x14};
    make_get_report_package((uint8_t*)package, sizeof(package));
#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT
    set_collabs_keys_status(buf);
#endif
}

static void Set_Proximity_Backlight_setting(void)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x17};
    make_get_report_package((uint8_t*)package, sizeof(package));
}

static void Get_Proximity_Backlight_setting(void)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x18};
    make_get_report_package((uint8_t*)package, sizeof(package));
}

static void Set_DPI_value(uint16_t dpi_value)
{
    LOG_INF("%s\n", __func__);
    static const uint8_t package[] = {0x33};
    make_get_report_package((uint8_t*)package, sizeof(package));
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3212)
    send_set_dpi_event(dpi_value);
#endif
}

static void Get_DPI_value(void)
{
    LOG_INF("%s\n", __func__);
    uint8_t package[] = {0x34,
#if (CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE && CONFIG_PAW3212)
                        (uint8_t)(get_current_dpi() & 0x00ff),
                        (uint8_t)(get_current_dpi() >> 8),
                        (uint8_t)(MIN_DPI_VAL & 0x00ff),
                        (uint8_t)(MIN_DPI_VAL >> 8),
                        (uint8_t)(MAX_DPI_VAL & 0x00ff),
                        (uint8_t)(MAX_DPI_VAL >> 8),
                        (uint8_t)(DPI_STEP & 0x00ff),
                        (uint8_t)(DPI_STEP >> 8)
#endif
                                    };
    make_get_report_package((uint8_t*)package, sizeof(package));
}
/**************************************************************************************************/
/**************************************************************************************************/
/**************************************************************************************************/
/**************************************************************************************************/
/**************************************************************************************************/
void dpm_get_feature_report(uint8_t *report_val, uint8_t size)
{
    size = sizeof(get_report_buf);
    memcpy(report_val, get_report_buf, size);
    LOG_HEXDUMP_DBG(report_val, size, __func__);
}

void dpm_set_feature_report(uint8_t *report_val, uint8_t size)
{
    LOG_HEXDUMP_DBG(report_val, size, __func__);
    switch(report_val[0])
    {
        case 0x01: // Get Device FW version
            Get_device_FW_version();
            break;
        case 0x0D: // Set software mode
            Set_software_mode(report_val[1]);
            break;
        case 0x0E: // Get battery status
            Get_battery_status();
            break;
        case 0x0F: // Device disconnect and reconnect
            Device_disconnect_and_reconnect(report_val[1]);
            break;
        case 0x10: // Get paired device host name
#if CONFIG_BT_GATT_CLIENT && CONFIG_DESKTOP_BLE_DISCOVERY_ENABLE
            Get_paired_device_host_name(report_val[1]);
#endif
            break;
        case 0x11:  //set DPI level
             break;
        case 0x12:  //get DPI level
            break;
        case 0x15:  //set touch scroll sensitivity
            Set_Touch_Scroll_Sensitivity();
            break;
        case 0x16:  //get touch scroll sensitivity
            Get_Touch_Scroll_Sensitivity();
            break;
        case 0x31:  //set report (polling) rate
            set_report_polling_rate(*(uint16_t*)(report_val + 1));
            break;
        case 0x32:  //get report (polling) rate
            get_report_polling_rate();
            break;
        case 0x23:  //set keep connection
            Set_Keep_Connection(&report_val[1]);
            break;
        case 0x14:  //set collabs keys setting
            Set_Collabs_keys_setting(&report_val[1]);
            break;
        case 0x17:  //set proximity backlight setting
            Set_Proximity_Backlight_setting();
            break;
        case 0x18:  //get proximity backlight setting
            Get_Proximity_Backlight_setting();
            break;
        case 0x33:  //set DPI value
            Set_DPI_value(*(uint16_t*)(report_val + 1));
            break;
        case 0x34:  //get DPI value
            Get_DPI_value();
            break;
        case 0xDF:  //set Darfon features
            //set_config(report_val[1], (report_val[5] << 24) + (report_val[4] << 16) + (report_val[3] << 8) + report_val[2]);
        default:
            //memcpy(InpVndCmd, report_val, INPUT_REP_VCMD_LEN);

            break;
    }
}
