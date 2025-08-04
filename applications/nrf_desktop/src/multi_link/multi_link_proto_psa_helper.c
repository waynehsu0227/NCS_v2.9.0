#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
#include <psa/crypto.h>
#include <psa/crypto_extra.h>
#endif

#include <zephyr/kernel.h>

#include <psa/crypto.h>
#include <psa/crypto_extra.h>
#ifdef NRF54L_SERIAL
#include <cracen_psa_kmu.h>
#endif
#include "multi_link_proto_common.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(psa_helper);

#ifdef PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT
#define PSA_KEY_HANDLE_KMU(n)           PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT(CRACEN_KMU_KEY_USAGE_SCHEME_PROTECTED, n) // USE KMU
#endif
#define PSA_KEY_ID_KMU_MIN              (3) //KMU_START
#define PSA_KEY_ID_TRUSTED_STORAGE_MIN  PSA_KEY_ID_USER_MIN
#define PSA_KEY_HANDLE(n)               (n)

#ifdef PSA_KEY_HANDLE_FROM_CRACEN_KMU_SLOT

#define CK_KEY_ID_MULTI_LINK_KC_AES128_ECB		PSA_KEY_HANDLE_KMU(PSA_KEY_ID_KMU_MIN)
//size 16 1 slot
#define CK_KEY_ID_MULTI_LINK_KC_KEY 			PSA_KEY_HANDLE(PSA_KEY_ID_TRUSTED_STORAGE_MIN+1)
//size 16 1 slot
#define CK_KEY_ID_MULTI_LINK_KC_HMAC			PSA_KEY_HANDLE_KMU(CRACEN_KMU_KEY_USAGE_SCHEME_ENCRYPTED, PSA_KEY_ID_KMU_MIN+1)
//size 32 2 slot
#define CK_KEY_ID_MULTI_LINK_DK_HMAC			PSA_KEY_HANDLE_KMU(CRACEN_KMU_KEY_USAGE_SCHEME_ENCRYPTED, PSA_KEY_ID_KMU_MIN+3)
//size 32 2 slot
#define CK_KEY_ID_MULTI_LINK_DK_AES128_ECB      PSA_KEY_HANDLE_KMU(PSA_KEY_ID_KMU_MIN+4)
//size 16 1 slot

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)

#define CK_KEY_ID_MULTI_LINK_KC_HMAC_V2			PSA_KEY_HANDLE_KMU(CRACEN_KMU_KEY_USAGE_SCHEME_ENCRYPTED, PSA_KEY_ID_KMU_MIN+5)
//size 32 2 slot
#define CK_KEY_ID_MULTI_LINK_DK_HMAC_V2			PSA_KEY_HANDLE_KMU(CRACEN_KMU_KEY_USAGE_SCHEME_ENCRYPTED, PSA_KEY_ID_KMU_MIN+7)
//size 32 2 slot

#endif

#else

#define CK_KEY_ID_MULTI_LINK_KC_AES128_ECB      PSA_KEY_HANDLE(PSA_KEY_ID_TRUSTED_STORAGE_MIN)
//size 16 1 slot
#define CK_KEY_ID_MULTI_LINK_KC_KEY 			PSA_KEY_HANDLE(PSA_KEY_ID_TRUSTED_STORAGE_MIN+1)
//size 16 1 slot
#define CK_KEY_ID_MULTI_LINK_KC_HMAC			PSA_KEY_HANDLE(PSA_KEY_ID_TRUSTED_STORAGE_MIN+2)
//size 32 2 slot
#define CK_KEY_ID_MULTI_LINK_DK_HMAC			PSA_KEY_HANDLE(PSA_KEY_ID_TRUSTED_STORAGE_MIN+4)
//size 32 2 slot
#define CK_KEY_ID_MULTI_LINK_DK_AES128_ECB      PSA_KEY_HANDLE(PSA_KEY_ID_TRUSTED_STORAGE_MIN+5)
//size 16 1 slot

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)

#define CK_KEY_ID_MULTI_LINK_KC_HMAC_V2			PSA_KEY_HANDLE(PSA_KEY_ID_TRUSTED_STORAGE_MIN+28)
//size 32 2 slot
#define CK_KEY_ID_MULTI_LINK_DK_HMAC_V2			PSA_KEY_HANDLE(PSA_KEY_ID_TRUSTED_STORAGE_MIN+30)
//size 32 2 slot

#endif

#endif

