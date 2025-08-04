/* main.c - Application main entry point */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
// #include <zephyr/bluetooth/hci_vs.h>

#include <psa/crypto.h>
#include <psa/crypto_extra.h>

#define MODULE dell_pair

#include <caf/events/module_state_event.h>
//#include <caf/events/ble_common_event.h>


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);

#define QUICK_PAIR_CMD_REQUEST_TO_AUTHENTICATE 1
#define QUICK_PAIR_CMD_REQUEST_TO_AUTHENTICATE_V2 0x41
#define QUICK_PAIR_CMD_REQUEST_TO_AUTHENTICATE_REPLY 0x31
#define QUICK_PAIR_CMD_CHALLENGE_RESPONSE 2 
#define QUICK_PAIR_CMD_CHALLENGE_RESPONSE_REPLY 0x32

bool is_dell_pair=false;
uint32_t g_passkey;

//
// Extracted private part from PEM BAS64 to HEX 
// MHcCAQEEIG9r9KxlCJxJrBtRQyusyJ4HyhKlEfKQhyRdnmRCDQF0oAoGCCqGSM49
// AwEHoUQDQgAEetAcy/AOoc/VY9kFOPtbgQK4npHtEc/NT6cezeXydhtfQ3nvNOZT
// F9/wWk6+86VBYCVK7mRm8+aW7Me1WEDzTQ==
//
static const uint8_t g_DevicePriv1Key[] = { 0x6f,0x6b,0xf4,0xac,0x65,0x08,
							   0x9c,0x49,0xac,0x1b,0x51,0x43,
							   0x2b,0xac,0xc8,0x9e,0x07,0xca,
							   0x12,0xa5,0x11,0xf2,0x90,0x87,
							   0x24,0x5d,0x9e,0x64,0x42,0x0d,
							   0x01,0x74 };

//
// Extracted public part from PEM BAS64 to HEX
// MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEetAcy/AOoc/VY9kFOPtbgQK4npHtEc/NT6cezeXydhtfQ3nvNOZTF9/wWk6+86VBYCVK7mRm8+aW7Me1WEDzTQ==
//
static const uint8_t g_DevicePub1Key[] = {0x04,0x7a,0xd0,0x1c,0xcb,0xf0,
							 0x0e,0xa1,0xcf,0xd5,0x63,0xd9,
							 0x05,0x38,0xfb,0x5b,0x81,0x02,
							 0xb8,0x9e,0x91,0xed,0x11,0xcf,
							 0xcd,0x4f,0xa7,0x1e,0xcd,0xe5,
							 0xf2,0x76,0x1b,0x5f,0x43,0x79,
							 0xef,0x34,0xe6,0x53,0x17,0xdf,
							 0xf0,0x5a,0x4e,0xbe,0xf3,0xa5,
							 0x41,0x60,0x25,0x4a,0xee,0x64,
							 0x66,0xf3,0xe6,0x96,0xec,0xc7,
							 0xb5,0x58,0x40,0xf3,0x4d};

//
// Extracted public part from PEM BAS64 to HEX
// MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE/PPDDHvuCuKFHn7Z6z+lWWhzoinsczsyhGSZt5BWnQ/Ot8Y6FqFP9FnaRdTXpjWn1xfTbxxP1bx6sXCh+9s2KQ==
//
static const uint8_t g_PCPub1Key[] = { 0x04, 0xfc,0xf3,0xc3,0x0c,0x7b,
                    	  0xee,0x0a,0xe2,0x85,0x1e,0x7e,
                          0xd9,0xeb,0x3f,0xa5,0x59,0x68,
                          0x73,0xa2,0x29,0xec,0x73,0x3b,
                          0x32,0x84,0x64,0x99,0xb7,0x90,
                          0x56,0x9d,0x0f,0xce,0xb7,0xc6,
                          0x3a,0x16,0xa1,0x4f,0xf4,0x59,
                          0xda,0x45,0xd4,0xd7,0xa6,0x35,
                          0xa7,0xd7,0x17,0xd3,0x6f,0x1c,
                          0x4f,0xd5,0xbc,0x7a,0xb1,0x70,
                          0xa1,0xfb,0xdb,0x36,0x29 };

// for dell pair v2
static const uint8_t g_DevicePriv2Key[] = { 0xEE, 0xCC, 0xDA, 0x5C, 0x4D, 0x31, 0xA0, 0xE3,
                                0xF9, 0x3A, 0x42, 0x81, 0x99, 0x17, 0xB3, 0xE5,
                                0xFA, 0x49, 0xDC, 0x3C, 0xD9, 0x49, 0xF4, 0x27,
                                0x80, 0x2E, 0xE8, 0x1A, 0xA4, 0x4A, 0xD8, 0xB0};

static const uint8_t g_DevicePub2Key[] = {0x04, 0x9F, 0x9C, 0x92, 0x5A, 0xD2, 0x4B, 0xCF,
                            0xAB, 0x2D, 0x39, 0xD4, 0xC6, 0xD8, 0x1C, 0xCA,
                            0x2F, 0x36, 0x71, 0x74, 0xA0, 0x33, 0x23, 0xA4,
                            0xCC, 0x7C, 0xDF, 0x94, 0x95, 0x86, 0x6E, 0x13,
                            0x75, 0xA9, 0xC6, 0x3F, 0x13, 0x04, 0x3B, 0x8B,
                            0xEC, 0xA0, 0x28, 0x9D, 0xDC, 0xB4, 0x05, 0x1C,
                            0x9A, 0xF7, 0x7A, 0xE1, 0x40, 0x02, 0xF1, 0xC6,
                            0x54, 0x6B, 0xC7, 0xC3, 0xC8, 0xC4, 0x22, 0x16,
                            0x38};

