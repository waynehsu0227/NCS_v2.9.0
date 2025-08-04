#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <caf/events/power_manager_event.h>

#include "multi_link.h"
#include "multi_link_crypto.h"
#include "multi_link_basic.h"
#include "multi_link_proto_common.h"
#include "../modules/dpm_config.h"
#include <zephyr/storage/flash_map.h>

#define MODULE multi_link_ota
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);


#define GEN3_CODE		   			0
#define SKIP_CHALLENGE_CODE_CHECK	1
#define CHECK_SUM_FROM_FLASH		1

#define OTA_DONGLE_REPLY_ERROR_CNT 2000

#define FLASH_ERASE_TIMEOUT		10			//ms
#define IMMEDIATELY_TIMEOUT		1			//ms



#if CONFIG_SECURE_BOOT
	BUILD_ASSERT(IS_ENABLED(CONFIG_PARTITION_MANAGER_ENABLED),
		     "B0 bootloader supported only with Partition Manager");
	#include <pm_config.h>
	#include <fw_info.h>
	#define BOOTLOADER_NAME	"B0"
	#if PM_ADDRESS == PM_S0_IMAGE_ADDRESS
		#define DFU_SLOT_ID		PM_S1_IMAGE_ID
	#elif PM_ADDRESS == PM_S1_IMAGE_ADDRESS
		#define DFU_SLOT_ID		PM_S0_IMAGE_ID
	#else
		#error Missing partition definitions.
	#endif
#elif CONFIG_BOOTLOADER_MCUBOOT
	BUILD_ASSERT(IS_ENABLED(CONFIG_PARTITION_MANAGER_ENABLED),
		     "MCUBoot bootloader supported only with Partition Manager");
	#include <pm_config.h>
	#include <zephyr/dfu/mcuboot.h>
	#if CONFIG_DESKTOP_CONFIG_CHANNEL_DFU_MCUBOOT_DIRECT_XIP
		#define BOOTLOADER_NAME	"MCUBOOT+XIP"
	#else
		#define BOOTLOADER_NAME	"MCUBOOT"
	#endif

	#ifdef PM_MCUBOOT_SECONDARY_PAD_SIZE
		BUILD_ASSERT(PM_MCUBOOT_PAD_SIZE == PM_MCUBOOT_SECONDARY_PAD_SIZE);
	#endif

	#if CONFIG_BUILD_WITH_TFM
		#define PM_ADDRESS_OFFSET (PM_MCUBOOT_PAD_SIZE + PM_TFM_SIZE)
	#else
		#define PM_ADDRESS_OFFSET (PM_MCUBOOT_PAD_SIZE)
	#endif

	#if (PM_ADDRESS - PM_ADDRESS_OFFSET) == PM_MCUBOOT_PRIMARY_ADDRESS
		#define DFU_SLOT_ID		PM_MCUBOOT_SECONDARY_ID
	#elif (PM_ADDRESS - PM_ADDRESS_OFFSET) == PM_MCUBOOT_SECONDARY_ADDRESS
		#define DFU_SLOT_ID		PM_MCUBOOT_PRIMARY_ID
	#else
		#error Missing partition definitions.
	#endif
#else
	#error Bootloader not supported.
#endif
static const struct flash_area *flash_area;

#define FLASH_PAGE_SIZE_LOG2	12
#define FLASH_PAGE_SIZE			BIT(FLASH_PAGE_SIZE_LOG2)


// ===== Chanllenge Code =====
#define AES_128_KEY_LEN	 	16
#define AES_192_KEY_LEN	 	24
#define AES_256_KEY_LEN		32
#define AES_KEY_LEN			AES_256_KEY_LEN



#define CHALLENGE_MK_LENGTH 16
#define CHALLENGE_CX_LENGTH 16

static volatile bool tx_complete; ///< Flag to indicate whether a GZLL TX attempt has completed.
static bool tx_success;		  ///< Flag to indicate whether a GZLL TX attempt was successful.
static uint8_t OTA_magic_word_check_status = 1;

typedef enum {
	WORK_NORMAL,
	WORK_BINDING,
	WORK_SLOW_WHITELIST,
	WORK_WAIT_DISCONNECT,
	WORK_OTA,
	WORK_OTA_FINISH,
	WORK_PWR_ON,
	WORK_BATT_LOW,
	WORK_CUT_OFF,
	WORK_FCC_TEST,
	WORK_UNKNOW
} working_mode_t;
uint8_t m_work_mode = WORK_UNKNOW;

static uint8_t OTA_step = 0;

typedef struct {
	uint8_t OTA_tx_data[20];
	uint8_t OTA_tx_len;
	uint8_t OTA_tx_pipe;
	uint8_t OTA_rx_data[20];
	uint32_t OTA_rx_len;
	uint8_t OTA_rx_pipe;
} OTA_packet_t;

typedef enum {
	OTA_UPATE_NO_ACTION = 0,
	OTA_UPATE_SUCCESS,
	OTA_UPATE_FIALED,
	OTA_UPATE_TIMEOUT,
	OTA_UPATE_IN_PROCESS,
	OTA_UPATE_STAY_IN_APP1

} OTA_update_status_t;

