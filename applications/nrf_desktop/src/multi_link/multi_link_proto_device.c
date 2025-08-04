#include "multi_link_proto_device.h"

#ifdef __ZEPHYR__
#define MODULE multi_link_proto_device
//Henry add start
//#include <logging/log.h>
//LOG_MODULE_REGISTER(MODULE, CONFIG_DELL_DONGLE_MULTI_LINK_PROTO_DEVICE_LOG_LEVEL);
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DELL_DEVICE_MULTI_LINK_PROTO_DEVICE_LOG_LEVEL);
#else
#define LOG_DBG(...)
#define LOG_ERR(...)
#define LOG_WRN(...)
#define LOG_HEXDUMP_DBG(...)
#define LOG_HEXDUMP_INF(...)
#endif

#define DEVICE_BACKOFF_TIME 300
#define DEVICE_AUTO_ACK_BACKOFF_CHECK_TIME 200
#define DEVICE_WAIT_G3P5_PAIRING_CONFIRM_CHECK_TIME 500

static uint8_t g_multi_link_proto_device_status;
static multi_link_proto_device_callbacks_t *g_multi_link_proto_device_callbacks;
static multi_link_common_callbacks_t *g_multi_link_common_callbacks;

static bool g_auto_ask_start_time_ms;
static uint32_t g_paring_start_time_ms;
static uint32_t g_data_send_error_start_time_ms;

static uint32_t g_dynamic_key_prepare_start_time_ms;
static multi_link_dynamic_key_info_ex_t g_multi_link_dynamic_key_info;

static uint32_t g_data_pipe;
static bool g_is_send_data_ready;

uint32_t flag_multilink_repair = false;

//extern uint32_t multilink_event_flag;

#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT

ecdh_key_t Dev_Key_info;

static uint8_t gen3p5_pairing_retry_counts = 0;

// #define device_debug_mode_enable 1

// #else

// #define device_debug_mode_enable 0

#endif

uint8_t g_taps_length = 6;    //default =6 

uint8_t g_taps_size = 22;   //injection data

uint8_t g_taps_length_p1 = 7;    //default =7

#if (CONFIG_ENABLE_INJECTTION_DATA)

uint8_t *g_taps_polynomial, *g_taps_polynomial_p1, *g_hash_lookup, *g_lprime_list;

uint64_t *g_prime_index;

#endif

uint16_t gUpdateCount =0;

bool device_ecdh_key_exist = false;

bool multi_link_proto_g3p5_paired_start(multi_link_common_callbacks_t *multi_link_common_callbacks,
                                        multi_link_proto_device_callbacks_t *multi_link_proto_device_callbacks)
{
    bool ret_val = true;

#if(CONFIG_ENABLE_INJECTTION_DATA) 

    if(!multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_TAPS_POLY,  (void**)&g_taps_polynomial) ||
        !multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_TAPS_POLY_P1, (void**)&g_taps_polynomial_p1) ||
        !multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_HASH_LOOKUP, (void**)&g_hash_lookup) ||
        !multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_PRIME, (void**)&g_prime_index) ||
        !multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_LPRIME, (void**)&g_lprime_list)
    )
    {
        return false;
    }
#endif
    multi_link_proto_common_set_lfst_poly(g_taps_size);

    g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_V2_PAIRING_REQUEST;
    //0x80
    multi_link_device_pairing_request_v2_ex_t *multi_link_device_pairing_request_v2 = multi_link_common_encode_paired_request_ex(multi_link_proto_device_callbacks->device_get_device_id(), (multi_link_common_callbacks_t*)multi_link_common_callbacks);
    
    if (multi_link_device_pairing_request_v2)
    {
        ret_val = g_multi_link_proto_device_callbacks->device_send_packet(GAZELL_HOST_PARING_PIPE, (const uint8_t *)multi_link_device_pairing_request_v2, sizeof(multi_link_device_pairing_request_v2_ex_t));
        if (!ret_val)
        {
            LOG_ERR("device_send_packet failed for multi_link_device_pairing_request_v2");
        }
        else
        {
            g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);

            LOG_INF("mdevice_send_packet success for multi_link_device_pairing_request_v2");
        }
    }
    else
    {
        LOG_ERR("multi_link_common_encode_paired_request_ex failed");
        ret_val = false;
    }
    g_paring_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();

    if (!ret_val)
    {
        g_paring_start_time_ms = 0;
        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ERROR;
        g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
    }
    return ret_val;
}
bool multi_link_proto_device_start(multi_link_common_callbacks_t *multi_link_common_callbacks,
                                   multi_link_proto_device_callbacks_t *multi_link_proto_device_callbacks
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                                   ,bool start_with_g3p5
#endif
                                )
{

    bool ret_val = true;

    if (!g_multi_link_proto_device_callbacks)
    {
        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_UNKNOW;

        if (multi_link_proto_device_callbacks->device_paring_status_changed &&
            multi_link_proto_device_callbacks->device_send_packet &&
            multi_link_proto_device_callbacks->device_sleep_ms &&
            multi_link_proto_device_callbacks->device_get_uptime_ms &&
            multi_link_proto_device_callbacks->device_backoff &&
            multi_link_proto_device_callbacks->device_update_host_address &&
            multi_link_proto_device_callbacks->device_update_host_id &&
            multi_link_proto_device_callbacks->device_get_device_id &&
            multi_link_proto_device_callbacks->device_get_device_name &&
            multi_link_proto_device_callbacks->device_get_device_type &&
            multi_link_proto_device_callbacks->device_get_vendor_id &&
            multi_link_proto_device_callbacks->device_get_paring_timout_ms &&
            multi_link_proto_device_callbacks->device_get_device_firmware_version &&
            multi_link_proto_device_callbacks->device_get_device_capability &&
            multi_link_proto_device_callbacks->device_get_device_kb_layout &&
            multi_link_proto_device_callbacks->device_update_challenge &&
            multi_link_proto_device_callbacks->device_update_response &&
            multi_link_proto_device_callbacks->device_get_host_address &&
            multi_link_proto_device_callbacks->device_get_host_id &&
            multi_link_proto_device_callbacks->device_get_challenge &&
            multi_link_proto_device_callbacks->device_get_response &&
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT            
            multi_link_proto_device_callbacks->device_get_paired_type &&
            multi_link_proto_device_callbacks->device_update_paired_type &&
#endif            
            multi_link_proto_device_callbacks->device_dynamic_key_status_changed &&
            multi_link_proto_device_callbacks->device_get_dynamic_key_prepare_timout_ms &&
            multi_link_proto_device_callbacks->device_data_send_status &&
            multi_link_proto_device_callbacks->device_set_settings       
            )
        {
            g_multi_link_proto_device_callbacks = multi_link_proto_device_callbacks;

            LOG_INF("Valid multi_link_proto_device_callbacks!!!");
        }
        else
        {
            LOG_ERR("Invalid multi_link_proto_device_callbacks!!!");
            return false;
        }

        ret_val = multi_link_proto_common_init_callbacks(multi_link_common_callbacks);

        if (!ret_val)
        {
            LOG_ERR("multi_link_proto_common_init_callbacks failed!!!");
            return false;
        }
        else
        {
            g_multi_link_common_callbacks = multi_link_common_callbacks;

            LOG_INF("multi_link_proto_common_init_callbacks success!!!");
        }
    }
    LOG_INF("multi_link_proto_common_init_callbacks ready!!!");
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT      
#if(CONFIG_ENABLE_INJECTTION_DATA)
    if(multi_link_common_callbacks)
    {
        if(!multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_TAPS_POLY, (void**)&g_taps_polynomial) ||
            !multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_TAPS_POLY_P1, (void**)&g_taps_polynomial_p1) ||
            !multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_HASH_LOOKUP, (void**)&g_hash_lookup) ||
            !multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_PRIME, (void**)&g_prime_index) ||
            !multi_link_common_callbacks->load_g3p5_data(MULTI_LINK_STATIC_DATA_TYPE_LPRIME, (void**)&g_lprime_list)
        )
        {
            LOG_ERR("G35Data Load failed!!!");
            return false;
        }
    }