static const uint8_t g_PCPub2Key[] = { 0x04, 0xC3, 0x60, 0x35, 0x41, 0xC0, 0x4C, 0x79,
                            0x5B, 0x86, 0xDE, 0x02, 0xD9, 0xA3, 0xBC, 0xCF,
                            0x62, 0x20, 0x65, 0xFE, 0xA3, 0xEC, 0xBE, 0x18,
                            0x13, 0xBF, 0xA9, 0x37, 0x5A, 0x6B, 0x2E, 0xE8,
                            0x81, 0x11, 0x53, 0x85, 0xDA, 0x41, 0x93, 0xE6,
                            0x49, 0x9F, 0x83, 0xA7, 0x1C, 0x3F, 0x35, 0xF8,
                            0x80, 0x8B, 0x9D, 0x36, 0x1B, 0xCF, 0x4B, 0x09,
                            0x97, 0xD9, 0xC7, 0x67, 0x72, 0x22, 0x41, 0x00,
                            0x87};
#define DevicePrivKeySize   32
#define DevicePubKeySize    65
#define PCPubKeySize        65

static const uint8_t *g_DevicePrivKey,*g_DevicePubKey,*g_PCPubKey;

static void establish_key(uint8_t dell_pair_ver)
{
    if (dell_pair_ver == 1) {
        g_DevicePrivKey = g_DevicePriv1Key;
        g_DevicePubKey = g_DevicePub1Key;
        g_PCPubKey= g_PCPub1Key;
    } else if (dell_pair_ver == 2) {
        g_DevicePrivKey = g_DevicePriv2Key;
        g_DevicePubKey = g_DevicePub2Key;
        g_PCPubKey= g_PCPub2Key;
    }
}

uint8_t g_LasKey[32];						   
uint8_t g_LastaNonce[12];
uint8_t g_LastChallenge[16];
uint8_t g_AddData[32];

typedef struct {
	uint8_t cmd;
	uint16_t data_length;
} __attribute__((packed)) quick_pair_cmd_header_t;

typedef struct {
	bool in_progress;

	union {
		uint8_t write_buffer[1024];
		quick_pair_cmd_header_t header;
	} __attribute__((packed)) write;

	union {
		uint8_t read_buffer[1024];
		quick_pair_cmd_header_t header;
	} __attribute__((packed)) read;

} __attribute__((packed)) quick_pair_cmd_buffer_t;

quick_pair_cmd_buffer_t g_quick_pair_cmd_buffer;


static bool hmac_sha_256(const uint8_t* key, const size_t key_length, const uint8_t *data, const size_t data_length, uint8_t (*result)[32])
{
	psa_status_t status;
	uint32_t olen;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
	psa_key_handle_t key_handle;

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_SIGN_HASH);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_HMAC);

	/* Import a key. The key is not exposed to the application,
	 * we can use it to encrypt/decrypt using the key handle
	 */
	status = psa_import_key(&key_attributes, key, key_length, &key_handle);
	if (status != PSA_SUCCESS)
	{
		LOG_ERR("hmac_sha_256 psa_import_key failed! (Error: %d)", status);
		return false;
	}

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	/* Initialize the HMAC signing operation */
	status = psa_mac_sign_setup(&operation, key_handle, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	if (status != PSA_SUCCESS)
	{
		psa_destroy_key(key_handle);
		LOG_ERR("psa_mac_sign_setup failed! (Error: %d)", status);
		return false;
	}

	/* Perform the HMAC signing */
	status = psa_mac_update(&operation, data, data_length);
	if (status != PSA_SUCCESS)
	{
		psa_destroy_key(key_handle);
		LOG_ERR("psa_mac_update failed! (Error: %d)", status);
		return false;
	}

	/* Finalize the HMAC signing */
	status = psa_mac_sign_finish(&operation, result[0], sizeof(result[0]), &olen);
	if (status != PSA_SUCCESS)
	{
		psa_destroy_key(key_handle);
		LOG_ERR("psa_mac_sign_finish failed! (Error: %d)", status);
		return false;
	}

	psa_destroy_key(key_handle);

	return true;
}

static bool encrypt_AES256_CCM(const uint8_t (*key)[32], uint8_t* inData, size_t inDataLength, uint8_t* outData, size_t* outDataLength, uint8_t* nonce, size_t nonceLength, uint8_t* addData, size_t addDataLength, uint8_t tagLen)
{
	psa_status_t status;
	uint32_t output_len;
	psa_key_handle_t key_handle;

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM,tagLen));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, 256);


	status = psa_import_key(&key_attributes, key[0], sizeof(key[0]), &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_ERR("decrypt_AES256_CCM psa_import_key failed! (Error: %d)", status);
		return false;
		
	}

	status = psa_aead_encrypt(key_handle,
							  PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM,tagLen),
							  nonce,
							  nonceLength,
							  addData,
							  addDataLength,
							  inData,
							  inDataLength,
							  outData,
							  *outDataLength,
							  &output_len);

	
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_aead_decrypt failed! (Error: %d)", status);
		return false;
		
	}

	*outDataLength = output_len;

	psa_destroy_key(key_handle);

	return true;	
}

