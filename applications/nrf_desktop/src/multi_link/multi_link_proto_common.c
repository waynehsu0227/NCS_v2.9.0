#include "multi_link_proto_common.h"

#ifdef __ZEPHYR__
#define MODULE multi_link_proto_common
//Henry add start
//#include <logging/log.h>
//LOG_MODULE_REGISTER(MODULE, CONFIG_DELL_DONGLE_MULTI_LINK_PROTO_COMMON_LOG_LEVEL);
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_INF);
//Henry add end
#else
#define LOG_DBG(...)
#define LOG_ERR(...)
#define LOG_WRN(...)
#define LOG_HEXDUMP_DBG(...)
#endif

/*
#ifdef MULTI_LINK_PROTO_COMMON_DONGLE
#endif
*/

#define SM_Y 257

static multi_link_common_callbacks_t *g_multi_link_common_callbacks;
static uint8_t g_device_nonce_start_value[3];
uint8_t g_Kc[16]; // This key must load from flash storage at runtime. MUST NOT HAVE PART OF CODE {0x2b, 0x55, 0x10, 0x09, 0x83, 0xae, 0x4e, 0x0c, 0x98, 0xaa, 0xe9, 0xd2, 0x6e, 0xfc, 0xae, 0x18};
uint8_t g_Kc_HMAC_KEY[32]; // This key must load from flash storage at runtime. MUST NOT HAVE PART OF CODE {0x8a, 0x22, 0xbe, 0x19, 0x80, 0x8e, 0x41, 0xb8, 0x82, 0xfb, 0xe9, 0xda, 0x29, 0x82, 0x86, 0x1, 0x8e, 0x25, 0xb6, 0x54, 0xb9, 0xb1, 0x4c, 0xa5, 0xb7, 0xea, 0xec, 0xe9, 0xd3, 0xd8, 0x2f, 0xaa};
static const uint8_t g_PC1[4] = {0x7A, 0x1C, 0x9D, 0xF1};
static const uint8_t g_PC2[5] = {0xB1, 0x63, 0x95, 0x57, 0x2E};
static const uint8_t g_taps[] = {24, 12, 9, 4, 2, 1};
static const uint8_t g_dev_class_info[8] = {0x96, 0x88, 0x85, 0x40, 0x6b, 0x6d, 0x4c, 0xf6};
static const uint8_t g_PD1[4] = {0x5B, 0x65, 0xD1, 0x73};
static const uint8_t g_PD2[4] = {0x15, 0xA6, 0x83, 0xC9};
uint8_t g_Dk_HMAC_KEY[32]; // This key must load from flash storage at runtime. MUST NOT HAVE PART OF CODE {0xfb, 0xeb, 0x23, 0x17, 0x5d, 0xbc, 0x49, 0x66, 0xa6, 0xe6, 0x9e, 0x65, 0x20, 0x44, 0xb9, 0xcc, 0x46, 0x41, 0x35, 0x94, 0x87, 0xfe, 0x4e, 0x2b, 0x9d, 0x9, 0xb2, 0x99, 0x5, 0x3f, 0x15, 0x23};


typedef struct
{
    uint8_t Kc[sizeof(g_Kc)];
    uint8_t PC1[sizeof(g_PC1)];
    uint8_t PC2[sizeof(g_PC2)];
    uint8_t device_nonce_start_value[sizeof(g_device_nonce_start_value)];

} __attribute__((packed)) pseed_hmac_p_t;

typedef struct
{
    uint8_t BPMG0[10];
    uint8_t MG0[10];
    uint8_t BPMGN[10];
    uint8_t MGN[10];

} __attribute__((packed)) pseed_t;


typedef struct
{
    uint8_t host_id[5];
    uint8_t PD1[sizeof(g_PD1)];
    uint8_t PD2[sizeof(g_PD2)];
    uint8_t response[5];
    uint8_t challenge[5];
} __attribute__((packed)) dkeyseed_hmac_p_t;


static pseed_t g_PSEED;
static uint16_t g_current_paring_state;
static uint8_t g_Kc0[16];
static uint32_t g_f_integer;
static uint8_t g_f[3];
static uint8_t g_current_packet_buffer[32];
static uint32_t g_current_packet_buffer_length;
static uint8_t g_last_state_packet_buffer[32];
static uint32_t g_last_state_packet_buffer_length;
static uint32_t g_device_nonce_integer;
static uint8_t g_device_nonce[3];
static uint8_t g_salt_challenge[5];
static uint8_t g_challenge_response[5];

void multi_link_proto_common_xor(uint8_t *dst, const uint8_t *src, const uint8_t *pad, uint8_t length)
{
    uint8_t i;

    for (i = 0; i < length; i++)
    {
        *dst = *src ^ *pad;
        dst++;
        src++;
        pad++;
    }
}

int32_t multi_link_proto_common_inverse_modulus(int32_t a, int32_t m)
{
   int m0 = m;
    int y = 0, x = 1;

    if (m == 0 || a == 0)
        return 0;
    if (a == 1)
        return 1;

    while (a > 1) {
        // q is quotient
        int q = a / m;
        int t = m;

        // m is remainder now, process same as
        // Euclid's algo
        m = a % m, a = t;
        t = y;

        // Update y and x
        y = x - q * y;
        x = t;

        if (m == 0 && a !=1)
            return 0;
    }

    // Make x positive
    if (x < 0)
        x += m0;

    return x;
}

uint32_t multi_link_proto_common_state_value(uint32_t start_value, uint16_t step, const uint8_t *taps, uint8_t taps_size)
{
    uint32_t sr;
    uint32_t mask = 16777215; // power(2, 24) - 1;
    uint32_t xor_v = 0;

    if (start_value)
        sr = start_value;
    else
    {
        sr = 1;
        step = 1;
    }

    uint32_t i = step;

    while (i)
    {
        for (uint8_t tap_index = 0; tap_index < taps_size; tap_index++)
        {
            xor_v ^= (sr >> (24 - taps[tap_index])) & 1;
        }

        sr = (xor_v << (24 - 1)) | ((sr >> 1) & mask);

        xor_v = 0;

        if (sr == start_value)
            break;

        i -= 1;
    }

    return sr;
}

uint16_t multi_link_proto_common_calculate_step(uint32_t value)
{
    uint16_t step = value % SM_Y;
    if (step == 0)
        step = SM_Y;

    return step;
}

void multi_link_proto_common_calculate_f(uint32_t start_value, uint16_t step, uint32_t *f_integer, uint8_t (*f)[3])
{
    if (step == 0)
    {
        step = multi_link_proto_common_calculate_step(start_value);
    }

    LOG_DBG("start_value 0x%x", start_value);
    LOG_DBG("step = %d", step);

    *f_integer = multi_link_proto_common_state_value(start_value, step, g_taps, sizeof(g_taps));
    f[0][2] = (*f_integer & 0xff);
    f[0][1] = (*f_integer >> 8 & 0xff);
    f[0][0] = (*f_integer >> 16 & 0xff);

    LOG_DBG("f_integer = 0x%x", *f_integer);
    LOG_HEXDUMP_DBG((uint8_t *)&f[0], sizeof(f[0]), "f :");
}

uint16_t multi_link_proto_common_current_paring_state(void)
{
    return g_current_paring_state;
}

void multi_link_proto_common_current_packet(const uint8_t **packet, uint32_t *packet_length)
{
    *packet = g_current_packet_buffer;
    *packet_length = g_current_packet_buffer_length;
}

void multi_link_proto_common_last_state_packet(const uint8_t **packet, uint32_t *packet_length)
{
    *packet = g_last_state_packet_buffer;
    *packet_length = g_last_state_packet_buffer_length;
}

bool multi_link_proto_common_init_callbacks(multi_link_common_callbacks_t *multi_link_common_callbacks)
{

    if (multi_link_common_callbacks->hmac_sha_256 &&
        multi_link_common_callbacks->generate_random &&
        multi_link_common_callbacks->ecb_128 && 
        multi_link_common_callbacks->load_key)
    {
        g_multi_link_common_callbacks = multi_link_common_callbacks;

        if(!g_multi_link_common_callbacks->load_key(MULTI_LINK_STATIC_KEY_TYPE_KC, g_Kc, sizeof(g_Kc)))
        {
            LOG_ERR("Loading g_Kc failed");
            return false;            
        }

        if(!g_multi_link_common_callbacks->load_key(MULTI_LINK_STATIC_KEY_TYPE_KC_HMAC, g_Kc_HMAC_KEY, sizeof(g_Kc_HMAC_KEY)))
        {
            LOG_ERR("Loading g_Kc_HMAC_KEY failed");
            return false;            
        }

        if(!g_multi_link_common_callbacks->load_key(MULTI_LINK_STATIC_KEY_TYPE_DK_HMAC, g_Dk_HMAC_KEY, sizeof(g_Dk_HMAC_KEY)))
        {
            LOG_ERR("Loading g_Dk_HMAC_KEY failed");
            return false;            
        }

        return true;
    }

    return false;
}

bool multi_link_proto_common_init_paring(uint8_t (*device_nonce_start_value)[3])
{
    if (device_nonce_start_value[0][0] == 0 && device_nonce_start_value[0][1] == 0 && device_nonce_start_value[0][2] == 0)
    {
        if (!g_multi_link_common_callbacks->generate_random(g_device_nonce_start_value, sizeof(g_device_nonce_start_value)))
        {
            LOG_ERR("generate_random failed");
            return false;
        }
    }
    else
    {
        memcpy(g_device_nonce_start_value, device_nonce_start_value, sizeof(g_device_nonce_start_value));
    }

    LOG_HEXDUMP_DBG(g_device_nonce_start_value, sizeof(g_device_nonce_start_value), "g_device_nonce_start_value :");

    //
    // PSEED = HMAC( SHA256, Kc||PC1||PC2||DeviceNonceStartValue)  = 32 Bytes.
    //
    pseed_hmac_p_t pseed_hmac_p;
    uint8_t pseed_temp[32];

    memset(&pseed_hmac_p, 0x00, sizeof(pseed_hmac_p));
    memcpy(pseed_hmac_p.Kc, g_Kc, sizeof(g_Kc));
    memcpy(pseed_hmac_p.PC1, g_PC1, sizeof(g_PC1));
    memcpy(pseed_hmac_p.PC2, g_PC2, sizeof(g_PC2));
    memcpy(pseed_hmac_p.device_nonce_start_value, g_device_nonce_start_value, sizeof(g_device_nonce_start_value));

    if (!g_multi_link_common_callbacks->hmac_sha_256((const uint8_t(*)[sizeof(g_Kc_HMAC_KEY)])g_Kc_HMAC_KEY, (uint8_t *)&pseed_hmac_p, sizeof(pseed_hmac_p), (uint8_t(*)[sizeof(pseed_temp)]) & pseed_temp))
    {
        LOG_ERR("hmac_sha_256 failed for g_PSEED");
        return false;
    }

    memcpy(&g_PSEED.BPMG0, &pseed_temp[0], 10);
    memcpy(&g_PSEED.MG0, &pseed_temp[21], 10);
    memcpy(&g_PSEED.BPMGN, &pseed_temp[9], 10);
    memcpy(&g_PSEED.MGN, &pseed_temp[17], 10);

    LOG_HEXDUMP_DBG((uint8_t *)&g_PSEED.BPMG0, sizeof(g_PSEED.BPMG0), "BPMG0 :");
    LOG_HEXDUMP_DBG((uint8_t *)&g_PSEED.MG0, sizeof(g_PSEED.MG0), "MG0 :");
    LOG_HEXDUMP_DBG((uint8_t *)&g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN), "BPMGN :");
    LOG_HEXDUMP_DBG((uint8_t *)&g_PSEED.MGN, sizeof(g_PSEED.MGN), "MGN :");

    memset(g_Kc0, 0x00, sizeof(g_Kc0));

    //
    // Kc0 = BPMG0||LSB-3-bytes of(Kc)||DeviceNonceStartValue
    //
    memcpy(g_Kc0, g_PSEED.BPMG0, sizeof(g_PSEED.BPMG0));
    memcpy(&g_Kc0[sizeof(g_PSEED.BPMG0)], &g_Kc[sizeof(g_Kc) - 3], 3);
    memcpy(&g_Kc0[sizeof(g_PSEED.BPMG0) + 3], g_device_nonce_start_value, sizeof(g_device_nonce_start_value));

    LOG_HEXDUMP_DBG((uint8_t *)&g_Kc0, sizeof(g_Kc0), "g_Kc0 :");

    memset(g_salt_challenge, 0x00, sizeof(g_salt_challenge));
    memset(g_challenge_response, 0x00, sizeof(g_challenge_response));

    g_current_packet_buffer_length = 0;
    g_last_state_packet_buffer_length = 0;
    g_current_paring_state = MULTI_LINK_PARING_STATE_UNKNOWN;

    return true;
}

void multi_link_proto_common_clear_paring(void)
{
    g_current_packet_buffer_length = 0;
    g_last_state_packet_buffer_length = 0;
    g_current_paring_state = MULTI_LINK_PARING_STATE_UNKNOWN;
}