#endif
#endif    
    g_data_pipe = (g_multi_link_proto_device_callbacks->device_get_device_type() == DEVICE_TYPE_KEYBOARD ? GAZELL_HOST_KEYBOARD_PIPE : GAZELL_HOST_MOUSE_PIPE);

    //
    // If device provides host_address, host_id, challenge and response, we are not in paring but in dynamic key update
    //
    p_host_address_t host_address = g_multi_link_proto_device_callbacks->device_get_host_address();
    p_host_id_t host_id = g_multi_link_proto_device_callbacks->device_get_host_id();
    p_challenge_t challenge = g_multi_link_proto_device_callbacks->device_get_challenge();
    p_response_t response = g_multi_link_proto_device_callbacks->device_get_response();

    if (!multi_link_proto_common_check_array_is_empty((const uint8_t *)&host_address[0], sizeof(host_address[0])) &&
        !multi_link_proto_common_check_array_is_empty((const uint8_t *)&host_id[0], sizeof(host_id[0])) &&
        !multi_link_proto_common_check_array_is_empty((const uint8_t *)&challenge[0], sizeof(challenge[0])) &&
        !multi_link_proto_common_check_array_is_empty((const uint8_t *)&response[0], sizeof(response[0]))
        &&!flag_multilink_repair

    )
    {
        LOG_DBG("Starting dynamic key exchange");

        g_is_send_data_ready = false;
        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_UNKNOW;
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT        
        G3p5_paired_init_status = g_multi_link_proto_device_callbacks->device_get_paired_type();
#endif
        //
        // Before dynamic key init start, tell device to update gazell_device_update_radio_params using provided host_address
        //
        ret_val = g_multi_link_proto_device_callbacks->device_update_host_address((uint8_t(*)[4]) host_address);
        
        if (!ret_val)
        {
            LOG_ERR("dynamic key device_update_host_address failed");        
        }
        else
        {
            memset(&g_multi_link_dynamic_key_info, 0x00, sizeof(g_multi_link_dynamic_key_info));

#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT

            if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
            {
                ret_val = multi_link_proto_common_init_dynamic_key(host_id, challenge, response, &g_multi_link_dynamic_key_info);
            }
            else
            {
                ret_val = multi_link_proto_common_init_dynamic_key_v2(host_id, challenge, response, &g_multi_link_dynamic_key_info);
            }
#else
            ret_val = multi_link_proto_common_init_dynamic_key(host_id, challenge, response, &g_multi_link_dynamic_key_info);
#endif

            if (!ret_val)
            {
                LOG_ERR("multi_link_proto_common_init_dynamic_key failed!!!");
            }
        }

        if (ret_val)
        {

            g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_PREPARE;
            g_multi_link_proto_device_callbacks->device_dynamic_key_status_changed(g_multi_link_proto_device_status);

            g_dynamic_key_prepare_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();

            //
            // Create multi_link_dynamic_key_prepare_ex_t and send
            //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
            multi_link_dynamic_key_prepare_ex_v2_t *multi_link_dynamic_key_prepare_ex_v2 = 0;

            multi_link_dynamic_key_prepare_ex_t *multi_link_dynamic_key_prepare_ex = 0;

            if(G3p5_paired_init_status==G3P5_PAIRING_INIT_STATUS_SUCCESS)
            {
                //0x6f
                multi_link_dynamic_key_prepare_ex_v2 = multi_link_proto_common_encode_dynamic_key_prepare_ex_v2(host_id, g_multi_link_proto_device_callbacks->device_get_device_id(), &g_multi_link_dynamic_key_info);

                if (multi_link_dynamic_key_prepare_ex_v2)
                {
                    ret_val = g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (const uint8_t *)multi_link_dynamic_key_prepare_ex_v2, sizeof(multi_link_dynamic_key_prepare_ex_v2_t));

                    if (!ret_val)
                    {
                        LOG_ERR("device_send_packet failed for multi_link_dynamic_key_prepare_ex_v2");
                    }
                }
                else
                {
                    LOG_ERR("multi_link_proto_common_encode_dynamic_key_prepare_ex_v2 failed");
                    ret_val = false;
                }
            }
            else
            {
                //0x64
                multi_link_dynamic_key_prepare_ex = multi_link_proto_common_encode_dynamic_key_prepare_ex(host_id, g_multi_link_proto_device_callbacks->device_get_device_id(), &g_multi_link_dynamic_key_info);

                if (multi_link_dynamic_key_prepare_ex)
                {
                    ret_val = g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (const uint8_t *)multi_link_dynamic_key_prepare_ex, sizeof(multi_link_dynamic_key_prepare_ex_t));

                    if (!ret_val)
                    {
                        LOG_ERR("device_send_packet failed for multi_link_dynamic_key_prepare_ex");
                    }
                }
                else
                {
                    LOG_ERR("multi_link_proto_common_encode_dynamic_key_prepare_ex failed");
                    ret_val = false;
                }
            }
#else
            multi_link_dynamic_key_prepare_ex_t *multi_link_dynamic_key_prepare_ex = multi_link_proto_common_encode_dynamic_key_prepare_ex(host_id, g_multi_link_proto_device_callbacks->device_get_device_id(), &g_multi_link_dynamic_key_info);
            if (multi_link_dynamic_key_prepare_ex)
            {
                ret_val = g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (const uint8_t *)multi_link_dynamic_key_prepare_ex, sizeof(multi_link_dynamic_key_prepare_ex_t));

                if (!ret_val)
                {
                    LOG_ERR("device_send_packet failed for multi_link_dynamic_key_prepare_ex");
                }
            }
            else
            {
                LOG_ERR("multi_link_proto_common_encode_dynamic_key_prepare_ex failed");
                ret_val = false;
            }
#endif			
            
        }

        if (!ret_val)
        {
            g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_ERROR;
            g_multi_link_proto_device_callbacks->device_dynamic_key_status_changed(g_multi_link_proto_device_status);
        }
    }
    else    // Pairing Process
    {
        LOG_INF("RF Start pairing!!!");
        g_auto_ask_start_time_ms = 0;

        flag_multilink_repair = false;
        uint8_t device_nonce_start_value[3] = {0x00, 0x00, 0x00}; // Device must passing all 0x00 will generate internally

        LOG_DBG("Starting paring");

 #if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT

        if(start_with_g3p5)
        {
            multi_link_proto_g3p5_paired_start(g_multi_link_common_callbacks, g_multi_link_proto_device_callbacks);

            G3p5_paired_init_status = G3P5_PAIRING_INIT_STATUS_START_STEP1;

            return true;
        }
        else
        {

 #endif

            ret_val = multi_link_proto_common_init_paring(&device_nonce_start_value);
            if (!ret_val)
            {
                LOG_ERR("multi_link_proto_common_init_paring failed!!!");
            }
            else
            {
                LOG_INF("multi_link_proto_common_init_paring Success!!!");
                g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ADDRESS_REQUEST;
                g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);

                g_paring_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();

                //
                // Create host_address_request_ex and send4
                //

    #if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                multi_link_host_address_request_ex_t *multi_link_host_address_request_ex;
                //0x70
                if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    multi_link_host_address_request_ex = multi_link_proto_common_encode_host_address_request_ex(0, 0, g_multi_link_proto_device_callbacks->device_get_device_type(), g_multi_link_proto_device_callbacks->device_get_vendor_id(), g_multi_link_proto_device_callbacks->device_get_device_id());
                else
                    multi_link_host_address_request_ex = multi_link_proto_common_encode_host_address_request_ex_v2(0, 0, g_multi_link_proto_device_callbacks->device_get_device_type(), g_multi_link_proto_device_callbacks->device_get_vendor_id(), g_multi_link_proto_device_callbacks->device_get_device_id());
    #else
                multi_link_host_address_request_ex_t *multi_link_host_address_request_ex = multi_link_proto_common_encode_host_address_request_ex(0, 0, g_multi_link_proto_device_callbacks->device_get_device_type(), g_multi_link_proto_device_callbacks->device_get_vendor_id(), g_multi_link_proto_device_callbacks->device_get_device_id());
    #endif
                if (multi_link_host_address_request_ex)
                {
                    ret_val = g_multi_link_proto_device_callbacks->device_send_packet(GAZELL_HOST_PARING_PIPE, (const uint8_t *)multi_link_host_address_request_ex, sizeof(multi_link_host_address_request_ex_t));

                    if (!ret_val)
                    {
                        LOG_ERR("device_send_packet failed for multi_link_host_address_request_ex");
                    }
                    else
                    {
                        LOG_INF("device_send_packet success for multi_link_host_address_request_ex");
                    }
                }
                else
                {
                    LOG_ERR("multi_link_proto_common_encode_host_address_request_ex failed");
                    ret_val = false;
                }
            }
            if (!ret_val)
            {
                g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ERROR;
                g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
            }
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
        } //if(start_with_g3p5)
#endif
    }

    return ret_val;
}

#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT

