#ifndef _MULTI_LINK_PROTO_COMMON_H_
#define _MULTI_LINK_PROTO_COMMON_H_

#if defined ( __CC_ARM   )
#pragma anon_unions
#endif

 

#include <stdint.h>
#include <stdio.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#include <limits.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

    #define MULTI_LINK_API_VERSION 1
     #define MULTI_LINK_GEN3P5_API_VERSION 2  //Gen3.5
    #define GAZELL_HOST_PARING_PIPE 0
    /*
    // New device should not use it
    #define GAZELL_HOST_ENCRYPTED_PIPE 1 
    #define GAZELL_HOST_UNENCRYPTED_PIPE 2
    */
    #define GAZELL_HOST_OTA_PIPE 3
    #define GAZELL_HOST_KEYBOARD_PIPE 4
    #define GAZELL_HOST_MOUSE_PIPE 5

    #ifndef FIELD_SIZEOF
    #define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
    #endif
    
    #define DEVICE_LIB_BUILD 0

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)

    #define psa_key_id_t uint32_t
    
    #define NRF_CRYPTO_EXAMPLE_ECDH_PUBLIC_KEY_SIZE (65)

    #define NRF_CRYPTO_EXAMPLE_ECDH_KEY_BITS (256)
    typedef struct
    {
        uint8_t TPubK1[NRF_CRYPTO_EXAMPLE_ECDH_PUBLIC_KEY_SIZE];

        uint8_t TPubK2[NRF_CRYPTO_EXAMPLE_ECDH_PUBLIC_KEY_SIZE];

        uint32_t TPrivK1;

        uint8_t SKTF1[32];
    } 
    ecdh_key_t;

    extern uint32_t UpdateCount;
    
    extern uint8_t G3p5_paired_init_status;

    extern uint8_t G3p5_dk_status;

    extern uint8_t g_taps_length;

    extern uint8_t g_taps_size;

    extern uint8_t *g_taps_polynomial;

    extern uint8_t g_taps_length_p1;

    extern uint8_t *g_taps_polynomial_p1;

    extern uint64_t *g_prime_index;

    extern uint8_t *g_hash_lookup;

    extern uint8_t *g_lprime_list;
    
#endif

    typedef bool (*p_generate_random)(uint8_t *result, size_t result_length);
    typedef psa_key_id_t (*p_create_ecb_128)(const uint8_t key[]); // auto release when recreate
    typedef bool (*p_hmac_sha_256_raw)(const uint8_t key[], const uint8_t *data, const size_t data_length, uint8_t result[]);
    typedef bool (*p_hmac_sha_256)(const psa_key_id_t key_handle, const uint8_t *data, const size_t data_length, uint8_t result[]);
    typedef bool (*p_ecb_128_raw)(const uint8_t key[], const uint8_t *data, const size_t data_length, uint8_t *result, size_t result_length);
    typedef bool (*p_ecb_128)(const psa_key_id_t key_handle, const uint8_t *data, const size_t data_length, uint8_t *result, size_t result_length);
   
    typedef bool (*p_load_key)(uint8_t key_type, uint8_t* key, size_t key_length);
    typedef psa_key_id_t (*p_load_key_handle)(uint8_t key_type);
#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
    typedef int (*p_generate_ecdh_data)(ecdh_key_t *key_info);
    typedef int (*p_generate_ecdh_shared_data)(uint32_t *prvkey, uint8_t *pubkey, uint8_t pubkey_length, uint8_t *sharekey, uint8_t sharekey_length);
    typedef bool (*p_load_g3p5_data)(uint8_t key_type, void** g3p5_data);