multi_link_host_address_request_ex_t *multi_link_proto_common_encode_host_address_request_ex(uint8_t backoff_delay, uint8_t retry_interval, uint8_t device_type, uint8_t vendor_id, uint8_t (*device_id)[3])
{

    LOG_DBG("-----multi_link_proto_common_encode_host_address_request_ex-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    multi_link_proto_common_calculate_f(((uint32_t)g_device_nonce_start_value[0] << 16 | (uint32_t)g_device_nonce_start_value[1] << 8 | (uint32_t)g_device_nonce_start_value[2]),
                                        0,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF0 now
    // SF0 = E(kc-0 , MG0||F0)
    //
    uint8_t sf0[16];
    uint8_t sf0_p[16];

    memset(sf0, 0x00, sizeof(sf0));
    memset(sf0_p, 0x00, sizeof(sf0_p));
    memcpy(sf0_p, g_PSEED.MG0, sizeof(g_PSEED.MG0));
    memcpy(&sf0_p[sizeof(g_PSEED.MG0)], g_f, sizeof(g_f));

    LOG_HEXDUMP_DBG((uint8_t *)&sf0_p, sizeof(sf0_p), "sf0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc0)])g_Kc0, sf0_p, sizeof(sf0_p), sf0, sizeof(sf0)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf0, sizeof(sf0), "sf0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf0");
        return 0;
    }

    //
    // Calculate MAC0
    // MAC0 =E( kc-0 , DellClassInfo||F0||Dev-ID||device_type||vendor_id )
    //
    uint8_t mac0[16];
    uint8_t mac0_p[16];

    memset(mac0, 0x00, sizeof(mac0));
    memset(mac0_p, 0x00, sizeof(mac0_p));
    memcpy(mac0_p, g_dev_class_info, sizeof(g_dev_class_info));
    memcpy(&mac0_p[sizeof(g_dev_class_info)], g_f, sizeof(g_f));
    memcpy(&mac0_p[sizeof(g_dev_class_info) + sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    mac0_p[sizeof(g_dev_class_info) + sizeof(g_f) + sizeof(device_id[0])] = device_type;
    mac0_p[sizeof(g_dev_class_info) + sizeof(g_f) + sizeof(device_id[0]) + sizeof(device_type)] = vendor_id;

    LOG_HEXDUMP_DBG((uint8_t *)&mac0_p, sizeof(mac0_p), "mac0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc0)])g_Kc0, mac0_p, sizeof(mac0_p), mac0, sizeof(mac0)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac0, sizeof(mac0), "mac0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac0");
        return 0;
    }

    //
    // Calculate Es
    // Es = E(Kc,SF0)
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf0, sizeof(sf0), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    //
    // g_device_nonce should be randnom not g_f
    //
    if (!g_multi_link_common_callbacks->generate_random(g_device_nonce, sizeof(g_device_nonce)))
    {
        LOG_ERR("generate_random failed");
        return 0;
    }
    g_device_nonce_integer = ((uint32_t)g_device_nonce[0] << 16 | (uint32_t)g_device_nonce[1] << 8 | (uint32_t)g_device_nonce[2]);


    LOG_DBG("g_device_nonce_integer = 0x%x", g_device_nonce_integer);
    LOG_HEXDUMP_DBG((uint8_t *)g_device_nonce, sizeof(g_device_nonce), "g_device_nonce :");

    multi_link_host_address_request_ex_t *multi_link_host_address_request_ex = (multi_link_host_address_request_ex_t *)g_current_packet_buffer;

    multi_link_host_address_request_ex->cmd = GZP_CMD_HOST_ADDRESS_REQ_EX;
    multi_link_host_address_request_ex->session_config_backoff_delay = backoff_delay;
    multi_link_host_address_request_ex->session_config_retry_interval = retry_interval;
    memcpy(&multi_link_host_address_request_ex->device_nonce_start_value, g_device_nonce_start_value, sizeof(g_device_nonce_start_value));

    memcpy(&multi_link_host_address_request_ex->payload_p.mac, mac0, sizeof(multi_link_host_address_request_ex->payload_p.mac));
    memcpy(&multi_link_host_address_request_ex->payload_p.device_nonce, g_device_nonce, sizeof(multi_link_host_address_request_ex->payload_p.device_nonce));
    multi_link_host_address_request_ex->payload_p.device_type = device_type;
    multi_link_host_address_request_ex->payload_p.vendor_id = vendor_id;
    memcpy(&multi_link_host_address_request_ex->payload_p.device_id, device_id, sizeof(multi_link_host_address_request_ex->payload_p.device_id));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_request_ex->payload_p, sizeof(multi_link_host_address_request_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_request_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_address_request_ex->payload_p, sizeof(multi_link_host_address_request_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_request_ex->payload_ep, sizeof(multi_link_host_address_request_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_request_ex, sizeof(multi_link_host_address_request_ex_t), "multi_link_host_address_request_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_address_request_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_EX;

    return multi_link_host_address_request_ex;
}

bool multi_link_proto_common_decode_host_address_request_ex(multi_link_host_address_request_ex_t *multi_link_host_address_request_ex)
{
    LOG_DBG("-----multi_link_proto_common_decode_host_address_request_ex-----");

    multi_link_proto_common_calculate_f(((uint32_t)g_device_nonce_start_value[0] << 16 | (uint32_t)g_device_nonce_start_value[1] << 8 | (uint32_t)g_device_nonce_start_value[2]),
                                        0,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF0 now
    // SF0 = E(kc-0 , MG0||F0)
    //
    uint8_t sf0[16];
    uint8_t sf0_p[16];

    memset(sf0, 0x00, sizeof(sf0));
    memset(sf0_p, 0x00, sizeof(sf0_p));
    memcpy(sf0_p, g_PSEED.MG0, sizeof(g_PSEED.MG0));
    memcpy(&sf0_p[sizeof(g_PSEED.MG0)], g_f, sizeof(g_f));

    LOG_HEXDUMP_DBG((uint8_t *)&sf0_p, sizeof(sf0_p), "sf0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc0)])g_Kc0, sf0_p, sizeof(sf0_p), sf0, sizeof(sf0)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf0, sizeof(sf0), "sf0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf0");
        return false;
    }

    //
    // Calculate Es
    // Es = E(Kc,SF0)
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf0, sizeof(sf0), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_request_ex->payload_ep, sizeof(multi_link_host_address_request_ex->payload_p), "payload_ep :");

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_request_ex->payload_p, (const uint8_t *)&multi_link_host_address_request_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_address_request_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_request_ex->payload_p, sizeof(multi_link_host_address_request_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_request_ex, sizeof(multi_link_host_address_request_ex_t), "multi_link_host_address_request_ex decoded:");

    //
    // Calculate MAC0
    // MAC0 =E( kc-0 , DellClassInfo||F0||Dev-ID||device_type||vendor_id )
    //
    uint8_t mac0[16];
    uint8_t mac0_p[16];

    memset(mac0, 0x00, sizeof(mac0));
    memset(mac0_p, 0x00, sizeof(mac0_p));
    memcpy(mac0_p, g_dev_class_info, sizeof(g_dev_class_info));
    memcpy(&mac0_p[sizeof(g_dev_class_info)], g_f, sizeof(g_f));
    memcpy(&mac0_p[sizeof(g_dev_class_info) + sizeof(g_f)], multi_link_host_address_request_ex->payload_p.device_id, sizeof(multi_link_host_address_request_ex->payload_p.device_id));
    mac0_p[sizeof(g_dev_class_info) + sizeof(g_f) + sizeof(multi_link_host_address_request_ex->payload_p.device_id)] = multi_link_host_address_request_ex->payload_p.device_type;
    mac0_p[sizeof(g_dev_class_info) + sizeof(g_f) + sizeof(multi_link_host_address_request_ex->payload_p.device_id) + sizeof(multi_link_host_address_request_ex->payload_p.device_type)] = multi_link_host_address_request_ex->payload_p.vendor_id;

    LOG_HEXDUMP_DBG((uint8_t *)&mac0_p, sizeof(mac0_p), "mac0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc0)])g_Kc0, mac0_p, sizeof(mac0_p), mac0, sizeof(mac0)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac0, sizeof(mac0), "mac0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac0");
        return false;
    }

    if (memcmp(multi_link_host_address_request_ex->payload_p.mac, mac0, sizeof(multi_link_host_address_request_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac0 compare failed");
        return false;
    }

    //
    // g_device_nonce should not g_f so get what device
    //
    g_device_nonce[0] = multi_link_host_address_request_ex->payload_p.device_nonce[0];
    g_device_nonce[1] = multi_link_host_address_request_ex->payload_p.device_nonce[1];
    g_device_nonce[2] = multi_link_host_address_request_ex->payload_p.device_nonce[2];
    g_device_nonce_integer = ((uint32_t)g_device_nonce[0] << 16 | (uint32_t)g_device_nonce[1] << 8 | (uint32_t)g_device_nonce[2]);

    LOG_DBG("g_device_nonce_integer = 0x%x", g_device_nonce_integer);
    LOG_HEXDUMP_DBG((uint8_t *)g_device_nonce, sizeof(g_device_nonce), "g_device_nonce :");

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_EX;

    return true;
}

multi_link_host_address_fetch_request_init_ex_t *multi_link_proto_common_encode_host_address_fetch_request_init_ex(uint8_t (*device_id)[3], uint8_t (*device_name)[10])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_address_fetch_request_init_ex-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(S1) ->   F(S0-value, Step=1, Count) , S0-value(DEV-N)  is sent at Host Address Request
    //
    multi_link_proto_common_calculate_f(g_device_nonce_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF(1) = E( kc-1,  MG-n||S1||F(S1) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n(1) = BPMGN||F||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E( kc-1 , F1||Device-ID||Device-Name||S1 )
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], device_name[0], sizeof(device_name[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(device_name[0])], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_address_fetch_request_init_ex_t *multi_link_host_address_fetch_request_init = (multi_link_host_address_fetch_request_init_ex_t *)g_current_packet_buffer;

    multi_link_host_address_fetch_request_init->cmd = GZP_CMD_HOST_ADDRESS_FETCH_REQ_INIT_EX;
    memcpy(&multi_link_host_address_fetch_request_init->payload_p.mac, mac, sizeof(multi_link_host_address_fetch_request_init->payload_p.mac));
    memcpy(&multi_link_host_address_fetch_request_init->payload_p.device_id, device_id[0], sizeof(multi_link_host_address_fetch_request_init->payload_p.device_id));
    memcpy(&multi_link_host_address_fetch_request_init->payload_p.device_name, device_name[0], sizeof(multi_link_host_address_fetch_request_init->payload_p.device_name));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_fetch_request_init->payload_p, sizeof(multi_link_host_address_fetch_request_init->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_fetch_request_init->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_address_fetch_request_init->payload_p, sizeof(multi_link_host_address_fetch_request_init->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_fetch_request_init->payload_ep, sizeof(multi_link_host_address_fetch_request_init->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_fetch_request_init, sizeof(multi_link_host_address_fetch_request_init_ex_t), "multi_link_host_address_fetch_request_init encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_address_fetch_request_init_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT;

    return multi_link_host_address_fetch_request_init;
}

bool multi_link_proto_common_decode_host_address_fetch_request_init_ex(multi_link_host_address_fetch_request_init_ex_t *multi_link_host_address_fetch_request_init)
{

    LOG_DBG("-----multi_link_proto_common_encode_host_address_fetch_request_init_ex-----");

    //
    // F(S1) ->   F(S0-value, Step=1, Count) , S0-value(DEV-N)  is sent at Host Address Request
    //
    multi_link_proto_common_calculate_f(g_device_nonce_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF(1) = E( kc-1,  MG-n||S1||F(S1) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n(1) = BPMGN||Dev-N||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_fetch_request_init->payload_p, (const uint8_t *)&multi_link_host_address_fetch_request_init->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_address_fetch_request_init->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_fetch_request_init->payload_p, sizeof(multi_link_host_address_fetch_request_init->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_fetch_request_init, sizeof(multi_link_host_address_fetch_request_init_ex_t), "multi_link_host_address_fetch_request_init decoded:");

    //
    // Calculate MAC
    // E( kc-1 , F1||Device-ID||Device-Name||S1 )
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], multi_link_host_address_fetch_request_init->payload_p.device_id, sizeof(multi_link_host_address_fetch_request_init->payload_p.device_id));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_address_fetch_request_init->payload_p.device_id)], multi_link_host_address_fetch_request_init->payload_p.device_name, sizeof(multi_link_host_address_fetch_request_init->payload_p.device_name));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_address_fetch_request_init->payload_p.device_id) + sizeof(multi_link_host_address_fetch_request_init->payload_p.device_name)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_address_fetch_request_init->payload_p.mac, mac, sizeof(multi_link_host_address_fetch_request_init->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT;

    return true;
}

multi_link_host_address_pairing_challenge_ex_t *multi_link_proto_common_encode_host_address_pairing_challenge(uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_address_pairing_challenge-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(S1) ->   F(S0-value, Step=1, Count) , S0-value(DEV-N)  is sent at Host Address Request
    //
    multi_link_proto_common_calculate_f(g_device_nonce_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-1,  MG-n||S1||F(S1) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Dev-N||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    if (g_multi_link_common_callbacks->generate_random(g_salt_challenge, sizeof(g_salt_challenge)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&g_salt_challenge, sizeof(g_salt_challenge), "salt_challenge :");
    }
    else
    {
        LOG_ERR("generate_random failed for salt_challenge");
        return 0;
    }

    //
    // Calculate MAC
    // MAC = E( kc-1 , F1||Device-ID||Salt-Challenge||S1 )
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], g_salt_challenge, sizeof(g_salt_challenge));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(g_salt_challenge)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_address_pairing_challenge_ex_t *multi_link_host_address_pairing_challenge = (multi_link_host_address_pairing_challenge_ex_t *)g_current_packet_buffer;

    multi_link_host_address_pairing_challenge->cmd = GZP_CMD_HOST_ADDRESS_PAIRING_CHALLENGE_EX;
    memcpy(&multi_link_host_address_pairing_challenge->payload_p.mac, mac, sizeof(multi_link_host_address_pairing_challenge->payload_p.mac));
    memcpy(&multi_link_host_address_pairing_challenge->payload_p.salt_challenge, g_salt_challenge, sizeof(multi_link_host_address_pairing_challenge->payload_p.salt_challenge));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_pairing_challenge->payload_p, sizeof(multi_link_host_address_pairing_challenge->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_pairing_challenge->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_address_pairing_challenge->payload_p, sizeof(multi_link_host_address_pairing_challenge->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_pairing_challenge->payload_ep, sizeof(multi_link_host_address_pairing_challenge->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_pairing_challenge, sizeof(multi_link_host_address_pairing_challenge_ex_t), "multi_link_host_address_pairing_challenge encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_address_pairing_challenge_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT;

    return multi_link_host_address_pairing_challenge;
}

bool multi_link_proto_common_decode_host_address_pairing_challenge(multi_link_host_address_pairing_challenge_ex_t *multi_link_host_address_pairing_challenge, uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_decode_host_address_pairing_challenge-----");

    //
    // F(S1) ->   F(S0-value, Step=1, Count) , S0-value(DEV-N)  is sent at Host Address Request
    //
    multi_link_proto_common_calculate_f(g_device_nonce_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-1,  MG-n||S1||F(S1) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_REQ_INIT};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Dev-N||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_pairing_challenge->payload_p, (const uint8_t *)&multi_link_host_address_pairing_challenge->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_address_pairing_challenge->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_pairing_challenge->payload_p, sizeof(multi_link_host_address_pairing_challenge->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_pairing_challenge, sizeof(multi_link_host_address_pairing_challenge_ex_t), "multi_link_host_address_pairing_challenge decoded:");

    //
    // Calculate MAC
    // MAC = E( kc-1 , F1||Device-ID||Salt-Challenge||S1 )
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], multi_link_host_address_pairing_challenge->payload_p.salt_challenge, sizeof(multi_link_host_address_pairing_challenge->payload_p.salt_challenge));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(multi_link_host_address_pairing_challenge->payload_p.salt_challenge)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    if (memcmp(multi_link_host_address_pairing_challenge->payload_p.mac, mac, sizeof(multi_link_host_address_pairing_challenge->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    return true;
}

multi_link_host_address_pairing_response_ex_t *multi_link_proto_common_encode_host_address_pairing_response(uint8_t (*device_id)[3], uint8_t (*salt_challenge)[5])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_address_pairing_response-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[32];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, 16, "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // CRx0= HMAC(kc-2, SHA256(Challenge|| Dev-N|| FS(s2)|| S2-STEP251 ) //32Bytes Output
    // CRx1=E( MSB(CRx0,16), LSB(CRx0, 16),  //16Bytes MSB as Key, 16 Lower bytes as Data)
    // CRx2= E( CRx1, MSB( CRx0,16) )
    // CR=MID(CRx2,4,5)   //Extract 5 Bytes ,from Byte 4 , CR[4]….CR[8]
    // S2-Step251 = LSB4Bytes(Challenge)  Modules 251 .//, 
    // If S2-Step calculation yield zero, Set S2-Step251 =251 
    //


    //
    // S2-Step251 = LSB4Bytes(Challenge)  Modules 251 .//, 
    // If S2-Step calculation yield zero, Set S2-Step251 =251 
    //
    uint32_t s2_step = *((uint32_t*)&salt_challenge[0]);
    uint32_t s2_step_251 = (s2_step % 251);
    if(s2_step_251 == 0)
        s2_step_251 = 251;

    //
    // CRx0= HMAC(kc-2, SHA256(Challenge|| Dev-N|| FS(s2)|| S2-STEP251 ) 
    //
    uint8_t response_p[32];
    uint8_t crx0[32];
    memset(response_p, 0x00, sizeof(response_p));
    memset(crx0, 0x00, sizeof(crx0));
    memcpy(response_p, salt_challenge[0], sizeof(salt_challenge[0]));
    memcpy(&response_p[sizeof(salt_challenge[0])], g_device_nonce, sizeof(g_device_nonce));
    memcpy(&response_p[sizeof(salt_challenge[0]) + sizeof(g_device_nonce)], sf, sizeof(sf));
    memcpy(&response_p[sizeof(salt_challenge[0]) + sizeof(g_device_nonce) + sizeof(sf)], &s2_step_251, sizeof(s2_step_251));
  
    if (g_multi_link_common_callbacks->hmac_sha_256((const uint8_t(*)[sizeof(kc_n)])kc_n, (uint8_t *)&response_p, sizeof(response_p), (uint8_t(*)[sizeof(crx0)]) & crx0))
    {
        uint8_t* crx1 = (uint8_t*)response_p;
        memset(response_p, 0x00, sizeof(response_p));   


        //
        // CRx1=E( MSB(CRx0,16), LSB(CRx0, 16),  //16Bytes MSB as Key, 16 Lower bytes as Data)
        //
        if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])crx0, (uint8_t *) &crx0[16], 16, crx1, 16))
        {
            uint8_t* crx2 = (uint8_t*)&response_p[16];

            //
            // CRx2= E( CRx1, MSB( CRx0,16) )
            //
            if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])crx1, crx0, 16, crx2, 16))
            {
                // 
                // CR=MID(CRx2,4,5)   
                //
                memcpy(g_challenge_response, &crx2[4], 5);

                LOG_HEXDUMP_DBG((uint8_t *)g_challenge_response, 5, "response :");
            }
            else
            {
                LOG_ERR("ecb_128 failed for response crx2");
                return 0;
            }
        }
        else
        {
            LOG_ERR("ecb_128 failed for response crx1");
            return 0;
        }
    }
    else
    {
        LOG_ERR("hmac_sha_256 failed for response crx0");
        return 0;
    }

    // Calculate MAC
    // MAC = E( kc-1 , F||Device-ID||Salt-response||s-n )
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], g_challenge_response, 5);
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + 5], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_address_pairing_response_ex_t *multi_link_host_address_pairing_response_ex = (multi_link_host_address_pairing_response_ex_t *)g_current_packet_buffer;

    multi_link_host_address_pairing_response_ex->cmd = GZP_CMD_HOST_ADDRESS_PAIRING_RESPONSE_EX;
    memcpy(&multi_link_host_address_pairing_response_ex->payload_p.mac, mac, sizeof(multi_link_host_address_pairing_response_ex->payload_p.mac));
    memcpy(&multi_link_host_address_pairing_response_ex->payload_p.device_id, device_id[0], sizeof(device_id[0]));
    memcpy(&multi_link_host_address_pairing_response_ex->payload_p.response, g_challenge_response, 5);

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_pairing_response_ex->payload_p, sizeof(multi_link_host_address_pairing_response_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_pairing_response_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_address_pairing_response_ex->payload_p, sizeof(multi_link_host_address_pairing_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_pairing_response_ex->payload_ep, sizeof(multi_link_host_address_pairing_response_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_pairing_response_ex, sizeof(multi_link_host_address_pairing_response_ex_t), "multi_link_host_address_pairing_response_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_address_pairing_response_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX;

    return multi_link_host_address_pairing_response_ex;
}

bool multi_link_proto_common_decode_host_address_pairing_response(multi_link_host_address_pairing_response_ex_t *multi_link_host_address_pairing_response_ex, uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_decode_host_address_pairing_response-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[32];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, 16, "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // CRx0= HMAC(kc-2, SHA256(Challenge|| Dev-N|| FS(s2)|| S2-STEP251 ) //32Bytes Output
    // CRx1=E( MSB(CRx0,16), LSB(CRx0, 16),  //16Bytes MSB as Key, 16 Lower bytes as Data)
    // CRx2= E( CRx1, MSB( CRx0,16) )
    // CR=MID(CRx2,4,5)   //Extract 5 Bytes ,from Byte 4 , CR[4]….CR[8]
    // S2-Step251 = LSB4Bytes(Challenge)  Modules 251 .//, 
    // If S2-Step calculation yield zero, Set S2-Step251 =251 
    //


    //
    // S2-Step251 = LSB4Bytes(Challenge)  Modules 251 .//, 
    // If S2-Step calculation yield zero, Set S2-Step251 =251 
    //
    uint32_t s2_step = *((uint32_t*)&g_salt_challenge[0]);
    uint32_t s2_step_251 = (s2_step % 251);
    if(s2_step_251 == 0)
        s2_step_251 = 251;

    //
    // CRx0= HMAC(kc-2, SHA256(Challenge|| Dev-N|| FS(s2)|| S2-STEP251 ) 
    //
    uint8_t response_p[32];
    uint8_t crx0[32];
    memset(response_p, 0x00, sizeof(response_p));
    memset(crx0, 0x00, sizeof(crx0));
    memcpy(response_p, g_salt_challenge, sizeof(g_salt_challenge));
    memcpy(&response_p[sizeof(g_salt_challenge)], g_device_nonce, sizeof(g_device_nonce));
    memcpy(&response_p[sizeof(g_salt_challenge) + sizeof(g_device_nonce)], sf, sizeof(sf));
    memcpy(&response_p[sizeof(g_salt_challenge) + sizeof(g_device_nonce) + sizeof(sf)], &s2_step_251, sizeof(s2_step_251));
  
    if (g_multi_link_common_callbacks->hmac_sha_256((const uint8_t(*)[sizeof(kc_n)])kc_n, (uint8_t *)&response_p, sizeof(response_p), (uint8_t(*)[sizeof(crx0)]) & crx0))
    {
        uint8_t* crx1 = (uint8_t*)response_p;
        memset(response_p, 0x00, sizeof(response_p));   


        //
        // CRx1=E( MSB(CRx0,16), LSB(CRx0, 16),  //16Bytes MSB as Key, 16 Lower bytes as Data)
        //
        if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])crx0, &crx0[16], 16, crx1, 16))
        {
            uint8_t* crx2 = (uint8_t*)&response_p[16];

            //
            // CRx2= E( CRx1, MSB( CRx0,16) )
            //
            if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])crx1, crx0, 16, crx2, 16))
            {
                // 
                // CR=MID(CRx2,4,5)   
                //
                memcpy(g_challenge_response, &crx2[4], 5);

                LOG_HEXDUMP_DBG((uint8_t *)g_challenge_response, 5, "response :");
            }
            else
            {
                LOG_ERR("ecb_128 failed for response crx2");
                return false;
            }
        }
        else
        {
            LOG_ERR("ecb_128 failed for response crx1");
            return false;
        }
    }
    else
    {
        LOG_ERR("hmac_sha_256 failed for response crx0");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_pairing_response_ex->payload_p, (const uint8_t *)&multi_link_host_address_pairing_response_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_address_pairing_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_pairing_response_ex->payload_p, sizeof(multi_link_host_address_pairing_response_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_pairing_response_ex, sizeof(multi_link_host_address_pairing_response_ex_t), "multi_link_host_address_pairing_response_ex decoded:");

    // Calculate MAC
    // MAC = E( kc-1 , F||Device-ID||Salt-response||s-n )
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], multi_link_host_address_pairing_response_ex->payload_p.response, sizeof(multi_link_host_address_pairing_response_ex->payload_p.response));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(multi_link_host_address_pairing_response_ex->payload_p.response)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_address_pairing_response_ex->payload_p.mac, mac, sizeof(multi_link_host_address_pairing_response_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    if (memcmp(multi_link_host_address_pairing_response_ex->payload_p.response, g_challenge_response, sizeof(multi_link_host_address_pairing_response_ex->payload_p.response)) != 0)
    {
        LOG_ERR("response compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_PAIRING_RESPONSE_EX;

    return true;
}

multi_link_host_address_fetch_request_final_ex_t *multi_link_proto_common_encode_host_address_fetch_request_final_ex(uint8_t (*device_id)[3], uint8_t (*device_name)[10])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_address_fetch_request_final_ex-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E( kc-1 , F1||Device-ID||Device-Name||Sn )
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], device_name[0], sizeof(device_name[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(device_name[0])], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_address_fetch_request_final_ex_t *multi_link_host_address_fetch_request_final_ex = (multi_link_host_address_fetch_request_final_ex_t *)g_current_packet_buffer;

    multi_link_host_address_fetch_request_final_ex->cmd = GZP_CMD_HOST_ADDRESS_FETCH_REQ_FINAL_EX;
    memcpy(&multi_link_host_address_fetch_request_final_ex->payload_p.mac, mac, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.mac));
    memcpy(&multi_link_host_address_fetch_request_final_ex->payload_p.device_id, device_id[0], sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.device_id));
    memcpy(&multi_link_host_address_fetch_request_final_ex->payload_p.device_name, device_name[0], sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.device_name));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_fetch_request_final_ex->payload_p, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_fetch_request_final_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_address_fetch_request_final_ex->payload_p, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_fetch_request_final_ex->payload_ep, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_fetch_request_final_ex, sizeof(multi_link_host_address_fetch_request_final_ex_t), "multi_link_host_address_fetch_request_final_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_address_fetch_request_final_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX;

    return multi_link_host_address_fetch_request_final_ex;
}

