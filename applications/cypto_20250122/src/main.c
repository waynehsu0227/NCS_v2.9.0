/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <psa/crypto.h>
#include <psa/crypto_extra.h>
#include <cracen_psa.h>
#ifdef CONFIG_BUILD_WITH_TFM
#include <tfm_ns_interface.h>
#endif

#include "trusted_storage_init.h"
#include <dk_buttons_and_leds.h>

#define APP_SUCCESS	    (0)
#define APP_ERROR	    (-1)
#define APP_SUCCESS_MESSAGE "Example finished successfully!"
#define APP_ERROR_MESSAGE   "Example exited with error!"

#define PRINT_HEX(p_label, p_text, len)                                                            \
	({                                                                                         \
		LOG_INF("---- %s (len: %u): ----", p_label, len);                                  \
		LOG_HEXDUMP_INF(p_text, len, "Content:");                                          \
		LOG_INF("---- %s end  ----", p_label);                                             \
	})

LOG_MODULE_REGISTER(persistent_key_usage, LOG_LEVEL_DBG);

/* ====================================================================== */
/*			Global variables/defines for the persistent key  example	  */

#define SAMPLE_ALG					PSA_ALG_GCM
#define NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE (128)
#define NRF_CRYPTO_EXAMPLE_AES_IV_SIZE (16)
#define NRF_CRYPTO_EXAMPLE_ECDSA_PUBLIC_KEY_SIZE (65)
#define NRF_CRYPTO_EXAMPLE_ECDSA_SIGNATURE_SIZE (64)
#define NRF_CRYPTO_EXAMPLE_ECDSA_HASH_SIZE (32)
#define NRF_CRYPTO_EXAMPLE_AES_CCM_TAG_LENGTH (16)

#define MK_PSA_KEY_HANDLE(key) \
    PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_RAW, key)
// PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_ENCRYPTED, key)


#define KEY_ID_COUNT 13
#define USE_KMU
#if defined(USE_KMU)
#define KEY_HANDLE1 MK_PSA_KEY_HANDLE(100)
#define KEY_HANDLE2 MK_PSA_KEY_HANDLE(104)
#define KEY_HANDLE3 MK_PSA_KEY_HANDLE(108)
#define KEY_HANDLE4 MK_PSA_KEY_HANDLE(112)
#define KEY_HANDLE5 MK_PSA_KEY_HANDLE(116)
#define KEY_HANDLE6 MK_PSA_KEY_HANDLE(118)
#define KEY_HANDLE7 MK_PSA_KEY_HANDLE(120)
#define KEY_HANDLE8 MK_PSA_KEY_HANDLE(124)
#define KEY_HANDLE9 MK_PSA_KEY_HANDLE(128)
#define KEY_HANDLE10 MK_PSA_KEY_HANDLE(130)
#define KEY_HANDLE11 MK_PSA_KEY_HANDLE(132)
#define KEY_HANDLE12 MK_PSA_KEY_HANDLE(134)
#define KEY_HANDLE13 MK_PSA_KEY_HANDLE(136)
static psa_key_id_t key_id1 = KEY_HANDLE1;
static psa_key_id_t key_id2 = KEY_HANDLE2;
static psa_key_id_t key_id3 = KEY_HANDLE3;
static psa_key_id_t key_id4 = KEY_HANDLE4;
static psa_key_id_t key_id5 = KEY_HANDLE5;
static psa_key_id_t key_id6 = KEY_HANDLE6;
static psa_key_id_t key_id7 = KEY_HANDLE7;
static psa_key_id_t key_id8 = 124;
static psa_key_id_t key_id9 = KEY_HANDLE9;
static psa_key_id_t key_id10 = KEY_HANDLE10;
static psa_key_id_t key_id11 = KEY_HANDLE11;
static psa_key_id_t key_id12 = KEY_HANDLE12;
static psa_key_id_t key_id13 = 136;
psa_key_id_t key_ids[KEY_ID_COUNT] = {KEY_HANDLE1, KEY_HANDLE2, KEY_HANDLE3, KEY_HANDLE4, KEY_HANDLE5, KEY_HANDLE6, KEY_HANDLE7, 124, KEY_HANDLE9, KEY_HANDLE10, KEY_HANDLE11, KEY_HANDLE12, 136};

#else
static psa_key_id_t key_id1=100;
static psa_key_id_t key_id2=104;
static psa_key_id_t key_id3=108;
static psa_key_id_t key_id4=112;
static psa_key_id_t key_id5=116;
static psa_key_id_t key_id6=118;
static psa_key_id_t key_id7=120;
static psa_key_id_t key_id8=124;
#endif

/* Below text is used as plaintext for encryption/decryption */
static uint8_t m_plain_text[NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE] = {
	"Example string to demonstrate basic usage of a persistent key."};

/* Below text is used as additional data for authentication */
static uint8_t m_additional_data[] = {
	"Example string of additional data"
};

// static uint8_t m_encrypted_text[PSA_CIPHER_ENCRYPT_OUTPUT_SIZE(
// 	PSA_KEY_TYPE_AES, SAMPLE_ALG, NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE)];
static uint8_t m_encrypted_text[NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE];