typedef union {
	uint8_t raw[CHALLENGE_CX_LENGTH];
	struct {
		uint8_t Challenge_Code[4];
		uint8_t Salt[2];
		uint8_t Iteration[2];
		uint8_t Reserved[8];
	} parameter;
} CHALLENGE_INFO_STRUCT;
static CHALLENGE_INFO_STRUCT Dev_Challenge;

typedef union {
	uint8_t OTA_data[20];
	struct {
		uint16_t deivce_pid;   // 2
		uint16_t FW_ver0;      // 2
		uint16_t FW_ver1;      // 2
		uint16_t Reserve;      // 2 for alignment
		uint32_t FW_checksum;  // 4
		uint32_t FW_code_size; // 4
		uint8_t FW_encryption; // 1
		uint8_t FW_type;       // 1
		uint8_t FW_inf[2];     // 2
	} parameter;
} OTA_file_inf_t;
static OTA_file_inf_t OTA_file_inf;

static OTA_update_status_t OTA_status = OTA_UPATE_SUCCESS;

struct k_work_delayable ota_work;

static uint8_t Dev_Cx[CHALLENGE_CX_LENGTH];

static const uint8_t Dev_Dx[CHALLENGE_MK_LENGTH * 2] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
						  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
						  0xf0, 0xe0, 0xd0, 0xc0, 0xb0, 0xa0, 0x90, 0x80,
						  0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10, 0x00};

static uint8_t App_Crx[CHALLENGE_CX_LENGTH];
static uint8_t Dev_Crx[CHALLENGE_CX_LENGTH];
static uint8_t ota_aes_key[16] = {0};

//------------------------------
static uint8_t OTA_FW_enc[48]; // 24*2 = 16*3
static uint8_t OTA_FW_buf[48]; // 24*2 = 16*3
static uint8_t OTA_FW_buf_cnt = 0;
//------------------------------
static uint8_t OTA_FW_hex_packet_ID = 0;
static uint8_t OTA_FW_hex_packet_seq = 0;
static uint32_t OTA_FW_recevied_checksum = 0;
static uint32_t fw_hex_code_received_byte = 0;
static uint8_t challenge_code_status = 0;
static uint32_t erase_offset = 0;
static uint16_t OTA_resend_count = 0;
typedef union {
	uint8_t OTA_data[24];
	struct {
		uint32_t OTA_FW_code[6];
	} parameter;
} OTA_FW_hex_code_t;
static OTA_FW_hex_code_t OTA_FW_hex_code;


/* OTA action */
#define TYPE_BYPASS  0x00
#define TYPE_BLE     0xA1
#define TYPE_SD	     0xA2
#define TYPE_SD_BLE  0xA3
#define TYPE_2G4     0xA4
#define TYPE_BLE_2G4 0xA5
static uint8_t OTA_type;

static void ota_work_fn(struct k_work *w);

void gzp_device_tx_success(void)
{
	tx_complete = true;
	tx_success = true;
}

void gzp_device_tx_failed(void)
{
	tx_complete = true;
	tx_success = false;
}

static void gzp_get_device_id(uint8_t *id)
{
	memcpy(id, multi_link_device_get_device_id(), MULTI_LINK_DEV_ID_WIDTH); // byte4 ~ byte6 device ID
}

static bool is_recive_dev_id_correct(uint8_t *id)
{
	return !memcmp(id, multi_link_device_get_device_id(), MULTI_LINK_DEV_ID_WIDTH);
}

bool gzp_tx_packet(const uint8_t *tx_packet, uint8_t length, uint8_t pipe)
{
	tx_complete = false;
	tx_success = false;
	power_manager_restrict((size_t)NULL, POWER_MANAGER_LEVEL_ALIVE);

	if (nrf_gzll_add_packet_to_tx_fifo(pipe, (uint8_t *)tx_packet, length)) {
		while (tx_complete == false) {
			__WFI();
		}
		LOG_HEXDUMP_DBG(tx_packet, length, "tx_packet:");
		return tx_success;
	} else {
		return false;
	}
}

/** @} */

bool gzp_tx_rx(const uint8_t *tx_packet, uint8_t tx_length, uint8_t *rx_dst, uint32_t *rx_length,
	       uint8_t pipe)
{
	LOG_DBG("%s", __func__);
	nrf_gzll_flush_rx_fifo(pipe);
	nrf_gzll_flush_tx_fifo(pipe);
	*rx_length = 0;
	if (gzp_tx_packet(tx_packet, tx_length, pipe)) {
		if (nrf_gzll_get_rx_fifo_packet_count(pipe) > 0) {
			*rx_length = 32;
			nrf_gzll_fetch_packet_from_rx_fifo(pipe, rx_dst, rx_length);
			LOG_HEXDUMP_DBG(rx_dst, *rx_length, "fetch_packet:");
		}
		// nrf_gzll_flush_tx_fifo(pipe);
		return true;
	}
	nrf_gzll_flush_tx_fifo(pipe);
	return false;
}

static void OTA_work_restart(uint16_t ms)
{
	LOG_DBG("%s=%d", __func__, ms);
	k_work_reschedule(&ota_work, K_MSEC(ms));
}

static void OTA_work_stop(void)
{
	LOG_DBG("%s", __func__);
	k_work_cancel_delayable(&ota_work);
}