bool multi_link_proto_common_decode_host_address_fetch_request_final_ex(multi_link_host_address_fetch_request_final_ex_t *multi_link_host_address_fetch_request_final_ex)
{
    LOG_DBG("-----multi_link_proto_common_decode_host_address_fetch_request_final_ex-----");


    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_fetch_request_final_ex->payload_p, (const uint8_t *)&multi_link_host_address_fetch_request_final_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_fetch_request_final_ex->payload_p, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_fetch_request_final_ex, sizeof(multi_link_host_address_fetch_request_final_ex_t), "multi_link_host_address_fetch_request_final_ex decoded:");

    //
    // Calculate MAC
    // E( kc-1 , F1||Device-ID||Device-Name||Sn )
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], multi_link_host_address_fetch_request_final_ex->payload_p.device_id, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.device_id));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.device_id)], multi_link_host_address_fetch_request_final_ex->payload_p.device_name, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.device_name));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.device_id) + sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.device_name)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_address_fetch_request_final_ex->payload_p.mac, mac, sizeof(multi_link_host_address_fetch_request_final_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX;

    return true;
}

multi_link_host_address_response_ex_t *multi_link_proto_common_encode_host_address_response_ex(uint8_t (*host_address)[4])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_address_fetch_request_final_ex-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E(Kc-3, F3||Host-Address||Sn)
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], host_address[0], sizeof(host_address[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(host_address[0])], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_address_response_ex_t *multi_link_host_address_response_ex = (multi_link_host_address_response_ex_t *)g_current_packet_buffer;

    multi_link_host_address_response_ex->cmd = GZP_CMD_HOST_ADDRESS_RESPONSE_EX;
    memcpy(&multi_link_host_address_response_ex->payload_p.mac, mac, sizeof(multi_link_host_address_response_ex->payload_p.mac));
    memcpy(&multi_link_host_address_response_ex->payload_p.host_address, host_address[0], sizeof(multi_link_host_address_response_ex->payload_p.host_address));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_response_ex->payload_p, sizeof(multi_link_host_address_response_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_response_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_address_response_ex->payload_p, sizeof(multi_link_host_address_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_response_ex->payload_ep, sizeof(multi_link_host_address_response_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_response_ex, sizeof(multi_link_host_address_response_ex_t), "multi_link_host_address_response_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_address_response_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX;

    return multi_link_host_address_response_ex;
}

bool multi_link_proto_common_decode_host_address_response_ex(multi_link_host_address_response_ex_t *multi_link_host_address_response_ex)
{
    LOG_DBG("-----multi_link_proto_common_decode_host_address_response_ex-----");


    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ADDRESS_FETCH_REQ_FINAL_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_address_response_ex->payload_p, (const uint8_t *)&multi_link_host_address_response_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_address_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_address_response_ex->payload_p, sizeof(multi_link_host_address_response_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_address_response_ex, sizeof(multi_link_host_address_response_ex_t), "multi_link_host_address_response_ex decoded:");

    //
    // Calculate MAC
    // E(Kc-3, F3||Host-Address||Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], multi_link_host_address_response_ex->payload_p.host_address, sizeof(multi_link_host_address_response_ex->payload_p.host_address));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_address_response_ex->payload_p.host_address)], s, sizeof(s));
    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_address_response_ex->payload_p.mac, mac, sizeof(multi_link_host_address_response_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    return true;
}

multi_link_host_id_request_ex_t *multi_link_proto_common_encode_host_id_request_ex(uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_id_request_ex-----");

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_REQ_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E(Kc-3, F3||Device-Id||System-Address-Response||Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], g_challenge_response, sizeof(g_challenge_response));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(g_challenge_response)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_id_request_ex_t *multi_link_host_id_request_ex = (multi_link_host_id_request_ex_t *)g_current_packet_buffer;

    multi_link_host_id_request_ex->cmd = GZP_CMD_HOST_ID_REQ_EX;
    memcpy(&multi_link_host_id_request_ex->payload_p.mac, mac, sizeof(multi_link_host_id_request_ex->payload_p.mac));
    memcpy(&multi_link_host_id_request_ex->payload_p.device_id, device_id[0], sizeof(multi_link_host_id_request_ex->payload_p.device_id));
    memcpy(&multi_link_host_id_request_ex->payload_p.host_address_response, g_challenge_response, sizeof(multi_link_host_id_request_ex->payload_p.host_address_response));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_request_ex->payload_p, sizeof(multi_link_host_id_request_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_request_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_id_request_ex->payload_p, sizeof(multi_link_host_id_request_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_request_ex->payload_ep, sizeof(multi_link_host_id_request_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_request_ex, sizeof(multi_link_host_id_request_ex_t), "multi_link_host_id_request_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_address_request_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_REQ_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_REQ_EX;

    return multi_link_host_id_request_ex;
}

bool multi_link_proto_common_decode_host_id_request_ex(multi_link_host_id_request_ex_t *multi_link_host_id_request_ex)
{
    LOG_DBG("-----multi_link_proto_common_decode_host_id_request_ex-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_REQ_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_request_ex->payload_p, (const uint8_t *)&multi_link_host_id_request_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_id_request_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_request_ex->payload_p, sizeof(multi_link_host_id_request_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_request_ex, sizeof(multi_link_host_id_request_ex_t), "multi_link_host_id_request_ex decoded:");

    //
    // Calculate MAC
    // E(Kc-3, F3||Device-Id||System-Address-Response||Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], multi_link_host_id_request_ex->payload_p.device_id, sizeof(multi_link_host_id_request_ex->payload_p.device_id));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_id_request_ex->payload_p.device_id)], multi_link_host_id_request_ex->payload_p.host_address_response, sizeof(multi_link_host_id_request_ex->payload_p.host_address_response));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_id_request_ex->payload_p.device_id) + sizeof(multi_link_host_id_request_ex->payload_p.host_address_response)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_id_request_ex->payload_p.mac, mac, sizeof(multi_link_host_id_request_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    if (memcmp(multi_link_host_id_request_ex->payload_p.host_address_response, g_challenge_response, sizeof(multi_link_host_id_request_ex->payload_p.host_address_response)) != 0)
    {
        LOG_ERR("response compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_REQ_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_REQ_EX;

    return true;
}

multi_link_host_id_fetch_request_init_ex_t *multi_link_proto_common_encode_host_id_fetch_request_init_ex(uint8_t (*device_id)[3], uint8_t (*device_name)[10])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_id_fetch_request_init_ex-----");

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E( kc-n , F-n||Device-ID||Device-Name||Sn )
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], device_name[0], sizeof(device_name[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(device_name[0])], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_id_fetch_request_init_ex_t *multi_link_host_id_fetch_request_init = (multi_link_host_id_fetch_request_init_ex_t *)g_current_packet_buffer;

    multi_link_host_id_fetch_request_init->cmd = GZP_CMD_HOST_ID_FETCH_REQ_INIT_EX;
    memcpy(&multi_link_host_id_fetch_request_init->payload_p.mac, mac, sizeof(multi_link_host_id_fetch_request_init->payload_p.mac));
    memcpy(&multi_link_host_id_fetch_request_init->payload_p.device_id, device_id[0], sizeof(multi_link_host_id_fetch_request_init->payload_p.device_id));
    memcpy(&multi_link_host_id_fetch_request_init->payload_p.device_name, device_name[0], sizeof(multi_link_host_id_fetch_request_init->payload_p.device_name));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_fetch_request_init->payload_p, sizeof(multi_link_host_id_fetch_request_init->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_fetch_request_init->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_id_fetch_request_init->payload_p, sizeof(multi_link_host_id_fetch_request_init->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_fetch_request_init->payload_ep, sizeof(multi_link_host_id_fetch_request_init->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_fetch_request_init, sizeof(multi_link_host_id_fetch_request_init_ex_t), "multi_link_host_id_fetch_request_init encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_id_fetch_request_init_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT;

    return multi_link_host_id_fetch_request_init;
}

bool multi_link_proto_common_decode_host_id_fetch_request_init_ex(multi_link_host_id_fetch_request_init_ex_t *multi_link_host_id_fetch_request_init)
{
    LOG_DBG("-----multi_link_proto_common_decode_host_id_fetch_request_init_ex-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_fetch_request_init->payload_p, (const uint8_t *)&multi_link_host_id_fetch_request_init->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_id_fetch_request_init->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_fetch_request_init->payload_p, sizeof(multi_link_host_id_fetch_request_init->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_fetch_request_init, sizeof(multi_link_host_id_fetch_request_init_ex_t), "multi_link_host_id_fetch_request_init decoded:");

    //
    // Calculate MAC
    // E( kc-1 , F1||Device-ID||Device-Name||S1 )
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], multi_link_host_id_fetch_request_init->payload_p.device_id, sizeof(multi_link_host_id_fetch_request_init->payload_p.device_id));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_id_fetch_request_init->payload_p.device_id)], multi_link_host_id_fetch_request_init->payload_p.device_name, sizeof(multi_link_host_id_fetch_request_init->payload_p.device_name));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_id_fetch_request_init->payload_p.device_id) + sizeof(multi_link_host_id_fetch_request_init->payload_p.device_name)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_id_fetch_request_init->payload_p.mac, mac, sizeof(multi_link_host_id_fetch_request_init->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT;

    return true;
}

multi_link_host_id_pairing_challenge_ex_t *multi_link_proto_common_encode_host_id_pairing_challenge(uint8_t (*device_id)[3], uint8_t (*dongle_challenge)[5])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_id_pairing_challenge-----");
    
    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    if (g_multi_link_common_callbacks->generate_random(g_salt_challenge, sizeof(g_salt_challenge)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&g_salt_challenge, sizeof(g_salt_challenge), "salt_challenge :");

        memcpy(&dongle_challenge[0], g_salt_challenge, sizeof(g_salt_challenge));
    }
    else
    {
        LOG_ERR("generate_random failed for salt_challenge");
        return 0;
    }

    //
    // Calculate MAC
    // MAC = E( kc-1 , F1||Device-ID||Salt-Challenge||S1 )
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], g_salt_challenge, sizeof(g_salt_challenge));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(g_salt_challenge)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_id_pairing_challenge_ex_t *multi_link_host_id_pairing_challenge = (multi_link_host_id_pairing_challenge_ex_t *)g_current_packet_buffer;

    multi_link_host_id_pairing_challenge->cmd = GZP_CMD_HOST_ID_PAIRING_CHALLENGE_EX;
    memcpy(&multi_link_host_id_pairing_challenge->payload_p.mac, mac, sizeof(multi_link_host_id_pairing_challenge->payload_p.mac));
    memcpy(&multi_link_host_id_pairing_challenge->payload_p.salt_challenge, g_salt_challenge, sizeof(multi_link_host_id_pairing_challenge->payload_p.salt_challenge));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_pairing_challenge->payload_p, sizeof(multi_link_host_id_pairing_challenge->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_pairing_challenge->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_id_pairing_challenge->payload_p, sizeof(multi_link_host_id_pairing_challenge->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_pairing_challenge->payload_ep, sizeof(multi_link_host_id_pairing_challenge->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_pairing_challenge, sizeof(multi_link_host_id_pairing_challenge_ex_t), "multi_link_host_id_pairing_challenge encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_id_pairing_challenge_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT;

    return multi_link_host_id_pairing_challenge;
}

bool multi_link_proto_common_decode_host_id_pairing_challenge(multi_link_host_id_pairing_challenge_ex_t *multi_link_host_id_pairing_challenge, uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_decode_host_id_pairing_challenge-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_REQ_INIT};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_pairing_challenge->payload_p, (const uint8_t *)&multi_link_host_id_pairing_challenge->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_id_pairing_challenge->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_pairing_challenge->payload_p, sizeof(multi_link_host_id_pairing_challenge->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_pairing_challenge, sizeof(multi_link_host_id_pairing_challenge_ex_t), "multi_link_host_id_pairing_challenge decoded:");

    //
    // Calculate MAC
    // MAC = E( kc-1 , F1||Device-ID||Salt-Challenge||S1 )
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], multi_link_host_id_pairing_challenge->payload_p.salt_challenge, sizeof(multi_link_host_id_pairing_challenge->payload_p.salt_challenge));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(multi_link_host_id_pairing_challenge->payload_p.salt_challenge)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_id_pairing_challenge->payload_p.mac, mac, sizeof(multi_link_host_id_pairing_challenge->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    return true;
}

multi_link_host_id_pairing_response_ex_t *multi_link_proto_common_encode_host_id_pairing_response(uint8_t (*device_id)[3], uint8_t (*salt_challenge)[5], uint8_t (*device_response)[5])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_id_pairing_response-----");

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[32];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, 16, "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

     //
    // CRx0= HMAC(kc-2, SHA256(Challenge|| Dev-N|| FS(s2)|| S2-STEP251 ) //32Bytes Output
    // CRx1=E( MSB(CRx0,16), LSB(CRx0, 16),  //16Bytes MSB as Key, 16 Lower bytes as Data)
    // CRx2= E( CRx1, MSB( CRx0,16) )
    // CR=MID(CRx2,4,5)   //Extract 5 Bytes ,from Byte 4 , CR[4]….CR[8]
    // S2-Step251 = LSB4Bytes(Challenge)  Modules 251 .//, 
    // If S2-Step calculation yield zero, Set S2-Step251 =251 
    //


    //
    // S2-Step251 = LSB4Bytes(Challenge)  Modules 251 .//, 
    // If S2-Step calculation yield zero, Set S2-Step251 =251 
    //
    uint32_t s2_step = *((uint32_t*)&salt_challenge[0]);
    uint32_t s2_step_251 = (s2_step % 251);
    if(s2_step_251 == 0)
        s2_step_251 = 251;

    //
    // CRx0= HMAC(kc-2, SHA256(Challenge|| Dev-N|| FS(s2)|| S2-STEP251 ) 
    //
    uint8_t response_p[32];
    uint8_t crx0[32];
    memset(response_p, 0x00, sizeof(response_p));
    memset(crx0, 0x00, sizeof(crx0));
    memcpy(response_p, salt_challenge[0], sizeof(salt_challenge[0]));
    memcpy(&response_p[sizeof(salt_challenge[0])], g_device_nonce, sizeof(g_device_nonce));
    memcpy(&response_p[sizeof(salt_challenge[0]) + sizeof(g_device_nonce)], sf, sizeof(sf));
    memcpy(&response_p[sizeof(salt_challenge[0]) + sizeof(g_device_nonce) + sizeof(sf)], &s2_step_251, sizeof(s2_step_251));
  
    if (g_multi_link_common_callbacks->hmac_sha_256((const uint8_t(*)[sizeof(kc_n)])kc_n, (uint8_t *)&response_p, sizeof(response_p), (uint8_t(*)[sizeof(crx0)]) & crx0))
    {
        uint8_t* crx1 = (uint8_t*)response_p;
        memset(response_p, 0x00, sizeof(response_p));   


        //
        // CRx1=E( MSB(CRx0,16), LSB(CRx0, 16),  //16Bytes MSB as Key, 16 Lower bytes as Data)
        //
        if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])crx0, (uint8_t *) &crx0[16], 16, crx1, 16))
        {
            uint8_t* crx2 = (uint8_t*)&response_p[16];

            //
            // CRx2= E( CRx1, MSB( CRx0,16) )
            //
            if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])crx1, crx0, 16, crx2, 16))
            {
                // 
                // CR=MID(CRx2,4,5)   
                //
                memcpy(g_challenge_response, &crx2[4], 5);
                memcpy(&device_response[0], g_challenge_response, 5);

                LOG_HEXDUMP_DBG((uint8_t *)g_challenge_response, 5, "response :");
            }
            else
            {
                LOG_ERR("ecb_128 failed for response crx2");
                return 0;
            }
        }
        else
        {
            LOG_ERR("ecb_128 failed for response crx1");
            return 0;
        }
    }
    else
    {
        LOG_ERR("hmac_sha_256 failed for response crx0");
        return 0;
    }
   

    // Calculate MAC
    // MAC = E( kc-1 , F||Device-ID||Salt-response||s-n )
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], g_challenge_response, 5);
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + 5], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_id_pairing_response_ex_t *multi_link_host_id_pairing_response_ex = (multi_link_host_id_pairing_response_ex_t *)g_current_packet_buffer;

    multi_link_host_id_pairing_response_ex->cmd = GZP_CMD_HOST_ID_PAIRING_RESPONSE_EX;
    memcpy(&multi_link_host_id_pairing_response_ex->payload_p.mac, mac, sizeof(multi_link_host_id_pairing_response_ex->payload_p.mac));
    memcpy(&multi_link_host_id_pairing_response_ex->payload_p.device_id, device_id[0], sizeof(device_id[0]));
    memcpy(&multi_link_host_id_pairing_response_ex->payload_p.response, g_challenge_response, 5);

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_pairing_response_ex->payload_p, sizeof(multi_link_host_id_pairing_response_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_pairing_response_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_id_pairing_response_ex->payload_p, sizeof(multi_link_host_id_pairing_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_pairing_response_ex->payload_ep, sizeof(multi_link_host_id_pairing_response_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_pairing_response_ex, sizeof(multi_link_host_id_pairing_response_ex_t), "multi_link_host_id_pairing_response_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_id_pairing_response_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX;

    return multi_link_host_id_pairing_response_ex;
}

bool multi_link_proto_common_decode_host_id_pairing_response(multi_link_host_id_pairing_response_ex_t *multi_link_host_id_pairing_response_ex, uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_decode_host_id_pairing_response-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[32];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, 16, "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // CRx0= HMAC(kc-2, SHA256(Challenge|| Dev-N|| FS(s2)|| S2-STEP251 ) //32Bytes Output
    // CRx1=E( MSB(CRx0,16), LSB(CRx0, 16),  //16Bytes MSB as Key, 16 Lower bytes as Data)
    // CRx2= E( CRx1, MSB( CRx0,16) )
    // CR=MID(CRx2,4,5)   //Extract 5 Bytes ,from Byte 4 , CR[4]….CR[8]
    // S2-Step251 = LSB4Bytes(Challenge)  Modules 251 .//, 
    // If S2-Step calculation yield zero, Set S2-Step251 =251 
    //


    //
    // S2-Step251 = LSB4Bytes(Challenge)  Modules 251 .//, 
    // If S2-Step calculation yield zero, Set S2-Step251 =251 
    //
    uint32_t s2_step = *((uint32_t*)&g_salt_challenge[0]);
    uint32_t s2_step_251 = (s2_step % 251);
    if(s2_step_251 == 0)
        s2_step_251 = 251;

    //
    // CRx0= HMAC(kc-2, SHA256(Challenge|| Dev-N|| FS(s2)|| S2-STEP251 ) 
    //
    uint8_t response_p[32];
    uint8_t crx0[32];
    memset(response_p, 0x00, sizeof(response_p));
    memset(crx0, 0x00, sizeof(crx0));
    memcpy(response_p, g_salt_challenge, sizeof(g_salt_challenge));
    memcpy(&response_p[sizeof(g_salt_challenge)], g_device_nonce, sizeof(g_device_nonce));
    memcpy(&response_p[sizeof(g_salt_challenge) + sizeof(g_device_nonce)], sf, sizeof(sf));
    memcpy(&response_p[sizeof(g_salt_challenge) + sizeof(g_device_nonce) + sizeof(sf)], &s2_step_251, sizeof(s2_step_251));
  
    if (g_multi_link_common_callbacks->hmac_sha_256((const uint8_t(*)[sizeof(kc_n)])kc_n, (uint8_t *)&response_p, sizeof(response_p), (uint8_t(*)[sizeof(crx0)]) & crx0))
    {
        uint8_t* crx1 = (uint8_t*)response_p;
        memset(response_p, 0x00, sizeof(response_p));   


        //
        // CRx1=E( MSB(CRx0,16), LSB(CRx0, 16),  //16Bytes MSB as Key, 16 Lower bytes as Data)
        //
        if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])crx0, &crx0[16], 16, crx1, 16))
        {
            uint8_t* crx2 = (uint8_t*)&response_p[16];

            //
            // CRx2= E( CRx1, MSB( CRx0,16) )
            //
            if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])crx1, crx0, 16, crx2, 16))
            {
                // 
                // CR=MID(CRx2,4,5)   
                //
                memcpy(g_challenge_response, &crx2[4], 5);

                LOG_HEXDUMP_DBG((uint8_t *)g_challenge_response, 5, "response :");
            }
            else
            {
                LOG_ERR("ecb_128 failed for response crx2");
                return false;
            }
        }
        else
        {
            LOG_ERR("ecb_128 failed for response crx1");
            return false;
        }
    }
    else
    {
        LOG_ERR("hmac_sha_256 failed for response crx0");
        return false;
    }
   

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_pairing_response_ex->payload_p, (const uint8_t *)&multi_link_host_id_pairing_response_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_id_pairing_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_pairing_response_ex->payload_p, sizeof(multi_link_host_id_pairing_response_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_pairing_response_ex, sizeof(multi_link_host_id_pairing_response_ex_t), "multi_link_host_id_pairing_response_ex decoded:");

    // Calculate MAC
    // MAC = E( kc-1 , F||Device-ID||Salt-response||s-n )
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], multi_link_host_id_pairing_response_ex->payload_p.response, sizeof(multi_link_host_id_pairing_response_ex->payload_p.response));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(multi_link_host_id_pairing_response_ex->payload_p.response)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_id_pairing_response_ex->payload_p.mac, mac, sizeof(multi_link_host_id_pairing_response_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    if (memcmp(multi_link_host_id_pairing_response_ex->payload_p.response, g_challenge_response, sizeof(multi_link_host_id_pairing_response_ex->payload_p.response)) != 0)
    {
        LOG_ERR("response compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_PAIRING_RESPONSE_EX;

    return true;
}

multi_link_host_id_fetch_request_final_ex_t *multi_link_proto_common_encode_host_id_fetch_request_final_ex(uint8_t (*device_id)[3], uint8_t (*device_name)[10])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_id_fetch_request_final_ex-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E( kc-1 , F1||Device-ID||Device-Name||Sn )
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], device_name[0], sizeof(device_name[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(device_name[0])], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_id_fetch_request_final_ex_t *multi_link_host_id_fetch_request_final_ex = (multi_link_host_id_fetch_request_final_ex_t *)g_current_packet_buffer;

    multi_link_host_id_fetch_request_final_ex->cmd = GZP_CMD_HOST_ID_FETCH_REQ_FINAL_EX;
    memcpy(&multi_link_host_id_fetch_request_final_ex->payload_p.mac, mac, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.mac));
    memcpy(&multi_link_host_id_fetch_request_final_ex->payload_p.device_id, device_id[0], sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.device_id));
    memcpy(&multi_link_host_id_fetch_request_final_ex->payload_p.device_name, device_name[0], sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.device_name));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_fetch_request_final_ex->payload_p, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_fetch_request_final_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_id_fetch_request_final_ex->payload_p, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_fetch_request_final_ex->payload_ep, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_fetch_request_final_ex, sizeof(multi_link_host_id_fetch_request_final_ex_t), "multi_link_host_id_fetch_request_final_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_id_fetch_request_final_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX;

    return multi_link_host_id_fetch_request_final_ex;
}

