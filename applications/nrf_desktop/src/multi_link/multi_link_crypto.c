#include "multi_link.h"
#include "multi_link_crypto.h"

#ifdef __ZEPHYR__

#define MODULE multi_link_crypto
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);
#else
#define LOG_DBG(...)
#define LOG_ERR(...)
#define LOG_WRN(...)
#define LOG_HEXDUMP_DBG(...)
#define LOG_HEXDUMP_INF(...)
#endif

//
// This is must implimeted depend on SDK used. For example it uses Zephyr's based crypto
//
bool multi_link_generate_random(uint8_t *result, size_t result_length)
{
	bool retval = false;
	psa_status_t status;

	status = psa_generate_random(result, result_length);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_generate_random failed! (Error: %d)", status);
	} else {
		retval = true;
	}

	return retval;
}

//
// This is must implimeted depend on SDK used. For example it uses Zephyr's based crypto
//
bool multi_link_hmac_sha_256(const uint8_t (*key)[32], const uint8_t *data,
			     const size_t data_length, uint8_t (*result)[32])
{
	psa_status_t status;
	uint32_t olen;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;
	psa_key_handle_t key_handle;

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes,
				PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_SIGN_HASH);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_HMAC);
	psa_set_key_bits(&key_attributes, (sizeof(key[0]) * CHAR_BIT));

	/* Import a key. The key is not exposed to the application,
	 * we can use it to encrypt/decrypt using the key handle
	 */
	status = psa_import_key(&key_attributes, key[0], sizeof(key[0]), &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_ERR("hmac_sha_256 psa_import_key failed! (Error: %d)", status);
		return false;
	}

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	/* Initialize the HMAC signing operation */
	status = psa_mac_sign_setup(&operation, key_handle, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_mac_sign_setup failed! (Error: %d)", status);
		return false;
	}

	/* Perform the HMAC signing */
	status = psa_mac_update(&operation, data, data_length);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_mac_update failed! (Error: %d)", status);
		return false;
	}

	/* Finalize the HMAC signing */
	status = psa_mac_sign_finish(&operation, result[0], sizeof(result[0]), &olen);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_mac_sign_finish failed! (Error: %d)", status);
		return false;
	}

	psa_destroy_key(key_handle);

	return true;
}

bool multi_link_ecb_128(const uint8_t (*key)[16], const uint8_t *data, const size_t data_length,
			uint8_t *result, size_t result_length)
{
	psa_status_t status;
	uint32_t olen;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
	psa_key_handle_t key_handle;

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECB_NO_PADDING);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, (sizeof(key[0]) * CHAR_BIT));

	/* Import a key. The key is not exposed to the application,
	 * we can use it to encrypt/decrypt using the key handle
	 */
	status = psa_import_key(&key_attributes, key[0], sizeof(key[0]), &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_ERR("ecb_128 psa_import_key failed! (Error: %d)", status);
		return false;
	}

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	/* Setup the encryption operation */
	status = psa_cipher_encrypt_setup(&operation, key_handle, PSA_ALG_ECB_NO_PADDING);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_encrypt_setup failed! (Error: %d)", status);
		return false;
	}

	/* Perform the encryption */
	status = psa_cipher_update(&operation, data, data_length, result, result_length, &olen);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_update failed! (Error: %d)", status);
		return false;
	}

	/* Finalize the encryption */
	status = psa_cipher_finish(&operation, result + olen, result_length - olen, &olen);

	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_finish failed! (Error: %d)", status);
		return false;
	}

	psa_destroy_key(key_handle);

	return true;
}

bool multi_link_ecb_aes_128_decrypt(const uint8_t (*key)[16], const uint8_t *data,
				    const size_t data_length, uint8_t *result, size_t result_length)
{
	psa_status_t status;
	uint32_t olen;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
	psa_key_handle_t key_handle;

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECB_NO_PADDING);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, (sizeof(key[0]) * CHAR_BIT));

	/* Import a key. The key is not exposed to the application,
	 * we can use it to encrypt/decrypt using the key handle
	 */
	status = psa_import_key(&key_attributes, key[0], sizeof(key[0]), &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_ERR("ecb_128 psa_import_key failed! (Error: %d)", status);
		return false;
	}

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	/* Perform the decryption */
	status = psa_cipher_decrypt_setup(&operation, key_handle, PSA_ALG_ECB_NO_PADDING);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_decrypt_setup failed! (Error: %d)", status);
		return false;
	}

	/* Perform the decryption */
	status = psa_cipher_update(&operation, data, data_length, result, result_length, &olen);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_update failed! (Error: %d)", status);
		return false;
	}

	/* Finalize the decryption */
	status = psa_cipher_finish(&operation, result + olen, result_length - olen, &olen);

	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_finish failed! (Error: %d)", status);
		return false;
	}

	psa_destroy_key(key_handle);

	return true;
}

