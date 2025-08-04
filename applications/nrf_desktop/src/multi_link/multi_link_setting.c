#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
//#include <nrf_gzll.h>
#include <gzp.h>
//#include "gzp_internal.h"
#include "multi_link_setting.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(muiti_link_setting);

#define MULTI_LINK_DEVICE_DB_NAME   "multi_link_dev"
#define MULTI_LINK_DB_HOST_ADDR     "host_addr"
#define MULTI_LINK_DB_HOST_ID       "host_id"
#define MULTI_LINK_DB_CHALLENGE     "challenge"
#define MULTI_LINK_DB_RESPONSE      "response"
#define MULTI_LINK_DB_PAIRING       "is_pairing"

static uint8_t *multi_link_host_address;
static uint8_t *multi_link_host_id;
static uint8_t *multi_link_challenge;
static uint8_t *multi_link_response;
static uint8_t *multi_link_pairing;

#ifndef  CONFIG_GAZELL_PAIRING_SETTINGS

/* Settings key for System Address */
static const char db_key_sys_addr[] = MULTI_LINK_DEVICE_DB_NAME "/" MULTI_LINK_DB_HOST_ADDR;
/* Settings key for Host ID */
static const char db_key_host_id[] = MULTI_LINK_DEVICE_DB_NAME "/" MULTI_LINK_DB_HOST_ID;
/* Settings key for challenge */
static const char db_key_challenge[] = MULTI_LINK_DEVICE_DB_NAME "/" MULTI_LINK_DB_CHALLENGE;
/* Settings key for response */
static const char db_key_response[] = MULTI_LINK_DEVICE_DB_NAME "/" MULTI_LINK_DB_RESPONSE;

/* Settings for enter pairing*/
static const char db_key_pairing[] = MULTI_LINK_DEVICE_DB_NAME "/" MULTI_LINK_DB_PAIRING;

bool muiti_link_params_restore(void)
{
	int err;

	err = settings_load_subtree(MULTI_LINK_DEVICE_DB_NAME);
	if (err) {
		LOG_ERR("Cannot load settings");
	}

	return (!err) ? true : false;
}