bool multi_link_proto_common_decode_host_id_fetch_request_final_ex(multi_link_host_id_fetch_request_final_ex_t *multi_link_host_id_fetch_request_final_ex)
{
    LOG_DBG("-----multi_link_proto_common_decode_host_id_fetch_request_final_ex-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_fetch_request_final_ex->payload_p, (const uint8_t *)&multi_link_host_id_fetch_request_final_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_fetch_request_final_ex->payload_p, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_fetch_request_final_ex, sizeof(multi_link_host_id_fetch_request_final_ex_t), "multi_link_host_id_fetch_request_final_ex decoded:");

    //
    // Calculate MAC
    // E( kc-1 , F1||Device-ID||Device-Name||Sn )
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], multi_link_host_id_fetch_request_final_ex->payload_p.device_id, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.device_id));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.device_id)], multi_link_host_id_fetch_request_final_ex->payload_p.device_name, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.device_name));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.device_id) + sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.device_name)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_id_fetch_request_final_ex->payload_p.mac, mac, sizeof(multi_link_host_id_fetch_request_final_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX;

    return true;
}

multi_link_host_id_response_ex_t *multi_link_proto_common_encode_host_id_response_ex(uint8_t (*host_id)[5])
{
    LOG_DBG("-----multi_link_proto_common_encode_host_id_fetch_request_final_ex-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E(Kc-3, F3||Host-Id||Sn)
    //
    uint8_t mac[32];
    uint8_t mac_p[32];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], host_id[0], sizeof(host_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(host_id[0])], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_host_id_response_ex_t *multi_link_host_id_response_ex = (multi_link_host_id_response_ex_t *)g_current_packet_buffer;

    multi_link_host_id_response_ex->cmd = GZP_CMD_HOST_ID_RESPONSE_EX;
    memcpy(&multi_link_host_id_response_ex->payload_p.mac, mac, sizeof(multi_link_host_id_response_ex->payload_p.mac));
    memcpy(&multi_link_host_id_response_ex->payload_p.host_id, host_id[0], sizeof(multi_link_host_id_response_ex->payload_p.host_id));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_response_ex->payload_p, sizeof(multi_link_host_id_response_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_response_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_host_id_response_ex->payload_p, sizeof(multi_link_host_id_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_response_ex->payload_ep, sizeof(multi_link_host_id_response_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_response_ex, sizeof(multi_link_host_id_response_ex_t), "multi_link_host_id_response_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_host_id_response_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX;

    return multi_link_host_id_response_ex;
}

bool multi_link_proto_common_decode_host_id_response_ex(multi_link_host_id_response_ex_t *multi_link_host_id_response_ex)
{
    LOG_DBG("-----multi_link_proto_common_decode_host_id_response_ex-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_HOST_ID_FETCH_REQ_FINAL_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_host_id_response_ex->payload_p, (const uint8_t *)&multi_link_host_id_response_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_host_id_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_host_id_response_ex->payload_p, sizeof(multi_link_host_id_response_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_host_id_response_ex, sizeof(multi_link_host_id_response_ex_t), "multi_link_host_id_response_ex decoded:");

    //
    // Calculate MAC
    // E(Kc-3, F3||Host-Id||Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], multi_link_host_id_response_ex->payload_p.host_id, sizeof(multi_link_host_id_response_ex->payload_p.host_id));
    memcpy(&mac_p[sizeof(g_f) + sizeof(multi_link_host_id_response_ex->payload_p.host_id)], s, sizeof(s));
    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_host_id_response_ex->payload_p.mac, mac, sizeof(multi_link_host_id_response_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    return true;
}

multi_link_device_info_ex_t *multi_link_proto_common_encode_device_info_ex(uint8_t (*device_id)[3], uint16_t firmware_version, uint16_t capability, uint16_t kb_layout)
{
    LOG_DBG("-----multi_link_proto_common_encode_device_info_ex-----");

    g_last_state_packet_buffer_length = g_current_packet_buffer_length;
    memcpy(g_last_state_packet_buffer, g_current_packet_buffer, g_last_state_packet_buffer_length);    

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E(Kc-3, F3||Device-Id||firmware_version||capability|kb_layout|Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], &firmware_version, sizeof(firmware_version));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(firmware_version)], &capability, sizeof(capability));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(firmware_version) + sizeof(capability)], &kb_layout, sizeof(kb_layout));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(firmware_version) + sizeof(capability) + sizeof(kb_layout)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_device_info_ex_t *multi_link_device_info_ex = (multi_link_device_info_ex_t *)g_current_packet_buffer;

    multi_link_device_info_ex->cmd = GZP_CMD_DEVICE_INFO_SEND_EX;
    memcpy(&multi_link_device_info_ex->payload_p.mac, mac, sizeof(multi_link_device_info_ex->payload_p.mac));
    memcpy(&multi_link_device_info_ex->payload_p.device_id, device_id[0], sizeof(multi_link_device_info_ex->payload_p.device_id));
    multi_link_device_info_ex->payload_p.capability = capability;
    multi_link_device_info_ex->payload_p.firmware_version = firmware_version;
    multi_link_device_info_ex->payload_p.kb_layout = kb_layout;

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_ex->payload_p, sizeof(multi_link_device_info_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_device_info_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_device_info_ex->payload_p, sizeof(multi_link_device_info_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_ex->payload_ep, sizeof(multi_link_device_info_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_device_info_ex, sizeof(multi_link_device_info_ex_t), "multi_link_device_info_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_device_info_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX;

    return multi_link_device_info_ex;
}

bool multi_link_proto_common_decode_device_info_ex(multi_link_device_info_ex_t *multi_link_device_info_ex, uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_decode_device_info_ex-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_device_info_ex->payload_p, (const uint8_t *)&multi_link_device_info_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_device_info_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_ex->payload_p, sizeof(multi_link_device_info_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_device_info_ex, sizeof(multi_link_device_info_ex_t), "multi_link_device_info_ex decoded:");

    //
    // Calculate MAC
    // E(Kc-3, F3||Device-Id||firmware_version||capability|kb_layout|Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], (const void *)&multi_link_device_info_ex->payload_p.firmware_version, sizeof(multi_link_device_info_ex->payload_p.firmware_version));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(multi_link_device_info_ex->payload_p.firmware_version)], (const void *)&multi_link_device_info_ex->payload_p.capability, sizeof(multi_link_device_info_ex->payload_p.capability));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(multi_link_device_info_ex->payload_p.firmware_version) + sizeof(multi_link_device_info_ex->payload_p.capability)], (const void *)&multi_link_device_info_ex->payload_p.kb_layout, sizeof(multi_link_device_info_ex->payload_p.kb_layout));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(multi_link_device_info_ex->payload_p.firmware_version) + sizeof(multi_link_device_info_ex->payload_p.capability) + sizeof(multi_link_device_info_ex->payload_p.kb_layout)], s, sizeof(s));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_device_info_ex->payload_p.mac, mac, sizeof(multi_link_device_info_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX;

    return true;
}

multi_link_device_info_fetch_response_ex_t *multi_link_proto_common_encode_device_info_fetch_response_ex(uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_encode_device_info_fetch_response_ex-----");

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E(Kc-3, F3||Device-Id||Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_device_info_fetch_response_ex_t *multi_link_device_info_fetch_response_ex = (multi_link_device_info_fetch_response_ex_t *)g_current_packet_buffer;

    multi_link_device_info_fetch_response_ex->cmd = GZP_CMD_DEVICE_INFO_FETCH_RESPONSE_EX;
    memcpy(&multi_link_device_info_fetch_response_ex->payload_p.mac, mac, sizeof(multi_link_device_info_fetch_response_ex->payload_p.mac));
    memcpy(&multi_link_device_info_fetch_response_ex->payload_p.device_id, device_id[0], sizeof(multi_link_device_info_fetch_response_ex->payload_p.device_id));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_fetch_response_ex->payload_p, sizeof(multi_link_device_info_fetch_response_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_device_info_fetch_response_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_device_info_fetch_response_ex->payload_p, sizeof(multi_link_device_info_fetch_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_fetch_response_ex->payload_ep, sizeof(multi_link_device_info_fetch_response_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_device_info_fetch_response_ex, sizeof(multi_link_device_info_fetch_response_ex_t), "multi_link_device_info_fetch_response_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_device_info_fetch_response_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX;

    return multi_link_device_info_fetch_response_ex;
}

bool multi_link_proto_common_decode_device_info_fetch_response_ex(multi_link_device_info_fetch_response_ex_t *multi_link_device_info_fetch_response_ex, uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_decode_device_info_fetch_response_ex-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_device_info_fetch_response_ex->payload_p, (const uint8_t *)&multi_link_device_info_fetch_response_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_device_info_fetch_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_fetch_response_ex->payload_p, sizeof(multi_link_device_info_fetch_response_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_device_info_fetch_response_ex, sizeof(multi_link_device_info_fetch_response_ex_t), "multi_link_device_info_fetch_response_ex decoded:");

    //
    // Calculate MAC
    // E(Kc-3, F3||Device-Id||firmware_version||capability|kb_layout|Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], s, sizeof(s));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_device_info_fetch_response_ex->payload_p.mac, mac, sizeof(multi_link_device_info_fetch_response_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_DEVICE_INFO_SEND_EX;

    return true;
}

multi_link_device_info_response_ex_t *multi_link_proto_common_encode_device_info_response_ex(uint8_t (*device_id)[3], uint8_t result)
{
    LOG_DBG("-----multi_link_proto_common_encode_device_info_response_ex-----");

    memset(g_current_packet_buffer, 0x00, sizeof(g_current_packet_buffer));

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;
    }

    //
    // Calculate MAC
    // E(Kc-3, F3||Device-Id||Result||Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], &result, sizeof(result));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(result)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return 0;
    }

    multi_link_device_info_response_ex_t *multi_link_device_info_response_ex = (multi_link_device_info_response_ex_t *)g_current_packet_buffer;

    multi_link_device_info_response_ex->cmd = GZP_CMD_DEVICE_INFO_RESPONSE_EX;
    memcpy(&multi_link_device_info_response_ex->payload_p.mac, mac, sizeof(multi_link_device_info_response_ex->payload_p.mac));
    memcpy(&multi_link_device_info_response_ex->payload_p.device_id, device_id[0], sizeof(multi_link_device_info_response_ex->payload_p.device_id));
    multi_link_device_info_response_ex->payload_p.result = result;

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_response_ex->payload_p, sizeof(multi_link_device_info_response_ex->payload_p), "payload_p :");

    //
    // Ep = Es XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_device_info_response_ex->payload_ep, (const uint8_t *)es, (const uint8_t *)&multi_link_device_info_response_ex->payload_p, sizeof(multi_link_device_info_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_response_ex->payload_ep, sizeof(multi_link_device_info_response_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_device_info_response_ex, sizeof(multi_link_device_info_response_ex_t), "multi_link_device_info_response_ex encoded :");

    LOG_DBG("------------------");

    g_current_packet_buffer_length = sizeof(multi_link_device_info_response_ex_t);

    //
    // We are in MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX;

    return multi_link_device_info_response_ex;
}

bool multi_link_proto_common_decode_device_info_response_ex(multi_link_device_info_response_ex_t *multi_link_device_info_response_ex, uint8_t (*device_id)[3])
{
    LOG_DBG("-----multi_link_proto_common_decode_device_info_response_ex-----");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(g_f_integer,
                                        1,
                                        &g_f_integer,
                                        &g_f);

    //
    // Calculate SF now
    // SF = E( kc-2,  MG-n||S2||F(s-n) )
    //
    uint8_t sf[16];
    uint8_t sf_p[16];
    uint8_t s[3] = {0x00, 0x00, MULTI_LINK_PARING_STATE_DEVICE_INFO_FETCH_RESPONSE_EX};

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));
    memcpy(sf_p, g_PSEED.MGN, sizeof(g_PSEED.MGN));
    memcpy(&sf_p[sizeof(g_PSEED.MGN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(g_PSEED.MGN) + sizeof(s)], g_f, sizeof(g_f));

    //
    // kc-n = BPMGN||Fn||Sn
    //
    uint8_t kc_n[16];
    memset(kc_n, 0x00, sizeof(kc_n));
    memcpy(kc_n, g_PSEED.BPMGN, sizeof(g_PSEED.BPMGN));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN)], g_f, sizeof(g_f));
    memcpy(&kc_n[sizeof(g_PSEED.BPMGN) + sizeof(g_f)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&kc_n, sizeof(sf_p), "kc_n :");
    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    //
    // Calculate Es
    // Es = Es= E(Kc,  SF )
    //
    uint8_t es[16];
    memset(es, 0x00, sizeof(es));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(g_Kc)])g_Kc, sf, sizeof(sf), es, sizeof(es)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&es, sizeof(es), "es :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for es");
        return false;
    }

    //
    // p = Ep XOR Es
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_device_info_response_ex->payload_p, (const uint8_t *)&multi_link_device_info_response_ex->payload_ep, (const uint8_t *)es, sizeof(multi_link_device_info_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_device_info_response_ex->payload_p, sizeof(multi_link_device_info_response_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_device_info_response_ex, sizeof(multi_link_device_info_response_ex_t), "multi_link_device_info_response_ex decoded:");

    //
    // Calculate MAC
    // E(Kc-3, F3||Device-Id||Result||Sn)
    //
    uint8_t mac[16];
    uint8_t mac_p[16];

    memset(mac, 0x00, sizeof(mac));
    memset(mac_p, 0x00, sizeof(mac_p));
    memcpy(mac_p, g_f, sizeof(g_f));
    memcpy(&mac_p[sizeof(g_f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0])], &multi_link_device_info_response_ex->payload_p.result, sizeof(multi_link_device_info_response_ex->payload_p.result));
    memcpy(&mac_p[sizeof(g_f) + sizeof(device_id[0]) + sizeof(multi_link_device_info_response_ex->payload_p.result)], s, sizeof(s));

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kc_n)])kc_n, mac_p, sizeof(mac_p), mac, sizeof(mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&mac, sizeof(mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return false;
    }

    if (memcmp(multi_link_device_info_response_ex->payload_p.mac, mac, sizeof(multi_link_device_info_response_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    //
    // We are in MULTI_LINK_PARING_STATE_UNKNOWN
    //
    g_current_paring_state = MULTI_LINK_PARING_STATE_UNKNOWN;    

    return true;
}

bool multi_link_proto_common_init_dynamic_key(uint8_t (*host_id)[5], uint8_t (*challenge)[5], uint8_t (*response)[5], multi_link_dynamic_key_info_ex_t *multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_init_dynamic_key-----");

    if (!multi_link_dynamic_key_info)
    {
        LOG_ERR("multi_link_dynamic_key_info is NULL");
        return false;
    }


    multi_link_dynamic_key_info->is_init_done = false;

    //
    // DKeySeed= HMAC( SHA256,HostID||PD1, PD2||ChallengeResponse||Challenge)
    //
    dkeyseed_hmac_p_t dkeyseed_hmac_p;
    uint8_t dkeyseed_temp[32];

    memset(&dkeyseed_hmac_p, 0x00, sizeof(dkeyseed_hmac_p));
    memcpy(dkeyseed_hmac_p.host_id, &host_id[0], sizeof(dkeyseed_hmac_p.host_id));
    memcpy(dkeyseed_hmac_p.PD1, g_PD1, sizeof(g_PD1));
    memcpy(dkeyseed_hmac_p.PD2, g_PD2, sizeof(g_PD2));
    memcpy(dkeyseed_hmac_p.challenge, &challenge[0], sizeof(dkeyseed_hmac_p.challenge));
    memcpy(dkeyseed_hmac_p.response, &response[0], sizeof(dkeyseed_hmac_p.response));

    if (!g_multi_link_common_callbacks->hmac_sha_256((const uint8_t(*)[sizeof(g_Dk_HMAC_KEY)])g_Dk_HMAC_KEY, (uint8_t *)&dkeyseed_hmac_p, sizeof(dkeyseed_hmac_p), (uint8_t(*)[sizeof(dkeyseed_temp)]) & dkeyseed_temp))
    {
        LOG_ERR("hmac_sha_256 failed for DKeySeed");
        return false;
    }

    memcpy(&multi_link_dynamic_key_info->DYMD0, &dkeyseed_temp[21], 10);
    memcpy(&multi_link_dynamic_key_info->MGD0, &dkeyseed_temp[10], 13);
    memcpy(&multi_link_dynamic_key_info->MGDN, &dkeyseed_temp[0], 10);
    memcpy(&multi_link_dynamic_key_info->ack_key, &dkeyseed_temp[5], sizeof(multi_link_dynamic_key_info->ack_key));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->DYMD0, sizeof(multi_link_dynamic_key_info->DYMD0), "DYMD0 :");
    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->MGD0, sizeof(multi_link_dynamic_key_info->MGD0), "MGD0 :");
    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->MGDN, sizeof(multi_link_dynamic_key_info->MGDN), "MGDN :");
    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->ack_key, sizeof(multi_link_dynamic_key_info->ack_key), "ack_key :");


    if (multi_link_dynamic_key_info->nonce_start_value[0] == 0 && multi_link_dynamic_key_info->nonce_start_value[1] == 0 && multi_link_dynamic_key_info->nonce_start_value[2] == 0)
    {
        if (!g_multi_link_common_callbacks->generate_random(&multi_link_dynamic_key_info->nonce_start_value[0], sizeof(multi_link_dynamic_key_info->nonce_start_value)))
        {
            LOG_ERR("nonce_start_value generate_random failed");
            return false;
        }
    }

    LOG_HEXDUMP_DBG(multi_link_dynamic_key_info->nonce_start_value, sizeof(multi_link_dynamic_key_info->nonce_start_value), "nonce_start_value :");

    LOG_DBG("------------------");

    multi_link_dynamic_key_info->is_init_done = true;

    return true;
}