static bool decrypt_AES256_CCM(const uint8_t (*key)[32], uint8_t* inData, size_t inDataLength, uint8_t* outData, size_t* outDataLength, uint8_t* nonce, size_t nonceLength, uint8_t* addData, size_t addDataLength, uint8_t tagLen)
{
	psa_status_t status;
	uint32_t output_len;
	psa_key_handle_t key_handle;

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM,tagLen));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, 256);


	status = psa_import_key(&key_attributes, key[0], sizeof(key[0]), &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_ERR("decrypt_AES256_CCM psa_import_key failed! (Error: %d)", status);
		return false;
		
	}

	status = psa_aead_decrypt(key_handle,
							  PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM,tagLen),
							  nonce,
							  nonceLength,
							  addData,
							  addDataLength,
							  inData,
							  inDataLength,
							  outData,
							  *outDataLength,
							  &output_len);

	
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_aead_decrypt failed! (Error: %d)", status);
		return false;
		
	}

	*outDataLength = output_len;

	psa_destroy_key(key_handle);

	return true;	
}

static bool sign_message_ECDSA(const uint8_t* key, const size_t key_length, const uint8_t *data, const size_t data_length, uint8_t *signature, size_t* signature_length )
{
	psa_key_handle_t priv_key_handle;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_status_t status;
	uint32_t output_len;
	uint8_t hash[32];

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_SIGN_HASH);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&key_attributes, 256);

	status = psa_import_key(&key_attributes, key, key_length, &priv_key_handle);

	if (status != PSA_SUCCESS) {
		LOG_ERR("sign_message_ECDSA psa_import_key failed! (Error: %d)", status);
		return false;
		
	}

	status = psa_hash_compute(PSA_ALG_SHA_256,
				  data,
				  data_length,
				  hash,
				  sizeof(hash),
				  &output_len);

	if (status != PSA_SUCCESS) {
		psa_destroy_key(priv_key_handle);
		LOG_ERR("psa_hash_compute failed! (Error: %d)", status);
		return false;
	}

	status = psa_sign_hash(priv_key_handle,
			       PSA_ALG_ECDSA(PSA_ALG_SHA_256),
			       hash,
			       sizeof(hash),
			       signature,
			       *signature_length,
			       &output_len);

	if (status != PSA_SUCCESS) {
		psa_destroy_key(priv_key_handle);
		LOG_ERR("psa_sign_hash failed! (Error: %d)", status);
		return false;
	}

	*signature_length = output_len;

	psa_destroy_key(priv_key_handle);

	return true;

}


static bool verify_message_ECDSA(const uint8_t* key, const size_t key_length, const uint8_t *data, const size_t data_length, const uint8_t *signature, const size_t signature_length )
{
	psa_key_handle_t pub_key_handle;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_status_t status;
	uint32_t output_len;
	uint8_t hash[32];

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_VERIFY_HASH);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&key_attributes, 256);

	status = psa_import_key(&key_attributes, key, key_length, &pub_key_handle);

	if (status != PSA_SUCCESS) {
		LOG_ERR("verify_message_ECDSA psa_import_key failed! (Error: %d)", status);
		return false;
		
	}

	status = psa_hash_compute(PSA_ALG_SHA_256,
				  data,
				  data_length,
				  hash,
				  sizeof(hash),
				  &output_len);

	if (status != PSA_SUCCESS) {
		psa_destroy_key(pub_key_handle);
		LOG_ERR("psa_hash_compute failed! (Error: %d)", status);
		return false;
	}

	status = psa_verify_hash(pub_key_handle,
				 PSA_ALG_ECDSA(PSA_ALG_SHA_256),
				 hash,
				 sizeof(hash),
				 signature,
				 signature_length);

	if (status != PSA_SUCCESS) {
		psa_destroy_key(pub_key_handle);
		LOG_ERR("psa_verify_hash failed! (Error: %d)", status);
		return false;
	}

	psa_destroy_key(pub_key_handle);

	return true;
}

bool generate_random(uint8_t *result, size_t result_length)
{	
	psa_status_t status;

	status = psa_generate_random(result, result_length);
	if (status != PSA_SUCCESS)
	{
		LOG_ERR("psa_generate_random failed! (Error: %d)", status);
		return false;
	}

	return true;
}

static bool create_shared_key(uint8_t* tcPub1Key, uint8_t tcPub1KeyLength, uint8_t (*shrKey1)[32])
{


	uint8_t skt1AgreedSecret[32];
	unsigned long cp1 = 0xb22ccea2;
//    uint8_t hashKey[ sizeof( cp1 ) + sizeof( g_DevicePub1Key ) + sizeof( g_PCPub1Key ) ] = {};
    uint8_t hashKey[ sizeof( cp1 ) + DevicePubKeySize + PCPubKeySize ] = {};

	psa_status_t status;
	psa_key_attributes_t dev_priv_1_key_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_handle_t dev_priv_1_key_handle;

	psa_set_key_usage_flags(&dev_priv_1_key_attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_lifetime(&dev_priv_1_key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&dev_priv_1_key_attributes, PSA_ALG_ECDH);
	psa_set_key_type(&dev_priv_1_key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&dev_priv_1_key_attributes, 256);

	//status = psa_import_key (&dev_priv_1_key_attributes, g_DevicePriv1Key, sizeof(g_DevicePriv1Key), &dev_priv_1_key_handle);
    status = psa_import_key (&dev_priv_1_key_attributes, g_DevicePrivKey, DevicePrivKeySize, &dev_priv_1_key_handle);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_import_key(dev_priv_1_key) failed! (Error: %d)", status);
		return false;
	}

	uint32_t skt1AgreedSecretSize;

	status = psa_raw_key_agreement(PSA_ALG_ECDH, dev_priv_1_key_handle, tcPub1Key, tcPub1KeyLength, skt1AgreedSecret, sizeof(skt1AgreedSecret), &skt1AgreedSecretSize);
	
	if (status != PSA_SUCCESS) {
		psa_destroy_key(dev_priv_1_key_handle);
		LOG_ERR("psa_raw_key_agreement failed! (Error: %d)", status);
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)skt1AgreedSecret, sizeof(skt1AgreedSecret), "skT1 :");

	//
    // Generate Shrkey1 = HMAC_SHA256(CP1 || DevPubk1|| PCPubk1 ,  skT1)
    //
	memcpy( &hashKey[ 0 ], &cp1, sizeof( cp1 ) );
