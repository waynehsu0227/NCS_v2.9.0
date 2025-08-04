#ifndef _BATTERY_READ_REQUEST_EVENT_H_
#define _BATTERY_READ_REQUEST_EVENT_H_

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

struct battery_read_request_event {
    struct app_event_header header;
};

APP_EVENT_TYPE_DECLARE(battery_read_request_event);

#endif /* _BATTERY_READ_REQUEST_EVENT_H_ */