multi_link_dynamic_key_prepare_ex_t* multi_link_proto_common_encode_dynamic_key_prepare_ex(uint8_t (*host_id)[5], uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_encode_dynamic_key_prepare_ex-----");

    if(!multi_link_dynamic_key_info->is_init_done)
        return 0;

    memset(multi_link_dynamic_key_info->current_packet_buffer, 0x00, sizeof(multi_link_dynamic_key_info->current_packet_buffer));

    //
    // F0(NonceStartValue, Step-X away
    //

    uint32_t f0_integer;
    uint8_t f0[3];
    multi_link_proto_common_calculate_f(((uint32_t)multi_link_dynamic_key_info->nonce_start_value[0] << 16 | (uint32_t)multi_link_dynamic_key_info->nonce_start_value[1] << 8 | (uint32_t)multi_link_dynamic_key_info->nonce_start_value[2]),
                                        0,
                                        &f0_integer,
                                        &f0);

    //
    // Kd-0 =  ecb_128(DKeySeed-hmak-key, DYM-D0 || NonceStartValue || f0)
    //

    uint8_t kd0_p[16];
    uint8_t kd0[16];

    memset(kd0, 0x00, sizeof(kd0));
    memcpy(kd0_p, multi_link_dynamic_key_info->DYMD0, sizeof(multi_link_dynamic_key_info->DYMD0));
    memcpy(&kd0_p[sizeof(multi_link_dynamic_key_info->DYMD0)], multi_link_dynamic_key_info->nonce_start_value, sizeof(multi_link_dynamic_key_info->nonce_start_value));
    memcpy(&kd0_p[sizeof(multi_link_dynamic_key_info->DYMD0) + sizeof(multi_link_dynamic_key_info->nonce_start_value)], f0, sizeof(f0));

    LOG_HEXDUMP_DBG((uint8_t *)&kd0_p, sizeof(kd0_p), "kd0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])g_Dk_HMAC_KEY, kd0_p, sizeof(kd0_p), kd0, sizeof(kd0)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&kd0, sizeof(kd0), "kd0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for kd0");
        return 0;
    }

    //
    // SF0 = E(Kd-0, MGD0 || f0  )
    //
    uint8_t sf0[16];
    uint8_t sf0_p[16];

    memset(sf0, 0x00, sizeof(sf0));
    memset(sf0_p, 0x00, sizeof(sf0_p));
    memcpy(sf0_p, multi_link_dynamic_key_info->MGD0, sizeof(multi_link_dynamic_key_info->MGD0));
    memcpy(&sf0_p[sizeof(multi_link_dynamic_key_info->MGD0)], f0, sizeof(f0));

    LOG_HEXDUMP_DBG((uint8_t *)&sf0_p, sizeof(sf0_p), "sf0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kd0)])kd0, sf0_p, sizeof(sf0_p), sf0, sizeof(sf0)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf0, sizeof(sf0), "sf0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf0");
        return 0;
    }


    //
    // SMInit
    // Note : To save static memory we save SMInit in multi_link_dynamic_key_info->f so in S0, SMInit == multi_link_dynamic_key_info->f
    //
    if (!g_multi_link_common_callbacks->generate_random(&multi_link_dynamic_key_info->f[0], sizeof(multi_link_dynamic_key_info->f)))
    {
        LOG_ERR("SMInit generate_random failed");
        return 0;
    }

    multi_link_dynamic_key_info->f_integer = ((uint32_t)multi_link_dynamic_key_info->f[0] << 16 | (uint32_t)multi_link_dynamic_key_info->f[1] << 8 | (uint32_t)multi_link_dynamic_key_info->f[2]);

    LOG_DBG("SMInit = 0x%x", multi_link_dynamic_key_info->f_integer);
    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->f[0], sizeof(multi_link_dynamic_key_info->f), "SMInit :");

    //
    // device_nonce should be randnom not g_f
    //
    if (!g_multi_link_common_callbacks->generate_random(multi_link_dynamic_key_info->device_nonce, sizeof(multi_link_dynamic_key_info->device_nonce)))
    {
        LOG_ERR("device_nonce generate_random failed");
        return 0;
    }
    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->device_nonce[0], sizeof(multi_link_dynamic_key_info->device_nonce), "device_nonce :");


    //
    // Calculate MAC0
    // MAC0 =E( kd-0 , HOSTID||Dev-ID-3||SMInit(3bytes) )
    //
    uint8_t mac0_p[16];

    memset(multi_link_dynamic_key_info->mac, 0x00, sizeof(multi_link_dynamic_key_info->mac));
    memset(mac0_p, 0x00, sizeof(mac0_p));
    memcpy(mac0_p, host_id[0], sizeof(host_id[0]));
    memcpy(&mac0_p[sizeof(host_id[0])], device_id[0], sizeof(device_id[0]));
    memcpy(&mac0_p[sizeof(host_id[0]) + sizeof(device_id[0])], multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));

    LOG_HEXDUMP_DBG((uint8_t *)&mac0_p, sizeof(mac0_p), "mac0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kd0)])kd0, mac0_p, sizeof(mac0_p), (uint8_t*)multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac), "mac0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac0");
        return 0;
    }

    multi_link_dynamic_key_prepare_ex_t *multi_link_dynamic_key_prepare_ex = (multi_link_dynamic_key_prepare_ex_t *)multi_link_dynamic_key_info->current_packet_buffer;

    multi_link_dynamic_key_prepare_ex->cmd = GZP_CMD_DYNAMIC_KEY_PREPARE_EX;
    memcpy(&multi_link_dynamic_key_prepare_ex->device_nonce_start_value, multi_link_dynamic_key_info->nonce_start_value, sizeof(multi_link_dynamic_key_info->nonce_start_value));

    memcpy(&multi_link_dynamic_key_prepare_ex->payload_p.mac, multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_prepare_ex->payload_p.mac));
    memcpy(&multi_link_dynamic_key_prepare_ex->payload_p.sminit, multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_prepare_ex->payload_p.sminit));
    memcpy(&multi_link_dynamic_key_prepare_ex->payload_p.device_nonce, multi_link_dynamic_key_info->device_nonce, sizeof(multi_link_dynamic_key_prepare_ex->payload_p.device_nonce));
    memcpy(&multi_link_dynamic_key_prepare_ex->payload_p.device_id, device_id[0], sizeof(multi_link_dynamic_key_prepare_ex->payload_p.device_id));
    multi_link_dynamic_key_prepare_ex->payload_p.api_version = MULTI_LINK_API_VERSION;

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_prepare_ex->payload_p, sizeof(multi_link_dynamic_key_prepare_ex->payload_p), "payload_p :");

    //
    // Ep = SF0 XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_dynamic_key_prepare_ex->payload_ep, (const uint8_t *)sf0, (const uint8_t *)&multi_link_dynamic_key_prepare_ex->payload_p, sizeof(multi_link_dynamic_key_prepare_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_prepare_ex->payload_ep, sizeof(multi_link_dynamic_key_prepare_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_dynamic_key_prepare_ex, sizeof(multi_link_dynamic_key_prepare_ex), "multi_link_dynamic_key_prepare_ex encoded :");

    LOG_DBG("------------------");

    multi_link_dynamic_key_info->current_packet_buffer_length = sizeof(multi_link_dynamic_key_prepare_ex_t);

    multi_link_dynamic_key_info->state = 0;

    return multi_link_dynamic_key_prepare_ex;
}

