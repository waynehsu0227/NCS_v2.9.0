#include "multi_link.h"
#include "multi_link_load_key.h"

#ifdef __ZEPHYR__

#define MODULE multi_link_load_key
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);
#else
#define LOG_DBG(...)
#define LOG_ERR(...)
#define LOG_WRN(...)
#define LOG_HEXDUMP_DBG(...)
#define LOG_HEXDUMP_INF(...)
#endif

bool multi_link_load_key(uint8_t key_type, uint8_t* key, size_t key_length)
{
	//
	// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
	// All this key must flashed in flash from factory and read from flash. In production device FW should not hard coded in code.
	// This keys should not part of any public FW updater also
	// Here is hard coded for demo code
	//   
	if(key_type == MULTI_LINK_STATIC_KEY_TYPE_KC)
	{
		uint8_t kc[16] = {0x2b, 0x55, 0x10, 0x09, 0x83, 0xae, 0x4e, 0x0c, 0x98, 0xaa, 0xe9, 0xd2, 0x6e, 0xfc, 0xae, 0x18};
		if(key_length == sizeof(kc))
		{
			memcpy(key, kc, key_length);
			return true;
		}
	}
	else if(key_type == MULTI_LINK_STATIC_KEY_TYPE_KC_HMAC)
	{
		uint8_t kc_HMAC_KEY[32] = {0x8a, 0x22, 0xbe, 0x19, 0x80, 0x8e, 0x41, 0xb8, 0x82, 0xfb, 0xe9, 0xda, 0x29, 0x82, 0x86, 0x1, 0x8e, 0x25, 0xb6, 0x54, 0xb9, 0xb1, 0x4c, 0xa5, 0xb7, 0xea, 0xec, 0xe9, 0xd3, 0xd8, 0x2f, 0xaa};
		if(key_length == sizeof(kc_HMAC_KEY))
		{
			memcpy(key, kc_HMAC_KEY, key_length);
			return true;
		}
	}
	else if(key_type == MULTI_LINK_STATIC_KEY_TYPE_DK_HMAC)
	{
		uint8_t dk_HMAC_KEY[32] = {0xfb, 0xeb, 0x23, 0x17, 0x5d, 0xbc, 0x49, 0x66, 0xa6, 0xe6, 0x9e, 0x65, 0x20, 0x44, 0xb9, 0xcc, 0x46, 0x41, 0x35, 0x94, 0x87, 0xfe, 0x4e, 0x2b, 0x9d, 0x9, 0xb2, 0x99, 0x5, 0x3f, 0x15, 0x23};
		if(key_length == sizeof(dk_HMAC_KEY) )
		{
			memcpy(key, dk_HMAC_KEY, key_length);
			return true;
		}
	}

	return false;
}