#endif


    enum gzp_cmd_ex
    {
        /** Host Address Request (Device to Dongle) */
        GZP_CMD_HOST_ADDRESS_REQ_EX = 0x70,
        /** Host Address Fetch Request – Initial (Device to Dongle) */
        GZP_CMD_HOST_ADDRESS_FETCH_REQ_INIT_EX = 0x71,
        /** Host Address Pairing Challenge (Dongle to Device) */
        GZP_CMD_HOST_ADDRESS_PAIRING_CHALLENGE_EX = 0x31,
        /** Host Address Pairing Response (Device to Dongle) */
        GZP_CMD_HOST_ADDRESS_PAIRING_RESPONSE_EX = 0x72,
        /** Host Address Fetch Request – Final (Device to Dongle) */
        GZP_CMD_HOST_ADDRESS_FETCH_REQ_FINAL_EX = 0x73,
        /** Host Address Response (Dongle to Device) */
        GZP_CMD_HOST_ADDRESS_RESPONSE_EX = 0x33,
        /** Host Id request (Device to Dongle) */
        GZP_CMD_HOST_ID_REQ_EX = 0x75,
        /** Host Id Fetch Request – Initial (Device to Dongle) */
        GZP_CMD_HOST_ID_FETCH_REQ_INIT_EX = 0x76,
        /** Host Id Pairing Challenge (Dongle to Device) */
        GZP_CMD_HOST_ID_PAIRING_CHALLENGE_EX = 0x36,
        /** Host Id Pairing Response (Device to Dongle) */
        GZP_CMD_HOST_ID_PAIRING_RESPONSE_EX = 0x77,
        /** Host Id Fetch Request – Final (Device to Dongle) */
        GZP_CMD_HOST_ID_FETCH_REQ_FINAL_EX = 0x78,
        /** Host Id Response (Dongle to Device) */
        GZP_CMD_HOST_ID_RESPONSE_EX = 0x38,
        /** Device information  – Final (Device to Dongle) */
        GZP_CMD_DEVICE_INFO_SEND_EX = 0x7C,
        /** Device information fetch response – (Device to Dongle) */
        GZP_CMD_DEVICE_INFO_FETCH_RESPONSE_EX = 0x7D,
        /** Device information response – Final (Dongle to Device) */
        GZP_CMD_DEVICE_INFO_RESPONSE_EX = 0x3D,
        /** Key Update prepare (Device to Dongle) */
        GZP_CMD_DYNAMIC_KEY_PREPARE_EX = 0x64,
        /** Key Update response (Dongle to Device) */
        GZP_CMD_DYNAMIC_KEY_RESPONSE_EX = 0x25,
        /** Key Update prepare (Device to Dongle) */
        GZP_CMD_DYNAMIC_KEY_FETCH_EX = 0x65,
        /** Dongle got reset (Dongle to Device) */
        GZP_CMD_DONGLE_RESET_EX = 0x39,
        /** Data CMD per device (Device to Dongle) */
        GZP_CMD_DATA_EX_START = 0x91,
        GZP_CMD_DATA_EX_END = 0x96,

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
        GZP_CMD_DEVICE_PAIRING_REQUEST_V2_EX= 0x80,
        GZP_CMD_DONGLE_PAIRING_RESPOND_V2_EX= 0x3E,
        GZP_CMD_ECDH_KEY_EXCHANGE_START= 0x81,
        GZP_CMD_ECDH_KEY_EXCHANGE_END= 0x85,
        /** Key Update prepare v2 (Device to Dongle) */
        GZP_CMD_DYNAMIC_KEY_PREPARE_EX_v2 = 0x6F,
        GZP_CMD_DYNAMIC_KEY_FETCH_EX_v2 = 0x6E,
#endif

    };

    enum ACK_COMMAND_EX
    {
        ACK_COMMAND_REQUEST_DYNAMIC_KEY_UPDATE = 0x41,        
        ACK_COMMAND_SET_SETTINGS,

    };

    enum device_type_ex
    {
        DEVICE_TYPE_KEYBOARD = 1,
        DEVICE_TYPE_MOUSE = 2,
    };

    #define VENDOR_ID_UNKNOWN 0
    #define VENDOR_ID_CHICONY 1
    #define VENDOR_ID_PRIMAX 2
    #define VENDOR_ID_LITEON 3
    #define VENDOR_ID_RESEVERED_4 4
    #define VENDOR_ID_RESEVERED_5 5
    #define VENDOR_ID_RESEVERED_6 6
    #define VENDOR_ID_RESEVERED_7 7

    enum multi_link_paring_state_ex
    {
        MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_EX, // State 0
        MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT, // State 1
        MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX, // State 2
        MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX, // State 3
        MULTI_LINK_PARING_STATE_HOST_ID_REQ_EX, // State 4
        MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT, // State 5
        MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX, // State 6
        MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX, // State 7
        MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX, // State 8
        MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX, // State 9
        #if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
        MULTI_LINK_PARING_STATE_PAIRING_REQUEST_V2_EX=0xFFF1, // State 0
        MULTI_LINK_PARING_STATE_ECDH_DATA_EXCHANGE_EX=0xFFF2, // State 0
        MULTI_LINK_PARING_STATE_HOST_ADDRESS_RE_REQ_EX, // State 0
        #endif
        MULTI_LINK_PARING_STATE_UNKNOWN = 0xFFFF
    };

    enum multi_link_device_data_type
    {
        MULTI_LINK_DEVICE_DATA_TYPE_KBD_GENERAL_KEY = 0x01,
        MULTI_LINK_DEVICE_DATA_TYPE_MOUSE = 0x2,
        MULTI_LINK_DEVICE_DATA_TYPE_KBD_CONSUMER_KEY = 0x3,
        MULTI_LINK_DEVICE_DATA_TYPE_KBD_POWER_CONTROL_KEY = 0x4,
        MULTI_LINK_DEVICE_DATA_TYPE_KBD_BATTERY_STATUS = 0x6, // only use for old protocol. New device must use MULTI_LINK_DEVICE_DATA_TYPE_BATTERY_LEVEL
        MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS = 0x20,
        MULTI_LINK_DEVICE_DATA_TYPE_BATTERY_LEVEL, // send battery level
        MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_0, // host name 0
        MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_1, // host name 1
        MULTI_LINK_DEVICE_DATA_TYPE_FW_VERSION_CAPABILITY, // send firmware version and capability
        #if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
        MULTI_LINK_DEVICE_DATA_TYPE_RANDOM_NONCE,
        #endif
    };

    #define  MULTI_LINK_DEVICE_SETTINGS_GET_MASK  0x80
    enum multi_link_device_settings_type // This must be use as sub_type if MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS is set
    {
        MULTI_LINK_DEVICE_SETTINGS_TYPE_CONFORM = 1, // Not to use by device, dongle or app, API use internal
        MULTI_LINK_DEVICE_SETTINGS_TYPE_LED_STATUS_SET = MULTI_LINK_DEVICE_SETTINGS_TYPE_CONFORM + 10,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_ENTER_OTA_MODE_SET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_LED_STATUS_SET + 1), // Value send dongle to device
        MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_SET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_ENTER_OTA_MODE_SET + 1),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_KEEP_CONNECTION_SET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_SET + 1),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_KEEP_CONNECTION_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_KEEP_CONNECTION_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_SETTING_SET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_KEEP_CONNECTION_SET + 1),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_SETTING_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_SETTING_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),        
        MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_SET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_SETTING_SET + 1),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_BUTTONS_GET = ((MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_SET + 1) | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_PROXIMITY_BACKLIGHTING_SET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_SET + 2),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_PROXIMITY_BACKLIGHTING_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_PROXIMITY_BACKLIGHTING_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_UPDATE_CHANNEL_TABLE_SET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_PROXIMITY_BACKLIGHTING_SET + 1),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_UPDATE_KEY_SET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_UPDATE_CHANNEL_TABLE_SET + 1),

        MULTI_LINK_DEVICE_SETTINGS_TYPE_PPID_SET = 0x14,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_PPID_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_PPID_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
 

        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_START = 0x61, // Above all values must need to add in this header file and documented in connect spec with corresponding data structure
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_0 = MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_START, // sample for vendor to use for own purpose, From this value vendor can define there custom  GET/SET settings and do not need to documented in connect spec
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_1,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_2,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_3,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_4,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_5,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_6,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_7,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_8,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_9,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_10,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_11,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_12,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_13,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_14,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_15,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_16,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_17,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_18,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_19,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_20,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_21,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_22,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_23,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_24,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_25,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_26,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_27,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_28,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_29,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_0 = (MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_SET_0 | MULTI_LINK_DEVICE_SETTINGS_GET_MASK), // sample for vendor to use for own purpose
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_1,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_2,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_3,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_4,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_5,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_6,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_7,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_8,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_9,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_10,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_11,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_12,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_13,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_14,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_15,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_16,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_17,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_18,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_19,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_20,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_21,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_22,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_23,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_24,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_25,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_26,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_27,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_28,
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET_29,
		
        // new for 2026 DellHIDFeatureControlSupplement_CY2025_v1a_20042025
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VERTICAL_SMOOTH_SCROLL_SPEED_SET = 0x6a, //debug purpose 3byte
        MULTI_LINK_DEVICE_SETTINGS_TYPE_VERTICAL_SMOOTH_SCROLL_SPEED_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_VERTICAL_SMOOTH_SCROLL_SPEED_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_PRESET_LIST_SET = 0x7a,// 11byte
        MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_PRESET_LIST_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_PRESET_LIST_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_SCROLL_OPTIONS_SET = 0x7b, // 1byte
        MULTI_LINK_DEVICE_SETTINGS_TYPE_SCROLL_OPTIONS_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_SCROLL_OPTIONS_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_FN_ROW_OPTIONS_SET = 0x7c, // 3byte
        MULTI_LINK_DEVICE_SETTINGS_TYPE_FN_ROW_OPTIONS_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_FN_ROW_OPTIONS_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),
        MULTI_LINK_DEVICE_SETTINGS_TYPE_MACRO_KEY_SET = 0x7d, // 2byte
        MULTI_LINK_DEVICE_SETTINGS_TYPE_MACRO_KEY_GET = (MULTI_LINK_DEVICE_SETTINGS_TYPE_MACRO_KEY_SET | MULTI_LINK_DEVICE_SETTINGS_GET_MASK),

		
    };
    enum multi_link_static_data_type_ex
    {
        MULTI_LINK_STATIC_DATA_TYPE_TAPS_POLY,
        MULTI_LINK_STATIC_DATA_TYPE_TAPS_POLY_P1,
        MULTI_LINK_STATIC_DATA_TYPE_HASH_LOOKUP,
        MULTI_LINK_STATIC_DATA_TYPE_LPRIME,
        MULTI_LINK_STATIC_DATA_TYPE_PRIME
    };
    
    enum multi_link_static_key_type_ex
    {
        MULTI_LINK_STATIC_KEY_TYPE_KC,
        MULTI_LINK_STATIC_KEY_TYPE_KC_HMAC,
        MULTI_LINK_STATIC_KEY_TYPE_DK_HMAC,
    };

    enum multi_link_static_key_handle_ex
    {
        MULTI_LINK_STATIC_KEY_HANDLE_KC_AES128_ECB,
        MULTI_LINK_STATIC_KEY_HANDLE_KC_HMAC,
        MULTI_LINK_STATIC_KEY_HANDLE_DK_AES128_ECB,
        MULTI_LINK_STATIC_KEY_HANDLE_DK_HMAC,
#if (CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT)        
        MULTI_LINK_STATIC_KEY_HANDLE_KC_HMAC_V2,
        MULTI_LINK_STATIC_KEY_HANDLE_DK_HMAC_V2,
#endif        
    };

