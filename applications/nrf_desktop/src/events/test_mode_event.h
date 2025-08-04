#ifndef _TEST_MODE_EVENT_H_
#define _TEST_MODE_EVENT_H_

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    test_id_PER_tx = 0xf0,
    test_id_count
};

struct test_mode_event {
	struct app_event_header header;

	uint8_t test_id;
    struct event_dyndata dyndata;
};
//APP_EVENT_TYPE_DECLARE(test_mode_event);
APP_EVENT_TYPE_DYNDATA_DECLARE(test_mode_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _TEST_MODE_EVENT_H_ */
