#include "battery_read_request_event.h"

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

APP_EVENT_TYPE_DEFINE(battery_read_request_event,
    NULL,                      // 事件釋放（通常為 NULL）
    NULL,                      // 事件轉字串（可 NULL）
    APP_EVENT_FLAGS_CREATE()   // 事件標誌（通常預設即可）
);