//    memcpy( &hashKey[ sizeof( cp1 ) ], g_DevicePub1Key, sizeof( g_DevicePub1Key ) );
    memcpy( &hashKey[ sizeof( cp1 ) ], g_DevicePubKey, DevicePubKeySize);
//    memcpy( &hashKey[ sizeof( cp1 ) + sizeof( g_DevicePub1Key ) ], g_PCPub1Key, sizeof( g_PCPub1Key ) );
    memcpy( &hashKey[ sizeof( cp1 ) + DevicePubKeySize ], g_PCPubKey, PCPubKeySize );

	if(!hmac_sha_256((const uint8_t*)hashKey, sizeof(hashKey), (const uint8_t *)skt1AgreedSecret, (const size_t)skt1AgreedSecretSize, (uint8_t(*)[sizeof(shrKey1[0])])shrKey1))
	{
		psa_destroy_key(dev_priv_1_key_handle);
		LOG_ERR("hmacHash256 failed!!!");
		return false;
	}

	//
    // Generate addition data for AES256_CCM
    // Addition Data= HMAC( CP3, tcPub1Key)
    //
	unsigned char cp3[] = { 0xd6, 0xf0, 0xa2, 0xb9, 0xa1, 0x43, 0x00, 0x58 }; // d6f0a2b9a1430058
	memset(g_AddData, 0x00, sizeof(g_AddData));

	if(!hmac_sha_256((const uint8_t*)cp3, sizeof(cp3), (const uint8_t *)tcPub1Key, (const size_t)tcPub1KeyLength, (uint8_t(*)[sizeof(g_AddData)])g_AddData))
	{
		psa_destroy_key(dev_priv_1_key_handle);
		LOG_ERR("hmacHash256 failed!!!");
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)g_AddData, sizeof(g_AddData), "g_AddData :");


	psa_destroy_key(dev_priv_1_key_handle);

	return true;
}

static bool process_quick_pair_cmd_request_to_authenticate(quick_pair_cmd_buffer_t *quick_pair_cmd_buffer)
{
	
	uint8_t shrKey1[32];
	uint8_t* tcPub1KeyLength = &quick_pair_cmd_buffer->write.write_buffer[sizeof(quick_pair_cmd_header_t)];
	uint8_t* tcPub1Key = (tcPub1KeyLength + 1);
	uint8_t buffer1[160];
	uint8_t buffer2[160];

	if(*tcPub1KeyLength != 65)
	{
		LOG_ERR("invalid tcPub1KeyLength : %d", *tcPub1KeyLength);
		return false;
	}

	LOG_HEXDUMP_DBG(tcPub1Key, *tcPub1KeyLength, "tcPub1Key :");


	if(!create_shared_key( tcPub1Key, *tcPub1KeyLength, (uint8_t(*)[sizeof(shrKey1)])shrKey1))
	{
		LOG_ERR("create_shared_key failed!!!");
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)shrKey1, sizeof(shrKey1), "shrKey1 :");

	//
    // ANonce1=LSB(HMAC(DevPubK1, TCPubkey1) ,13 Bytes)  ///Extract 12 Bytes from
    //
    uint8_t aNonce1Data[32];