static void ota_aes_key_generation(uint8_t *seed, uint8_t seed_len, uint8_t *aes_key)
{
#if 0
    SHA256_CTX *sha = &g_hmac.sha;

    sha256_init(sha);
    sha256_update(sha, seed, seed_len);
    sha256_final(sha, g_hmac.buf);
    memcpy(aes_key, &g_hmac.buf[8], 16);
#else
	uint8_t hash[64] = {0};

	if (multi_link_sha256(seed, seed_len, hash, sizeof(hash))) {
		memcpy(aes_key, &hash[8], 16);
		LOG_HEXDUMP_DBG(hash, sizeof(hash), "hash:");
		LOG_HEXDUMP_DBG(aes_key, 16, "aes_key:");
	}
#endif
}

static void cifra_cbc_aes_256_encrypt(const uint8_t (*key)[32], uint8_t *input, uint8_t *output)
{
	#define BLOCK_SIZE 16
	uint8_t iv[BLOCK_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	multi_link_cbc_aes_256_encrypt(key, input, 16, iv, sizeof(iv), output, 16);
}

static void Generate_Challenge_Code(void)
{
	// const cf_chash *h = &cf_sha256;
	uint16_t ProductID = CONFIG_DESKTOP_DEVICE_PID; // 0x2122;
	uint8_t *DeviceID = (uint8_t*)multi_link_device_get_device_id();

	uint8_t Dev_MK[CHALLENGE_MK_LENGTH];
	uint8_t Dev_Kd_i0[AES_KEY_LEN];

	// paired device ID
	// memcpy(DeviceID, &bond_24g.data[1] , 3);

	// Dev_MK = Dev_Dx1 ^ Dev_Dx2;
	multi_link_proto_common_xor(Dev_MK, &Dev_Dx[0], &Dev_Dx[16], CHALLENGE_MK_LENGTH);
	multi_link_pbkdf2_hmac_sha256(Dev_MK, CHALLENGE_MK_LENGTH, (uint8_t *)&ProductID,
				      sizeof(ProductID), Dev_Kd_i0, AES_KEY_LEN,
				      DeviceID[2] ? DeviceID[2] : 1);

	multi_link_generate_random(Dev_Challenge.raw, CHALLENGE_CX_LENGTH);
	cifra_cbc_aes_256_encrypt((const uint8_t(*)[sizeof(Dev_Kd_i0)])Dev_Kd_i0, Dev_Challenge.raw,
				  Dev_Cx);
}

static void Check_Challenge_Code(void)
{
	LOG_DBG("%s", __func__);
	// const cf_chash *h = &cf_sha256;

	uint8_t Dev_MK[CHALLENGE_MK_LENGTH];
	uint8_t Dev_Kd_i1[AES_KEY_LEN];

	// Dev_MK = Dev_Dx1 ^ Dev_Dx2;
	multi_link_proto_common_xor(Dev_MK, &Dev_Dx[0], &Dev_Dx[16], CHALLENGE_MK_LENGTH);

	multi_link_pbkdf2_hmac_sha256(Dev_MK, CHALLENGE_MK_LENGTH,
				      (uint8_t *)Dev_Challenge.parameter.Salt,
				      sizeof(Dev_Challenge.parameter.Salt), Dev_Kd_i1, AES_KEY_LEN,
				      Dev_Challenge.parameter.Iteration[1]);
	cifra_cbc_aes_256_encrypt((const uint8_t(*)[sizeof(Dev_Kd_i1)])Dev_Kd_i1, Dev_Challenge.raw,
				  Dev_Crx);
}

static bool ecb_128_decrpt_with_aes128_key(const uint8_t (*key)[16], const uint8_t *data,
				    const size_t data_length, uint8_t *result, size_t result_length)
{
	LOG_DBG("%s", __func__);
	multi_link_ecb_aes_128_decrypt(key, data, data_length, result, result_length);

	return true;
}

void OTA_start(void)
{
	if (m_work_mode != WORK_OTA) {
		m_work_mode = WORK_OTA;
		OTA_step = 0;
#if GEN3_CODE
		buffer_clear();
		keyboard_matrix_disable();
#endif
		OTA_work_restart(100);
	}
}

static bool OTA_send_current_FW_version(void)
{
	OTA_packet_t OTA_packet;
	OTA_packet.OTA_tx_data[0] = 0x28;

	OTA_packet.OTA_tx_data[1] = OTA_status; // Informat Time Out

	OTA_packet.OTA_tx_data[2] = 0x03; // FW_type

	OTA_packet.OTA_tx_data[3] = DPM_FW_VER_LOW_BYTE;  // LSB FW version
	OTA_packet.OTA_tx_data[4] = DPM_FW_VER_HIGH_BYTE; // MSB FW version
	OTA_packet.OTA_tx_data[5] = CAPABILITY_FLAG; //
	OTA_packet.OTA_tx_data[6] = 0;		     //
	gzp_get_device_id(&OTA_packet.OTA_tx_data[7]);
	OTA_packet.OTA_tx_len = 10;
	OTA_packet.OTA_rx_pipe = 3;
	OTA_resend_count = 0;
	return gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			 &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe);
}


static void checksum_from_recive(uint32_t addr, uint8_t *buf, size_t size)
{
	for (int i = 0; i < size; i++) {
		OTA_FW_recevied_checksum += buf[i];
	}
}

static uint32_t checksum_from_flash(void)
{
	uint8_t tmp;
	uint32_t csum = 0;
	for (uint32_t i = 0; i < fw_hex_code_received_byte; i ++) {
		flash_area_read(flash_area, i, &tmp, 1);
		csum += tmp;
	}
	return csum;
}