#if (CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT)
    enum
    {
        G3P5_PAIRING_INIT_STATUS_NONE=0x00,
        G3P5_PAIRING_INIT_STATUS_START_STEP1,
        G3P5_PAIRING_INIT_STATUS_START_STEP2,
        G3P5_PAIRING_INIT_STATUS_ECDH_DATA_EXCHANGE,
        G3P5_PAIRING_INIT_STATUS_FAILED=G3P5_PAIRING_INIT_STATUS_ECDH_DATA_EXCHANGE+5,
        G3P5_PAIRING_INIT_STATUS_SUCCESS,
    };

     enum
    {
        G3P5_DYNAMIC_KEY_STATUS_NONE=0x00,
        G3P5_DYNAMIC_KEY_STATUS_START_STEP1,
        G3P5_DYNAMIC_KEY_STATUS_START_STEP2,
        G3P5_DYNAMIC_KEY_STATUS_ECDH_DATA_EXCHANGE,
        G3P5_DYNAMIC_KEY_STATUS_FAILED,
        G3P5_DYNAMIC_KEY_STATUS_SUCCESS,
    };

    enum multi_link_data_random_type_ex
    {
        MULTI_LINK_DATA_RANDOM_TYPE_INJECTION= 0x55,
        MULTI_LINK_DATA_RANDOM_TYPE_PAIRING_KEY_CHANGE= 0xAA,
    };

    enum multi_link_common_host_address_pairing_challenge_ex
    {
        MULTI_LINK_COMMON_HOST_ADDRESS_PAIRING_CHALLENGE_SUCCESS = 0,
        MULTI_LINK_COMMON_HOST_ADDRESS_PAIRING_CHALLENGE_FALIED_0= 0x310,
        MULTI_LINK_COMMON_HOST_ADDRESS_PAIRING_CHALLENGE_FALIED_1,
        MULTI_LINK_COMMON_HOST_ADDRESS_PAIRING_CHALLENGE_FALIED_2,
        MULTI_LINK_COMMON_HOST_ADDRESS_PAIRING_CHALLENGE_FALIED_3,
    };
    enum multi_link_common_host_address_response_ex
    {
        MULTI_LINK_COMMON_HOST_ADDRESS_RESPONSE_SUCCESS = 0,
        MULTI_LINK_COMMON_HOST_ADDRESS_RESPONSE_FALIED_0= 0x510,
        MULTI_LINK_COMMON_HOST_ADDRESS_RESPONSE_FALIED_1,
        MULTI_LINK_COMMON_HOST_ADDRESS_RESPONSE_FALIED_2,
        MULTI_LINK_COMMON_HOST_ADDRESS_RESPONSE_FALIED_3,
    };
    enum multi_link_common_host_id_pairing_challenge_ex
    {
        MULTI_LINK_COMMON_HOST_ID_PAIRING_CHALLENGE_SUCCESS = 0,
        MULTI_LINK_COMMON_HOST_ID_PAIRING_CHALLENGE_FALIED_0= 0x540,
        MULTI_LINK_COMMON_HOST_ID_PAIRING_CHALLENGE_FALIED_1,
        MULTI_LINK_COMMON_HOST_ID_PAIRING_CHALLENGE_FALIED_2,
        MULTI_LINK_COMMON_HOST_ID_PAIRING_CHALLENGE_FALIED_3,
    };
    enum multi_link_common_host_id_response_ex
    {
        MULTI_LINK_COMMON_HOST_ID_RESPONSE_SUCCESS = 0,
        MULTI_LINK_COMMON_HOST_ID_RESPONSE_FALIED_0= 0x560,
        MULTI_LINK_COMMON_HOST_ID_RESPONSE_FALIED_1,
        MULTI_LINK_COMMON_HOST_ID_RESPONSE_FALIED_2,
        MULTI_LINK_COMMON_HOST_ID_RESPONSE_FALIED_3,
    };
    enum multi_link_common_device_info_response_ex
    {
        MULTI_LINK_COMMON_DEVICE_INFO_RESPONSE_SUCCESS = 0,
        MULTI_LINK_COMMON_DEVICE_INFO_RESPONSE_FALIED_0= 0x3D0,
        MULTI_LINK_COMMON_DEVICE_INFO_RESPONSE_FALIED_1,
        MULTI_LINK_COMMON_DEVICE_INFO_RESPONSE_FALIED_2,
        MULTI_LINK_COMMON_DEVICE_INFO_RESPONSE_FALIED_3,
    };
    enum multi_link_common_confirm_request_ex
    {
        MULTI_LINK_COMMON_CONFIRM_REQUEST_SUCCESS = 0,
        MULTI_LINK_COMMON_CONFIRM_REQUEST_FALIED_0= 0x3E0,
        MULTI_LINK_COMMON_CONFIRM_REQUEST_FALIED_1,
    };
    enum multi_link_common_debug_ex
    {
        dDISABLE_INJECTTION_DATA,
        dODM_COMMON_DEBUG_MODE_ENABLE,
    };

    typedef struct
    {
        uint8_t PacketCnt;
        uint8_t LFSR_NS0[3];
        uint8_t PolyIdx;
        uint8_t AppVer;
        uint8_t DeviceID[3];
    } 
    __attribute__((packed)) metadata_t;

    typedef struct
    {
        uint8_t cmd;

        uint8_t device_nonce_value[5];

        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)

            struct
            {
                uint8_t mac[3]; //MAC_L3
                metadata_t md;
            }
            __attribute__((packed)) payload_p; // Plain (p) payload

        }__attribute__((packed));

    } __attribute__((packed)) multi_link_device_pairing_request_v2_ex_t;


    typedef struct
    {
        uint8_t cmd;

        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)

            struct
            {
                uint8_t mac[3];
                uint8_t confirm[13];
            }
            __attribute__((packed)) payload_p; // Plain (p) payload

        }__attribute__((packed));

    } __attribute__((packed)) multi_link_device_confirm_request_ex_t;

    typedef struct
    {
        uint8_t cmd;

        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)

            struct
            {
                uint8_t mac[3]; //MAC_L3
                uint8_t Pn_N[13]; //
            }
            __attribute__((packed)) payload_p; // Plain (p) payload

        }__attribute__((packed));

    } __attribute__((packed)) multi_link_publick_ex_t;