static psa_key_id_t CK_Create_hmac_sha_256_Volatile(const uint8_t key[])
{
	psa_status_t status;

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_SIGN_HASH);

	psa_set_key_algorithm(&key_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_HMAC);
	psa_set_key_bits(&key_attributes, 256);

	/* Persistent key specific settings */
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);

	psa_key_id_t key_handle;
	status = psa_import_key(&key_attributes, key, 32, &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_import_key failed! (Error: %d)", status);
		return 0;
	}

	return key_handle;
}

static bool CK_hmac_sha_256(const psa_key_id_t key_handle, const uint8_t *data, const size_t data_length, uint8_t result[])
{
	psa_status_t status;
	uint32_t olen;
	psa_mac_operation_t operation = PSA_MAC_OPERATION_INIT;

	/* Initialize the HMAC signing operation */
	status = psa_mac_sign_setup(&operation, key_handle, PSA_ALG_HMAC(PSA_ALG_SHA_256));
	if (status != PSA_SUCCESS)
	{
		LOG_ERR("psa_mac_sign_setup failed %d key=%d", status, key_handle);
		return false;
	}

	/* Perform the HMAC signing */
	status = psa_mac_update(&operation, data, data_length);
	if (status != PSA_SUCCESS)
	{
		return false;
	}

	/* Finalize the HMAC signing */
	status = psa_mac_sign_finish(&operation, result, 32, &olen);
	return (status == PSA_SUCCESS);	
}

static bool CK_hmac_sha_256_Volatile(const uint8_t key[], const uint8_t *data, const size_t data_length, uint8_t result[]) {
	psa_key_id_t key_handle = CK_Create_hmac_sha_256_Volatile(key);
	if (key_handle == 0) {
		LOG_INF("CK_hmac_sha_256_Volatile  create key failed!");
		return false;
	}
	bool ret = CK_hmac_sha_256(key_handle, data, data_length, result);

	psa_destroy_key(key_handle);
	return ret;	
}   

static psa_key_id_t CK_Create_ecb_128_Volatile(const uint8_t key[])
{
	psa_status_t status;
	psa_key_id_t key_handle;

	/* Configure the key attributes */
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECB_NO_PADDING);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&key_attributes, 128);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);

	status = psa_import_key(&key_attributes, key, 16, &key_handle);
	if (status != PSA_SUCCESS) {
		LOG_INF("CK_Create_ecb_128_Volatile psa_import_key failed! (Error: %d)", status);
		return 0;
	}

	return key_handle;
}

static bool CK_ecb_128_Volatile(const uint8_t key[], const uint8_t *data, const size_t data_length, uint8_t *result, size_t result_length) {
	psa_status_t status;
	uint32_t olen;

	psa_key_id_t key_handle = CK_Create_ecb_128_Volatile(key);
	if (key_handle == 0) {
		LOG_INF("CK_ecb_128_Volatile create key failed!");
		return false;
	}

	status = psa_cipher_encrypt(key_handle, PSA_ALG_ECB_NO_PADDING, data, data_length, result, result_length, &olen);

	psa_destroy_key(key_handle);
	return (status == PSA_SUCCESS);	
}

static bool CK_ecb_128(const psa_key_id_t key_handle, const uint8_t *data, const size_t data_length, uint8_t *result, size_t result_length)
{
	psa_status_t status;
	uint32_t olen;

	status = psa_cipher_encrypt(key_handle, PSA_ALG_ECB_NO_PADDING, data, data_length, result, result_length, &olen);
	return (status == PSA_SUCCESS);	
}

static bool CK_load_key(const psa_key_id_t key_handle, uint8_t *key, const size_t buf_len, size_t *pkey_len)
{
	psa_status_t status;

	status = psa_export_key(key_handle, key, buf_len, pkey_len);
	if (status != PSA_SUCCESS) {
		LOG_ERR("psa_export_key failed");
		return false;
		
	}
	return true;	
}

static bool generate_random(uint8_t *result, size_t result_length)
{
	bool retval = false;
	psa_status_t status;

	status = psa_generate_random(result, result_length);

	LOG_HEXDUMP_INF((uint8_t *)result, sizeof(result), "RGN :");

	if (status != PSA_SUCCESS)
	{
		LOG_ERR("psa_generate_random failed! (Error: %d)", status);
	}
	else
	{
		LOG_INF("psa_generate_random Success! (Length: %d)", result_length);
	}
	retval = true;

	return retval;
}

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
///*****************************************************************************
//* Function	: 
//* Description	: 
//* Input		: None
//* Output		: None
//* Return		: None
//* Note		: None
///*****************************************************************************/
#define APP_SUCCESS_MESSAGE "Example finished successfully!"