static void ota_work_fn(struct k_work *w)
{
	OTA_packet_t OTA_packet;
	uint32_t err_code = 0 /*, scr*/;
#if 0
	uint8_t i;
#endif

	if (m_work_mode != WORK_OTA) {
		OTA_step = 20;
	}
#if GEN3_CODE
	wdt_feed();
#endif
	//	OTA_status = OTA_UPATE_FIALED;   //First assume that OTA TIMEOUT
	LOG_PRINTK("\n\n");
	LOG_DBG("%s step=%d", __func__, OTA_step);
	switch (OTA_step) {
	case 0: // Generate Challenge code
#if GEN3_CODE
		pair_led_on(LED_OTA_BLINK, 500, NULL);
#endif
		Generate_Challenge_Code();
		OTA_step++;
		OTA_status = OTA_UPATE_FIALED; // First assume that OTA TIMEOUT

	case 1: // Send Challenge Code
	case 2:
		OTA_packet.OTA_tx_data[0] = 0x26;
		OTA_packet.OTA_tx_data[1] = OTA_step - 1;
		OTA_magic_word_check_status = 1;
		if (OTA_step == 1) { // TODO: send Dev_Cx #1 to Dongle
			memcpy(&OTA_packet.OTA_tx_data[2], &Dev_Cx[0], 8);
		} else { // TODO: send Dev_Cx #2 to Dongle
			memcpy(&OTA_packet.OTA_tx_data[2], &Dev_Cx[8], 8);
		}
		gzp_get_device_id(&OTA_packet.OTA_tx_data[10]);
		OTA_packet.OTA_tx_len = 13; // 13 Byte
		OTA_packet.OTA_rx_pipe = 3; // Sent by pipe 3
		OTA_work_stop();

		if (gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			      &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe)) {
			OTA_step++; // go to next step;
			OTA_resend_count = 0;
			OTA_work_restart(20);
		} else {
			OTA_resend_count++;
			OTA_work_restart(20);
			if (OTA_resend_count > 100) {
				OTA_step = 20; // exit OTA mode
				OTA_status = OTA_UPATE_TIMEOUT;
			}
		}
		break;

	case 3: // check tx status
		//             test = 3;
		OTA_packet.OTA_tx_data[0] = 0x26;
		OTA_packet.OTA_tx_data[1] = 0x02;
		gzp_get_device_id(&OTA_packet.OTA_tx_data[2]);
		OTA_packet.OTA_tx_len = 5;  // 5 Byte
		OTA_packet.OTA_rx_pipe = 3; // Sent by pipe 3
		OTA_work_stop();

		if (gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			      &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe)) {
			if (OTA_packet.OTA_rx_len == 5 && OTA_packet.OTA_rx_data[0] == 0xA7 &&
			    OTA_packet.OTA_rx_data[1] == 0x00 &&// sucess
				is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[2])) {
				OTA_step++; // go to next step;
				OTA_resend_count = 0;
				challenge_code_status = 0;
				OTA_work_restart(20);
			} else if (OTA_packet.OTA_rx_len == 4 &&
				   OTA_packet.OTA_rx_data[0] == 0xA5 &&
				   is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[1])) {
				OTA_work_restart(20);
				OTA_step = 20;

			} else {
				//                    err_code =
				//                    app_timer_start(m_fw_update_id,
				//                    APP_TIMER_TICKS(10 ), 0);
				//                    APP_ERROR_CHECK(err_code);
				//                    OTA_step = 3;
				//                    OTA_resend_count++;
				//                    if( OTA_resend_count > 40)
				//                       OTA_step = 20;
				OTA_step++; // go to next step;
				OTA_resend_count = 0;
				challenge_code_status = 0;
				OTA_work_restart(10);
			}
		} else {
			OTA_resend_count++;
			if (OTA_resend_count > 100) {
				OTA_step = 20; // exit OTA mode
				OTA_status = OTA_UPATE_TIMEOUT;
			}
			OTA_work_restart(20);
		}

		break;

	case 4: // Fetch Descripted Magic Word
	case 5:
		OTA_packet.OTA_tx_data[0] = 0x27;
		OTA_packet.OTA_tx_data[1] = challenge_code_status;

		gzp_get_device_id(&OTA_packet.OTA_tx_data[2]);
		OTA_packet.OTA_tx_len = 5;
		OTA_packet.OTA_rx_pipe = 3;
		OTA_work_stop();

		if (gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			      &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe)) {
			if (OTA_packet.OTA_rx_len && OTA_packet.OTA_rx_data[1] == challenge_code_status &&
			    is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[10])) {
				if (challenge_code_status == 0x00 || challenge_code_status == 0x01) {
					if (challenge_code_status == 0x00) { // TODO: Receive App_Crx #1
						// memcpy(&OTA_encrypted_magic_word[0],
						// &OTA_packet.OTA_rx_data[2],
						// sizeof(OTA_encrypted_magic_word)/2);
						memcpy(&App_Crx[0], &OTA_packet.OTA_rx_data[2],
						       sizeof(App_Crx) / 2);
					} else { // TODO: Receive App_Crx #2
						// memcpy(&OTA_encrypted_magic_word[8],
						// &OTA_packet.OTA_rx_data[2],
						// sizeof(OTA_encrypted_magic_word)/2);
						memcpy(&App_Crx[8], &OTA_packet.OTA_rx_data[2],
						       sizeof(App_Crx) / 2);
					}
					challenge_code_status++;
					OTA_resend_count = 0;
				}
				OTA_work_restart(10);
			} else if (OTA_packet.OTA_rx_len == 4 &&
				   OTA_packet.OTA_rx_data[0] == 0xA5 &&
				   is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[1])) {
				OTA_step = 20;
				OTA_work_restart(20);

			} else {
				if (challenge_code_status == 0x02) {
					OTA_resend_count = 0;
					OTA_step++;
					// TODO: check if Dev_Crx == App_Crx
					Check_Challenge_Code();
					LOG_HEXDUMP_DBG(Dev_Crx, sizeof(Dev_Crx), "Dev_Crx");
					LOG_HEXDUMP_DBG(App_Crx, sizeof(App_Crx), "App_Crx");
#if SKIP_CHALLENGE_CODE_CHECK
					LOG_DBG("skip challenge code check set!!");
					LOG_DBG("challenge code check always set pass !!");
					OTA_magic_word_check_status = 0;
#else
					OTA_magic_word_check_status =
						memcmp(Dev_Crx, App_Crx, sizeof(Dev_Crx));
#endif
					// OTA_magic_word_check_status = 0; //Chanllenge
					// Code is Correct, and do the next step
					//  Do chanllenge code checking
					//  If chanllenge code is correct,
					//  OTA_magic_word_check_status = 0; If chanllenge
					//  code is wrong, OTA_magic_word_check_status = 1;
					//  err_code = app_timer_start(m_fw_update_id,
					//  APP_TIMER_TICKS(10 ), 0);
					ota_aes_key_generation(
						Dev_Crx, sizeof(Dev_Crx),
						ota_aes_key); // for OTA bin descrption;
					OTA_work_restart(10);
				} else {
					OTA_resend_count++;
					if (OTA_resend_count > 100) {
						OTA_step = 20; // exit again
						OTA_status = OTA_UPATE_TIMEOUT;
					}
					OTA_work_restart(10);
				}
			}
		} else {
			OTA_resend_count++;
			if (OTA_resend_count > 100) {
				OTA_step = 20; // exit again
				OTA_status = OTA_UPATE_TIMEOUT;
			} else {
				OTA_step = 4;
			}

			OTA_work_restart(10);
		}
		break;

	case 6:
		//               test = 6;
		OTA_packet.OTA_tx_data[0] = 0x20;
