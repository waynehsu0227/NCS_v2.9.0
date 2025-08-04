#ifndef _MULTI_LINK_SETTING_H_
#define _MULTI_LINK_SETTING_H_

#if defined ( __CC_ARM   )
#pragma anon_unions
#endif

#include <string.h>
#include "multi_link_proto_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MULTI_LINK_HOST_ADDR_LENGTH     4
#define MULTI_LINK_HOST_ID_LENGTH       5
#define MULTI_LINK_CHALLENGE_LENGTH     5
#define MULTI_LINK_RESPONSE_LENGTH      5

#define MULTI_LINK_PAIRING_LENGTH       1
#define MULTI_LINK_PAIRING_FLAG         0xAA

void multi_link_get_pairing_setting(uint8_t* host_addr,
                                    uint8_t* host_id,
                                    uint8_t* challenge,
                                    uint8_t* response,
									uint8_t* pairing);
bool multi_link_params_store(void);
bool muiti_link_params_restore(void);
bool multi_link_pairing_store(void);

#ifdef __cplusplus
}
#endif

#endif /* _MULTI_LINK_PROTO_H_ */