void multi_link_proto_device_process_gen3p5_pairing_request(bool is_success, uint32_t pipe, const uint8_t *payload, uint32_t payload_length, uint16_t paring_state)
{
    bool retry_last_packet = false;
    bool is_good_payload = false;
    bool is_failed = false;
    static uint8_t paring_public_key_exchange_times;

    if (is_success)
    {
        if (payload_length > 0)
        {
            g_auto_ask_start_time_ms = 0;

            //
            // We got payload in ACK
            //
            LOG_DBG("Got ack with payload");
            //
            // We should get GZP_CMD_DONGLE_PAIRING_RESPOND_V2_EX
            //
            if (payload[0] == GZP_CMD_DEVICE_PAIRING_REQUEST_V2_EX && payload_length == sizeof(multi_link_device_confirm_request_ex_t))
            {
                multi_link_device_confirm_request_ex_t *multi_link_device_confirm_request_ex = (multi_link_device_confirm_request_ex_t *)payload;

                //
                // Check if payload_p is all 0, dongle rejected paring as device already paired.
                //
                if(multi_link_proto_common_check_array_is_empty((uint8_t *)&multi_link_device_confirm_request_ex->payload_p,sizeof(multi_link_device_confirm_request_ex->payload_p)))
                {
                    LOG_WRN("Device already paired");
                    g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ALREADY_PAIRED;
                    g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
                    return;
                }
            }
            else if (payload[0] == GZP_CMD_DONGLE_PAIRING_RESPOND_V2_EX && payload_length == sizeof(multi_link_device_confirm_request_ex_t) && G3p5_paired_init_status <= G3P5_PAIRING_INIT_STATUS_START_STEP2)
            {
                multi_link_device_confirm_request_ex_t *multi_link_device_confirm_request_ex = (multi_link_device_confirm_request_ex_t *)payload;
                //
                // Decode request
                //
                //0x3E
                uint32_t ret = multi_link_common_decode_confirm_request_ex(multi_link_device_confirm_request_ex, g_multi_link_proto_device_callbacks->device_get_device_id());

                is_good_payload = (ret==0)?true:false;

                if (is_good_payload)
                {
                    if(!device_ecdh_key_exist)
                    {
                        if(g_multi_link_common_callbacks->generate_ecdh_data(&Dev_Key_info)!=0)
                        {
                            LOG_ERR("generate_ecdh_data failed for generate_ecdh_data");
                            is_failed = true;
                        }
                        else
                        {
                            device_ecdh_key_exist = true;
                        }
                    }
                    else
                    {
                        LOG_INF("Already had Device ECDH key");
                    }
                    paring_public_key_exchange_times =0;

                    multi_link_publick_ex_t *multi_link_publick_ex = multi_link_common_encode_public_key_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), &Dev_Key_info, paring_public_key_exchange_times);

                    if (multi_link_publick_ex)
                    {
                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_publick_ex, sizeof(multi_link_publick_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_publick_ex");
                            is_failed = true;
                        }
                        else
                        {
                            g_paring_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();

                            G3p5_paired_init_status = G3P5_PAIRING_INIT_STATUS_ECDH_DATA_EXCHANGE+paring_public_key_exchange_times;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_common_encode_public_key_ex failed");
                        is_failed = true;
                    }
                }
                else
                {
                    LOG_WRN("multi_link_common_decode_confirm_request_ex failed");
                }
            }
            else if ((payload[0] >=GZP_CMD_ECDH_KEY_EXCHANGE_START && payload[0] <=GZP_CMD_ECDH_KEY_EXCHANGE_END )&& payload_length == sizeof(multi_link_publick_ex_t) && G3p5_paired_init_status >= G3P5_PAIRING_INIT_STATUS_ECDH_DATA_EXCHANGE)
            {
                multi_link_publick_ex_t *multi_link_publick_ex = (multi_link_publick_ex_t *)payload;
                //
                // Decode ECDH DATA
                //

                is_good_payload = multi_link_common_decode_public_key_ex(multi_link_publick_ex, &Dev_Key_info, payload[0]-GZP_CMD_ECDH_KEY_EXCHANGE_START);

                if (is_good_payload)
                {
                    paring_public_key_exchange_times = payload[0]-GZP_CMD_ECDH_KEY_EXCHANGE_START+1;

                    if(payload[0]<GZP_CMD_ECDH_KEY_EXCHANGE_END)
                    {
                        multi_link_publick_ex_t *multi_link_publick_ex = multi_link_common_encode_public_key_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), &Dev_Key_info, paring_public_key_exchange_times);

                        if (multi_link_publick_ex)
                        {
                            if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_publick_ex, sizeof(multi_link_publick_ex_t)))
                            {
                                LOG_ERR("device_send_packet failed for multi_link_publick_ex");
                                is_failed = true;
                            }
                            else
                            {
                                G3p5_paired_init_status = G3P5_PAIRING_INIT_STATUS_ECDH_DATA_EXCHANGE;
                            }
                        }
                        else
                        {
                            LOG_ERR("multi_link_common_encode_public_key_ex failed");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        bool ret_val = true;

                        G3p5_paired_init_status = G3P5_PAIRING_INIT_STATUS_SUCCESS;

                        G3p5_dk_status = G3P5_DYNAMIC_KEY_STATUS_NONE;

                        multi_link_proto_common_init_pairing_v2(&Dev_Key_info);

                        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ADDRESS_REQUEST;
                        g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
                        g_multi_link_proto_device_callbacks->device_update_paired_type(G3P5_PAIRING_INIT_STATUS_SUCCESS);
                        g_paring_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
                        //0x70
                        multi_link_host_address_request_ex_t *multi_link_host_address_request_ex = multi_link_proto_common_encode_host_address_request_ex_v2(0, 0, g_multi_link_proto_device_callbacks->device_get_device_type(), g_multi_link_proto_device_callbacks->device_get_vendor_id(), g_multi_link_proto_device_callbacks->device_get_device_id());

                        if (multi_link_host_address_request_ex)
                        {
                            ret_val = g_multi_link_proto_device_callbacks->device_send_packet(GAZELL_HOST_PARING_PIPE, (const uint8_t *)multi_link_host_address_request_ex, sizeof(multi_link_host_address_request_ex_t));

                            if (!ret_val)
                            {
                                LOG_ERR("device_send_packet failed for multi_link_host_address_request_ex_v2");
                            }
                            else
                            {
                                LOG_INF("device_send_packet success for multi_link_host_address_request_ex_v2");
                            }
                        }
                        else
                        {
                            LOG_ERR("multi_link_host_address_request_ex_v2 failed");
                            ret_val = false;
                        }

                        //
                        // Create host_address_request_ex and send4
                        //
                    }
                }
            }
            if (!is_good_payload)
            {
                LOG_DBG("Backingoff due to bad payload");
                g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);
            }
            retry_last_packet = (is_good_payload == false);
        }
        else
        {
            bool backoff = false;
            //
            // We got auto ACK
            //
            LOG_DBG("Got auto ack");

            //
            // Check we are continues getting auto ACK.
            // If yes, 99% old dongle near by and in paring mode so backoff for some time and try again
            //
            if (g_auto_ask_start_time_ms == 0)
            {
                g_auto_ask_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
            }
            else
            {
                uint32_t auto_ask_time = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
                if ((auto_ask_time - g_auto_ask_start_time_ms) >= DEVICE_WAIT_G3P5_PAIRING_CONFIRM_CHECK_TIME)
                {
                    backoff = true;
                    g_auto_ask_start_time_ms = 0;
                }
            }

            if (backoff)
            {
                LOG_DBG("Backingoff due to continues auto ask");

                g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);

                if (paring_state == MULTI_LINK_PARING_STATE_PAIRING_REQUEST_V2_EX ||
                    paring_state == MULTI_LINK_PARING_STATE_ECDH_DATA_EXCHANGE_EX)
                {
                    //
                    // We need to send last state packet as other dongle may ate up
                    //
                    const uint8_t *packet;
                    uint32_t packet_length;

                    multi_link_proto_common_last_state_packet(&packet, &packet_length);

                    if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
                    {
                        LOG_ERR("device_send_packet failed for retring last state packet");

                        is_failed = true;
                    }
                }
                else
                {
                    //
                    // We need to send last packet may dongle  did not process last packet
                    //
                    retry_last_packet = true;
                }
            }
            else
            {
                if(G3p5_paired_init_status <G3P5_PAIRING_INIT_STATUS_START_STEP2)
                {
                    g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_V2_PAIRING_REQUEST;

                    const uint8_t *packet;
                    uint32_t packet_length;

                    //multi_link_proto_common_last_state_packet(&packet, &packet_length);

                    multi_link_proto_common_current_packet(&packet, &packet_length);

                    if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
                    {
                        LOG_ERR("device_send_packet failed for multi_link_device_pairing_request_v2");

                        is_failed = true;
                    }
                    else
                    {
                        g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
                        LOG_INF("mdevice_send_packet success for multi_link_device_pairing_request_v2");
                        is_failed = false;
                    }
                    //multi_link_device_pairing_request_v2_ex_t *multi_link_device_pairing_request_v2 = multi_link_common_encode_paired_request_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), (multi_link_common_callbacks_t*)g_multi_link_common_callbacks);

                    G3p5_paired_init_status = G3P5_PAIRING_INIT_STATUS_START_STEP2;

                    // if (multi_link_device_pairing_request_v2)
                    // {
                    //     if(!g_multi_link_proto_device_callbacks->device_send_packet(GAZELL_HOST_PARING_PIPE, (const uint8_t *)multi_link_device_pairing_request_v2, sizeof(multi_link_device_pairing_request_v2_ex_t)))
                    //     {
                    //         LOG_ERR("device_send_packet failed for multi_link_device_pairing_request_v2");
                    //         is_failed = true;
                    //     }
                    //     else
                    //     {
                    //         g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);                        
                    //         LOG_INF("mdevice_send_packet success for multi_link_device_pairing_request_v2");
                    //         is_failed = false;
                    //         // G3p5_paired_init_counts++;
                    //     }
                    // }
                    // else
                    // {
                    //     LOG_ERR("multi_link_common_encode_paired_request_ex failed");
                    //     is_failed = true;
                    // }
                }
                else if(G3p5_paired_init_status == G3P5_PAIRING_INIT_STATUS_ECDH_DATA_EXCHANGE)
                {
                    const uint8_t *packet;

                    uint32_t packet_length;

                    multi_link_proto_common_last_state_packet(&packet, &packet_length);

                    if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
                    {
                        LOG_ERR("device_send_packet failed for retring last state packet");

                        is_failed = true;
                    }
                }
                else
                {
                    LOG_ERR("Gen3.5 retry over 3 timers and back to Gen3 process");
                    is_failed = true;
                }
            }
        }
    }
    else
    {
        LOG_DBG("Backingoff due to error");

        retry_last_packet = true;

        //
        // Backoff some time and try again
        //
        g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);
    }

    if (retry_last_packet && !is_failed)
    {
        LOG_DBG("Retrying current packet");

        const uint8_t *packet;
        uint32_t packet_length;

        multi_link_proto_common_current_packet(&packet, &packet_length);

        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
        {
            LOG_ERR("device_send_packet failed for retring current  packet");

            is_failed = true;
        }
    }
    if (is_failed)
    {
        //
        // stop g3.5 paring and start g3 pairing
        //
        //
        gen3p5_pairing_retry_counts++;

        if(gen3p5_pairing_retry_counts<5)
        {
            multi_link_proto_device_start(g_multi_link_common_callbacks, g_multi_link_proto_device_callbacks,1);
        }
        else
        {
            multi_link_proto_device_start(g_multi_link_common_callbacks, g_multi_link_proto_device_callbacks,0);
        }
    }
}
#endif

