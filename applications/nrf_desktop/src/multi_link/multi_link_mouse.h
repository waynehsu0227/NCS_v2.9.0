#ifndef __MULTI_LINK_MOUSE_H__
#define __MULTI_LINK_MOUSE_H__

#include <stdbool.h>
#include <stdint.h>
#include "multi_link.h"

typedef enum
{
    MOUSE_PACKET_TYPE_MOTION,
    MOUSE_PACKET_TYPE_BUTTON,
    MOUSE_PACKET_TYPE_SCROLL,
    MOUSE_PACKET_TYPE_BATTERY_LEVEL,
    MOUSE_PACKET_TYPE_POWER_ON_BATTERY_LEVEL,
 
    MOUSE_PACKET_TYPE_DPI_REPORT,
    MOUSE_PACKET_TYPE_REPORT_RATE,
    MOUSE_PACKET_TYPE_HOST_NAME,  
    MOUSE_PACKET_TYPE_FW_VER,   
     
    MOUSE_PACKET_TYPE_PAIRING_BTN_PRESSED,
    MOUSE_PACKET_TYPE_PAIRING_BTN_RELEASED,
    MOUSE_PACKET_TYPE_MODE_BUTTON,
    MOUSE_PACKET_TYPE_TEST_MODE,
    MOUSE_PACKET_TYPE_CUT_OFF_MODE,
    MOUSE_PACKET_TYPE_SWITCH_TO_24G_MODE,
    MOUSE_PACKET_TYPE_OOB_MODE
  
} mouse_packet_type_t;

typedef struct
{
    mouse_packet_type_t type;
    uint8_t button;
    uint8_t xbit;
    uint8_t xybit;
    uint8_t ybit;
    uint8_t scroll;
    uint8_t pan;
} m_mouse_data_t;


#endif /* __MULTI_LINK_MOUSE_H__ */
