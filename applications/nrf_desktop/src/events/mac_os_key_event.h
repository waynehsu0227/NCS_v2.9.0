#ifndef _MAC_OS_KEY_EVENT_H_
#define _MAC_OS_KEY_EVENT_H_

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAC_MODE_NO_CONVER_KEY  KEY_ID(0x0A, 0x07)

struct mac_os_mode_on_event{
    struct app_event_header header;
    bool sw;
};
APP_EVENT_TYPE_DECLARE(mac_os_mode_on_event);


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _MAC_OS_KEY_EVENT_H_ */
