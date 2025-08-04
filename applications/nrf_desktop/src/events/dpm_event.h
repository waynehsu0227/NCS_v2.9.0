#ifndef _DPM_EVENT_H_
#define _DPM_EVENT_H_

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REPORT_SIZE_DPM_MULTI_LINK_EVENT        1 /* bytes */
#define DPM_MULTI_LINK_EVENT_REPORT_COUNT_MAX	 1


struct dpm_save_host_name_event {
	struct app_event_header header;

    struct event_dyndata dyndata;
};

APP_EVENT_TYPE_DYNDATA_DECLARE(dpm_save_host_name_event);

#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT
struct dpm_set_collabs_led_event {
	struct app_event_header header;

    bool led_lower_brightness;
    struct {
        bool mic_mute;
        bool chat;
        bool share;
        bool video;
    } led_sw;

    struct {
        bool mic_disable_led;
        bool mic_led;
        bool chat_led;
        bool chat_led_blink;
        bool share_screen_led;
        bool camera_disable_led;
        bool camera_led;
    } led_state;
};

APP_EVENT_TYPE_DECLARE(dpm_set_collabs_led_event);
#endif

#ifdef CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE
struct dpm_set_report_rate_event {
	struct app_event_header header;

    float report_interval;
};

APP_EVENT_TYPE_DECLARE(dpm_set_report_rate_event);
#endif

#ifdef CONFIG_DPM_MULTI_LINK_SUPPORT
struct dpm_multi_link_event{
    struct app_event_header header;
    uint8_t cmd;
};
APP_EVENT_TYPE_DECLARE(dpm_multi_link_event);
#endif

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DPM_EVENT_H_ */