void multi_link_proto_device_process_host_address_request(bool is_success, uint32_t pipe, const uint8_t *payload, uint32_t payload_length, uint16_t paring_state)
{
    bool retry_last_packet = false;
    bool is_failed = false;

    if (is_success)
    {
        if (payload_length > 0)
        {
            bool is_good_payload = false;
            g_auto_ask_start_time_ms = 0;

            //
            // We got payload in ACK
            //
            LOG_DBG("Got ack with payload");
            //
            // We should get GZP_CMD_HOST_ADDRESS_PAIRING_CHALLENGE_EX or GZP_CMD_HOST_ADDRESS_RESPONSE_EX
            //

            if (payload[0] == GZP_CMD_HOST_ADDRESS_PAIRING_CHALLENGE_EX &&
                payload_length == sizeof(multi_link_host_address_pairing_challenge_ex_t) &&
                paring_state >= MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT &&
                paring_state <= MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX)
            {
                multi_link_host_address_pairing_challenge_ex_t *multi_link_host_address_pairing_challenge = (multi_link_host_address_pairing_challenge_ex_t *)payload;

  				//
                // Check if payload_p is all 0, dongle rejected paring as device already paired.
                //
                if(multi_link_proto_common_check_array_is_empty((uint8_t *)&multi_link_host_address_pairing_challenge->payload_p,sizeof(multi_link_host_address_pairing_challenge->payload_p)))
                {
                    LOG_WRN("Device already paired");
                    g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ALREADY_PAIRED;
                    g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
                    return;
                }

                //
                // Decode request
                //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                //0x31
                if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                {
                    is_good_payload = multi_link_proto_common_decode_host_address_pairing_challenge(multi_link_host_address_pairing_challenge, g_multi_link_proto_device_callbacks->device_get_device_id());
                }
                else
                {
                    uint32_t ret = multi_link_proto_common_decode_host_address_pairing_challenge_v2(multi_link_host_address_pairing_challenge, g_multi_link_proto_device_callbacks->device_get_device_id());
            
                    is_good_payload = (ret==0)?true:false;
                }
#else
                is_good_payload = multi_link_proto_common_decode_host_address_pairing_challenge(multi_link_host_address_pairing_challenge, g_multi_link_proto_device_callbacks->device_get_device_id());
#endif
                if (is_good_payload)
                {
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    //0x72
                    multi_link_host_address_pairing_response_ex_t *multi_link_host_address_pairing_response_ex = 0;
                    if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {
                        multi_link_host_address_pairing_response_ex = multi_link_proto_common_encode_host_address_pairing_response(g_multi_link_proto_device_callbacks->device_get_device_id(), (uint8_t(*)[5]) & multi_link_host_address_pairing_challenge->payload_p.salt_challenge);
                    }
                    else
                    {
                        multi_link_host_address_pairing_response_ex = multi_link_proto_common_encode_host_address_pairing_response_v2(g_multi_link_proto_device_callbacks->device_get_device_id(), (uint8_t(*)[5]) & multi_link_host_address_pairing_challenge->payload_p.salt_challenge);
                    }

#else
                    multi_link_host_address_pairing_response_ex_t *multi_link_host_address_pairing_response_ex = multi_link_proto_common_encode_host_address_pairing_response(g_multi_link_proto_device_callbacks->device_get_device_id(), (uint8_t(*)[5]) & multi_link_host_address_pairing_challenge->payload_p.salt_challenge);
#endif

                    if (multi_link_host_address_pairing_response_ex)
                    {
                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_host_address_pairing_response_ex, sizeof(multi_link_host_address_pairing_response_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_host_address_pairing_response_ex");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_host_address_pairing_response failed");
                        is_failed = true;
                    }
                }
                else
                {
                    LOG_WRN("multi_link_proto_common_decode_host_address_pairing_challenge failed");
                }
            }
            else if (payload[0] == GZP_CMD_HOST_ADDRESS_RESPONSE_EX && payload_length == sizeof(multi_link_host_address_response_ex_t))
            {
                //0x33
                LOG_DBG("Got host address response");

                multi_link_host_address_response_ex_t *multi_link_host_address_response_ex = (multi_link_host_address_response_ex_t *)payload;

                //
                // Check if payload_p is all 0, dongle rejected paring as user canceled paring.
                //
                if(multi_link_proto_common_check_array_is_empty((uint8_t *)&multi_link_host_address_response_ex->payload_p,sizeof(multi_link_host_address_response_ex->payload_p)))
                {
                    LOG_WRN("Device paring rejected");
                    g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_REJECTED;
                    g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
                    return;
                }

 #if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                //0x51
                if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                {
                    is_good_payload = multi_link_proto_common_decode_host_address_response_ex(multi_link_host_address_response_ex);
                }
                else
                {
                    uint32_t ret  = multi_link_proto_common_decode_host_address_response_ex_v2(multi_link_host_address_response_ex);

                    is_good_payload = (ret==0)?true:false;
                }
#else
                is_good_payload = multi_link_proto_common_decode_host_address_response_ex(multi_link_host_address_response_ex);
#endif
                if (is_good_payload)
                {

                    if (!g_multi_link_proto_device_callbacks->device_update_host_address((uint8_t(*)[4]) & multi_link_host_address_response_ex->payload_p.host_address))
                    {
                        LOG_WRN("device_update_host_address failed");
                        is_failed = true;
                    }
                    else
                    {

                        //
                        // Start Host ID request;
                        //
                        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ID_REQUEST;
                        g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                        //0x75****
                        multi_link_host_id_request_ex_t *multi_link_host_id_request_ex = 0;
                        if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                        {
                            multi_link_host_id_request_ex = multi_link_proto_common_encode_host_id_request_ex(g_multi_link_proto_device_callbacks->device_get_device_id());
                        }
                        else
                        {
                            multi_link_host_id_request_ex = multi_link_proto_common_encode_host_id_request_ex_v2(g_multi_link_proto_device_callbacks->device_get_device_id());
                        }
#else
                        multi_link_host_id_request_ex_t *multi_link_host_id_request_ex = multi_link_proto_common_encode_host_id_request_ex(g_multi_link_proto_device_callbacks->device_get_device_id());
#endif
                        if (multi_link_host_id_request_ex)
                        {
                            if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_host_id_request_ex, sizeof(multi_link_host_id_request_ex_t)))
                            {
                                LOG_ERR("device_send_packet failed for multi_link_host_id_request_ex");
                                is_failed = true;
                            }
                        }
                        else
                        {
                            LOG_ERR("multi_link_proto_common_encode_host_id_request_ex failed");
                            is_failed = true;
                        }
                    }
                }
                else
                {
                    LOG_WRN("multi_link_proto_common_decode_host_address_response_ex failed");
                }
            }

            if (!is_good_payload)
            {
                LOG_DBG("Backingoff due to bad payload");
                g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);
            }

            retry_last_packet = (is_good_payload == false);
        }
        else
        {
            bool backoff = false;
            //
            // We got auto ACK
            //
            LOG_DBG("Got auto ack");

            //
            // Check we are continues getting auto ACK.
            // If yes, 99% old dongle near by and in paring mode so backoff for some time and try again
            //
            if (g_auto_ask_start_time_ms == 0)
            {
                g_auto_ask_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
            }
            else
            {
                uint32_t auto_ask_time = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
                if ((auto_ask_time - g_auto_ask_start_time_ms) >= DEVICE_AUTO_ACK_BACKOFF_CHECK_TIME)
                {
                    backoff = true;
                    g_auto_ask_start_time_ms = 0;
                }
            }

            if (backoff)
            {
                LOG_DBG("Backingoff due to continues auto ask");

                g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);

                if (paring_state == MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT ||
                    paring_state == MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX)
                {
                    //
                    // We need to send last state packet as other dongle may ate up
                    //
                    const uint8_t *packet;
                    uint32_t packet_length;

                    multi_link_proto_common_last_state_packet(&packet, &packet_length);

                    if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
                    {
                        LOG_ERR("device_send_packet failed for retring last state packet");

                        is_failed = true;
                    }
                }
                else
                {
                    //
                    // We need to send last packet may dongle  did not process last packet
                    //
                    retry_last_packet = true;
                }
            }
            else
            {
                if (paring_state == MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_EX)
                {
                    //
                    // send multi_link_host_address_fetch_request_init packet
                    //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    multi_link_host_address_fetch_request_init_ex_t *multi_link_host_address_fetch_request_init=0;
                    //0x71
                    if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {
                        multi_link_host_address_fetch_request_init = multi_link_proto_common_encode_host_address_fetch_request_init_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
                    }
                    else
                    {
                        multi_link_host_address_fetch_request_init = multi_link_proto_common_encode_host_address_fetch_request_init_ex_v2(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
                    }
#else
                    multi_link_host_address_fetch_request_init_ex_t *multi_link_host_address_fetch_request_init = multi_link_proto_common_encode_host_address_fetch_request_init_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
#endif

                    if (multi_link_host_address_fetch_request_init)
                    {

                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_host_address_fetch_request_init, sizeof(multi_link_host_address_fetch_request_init_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_host_address_fetch_request_init");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_host_address_fetch_request_init_ex failed!!!");
                        is_failed = true;
                    }
                }
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT                
                else if (paring_state == MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT)
                {
                    if(G3p5_paired_init_status==G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {                    
                        //0x70
                        multi_link_host_address_request_ex_t *multi_link_host_address_request_ex = multi_link_proto_common_encode_host_address_request_ex_v2(0, 0, g_multi_link_proto_device_callbacks->device_get_device_type(), g_multi_link_proto_device_callbacks->device_get_vendor_id(), g_multi_link_proto_device_callbacks->device_get_device_id());

                        if (multi_link_host_address_request_ex)
                        {
                            if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_host_address_request_ex, sizeof(multi_link_host_address_request_ex_t)))
                            {
                                LOG_ERR("device_send_packet failed for multi_link_host_address_request_ex_v2");
                                is_failed = true;
                            }
                        }
                        else
                        {
                            LOG_ERR("multi_link_proto_common_encode_host_address_request_ex_v2 failed!!!");
                            is_failed = true;
                        }
                    }
                }
#endif                
                else if (paring_state == MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX)
                {
                    //
                    // send multi_link_host_address_fetch_request_final_ex packet
                    //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    //0x73
                    multi_link_host_address_fetch_request_final_ex_t *multi_link_host_address_fetch_request_final_ex = 0;

                    if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {
                        multi_link_host_address_fetch_request_final_ex = multi_link_proto_common_encode_host_address_fetch_request_final_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
                    }
                    else
                    {
                        multi_link_host_address_fetch_request_final_ex = multi_link_proto_common_encode_host_address_fetch_request_final_ex_v2(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
                    }
#else
                    multi_link_host_address_fetch_request_final_ex_t *multi_link_host_address_fetch_request_final_ex = multi_link_proto_common_encode_host_address_fetch_request_final_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
#endif
                    if (multi_link_host_address_fetch_request_final_ex)
                    {

                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_host_address_fetch_request_final_ex, sizeof(multi_link_host_address_fetch_request_final_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_host_address_fetch_request_final_ex");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_host_address_fetch_request_init_ex failed!!!");
                        is_failed = true;
                    }
                }
                else
                {
                    retry_last_packet = true;
                }
            }
        }
    }
    else
    {
        LOG_DBG("Backingoff due to error");

        retry_last_packet = true;

        //
        // Backoff some time and try again
        //
        g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);
    }

    if (retry_last_packet && !is_failed)
    {
        LOG_DBG("Retrying current packet");

        const uint8_t *packet;
        uint32_t packet_length;

        multi_link_proto_common_current_packet(&packet, &packet_length);

        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
        {
            LOG_ERR("device_send_packet failed for retring current  packet");

            is_failed = true;
        }
    }

    if (is_failed)
    {
        //
        // not recoverable error stop paring
        //
        //
        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ERROR;
        g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);

        
    }
}