//	if(!hmac_sha_256((const uint8_t*)g_DevicePub1Key, sizeof(g_DevicePub1Key), (const uint8_t *)tcPub1Key, 65, (uint8_t(*)[sizeof(aNonce1Data)])aNonce1Data))
    if(!hmac_sha_256((const uint8_t*)g_DevicePubKey, DevicePubKeySize, (const uint8_t *)tcPub1Key, 65, (uint8_t(*)[sizeof(aNonce1Data)])aNonce1Data))
	{
		LOG_ERR("hmacHash256 failed!!!");
	}

	LOG_HEXDUMP_DBG((uint8_t *)aNonce1Data, 12, "aNonce1 :");

	uint8_t* ep1Length = tcPub1Key + *tcPub1KeyLength;

	if(*ep1Length < 16)
	{
		LOG_ERR("invalid ep1Length : %d", *ep1Length);
		return false;
	}

	memset(buffer1, 0x00, sizeof(buffer1));
	uint8_t * ep1 = (ep1Length + 1);
	uint8_t* p1 = buffer1;
	size_t p1Length = sizeof(buffer1);

	LOG_HEXDUMP_DBG((uint8_t *)ep1, *ep1Length, "ep1 :");

	if(!decrypt_AES256_CCM((uint8_t(*)[sizeof(shrKey1)])shrKey1,ep1,*ep1Length,p1, &p1Length, aNonce1Data, 12, g_AddData, 16, 4))
	{
		LOG_ERR("decrypt_AES256_CCM failed!!!");
		return false;

	}

	LOG_HEXDUMP_DBG((uint8_t *)p1, p1Length, "p1 :");

	uint8_t * key2Length = p1;

	if(*key2Length != sizeof(g_LasKey))
	{
		LOG_ERR("invalid key2Length : %d", *key2Length);
		return false;
	}

	uint8_t * key2 = (key2Length + 1);

	LOG_HEXDUMP_DBG((uint8_t *)key2, *key2Length, "key2 :");

	uint8_t *ss1Length = key2 + *key2Length;

	if(*ss1Length < 16)
	{
		LOG_ERR("invalid ss1Length : %d", *ss1Length);
		return false;
	}

	uint8_t *ss1 = (ss1Length + 1);

	LOG_HEXDUMP_DBG((uint8_t *)ss1, *ss1Length, "ss1 :");

	//
    // ECDSA (PCPubK1,SHA256(TCPub1|| KEY2))
    //
	memset(buffer2, 0x00, sizeof(buffer2));
	uint8_t* ss1Data = buffer2;
	memcpy(ss1Data, tcPub1Key, *tcPub1KeyLength);
	memcpy(&ss1Data[*tcPub1KeyLength], key2, *key2Length);

	if(!verify_message_ECDSA(g_PCPubKey, PCPubKeySize, ss1Data, *tcPub1KeyLength + *key2Length, ss1, *ss1Length))
	{
		LOG_ERR("verify_message_ECDSA failed!!!");
		return false;

	}

	//
	// Keep key2 fore next steps
	//
	memcpy(g_LasKey, key2, sizeof(g_LasKey));

	//
	// Step-2, Device  to send challenge String
	//

	//
	// Create challenge
	//
	memset(buffer1, 0x00, sizeof(buffer1));
	uint8_t *challenge = buffer1;
	generate_random(challenge,16);

	LOG_HEXDUMP_DBG((uint8_t *)challenge, 16, "challenge :");

	memcpy(g_LastChallenge, challenge, sizeof(g_LastChallenge));

	//
	// DevSS2=ECDSA( DevPrivK1,SHA256(PCPubk1||Key2)) 
	//
	memset(buffer2, 0x00, sizeof(buffer2));
	uint8_t* ss2Data = buffer2;
//	memcpy(ss2Data, g_PCPub1Key, sizeof(g_PCPub1Key));
	memcpy(ss2Data, g_PCPubKey, PCPubKeySize);
//	memcpy(&ss2Data[sizeof(g_PCPub1Key)], g_LasKey, sizeof(g_LasKey));
    memcpy(&ss2Data[PCPubKeySize], g_LasKey, sizeof(g_LasKey));

	uint8_t* ss2 = &challenge[1 + 1 + 16];
	size_t ss2Length = 128;

	//if(!sign_message_ECDSA(g_DevicePriv1Key, sizeof(g_DevicePriv1Key),ss2Data, sizeof(g_PCPub1Key) + sizeof(g_LasKey), ss2, &ss2Length))
	if(!sign_message_ECDSA(g_DevicePrivKey, DevicePrivKeySize,ss2Data, PCPubKeySize + sizeof(g_LasKey), ss2, &ss2Length))
    {
		LOG_ERR("sign_message_ECDSA failed!!!");
		return false;
	}	

	LOG_HEXDUMP_DBG((uint8_t *)ss2, ss2Length, "ss2 :");

	
	memset(buffer2, 0x00, sizeof(buffer2));
	uint8_t* p2 = buffer2;
	p2[0] = 16;
	memcpy(&p2[1], challenge, 16);
	p2[1 + 16] = (uint8_t)ss2Length;
	memcpy(&p2[1 + 16 + 1], ss2, ss2Length);
	size_t p2Length = (1 + 16 + 1 + ss2Length);
	p2Length = ( ( p2Length + 16 ) - ( p2Length % 16 ) ); // AES 16 byte block align

	LOG_HEXDUMP_DBG((uint8_t *)p2, p2Length, "p2 :");
	

	//
	// ANonce2(Byte length :13 ): LSB( HMAC(ANonce1,Key2),13 bytes )  ///Extract Byte[0] ..[11]
	//
	uint8_t aNonce2Data[32];
	if(!hmac_sha_256((const uint8_t*)aNonce1Data, 12, (const uint8_t *)g_LasKey, sizeof(g_LasKey), (uint8_t(*)[sizeof(aNonce2Data)])aNonce2Data))
	{
		LOG_ERR("hmacHash256 failed!!!");
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)aNonce2Data, 12, "aNonce2 :");

	memcpy(g_LastaNonce,aNonce2Data, sizeof(g_LastaNonce));

	uint8_t* ep2 = &quick_pair_cmd_buffer->read.read_buffer[sizeof(quick_pair_cmd_header_t)];
	size_t ep2Length = p2Length + 4;

	if(!encrypt_AES256_CCM((uint8_t(*)[sizeof(g_LasKey)])g_LasKey,p2,p2Length, ep2, &ep2Length, aNonce2Data, 12, g_AddData, 16, 4))
	{
		LOG_ERR("decrypt_AES256_CCM failed!!!");
		return false;

	}

	LOG_HEXDUMP_DBG((uint8_t *)ep2, ep2Length, "ep2 :");

	quick_pair_cmd_buffer->read.header.cmd = QUICK_PAIR_CMD_REQUEST_TO_AUTHENTICATE_REPLY;
	quick_pair_cmd_buffer->read.header.data_length = ep2Length;


	LOG_HEXDUMP_DBG((uint8_t *)quick_pair_cmd_buffer->read.read_buffer, sizeof(quick_pair_cmd_header_t) + quick_pair_cmd_buffer->read.header.data_length, "Response data :");


	return true;
}