#endif


    typedef struct
    {
        uint8_t cmd;
        uint8_t session_config_backoff_delay;
        uint8_t session_config_retry_interval;
        uint8_t device_nonce_start_value[3];
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_nonce[3]; // Dev-N
                uint8_t device_type;
                uint8_t vendor_id;
                uint8_t device_id[3];
            } __attribute__((packed)) payload_p; // Plain (p) payload
        }__attribute__((packed));

    } __attribute__((packed)) multi_link_host_address_request_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint8_t device_name[10];
            } __attribute__((packed)) payload_p; // Plain (p) payload
        }__attribute__((packed));

    } __attribute__((packed)) multi_link_host_address_fetch_request_init_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t salt_challenge[5];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_address_pairing_challenge_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint8_t response[5];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_address_pairing_response_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint8_t device_name[10];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_address_fetch_request_final_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t host_address[4];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_address_response_ex_t;


    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint8_t host_address_response[5];

            } __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_id_request_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint8_t device_name[10];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_id_fetch_request_init_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t salt_challenge[5];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_id_pairing_challenge_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint8_t response[5];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_id_pairing_response_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint8_t device_name[10]; // last byte is color code
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_id_fetch_request_final_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t host_id[5];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_host_id_response_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint16_t firmware_version;
                uint16_t capability;
                uint16_t kb_layout;
            } __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_device_info_ex_t;


    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_device_info_fetch_response_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_id[3];
                uint8_t result; // 1 success, 0 fail
            }  __attribute__((packed)) payload_p; // Plain (p) payload
        } __attribute__((packed));

    } __attribute__((packed)) multi_link_device_info_response_ex_t;


    typedef struct
    {
        uint8_t cmd;
        uint8_t device_nonce_start_value[3];
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t sminit[3];
                uint8_t device_nonce[5]; // Dev-N
                uint8_t device_id[3];
                uint16_t api_version; // device api version
            } __attribute__((packed)) payload_p; // Plain (p) payload
        }__attribute__((packed));

    } __attribute__((packed)) multi_link_dynamic_key_prepare_ex_t;

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
    typedef struct
    {
        uint8_t cmd;
        uint8_t device_nonce_start_value[5];
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t sminit[3];
                uint8_t device_nonce[5]; // Dev-N
                uint8_t device_id[3];
                uint8_t api_version; // device api version
                uint8_t LFSR_idx;
            } __attribute__((packed)) payload_p; // Plain (p) payload
        }__attribute__((packed));
    } __attribute__((packed)) multi_link_dynamic_key_prepare_ex_v2_t;
#endif

    typedef struct
    {
        uint8_t cmd;
        uint8_t device_nonce_start_value[3];
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t device_nonce[5]; // Dev-N
                uint8_t device_id[3];
            } __attribute__((packed)) payload_p; // Plain (p) payload
        }__attribute__((packed));

    } __attribute__((packed)) multi_link_dynamic_key_fetch_ex_t;

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
    typedef struct
    {
        uint8_t cmd;
        uint8_t device_nonce_start_value[3];
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t sminit[3];
                uint8_t device_id[3];
                uint8_t device_nonce[5]; // Dev-N
                uint8_t api_version; // device api version
                uint8_t LFSR_idx;
            } __attribute__((packed)) payload_p; // Plain (p) payload
        }__attribute__((packed));

    } __attribute__((packed)) multi_link_dynamic_key_fetch_ex_v2_t;
#endif

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t dongle_nonce[5]; // Don-N
                uint8_t cmd_data;
                uint16_t api_version; // dongle api version

            } __attribute__((packed)) payload_p; // Plain (p) payload
        }__attribute__((packed));

    } __attribute__((packed)) multi_link_dynamic_key_response_ex_t;

    typedef struct
    {
        uint8_t cmd;
        union
        {
            uint8_t payload_ep[1]; //  Encrypted (Ep) payload. length == sizeof(payload_p)
            struct
            {
                uint8_t mac[3];
                uint8_t data[13];
            } __attribute__((packed)) payload_p; // Plain (p) payload
        }__attribute__((packed));

    } __attribute__((packed)) multi_link_data_ex_t;

   typedef struct
    {
        uint8_t cmd_data;
        uint8_t ack_cmd;  // This must be ACK_COMMAND_SET_SETTINGS
        uint8_t settings_type; // This must be any of multi_link_device_settings_type
        uint8_t sequence;
        uint8_t is_conform_required : 1;
        uint8_t payload_length : 5;
    } __attribute__((packed)) multi_link_setting_ack_info_header_ex_t;

    typedef struct
    {
        multi_link_setting_ack_info_header_ex_t header;
        uint8_t payload[16];
    } __attribute__((packed)) multi_link_setting_ack_info_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t sub_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_CONFORM
        uint8_t settings_type; // This must be any of multi_link_device_settings_type
        uint8_t sequence;
        uint8_t status; // 1 success, 0 fail
    } __attribute__((packed)) multi_link_setting_conformation_data_ex_t;


    typedef struct
    {
        uint8_t type; // This must be multi_link_device_data_type 
        uint8_t total_packet : 4; // 4bit total_packet for all data. Max 15
        uint8_t packet_id : 4; // 4bit packet_id. 0 to 14
        uint8_t data_length; // Length of data in this packet. Max 10 bytes         
    } __attribute__((packed)) multi_link_multi_packet_data_header_ex_t;

    #define MULTI_LINK_PACKET_DATA_PACKET_ID_MIN 0
    #define MULTI_LINK_PACKET_DATA_PACKET_ID_MAX 14
    #define MULTI_LINK_PACKET_DATA_TOTAL_PACKET_MAX 15
    #define MULTI_LINK_PACKET_DATA_LENGTH_MAX 10
    #define MULTI_LINK_PACKET_SETTING_DATA_SET_LENGTH_MAX 14
    #define MULTI_LINK_PACKET_SETTING_DATA_GET_LENGTH_MAX 9

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_RANDOM_NONCE
        uint8_t setting;
        uint8_t Rdata[8];
    } __attribute__((packed)) multi_link_general_random_data_0_ext_t;