bool multi_link_cbc_aes_256_encrypt(const uint8_t (*key)[32], const uint8_t *data,
				    const size_t data_length, uint8_t *iv, size_t iv_length,
				    uint8_t *result, size_t result_length)
{
	psa_status_t status;
	uint32_t olen;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
	psa_key_handle_t key_handle;

	LOG_INF("Generating random AES key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_CBC_NO_PADDING);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, (sizeof(key[0]) * CHAR_BIT));

	/* Generate a random key. The key is not exposed to the application,
	 * we can use it to encrypt/decrypt using the key handle
	 */
	status = psa_import_key(&key_attributes, key[0], sizeof(key[0]), &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_import_key failed! (Error: %d)", status);
		return false;
	}

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	LOG_INF("AES key generated successfully!");

	/* Setup the encryption operation */
	status = psa_cipher_encrypt_setup(&operation, key_handle, PSA_ALG_CBC_NO_PADDING);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_encrypt_setup failed! (Error: %d)", status);
		return false;
	}

	/* Generate an IV */
	status = psa_cipher_set_iv(&operation, iv, iv_length);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_set_iv failed! (Error: %d)", status);
		return false;
	}

	/* Perform the encryption */
	status = psa_cipher_update(&operation, data, data_length, result, result_length, &olen);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_update failed! (Error: %d)", status);
		return false;
	}

	/* Finalize the encryption */
	status = psa_cipher_finish(&operation, result + olen, result_length - olen, &olen);

	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_finish failed! (Error: %d)", status);
		return false;
	}

	psa_destroy_key(key_handle);

	return true;
}
#if 0
bool multi_link_cbc_aes_256_decrypt(const uint8_t (*key)[32], const uint8_t *data,
				    const size_t data_length, const uint8_t *iv, size_t iv_length,
				    uint8_t *result, size_t result_length)
{
	psa_status_t status;
	uint32_t olen;
	psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
	psa_key_handle_t key_handle;

	LOG_INF("Generating random AES key...");

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_CBC_NO_PADDING);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, (sizeof(key[0]) * CHAR_BIT));

	/* Generate a random key. The key is not exposed to the application,
	 * we can use it to encrypt/decrypt using the key handle
	 */
	status = psa_import_key(&key_attributes, key[0], sizeof(key[0]), &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_import_key failed! (Error: %d)", status);
		return false;
	}

	/* After the key handle is acquired the attributes are not needed */
	psa_reset_key_attributes(&key_attributes);

	LOG_INF("AES key generated successfully!");

	/* Setup the encryption operation */
	status = psa_cipher_decrypt_setup(&operation, key_handle, PSA_ALG_CBC_NO_PADDING);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_decrypt_setup failed! (Error: %d)", status);
		return false;
	}

	/* Generate an IV */
	status = psa_cipher_set_iv(&operation, iv, iv_length);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_cipher_set_iv failed! (Error: %d)", status);
		return false;
	}

	/* Perform the encryption */
	status = psa_cipher_update(&operation, data, data_length, result, result_length, &olen);
	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_update failed! (Error: %d)", status);
		return false;
	}

	/* Finalize the encryption */
	status = psa_cipher_finish(&operation, result + olen, result_length - olen, &olen);

	if (status != PSA_SUCCESS) {
		psa_destroy_key(key_handle);
		LOG_ERR("psa_cipher_finish failed! (Error: %d)", status);
		return false;
	}

	psa_destroy_key(key_handle);

	return true;
}
#endif

bool multi_link_sha256(uint8_t *data, size_t data_len, uint8_t *hash, size_t hash_len)
{
	uint32_t olen;
	psa_status_t status;
	LOG_INF("Hashing using SHA256...");

	/* Calculate the SHA256 hash */
	status = psa_hash_compute(PSA_ALG_SHA_256, data, data_len, hash, hash_len, &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_hash_compute failed! (Error: %d)", status);
		return false;
	}

	LOG_INF("Hashing successful!");

	return true;
}

bool multi_link_pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
									const uint8_t *salt, size_t salt_len,
									uint8_t *key, size_t key_len, 
									uint32_t count)
{
	psa_status_t status;
	psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;
	static psa_key_id_t m_input_key_id;

	psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&attributes, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
	psa_set_key_type(&attributes, PSA_KEY_TYPE_PASSWORD);
	psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(password_len));

	status = psa_import_key(&attributes, password, password_len, &m_input_key_id);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_import_key failed! (Error: %d)", status);
		return false;
	}

	status = psa_key_derivation_setup(&operation, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_key_derivation_setup failed! (Error: %d)", status);
		psa_destroy_key(m_input_key_id);
		return false;
	}

	status = psa_key_derivation_input_integer(&operation, PSA_KEY_DERIVATION_INPUT_COST, count);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_key_derivation_input_integer failed! (Error: %d)", status);
		psa_destroy_key(m_input_key_id);
		return false;
	}

	status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT, salt,
						salt_len);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_key_derivation_input_bytes failed! (Error: %d)", status);
		psa_destroy_key(m_input_key_id);
		return false;
	}

	status = psa_key_derivation_input_key(&operation, PSA_KEY_DERIVATION_INPUT_PASSWORD,
					      m_input_key_id);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_key_derivation_input_key failed! (Error: %d)", status);
		psa_destroy_key(m_input_key_id);
		return false;
	}

	/* This outputs the derived key as bytes to the application.
	 * If the derived key is to be used for in cryptographic operations use
	 * psa_key_derivation_output_key instead.
	 */
	status = psa_key_derivation_output_bytes(&operation, key, key_len);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_key_derivation_output_bytes failed! (Error: %d)", status);
		psa_destroy_key(m_input_key_id);
		return false;
	}

	psa_destroy_key(m_input_key_id);
	return true;
}