void multi_link_proto_device_process_host_id_request(bool is_success, uint32_t pipe, const uint8_t *payload, uint32_t payload_length, uint16_t paring_state)
{

    bool retry_last_packet = false;
    bool is_failed = false;

    if (is_success)
    {

        if (payload_length > 0)
        {
            bool is_good_payload = false;
            g_auto_ask_start_time_ms = 0;

            //
            // We got payload in ACK
            //
            LOG_DBG("Got ack with payload");

            //
            // We should get GZP_CMD_HOST_ID_PAIRING_CHALLENGE_EX or GZP_CMD_HOST_ID_RESPONSE_EX
            //

            if (payload[0] == GZP_CMD_HOST_ID_PAIRING_CHALLENGE_EX &&
                payload_length == sizeof(multi_link_host_id_pairing_challenge_ex_t) &&
                paring_state >= MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT &&
                paring_state <= MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX)
            {
                multi_link_host_id_pairing_challenge_ex_t *multi_link_host_id_pairing_challenge = (multi_link_host_id_pairing_challenge_ex_t *)payload;

                //
                // Decode request
                //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                //0x54
                if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                {
                    is_good_payload = multi_link_proto_common_decode_host_id_pairing_challenge(multi_link_host_id_pairing_challenge, g_multi_link_proto_device_callbacks->device_get_device_id());
                }
                else
                {
                    uint32_t ret = multi_link_proto_common_decode_host_id_pairing_challenge_v2(multi_link_host_id_pairing_challenge, g_multi_link_proto_device_callbacks->device_get_device_id());   

                    is_good_payload = (ret==0)?true:false;
                }
#else
                is_good_payload = multi_link_proto_common_decode_host_id_pairing_challenge(multi_link_host_id_pairing_challenge, g_multi_link_proto_device_callbacks->device_get_device_id());
#endif
                if (is_good_payload)
                {
                    uint8_t device_response[5];
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    //0x77 *****
                    multi_link_host_id_pairing_response_ex_t *multi_link_host_id_pairing_response_ex = 0;

                    if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {
                        multi_link_host_id_pairing_response_ex = multi_link_proto_common_encode_host_id_pairing_response(g_multi_link_proto_device_callbacks->device_get_device_id(), (uint8_t(*)[5]) & multi_link_host_id_pairing_challenge->payload_p.salt_challenge, (uint8_t(*)[5]) & device_response);
                    }
                    else
                    {
                        multi_link_host_id_pairing_response_ex = multi_link_proto_common_encode_host_id_pairing_response_v2(g_multi_link_proto_device_callbacks->device_get_device_id(), (uint8_t(*)[5]) & multi_link_host_id_pairing_challenge->payload_p.salt_challenge, (uint8_t(*)[5]) & device_response);
                    }
#else
                    multi_link_host_id_pairing_response_ex_t *multi_link_host_id_pairing_response_ex = multi_link_proto_common_encode_host_id_pairing_response(g_multi_link_proto_device_callbacks->device_get_device_id(), (uint8_t(*)[5]) & multi_link_host_id_pairing_challenge->payload_p.salt_challenge, (uint8_t(*)[5]) & device_response);
#endif
                    if (multi_link_host_id_pairing_response_ex)
                    {
                        g_multi_link_proto_device_callbacks->device_update_challenge((uint8_t(*)[5]) & multi_link_host_id_pairing_challenge->payload_p.salt_challenge);
                        g_multi_link_proto_device_callbacks->device_update_response((uint8_t(*)[5]) & device_response);

                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_host_id_pairing_response_ex, sizeof(multi_link_host_id_pairing_response_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_host_id_pairing_response_ex");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_host_id_pairing_response failed");
                        is_failed = true;
                    }
                }
                else
                {
                    LOG_ERR("multi_link_proto_common_decode_host_id_pairing_challenge failed");
                    is_failed = true;
                }
            }
            else if (payload[0] == GZP_CMD_HOST_ID_RESPONSE_EX && payload_length == sizeof(multi_link_host_id_response_ex_t))
            {
                LOG_DBG("Got host address response");

                multi_link_host_id_response_ex_t *multi_link_host_id_response_ex = (multi_link_host_id_response_ex_t *)payload;
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                //0x56
                if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                {
                    is_good_payload = multi_link_proto_common_decode_host_id_response_ex(multi_link_host_id_response_ex);
                }
                else
                {
                    uint32_t ret = multi_link_proto_common_decode_host_id_response_ex_v2(multi_link_host_id_response_ex);

                    is_good_payload = (ret==0)?true:false;
                }
#else
                is_good_payload = multi_link_proto_common_decode_host_id_response_ex(multi_link_host_id_response_ex);
#endif
                if (is_good_payload)
                {

                    g_multi_link_proto_device_callbacks->device_update_host_id((uint8_t(*)[5]) & multi_link_host_id_response_ex->payload_p.host_id);

                    //
                    // Start device info send
                    //
                    g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_DEVICE_INFO_SEND;
                    g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    //0x7C
                    multi_link_device_info_ex_t *multi_link_device_info_ex = 0;
                    if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {
                        multi_link_device_info_ex = multi_link_proto_common_encode_device_info_ex(g_multi_link_proto_device_callbacks->device_get_device_id(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_firmware_version(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_capability(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_kb_layout());
                    }
                    else
                    {
                        multi_link_device_info_ex = multi_link_proto_common_encode_device_info_ex_v2(g_multi_link_proto_device_callbacks->device_get_device_id(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_firmware_version(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_capability(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_kb_layout());
                    }

#else
                    multi_link_device_info_ex_t *multi_link_device_info_ex = multi_link_proto_common_encode_device_info_ex(g_multi_link_proto_device_callbacks->device_get_device_id(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_firmware_version(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_capability(),
                                                                                                                           g_multi_link_proto_device_callbacks->device_get_device_kb_layout());
#endif
                    if (multi_link_device_info_ex)
                    {
                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_device_info_ex, sizeof(multi_link_device_info_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_device_info_ex");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_device_info_ex failed");
                        is_failed = true;
                    }
                }
                else
                {
                    LOG_WRN("multi_link_proto_common_decode_host_id_response_ex failed");
                }
            }

            if (!is_good_payload)
            {
                LOG_DBG("Backingoff due to bad payload");
                g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);
            }

            retry_last_packet = (is_good_payload == false);
        }
        else
        {
            bool backoff = false;
            //
            // We got auto ACK
            //
            LOG_DBG("Got auto ack");

            //
            // Check we are continues getting auto ACK.
            // If yes, 99% old dongle near by and in paring mode so backoff for some time and try again
            //
            if (g_auto_ask_start_time_ms == 0)
            {
                g_auto_ask_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
            }
            else
            {
                uint32_t auto_ask_time = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
                if ((auto_ask_time - g_auto_ask_start_time_ms) >= DEVICE_AUTO_ACK_BACKOFF_CHECK_TIME)
                {
                    backoff = true;
                    g_auto_ask_start_time_ms = 0;
                }
            }

            if (backoff)
            {
                LOG_DBG("Backingoff due to continues auto ask");

                g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);

                if (paring_state == MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT ||
                    paring_state == MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX)
                {
                    //
                    // We need to send last state packet as other dongle may ate up
                    //
                    const uint8_t *packet;
                    uint32_t packet_length;

                    multi_link_proto_common_last_state_packet(&packet, &packet_length);

                    if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
                    {
                        LOG_ERR("device_send_packet failed for retring last state packet");

                        is_failed = true;
                    }
                }
                else
                {
                    //
                    // We need to send last packet may dongle  did not process last packet
                    //
                    retry_last_packet = true;
                }
            }
            else
            {
                if (paring_state == MULTI_LINK_PARING_STATE_HOST_ID_REQ_EX)
                {
                    //
                    // send multi_link_host_id_fetch_request_init packet
                    //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    //0x76****
                    multi_link_host_id_fetch_request_init_ex_t *multi_link_host_id_fetch_request_init = 0;

                    if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {
                        multi_link_host_id_fetch_request_init = multi_link_proto_common_encode_host_id_fetch_request_init_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
                    }
                    else
                    {
                        multi_link_host_id_fetch_request_init = multi_link_proto_common_encode_host_id_fetch_request_init_ex_v2(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
                    }
#else
                    multi_link_host_id_fetch_request_init_ex_t *multi_link_host_id_fetch_request_init = multi_link_proto_common_encode_host_id_fetch_request_init_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
#endif
                    if (multi_link_host_id_fetch_request_init)
                    {

                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_host_id_fetch_request_init, sizeof(multi_link_host_id_fetch_request_init_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_host_id_fetch_request_init");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_host_id_fetch_request_init_ex failed!!!");
                        is_failed = true;
                    }
                }
                else if (paring_state == MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX)
                {
                    //
                    // send multi_link_host_id_fetch_request_final_ex packet
                    //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    //0x78
                    multi_link_host_id_fetch_request_final_ex_t *multi_link_host_id_fetch_request_final_ex = 0;

                    if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {
                        multi_link_host_id_fetch_request_final_ex = multi_link_proto_common_encode_host_id_fetch_request_final_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
                    }
                    else
                    {
                        multi_link_host_id_fetch_request_final_ex = multi_link_proto_common_encode_host_id_fetch_request_final_ex_v2(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
                    }
#else
                    multi_link_host_id_fetch_request_final_ex_t *multi_link_host_id_fetch_request_final_ex = multi_link_proto_common_encode_host_id_fetch_request_final_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), g_multi_link_proto_device_callbacks->device_get_device_name());
#endif
                    if (multi_link_host_id_fetch_request_final_ex)
                    {

                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_host_id_fetch_request_final_ex, sizeof(multi_link_host_id_fetch_request_final_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_host_id_fetch_request_final_ex");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_host_id_fetch_request_init_ex failed!!!");
                        is_failed = true;
                    }
                }
                else
                {
                    retry_last_packet = true;
                }
            }
        }
    }
    else
    {
        LOG_DBG("Backingoff due to error");

        retry_last_packet = true;

        //
        // Backoff some time and try again
        //
        g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);
    }

    if (retry_last_packet && !is_failed)
    {
        LOG_DBG("Retrying current packet");

        const uint8_t *packet;
        uint32_t packet_length;

        multi_link_proto_common_current_packet(&packet, &packet_length);

        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
        {
            LOG_ERR("device_send_packet failed for retring current  packet");

            is_failed = true;
        }
    }

    if (is_failed)
    {
        //
        // not recoverable error stop paring
        //
        //
        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ERROR;
        g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
    }


}

void multi_link_proto_device_process_device_info_send(bool is_success, uint32_t pipe, const uint8_t *payload, uint32_t payload_length, uint16_t paring_state)
{

    bool retry_last_packet = false;
    bool is_failed = false;

    if (is_success)
    {

        if (payload_length > 0)
        {
            bool is_good_payload = false;
            g_auto_ask_start_time_ms = 0;

            //
            // We got payload in ACK
            //
            LOG_DBG("Got ack with payload");

            //
            // We should get GZP_CMD_DEVICE_INFO_RESPONSE_EX
            //
            if (payload[0] == GZP_CMD_DEVICE_INFO_RESPONSE_EX && payload_length == sizeof(multi_link_device_info_response_ex_t) && paring_state == MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX)
            {
                multi_link_device_info_response_ex_t *multi_link_device_info_response_ex = (multi_link_device_info_response_ex_t *)payload;

                //
                // Decode request
                //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                //0x3D
                if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                {
                    is_good_payload = multi_link_proto_common_decode_device_info_response_ex(multi_link_device_info_response_ex, g_multi_link_proto_device_callbacks->device_get_device_id());
                }
                else
                {
                    uint32_t ret = multi_link_proto_common_decode_device_info_response_ex_v2(multi_link_device_info_response_ex, g_multi_link_proto_device_callbacks->device_get_device_id());   

                    is_good_payload = (ret==0)?true:false;
                }
#else
                is_good_payload = multi_link_proto_common_decode_device_info_response_ex(multi_link_device_info_response_ex, g_multi_link_proto_device_callbacks->device_get_device_id());
#endif
                if (is_good_payload)
                {
                    if (multi_link_device_info_response_ex->payload_p.result == 1)
                    {
                        //
                        // Paring done
                        //
                        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_DONE;
                        g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);

                        //
                        // Now we will start dynamic key exchange. We are continue from paring so callbacks already set dose not need it
                        //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                        multi_link_proto_device_start(0, 0, 0);
#else
                        multi_link_proto_device_start(0, 0);
#endif
                    }
                    else
                    {
                        LOG_ERR("Dongle return result == 0");
                        is_failed = true;
                    }
                }
                else
                {
                    LOG_ERR("multi_link_proto_common_decode_device_info_response_ex failed");
                    is_failed = true;
                }
            }

            if (!is_good_payload)
            {
                LOG_DBG("Backingoff due to bad payload");
                g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);
            }

            retry_last_packet = (is_good_payload == false);
        }
        else
        {
            bool backoff = false;
            //
            // We got auto ACK
            //
            LOG_DBG("Got auto ack");

            //
            // Check we are continues getting auto ACK.
            // If yes, 99% old dongle near by and in paring mode so backoff for some time and try again
            //
            if (g_auto_ask_start_time_ms == 0)
            {
                g_auto_ask_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
            }
            else
            {
                uint32_t auto_ask_time = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
                if ((auto_ask_time - g_auto_ask_start_time_ms) >= DEVICE_AUTO_ACK_BACKOFF_CHECK_TIME)
                {
                    backoff = true;
                    g_auto_ask_start_time_ms = 0;
                }
            }

            if (backoff)
            {
                LOG_DBG("Backingoff due to continues auto ask");

                g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);

                if (paring_state == MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX)
                {
                    //
                    // We need to send last state packet as other dongle may ate up
                    //
                    const uint8_t *packet;
                    uint32_t packet_length;

                    multi_link_proto_common_last_state_packet(&packet, &packet_length);

                    if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
                    {
                        LOG_ERR("device_send_packet failed for retring last state packet");

                        is_failed = true;
                    }
                }
                else
                {
                    //
                    // We need to send last packet may dongle  did not process last packet
                    //
                    retry_last_packet = true;
                }
            }
            else
            {
                if (paring_state == MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX)
                {
                    //
                    // send multi_link_device_info_fetch_response_ex packet
                    //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    //0x7D****
                    multi_link_device_info_fetch_response_ex_t *multi_link_device_info_fetch_response_ex = 0;
                    if(G3p5_paired_init_status!=G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    {
                        multi_link_device_info_fetch_response_ex = multi_link_proto_common_encode_device_info_fetch_response_ex(g_multi_link_proto_device_callbacks->device_get_device_id());
                    }
                    else
                    {
                        multi_link_device_info_fetch_response_ex = multi_link_proto_common_encode_device_info_fetch_response_ex_v2(g_multi_link_proto_device_callbacks->device_get_device_id());
                    }
#else
                    multi_link_device_info_fetch_response_ex_t *multi_link_device_info_fetch_response_ex = multi_link_proto_common_encode_device_info_fetch_response_ex(g_multi_link_proto_device_callbacks->device_get_device_id());
#endif
                    if (multi_link_device_info_fetch_response_ex)
                    {

                        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, (const uint8_t *)multi_link_device_info_fetch_response_ex, sizeof(multi_link_device_info_fetch_response_ex_t)))
                        {
                            LOG_ERR("device_send_packet failed for multi_link_device_info_fetch_response_ex");
                            is_failed = true;
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_device_info_fetch_response_ex failed!!!");
                        is_failed = true;
                    }
                }

                else
                {
                    retry_last_packet = true;
                }
            }
        }
    }
    else
    {
        LOG_DBG("Backingoff due to error");

        retry_last_packet = true;

        //
        // Backoff some time and try again
        //
        g_multi_link_proto_device_callbacks->device_backoff(pipe, DEVICE_BACKOFF_TIME);
    }

    if (retry_last_packet && !is_failed)
    {
        LOG_DBG("Retrying current packet");

        const uint8_t *packet;
        uint32_t packet_length;

        multi_link_proto_common_current_packet(&packet, &packet_length);

        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, packet, packet_length))
        {
            LOG_ERR("device_send_packet failed for retring current  packet");

            is_failed = true;
        }
    }

    if (is_failed)
    {
        //
        // not recoverable error stop paring
        //
        //
        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_ERROR;
        g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
    }


}

