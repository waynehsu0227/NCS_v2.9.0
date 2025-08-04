#ifndef _MULTI_LINK_BASIC_H_
#define _MULTI_LINK_BASIC_H_

void multi_link_device_sleep_ms(uint32_t time_ms);
uint32_t multi_link_device_get_uptime_ms();
uint32_t multi_link_device_get_dynamic_key_prepare_timout_ms();
p_response_t multi_link_device_get_response();
p_challenge_t multi_link_device_get_challenge();
p_host_id_t multi_link_device_get_host_id();
p_host_address_t multi_link_device_get_host_address();
void multi_link_device_update_response(uint8_t (*response)[5]);
void multi_link_device_update_challenge(uint8_t (*challenge)[5]);
uint16_t multi_link_device_get_device_kb_layout();
uint16_t multi_link_device_get_device_capability();
uint16_t multi_link_device_get_device_firmware_version();
uint32_t multi_link_device_get_paring_timout_ms();
uint8_t multi_link_device_get_vendor_id();
uint8_t multi_link_device_get_device_type();
p_device_name_t multi_link_device_get_device_name();
p_device_id_t multi_link_device_get_device_id();
void multi_link_device_update_host_id(uint8_t (*host_id)[5]);
uint32_t multi_link_device_get_uptime_ms();
void multi_link_device_sleep_ms(uint32_t time_ms);

#endif /*_MULTI_LINK_BASIC_H_*/
