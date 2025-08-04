#ifndef _MULTI_LINK_HANDLER_H_
#define _MULTI_LINK_HANDLER_H_


#include <zephyr/kernel.h>

#include "multi_link_proto_device.h"
#include "multi_link_setting.h"

#define KEEP_ALIVE_USE_KTIMER    true

void my_task_queue_init(void);
void add_data_to_queue(const uint8_t *data, size_t size);

void multi_link_send_info_step_work_init(void);
void multi_link_send_info_step_work_submit(uint64_t ms);

void multi_link_send_get_report_response_work_init(void);
void multi_link_send_get_report_response_work_submit(uint64_t ms);

void multi_link_send_keep_alive_work_init(void);
void multi_link_send_keep_alive_work_submit(uint64_t ms);
void multi_link_send_keep_alive_work_cancel(void);

bool send_multi_link_by_type(void * p_packet, uint32_t data_length);

bool multi_link_device_set_settings(uint8_t settings_type, uint8_t* data, uint8_t data_length);

void multi_link_handler_init(void);

void multi_link_send_keep_alive_timer_start(int32_t ms);
void multi_link_send_keep_alive_timer_stop(void);

#if (CONFIG_DPM_MULTI_LINK_SUPPORT && CONFIG_DESKTOP_BATTERY_MEAS)
bool send_multi_link_battery_level(const uint8_t *data, size_t size);
#endif

bool send_hid_report_multi_link_keyboard_keys(const uint8_t *data, size_t size);

#if CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT
bool send_hid_report_multi_link_consumer_keys(const uint8_t *data, size_t size);
#endif

bool send_hid_report_multi_link_dpm_input(const uint8_t *data, size_t size);
bool send_hid_report_multi_link_mouse(const uint8_t *data, size_t size, uint8_t *packet_cnt);

//#if (CONFIG_EARY_BATTERY_MEAS  && CONFIG_DPM_ENABLE)
//static bool battery_meas_is_ready = false; //aw
//#endif


#endif /*_MULTI_LINK_HANDLER_H_*/