#if 0
            if( drv_battery_get() <= 20)
            {
                OTA_packet.OTA_tx_data[1] = 0x03; //Battery Low
                OTA_magic_word_check_status = 0x03;
            }	
            else
#endif
		{
			OTA_packet.OTA_tx_data[1] = OTA_magic_word_check_status;
		}
		gzp_get_device_id(&OTA_packet.OTA_tx_data[2]);
		OTA_packet.OTA_tx_len = 5;
		OTA_packet.OTA_rx_pipe = 3;

		OTA_work_stop();
		if (gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			      &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe)) {
			if (OTA_magic_word_check_status == 0) {
				OTA_resend_count = 0;
				OTA_step++;
				OTA_work_restart(10);
			} else {
				OTA_step = 20; // Chanllenge Code Failure
				OTA_work_restart(10);
			}
		} else {
			OTA_resend_count++;
			if (OTA_resend_count > 40) {
				OTA_step = 20; // resend again
				OTA_status = OTA_UPATE_TIMEOUT;
			} else {
				OTA_step = 1; // resend again
			}
			OTA_work_restart(10);
		}
		break;

	case 7: // FW information transfer: Packet 1
		OTA_packet.OTA_tx_data[0] = 0x21;
		OTA_packet.OTA_tx_data[1] = 0x01;
		gzp_get_device_id(&OTA_packet.OTA_tx_data[2]);
		OTA_packet.OTA_tx_len = 5;
		OTA_packet.OTA_rx_pipe = 3;
		OTA_work_stop();
		if (gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			      &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe)) {
			if (OTA_packet.OTA_rx_len == 15 && OTA_packet.OTA_rx_data[0] == 0xA1 &&
			    OTA_packet.OTA_rx_data[1] == 0x01 &&
			    is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[12])) {
				OTA_step++;
				OTA_resend_count = 0;
				memcpy(&OTA_file_inf.OTA_data[0], &OTA_packet.OTA_rx_data[2], 10);
			} else if (OTA_packet.OTA_rx_len == 4 &&
				   OTA_packet.OTA_rx_data[0] == 0xA5 &&
				   is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[1]))

			{
				OTA_step = 20;

			} else {
				OTA_resend_count++;
				if (OTA_resend_count > OTA_DONGLE_REPLY_ERROR_CNT) {
					OTA_step = 20;
					OTA_status = OTA_UPATE_TIMEOUT;
				}
			}
			OTA_work_restart(10);
		}
#if 1
		else {
			OTA_resend_count++;
			if (OTA_resend_count > 40) {
				OTA_step = 20; // resend again
				OTA_status = OTA_UPATE_TIMEOUT;
			}
			OTA_work_restart(10);
		}