static int settings_set(const char *key, size_t len_rd,
			settings_read_cb read_cb, void *cb_arg)
{
    LOG_DBG("%s start", __func__);
	if (!strcmp(key, MULTI_LINK_DB_HOST_ADDR)) {
		uint8_t tmp_addr[MULTI_LINK_HOST_ADDR_LENGTH];

		ssize_t len = read_cb(cb_arg, tmp_addr,
				      sizeof(tmp_addr));

		if (len == sizeof(tmp_addr)) {
			memcpy(multi_link_host_address, tmp_addr, len);
            LOG_HEXDUMP_DBG(multi_link_host_address,
                            MULTI_LINK_HOST_ADDR_LENGTH, 
                            "multi_link_host_address:");
		} else {
			LOG_WRN("Sys Addr data invalid length (%u)", len);

			return -EINVAL;
		}

	} else if (!strcmp(key, MULTI_LINK_DB_HOST_ID)) {
		uint8_t tmp_id[MULTI_LINK_HOST_ID_LENGTH];

		ssize_t len = read_cb(cb_arg, tmp_id,
				      sizeof(tmp_id));

		if (len == sizeof(tmp_id)) {
			memcpy(multi_link_host_id, tmp_id, len);
            LOG_HEXDUMP_DBG(multi_link_host_id,
                            MULTI_LINK_HOST_ID_LENGTH, 
                            "multi_link_host_id:");
		} else {
			LOG_WRN("Host ID data invalid length (%u)", len);

			return -EINVAL;
		}

    } else if (!strcmp(key, MULTI_LINK_DB_CHALLENGE)) {
		uint8_t tmp_challenge[MULTI_LINK_CHALLENGE_LENGTH];

		ssize_t len = read_cb(cb_arg, tmp_challenge,
				      sizeof(tmp_challenge));

		if (len == sizeof(tmp_challenge)) {
			memcpy(multi_link_challenge, tmp_challenge, len);
            LOG_HEXDUMP_DBG(multi_link_challenge,
                            MULTI_LINK_CHALLENGE_LENGTH, 
                            "multi_link_challenge:");
		} else {
			LOG_WRN("Challenge code invalid length (%u)", len);

			return -EINVAL;
		}

    } else if (!strcmp(key, MULTI_LINK_DB_RESPONSE)) {
		uint8_t tmp_response[MULTI_LINK_RESPONSE_LENGTH];

		ssize_t len = read_cb(cb_arg, tmp_response,
				      sizeof(tmp_response));

		if (len == sizeof(tmp_response)) {
			memcpy(multi_link_response, tmp_response, len);
            LOG_HEXDUMP_DBG(multi_link_response,
                            MULTI_LINK_RESPONSE_LENGTH, 
                            "multi_link_response:");
		} else {
			LOG_WRN("Response invalid length (%u)", len);

			return -EINVAL;
		}
	}else if (!strcmp(key, MULTI_LINK_DB_PAIRING)) {
		uint8_t tmp_pairing[MULTI_LINK_PAIRING_LENGTH];

		ssize_t len = read_cb(cb_arg, tmp_pairing,
				      sizeof(tmp_pairing));

		if (len == sizeof(tmp_pairing)) {
			memcpy(multi_link_pairing, tmp_pairing, len);
            LOG_HEXDUMP_DBG(multi_link_pairing,
                            MULTI_LINK_PAIRING_LENGTH, 
                            "multi_link_pairing:");
		} else {
			LOG_WRN("pairing invalid length (%u)", len);

			return -EINVAL;
		}
	}

	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(multi_link_setting, MULTI_LINK_DEVICE_DB_NAME, NULL, settings_set, NULL,
			       NULL);

#endif


void multi_link_get_pairing_setting(uint8_t* host_addr,
                                    uint8_t* host_id,
                                    uint8_t* challenge,
                                    uint8_t* response,
									uint8_t* pairing)
{
    LOG_INF("%s", __func__);

    multi_link_host_address = host_addr;
    multi_link_host_id = host_id;
    multi_link_challenge = challenge;
    multi_link_response = response;
	multi_link_pairing = pairing;
}

bool multi_link_params_store(void)
{
	int err;
/*	err = settings_delete(db_key_host_id);
	if (err) {
		LOG_WRN("Cannot delete host id, err %d", err);
	}
*/
	err = settings_save_one(db_key_sys_addr,
				multi_link_host_address,
				MULTI_LINK_HOST_ADDR_LENGTH);
	if (err) {
		LOG_ERR("Cannot store system address, err %d", err);
        return (!err) ? true : false;
	} else {
		err = settings_save_one(db_key_host_id,
                                multi_link_host_id,
                                MULTI_LINK_HOST_ID_LENGTH);
	}
    if (err) {
        LOG_ERR("Cannot store host id, err %d", err);
        return (!err) ? true : false;
    } else {
        err = settings_save_one(db_key_challenge,
                                multi_link_challenge,
                                MULTI_LINK_CHALLENGE_LENGTH);
    }
    if (err) {
        LOG_ERR("Cannot store challenge, err %d", err);
        return (!err) ? true : false;
    } else { 
        err = settings_save_one(db_key_response,
                                multi_link_response,
                                MULTI_LINK_RESPONSE_LENGTH);
    }

    if (err) {
        LOG_ERR("Cannot store response, err %d", err);
    }
    LOG_ERR("%s OK!!!!", __func__);
	return (!err) ? true : false;
}

bool multi_link_pairing_store(void)
{
	int err = settings_save_one(db_key_pairing,
				multi_link_pairing,
				MULTI_LINK_PAIRING_LENGTH);
	if (err) {
		LOG_ERR("Cannot store pairing status, err %d", err);
	}
	return (!err) ? true : false; 
}