static bool process_quick_pair_cmd_challenge_response(quick_pair_cmd_buffer_t *quick_pair_cmd_buffer)
{
	uint8_t buffer1[160];
	uint8_t buffer2[160];

	uint16_t* ep3Length = &quick_pair_cmd_buffer->write.header.data_length;

	if(*ep3Length < 16)
	{
		LOG_ERR("invalid ep3Length : %d", *ep3Length);
		return false;
	}

	memset(buffer1, 0x00, sizeof(buffer1));
	uint8_t * ep3 = ((uint8_t*)ep3Length + 2);
	uint8_t* p3 = buffer1;
	size_t p3Length = sizeof(buffer1);

	LOG_HEXDUMP_DBG((uint8_t *)ep3, *ep3Length, "ep3 :");

	//
	// Ep3=Eaes(Key2, Anonce2, EP3)  
	//
	if(!decrypt_AES256_CCM((uint8_t(*)[sizeof(g_LasKey)])g_LasKey,ep3,*ep3Length,p3, &p3Length, g_LastaNonce, 12, g_AddData, 16, 4))
	{
		LOG_ERR("decrypt_AES256_CCM failed!!!");
		return false;

	}

	LOG_HEXDUMP_DBG((uint8_t *)p3, p3Length, "p3 :");

	//
    // CR = HMAC( Rkey1, CHx ) ///HMAC( Key, SHA256(Data) ). Rkey1= SHA256(PCPubK1)
    //

	memset(buffer2, 0x00, sizeof(buffer2));
	uint8_t* rkey1 = buffer2;
	size_t rkey1Length;

	psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256,
				  						   g_PCPubKey,//g_PCPub1Key,
				  						   PCPubKeySize,//sizeof(g_PCPub1Key),
										   rkey1,
										   32,
										   &rkey1Length);

	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_hash_compute failed! (Error: %d)", status);
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)rkey1, rkey1Length, "rkey1 :");

	uint8_t challengeResponse[32] = {};

	if(!hmac_sha_256(rkey1, rkey1Length,g_LastChallenge, sizeof(g_LastChallenge),(uint8_t(*)[sizeof(challengeResponse)])challengeResponse ))
	{
		LOG_ERR("hmac_sha_256 failed!!!");
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)challengeResponse, sizeof(challengeResponse), "CR :");


	//
	// CRsigned=ECDSA( PCPubK1, CR, )
	//
	uint8_t* crSignedLength = p3;
	uint8_t* crSigned = crSignedLength + 1;