#endif

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_MOUSE
        uint8_t button;
        uint8_t xbit;
        uint8_t xybit;
        uint8_t ybit;
        uint8_t scroll;
        uint8_t pan;
    } __attribute__((packed)) multi_link_mouse_data_ex_t;


    typedef struct 
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_KBD_GENERAL_KEY
        uint8_t modifier;
        uint8_t reserve;
        uint8_t key1;
        uint8_t key2;
        uint8_t key3;
        uint8_t key4;
        uint8_t key5;
        uint8_t key6;
        uint8_t fn_lock : 1;
        uint8_t fn_pressed : 1;
    } __attribute__((packed)) multi_link_kbd_general_key_data_0_ext_t;


    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_KBD_CONSUMER_KEY
        uint8_t key1;
        uint8_t key2;
        uint8_t fn_lock : 1;
        uint8_t fn_pressed : 1;
    } __attribute__((packed))  multi_link_kbd_consumer_key_data_ext_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_BATTERY_LEVEL 
        uint8_t level; // 0 to 100 in percentage 
        uint8_t b0 : 1; // Battery and Charger Information 
        uint8_t b1 : 1; // Battery and Charger Information
        uint8_t b2 : 1; // Battery and Charger Information
        uint8_t b3 : 1; // Battery and Charger Information
        uint8_t b4 : 1; // Battery and Charger Information
        uint8_t b5 : 1; // Battery and Charger Information
        uint8_t b6 : 1; // Battery and Charger Information
        uint8_t b7 : 1; // Battery and Charger Information
        uint16_t charge_cycles_counter;
        uint16_t loww_batt_counter;
        uint16_t mAH : 14; // Numeric value in mAH
        uint16_t multiplier : 2; // Multiplier, 11=x10,10=x5,01=x2, 00=x1
    } __attribute__((packed)) multi_link_battery_data_ex_t;

    /*Battery and Charger Information Bit Pattern
    B7	B6	B5	B4	B3	B2	B1	B0	
                        0	0	0	Unused (Alkaline battery), No charging status
                        0	1	1	Charger connected and charging in progress,Bit-1
                        1	1	1	Charger connected and charging completed,Bit-2
    0	0	0	0	0				AA x1 ,00- Bit 4,3,=00
    0	0	0	0	1				AA x2  -In series, bit 4,3=01
    0	0	0	1	0				AA ||AA  – 2 AA Cells In Parallel
    0	0	1	0	0				AAA x1
    0	0	1	0	1				AAAx2
    0	0	1	1	0				AAA ||AAA  – 2 AAA Cells In Parallel
    0	1	1	1	1				Rechargeable Capacity,Bit 6=1
    Refer to byte-8/9 for Rechargeable Battery Capacity*/
    
    typedef struct
    {
        multi_link_multi_packet_data_header_ex_t header; //header.type must be MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_0 or MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_1 
        uint8_t data[MULTI_LINK_PACKET_DATA_LENGTH_MAX]; 
    } __attribute__((packed)) multi_link_multi_packet_hostname_data_ex_t;

    #define MULTI_LINK_DEVICE_DATA_MULTI_PACKET_TYPE_HOST_NAME_MAX 20 // 19 bytes + 1 null terminator  (URF-8 encoded)

     typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_FW_VERSION_CAPABILITY 
        uint16_t version;
        uint16_t capability;
    } __attribute__((packed)) multi_link_firmware_version_capability_data_ex_t;

    typedef struct
    {
        uint8_t num_lock : 1;
        uint8_t caps_lock : 1;
        uint8_t scroll_lock : 1;
        
    } __attribute__((packed)) multi_link_setting_led_status_set_value_ex_t;
    
    typedef struct
    {
        uint32_t timeslot_period;
        uint8_t channel_selection_policy;
    } __attribute__((packed)) multi_link_setting_ota_enter_set_value_ex_t;

    typedef struct
    {
        uint16_t interval;
        uint16_t duration : 15;
        uint16_t multiplier : 1;
    } __attribute__((packed)) multi_link_setting_keep_connection_set_value_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_KEEP_CONNECTION_GET
        uint16_t remaining_tick_count; // n to 0. last tick can be 0 or 1
    } __attribute__((packed)) multi_link_setting_keep_connection_get_data_ex_t;

    typedef struct
    {
        uint8_t enable_video : 1;
        uint8_t enable_share : 1;
        uint8_t enable_chat : 1;
        uint8_t enable_mic_mute : 1;
        uint8_t reserved_0 : 4;
        uint8_t chat_led_blink_effect_enable : 1;
        uint8_t reserved_1 : 7;
        uint8_t led_lower_brightness : 1;
        uint8_t tap_max_duration : 7;
        uint8_t double_tap_enable : 1;
        uint8_t reserved_2 : 7;        
        uint8_t camera_led : 1;
        uint8_t camera_disable_led : 1;
        uint8_t share_screen_led : 1;
        uint8_t chat_led : 1;
        uint8_t mic_led : 1;
        uint8_t mic_disable_led : 1;
        uint8_t reserved_3 : 2;
    } __attribute__((packed)) multi_link_setting_collab_keys_setting_set_value_ex_t;


    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_SETTING_GET
        multi_link_setting_collab_keys_setting_set_value_ex_t current_value;
    } __attribute__((packed)) multi_link_setting_collab_keys_get_data_ex_t;

    
    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_COLLAB_KEYS_BUTTONS_GET
        uint8_t swipe : 1;
        uint8_t camera : 1;
        uint8_t share_screen : 1;
        uint8_t chat : 1;
        uint8_t mic_mute : 1;
        uint8_t manual_override : 1;        
    } __attribute__((packed)) multi_link_setting_collab_keys_button_get_data_ex_t;


    #define MULTI_LINK_SETTING_DATA_DPI_VALUE_UNKNOW 0


    typedef struct
    {
        uint16_t dpi_value; 
    } __attribute__((packed)) multi_link_setting_dpi_value_set_value_ex_t;
   
    
    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_VALUE_GET
        uint16_t current_dpi_value;
        uint16_t min_dpi_value; // minimum DPI value
        uint16_t max_dpi_value; // maximum DPI value
        uint16_t stepping_delta; // stepping delta to increment dpi value from min to max         
    } __attribute__((packed)) multi_link_setting_dpi_value_get_data_ex_t;



    typedef struct
    {
        uint16_t report_rate_hz; // 250 to n. in Hz. In Little-Endian (LSB-MSB)
    } __attribute__((packed)) multi_link_setting_report_rate_set_value_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_REPORT_RATE_GET
        uint16_t current_report_rate_hz; // 250 to n. in Hz. In Little-Endian (LSB-MSB)        
        uint16_t supported_report_rates_hz[4]; // 250 to n. in Hz. In Little-Endian (LSB-MSB). Terminated last by 0x00 if less that < 4
    } __attribute__((packed)) multi_link_setting_report_rate_get_data_ex_t;


    typedef struct
    {
        uint8_t vendor_data[FIELD_SIZEOF(multi_link_setting_ack_info_ex_t,payload)]; // Any vendor defined data. Data size should not more than this array size
    } __attribute__((packed)) multi_link_setting_vendor_open_set_value_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_VENDOR_OPEN_GET
        uint8_t vendor_data[FIELD_SIZEOF(multi_link_data_ex_t,payload_p.data) - 2];  // Any vendor defined data. Data size should not more than this array size      
    } __attribute__((packed)) multi_link_setting_vendor_open_get_data_ex_t;


    typedef struct
    {
        uint8_t backlight_enable : 1;
        uint8_t proximity_enable : 1;
        uint8_t ambient_enable : 1;
        uint8_t reserved_0 : 5;
        uint8_t backlighting_levels_from_software;
    } __attribute__((packed)) multi_link_setting_backlighting_set_value_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_PROXIMITY_BACKLIGHTING_GET
        multi_link_setting_backlighting_set_value_ex_t current_value;
    } __attribute__((packed)) multi_link_setting_backlighting_get_data_ex_t;

    typedef struct
    {
        uint8_t channel_table[FIELD_SIZEOF(multi_link_setting_ack_info_ex_t,payload)];
    } __attribute__((packed)) multi_link_setting_update_channel_table_set_value_ex_t;

    typedef struct
    {
        uint8_t total_packet : 4; // 4bit total_packet for all data. Max 15
        uint8_t packet_id : 4; // 4bit packet_id. 0 to 14
        uint8_t data_length; // Length of data in this packet. Max 14 bytes         
        uint8_t data[FIELD_SIZEOF(multi_link_setting_ack_info_ex_t,payload) - 2];
    } __attribute__((packed)) multi_link_setting_ppid_set_value_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_PPID_GET
        uint8_t total_packet : 4; // 4bit total_packet for all data. Max 15
        uint8_t packet_id : 4; // 4bit packet_id. 0 to 14
        uint8_t data_length; // Length of data in this packet. Max 9 bytes         
        uint8_t data[FIELD_SIZEOF(multi_link_data_ex_t,payload_p.data) - 4]; 
    } __attribute__((packed)) multi_link_setting_ppid_get_data_ex_t;

    // new for 2026 DellHIDFeatureControlSupplement_CY2025_v1a_20042025

    typedef struct
    {
        uint8_t cmd;    // 0x01=Enable and Set D, T parameters. 
                        // 0x80=Enable  with Default D,T parameters  (No Parameters send bytes 6,7)
                        // 0x81=Disable Wheel Speed up , revert to convention design QDECRegisterDivider(F)=0.5
        uint8_t parameter_D;    //<Attributes for 0x01 commands>
                                // D = Trigger Duration (if 2nd wheel notification comes in less than the defined time)
                                // RF: MSB(B3) ,e.g 0x32(50units x 2ms) , unit time=2ms
        uint8_t parameter_T;    // T = Time to reset back to F=0.5 if no data coming after T duration.
                                // T in 2ms unit., e.g ( 0x96 ) for 300ms . 
    } __attribute__((packed)) multi_link_device_settings_type_vertical_smooth_scroll_speed_set_value_ex_t;
    
    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_VERTICAL_SMOOTH_SCROLL_SPEED_GET
        uint8_t status; // 0x80=Vertical Smooth scroll enabled with default parameter. 0x81=Vertical; Smooth scroll not used 
        uint8_t parameter_D;
        uint8_t parameter_T;
    } __attribute__((packed)) multi_link_device_settings_type_vertical_smooth_scroll_speed_get_value_ex_t;

    typedef struct
    {
        uint8_t default_slot; //  0x01-Slot 1 as active DPI, Device does not take action with invalid parameter’s value .
        uint16_t dpi_value[5]; // 0:Slot 1 , LSB:MSB  (0x40 0x06 =1600dpi)
        // Remarks: 
        // 1.	Empty slots should be filled with 0xFFFF, 0x0000 will be skipped.
        // 2.	Valid DPI range : 400 to Max . 
    } __attribute__((packed)) multi_link_setting_dpi_preset_list_set_value_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_PRESET_LIST_GET
        uint8_t packet_id; // This must be 0x02=packet id 1/2
        uint16_t dpi_value[5]; // 0:Slot 1 , LSB:MSB  (0x40 0x06 =1600dpi)
    } __attribute__((packed)) multi_link_setting_dpi_preset_list_get_data_1_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_DPI_PRESET_LIST_GET
        uint8_t packet_id; // This must be 0x12=packet id 2/2
        uint8_t active : 4; // 4bit active
        uint8_t version : 4; // This must be 3=version3
        uint16_t dpi_min; // LSB:MSB  (0x40 0x06 =1600dpi)
        uint16_t dpi_max; // LSB:MSB  (0x40 0x06 =1600dpi)
    } __attribute__((packed)) multi_link_setting_dpi_preset_list_get_data_2_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_SCROLL_OPTIONS_GET
        uint8_t side_scroll; // 0x01=Scroll Up as Right scroll. (SURS) 0x02=Scroll Down as Right Scroll (SDRS) 
        uint8_t smooth_scroll; //0xA1=Vertical Smooth scroll is enabled. 0xA0=Vertical Smooth scroll is disabled.
    } __attribute__((packed)) multi_link_device_settings_type_scroll_options_get_value_ex_t;
    
    typedef struct
    {
        uint8_t cmd; // 0x61=Apply Inverse (Not using 0x01 to avoid crashing with Ad-hoc commands), 0x00=no Action
        uint8_t fn_inverse_lsb;
        uint8_t fn_inverse_msb;
    } __attribute__((packed)) multi_link_device_settings_type_fn_row_options_set_value_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_FN_ROW_OPTIONS_GET
        uint8_t fn_inverse_lsb;
        uint8_t fn_inverse_msb;
    } __attribute__((packed)) multi_link_device_settings_type_fn_row_options_get_value_ex_t;

    typedef struct
    {
        uint8_t cmd;    // 01	<0x0F>  Macro Enable //M1 to M4 enabled, 4 Bits set 
                        // 02	<0x10>  Macro Disable  //M1 to M4 bits being disabled. 
                        // 03	<0x00> ,no Action  //Not to accidentally disabled Macro .
        
        uint8_t atrributes; //  0x0F=Macro Enable, 0x10=Macro disabled. Other values to be ignored.
        
    } __attribute__((packed)) multi_link_device_settings_type_macro_key_set_value_ex_t;

    typedef struct
    {
        uint8_t type; // This must be MULTI_LINK_DEVICE_DATA_TYPE_SETTINGS 
        uint8_t settings_type; // This must be MULTI_LINK_DEVICE_SETTINGS_TYPE_MACRO_KEY_GET
        uint8_t status;
    } __attribute__((packed)) multi_link_device_settings_type_macro_key_get_value_ex_t;
	
    typedef struct
    {
        uint8_t DYMD0[10];
#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
        uint8_t DYMDX[6];
#endif
        uint8_t MGD0[13];
        uint8_t MGDN[10];
        uint8_t nonce_start_value[3];
#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)        
        uint8_t nonce_start_value_v2[5];
#endif        
        uint32_t f_integer;
        uint8_t f[3];
        uint8_t device_nonce[5]; // Dev-N
        uint8_t mac[16];
        uint32_t state;
        uint8_t current_packet_buffer[32];
        uint32_t current_packet_buffer_length;
        bool is_init_done;
        uint8_t dongle_nonce[5]; // Don-N
        uint8_t cmd_data;        
        bool is_dynamic_key_done;
        uint16_t api_version;  // This API version for device or dongle depend on context
        uint8_t settings_sequence;
        uint8_t ack_key[16];
#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
        uint8_t LSFR_idx;
#endif
        #ifdef DONGLE_BUILD
        uint8_t device_id[3];
        multi_link_data_ex_t last_multi_link_data_ex;
        uint8_t battery_level;
        uint8_t battery_information[7];
        uint8_t dpi_level;
   		bool is_host_name_0_changed;
		bool is_host_name_1_changed;
        #endif
    } multi_link_dynamic_key_info_ex_t;

    typedef union
    {
        uint64_t f_integer;

        uint8_t f[8];
    }
    smstate_t;

    typedef struct
    {
        p_generate_random generate_random;
        p_create_ecb_128 create_ecb_128;
        p_hmac_sha_256_raw hmac_sha_256_raw;
        p_hmac_sha_256 hmac_sha_256;
        p_ecb_128_raw ecb_128_raw;
        p_ecb_128 ecb_128;
        p_load_key load_key;
        p_load_key_handle load_key_handle;
#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
        p_generate_ecdh_data generate_ecdh_data;
        p_generate_ecdh_shared_data generate_ecdh_shared_data;
        p_load_g3p5_data load_g3p5_data;
#endif
    } multi_link_common_callbacks_t;

    void multi_link_proto_common_xor(uint8_t *dst, const uint8_t *src, const uint8_t *pad, uint8_t length);
    int32_t multi_link_proto_common_inverse_modulus(int32_t a, int32_t m);
    
    void multi_link_proto_common_current_packet(const uint8_t **packet, uint32_t *packet_length);
    void multi_link_proto_common_last_state_packet(const uint8_t **packet, uint32_t *packet_length);

    bool multi_link_proto_common_init_callbacks(multi_link_common_callbacks_t *multi_link_common_callbacks);

    bool multi_link_proto_common_init_paring(uint8_t (*device_nonce_start_value)[3]);
    void multi_link_proto_common_clear_paring(void);
    uint16_t multi_link_proto_common_current_paring_state(void);

    multi_link_host_address_request_ex_t *multi_link_proto_common_encode_host_address_request_ex(uint8_t backoff_delay, uint8_t retry_interval, uint8_t device_type, uint8_t vendor_id, uint8_t (*device_id)[3]);
    bool multi_link_proto_common_decode_host_address_request_ex(multi_link_host_address_request_ex_t* multi_link_host_address_request_ex);

    multi_link_host_address_fetch_request_init_ex_t *multi_link_proto_common_encode_host_address_fetch_request_init_ex(uint8_t (*device_id)[3], uint8_t (*device_name)[10]);
    bool multi_link_proto_common_decode_host_address_fetch_request_init_ex(multi_link_host_address_fetch_request_init_ex_t* multi_link_host_address_fetch_request_init);

    multi_link_host_address_pairing_challenge_ex_t* multi_link_proto_common_encode_host_address_pairing_challenge(uint8_t (*device_id)[3]);
    bool multi_link_proto_common_decode_host_address_pairing_challenge(multi_link_host_address_pairing_challenge_ex_t* multi_link_host_address_pairing_challenge, uint8_t (*device_id)[3]);

    multi_link_host_address_pairing_response_ex_t* multi_link_proto_common_encode_host_address_pairing_response(uint8_t (*device_id)[3], uint8_t (*salt_challenge)[5]);
    bool multi_link_proto_common_decode_host_address_pairing_response(multi_link_host_address_pairing_response_ex_t *multi_link_host_address_pairing_response_ex, uint8_t (*device_id)[3]);

    multi_link_host_address_fetch_request_final_ex_t *multi_link_proto_common_encode_host_address_fetch_request_final_ex(uint8_t (*device_id)[3], uint8_t (*device_name)[10]);
    bool multi_link_proto_common_decode_host_address_fetch_request_final_ex(multi_link_host_address_fetch_request_final_ex_t * multi_link_host_address_fetch_request_final_ex);

    multi_link_host_address_response_ex_t *multi_link_proto_common_encode_host_address_response_ex(uint8_t (*host_address)[4]);
    bool multi_link_proto_common_decode_host_address_response_ex(multi_link_host_address_response_ex_t *multi_link_host_address_response_ex);

    multi_link_host_id_request_ex_t *multi_link_proto_common_encode_host_id_request_ex(uint8_t (*device_id)[3]);
    bool multi_link_proto_common_decode_host_id_request_ex(multi_link_host_id_request_ex_t * multi_link_host_id_request_ex);

    multi_link_host_id_fetch_request_init_ex_t *multi_link_proto_common_encode_host_id_fetch_request_init_ex(uint8_t (*device_id)[3], uint8_t (*device_name)[10]);
    bool multi_link_proto_common_decode_host_id_fetch_request_init_ex(multi_link_host_id_fetch_request_init_ex_t* multi_link_host_id_fetch_request_init);

    multi_link_host_id_pairing_challenge_ex_t* multi_link_proto_common_encode_host_id_pairing_challenge(uint8_t (*device_id)[3], uint8_t (*dongle_challenge)[5]);
    bool multi_link_proto_common_decode_host_id_pairing_challenge(multi_link_host_id_pairing_challenge_ex_t* multi_link_host_id_pairing_challenge, uint8_t (*device_id)[3]);

    multi_link_host_id_pairing_response_ex_t* multi_link_proto_common_encode_host_id_pairing_response(uint8_t (*device_id)[3], uint8_t (*salt_challenge)[5], uint8_t (*device_response)[5]);
    bool multi_link_proto_common_decode_host_id_pairing_response(multi_link_host_id_pairing_response_ex_t *multi_link_host_id_pairing_response_ex, uint8_t (*device_id)[3]);

    multi_link_host_id_fetch_request_final_ex_t *multi_link_proto_common_encode_host_id_fetch_request_final_ex(uint8_t (*device_id)[3], uint8_t (*device_name)[10]);
    bool multi_link_proto_common_decode_host_id_fetch_request_final_ex(multi_link_host_id_fetch_request_final_ex_t * multi_link_host_id_fetch_request_final_ex);

    multi_link_host_id_response_ex_t *multi_link_proto_common_encode_host_id_response_ex(uint8_t (*host_id)[5]);
    bool multi_link_proto_common_decode_host_id_response_ex(multi_link_host_id_response_ex_t *multi_link_host_id_response_ex);

    multi_link_device_info_ex_t *multi_link_proto_common_encode_device_info_ex(uint8_t (*device_id)[3], uint16_t firmware_version, uint16_t capability, uint16_t kb_layout);
    bool multi_link_proto_common_decode_device_info_ex(multi_link_device_info_ex_t *multi_link_device_info_ex,uint8_t (*device_id)[3]);

    multi_link_device_info_fetch_response_ex_t *multi_link_proto_common_encode_device_info_fetch_response_ex(uint8_t (*device_id)[3]);
    bool multi_link_proto_common_decode_device_info_fetch_response_ex(multi_link_device_info_fetch_response_ex_t * multi_link_device_info_fetch_response_ex, uint8_t (*device_id)[3]);

    multi_link_device_info_response_ex_t *multi_link_proto_common_encode_device_info_response_ex(uint8_t (*device_id)[3], uint8_t result);
    bool multi_link_proto_common_decode_device_info_response_ex(multi_link_device_info_response_ex_t *multi_link_device_info_response_ex, uint8_t (*device_id)[3]);

    bool multi_link_proto_common_init_dynamic_key(uint8_t (*host_id)[5], uint8_t (*challenge)[5], uint8_t (*response)[5], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);

    multi_link_dynamic_key_prepare_ex_t* multi_link_proto_common_encode_dynamic_key_prepare_ex(uint8_t (*host_id)[5], uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    bool multi_link_proto_common_decode_dynamic_key_prepare_ex(multi_link_dynamic_key_prepare_ex_t* multi_link_dynamic_key_prepare_ex, uint8_t (*host_id)[5], uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);

    multi_link_dynamic_key_fetch_ex_t* multi_link_proto_common_encode_dynamic_key_fetch_ex(uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    bool multi_link_proto_common_decode_dynamic_key_fetch_ex(multi_link_dynamic_key_fetch_ex_t* multi_link_dynamic_key_fetch_ex, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);

    multi_link_dynamic_key_response_ex_t* multi_link_proto_common_encode_dynamic_key_response_ex(uint8_t cmd_data, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    bool multi_link_proto_common_decode_dynamic_key_response_ex(multi_link_dynamic_key_response_ex_t* multi_link_dynamic_key_response_ex, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    bool multi_link_proto_common_decode_dynamic_key_response_ex_v2(multi_link_dynamic_key_response_ex_t* multi_link_dynamic_key_response_ex, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);

    multi_link_data_ex_t* multi_link_proto_common_encode_data_ex(uint8_t * data, uint32_t data_length, multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    bool multi_link_proto_common_decode_data_ex(multi_link_data_ex_t* multi_link_data_ex, multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);

    bool multi_link_proto_common_check_array_is_empty(const uint8_t *array, uint32_t array_length);

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
    multi_link_host_address_request_ex_t *multi_link_proto_common_encode_host_address_request_ex_v2(uint8_t backoff_delay, uint8_t retry_interval, uint8_t device_type, uint8_t vendor_id, uint8_t (*device_id)[3]);
    bool multi_link_proto_common_decode_host_address_request_ex_v2(multi_link_host_address_request_ex_t* multi_link_host_address_request_ex);

    multi_link_host_address_fetch_request_init_ex_t *multi_link_proto_common_encode_host_address_fetch_request_init_ex_v2(uint8_t (*device_id)[3], uint8_t (*device_name)[10]);
    bool multi_link_proto_common_decode_host_address_fetch_request_init_ex_v2(multi_link_host_address_fetch_request_init_ex_t* multi_link_host_address_fetch_request_init);

    multi_link_host_address_pairing_challenge_ex_t* multi_link_proto_common_encode_host_address_pairing_challenge_v2(uint8_t (*device_id)[3]);
    uint32_t multi_link_proto_common_decode_host_address_pairing_challenge_v2(multi_link_host_address_pairing_challenge_ex_t* multi_link_host_address_pairing_challenge, uint8_t (*device_id)[3]);

    multi_link_host_address_pairing_response_ex_t* multi_link_proto_common_encode_host_address_pairing_response_v2(uint8_t (*device_id)[3], uint8_t (*salt_challenge)[5]);
    bool multi_link_proto_common_decode_host_address_pairing_response_v2(multi_link_host_address_pairing_response_ex_t *multi_link_host_address_pairing_response_ex, uint8_t (*device_id)[3]);

    multi_link_host_address_fetch_request_final_ex_t *multi_link_proto_common_encode_host_address_fetch_request_final_ex_v2(uint8_t (*device_id)[3], uint8_t (*device_name)[10]);
    bool multi_link_proto_common_decode_host_address_fetch_request_final_ex_v2(multi_link_host_address_fetch_request_final_ex_t * multi_link_host_address_fetch_request_final_ex);

    multi_link_host_address_response_ex_t *multi_link_proto_common_encode_host_address_response_ex_v2(uint8_t (*host_address)[4]);
    uint32_t multi_link_proto_common_decode_host_address_response_ex_v2(multi_link_host_address_response_ex_t *multi_link_host_address_response_ex);

    multi_link_host_id_request_ex_t *multi_link_proto_common_encode_host_id_request_ex_v2(uint8_t (*device_id)[3]);
    bool multi_link_proto_common_decode_host_id_request_ex_v2(multi_link_host_id_request_ex_t * multi_link_host_id_request_ex);

    multi_link_host_id_fetch_request_init_ex_t *multi_link_proto_common_encode_host_id_fetch_request_init_ex_v2(uint8_t (*device_id)[3], uint8_t (*device_name)[10]);
    bool multi_link_proto_common_decode_host_id_fetch_request_init_ex_v2(multi_link_host_id_fetch_request_init_ex_t* multi_link_host_id_fetch_request_init);

    multi_link_host_id_pairing_challenge_ex_t* multi_link_proto_common_encode_host_id_pairing_challenge_v2(uint8_t (*device_id)[3], uint8_t (*dongle_challenge)[5]);
    uint32_t multi_link_proto_common_decode_host_id_pairing_challenge_v2(multi_link_host_id_pairing_challenge_ex_t* multi_link_host_id_pairing_challenge, uint8_t (*device_id)[3]);

    multi_link_host_id_pairing_response_ex_t* multi_link_proto_common_encode_host_id_pairing_response_v2(uint8_t (*device_id)[3], uint8_t (*salt_challenge)[5], uint8_t (*device_response)[5]);
    bool multi_link_proto_common_decode_host_id_pairing_response_v2(multi_link_host_id_pairing_response_ex_t *multi_link_host_id_pairing_response_ex, uint8_t (*device_id)[3]);

    multi_link_host_id_fetch_request_final_ex_t *multi_link_proto_common_encode_host_id_fetch_request_final_ex_v2(uint8_t (*device_id)[3], uint8_t (*device_name)[10]);
    bool multi_link_proto_common_decode_host_id_fetch_request_final_ex_v2(multi_link_host_id_fetch_request_final_ex_t * multi_link_host_id_fetch_request_final_ex);

    multi_link_host_id_response_ex_t *multi_link_proto_common_encode_host_id_response_ex_v2(uint8_t (*host_id)[5]);
    uint32_t multi_link_proto_common_decode_host_id_response_ex_v2(multi_link_host_id_response_ex_t *multi_link_host_id_response_ex);

    multi_link_device_info_ex_t *multi_link_proto_common_encode_device_info_ex_v2(uint8_t (*device_id)[3], uint16_t firmware_version, uint16_t capability, uint16_t kb_layout);
    bool multi_link_proto_common_decode_device_info_ex_v2(multi_link_device_info_ex_t *multi_link_device_info_ex,uint8_t (*device_id)[3]);

    multi_link_device_info_fetch_response_ex_t *multi_link_proto_common_encode_device_info_fetch_response_ex_v2(uint8_t (*device_id)[3]);
    bool multi_link_proto_common_decode_device_info_fetch_response_ex_v2(multi_link_device_info_fetch_response_ex_t * multi_link_device_info_fetch_response_ex, uint8_t (*device_id)[3]);

    multi_link_device_info_response_ex_t *multi_link_proto_common_encode_device_info_response_ex_v2(uint8_t (*device_id)[3], uint8_t result);
    uint32_t multi_link_proto_common_decode_device_info_response_ex_v2(multi_link_device_info_response_ex_t *multi_link_device_info_response_ex, uint8_t (*device_id)[3]);

    bool multi_link_proto_common_init_dynamic_key_v2(uint8_t (*host_id)[5], uint8_t (*challenge)[5], uint8_t (*response)[5], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);

    multi_link_dynamic_key_prepare_ex_v2_t* multi_link_proto_common_encode_dynamic_key_prepare_ex_v2(uint8_t (*host_id)[5], uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    bool multi_link_proto_common_decode_dynamic_key_prepare_ex_v2(multi_link_dynamic_key_prepare_ex_v2_t* multi_link_dynamic_key_prepare_ex_v2, uint8_t (*host_id)[5], uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info_ex);
    
    multi_link_dynamic_key_fetch_ex_v2_t* multi_link_proto_common_encode_dynamic_key_fetch_ex_v2(uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    bool multi_link_proto_common_decode_dynamic_key_fetch_ex_v2(multi_link_dynamic_key_fetch_ex_v2_t* multi_link_dynamic_key_fetch_ex_v2, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);

    // multi_link_dynamic_key_response_ex_t* multi_link_proto_common_encode_dynamic_key_response_ex(uint8_t cmd_data, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    // bool multi_link_proto_common_decode_dynamic_key_response_ex(multi_link_dynamic_key_response_ex_t* multi_link_dynamic_key_response_ex, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    // bool multi_link_proto_common_decode_dynamic_key_response_ex_v2(multi_link_dynamic_key_response_ex_t* multi_link_dynamic_key_response_ex, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    
    multi_link_general_random_data_0_ext_t *multi_link_proto_common_general_random_data(uint8_t setting);
    multi_link_device_pairing_request_v2_ex_t* multi_link_common_encode_paired_request_ex(uint8_t (*device_id)[3], multi_link_common_callbacks_t *multi_link_common_callbacks);
    multi_link_dynamic_key_prepare_ex_v2_t* multi_link_proto_common_encode_dynamic_key_prepare_ex_v2(uint8_t (*host_id)[5], uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info);
    uint32_t multi_link_common_decode_confirm_request_ex(multi_link_device_confirm_request_ex_t* multi_link_device_confirm_request, uint8_t (*device_id)[3]);
    multi_link_publick_ex_t* multi_link_common_encode_public_key_ex(uint8_t (*device_id)[3], ecdh_key_t *ecdh_data, uint8_t publick_n);
    bool multi_link_common_decode_public_key_ex(multi_link_publick_ex_t* multi_link_publick, ecdh_key_t *ecdh_data, uint8_t publick_n);
    int32_t multi_link_proto_common_init_pairing_v2(ecdh_key_t *key_info);
    uint32_t multi_link_proto_common_dynamic_key_seed_reflash(uint8_t (*dev_nonce), uint8_t (*dgl_nonce), uint8_t nonce_length, uint32_t updatecount);
    void multi_link_proto_common_set_lfst_poly(uint8_t size);    
    bool Calculate_Kd0_Seed(uint8_t (*KeySeed)[32], uint8_t (*device_nonce)[5], uint8_t N);
    uint8_t get_g_taps_index(void);
    void set_g_taps_index(uint8_t value);
    bool get_dkseed_update_flag(void);
    void set_dkseed_update_flag(bool value);
    uint8_t *get_g_Kd0_Seedkey(uint8_t *length);
    uint8_t *get_dkeyseed_v2(uint8_t type, uint8_t *length);
    uint8_t *get_multi_link_common_debug_info(uint8_t type, uint8_t *length);


    

#endif

    extern uint8_t g_Kc[16];
    extern uint8_t g_Kc_HMAC_KEY[32];
    extern uint8_t g_Dk_HMAC_KEY[32];

    

#ifdef __cplusplus
}
#endif

#endif /* _MULTI_LINK_PROTO_H_ */
