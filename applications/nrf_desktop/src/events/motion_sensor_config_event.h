#ifndef _MOTION_SENSOR_CONFIG_EVENT_H_
#define _MOTION_SENSOR_CONFIG_EVENT_H_

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>
#include "motion_sensor.h"
#ifdef __cplusplus
extern "C" {
#endif


struct motion_sensor_config_event {
	struct app_event_header header;

	enum motion_sensor_option opt;

	uint32_t value;
};

APP_EVENT_TYPE_DECLARE(motion_sensor_config_event);

#ifdef __cplusplus
}
#endif


#endif /* _MOTION_SENSOR_CONFIG_EVENT_H_ */