//	if(!verify_message_ECDSA(g_PCPub1Key, sizeof(g_PCPub1Key), challengeResponse, sizeof(challengeResponse), crSigned, *crSignedLength))
    if(!verify_message_ECDSA(g_PCPubKey, PCPubKeySize, challengeResponse, sizeof(challengeResponse), crSigned, *crSignedLength))
	{
		LOG_ERR("verify_message_ECDSA failed!!!");
		return false;

	}

	//
	// All good
	// Step-4, Prepare TOTP ( Time base OTP) , Dev->Client
	// Note : we have not finish State-4 to State-6 (section 2.2.1.2.4)
	// so we use temporary solution to generate passcode. As per Kaileong suggestion
	//
	// k=HMAC(key3, SHA256( CR || CRsigned) ) //this HOTP is based on retrieving from the last byte lower 4 bits as offset to retrive 4 bytes from offset . 
	// [offset Byte &0x7f ,MSB,[Offset+1],{offset +2][Offset+3,LSB]
	// HOTP =Truncate (k, 31 bits from offset ) Modulus 1000,000 , offset= Last Byte, lower nibble 4bits
	//
	

	//
	// Created Key3(32 Bytes) with Random() function. Key3=0x00 is not accepted
	// Key3Seed=Random(12 bytes)
	// Key3=SHA256( CP2||Key3Seed)  //Key3 to be reconstructed by PC.
	//

	memset(buffer2, 0x00, sizeof(buffer2));
	uint8_t *key3 = buffer2;
	size_t key3Length;
	uint8_t cp2[8] = {0x3f, 0xa3, 0x2a, 0x6e, 0xf0, 0x42, 0xb6, 0xcd};
	uint8_t key3Data[sizeof(cp2) + 12];
	
	memcpy(key3Data, cp2, sizeof(cp2));
	generate_random(&key3Data[sizeof(cp2)], 12);

	LOG_HEXDUMP_DBG((uint8_t *)key3Data, sizeof(key3Data), "key3Data :");


	status = psa_hash_compute(PSA_ALG_SHA_256,
				  			  key3Data,
				  			  sizeof(key3Data),
							  key3,
							  32,
							  &key3Length);

	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_hash_compute failed! (Error: %d)", status);
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)key3, 32, "key3 :");

	uint8_t *hsData = &buffer2[key3Length];
	size_t hsDataLength = sizeof(challengeResponse) + *crSignedLength;
	memcpy(hsData, challengeResponse, sizeof(challengeResponse));
	memcpy(&hsData[sizeof(challengeResponse)], crSigned, *crSignedLength);

	LOG_DBG("hsDataLength  = %d", hsDataLength);
	LOG_HEXDUMP_DBG((uint8_t *)hsData, hsDataLength, "hsData :");
	
	uint8_t *hs = &hsData[hsDataLength];

	if(!hmac_sha_256(key3, key3Length, hsData, hsDataLength,(uint8_t(*)[32])hs ))
	{
		LOG_ERR("hmac_sha_256 failed!!!");
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)hs, 32, "hs :");

	uint8_t dtData[4];
	uint8_t offset = (hs[31] & 0xF); // 4bits
	dtData[0] = hs[offset];
	dtData[1] = hs[offset + 1];
	dtData[2] = hs[offset + 2];
	dtData[3] = hs[offset + 3];
	uint32_t dt = *((uint32_t*)&dtData[0]) & 0x7FFFFFFF; // 31 bits

	LOG_DBG("offset  = %d", offset);
	LOG_HEXDUMP_DBG((uint8_t *)dtData, sizeof(dtData), "dtData :");
	LOG_DBG("dt  = 0x%x", dt);

	g_passkey = (dt % 1000000); // Paring pass key
	//bill gen passkey
	// dell_pair_passkey[0]=0x30+(g_passkey/100000)%10;
	// dell_pair_passkey[1]=0x30+(g_passkey/10000)%10;
	// dell_pair_passkey[2]=0x30+(g_passkey/1000)%10;
	// dell_pair_passkey[3]=0x30+(g_passkey/100)%10;
	// dell_pair_passkey[4]=0x30+(g_passkey/10)%10;
	// dell_pair_passkey[5]=0x30+g_passkey%10;
	is_dell_pair=true;
	//bill gen passkey
	LOG_INF("g_passkey = %d, 0x%X", g_passkey, g_passkey);

	memset(buffer1, 0x00, sizeof(buffer1));
	uint8_t* p4 = buffer1;	
	p4[0] = 12; // length of Key3Seed
	memcpy(&p4[1], &key3Data[sizeof(cp2)], p4[0]); //Key3Seed

	size_t p4Length = 1 + p4[0];
	p4Length = ( ( p4Length + 16 ) - ( p4Length % 16 ) ); // AES 16 byte block align

	LOG_HEXDUMP_DBG((uint8_t *)p4, p4Length, "p4 :");


	//
	// ANonce3(13 Bytes,) = LSB( HMAC(ANonce2,Key3),13 bytes )  // TODO : we can not use Key3 for ANonce3 because PC need to get Key3Seed to decrypt EP4 to create Key3
	// For now we will use ANonce2

	/*uint8_t aNonce3Data[32];
	if(!hmac_sha_256((const uint8_t*)g_LastaNonce, 12, (const uint8_t *)key3, 32, (uint8_t(*)[sizeof(aNonce3Data)])aNonce3Data))
	{
		LOG_ERR("hmacHash256 failed!!!");
		return false;
	}

	LOG_HEXDUMP_DBG((uint8_t *)aNonce3Data, 12, "aNonce3 :");*/


	uint8_t* ep4 = &quick_pair_cmd_buffer->read.read_buffer[sizeof(quick_pair_cmd_header_t)];
	size_t ep4Length = p4Length + 4;

	if(!encrypt_AES256_CCM((uint8_t(*)[sizeof(g_LasKey)])g_LasKey,p4,p4Length, ep4, &ep4Length, g_LastaNonce, 12, g_AddData, 16, 4)) // 
	{
		LOG_ERR("decrypt_AES256_CCM failed!!!");
		return false;

	}

	LOG_HEXDUMP_DBG((uint8_t *)ep4, ep4Length, "ep4 :");

	//
	// Keep key3 fore next steps
	//
	memcpy(g_LasKey, key3, sizeof(g_LasKey));


	quick_pair_cmd_buffer->read.header.cmd = QUICK_PAIR_CMD_CHALLENGE_RESPONSE_REPLY;
	quick_pair_cmd_buffer->read.header.data_length = ep4Length;


	LOG_HEXDUMP_DBG((uint8_t *)quick_pair_cmd_buffer->read.read_buffer, sizeof(quick_pair_cmd_header_t) + quick_pair_cmd_buffer->read.header.data_length, "Response data :");





	return true;
}

static ssize_t read_quick_pair_cmd_characteristics(struct bt_conn *conn,
						   const struct bt_gatt_attr *attr, void *buf,
						   uint16_t len, uint16_t offset)
{
	quick_pair_cmd_buffer_t *quick_pair_cmd_buffer = (quick_pair_cmd_buffer_t *)attr->user_data;

	LOG_DBG("Read len = %d, offset = %d, in_progress = %d\n", len, offset,
		quick_pair_cmd_buffer->in_progress);

	if (!quick_pair_cmd_buffer->in_progress) {
		return bt_gatt_attr_read(conn, attr, buf, len, offset,
					 quick_pair_cmd_buffer->read.read_buffer,
					 quick_pair_cmd_buffer->read.header.data_length +
						 sizeof(quick_pair_cmd_header_t));
	} else {
		return BT_GATT_ERR(BT_ATT_ERR_PROCEDURE_IN_PROGRESS);
	}
}

