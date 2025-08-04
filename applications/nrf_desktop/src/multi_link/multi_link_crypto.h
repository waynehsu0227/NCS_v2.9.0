#ifndef _MULTI_LINK_CRYPTO_H_
#define _MULTI_LINK_CRYPTO_H_

bool multi_link_generate_random(uint8_t *result, size_t result_length);
bool multi_link_hmac_sha_256(const uint8_t (*key)[32], const uint8_t *data, const size_t data_length, uint8_t (*result)[32]);
bool multi_link_ecb_128(const uint8_t (*key)[16], const uint8_t *data, const size_t data_length, uint8_t *result, size_t result_length);
bool multi_link_ecb_aes_128_decrypt(const uint8_t (*key)[16], const uint8_t *data, const size_t data_length, uint8_t *result, size_t result_length);
bool multi_link_cbc_aes_256_encrypt(const uint8_t (*key)[32], const uint8_t *data, const size_t data_length,
									uint8_t *iv, size_t iv_length,
									uint8_t *result, size_t result_length);
#if 0
bool multi_link_cbc_aes_256_decrypt(const uint8_t (*key)[32], const uint8_t *data, const size_t data_length,
									const uint8_t *iv, size_t iv_length,
									uint8_t *result, size_t result_length);
#endif
bool multi_link_sha256(uint8_t *data, size_t data_len, uint8_t *hash, size_t hash_len);
bool multi_link_pbkdf2_hmac_sha256(const uint8_t *password, size_t password_len,
									const uint8_t *salt, size_t salt_len,
									uint8_t *key, size_t key_len, 
									uint32_t count);
#endif /*_MULTI_LINK_CRYPTO_H_*/