bool multi_link_proto_common_decode_dynamic_key_prepare_ex(multi_link_dynamic_key_prepare_ex_t* multi_link_dynamic_key_prepare_ex, uint8_t (*host_id)[5], uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_decode_dynamic_key_prepare_ex-----");

    if(!multi_link_dynamic_key_info->is_init_done)
        return 0;


    //
    // F0(NonceStartValue, Step-X away
    //

    uint32_t f0_integer;
    uint8_t f0[3];
    multi_link_proto_common_calculate_f(((uint32_t)multi_link_dynamic_key_info->nonce_start_value[0] << 16 | (uint32_t)multi_link_dynamic_key_info->nonce_start_value[1] << 8 | (uint32_t)multi_link_dynamic_key_info->nonce_start_value[2]),
                                        0,
                                        &f0_integer,
                                        &f0);


    //
    // Kd-0 =  ecb_128(DKeySeed-hmak-key, DYM-D0 || NonceStartValue || f0)
    //

    uint8_t kd0_p[16];
    uint8_t kd0[16];

    memset(kd0, 0x00, sizeof(kd0));
    memcpy(kd0_p, multi_link_dynamic_key_info->DYMD0, sizeof(multi_link_dynamic_key_info->DYMD0));
    memcpy(&kd0_p[sizeof(multi_link_dynamic_key_info->DYMD0)], multi_link_dynamic_key_info->nonce_start_value, sizeof(multi_link_dynamic_key_info->nonce_start_value));
    memcpy(&kd0_p[sizeof(multi_link_dynamic_key_info->DYMD0) + sizeof(multi_link_dynamic_key_info->nonce_start_value)], f0, sizeof(f0));

    LOG_HEXDUMP_DBG((uint8_t *)&kd0_p, sizeof(kd0_p), "kd0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[16])g_Dk_HMAC_KEY, kd0_p, sizeof(kd0_p), kd0, sizeof(kd0)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&kd0, sizeof(kd0), "kd0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for kd0");
        return false;
    }

    //
    // SF0 = E(Kd-0, MGD0 || f0  )
    //
    uint8_t sf0[16];
    uint8_t sf0_p[16];

    memset(sf0, 0x00, sizeof(sf0));
    memset(sf0_p, 0x00, sizeof(sf0_p));
    memcpy(sf0_p, multi_link_dynamic_key_info->MGD0, sizeof(multi_link_dynamic_key_info->MGD0));
    memcpy(&sf0_p[sizeof(multi_link_dynamic_key_info->MGD0)], f0, sizeof(f0));

    LOG_HEXDUMP_DBG((uint8_t *)&sf0_p, sizeof(sf0_p), "sf0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kd0)])kd0, sf0_p, sizeof(sf0_p), sf0, sizeof(sf0)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf0, sizeof(sf0), "sf0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf0");
        return false;
    }

    //
    // p =  ep  XOR SF0
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_dynamic_key_prepare_ex->payload_p, (const uint8_t *)&multi_link_dynamic_key_prepare_ex->payload_ep, (const uint8_t *)sf0, sizeof(multi_link_dynamic_key_prepare_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_prepare_ex->payload_p, sizeof(multi_link_dynamic_key_prepare_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_dynamic_key_prepare_ex, sizeof(multi_link_dynamic_key_prepare_ex_t), "multi_link_dynamic_key_prepare_ex decoded:");


    //
    // SMInit
    // Note : To save static memory we save SMInit in multi_link_dynamic_key_info->f so in S0, SMInit == multi_link_dynamic_key_info->f
    //
    memcpy(multi_link_dynamic_key_info->f, multi_link_dynamic_key_prepare_ex->payload_p.sminit, sizeof(multi_link_dynamic_key_info->f) );
    multi_link_dynamic_key_info->f_integer = ((uint32_t)multi_link_dynamic_key_info->f[0] << 16 | (uint32_t)multi_link_dynamic_key_info->f[1] << 8 | (uint32_t)multi_link_dynamic_key_info->f[2]);

    LOG_DBG("SMInit = 0x%x", multi_link_dynamic_key_info->f_integer);
    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->f[0], sizeof(multi_link_dynamic_key_info->f), "SMInit :");

    //
    // device_nonce should be randnom not g_f
    //

    memcpy(multi_link_dynamic_key_info->device_nonce, multi_link_dynamic_key_prepare_ex->payload_p.device_nonce, sizeof(multi_link_dynamic_key_info->device_nonce) );

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->device_nonce[0], sizeof(multi_link_dynamic_key_info->device_nonce), "device_nonce :");


    //
    // Calculate MAC0
    // MAC0 =E( kd-0 , HOSTID||Dev-ID-3||SMInit(3bytes) )
    //
    uint8_t mac0_p[16];

    memset(multi_link_dynamic_key_info->mac, 0x00, sizeof(multi_link_dynamic_key_info->mac));
    memset(mac0_p, 0x00, sizeof(mac0_p));
    memcpy(mac0_p, host_id[0], sizeof(host_id[0]));
    memcpy(&mac0_p[sizeof(host_id[0])], device_id[0], sizeof(device_id[0]));
    memcpy(&mac0_p[sizeof(host_id[0]) + sizeof(device_id[0])], multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));

    LOG_HEXDUMP_DBG((uint8_t *)&mac0_p, sizeof(mac0_p), "mac0_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kd0)])kd0, mac0_p, sizeof(mac0_p), (uint8_t*)multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac), "mac0 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac0");
        return false;
    }

    if (memcmp(multi_link_dynamic_key_prepare_ex->payload_p.mac, multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_prepare_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }


    LOG_DBG("------------------");

    multi_link_dynamic_key_info->api_version = multi_link_dynamic_key_prepare_ex->payload_p.api_version;

    multi_link_dynamic_key_info->state = 0;


    return true;;

}