#define APP_ERROR_MESSAGE "Example exited with error!"

#define APP_SUCCESS		(0)

#define APP_ERROR		(-1)


///*****************************************************************************
//* Function	: 
//* Description	: 
//* Input		: None
//* Output		: None
//* Return		: None
//* Note		: None
///*****************************************************************************/
static int create_ecdh_keypair(uint32_t *key_id)
{
	psa_status_t status;
	psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

	/* Crypto settings for ECDH using the SHA256 hashing algorithm,
	 * the secp256r1 curve
	 */
	psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_DERIVE);
	psa_set_key_lifetime(&key_attributes, PSA_KEY_LIFETIME_VOLATILE);
	psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDH);
	psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
	psa_set_key_bits(&key_attributes, 256);

	/* Generate a key pair */
	status = psa_generate_key(&key_attributes, key_id);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_generate_key failed! (Error: %d)", status);
		return APP_ERROR;
	}
	psa_reset_key_attributes(&key_attributes);

	LOG_INF("ECDH keypair created successfully!");

	return APP_SUCCESS;
}
///*****************************************************************************
//* Function	: 
//* Description	: 
//* Input		: None
//* Output		: None
//* Return		: None
//* Note		: None
///*****************************************************************************/
static int calculate_ecdh_secret(uint32_t *key_id,
			  uint8_t *pub_key,
			  size_t pub_key_len,
			  uint8_t *secret,
			  size_t secret_len)
{
	uint32_t output_len;
	psa_status_t status;

	/* Perform the ECDH key exchange to calculate the secret */
	status = psa_raw_key_agreement(
		PSA_ALG_ECDH, *key_id, pub_key, pub_key_len, secret, secret_len, &output_len);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_raw_key_agreement failed! (Error: %d)", status);
		return APP_ERROR;
	}

	LOG_INF("ECDH secret calculated successfully!");

	return APP_SUCCESS;
}
///*****************************************************************************
//* Function	: 
//* Description	: 
//* Input		: None
//* Output		: None
//* Return		: None
//* Note		: None
///*****************************************************************************/
static int export_ecdh_public_data(uint32_t *key_id, uint8_t *buff, size_t buff_size)
{
	size_t olen;
	psa_status_t status;

	/* Export the public key */
	status = psa_export_public_key(*key_id, buff, buff_size, &olen);
	if (status != PSA_SUCCESS) {
		LOG_INF("psa_export_public_key failed! (Error: %d)", status);
		return APP_ERROR;
	}
	LOG_INF("ECDH public key exported successfully!");

	return APP_SUCCESS;
}
///*****************************************************************************
//* Function	: 
//* Description	: 
//* Input		: None
//* Output		: None
//* Return		: None
//* Note		: None
///*****************************************************************************/
static int generate_ecdh_shared_data(uint32_t *prvkey, uint8_t *pubkey, uint8_t pubkey_length, uint8_t *sharekey, uint8_t sharekey_length)
{
    int status = APP_ERROR;

    /* Calculate the secret value for each participant */
	LOG_INF("Calculating the secret value for Device");

    status = calculate_ecdh_secret(prvkey,
				       pubkey,
				       pubkey_length,
				       sharekey,
				       sharekey_length);

	if (status != APP_SUCCESS) 
    {
		LOG_INF(APP_ERROR_MESSAGE);
	}
    return status;
}
///*****************************************************************************
//* Function	: 
//* Description	: 
//* Input		: None
//* Output		: None
//* Return		: None
//* Note		: None
///*****************************************************************************/
static int generate_ecdh_data(ecdh_key_t *key_info)
{
    int status;

    /* Create the ECDH key pairs for Device */
    LOG_INF("Creating ECDH key pair for Device");

	status = create_ecdh_keypair(&(key_info->TPrivK1)); 

	if (status != PSA_SUCCESS) 
    {
		LOG_INF(APP_ERROR_MESSAGE);

		return status;
	}
    /* Export the ECDH public keys */
	LOG_INF("Export Device public key");
	
    status = export_ecdh_public_data(&key_info->TPrivK1, key_info->TPubK1, sizeof(key_info->TPubK1));

	if (status != APP_SUCCESS) 
    {
		LOG_INF(APP_ERROR_MESSAGE);

		return APP_ERROR;
	}
    memset(key_info->TPubK2, 0, sizeof(key_info->TPubK2));

    return status;
}
#endif