void multi_link_proto_device_process_dynamic_key_prepare(bool is_success, uint32_t pipe, const uint8_t *payload, uint32_t payload_length)
{
    bool retry_last_packet = false;
    bool is_failed = false;

    if (is_success)
    {
        if (payload_length > 0)
        {
            bool is_good_payload = false;

            //
            // We got payload in ACK
            //
            LOG_DBG("Got ack with payload");

            //LOG_HEXDUMP_DBG((uint8_t *)payload, payload_length, "payload :");
            //LOG_HEXDUMP_DBG((uint8_t *)&g_multi_link_dynamic_key_info.nonce_start_value, 3, "nonce_start_value :");


            if (payload[0] == GZP_CMD_DYNAMIC_KEY_RESPONSE_EX &&
                payload_length == sizeof(multi_link_dynamic_key_response_ex_t)  &&
                g_multi_link_dynamic_key_info.state == 1)

            {
                //0x25
                // We can not use original g_multi_link_dynamic_key_info because if some other device  may send ACK and that can mess up
                // this
                //
                multi_link_dynamic_key_info_ex_t multi_link_dynamic_key_info_temp = g_multi_link_dynamic_key_info;;

#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                if(g_multi_link_proto_device_callbacks->device_get_paired_type()==G3P5_PAIRING_INIT_STATUS_SUCCESS)
                {
                    is_good_payload = multi_link_proto_common_decode_dynamic_key_response_ex_v2((multi_link_dynamic_key_response_ex_t *)payload, g_multi_link_proto_device_callbacks->device_get_device_id(), &multi_link_dynamic_key_info_temp);

                    k_sleep(K_MSEC(50));
                }
                else
                {
                    is_good_payload = multi_link_proto_common_decode_dynamic_key_response_ex((multi_link_dynamic_key_response_ex_t *)payload, g_multi_link_proto_device_callbacks->device_get_device_id(), &multi_link_dynamic_key_info_temp);
                }
#else                
                is_good_payload = multi_link_proto_common_decode_dynamic_key_response_ex((multi_link_dynamic_key_response_ex_t *)payload, g_multi_link_proto_device_callbacks->device_get_device_id(), &multi_link_dynamic_key_info_temp);
#endif

                if(is_good_payload)
                {

                    g_multi_link_dynamic_key_info = multi_link_dynamic_key_info_temp;
                    g_multi_link_dynamic_key_info.is_dynamic_key_done = true;
                    g_is_send_data_ready = true;
                    
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    // if(G3p5_paired_init_status==G3P5_PAIRING_INIT_STATUS_SUCCESS)
                    // {
                    //     g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE_STEP0;
                    // }
                    // else
                    {
                        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE;
                    }
#else
                    g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE;
#endif
                    g_multi_link_proto_device_callbacks->device_dynamic_key_status_changed(g_multi_link_proto_device_status);

                    return;
                }
            }
            else if (payload[0] == GZP_CMD_DYNAMIC_KEY_RESPONSE_EX &&
                payload_length == 4  &&
                g_multi_link_dynamic_key_info.nonce_start_value[0] == payload[1] &&
                g_multi_link_dynamic_key_info.nonce_start_value[1] == payload[2] &&
                g_multi_link_dynamic_key_info.nonce_start_value[2] == payload[3])

            {
                g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_DEVICE_NOT_PAIRED;
                g_multi_link_proto_device_callbacks->device_dynamic_key_status_changed(g_multi_link_proto_device_status);
                return;
            }


            retry_last_packet = (is_good_payload == false);
        }
        else
        {
            //
            // We got auto ACK
            //
            LOG_DBG("Got auto ack");

            if (g_multi_link_dynamic_key_info.state == 0)
            {
                //
                // Create multi_link_dynamic_key_fetch_ex_t and send
                //
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                multi_link_dynamic_key_fetch_ex_t *multi_link_dynamic_key_fetch_ex = 0;
                multi_link_dynamic_key_fetch_ex_v2_t *multi_link_dynamic_key_fetch_ex_v2 = 0;

                if(G3p5_paired_init_status==G3P5_PAIRING_INIT_STATUS_SUCCESS)
                {   
                    //0x6E
                    k_sleep(K_MSEC(20));

                    multi_link_dynamic_key_fetch_ex_v2 = multi_link_proto_common_encode_dynamic_key_fetch_ex_v2(g_multi_link_proto_device_callbacks->device_get_device_id(), &g_multi_link_dynamic_key_info);
                    if (multi_link_dynamic_key_fetch_ex_v2)
                    {

                        is_failed = !g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (const uint8_t *)multi_link_dynamic_key_fetch_ex_v2, sizeof(multi_link_dynamic_key_fetch_ex_v2_t));

                        if (is_failed)
                        {
                            LOG_ERR("device_send_packet failed for multi_link_dynamic_key_fetch_ex_v2");
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_dynamic_key_fetch_ex_v2 failed");
                        is_failed = true;
                    }
                }
                else
                {
                    //0x65
                    multi_link_dynamic_key_fetch_ex = multi_link_proto_common_encode_dynamic_key_fetch_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), &g_multi_link_dynamic_key_info);
                    if (multi_link_dynamic_key_fetch_ex)
                    {

                        is_failed = !g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (const uint8_t *)multi_link_dynamic_key_fetch_ex, sizeof(multi_link_dynamic_key_fetch_ex_t));

                        if (is_failed)
                        {
                            LOG_ERR("device_send_packet failed for multi_link_dynamic_key_fetch_ex");
                        }
                    }
                    else
                    {
                        LOG_ERR("multi_link_proto_common_encode_dynamic_key_fetch_ex failed");
                        is_failed = true;
                    }
                }
#else
                multi_link_dynamic_key_fetch_ex_t *multi_link_dynamic_key_fetch_ex = multi_link_proto_common_encode_dynamic_key_fetch_ex(g_multi_link_proto_device_callbacks->device_get_device_id(), &g_multi_link_dynamic_key_info);
                if (multi_link_dynamic_key_fetch_ex)
                {

                    is_failed = !g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (const uint8_t *)multi_link_dynamic_key_fetch_ex, sizeof(multi_link_dynamic_key_fetch_ex_t));

                    if (is_failed)
                    {
                        LOG_ERR("device_send_packet failed for multi_link_dynamic_key_fetch_ex");
                    }
                }
                else
                {
                    LOG_ERR("multi_link_proto_common_encode_dynamic_key_fetch_ex failed");
                    is_failed = true;
                }