static uint8_t m_decrypted_text[NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE];
static uint8_t buf[32] = {"This is a test key from nordic.."};
static uint8_t buf2[65] = {"This is a sentence that has exactly sixty-five characters in len"};
static uint8_t pubkey[] = {0x04, 0xb1, 0x4e, 0x7b, 0x7a, 0x3f, 0x9e, 0x51, 0x63, 0x8c, 0x76,
			   0x17, 0xfb, 0x2f, 0x03, 0x38, 0xbd, 0xda, 0x7a, 0xf6, 0xb0, 0xbe,
			   0x98, 0x9f, 0x9d, 0xc6, 0x5c, 0x71, 0xb1, 0x83, 0xd8, 0x6a, 0x2b,
			   0xb9, 0x2d, 0xf4, 0xa3, 0xfc, 0xf1, 0xdd, 0xb9, 0xd8, 0x32, 0x2d,
			   0x41, 0x08, 0xad, 0x61, 0x7a, 0x29, 0x45, 0x79, 0xd1, 0x9f, 0xfd,
			   0x37, 0xf2, 0x45, 0x60, 0x27, 0xdd, 0xa0, 0xce, 0x34, 0xb4};
static uint8_t kc_HMAC_KEY[32] = {0x8a, 0x22, 0xbe, 0x19, 0x80, 0x8e, 0x41, 0xb8, 0x82, 0xfb, 0xe9, 0xda, 0x29, 0x82, 0x86, 0x1, 0x8e, 0x25, 0xb6, 0x54, 0xb9, 0xb1, 0x4c, 0xa5, 0xb7, 0xea, 0xec, 0xe9, 0xd3, 0xd8, 0x2f, 0xaa};
static uint8_t dk_HMAC_KEY[32] = {0xfb, 0xeb, 0x23, 0x17, 0x5d, 0xbc, 0x49, 0x66, 0xa6, 0xe6, 0x9e, 0x65, 0x20, 0x44, 0xb9, 0xcc, 0x46, 0x41, 0x35, 0x94, 0x87, 0xfe, 0x4e, 0x2b, 0x9d, 0x9, 0xb2, 0x99, 0x5, 0x3f, 0x15, 0x23};
static uint8_t m_iv[NRF_CRYPTO_EXAMPLE_AES_IV_SIZE];
static uint8_t m_pub_key[NRF_CRYPTO_EXAMPLE_ECDSA_PUBLIC_KEY_SIZE];
static uint8_t m_signature[NRF_CRYPTO_EXAMPLE_ECDSA_SIGNATURE_SIZE];
static uint8_t m_hash[NRF_CRYPTO_EXAMPLE_ECDSA_HASH_SIZE];
static uint8_t hmac[32];
static size_t m_pub_key_len;
static size_t m_signature_len;
/* ====================================================================== */

int crypto_init(void)
{
	psa_status_t status;

#ifdef CONFIG_TRUSTED_STORAGE_BACKEND_AEAD_KEY_DERIVE_FROM_HUK
	write_huk();
#endif /* CONFIG_TRUSTED_STORAGE_BACKEND_AEAD_KEY_DERIVE_FROM_HUK */

	/* Initialize PSA Crypto */
	status = psa_crypto_init();
	if (status != PSA_SUCCESS) {
		return APP_ERROR;
	}

	return APP_SUCCESS;
}

int crypto_finish(void)
{
	psa_status_t status;

	/* Destroy the key handle */
	status = psa_destroy_key(key_id1);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_destroy_key failed! (Error: %d)", status);
		return APP_ERROR;
	}

	return APP_SUCCESS;
}

int generate_persistent_gcm_key(void)
{
	psa_status_t status;

	LOG_INF("Generating random persistent AES key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT | PSA_KEY_USAGE_EXPORT) ;
	// psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_EXPORT) ;
	psa_set_key_algorithm(&key_attributes, PSA_ALG_GCM);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, 256);

	/* Persistent key specific settings */
#if defined(USE_KMU)
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
							PSA_KEY_PERSISTENCE_DEFAULT,
							PSA_KEY_LOCATION_CRACEN_KMU));
#else
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);
#endif
	psa_set_key_id(&key_attributes, key_id1);

	// /* Generate a random AES key with persistent lifetime. The key can be used for
	//  * encryption/decryption using the key_id.
	//  */
	// status = psa_generate_key(&key_attributes, &key_id);
	// if (status != PSA_SUCCESS) {
	// 	LOG_INF("psa_generate_key failed! (Error: %d)", status);
	// 	return APP_ERROR;
	// }

	status = psa_import_key(&key_attributes, buf, 32, &key_id1);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}

	/* Make sure the key is not in memory anymore, has the same affect then resetting the device
	 */
	// status = psa_purge_key(key_id);
	// if (status != PSA_SUCCESS) {
	// 	LOG_INF("psa_purge_key failed! (Error: %d)", status);
	// 	return APP_ERROR;
	// }

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x", key_id1);

	return APP_SUCCESS;
}

int generate_aes_ctr_key(void)
{
	psa_status_t status;

	LOG_INF("Generating random persistent AES CTR key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes,
				PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT | PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_CTR);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, 256);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
							PSA_KEY_PERSISTENCE_DEFAULT,
							PSA_KEY_LOCATION_CRACEN_KMU));
	psa_set_key_id(&key_attributes, key_id6);
	status = psa_import_key(&key_attributes, buf, 32, &key_id6);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}

	psa_reset_key_attributes(&key_attributes);

	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x", key_id6);

	return APP_SUCCESS;
}

int generate_aes_ecb_key(void)
{
	psa_status_t status;

	LOG_INF("Generating random persistent AES ECB key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes,
				PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT | PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECB_NO_PADDING);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, 128);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
							PSA_KEY_PERSISTENCE_DEFAULT,
							PSA_KEY_LOCATION_CRACEN_KMU));
	psa_set_key_id(&key_attributes, key_id9);
	status = psa_import_key(&key_attributes, buf, 32, &key_id9);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}

	psa_reset_key_attributes(&key_attributes);

	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x", key_id9);

	return APP_SUCCESS;
}