multi_link_dynamic_key_fetch_ex_t* multi_link_proto_common_encode_dynamic_key_fetch_ex(uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_encode_dynamic_key_fetch_ex-----");

    if(!multi_link_dynamic_key_info->is_init_done)
        return 0;


    memset(multi_link_dynamic_key_info->current_packet_buffer, 0x00, sizeof(multi_link_dynamic_key_info->current_packet_buffer));

    multi_link_dynamic_key_info->state++;

    uint8_t s[3];

    s[2] = (multi_link_dynamic_key_info->state & 0xff);
    s[1] = ((multi_link_dynamic_key_info->state >> 8) & 0xff);
    s[0] = ((multi_link_dynamic_key_info->state >> 16) & 0xff);

    LOG_HEXDUMP_DBG((uint8_t *)&s, sizeof(s), "s :");

    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(multi_link_dynamic_key_info->f_integer,
                                        1,
                                        (uint32_t *)&multi_link_dynamic_key_info->f_integer,
                                        (uint8_t (*)[3])&multi_link_dynamic_key_info->f);


    //
    // SF-1 = E( MAC0,    MGDn ||S1||F(S1) )  ,  //F(S1)=>StateMachine value 1 step advance from FS0
    //

    uint8_t sf1[16];
    uint8_t sf1_p[16];

    memset(sf1, 0x00, sizeof(sf1));
    memset(sf1_p, 0x00, sizeof(sf1_p));
    memcpy(sf1_p, multi_link_dynamic_key_info->MGDN, sizeof(multi_link_dynamic_key_info->MGDN));
    memcpy(&sf1_p[sizeof(multi_link_dynamic_key_info->MGDN)], s, sizeof(s));
    memcpy(&sf1_p[sizeof(multi_link_dynamic_key_info->MGDN) + sizeof(s)], multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));

    LOG_HEXDUMP_DBG((uint8_t *)&sf1_p, sizeof(sf1_p), "sf1_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(multi_link_dynamic_key_info->mac)])multi_link_dynamic_key_info->mac, sf1_p, sizeof(sf1_p), sf1, sizeof(sf1)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf1, sizeof(sf1), "sf1 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf1");
        return 0;
    }


    //
    // Calculate MAC1
    // MAC1 =E( MAC0 , F(S1)-3 ||Dev-ID-3|Dev-N-5|||S1-3  )
    //
    uint8_t mac1_p[16];

    memset(multi_link_dynamic_key_info->mac, 0x00, sizeof(multi_link_dynamic_key_info->mac));
    memset(mac1_p, 0x00, sizeof(mac1_p));
    memcpy(mac1_p, multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f) + sizeof(device_id[0])], multi_link_dynamic_key_info->device_nonce, sizeof(multi_link_dynamic_key_info->device_nonce));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f) + sizeof(multi_link_dynamic_key_info->f) + sizeof(multi_link_dynamic_key_info->device_nonce)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac1_p, sizeof(mac1_p), "mac1_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(multi_link_dynamic_key_info->mac)])multi_link_dynamic_key_info->mac, mac1_p, sizeof(mac1_p), (uint8_t*)multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac), "mac1 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac1");
        return 0;
    }

    multi_link_dynamic_key_fetch_ex_t *multi_link_dynamic_key_fetch_ex = (multi_link_dynamic_key_fetch_ex_t *)multi_link_dynamic_key_info->current_packet_buffer;

    multi_link_dynamic_key_fetch_ex->cmd = GZP_CMD_DYNAMIC_KEY_FETCH_EX;
    memcpy(&multi_link_dynamic_key_fetch_ex->device_nonce_start_value, multi_link_dynamic_key_info->nonce_start_value, sizeof(multi_link_dynamic_key_fetch_ex->device_nonce_start_value));

    memcpy(&multi_link_dynamic_key_fetch_ex->payload_p.mac, multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_fetch_ex->payload_p.mac));
    memcpy(&multi_link_dynamic_key_fetch_ex->payload_p.device_nonce, multi_link_dynamic_key_info->device_nonce, sizeof(multi_link_dynamic_key_fetch_ex->payload_p.device_nonce));
    memcpy(&multi_link_dynamic_key_fetch_ex->payload_p.device_id, device_id[0], sizeof(multi_link_dynamic_key_fetch_ex->payload_p.device_id));


    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_fetch_ex->payload_p, sizeof(multi_link_dynamic_key_fetch_ex->payload_p), "payload_p :");

    //
    // Ep = SF1 XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_dynamic_key_fetch_ex->payload_ep, (const uint8_t *)sf1, (const uint8_t *)&multi_link_dynamic_key_fetch_ex->payload_p, sizeof(multi_link_dynamic_key_fetch_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_fetch_ex->payload_ep, sizeof(multi_link_dynamic_key_fetch_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_dynamic_key_fetch_ex, sizeof(multi_link_dynamic_key_fetch_ex_t), "multi_link_dynamic_key_fetch_ex encoded :");


    LOG_DBG("------------------");

    multi_link_dynamic_key_info->current_packet_buffer_length = sizeof(multi_link_dynamic_key_fetch_ex_t);


    return multi_link_dynamic_key_fetch_ex;


}

bool multi_link_proto_common_decode_dynamic_key_fetch_ex(multi_link_dynamic_key_fetch_ex_t* multi_link_dynamic_key_fetch_ex, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_decode_dynamic_key_fetch_ex-----");

    if(!multi_link_dynamic_key_info->is_init_done)
        return false;

    multi_link_dynamic_key_info->state++;

    uint8_t s[3];

    s[2] = (multi_link_dynamic_key_info->state & 0xff);
    s[1] = ((multi_link_dynamic_key_info->state >> 8) & 0xff);
    s[0] = ((multi_link_dynamic_key_info->state >> 16) & 0xff);

    LOG_HEXDUMP_DBG((uint8_t *)&s, sizeof(s), "s :");


    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(multi_link_dynamic_key_info->f_integer,
                                        1,
                                        (uint32_t *)&multi_link_dynamic_key_info->f_integer,
                                        (uint8_t (*)[3])&multi_link_dynamic_key_info->f);



    //
    // SF-1 = E( MAC0,    MGDn ||S1||F(S1) )  ,  //F(S1)=>StateMachine value 1 step advance from FS0
    //

    uint8_t sf1[16];
    uint8_t sf1_p[16];

    memset(sf1, 0x00, sizeof(sf1));
    memset(sf1_p, 0x00, sizeof(sf1_p));
    memcpy(sf1_p, multi_link_dynamic_key_info->MGDN, sizeof(multi_link_dynamic_key_info->MGDN));
    memcpy(&sf1_p[sizeof(multi_link_dynamic_key_info->MGDN)], s, sizeof(s));
    memcpy(&sf1_p[sizeof(multi_link_dynamic_key_info->MGDN) + sizeof(s)], multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));

    LOG_HEXDUMP_DBG((uint8_t *)&sf1_p, sizeof(sf1_p), "sf1_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(multi_link_dynamic_key_info->mac)])multi_link_dynamic_key_info->mac, sf1_p, sizeof(sf1_p), sf1, sizeof(sf1)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf1, sizeof(sf1), "sf1 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf1");
        return false;
    }

    //
    // p =  ep  XOR SF1
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_dynamic_key_fetch_ex->payload_p, (const uint8_t *)&multi_link_dynamic_key_fetch_ex->payload_ep, (const uint8_t *)sf1, sizeof(multi_link_dynamic_key_fetch_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_fetch_ex->payload_p, sizeof(multi_link_dynamic_key_fetch_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_dynamic_key_fetch_ex, sizeof(multi_link_dynamic_key_fetch_ex_t), "multi_link_dynamic_key_fetch_ex decoded:");

    //
    // Calculate MAC1
    // MAC1 =E( MAC0 , F(S1)-3 ||Dev-ID-3|Dev-N-5|||S1-3  )
    //
    uint8_t mac1_p[16];

    memset(multi_link_dynamic_key_info->mac, 0x00, sizeof(multi_link_dynamic_key_info->mac));
    memset(mac1_p, 0x00, sizeof(mac1_p));
    memcpy(mac1_p, multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f) + sizeof(device_id[0])], multi_link_dynamic_key_info->device_nonce, sizeof(multi_link_dynamic_key_info->device_nonce));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f) + sizeof(device_id[0]) + sizeof(multi_link_dynamic_key_info->device_nonce)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac1_p, sizeof(mac1_p), "mac1_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(multi_link_dynamic_key_info->mac)])multi_link_dynamic_key_info->mac, mac1_p, sizeof(mac1_p), (uint8_t*)multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac), "mac1 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac1");
        return false;
    }

    if (memcmp(multi_link_dynamic_key_fetch_ex->payload_p.mac, multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_fetch_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    LOG_DBG("------------------");

    return true;

}

multi_link_dynamic_key_response_ex_t* multi_link_proto_common_encode_dynamic_key_response_ex(uint8_t cmd_data, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_encode_dynamic_key_response_ex-----");

    if(!multi_link_dynamic_key_info->is_init_done)
        return 0;


    memset(multi_link_dynamic_key_info->current_packet_buffer, 0x00, sizeof(multi_link_dynamic_key_info->current_packet_buffer));

    uint8_t s[3];

    s[2] = (multi_link_dynamic_key_info->state & 0xff);
    s[1] = ((multi_link_dynamic_key_info->state >> 8) & 0xff);
    s[0] = ((multi_link_dynamic_key_info->state >> 16) & 0xff);

    LOG_HEXDUMP_DBG((uint8_t *)&s, sizeof(s), "s :");


    if (!g_multi_link_common_callbacks->generate_random(multi_link_dynamic_key_info->dongle_nonce, sizeof(multi_link_dynamic_key_info->dongle_nonce)))
    {
        LOG_ERR("dongle_nonce generate_random failed");
        return 0;
    }
    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->dongle_nonce[0], sizeof(multi_link_dynamic_key_info->dongle_nonce), "dongle_nonce :");

    //
    // SF-1 = E( MAC0,    MGDn ||S1||F(S1) )  ,  //F(S1)=>StateMachine value
    //

    uint8_t sf1[16];
    uint8_t sf1_p[16];

    memset(sf1, 0x00, sizeof(sf1));
    memset(sf1_p, 0x00, sizeof(sf1_p));
    memcpy(sf1_p, multi_link_dynamic_key_info->MGDN, sizeof(multi_link_dynamic_key_info->MGDN));
    memcpy(&sf1_p[sizeof(multi_link_dynamic_key_info->MGDN)], s, sizeof(s));
    memcpy(&sf1_p[sizeof(multi_link_dynamic_key_info->MGDN) + sizeof(s)], multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));

    LOG_HEXDUMP_DBG((uint8_t *)&sf1_p, sizeof(sf1_p), "sf1_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(multi_link_dynamic_key_info->mac)])multi_link_dynamic_key_info->mac, sf1_p, sizeof(sf1_p), sf1, sizeof(sf1)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf1, sizeof(sf1), "sf1 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf1");
        return 0;
    }


    //
    // Calculate MAC1
    // MAC1 =E( MAC0 , F(S1)-3 ||Dev-ID-3|Dev-N-5|||S1-3  )
    //
    uint8_t mac1_p[16];

    memset(multi_link_dynamic_key_info->mac, 0x00, sizeof(multi_link_dynamic_key_info->mac));
    memset(mac1_p, 0x00, sizeof(mac1_p));
    memcpy(mac1_p, multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f) + sizeof(device_id[0])], multi_link_dynamic_key_info->dongle_nonce, sizeof(multi_link_dynamic_key_info->dongle_nonce));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f) + sizeof(multi_link_dynamic_key_info->f) + sizeof(multi_link_dynamic_key_info->dongle_nonce)], s, sizeof(s));
    

    LOG_HEXDUMP_DBG((uint8_t *)&mac1_p, sizeof(mac1_p), "mac1_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(multi_link_dynamic_key_info->mac)])multi_link_dynamic_key_info->mac, mac1_p, sizeof(mac1_p), (uint8_t*)multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac), "mac1 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac1");
        return 0;
    }

    multi_link_dynamic_key_response_ex_t *multi_link_dynamic_key_response_ex = (multi_link_dynamic_key_response_ex_t *)multi_link_dynamic_key_info->current_packet_buffer;

    multi_link_dynamic_key_response_ex->cmd = GZP_CMD_DYNAMIC_KEY_RESPONSE_EX;

    memcpy(&multi_link_dynamic_key_response_ex->payload_p.mac, multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_response_ex->payload_p.mac));
    memcpy(&multi_link_dynamic_key_response_ex->payload_p.dongle_nonce, multi_link_dynamic_key_info->dongle_nonce, sizeof(multi_link_dynamic_key_response_ex->payload_p.dongle_nonce));
    multi_link_dynamic_key_response_ex->payload_p.cmd_data = cmd_data;
    multi_link_dynamic_key_response_ex->payload_p.api_version = MULTI_LINK_API_VERSION;


    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_response_ex->payload_p, sizeof(multi_link_dynamic_key_response_ex->payload_p), "payload_p :");

    //
    // Ep = SF1 XOR p
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_dynamic_key_response_ex->payload_ep, (const uint8_t *)sf1, (const uint8_t *)&multi_link_dynamic_key_response_ex->payload_p, sizeof(multi_link_dynamic_key_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_response_ex->payload_ep, sizeof(multi_link_dynamic_key_response_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_dynamic_key_response_ex, sizeof(multi_link_dynamic_key_response_ex_t), "multi_link_dynamic_key_response_ex encoded :");


    LOG_DBG("------------------");

    multi_link_dynamic_key_info->cmd_data = cmd_data;
    multi_link_dynamic_key_info->current_packet_buffer_length = sizeof(multi_link_dynamic_key_response_ex_t);


    return multi_link_dynamic_key_response_ex;


}