#endif
            }
            else if (g_multi_link_dynamic_key_info.state == 1)
            {
                retry_last_packet = true;
            }
        }
    }
    else
    {
        retry_last_packet = true;
    }

    if (retry_last_packet && !is_failed)
    {
        LOG_DBG("Retrying current packet");

        if (!g_multi_link_proto_device_callbacks->device_send_packet(pipe, g_multi_link_dynamic_key_info.current_packet_buffer, g_multi_link_dynamic_key_info.current_packet_buffer_length))
        {
            LOG_ERR("device_send_packet failed for retring current packet");

            is_failed = true;
        }
    }

    if (is_failed)
    {
        //
        // not recoverable error stop key request
        //
        //
        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_ERROR;
        g_multi_link_proto_device_callbacks->device_dynamic_key_status_changed(g_multi_link_proto_device_status);
    }
}

static uint32_t multi_link_proto_device_decrypt_ack_info(multi_link_setting_ack_info_ex_t* multi_link_setting_ack_info_ex)  
{

	uint8_t ack_key[16];
	memcpy(ack_key, g_multi_link_dynamic_key_info.ack_key, sizeof(ack_key));

	int invs = multi_link_proto_common_inverse_modulus(multi_link_setting_ack_info_ex->header.sequence, 17);
	int invs2 = multi_link_proto_common_inverse_modulus(multi_link_setting_ack_info_ex->header.sequence, 251);

	//
	// Es = E(ack_key,  multi_link_setting_ack_info_ex.header.sequence)
	//
	uint8_t es[16] = {};
	uint8_t es_p[16] = {};


	if(invs >= 16)
	{
		invs = 0;
	}
	
	if(invs2 == 0)
	{
		invs2 = 24;
	}

	//
	// Randomize ACK key and es_p
	//
	ack_key[0] = multi_link_setting_ack_info_ex->header.sequence;
	es_p[0] = multi_link_setting_ack_info_ex->header.sequence;

	ack_key[invs] = invs2;
	es_p[invs] = invs2;;

	if (g_multi_link_common_callbacks->ecb_128_raw(ack_key, es_p, sizeof(es_p), es, sizeof(es)))
	{
		LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
	}
	else
	{
		LOG_ERR("ecb_128 failed for es");
		return 0;
	}


	//
	// p = multi_link_setting_ack_info_ex.payload XOR es
	//
	multi_link_proto_common_xor(multi_link_setting_ack_info_ex->payload, (const uint8_t *)&multi_link_setting_ack_info_ex->payload, (const uint8_t *)es, multi_link_setting_ack_info_ex->header.payload_length);


	return multi_link_setting_ack_info_ex->header.payload_length;
}