#endif
		break;

	case 8: // FW information transfer: Packet 2
		OTA_packet.OTA_tx_data[0] = 0x21;
		OTA_packet.OTA_tx_data[1] = 0x02;
		gzp_get_device_id(&OTA_packet.OTA_tx_data[2]);
		OTA_packet.OTA_tx_len = 5;
		OTA_packet.OTA_rx_pipe = 3;
		OTA_work_stop();
		if (gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			      &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe)) {
			if (OTA_packet.OTA_rx_len == 15 && OTA_packet.OTA_rx_data[0] == 0xA1 &&
			    OTA_packet.OTA_rx_data[1] == 0x02 &&
			    is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[12])) {
				OTA_step++;
				OTA_resend_count = 0;
				memcpy(&OTA_file_inf.OTA_data[10], &OTA_packet.OTA_rx_data[2], 10);
				// note FW type
				switch (OTA_file_inf.parameter.FW_type) {
				case 5:
					OTA_type = TYPE_BLE;
					break;
				case 2:
					OTA_type = TYPE_SD;
					break;
				case 7:
					OTA_type = TYPE_SD_BLE;
					break;
				case 6:
				case 3:
					OTA_type = TYPE_BLE_2G4;
					break;
				default: // not support 1: bootloader, 4: SD_BLE_2G4
					OTA_type = TYPE_BYPASS;
					break;
				}
				//------------------------------
			} else if (OTA_packet.OTA_rx_len == 4 &&
				   OTA_packet.OTA_rx_data[0] == 0xA5 &&
				  is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[1])) {
				OTA_step = 20;
			} else {
				OTA_resend_count++;
				if (OTA_resend_count > OTA_DONGLE_REPLY_ERROR_CNT) {
					OTA_step = 20;
					OTA_status = OTA_UPATE_TIMEOUT;
				}
			}
			OTA_work_restart(10);
		}
#if 1
		else {
			OTA_resend_count++;
			if (OTA_resend_count > 40) {
				OTA_step = 20; // resend again
				OTA_status = OTA_UPATE_TIMEOUT;
			}
			OTA_work_restart(10);
		}
#endif
		break;

	case 9:
		OTA_packet.OTA_tx_data[0] = 0x22;
		OTA_packet.OTA_tx_data[1] = 0x00; // Status OK
		OTA_packet.OTA_tx_data[2] = 50;	  // Waiting Time
		gzp_get_device_id(&OTA_packet.OTA_tx_data[3]);
		OTA_packet.OTA_tx_len = 6;
		OTA_packet.OTA_rx_pipe = 3;
		OTA_work_stop();

		LOG_DBG("PID=0x%04X, Ver0=0x%04X, Ver1=0x%04X",
		OTA_file_inf.parameter.deivce_pid,
		OTA_file_inf.parameter.FW_ver0,
		OTA_file_inf.parameter.FW_ver1);
		LOG_DBG("Checksum=0x%08X, code size=%d",
		OTA_file_inf.parameter.FW_checksum,
		OTA_file_inf.parameter.FW_code_size);

		if (gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			      &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe)) {
			// update BlActionInfo
			//------------------------------
			//------------------------------
			OTA_FW_hex_packet_seq = 0x00;
			OTA_FW_hex_packet_ID = 0xff;
			OTA_FW_recevied_checksum = 0;
			fw_hex_code_received_byte = 0;
			OTA_step++;
			//                led_ota_trigger(CUSTOM_SS_OTA_PROCESS);  Wayne Hsu
			erase_offset = 0;
			__ASSERT_NO_MSG(flash_area == NULL);
			int err = flash_area_open(DFU_SLOT_ID, &flash_area);
			if (err) {
				LOG_ERR("Cannot open flash area (%d)", err);
				OTA_step = 20;
			} else {
				LOG_WRN("flash open ID%d ok!!! offset:0x%08X,size:0x%08X", 
						DFU_SLOT_ID,
						(uint32_t)flash_area->fa_off,
						(uint32_t)flash_area->fa_size);
			}

			if (flash_area->fa_size < OTA_file_inf.parameter.FW_code_size) {
				LOG_ERR("Insufficient space for DFU (%zu < %" PRIu32 ")",
				flash_area->fa_size, OTA_file_inf.parameter.FW_code_size);

				OTA_step = 20;
			} else {
				OTA_work_restart(IMMEDIATELY_TIMEOUT);
			}
		}
#if 1
		else {
			OTA_resend_count++;
			if (OTA_resend_count > 40) {
				OTA_step = 20; // resend again
			}
			OTA_work_restart(10);
		}