static bool load_key(uint8_t key_type, uint8_t* key, size_t key_length)
{
	if(key_type == MULTI_LINK_STATIC_KEY_TYPE_KC) {
		// size_t len;
		// return CK_load_key(CK_KEY_ID_MULTI_LINK_KC_KEY, key, key_length, &len); //release

        //WARNING WARNING WARNING must remove when release BEGIN
        const uint8_t kc[] = {0x2b, 0x55, 0x10, 0x09, 0x83, 0xae, 0x4e, 0x0c, 0x98, 0xaa, 0xe9, 0xd2, 0x6e, 0xfc, 0xae, 0x18}; // debug
        if(key_length == sizeof(kc))
		{
			memcpy(key, kc, key_length);
			return true;
		}
        //WARNING WARNING WARNING must remove when release END

	}
	return false;
}
#define LOC_LFSR_POLYNOMOAL 0

#define LOC_LFSR_POLYNOMOAL_P1 0

#define LOC_PRIME_INDEX 0

#define LOC_HASH_LOOKUP_TABLE 0

#define LOC_LPRIME_LIST 0

bool load_g3p5_data(uint8_t key_type, void** g3p5_data)
{
	if(key_type == MULTI_LINK_STATIC_DATA_TYPE_TAPS_POLY) 
	{
		*g3p5_data = LOC_LFSR_POLYNOMOAL;
	}
	else if(key_type == MULTI_LINK_STATIC_DATA_TYPE_TAPS_POLY_P1) 
	{
		*g3p5_data = LOC_LFSR_POLYNOMOAL_P1;
	}
	else if(key_type == MULTI_LINK_STATIC_DATA_TYPE_HASH_LOOKUP) 
	{
		*g3p5_data = LOC_HASH_LOOKUP_TABLE;
	}
	else if(key_type == MULTI_LINK_STATIC_DATA_TYPE_LPRIME) 
	{
		*g3p5_data = LOC_LPRIME_LIST;
	}
	else if(key_type == MULTI_LINK_STATIC_DATA_TYPE_PRIME) 
	{
		*g3p5_data = LOC_PRIME_INDEX;
	}
	if(*g3p5_data)
	{
		return true;
	}
	else
	{
		return false;
	}
}
static psa_key_id_t load_key_handle(uint8_t key_type)
{
	switch(key_type) {
		case MULTI_LINK_STATIC_KEY_HANDLE_KC_AES128_ECB:
#if(CONFIG_CHICONY_DEBUG_PSA_HELPER_ENABLE==0)
		    return CK_KEY_ID_MULTI_LINK_KC_AES128_ECB; // release
#else


            //WARNING WARNING WARNING must remove when release BEGIN
            {
            const uint8_t kc[16] = {0x2b, 0x55, 0x10, 0x09, 0x83, 0xae, 0x4e, 0x0c, 0x98, 0xaa, 0xe9, 0xd2, 0x6e, 0xfc, 0xae, 0x18}; 
	    	return CK_Create_ecb_128_Volatile(kc); // debug
            }   
			//WARNING WARNING WARNING must remove when release END
#endif
		case MULTI_LINK_STATIC_KEY_HANDLE_KC_HMAC:
#if(CONFIG_CHICONY_DEBUG_PSA_HELPER_ENABLE==0)
			return CK_KEY_ID_MULTI_LINK_KC_HMAC; // release
#else

            //WARNING WARNING WARNING must remove when release BEGIN
            {
                const uint8_t kc_HMAC_KEY[32] = {0x8a, 0x22, 0xbe, 0x19, 0x80, 0x8e, 0x41, 0xb8, 0x82, 0xfb, 0xe9, 0xda, 0x29, 0x82, 0x86, 0x1, 0x8e, 0x25, 0xb6, 0x54, 0xb9, 0xb1, 0x4c, 0xa5, 0xb7, 0xea, 0xec, 0xe9, 0xd3, 0xd8, 0x2f, 0xaa};
                return CK_Create_hmac_sha_256_Volatile(kc_HMAC_KEY); // debug
            }   
			//WARNING WARNING WARNING must remove when release END
#endif
        case MULTI_LINK_STATIC_KEY_HANDLE_DK_AES128_ECB:
#if(CONFIG_CHICONY_DEBUG_PSA_HELPER_ENABLE==0)
			return CK_KEY_ID_MULTI_LINK_DK_AES128_ECB; // release
#else

            //WARNING WARNING WARNING must remove when release BEGIN
            {
                const uint8_t dk_HMAC_KEY[32] = {0xfb, 0xeb, 0x23, 0x17, 0x5d, 0xbc, 0x49, 0x66, 0xa6, 0xe6, 0x9e, 0x65, 0x20, 0x44, 0xb9, 0xcc, 0x46, 0x41, 0x35, 0x94, 0x87, 0xfe, 0x4e, 0x2b, 0x9d, 0x9, 0xb2, 0x99, 0x5, 0x3f, 0x15, 0x23};
	    	    return CK_Create_ecb_128_Volatile(dk_HMAC_KEY); // debug
            }   
			//WARNING WARNING WARNING must remove when release END
#endif         
		case MULTI_LINK_STATIC_KEY_HANDLE_DK_HMAC:
#if(CONFIG_CHICONY_DEBUG_PSA_HELPER_ENABLE==0)		
			return CK_KEY_ID_MULTI_LINK_DK_HMAC; // release
#else			

            //WARNING WARNING WARNING must remove when release BEGIN
            {
                const uint8_t dk_HMAC_KEY[32] = {0xfb, 0xeb, 0x23, 0x17, 0x5d, 0xbc, 0x49, 0x66, 0xa6, 0xe6, 0x9e, 0x65, 0x20, 0x44, 0xb9, 0xcc, 0x46, 0x41, 0x35, 0x94, 0x87, 0xfe, 0x4e, 0x2b, 0x9d, 0x9, 0xb2, 0x99, 0x5, 0x3f, 0x15, 0x23};
	    	    return CK_Create_hmac_sha_256_Volatile(dk_HMAC_KEY); // debug
            }   
			//WARNING WARNING WARNING must remove when release END
#endif
		case MULTI_LINK_STATIC_KEY_HANDLE_KC_HMAC_V2:
#if(CONFIG_CHICONY_DEBUG_PSA_HELPER_ENABLE==0)	
            return CK_KEY_ID_MULTI_LINK_KC_HMAC_V2; // release

#else		
		 	{
				const uint8_t kc_HMAC_KEY_V2[32] = { 0x43, 0x7e, 0xef, 0xeb, 0x92, 0x27, 0xd1, 0x32, 0x5e, 0x0b, 0x6b, 0xbd, 0xec, 0x3b, 0x6b, 0x46, 0x9f, 0x7e, 0xcb, 0x59, 0x48, 0x85, 0x58, 0x6e, 0xa5, 0x0b, 0x2d, 0xb8, 0x33, 0x67, 0x4f, 0xcb };
				 return CK_Create_hmac_sha_256_Volatile(kc_HMAC_KEY_V2); // debug
            } 
#endif

        case MULTI_LINK_STATIC_KEY_HANDLE_DK_HMAC_V2:
#if(CONFIG_CHICONY_DEBUG_PSA_HELPER_ENABLE==0)	
            return CK_KEY_ID_MULTI_LINK_DK_HMAC_V2; // release
#else
			const uint8_t dk_HMAC_KEY_V2[32] =  { 0x7e, 0xa8, 0x77, 0x01, 0x1a, 0x4b, 0x6c, 0x49, 0xb7, 0xd2, 0x4f, 0x70, 0x56, 0x71, 0x1e, 0x07, 0x3d, 0xb7, 0xcd, 0x56, 0x3d, 0xd1, 0x89, 0x95, 0x6e, 0x0b, 0x2f, 0x83, 0x3f, 0xe0, 0x6e, 0xbd };

        	 return CK_Create_hmac_sha_256_Volatile(dk_HMAC_KEY_V2); // debug
#endif
		default:
			return 0;
	}
}

void fill_callback(multi_link_common_callbacks_t *cb) {
    cb->generate_random = generate_random;
    cb->create_ecb_128 = CK_Create_ecb_128_Volatile;
    cb->hmac_sha_256_raw = CK_hmac_sha_256_Volatile;
    cb->hmac_sha_256 = CK_hmac_sha_256;
    cb->ecb_128_raw = CK_ecb_128_Volatile;
    cb->ecb_128 = CK_ecb_128;
    cb->load_key = load_key;
    cb->load_key_handle = load_key_handle;

#if(CONFIG_DESKTOP_MULTI_LINK_G35_SUPPORT==1)
	cb->generate_ecdh_data = &generate_ecdh_data;
	cb->generate_ecdh_shared_data = &generate_ecdh_shared_data;
	cb->load_g3p5_data = &load_g3p5_data;
#endif
}