void multi_link_proto_device_process_packet(bool is_success, uint32_t pipe, const uint8_t *payload, uint32_t payload_length)
{
    if (
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
        g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_V2_PAIRING_REQUEST ||
#endif
        g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ADDRESS_REQUEST ||
        g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ID_REQUEST ||
        g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_DEVICE_INFO_SEND)
    {
        uint16_t paring_state = multi_link_proto_common_current_paring_state();

        LOG_DBG("is_success = %d, pipe = %d, payload_length = %d, paring_state = %d, paring_status = %d", is_success, pipe, payload_length, paring_state, g_multi_link_proto_device_status);

        if (g_multi_link_proto_device_callbacks->device_get_uptime_ms() - g_paring_start_time_ms >= g_multi_link_proto_device_callbacks->device_get_paring_timout_ms())
        {
            //
            // time out
            //
            g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_PARING_STATUS_TIMEOUT;
            g_multi_link_proto_device_callbacks->device_paring_status_changed(g_multi_link_proto_device_status);
            return;
        }
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
        if (g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_V2_PAIRING_REQUEST)
            multi_link_proto_device_process_gen3p5_pairing_request(is_success, pipe, payload, payload_length, paring_state);
        else
#endif
        if (g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ADDRESS_REQUEST)
            multi_link_proto_device_process_host_address_request(is_success, pipe, payload, payload_length, paring_state);
        else if (g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_HOST_ID_REQUEST)
            multi_link_proto_device_process_host_id_request(is_success, pipe, payload, payload_length, paring_state);
        else if (g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_PARING_STATUS_DEVICE_INFO_SEND)
            multi_link_proto_device_process_device_info_send(is_success, pipe, payload, payload_length, paring_state);
    }
    else if (g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_PREPARE ||
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
             g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE_STEP0 ||
#endif
             g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE)
    {
        LOG_DBG("is_success = %d, pipe = %d, payload_length = %d, state = %d, dynamic_key_status = %d", is_success, pipe, payload_length, g_multi_link_dynamic_key_info.state, g_multi_link_proto_device_status);

        if (g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_PREPARE)
        {
            if (g_multi_link_proto_device_callbacks->device_get_uptime_ms() - g_dynamic_key_prepare_start_time_ms >= g_multi_link_proto_device_callbacks->device_get_dynamic_key_prepare_timout_ms())
            {
                //
                // time out
                //
                g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_TIMEOUT;
                g_multi_link_proto_device_callbacks->device_dynamic_key_status_changed(g_multi_link_proto_device_status);
                return;
            }

            multi_link_proto_device_process_dynamic_key_prepare(is_success, pipe, payload, payload_length);
        }
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
        else if (g_multi_link_proto_device_status < MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE)
        {
            multi_link_general_random_data_0_ext_t *multi_link_general_random_data_0_ext = multi_link_proto_common_general_random_data(MULTI_LINK_DATA_RANDOM_TYPE_INJECTION);

            if (multi_link_general_random_data_0_ext)
            {
                if(!g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (uint8_t *)multi_link_general_random_data_0_ext, sizeof(multi_link_general_random_data_0_ext_t)))
                {
                    LOG_ERR("multi_link_general_random_data_0_ext send failed");
                }
                else
                {
                    g_multi_link_proto_device_status++;
                    if(g_multi_link_proto_device_status >= MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE)
                    {
                        g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_KEY_DONE;
                        g_multi_link_proto_device_callbacks->device_dynamic_key_status_changed(g_multi_link_proto_device_status);
                    }
                }
            }
            else
            {
                LOG_ERR("multi_link_proto_common_general_random_data failed");
            }
        }
#endif
        else if (g_multi_link_dynamic_key_info.is_dynamic_key_done && pipe == g_data_pipe)
        {
            bool is_dynamic_key_request = false;
            //
            // If failed, that means either dongle disconnected or transmistion error.
            // We will retry till error timeout
            //
            if(!is_success)
            {
                if(g_data_send_error_start_time_ms == 0)
                {
                    g_data_send_error_start_time_ms = g_multi_link_proto_device_callbacks->device_get_uptime_ms();
                }

                if((g_multi_link_proto_device_callbacks->device_get_uptime_ms() - g_data_send_error_start_time_ms) >= 5000) // TODO get timout from callback
                {
                    g_data_send_error_start_time_ms = 0;                    
                    g_multi_link_proto_device_callbacks->device_data_send_status(false);
                    is_dynamic_key_request = true;
                }
                else
                {
                    LOG_WRN("Sending data packet again due to error");

                    g_multi_link_proto_device_callbacks->device_backoff(g_data_pipe, 0);

                    if(!g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (uint8_t *)g_multi_link_dynamic_key_info.current_packet_buffer, g_multi_link_dynamic_key_info.current_packet_buffer_length))
                    {
                        g_data_send_error_start_time_ms = 0;
                        g_multi_link_proto_device_callbacks->device_data_send_status(false);
                        is_dynamic_key_request = true;
                    }
                }
            }
            else
            {
                g_data_send_error_start_time_ms = 0;

                if (g_multi_link_dynamic_key_info.is_dynamic_key_done && g_multi_link_dynamic_key_info.current_packet_buffer[0] == g_multi_link_dynamic_key_info.cmd_data )
                {
                    bool can_send_data_ready = true;

                    //
                    // We may get ACK after sending data packet
                    //
                    if(payload_length > 0 && payload[0] != g_multi_link_dynamic_key_info.cmd_data && payload[0] == GZP_CMD_DONGLE_RESET_EX)
                    {
                        //
                        // If is_dynamic_key_done and got this ACK, dongle got reset so device need to send last packet again
                        // and will go in dynamic key exchange
                        //
                        LOG_HEXDUMP_INF((uint8_t *)payload, payload_length, "Dongle reset payload :");

                        // Inform that last packet failed and retry
                        g_multi_link_proto_device_callbacks->device_data_send_status(false);
                        is_dynamic_key_request = true;


                    }
                    else if(payload_length > 0 && payload[0] == g_multi_link_dynamic_key_info.cmd_data)
                    {

                        //
                        // This ACK payload for this device
                        //

                        LOG_HEXDUMP_INF((uint8_t *)payload, payload_length, "Our payload :");


                        if(payload[1] == ACK_COMMAND_REQUEST_DYNAMIC_KEY_UPDATE)
                        {
                            is_dynamic_key_request = true;
                        }
                        else if(payload[1] == ACK_COMMAND_SET_SETTINGS && payload_length >= sizeof(multi_link_setting_ack_info_header_ex_t) && g_multi_link_dynamic_key_info.is_dynamic_key_done)
                        {
                            //
                            // Same setting ACK can be added multiple time from dongle so avoid processing by checking sequence
                            //
                            multi_link_setting_ack_info_ex_t* multi_link_setting_ack_info_ex = (multi_link_setting_ack_info_ex_t*)payload;
                           
                            if(multi_link_setting_ack_info_ex->header.sequence != g_multi_link_dynamic_key_info.settings_sequence)
                            {
                                multi_link_proto_device_decrypt_ack_info(multi_link_setting_ack_info_ex);

                                LOG_HEXDUMP_DBG((uint8_t *)payload, payload_length, "Decrypted payload :");

                                g_multi_link_dynamic_key_info.settings_sequence = multi_link_setting_ack_info_ex->header.sequence;
                                
                                bool status = g_multi_link_proto_device_callbacks->device_set_settings(multi_link_setting_ack_info_ex->header.settings_type, (uint8_t*)multi_link_setting_ack_info_ex->payload, multi_link_setting_ack_info_ex->header.payload_length) ;
                                
                                if(multi_link_setting_ack_info_ex->header.is_conform_required)
                                {
                                    multi_link_setting_conformation_data_ex_t multi_link_setting_conformation_data_ex;
                                    multi_link_setting_conformation_data_ex.type = MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS;
                                    multi_link_setting_conformation_data_ex.sub_type = MULTI_LINK_DEVICE_SETTINGS_TYPE_CONFORM;
                                    multi_link_setting_conformation_data_ex.settings_type = multi_link_setting_ack_info_ex->header.settings_type;
                                    multi_link_setting_conformation_data_ex.sequence = multi_link_setting_ack_info_ex->header.sequence;
                                    multi_link_setting_conformation_data_ex.status = status ? 1 : 0;

                                    multi_link_data_ex_t *multi_link_data_ex = multi_link_proto_common_encode_data_ex((uint8_t* )&multi_link_setting_conformation_data_ex, sizeof(multi_link_setting_conformation_data_ex_t), &g_multi_link_dynamic_key_info);

                                    if (multi_link_data_ex)
                                    {
                                        if(!g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (uint8_t *)multi_link_data_ex, sizeof(multi_link_data_ex_t)))
                                        {
                                            
                                            LOG_ERR("device_send_packet failed(setting conformation)");
                                        }
                                        else
                                        {
                                            can_send_data_ready = false;
                                        }
                                    }
                                    else
                                    {
                                        LOG_ERR("multi_link_proto_common_encode_data_ex failed(setting conformation)");
                                    }
                                }
                            }

                        }
                        else
                        {
                            LOG_HEXDUMP_WRN((uint8_t *)payload, payload_length, "Our unknown payload :");
                        }
                    }
                    else if(payload_length > 0 && payload[0] != g_multi_link_dynamic_key_info.cmd_data && payload[0] >= GZP_CMD_DATA_EX_START && payload[0] <= GZP_CMD_DATA_EX_END)
                    {
                        //
                        // This ACK payload for other device and we got it so wait for some times so other device can get time to take it when this device is sending data at very high rate.
                        // Also we have noticed when this happens, this device's last packet get lost so try sending last packet again and prey
                        //

                        LOG_HEXDUMP_ERR((uint8_t *)payload, payload_length, "Other device payload :");
                        g_multi_link_proto_device_callbacks->device_sleep_ms(5);

                        if(!g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (uint8_t *)g_multi_link_dynamic_key_info.current_packet_buffer, g_multi_link_dynamic_key_info.current_packet_buffer_length))
                        {
                            g_multi_link_proto_device_callbacks->device_data_send_status(false);
                            is_dynamic_key_request = true;
                        }
                    }
                    else
                    {
                        if(payload_length > 0)
                        {
                            LOG_HEXDUMP_WRN((uint8_t *)payload, payload_length, "Not expected payload :");
                        }
                    }

                    if(!is_dynamic_key_request && can_send_data_ready)
                    {
                        //
                        // We are here means our last packet sent successfully and ready for next packet
                        //
                        g_is_send_data_ready = true;
                        g_multi_link_proto_device_callbacks->device_data_send_status(true);

                    }
                }                
            }

            if(is_dynamic_key_request)
            {
                multi_link_proto_device_request_dynamic_key();
            }

        }
    }
}

int multi_link_proto_device_send_data(uint8_t *data, uint32_t data_length)
{
    //g_multi_link_proto_device_callbacks->device_sleep_ms(1000);

    if (g_multi_link_dynamic_key_info.is_dynamic_key_done)
    {
        if(g_is_send_data_ready)
        {
                g_is_send_data_ready = false;

                multi_link_data_ex_t *multi_link_data_ex = multi_link_proto_common_encode_data_ex(data, data_length, &g_multi_link_dynamic_key_info);

                if (multi_link_data_ex)
                {
                    if(!g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (uint8_t *)multi_link_data_ex, sizeof(multi_link_data_ex_t)))
                    {
                        
                        g_is_send_data_ready = true;

                        LOG_ERR("device_send_packet failed");
                        return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_SEND_PACKET_ERROR;
                    }
#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
                    else
                    {
                        multi_link_proto_common_dynamic_key_seed_reflash(g_multi_link_dynamic_key_info.device_nonce, g_multi_link_dynamic_key_info.dongle_nonce, sizeof(g_multi_link_dynamic_key_info.device_nonce), 0);
                    }
#endif
                }
                else
                {
                    g_is_send_data_ready = true;

                    LOG_ERR("multi_link_proto_common_encode_data_ex failed");
                    return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_ENCODING_ERROR;
                }
        }
        else
        {
            LOG_ERR("send data is not ready");
            return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_NOT_READY_ERROR;
        }
    }
    else
    {
        if(g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_UNKNOW)
        {
            //
            // Dynamic key is not done so start preparing key
            //

#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
            multi_link_proto_device_start(0, 0, 0);
#else
            multi_link_proto_device_start(0, 0);
#endif

        }

        return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING; 

        
    }

    return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE;
}

bool multi_link_proto_device_is_send_data_ready()
{
    return g_is_send_data_ready;
}

void multi_link_proto_device_request_dynamic_key()
{
    g_multi_link_proto_device_status = MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_UNKNOW;
    g_multi_link_dynamic_key_info.is_dynamic_key_done = false;
    g_is_send_data_ready = true;

}

uint16_t multi_link_proto_device_get_dongle_api_version()
{
    if (g_multi_link_dynamic_key_info.is_dynamic_key_done)
    {
        return g_multi_link_dynamic_key_info.api_version;
    }

    return 0xFFFF; // invalid version
}

#if CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT
///*****************************************************************************
//* Function	: 
//* Description	: 
//* Input		: None
//* Output		: None
//* Return		: None
//* Note		: None
///*****************************************************************************/
bool multi_link_proto_device_key_seed_reflash(void)
{
    return(multi_link_proto_common_dynamic_key_seed_reflash(g_multi_link_dynamic_key_info.device_nonce, g_multi_link_dynamic_key_info.dongle_nonce, sizeof(g_multi_link_dynamic_key_info.device_nonce),gUpdateCount));
}
///*****************************************************************************
//* Function	:
//* Description	:
//* Input		: None
//* Output		: None
//* Return		: None
//* Note		: None
///*****************************************************************************/
int multi_link_proto_device_send_random_data(uint8_t setting)
{
    if (g_multi_link_dynamic_key_info.is_dynamic_key_done)
    {
        if(g_is_send_data_ready)
        {
                g_is_send_data_ready = false;

                multi_link_general_random_data_0_ext_t *multi_link_general_random_data_0_ext = multi_link_proto_common_general_random_data(setting);

                if (multi_link_general_random_data_0_ext)
                {
                    if(!g_multi_link_proto_device_callbacks->device_send_packet(g_data_pipe, (uint8_t *)multi_link_general_random_data_0_ext, sizeof(multi_link_general_random_data_0_ext_t)))
                    {
                        g_is_send_data_ready = true;

                        LOG_ERR("device_send_packet failed");

                        return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_SEND_PACKET_ERROR;
                    }
                }
                else
                {
                    g_is_send_data_ready = true;

                    LOG_ERR("multi_link_proto_common_encode_data_ex failed");

                    return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_ENCODING_ERROR;
                }
        }
        else
        {
            LOG_ERR("send data is not ready");
            return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_NOT_READY_ERROR;
        }
    }
    else
    {
        if(g_multi_link_proto_device_status == MULTI_LINK_PROTO_DEVICE_DYNAMIC_KEY_STATUS_UNKNOW)
        {
            //
            // Dynamic key is not done so start preparing key
            //
            multi_link_proto_device_start(0, 0, 0);
        }
        return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DYNAMIC_KEY_REQUESTING;
    }
    return MULTI_LINK_PROTO_DEVICE_SEND_DATA_STATUS_DONE;
}
#endif