#endif
		break;

	case 10:
		if (erase_offset == flash_area->fa_size) {
			OTA_step++;
			OTA_work_stop();
			OTA_work_restart(IMMEDIATELY_TIMEOUT);
#if GEN3_CODE
			keyboard_matrix_disable();
#endif
		} else {
			int err = flash_area_erase(flash_area, erase_offset, FLASH_PAGE_SIZE);
			if (err) {
				LOG_ERR("Cannot erase flash area (%d)", err);
				OTA_step = 20;
				OTA_work_restart(IMMEDIATELY_TIMEOUT);
			} else {
				LOG_DBG("Erase flash addr:0x%08x ok!", (uint32_t)(erase_offset + flash_area->fa_off));
				erase_offset += FLASH_PAGE_SIZE;
				OTA_work_restart(FLASH_ERASE_TIMEOUT);
			}
		}
		OTA_resend_count = 0;
		break;

	case 11:
		OTA_packet.OTA_tx_data[0] = 0x23;
		OTA_packet.OTA_tx_data[1] = OTA_FW_hex_packet_seq; //
		OTA_packet.OTA_tx_data[2] = OTA_FW_hex_packet_ID;  //
		gzp_get_device_id(&OTA_packet.OTA_tx_data[3]);
		OTA_packet.OTA_tx_len = 6;
		OTA_packet.OTA_rx_pipe = 3;
		OTA_work_stop();

		if (gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len, OTA_packet.OTA_rx_data,
			      &OTA_packet.OTA_rx_len, OTA_packet.OTA_rx_pipe)) {
			if ( // OTA_packet.OTA_rx_len == 18 &&
				OTA_packet.OTA_rx_data[0] == 0xA3 &&
				OTA_packet.OTA_rx_data[1] == OTA_FW_hex_packet_seq &&
				is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[OTA_packet.OTA_rx_len - 3])) {
				if (OTA_packet.OTA_rx_data[2] == OTA_packet.OTA_tx_data[2]) {
					if (OTA_FW_hex_packet_ID == 0x00) {
						memcpy(&OTA_FW_hex_code.OTA_data[0],
						       &OTA_packet.OTA_rx_data[3], 12);
						OTA_FW_hex_packet_ID++;
					} else if (OTA_FW_hex_packet_ID == 0x01) {
						memcpy(&OTA_FW_hex_code.OTA_data[12],
						       &OTA_packet.OTA_rx_data[3], 12);

						OTA_FW_hex_packet_seq++;
						OTA_FW_hex_packet_ID = 0xff;

						// TODO: store FW_data to OTA_FW_buf
						if (OTA_FW_buf_cnt == 0) {
							if (OTA_file_inf.parameter.FW_encryption) {
								memcpy(&OTA_FW_enc[0],
								       OTA_FW_hex_code.OTA_data,
								       24);
							} else {
								memcpy(&OTA_FW_buf[0],
								       OTA_FW_hex_code.OTA_data,
								       24);
							}

							OTA_FW_buf_cnt = 1;
						} else // if(OTA_FW_buf_cnt == 1)
						{
							if (OTA_file_inf.parameter.FW_encryption) {
								memcpy(&OTA_FW_enc[24],
								       OTA_FW_hex_code.OTA_data,
								       24);
								// TODO: aes decryption
								// cf_cbc_decrypt(&g_cbc,
								// OTA_FW_enc, OTA_FW_buf,
								// 3);
								ecb_128_decrpt_with_aes128_key(
									(const uint8_t(*)[sizeof(
										ota_aes_key)])
										ota_aes_key,
									OTA_FW_enc,
									sizeof(OTA_FW_enc),
									OTA_FW_buf,
									sizeof(OTA_FW_buf));
							} else {
								memcpy(&OTA_FW_buf[24],
								       OTA_FW_hex_code.OTA_data,
								       24);
							}

							int err = flash_area_write(flash_area,
												fw_hex_code_received_byte,
												OTA_FW_buf, 48);
							if (err) {
								LOG_ERR("Cannot write data (%d)", err);
								OTA_step = 20;
							} else {
								LOG_DBG("write flash offset:0x%08X OK", fw_hex_code_received_byte);
							}
							checksum_from_recive(fw_hex_code_received_byte, OTA_FW_buf,
								     sizeof(OTA_FW_buf));

							fw_hex_code_received_byte +=
									48; // for next data
							OTA_FW_buf_cnt = 0;
						}
					}
				} else if (OTA_packet.OTA_tx_data[2] == 0xff) {
					OTA_FW_hex_packet_ID = 0x00;
				} else {
					OTA_FW_hex_packet_ID = 0xff;
				}
				OTA_work_restart(2);

				OTA_resend_count = 0;
			} else {
				if (OTA_packet.OTA_rx_data[0] == 0xA4 &&
				is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[OTA_packet.OTA_rx_len - 3])) {

					OTA_step++;
					OTA_work_restart(4);

					OTA_resend_count = 0;
				} else if (OTA_packet.OTA_rx_data[0] == 0xA5 &&
					is_recive_dev_id_correct(&OTA_packet.OTA_rx_data[1])) {
					OTA_step = 20;
					OTA_work_restart(4);

					OTA_resend_count = 0;

				} else //( OTA_packet.OTA_rx_len == 0x00  )
				{
					if (OTA_packet.OTA_tx_data[2] == 0xff) {
						OTA_FW_hex_packet_ID = 0x00;
					} else {
						OTA_FW_hex_packet_ID = 0xFF;
					}
					if (OTA_packet.OTA_rx_len == 0x00) {
						OTA_work_restart(4);
					} else {
						OTA_work_restart(2);
					}

					__ASSERT_NO_MSG(err_code == 0);
					// APP_ERROR_CHECK(err_code);

					OTA_resend_count++;
					if (OTA_resend_count > OTA_DONGLE_REPLY_ERROR_CNT) {
						OTA_step = 20; // eixt OTA mode
					}
				}
			}
		} else {
			if (OTA_packet.OTA_tx_data[2] == 0xff) {
				OTA_FW_hex_packet_ID = 0x00;
			} else {
				OTA_FW_hex_packet_ID = 0xFF;
			}
			OTA_resend_count++;
			if (OTA_resend_count > 250) {
				OTA_step = 20;
				OTA_status = OTA_UPATE_TIMEOUT;
				LOG_DBG("OTA_resend_count max!!!");
			}
			OTA_work_restart(10);
		}
		break;

	case 12:
		OTA_work_stop();

		// Byte1 Status:
		//       0: Check Sum is correct
		//       1: Check Sum is error.
		//       2: Time Out
		// Byte2 Time to waiting for moving FW hex code
		//       from external FLASH ROM to internal FLASH ROM.
		//       Waiting time is in unit of 100 mS

		OTA_packet.OTA_tx_data[0] = 0x24;
		OTA_packet.OTA_tx_data[2] = 0; //
		LOG_DBG("Info check sum  :0x%08X", OTA_file_inf.parameter.FW_checksum);
		LOG_DBG("Recive check sum:0x%08X", OTA_FW_recevied_checksum);
		LOG_DBG("Info code size   :%d", OTA_file_inf.parameter.FW_code_size);
		LOG_DBG("Recive code bytes:%d", fw_hex_code_received_byte);