static ssize_t write_quick_pair_cmd_characteristics(struct bt_conn *conn,
						    const struct bt_gatt_attr *attr,
						    const void *buf, uint16_t len, uint16_t offset,
						    uint8_t flags)
{
	quick_pair_cmd_buffer_t *quick_pair_cmd_buffer = (quick_pair_cmd_buffer_t *)attr->user_data;

	LOG_DBG("Write len = %d, offset = %d, flags = 0x%x\n", len, offset, flags);

	if (offset + len > sizeof(quick_pair_cmd_buffer->write.write_buffer)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	LOG_HEXDUMP_DBG((uint8_t *)buf, len, "buf :");

	if (offset == 0) {
		memset(quick_pair_cmd_buffer->write.write_buffer, 0x00,
		       sizeof(quick_pair_cmd_buffer->write.write_buffer));
		memset(quick_pair_cmd_buffer->read.read_buffer, 0x00,
		       sizeof(quick_pair_cmd_buffer->read.read_buffer));

		if (len >= sizeof(quick_pair_cmd_header_t)) {
			if (((quick_pair_cmd_header_t *)buf)->cmd != QUICK_PAIR_CMD_REQUEST_TO_AUTHENTICATE &&
                ((quick_pair_cmd_header_t *)buf)->cmd != QUICK_PAIR_CMD_REQUEST_TO_AUTHENTICATE_V2 &&
			    ((quick_pair_cmd_header_t *)buf)->cmd != QUICK_PAIR_CMD_CHALLENGE_RESPONSE) {
				return BT_GATT_ERR(BT_ATT_ERR_NOT_SUPPORTED);
			}
		} else {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}

		LOG_DBG("CMD = %d, data_length = %d\n", ((quick_pair_cmd_header_t *)buf)->cmd,
			((quick_pair_cmd_header_t *)buf)->data_length);

		quick_pair_cmd_buffer->in_progress = true;
	}

	memcpy(&quick_pair_cmd_buffer->write.write_buffer[offset], buf, len);

	if (offset + len >=
	    (quick_pair_cmd_buffer->write.header.data_length + sizeof(quick_pair_cmd_header_t))) {
		bool is_cmd_success = false;

		LOG_DBG("processing command = %d\n", quick_pair_cmd_buffer->write.header.cmd);

		switch (quick_pair_cmd_buffer->write.header.cmd) {
		case QUICK_PAIR_CMD_REQUEST_TO_AUTHENTICATE:
            LOG_WRN("Dell pair is V1");
            establish_key(1);
			is_cmd_success =
				process_quick_pair_cmd_request_to_authenticate(quick_pair_cmd_buffer);
			break;
        case QUICK_PAIR_CMD_REQUEST_TO_AUTHENTICATE_V2:
            LOG_WRN("Dell pair is V2");
            establish_key(2);
			is_cmd_success =
				process_quick_pair_cmd_request_to_authenticate(quick_pair_cmd_buffer);

			break;
		case QUICK_PAIR_CMD_CHALLENGE_RESPONSE:
			is_cmd_success =
				process_quick_pair_cmd_challenge_response(quick_pair_cmd_buffer);
			break;
		default:
			break;
		}

		quick_pair_cmd_buffer->in_progress = false;

		if (!is_cmd_success) {
			return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
		}
	}

	return len;
}

static struct bt_uuid_128 quick_pair_svc_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x017A6F75, 0x6C6C, 0x42DA, 0xA6BC, 0xAC8AE948149E));

static const struct bt_uuid_128 quick_pair_cmd_characteristics_uuid =
	BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x08D1901C, 0xAC1F, 0x4BC1, 0x8362, 0xD06EDBCAF022));

static struct bt_gatt_cep quick_pair_cmd_characteristics_cep = {
	.properties = BT_GATT_CEP_RELIABLE_WRITE,
};

BT_GATT_SERVICE_DEFINE(quick_pair_svc, BT_GATT_PRIMARY_SERVICE(&quick_pair_svc_uuid),
		       BT_GATT_CHARACTERISTIC(&quick_pair_cmd_characteristics_uuid.uuid,
					      BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
					      BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
					      read_quick_pair_cmd_characteristics,
					      write_quick_pair_cmd_characteristics,
					      &g_quick_pair_cmd_buffer),
		       BT_GATT_CEP(&quick_pair_cmd_characteristics_cep), );


// void mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
// {
// 	printk("Updated MTU: TX: %d RX: %d bytes\n", tx, rx);
// }

// static struct bt_gatt_cb gatt_callbacks = { .att_mtu_updated = mtu_updated };
quick_pair_cmd_buffer_t test_cmd;
static bool dell_pair_init(void)
{
	psa_status_t status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_crypto_init failed, status = %d", status);
	}
    return status;
}

static bool handle_module_state_event(const struct module_state_event *event)
{
	if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
		int err = dell_pair_init();

		if (!err) {
			module_set_state(MODULE_STATE_READY);
            LOG_WRN("dell pair init ok!");
		} else {
			module_set_state(MODULE_STATE_ERROR);
            LOG_ERR("dell pair init ok!");
		}
	}

	return false;
}

static bool app_event_handler(const struct app_event_header *aeh)
{

    if (is_module_state_event(aeh)) {
		return handle_module_state_event(cast_module_state_event(aeh));
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}

APP_EVENT_LISTENER(MODULE, app_event_handler);