int generate_aes_cbc_key(void)
{
	psa_status_t status;

	LOG_INF("Generating random persistent AES CBC key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes,
				PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT | PSA_KEY_USAGE_EXPORT);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_CBC_NO_PADDING);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, 256);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
							PSA_KEY_PERSISTENCE_DEFAULT,
							PSA_KEY_LOCATION_CRACEN_KMU));
	psa_set_key_id(&key_attributes, key_id10);
	status = psa_import_key(&key_attributes, buf, 32, &key_id10);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}

	psa_reset_key_attributes(&key_attributes);

	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x", key_id10);

	return APP_SUCCESS;
}

int generate_aes_ccm_key(void)
{
	psa_status_t status;

	LOG_INF("Generating random persistent AES CCM key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes,
				PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT | PSA_KEY_USAGE_EXPORT);
	// psa_set_key_algorithm(&key_attributes, PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, NRF_CRYPTO_EXAMPLE_AES_CCM_TAG_LENGTH));
	psa_set_key_algorithm(&key_attributes, PSA_ALG_CCM);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, 256);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(
							PSA_KEY_PERSISTENCE_DEFAULT,
							PSA_KEY_LOCATION_CRACEN_KMU));
	psa_set_key_id(&key_attributes, key_id11);
	status = psa_import_key(&key_attributes, buf, 32, &key_id11);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}

	psa_reset_key_attributes(&key_attributes);

	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x", key_id11);

	return APP_SUCCESS;
}

int generate_persistent_key1(void)
{
	psa_status_t status;

	LOG_INF("Generating random persistent HASH key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes,
				PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_EXPORT|PSA_KEY_USAGE_COPY);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_HMAC);

	psa_set_key_bits(&key_attributes, 256);

	/* Persistent key specific settings */
#if defined(USE_KMU)
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, PSA_KEY_LOCATION_CRACEN_KMU));
#else
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);
#endif
	psa_set_key_id(&key_attributes, key_id2);

	status = psa_import_key(&key_attributes, kc_HMAC_KEY, 32, &key_id2);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}

	/* Make sure the key is not in memory anymore, has the same affect then resetting the device
	//  */
	// status = psa_purge_key(key_id2);
	// if (status != PSA_SUCCESS) {
	// 	LOG_INF("psa_purge_key failed! (Error: %d)", status);
	// 	return APP_ERROR;
	// }

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x",key_id2);
	return APP_SUCCESS;
}

int generate_persistent_key2(void)
{
	psa_status_t status;

	LOG_INF("Generating random persistent HASH key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes,
				PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_EXPORT|PSA_KEY_USAGE_COPY);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_HMAC);
	psa_set_key_bits(&key_attributes, 256);

	/* Persistent key specific settings */
#if defined(USE_KMU)
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, PSA_KEY_LOCATION_CRACEN_KMU));
#else
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);
#endif
	psa_set_key_id(&key_attributes, key_id3);

	status = psa_import_key(&key_attributes, dk_HMAC_KEY, 32, &key_id3);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}

	/* Make sure the key is not in memory anymore, has the same affect then resetting the device
	//  */
	// status = psa_purge_key(key_id3);
	// if (status != PSA_SUCCESS) {
	// 	LOG_INF("psa_purge_key failed! (Error: %d)", status);
	// 	return APP_ERROR;
	// }

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x",key_id3);
	return APP_SUCCESS;
}

int generate_eddsa_key(void)
{
	psa_status_t status;
	size_t olen;

	LOG_INF("Generating random EDDSA keypair...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_EXPORT) ;
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, PSA_KEY_LOCATION_CRACEN_KMU));
	// psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);
	psa_set_key_id(&key_attributes, key_id4);

	psa_set_key_algorithm(&key_attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));

	psa_set_key_bits(&key_attributes, 255);

	status = psa_import_key(&key_attributes, buf2, 32, &key_id4);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}
	/* Generate a random keypair. The keypair is not exposed to the application,
	 * we can use it to sign hashes.
	 */
	// status = psa_generate_key(&key_attributes, &key_id4);
	// if (status != PSA_SUCCESS) {
	// 	LOG_INF("psa_generate_key failed! (Error: %d)", status);
	// 	return APP_ERROR;
	// }

	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x",key_id3);

	/* Export the public key */
	status = psa_export_public_key(key_id4,
				       m_pub_key, sizeof(m_pub_key),
				       &olen);

	printk("export public key status: %d len %d\n", status, olen);

	/* Configure the public key attributes */
	psa_reset_key_attributes(&key_attributes);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_VERIFY_MESSAGE);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, PSA_KEY_LOCATION_CRACEN_KMU));

	psa_set_key_algorithm(&key_attributes, PSA_ALG_PURE_EDDSA);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_TWISTED_EDWARDS));
	psa_set_key_bits(&key_attributes, 255);
	psa_set_key_id(&key_attributes, key_id5);
	status = psa_import_key(&key_attributes, m_pub_key, 32, &key_id5);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_import_key failed! (Error: %d)", status);
		return APP_ERROR;
	}

	return APP_SUCCESS;
}