#if CHECK_SUM_FROM_FLASH
		OTA_FW_recevied_checksum = checksum_from_flash();
		LOG_DBG("checksum from flash:0x%08X", OTA_FW_recevied_checksum);
#endif
		if ((OTA_FW_recevied_checksum == OTA_file_inf.parameter.FW_checksum) &&
		    (fw_hex_code_received_byte == OTA_file_inf.parameter.FW_code_size))
		// if(OTA_FW_recevied_checksum == OTA_file_inf.parameter.FW_checksum)
		{
			LOG_DBG("Check sum & firmware size OK");
			OTA_packet.OTA_tx_data[1] = 0;	//
			OTA_packet.OTA_tx_data[2] = 50; // 100ms *50
			// TODO: set bootloader action
			//------------------------------
		} else if (fw_hex_code_received_byte == OTA_file_inf.parameter.FW_code_size) {
			LOG_ERR("Check sum fail!");
			OTA_packet.OTA_tx_data[1] = 2; //

		} else {
			LOG_ERR("Recive firmware error!");
			OTA_packet.OTA_tx_data[1] = 1; //
		}

		gzp_get_device_id(&OTA_packet.OTA_tx_data[3]);
		OTA_packet.OTA_tx_len = 6;
		OTA_packet.OTA_rx_pipe = 3;

		OTA_resend_count = 0;
		while (!gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len,
				  OTA_packet.OTA_rx_data, &OTA_packet.OTA_rx_len,
				  OTA_packet.OTA_rx_pipe)) {
			// OTA_step++;

			OTA_resend_count++;
			if (OTA_resend_count > 20) {
				break;
			}
		}
		if (OTA_resend_count == 20) {
			OTA_step = 20;
			OTA_status = OTA_UPATE_TIMEOUT;
		} else {
#if CONFIG_BOOTLOADER_MCUBOOT && !CONFIG_DESKTOP_CONFIG_CHANNEL_DFU_MCUBOOT_DIRECT_XIP
		int err = boot_request_upgrade(false);
		if (err) {
			LOG_ERR("Cannot request the image upgrade (err:%d)", err);
		} else {
			LOG_INF("boot_request_upgrade set");
		}
#endif
			OTA_step++;
		}
		OTA_work_restart(200);
		break;

	case 13:
		//    if(gzp_tx_rx(OTA_packet.OTA_tx_data, OTA_packet.OTA_tx_len,
		//    OTA_packet.OTA_rx_data, &OTA_packet.OTA_rx_len,
		//    OTA_packet.OTA_rx_pipe))
		{
			NVIC_SystemReset();
		}

		// Byte 1: Status:
		//  0: succeed in storing into internal Flash ROM
		//  1: fail in storing into internal Flash ROM
		//  2: Time out

		// Byte 2: FW type
		// Byte 3~6:  Current FW version
		break;

	case 20:

		//  OTA_send_current_FW_version();
		//  OTA_mode_f = false;
		//------------------------------
		OTA_step++;
		//------------------------------

#if 0
            nrf_gzp_disable_gzll();
  
            nrf_gzll_set_max_tx_attempts(40);
            gzll_update_ok &=  nrf_gzll_set_sync_lifetime(10);
            gzll_update_ok &= nrf_gzll_enable();
#endif
		break;

	case 21:
		OTA_work_stop();
#if GEN3_CODE
		pair_led_off();
		//            led_ota_trigger(CUSTOM_SS_NORMAL);  wayne
#endif

		// OTA_mode_f = false;
		m_work_mode = WORK_NORMAL;

		if (OTA_send_current_FW_version()) {
			//	  OTA_status = 0;
		}
		flash_area_close(flash_area);
		flash_area = NULL;

		// buffer_clear();
#if GEN3_CODE
		keyboard_matrix_enable();
#endif
		break;
	}
}

void OTA_init(void)
{
#if CONFIG_BOOTLOADER_MCUBOOT && !CONFIG_DESKTOP_CONFIG_CHANNEL_DFU_MCUBOOT_DIRECT_XIP
	int err = boot_write_img_confirmed();

	if (err) {
		LOG_ERR("Cannot confirm a running image");
	}
#endif
	k_work_init_delayable(&ota_work, ota_work_fn);
	//  keyboard_matrix_disable();
	// Generate_Challenge_Code();
}