bool multi_link_proto_common_decode_dynamic_key_response_ex(multi_link_dynamic_key_response_ex_t* multi_link_dynamic_key_response_ex, uint8_t (*device_id)[3], multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_decode_dynamic_key_response_ex-----");

    if(!multi_link_dynamic_key_info->is_init_done)
        return false;

    uint8_t s[3];

    s[2] = (multi_link_dynamic_key_info->state & 0xff);
    s[1] = ((multi_link_dynamic_key_info->state >> 8) & 0xff);
    s[0] = ((multi_link_dynamic_key_info->state >> 16) & 0xff);

    LOG_HEXDUMP_DBG((uint8_t *)&s, sizeof(s), "s :");


    //
    // SF-1 = E( MAC0,    MGDn ||S1||F(S1) )  ,  //F(S1)=>StateMachine value
    //

    uint8_t sf1[16];
    uint8_t sf1_p[16];

    memset(sf1, 0x00, sizeof(sf1));
    memset(sf1_p, 0x00, sizeof(sf1_p));
    memcpy(sf1_p, multi_link_dynamic_key_info->MGDN, sizeof(multi_link_dynamic_key_info->MGDN));
    memcpy(&sf1_p[sizeof(multi_link_dynamic_key_info->MGDN)], s, sizeof(s));
    memcpy(&sf1_p[sizeof(multi_link_dynamic_key_info->MGDN) + sizeof(s)], multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));

    LOG_HEXDUMP_DBG((uint8_t *)&sf1_p, sizeof(sf1_p), "sf1_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(multi_link_dynamic_key_info->mac)])multi_link_dynamic_key_info->mac, sf1_p, sizeof(sf1_p), sf1, sizeof(sf1)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf1, sizeof(sf1), "sf1 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf1");
        return false;
    }

    //
    // p =  ep  XOR SF1
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_dynamic_key_response_ex->payload_p, (const uint8_t *)&multi_link_dynamic_key_response_ex->payload_ep, (const uint8_t *)sf1, sizeof(multi_link_dynamic_key_response_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_response_ex->payload_p, sizeof(multi_link_dynamic_key_response_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_dynamic_key_response_ex, sizeof(multi_link_dynamic_key_response_ex_t), "multi_link_dynamic_key_response_ex decoded:");

    //
    // Calculate MAC1
    // MAC1 =E( MAC0 , F(S1)-3 ||Dev-ID-3|Dev-N-5|||S1-3  )
    //
    uint8_t mac1_p[16];

    memset(multi_link_dynamic_key_info->mac, 0x00, sizeof(multi_link_dynamic_key_info->mac));
    memset(mac1_p, 0x00, sizeof(mac1_p));
    memcpy(mac1_p, multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f)], device_id[0], sizeof(device_id[0]));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f) + sizeof(device_id[0])], multi_link_dynamic_key_response_ex->payload_p.dongle_nonce, sizeof(multi_link_dynamic_key_response_ex->payload_p.dongle_nonce));
    memcpy(&mac1_p[sizeof(multi_link_dynamic_key_info->f) + sizeof(device_id[0]) + sizeof(multi_link_dynamic_key_response_ex->payload_p.dongle_nonce)], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac1_p, sizeof(mac1_p), "mac1_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(multi_link_dynamic_key_info->mac)])multi_link_dynamic_key_info->mac, mac1_p, sizeof(mac1_p), (uint8_t*)multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac), "mac1 :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac1");
        return 0;
    }

    if (memcmp(multi_link_dynamic_key_response_ex->payload_p.mac, multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_response_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return false;
    }

    memcpy(&multi_link_dynamic_key_info->dongle_nonce,&multi_link_dynamic_key_response_ex->payload_p.dongle_nonce, sizeof(multi_link_dynamic_key_info->dongle_nonce) );
    multi_link_dynamic_key_info->cmd_data = multi_link_dynamic_key_response_ex->payload_p.cmd_data;
    multi_link_dynamic_key_info->api_version = multi_link_dynamic_key_response_ex->payload_p.api_version;

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->dongle_nonce[0], sizeof(multi_link_dynamic_key_info->dongle_nonce), "dongle_nonce :");

    LOG_DBG("cmd_data = 0x%x", multi_link_dynamic_key_info->cmd_data);

    LOG_DBG("api_version = 0x%x", multi_link_dynamic_key_info->api_version);

    LOG_DBG("------------------");

    return true;
}

multi_link_data_ex_t* multi_link_proto_common_encode_data_ex(uint8_t * data, uint32_t data_length, multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_encode_data_ex-----");


   if(!multi_link_dynamic_key_info->is_dynamic_key_done)
        return 0;

   if (data_length > FIELD_SIZEOF(multi_link_data_ex_t,payload_p.data))
        return 0;


    memset(multi_link_dynamic_key_info->current_packet_buffer, 0x00, sizeof(multi_link_dynamic_key_info->current_packet_buffer));

    multi_link_dynamic_key_info->state++;

    if(multi_link_dynamic_key_info->state > 0xFFFFFF)
    {
        multi_link_dynamic_key_info->state = 3;
    }


    uint8_t s[3];

    s[2] = (multi_link_dynamic_key_info->state & 0xff);
    s[1] = ((multi_link_dynamic_key_info->state >> 8) & 0xff);
    s[0] = ((multi_link_dynamic_key_info->state >> 16) & 0xff);

    LOG_HEXDUMP_DBG((uint8_t *)&s, sizeof(s), "s :");


    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(multi_link_dynamic_key_info->f_integer,
                                        1,
                                        (uint32_t *)&multi_link_dynamic_key_info->f_integer,
                                        (uint8_t (*)[3])&multi_link_dynamic_key_info->f);

    
    uint8_t kd[16];
    uint8_t pmac[FIELD_SIZEOF(multi_link_data_ex_t,payload_p.data)];

    memset(kd, 0x00, sizeof(kd));
    memcpy(pmac, &multi_link_dynamic_key_info->mac[3], sizeof(pmac));

    if(multi_link_dynamic_key_info->state == 2)
    {
        //
        // Kd2=MID(MAC1, MSB,6)||Dev-N-5||Don-N-5 //6-5-5 byte sequence
        //
        memcpy(kd, multi_link_dynamic_key_info->mac, 6);
        memcpy(&kd[6], multi_link_dynamic_key_info->device_nonce, sizeof(multi_link_dynamic_key_info->device_nonce));
        memcpy(&kd[6 + sizeof(multi_link_dynamic_key_info->device_nonce)], multi_link_dynamic_key_info->dongle_nonce, sizeof(multi_link_dynamic_key_info->dongle_nonce));

        LOG_HEXDUMP_DBG((uint8_t *)&kd, sizeof(kd), "kd2 :");
    }
    else
    {
        //
        // Kd-n= MACn-1  // Use previous MAC output as Key for next Input Stream Function.
        //
        memcpy(kd, multi_link_dynamic_key_info->mac, sizeof(kd) );

        LOG_HEXDUMP_DBG((uint8_t *)&kd, sizeof(kd), "kdn :");
    }

    uint8_t sf[16];
    uint8_t sf_p[16];

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));

    memcpy(sf_p, multi_link_dynamic_key_info->MGDN, sizeof(multi_link_dynamic_key_info->MGDN));
    memcpy(&sf_p[sizeof(multi_link_dynamic_key_info->MGDN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(multi_link_dynamic_key_info->MGDN) + sizeof(s)], multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));

    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");


    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kd)])kd, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return 0;

    }

    memcpy(&multi_link_dynamic_key_info->MGDN[2], &sf[8], 8);

    //
    // Calculate MAC1
    // E( kd, f(Sn)|| Dev-ID||Data || Sn ) -> in state > kd == MACn-1 see above
    //
    uint8_t mac_p[16];

    memset(multi_link_dynamic_key_info->mac, 0x00, sizeof(multi_link_dynamic_key_info->mac));
    memset(mac_p, 0x00, sizeof(mac_p));

    memcpy(mac_p, multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));
    memcpy(&mac_p[sizeof(multi_link_dynamic_key_info->f)], data, MIN(10,data_length));
    memcpy(&mac_p[sizeof(multi_link_dynamic_key_info->f) + 10], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kd)])kd, mac_p, sizeof(mac_p), (uint8_t*)multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    multi_link_data_ex_t *multi_link_data_ex = (multi_link_data_ex_t *)multi_link_dynamic_key_info->current_packet_buffer;

    multi_link_data_ex->cmd = multi_link_dynamic_key_info->cmd_data;

    memcpy(&multi_link_data_ex->payload_p.mac, multi_link_dynamic_key_info->mac, sizeof(multi_link_data_ex->payload_p.mac));
    memcpy(&multi_link_data_ex->payload_p.data, data, data_length);

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_data_ex->payload_p, sizeof(multi_link_data_ex->payload_p), "payload_p :");

    //
    // Ep = SF XOR (p XOR PMAC) 
    //
    multi_link_proto_common_xor((uint8_t *)&multi_link_data_ex->payload_p.data, (const uint8_t *)&multi_link_data_ex->payload_p.data, (const uint8_t *)pmac, sizeof(multi_link_data_ex->payload_p.data));
    multi_link_proto_common_xor((uint8_t *)&multi_link_data_ex->payload_ep, (const uint8_t *)sf, (const uint8_t *)&multi_link_data_ex->payload_p, sizeof(multi_link_data_ex->payload_p));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_data_ex->payload_ep, sizeof(multi_link_data_ex->payload_p), "payload_ep :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_data_ex, sizeof(multi_link_data_ex_t), "multi_link_data_ex encoded :");

    multi_link_dynamic_key_info->current_packet_buffer_length = sizeof(multi_link_data_ex_t);


    LOG_DBG("------------------");

    return multi_link_data_ex;

}

bool multi_link_proto_common_decode_data_ex(multi_link_data_ex_t* multi_link_data_ex, multi_link_dynamic_key_info_ex_t* multi_link_dynamic_key_info)
{
    LOG_DBG("-----multi_link_proto_common_decode_data_ex-----");


    if(!multi_link_dynamic_key_info->is_dynamic_key_done)
        return false;

    multi_link_dynamic_key_info->state++;

    if(multi_link_dynamic_key_info->state > 0xFFFFFF)
    {
        multi_link_dynamic_key_info->state = 3;
    }

    uint8_t s[3];

    s[2] = (multi_link_dynamic_key_info->state & 0xff);
    s[1] = ((multi_link_dynamic_key_info->state >> 8) & 0xff);
    s[0] = ((multi_link_dynamic_key_info->state >> 16) & 0xff);

    LOG_HEXDUMP_DBG((uint8_t *)&s, sizeof(s), "s :");


    //
    // F(s-n, Step=1, Count) , s-n is last f step 1
    //
    multi_link_proto_common_calculate_f(multi_link_dynamic_key_info->f_integer,
                                        1,
                                        (uint32_t *)&multi_link_dynamic_key_info->f_integer,
                                        (uint8_t (*)[3])&multi_link_dynamic_key_info->f);

   
    uint8_t kd[16];
    uint8_t pmac[FIELD_SIZEOF(multi_link_data_ex_t,payload_p.data)];

    memset(kd, 0x00, sizeof(kd));
    memcpy(pmac, &multi_link_dynamic_key_info->mac[3], sizeof(pmac));


    if(multi_link_dynamic_key_info->state == 2)
    {
        //
        // Kd2=MID(MAC1, MSB,6)||Dev-N-5||Don-N-5 //6-5-5 byte sequence
        //
        memcpy(kd, multi_link_dynamic_key_info->mac, 6);
        memcpy(&kd[6], multi_link_dynamic_key_info->device_nonce, sizeof(multi_link_dynamic_key_info->device_nonce));
        memcpy(&kd[6 + sizeof(multi_link_dynamic_key_info->device_nonce)], multi_link_dynamic_key_info->dongle_nonce, sizeof(multi_link_dynamic_key_info->dongle_nonce));

        LOG_HEXDUMP_DBG((uint8_t *)&kd, sizeof(kd), "kd2 :");
    }
    else
    {
        //
        // Kd-n= MACn-1  // Use previous MAC output as Key for next Input Stream Function.
        //
        memcpy(kd, multi_link_dynamic_key_info->mac, sizeof(kd) );
        
        LOG_HEXDUMP_DBG((uint8_t *)&kd, sizeof(kd), "kdn :");
    }

    uint8_t sf[16];
    uint8_t sf_p[16];

    memset(sf, 0x00, sizeof(sf));
    memset(sf_p, 0x00, sizeof(sf_p));

    memcpy(sf_p, multi_link_dynamic_key_info->MGDN, sizeof(multi_link_dynamic_key_info->MGDN));
    memcpy(&sf_p[sizeof(multi_link_dynamic_key_info->MGDN)], s, sizeof(s));
    memcpy(&sf_p[sizeof(multi_link_dynamic_key_info->MGDN) + sizeof(s)], multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));
    

    LOG_HEXDUMP_DBG((uint8_t *)&sf_p, sizeof(sf_p), "sf_p :");

      
    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kd)])kd, sf_p, sizeof(sf_p), sf, sizeof(sf)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&sf, sizeof(sf), "sf :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for sf");
        return false;
    }

    memcpy(&multi_link_dynamic_key_info->MGDN[2], &sf[8], 8);


    //
    // p = PMAC XOR (ep XOR SF)
    //
    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_data_ex->payload_ep, sizeof(multi_link_data_ex->payload_p), "payload_ep :");

    multi_link_proto_common_xor((uint8_t *)&multi_link_data_ex->payload_p, (const uint8_t *)&multi_link_data_ex->payload_ep, (const uint8_t *)sf, sizeof(multi_link_data_ex->payload_p));
    multi_link_proto_common_xor((uint8_t *)&multi_link_data_ex->payload_p.data, (const uint8_t *)pmac, (const uint8_t *)&multi_link_data_ex->payload_p.data, sizeof(multi_link_data_ex->payload_p.data));

    LOG_HEXDUMP_DBG((uint8_t *)&multi_link_data_ex->payload_p, sizeof(multi_link_data_ex->payload_p), "payload_p :");

    LOG_HEXDUMP_DBG((uint8_t *)multi_link_data_ex, sizeof(multi_link_data_ex_t), "multi_link_data_ex decoded:");

    //
    // Calculate MAC
    // E( kd, f(Sn)|| Dev-ID||Data || Sn ) -> in state > kd == MACn-1 see above
    //
    uint8_t mac_p[16];

    memset(multi_link_dynamic_key_info->mac, 0x00, sizeof(multi_link_dynamic_key_info->mac));
    memset(mac_p, 0x00, sizeof(mac_p));

    memcpy(mac_p, multi_link_dynamic_key_info->f, sizeof(multi_link_dynamic_key_info->f));
    memcpy(&mac_p[sizeof(multi_link_dynamic_key_info->f)], multi_link_data_ex->payload_p.data, 10);
    memcpy(&mac_p[sizeof(multi_link_dynamic_key_info->f) + 10 ], s, sizeof(s));

    LOG_HEXDUMP_DBG((uint8_t *)&mac_p, sizeof(mac_p), "mac_p :");

    if (g_multi_link_common_callbacks->ecb_128((const uint8_t(*)[sizeof(kd)])kd, mac_p, sizeof(mac_p), (uint8_t*)multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac)))
    {
        LOG_HEXDUMP_DBG((uint8_t *)&multi_link_dynamic_key_info->mac, sizeof(multi_link_dynamic_key_info->mac), "mac :");
    }
    else
    {
        LOG_ERR("ecb_128 failed for mac");
        return 0;
    }

    if (memcmp(multi_link_data_ex->payload_p.mac, multi_link_dynamic_key_info->mac, sizeof(multi_link_data_ex->payload_p.mac)) != 0)
    {
        LOG_ERR("mac compare failed");
        return 0;
    }


    LOG_DBG("------------------");

    return true;
}

bool multi_link_proto_common_check_array_is_empty(const uint8_t *array, uint32_t array_length)
{

    if (array)
    {
        for (uint32_t i = 0; i < array_length; i++)
        {
            if (array[i] != 0x00)
                return false;
        }
    }

    return true;
}