int generate_ecdsa_key(void)
{
	psa_status_t status;
	size_t olen;

	LOG_INF("Generating random ECDSA keypair...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_VERIFY_MESSAGE | PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_EXPORT) ;
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, PSA_KEY_LOCATION_CRACEN_KMU));
	// psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);
	psa_set_key_id(&key_attributes, key_id7);

	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));

	psa_set_key_bits(&key_attributes, 256);

	// status = psa_generate_key(&key_attributes, &key_id7);
	// if (status != PSA_SUCCESS) {
	// 	LOG_INF("psa_generate_key failed! (Error: %d)", status);
	// 	return APP_ERROR;
	// }

	status = psa_import_key(&key_attributes, buf2, 32, &key_id7);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}
	/* Generate a random keypair. The keypair is not exposed to the application,
	 * we can use it to sign hashes.
	 */
	LOG_INF("Persistent key generated successfully!");
	LOG_INF("Key id 0x%08x",key_id7);

	// /* Export the public key */
	status = psa_export_public_key(key_id7,
				       m_pub_key, sizeof(m_pub_key),
				       &olen);

	printk("export ECDSA public key status: %d len %d\n", status, olen);

	/* Configure the public key attributes */
	psa_reset_key_attributes(&key_attributes);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_EXPORT);
	// psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, PSA_KEY_LOCATION_CRACEN_KMU));
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);

	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&key_attributes, 256);
	psa_set_key_id(&key_attributes, key_id8);
	status = psa_import_key(&key_attributes, m_pub_key, sizeof(m_pub_key), &key_id8);
	if (status != PSA_SUCCESS) {
		LOG_INF("ECDSA public key psa_import_key failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("ECDSA public key generated successfully!");
	LOG_INF("Key id 0x%08x",key_id8);

	return APP_SUCCESS;
}

int generate_ecdh_key(void)
{
	psa_status_t status;
	size_t olen;

	LOG_INF("Generating random ECDH keypair...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_VERIFY_MESSAGE | PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_EXPORT) ;
	// psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, PSA_KEY_LOCATION_CRACEN_KMU));
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);
	psa_set_key_id(&key_attributes, key_id12);

	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));

	psa_set_key_bits(&key_attributes, 256);

	// status = psa_generate_key(&key_attributes, &key_id12);
	// if (status != PSA_SUCCESS) {
	// 	LOG_INF("psa_generate_key failed! (Error: %d)", status);
	// 	return APP_ERROR;
	// }

	status = psa_import_key(&key_attributes, buf2, 32, &key_id12);
	if (status != PSA_SUCCESS) {
		LOG_INF("import key  (Error: %d)", status);
	}
	/* Generate a random keypair. The keypair is not exposed to the application,
	 * we can use it to sign hashes.
	 */
	LOG_INF("Persistent ECDH key generated successfully!");
	LOG_INF("Key id 0x%08x",key_id12);

	// /* Export the public key */
	status = psa_export_public_key(key_id12,
				       m_pub_key, sizeof(m_pub_key),
				       &olen);

	printk("export ECDH public key status: %d len %d\n", status, olen);

	/* Configure the public key attributes */
	psa_reset_key_attributes(&key_attributes);

	/* Configure the key attributes */
	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_EXPORT);
	// psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, PSA_KEY_LOCATION_CRACEN_KMU));
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_PERSISTENT);

	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&key_attributes, 256);
	psa_set_key_id(&key_attributes, key_id13);
	status = psa_import_key(&key_attributes, pubkey, sizeof(pubkey), &key_id13);
	if (status != PSA_SUCCESS) {
		LOG_INF("ECDH public key psa_import_key failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("ECDH public key generated successfully!");
	LOG_INF("Key id 0x%08x",key_id13);

	return APP_SUCCESS;
}

int use_persistent_key(void)
{
	uint32_t olen;
	psa_status_t status;
#if defined(USE_KMU)
	status = psa_cipher_encrypt(KEY_HANDLE1, PSA_ALG_GCM, m_plain_text,
#else
	status = psa_cipher_encrypt(key_id1, SAMPLE_ALG, m_plain_text,
#endif
				    sizeof(m_plain_text), m_encrypted_text,
				    sizeof(m_encrypted_text), &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_encrypt failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Encryption successful!");
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("Encrypted text", m_encrypted_text, sizeof(m_encrypted_text));

#if defined(USE_KMU)
	status = psa_cipher_decrypt(KEY_HANDLE1, PSA_ALG_GCM, m_encrypted_text,
#else
	status = psa_cipher_decrypt(key_id1, SAMPLE_ALG, m_encrypted_text,
#endif
				    sizeof(m_encrypted_text), m_decrypted_text,
				    sizeof(m_decrypted_text), &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_decrypt failed! (Error: %d)", status);
		return APP_ERROR;
	}

	PRINT_HEX("Decrypted text", m_decrypted_text, sizeof(m_decrypted_text));

	/* Check the validity of the decryption */
	if (memcmp(m_decrypted_text, m_plain_text,
		   NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE) != 0) {
		LOG_INF("Error: Decrypted text doesn't match the plaintext");
		return APP_ERROR;
	}

	LOG_INF("Decryption successful!");

	return APP_SUCCESS;
}

int use_persistent_gcm_key(void)
{
	uint32_t olen;
	psa_status_t status;

	LOG_INF("Encrypting using AES GCM MODE...");

	/* Generate a random IV */
	status = psa_generate_random(m_iv, NRF_CRYPTO_EXAMPLE_AES_IV_SIZE);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_generate_random failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Encrypt the plaintext and create the authentication tag */
#if defined(USE_KMU)
	status = psa_aead_encrypt(KEY_HANDLE1,
#else
	status = psa_aead_encrypt(key_id1,
#endif
				  PSA_ALG_GCM,
				  m_iv,
				  sizeof(m_iv),
				  m_additional_data,
				  sizeof(m_additional_data),
				  m_plain_text,
				  sizeof(m_plain_text),
				  m_encrypted_text,
				  sizeof(m_encrypted_text),
				  &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_aead_encrypt failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Encryption successful!");
	PRINT_HEX("IV", m_iv, sizeof(m_iv));
	PRINT_HEX("Additional data", m_additional_data, sizeof(m_additional_data));
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("Encrypted text", m_encrypted_text, sizeof(m_encrypted_text));

	LOG_INF("Decrypting using AES GCM MODE...");

	/* Decrypt and authenticate the encrypted data */
#if defined(USE_KMU)
	status = psa_aead_decrypt(KEY_HANDLE1,
#else
	status = psa_aead_decrypt(key_id1,
#endif
				  PSA_ALG_GCM,
				  m_iv,
				  sizeof(m_iv),
				  m_additional_data,
				  sizeof(m_additional_data),
				  m_encrypted_text,
				  sizeof(m_encrypted_text),
				  m_decrypted_text,
				  sizeof(m_decrypted_text),
				  &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_aead_decrypt failed! (Error: %d)", status);
		return APP_ERROR;
	}

	PRINT_HEX("Decrypted text", m_decrypted_text, sizeof(m_decrypted_text));

	/* Check the validity of the decryption */
	if (memcmp(m_decrypted_text, m_plain_text, NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE) != 0) {
		LOG_INF("Error: Decrypted text doesn't match the plaintext");
		return APP_ERROR;
	}

	LOG_INF("Decryption and authentication successful!");

	return APP_SUCCESS;
}

int use_persistent_ctr_key(void)
{
	uint32_t olen;
	psa_status_t status;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

	LOG_INF("Encrypting using AES CTR MODE...");

	/* Setup the encryption operation */
	status = psa_cipher_encrypt_setup(&operation, key_id6, PSA_ALG_CTR);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_encrypt_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Generate a random IV */
	status = psa_cipher_generate_iv(&operation, m_iv, sizeof(m_iv),
					&olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_generate_iv failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Perform the encryption */
	status = psa_cipher_update(&operation,
							   m_plain_text,
							   sizeof(m_plain_text),
							   m_encrypted_text,
							   sizeof(m_encrypted_text),
							   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize encryption */
	status = psa_cipher_finish(&operation,
							   m_encrypted_text + olen,
							   sizeof(m_encrypted_text) - olen,
							   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Clean up cipher operation context */
	status = psa_cipher_abort(&operation);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_abort failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Encryption successful!\r\n");
	PRINT_HEX("IV", m_iv, sizeof(m_iv));
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("Encrypted text", m_encrypted_text, sizeof(m_encrypted_text));

	LOG_INF("Decrypting using AES CTR MODE...");

	/* Setup the decryption operation */
	status = psa_cipher_decrypt_setup(&operation, key_id6, PSA_ALG_CTR);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_decrypt_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Set the IV to the one generated during encryption */
	status = psa_cipher_set_iv(&operation, m_iv, sizeof(m_iv));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_set_iv failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Perform the decryption */
	status = psa_cipher_update(&operation,
				   m_encrypted_text,
				   sizeof(m_encrypted_text),
				   m_decrypted_text,
				   sizeof(m_decrypted_text), &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize the decryption */
	status = psa_cipher_finish(&operation,
				   m_decrypted_text + olen,
				   sizeof(m_decrypted_text) - olen,
				   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	PRINT_HEX("Decrypted text", m_decrypted_text, sizeof(m_decrypted_text));

	/* Check the validity of the decryption */
	if (memcmp(m_decrypted_text, m_plain_text, 128) != 0) {
		LOG_INF("Error: Decrypted text doesn't match the plaintext");
		return APP_ERROR;
	}

	/* Clean up cipher operation context */
	status = psa_cipher_abort(&operation);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_abort failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Decryption successful!");

	return APP_SUCCESS;
}

int use_persistent_ecb_key(void)
{
	uint32_t olen;
	psa_status_t status;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

	LOG_INF("Encrypting using AES ECB MODE...");

	status = psa_cipher_encrypt_setup(&operation, key_id9, PSA_ALG_ECB_NO_PADDING);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_encrypt failed! (Error: %d)", status);
		return APP_ERROR;
	}

	status = psa_cipher_update(&operation,
				   m_plain_text,
				   sizeof(m_plain_text),
				   m_encrypted_text,
				   sizeof(m_encrypted_text),
				   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize encryption */
	status = psa_cipher_finish(&operation,
							   m_encrypted_text + olen,
							   sizeof(m_encrypted_text) - olen,
							   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Clean up cipher operation context */
	status = psa_cipher_abort(&operation);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_abort failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Encryption successful!");
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("Encrypted text", m_encrypted_text, sizeof(m_encrypted_text));


	/* Setup the decryption operation */
	status = psa_cipher_decrypt_setup(&operation, key_id9, PSA_ALG_ECB_NO_PADDING);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_decrypt_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Perform the decryption */
	status = psa_cipher_update(&operation,
				   m_encrypted_text,
				   sizeof(m_encrypted_text),
				   m_decrypted_text,
				   sizeof(m_decrypted_text), &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize the decryption */
	status = psa_cipher_finish(&operation,
				   m_decrypted_text + olen,
				   sizeof(m_decrypted_text) - olen,
				   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	PRINT_HEX("Decrypted text", m_decrypted_text, sizeof(m_decrypted_text));

	/* Check the validity of the decryption */
	if (memcmp(m_decrypted_text, m_plain_text, NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE) != 0) {
		LOG_INF("Error: Decrypted text doesn't match the plaintext");
		return APP_ERROR;
	}

	/* Clean up cipher operation context */
	status = psa_cipher_abort(&operation);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_abort failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Decryption successful!");

	return APP_SUCCESS;
}

int use_persistent_cbc_key(void)
{
	uint32_t olen;
	psa_status_t status;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

	LOG_INF("Encrypting using AES CBC MODE...");

	status = psa_cipher_encrypt_setup(&operation, key_id10, PSA_ALG_CBC_NO_PADDING);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_encrypt_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Generate a random IV */
	status = psa_cipher_generate_iv(&operation, m_iv, sizeof(m_iv),
					&olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_generate_iv failed! (Error: %d)", status);
		return APP_ERROR;
	}

	status = psa_cipher_update(&operation,
				   m_plain_text,
				   sizeof(m_plain_text),
				   m_encrypted_text,
				   sizeof(m_encrypted_text),
				   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize encryption */
	status = psa_cipher_finish(&operation,
				   m_encrypted_text + olen,
				   sizeof(m_encrypted_text) - olen,
				   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Clean up cipher operation context */
	status = psa_cipher_abort(&operation);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_abort failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Encryption successful!");
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("Encrypted text", m_encrypted_text, sizeof(m_encrypted_text));


	/* Setup the decryption operation */
	status = psa_cipher_decrypt_setup(&operation, key_id10, PSA_ALG_CBC_NO_PADDING);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_decrypt_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Set the IV to the one generated during encryption */
	status = psa_cipher_set_iv(&operation, m_iv, sizeof(m_iv));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_set_iv failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Perform the decryption */
	status = psa_cipher_update(&operation,
				   m_encrypted_text,
				   sizeof(m_encrypted_text),
				   m_decrypted_text,
				   sizeof(m_decrypted_text), &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_update decrypt failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize the decryption */
	status = psa_cipher_finish(&operation,
				   m_decrypted_text + olen,
				   sizeof(m_decrypted_text) - olen,
				   &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	PRINT_HEX("Decrypted text", m_decrypted_text, sizeof(m_decrypted_text));

	/* Check the validity of the decryption */
	if (memcmp(m_decrypted_text, m_plain_text, NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE) != 0) {
		LOG_INF("Error: Decrypted text doesn't match the plaintext");
		return APP_ERROR;
	}

	/* Clean up cipher operation context */
	status = psa_cipher_abort(&operation);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_abort failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Decryption successful!");

	return APP_SUCCESS;
}

int use_persistent_ccm_key(void)
{
	uint32_t olen;
	psa_status_t status;
	static uint8_t m_encrypted[NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE +
				   NRF_CRYPTO_EXAMPLE_AES_CCM_TAG_LENGTH];

	static uint8_t m_decrypted[NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE];

	LOG_INF("Encrypting using AES CCM MODE...");

	/* Generate a random IV */
	status = psa_generate_random(m_iv, NRF_CRYPTO_EXAMPLE_AES_IV_SIZE);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_generate_random failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Encrypt the plaintext and create the authentication tag */
	status = psa_aead_encrypt(key_id11,
				  PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, NRF_CRYPTO_EXAMPLE_AES_CCM_TAG_LENGTH),
				  m_iv,
				  13,
				  m_additional_data,
				  sizeof(m_additional_data),
				  m_plain_text,
				  sizeof(m_plain_text),
				  m_encrypted,
				  sizeof(m_encrypted),
				  &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_aead_encrypt failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Encryption successful!");
	PRINT_HEX("IV", m_iv, sizeof(m_iv));
	PRINT_HEX("Additional data", m_additional_data, sizeof(m_additional_data));
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("Encrypted text", m_encrypted, sizeof(m_encrypted));

	LOG_INF("Decrypting using AES CCM MODE...");

	/* Decrypt and authenticate the encrypted data */
	status = psa_aead_decrypt(key_id11,
				  PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, NRF_CRYPTO_EXAMPLE_AES_CCM_TAG_LENGTH),
				  m_iv,
				  13,
				  m_additional_data,
				  sizeof(m_additional_data),
				  m_encrypted,
				  sizeof(m_encrypted),
				  m_decrypted,
				  sizeof(m_decrypted),
				  &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_aead_decrypt failed! (Error: %d)", status);
		return APP_ERROR;
	}

	PRINT_HEX("Decrypted text", m_decrypted, sizeof(m_decrypted));

	/* Check the validity of the decryption */
	if (memcmp(m_decrypted, m_plain_text, NRF_CRYPTO_EXAMPLE_PERSISTENT_KEY_MAX_TEXT_SIZE) != 0) {
		LOG_INF("Error: Decrypted text doesn't match the plaintext");
		return APP_ERROR;
	}

	LOG_INF("Decryption and authentication successful!");

	return APP_SUCCESS;
}

int hmac_sign(void)
{
	uint32_t olen;
	psa_status_t status;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	LOG_INF("Signing using EC HMAC ...");

	/* Initialize the HMAC signing operation */
#if defined(USE_KMU)
	status = psa_mac_sign_setup(&operation, KEY_HANDLE2, PSA_ALG_HMAC(PSA_ALG_SHA_256));
#else
	status = psa_mac_sign_setup(&operation, key_id2, PSA_ALG_HMAC(PSA_ALG_SHA_256));
#endif
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_sign_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Perform the HMAC signing */
	status = psa_mac_update(&operation, m_plain_text, sizeof(m_plain_text));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize the HMAC signing */
	status = psa_mac_sign_finish(&operation, hmac, sizeof(hmac), &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_sign_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Signing successful!");
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("HMAC", hmac, sizeof(hmac));

	return APP_SUCCESS;
}

int hmac_verify(void)
{
	psa_status_t status;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	LOG_INF("Verifying the HMAC signature...");

	/* Initialize the HMAC verification operation */
#if defined(USE_KMU)
	status = psa_mac_verify_setup(&operation, KEY_HANDLE2, PSA_ALG_HMAC(PSA_ALG_SHA_256));
#else
	status = psa_mac_verify_setup(&operation, key_id2, PSA_ALG_HMAC(PSA_ALG_SHA_256));
#endif
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_verify_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Perform the HMAC verification */
	status = psa_mac_update(&operation, m_plain_text, sizeof(m_plain_text));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize the HMAC verification */
	status = psa_mac_verify_finish(&operation, hmac, sizeof(hmac));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_verify_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("HMAC verified successfully!");

	return APP_SUCCESS;
}

int hmac_sign1(void)
{
	uint32_t olen;
	psa_status_t status;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	LOG_INF("Signing using HMAC ...");

	/* Initialize the HMAC signing operation */
#if defined(USE_KMU)
	status = psa_mac_sign_setup(&operation, KEY_HANDLE3, PSA_ALG_HMAC(PSA_ALG_SHA_256));
#else
	status = psa_mac_sign_setup(&operation, key_id3, PSA_ALG_HMAC(PSA_ALG_SHA_256));
#endif
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_sign_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Perform the HMAC signing */
	status = psa_mac_update(&operation, m_plain_text, sizeof(m_plain_text));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize the HMAC signing */
	status = psa_mac_sign_finish(&operation, hmac, sizeof(hmac), &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_sign_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Signing successful!");
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("HMAC", hmac, sizeof(hmac));

	return APP_SUCCESS;
}

int hmac_verify1(void)
{
	psa_status_t status;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	LOG_INF("Verifying the DK HMAC signature...");

	/* Initialize the HMAC verification operation */
#if defined(USE_KMU)
	status = psa_mac_verify_setup(&operation, KEY_HANDLE3, PSA_ALG_HMAC(PSA_ALG_SHA_256));
#else
	status = psa_mac_verify_setup(&operation, key_id3, PSA_ALG_HMAC(PSA_ALG_SHA_256));
#endif
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_verify_setup failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Perform the HMAC verification */
	status = psa_mac_update(&operation, m_plain_text, sizeof(m_plain_text));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_update failed! (Error: %d)", status);
		return APP_ERROR;
	}

	/* Finalize the HMAC verification */
	status = psa_mac_verify_finish(&operation, hmac, sizeof(hmac));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_mac_verify_finish failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("HMAC verified successfully!");

	return APP_SUCCESS;
}

int hmac_sign_edd(void)
{
	uint32_t olen;
	psa_status_t status;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	LOG_INF("Signing using HMAC ...");

	/* Initialize the HMAC signing operation */
#if defined(USE_KMU)
	status = psa_sign_message(KEY_HANDLE4,
				  PSA_ALG_PURE_EDDSA,
				  m_plain_text,
				  sizeof(m_plain_text),
				  m_signature,
				  sizeof(m_signature),
				  &m_signature_len);

#else
	status = psa_mac_sign_setup(&operation, key_id3, PSA_ALG_PURE_EDDSA);
#endif
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_sign_message failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Message signed successfully!");
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("Signature", m_signature, sizeof(m_signature));

	return APP_SUCCESS;


	return APP_SUCCESS;
}

int hmac_verify_edd(void)
{
	psa_status_t status;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	LOG_INF("Verifying the HMAC signature...");

	/* Initialize the HMAC verification operation */
#if defined(USE_KMU)
	LOG_INF("Verifying EDDSA signature...");

	/* Verify the signature of the message */
	status = psa_verify_message(key_id5,
				    PSA_ALG_PURE_EDDSA,
				    m_plain_text,
				    sizeof(m_plain_text),
				    m_signature,
				    m_signature_len);
#else
	status = psa_mac_verify_setup(&operation, key_id5, PSA_ALG_PURE_EDDSA);
#endif
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_verify_message failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Signature verification was successful!");

	return APP_SUCCESS;

}

int sign_message_ecd(void)
{
	uint32_t output_len;
	psa_status_t status;

	LOG_INF("Signing a message using ECDSA...");

	// /* Compute the SHA256 hash*/
	// status = psa_hash_compute(PSA_ALG_SHA_256,
	// 			  m_plain_text,
	// 			  sizeof(m_plain_text),
	// 			  m_hash,
	// 			  sizeof(m_hash),
	// 			  &output_len);
	// if (status != PSA_SUCCESS) {
	// 	LOG_INF("psa_hash_compute failed! (Error: %d)", status);
	// 	return APP_ERROR;
	// }

	// /* Sign the hash */
	// status = psa_sign_hash(KEY_HANDLE7,
	// 		       PSA_ALG_ECDSA(PSA_ALG_SHA_256),
	// 		       m_hash,
	// 		       sizeof(m_hash),
	// 		       m_signature,
	// 		       sizeof(m_signature),
	// 		       &output_len);

	status = psa_sign_message(KEY_HANDLE7,
				  PSA_ALG_ECDSA(PSA_ALG_SHA_256),
				  m_plain_text,
				  sizeof(m_plain_text),
				  m_signature,
				  sizeof(m_signature),
				  &m_signature_len);

	if (status != PSA_SUCCESS) {
		LOG_INF("psa_sign_hash failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Message signed successfully!");
	PRINT_HEX("Plaintext", m_plain_text, sizeof(m_plain_text));
	PRINT_HEX("Signature", m_signature, sizeof(m_signature));

	return APP_SUCCESS;
}

int verify_message_ecd(void)
{
	psa_status_t status;

	LOG_INF("Verifying ECDSA signature...");

	/* Verify the signature of the hash */
	status = psa_verify_message(KEY_HANDLE8,
				    PSA_ALG_ECDSA(PSA_ALG_SHA_256),
				    m_plain_text,
				    sizeof(m_plain_text),
				    m_signature,
				    m_signature_len);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_verify_message failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("Signature verification was successful!");

	return APP_SUCCESS;
}

int dump_key(void)
{
	psa_key_lifetime_t lifetime;
	psa_drv_slot_number_t slot_number;
	uint8_t str[65];
	psa_status_t status;
	uint32_t olen;

#if defined(USE_KMU)
 	psa_key_id_t target_key;
	psa_key_attributes_t attributes;
	for(size_t i = 0; i < KEY_ID_COUNT; i++)
	{
		// status = cracen_kmu_get_key_slot(key_ids[i], &lifetime, &slot_number);
		status = mbedtls_psa_platform_get_builtin_key(key_ids[i], &lifetime, &slot_number);
		LOG_INF("\ncracen_kmu_get_key_slot KEY_HANDLE %d status: %d lifetime 0x%08x slot_number %lld", (i+1), status, lifetime, slot_number);
		status = psa_export_key(key_ids[i], str, 65, &olen);
		LOG_INF("psa_export_key status: %d olen %d", status, olen);
		LOG_HEXDUMP_INF(str, olen, "Exported key");
	}
#else
	status = psa_export_key(key_id1, str, 32, &olen);
	LOG_INF("psa_export_key status: %d olen %d\n", status, olen);
	LOG_HEXDUMP_INF(str, 32, "Exported key");

	status = psa_export_key(key_id2, str, 32, &olen);
	LOG_INF("psa_export_key status: %d olen %d\n", status, olen);
	LOG_HEXDUMP_INF(str, 32, "Exported key");

	status = psa_export_key(key_id3, str, 32, &olen);
	LOG_INF("psa_export_key status: %d olen %d\n", status, olen);
	LOG_HEXDUMP_INF(str, 32, "Exported key");

	status = psa_export_key(key_id4, str, 32, &olen);
	LOG_INF("psa_export_key status: %d olen %d\n", status, olen);
	LOG_HEXDUMP_INF(str, 32, "Exported key");
#endif

	return 0;
}

void button_changed(uint32_t button_state, uint32_t has_changed)
{
		int status;
	uint32_t buttons = button_state & has_changed;

	if (buttons & DK_BTN1_MSK) {
		LOG_INF("generate key and store at KMU");
		status = generate_persistent_gcm_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}
		status = generate_persistent_key1();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		status = generate_persistent_key2();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		status = generate_eddsa_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		status = generate_aes_ctr_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		status = generate_ecdsa_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		status = generate_aes_ecb_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		status = generate_aes_cbc_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		status = generate_aes_ccm_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		status = generate_ecdh_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}
	}

	if (buttons & DK_BTN2_MSK) {
		LOG_INF("do encryption and hash test");

		// status = use_persistent_key();
		// status = use_persistent_gcm_key();
		status = use_persistent_ecb_key();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		LOG_INF(APP_SUCCESS_MESSAGE);
		k_sleep(K_MSEC(100));
		status = hmac_sign();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		k_sleep(K_MSEC(100));
		status = hmac_verify();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		LOG_INF(APP_SUCCESS_MESSAGE);
		k_sleep(K_MSEC(100));
		status = hmac_sign1();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		k_sleep(K_MSEC(100));
		status = hmac_verify1();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		LOG_INF(APP_SUCCESS_MESSAGE);
		k_sleep(K_MSEC(100));
		status = hmac_sign_edd();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		k_sleep(K_MSEC(100));
		status = hmac_verify_edd();
		if (status != APP_SUCCESS) {
			LOG_INF(APP_ERROR_MESSAGE);
		}

		LOG_INF(APP_SUCCESS_MESSAGE);
	}

	if (buttons & DK_BTN3_MSK) {
		LOG_INF("Dump key");
		status = dump_key();
	}

	if (buttons & DK_BTN4_MSK) {
		status = use_persistent_ccm_key();
		// status = use_persistent_cbc_key();
		// status = use_persistent_ecb_key();
		// status = sign_message_ecd();
		// if (status != APP_SUCCESS) {
		// 	LOG_INF(APP_ERROR_MESSAGE);
		// }

		// status = verify_message_ecd();
		// if (status != APP_SUCCESS) {
		// 	LOG_INF(APP_ERROR_MESSAGE);
		// }

	}
}

void configure_buttons(void)
{
	int err;

	err = dk_buttons_init(button_changed);
	if (err) {
		LOG_INF("Cannot init buttons (err: %d)\n", err);
	}
}

int main(void)
{
	int status;

	configure_buttons();

	LOG_INF("Starting persistent key example...");

	status = crypto_init();
	if (status != APP_SUCCESS) {
		LOG_INF(APP_ERROR_MESSAGE);
		return APP_ERROR;
	}
}
