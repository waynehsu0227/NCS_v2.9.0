#ifndef _MULTI_LINK_PEER_EVENT_H_
#define _MULTI_LINK_PEER_EVENT_H_

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

#ifdef __cplusplus
extern "C" {
#endif

enum multi_link_state {
	MULTI_LINK_STATE_DISCONNECTED,

	MULTI_LINK_STATE_DISCONNECTING,

	MULTI_LINK_STATE_CONNECTED,

	MULTI_LINK_STATE_SECURED,

	MULTI_LINK_STATE_CONN_FAILED,

	MULTI_LINK_STATE_PAIRING,

	MULTI_LINK_STATE_COUNT,

	APP_EM_ENFORCE_ENUM_SIZE(MULTI_LINK_STATE)
};

enum multi_link_operation {
	MULTI_LINK_OPERATION_SELECT,

	MULTI_LINK_OPERATION_SELECTED,

	MULTI_LINK_OPERATION_SCAN_REQUEST,

	MULTI_LINK_OPERATION_ERASE,

	MULTI_LINK_OPERATION_ERASE_ADV,

	MULTI_LINK_OPERATION_ERASE_ADV_CANCEL,

	MULTI_LINK_OPERATION_ERASED,

	MULTI_LINK_OPERATION_CANCEL,

    MULTI_LINK_OPERATION_SWITCH,

	MULTI_LINK_OPERATION_COUNT,

	APP_EM_ENFORCE_ENUM_SIZE(MULTI_LINK_OPERATION)
};


struct multi_link_peer_event {
	struct app_event_header header;

	enum multi_link_state state;

	void *id;
};

APP_EVENT_TYPE_DECLARE(multi_link_peer_event);

#ifdef __cplusplus
}
#endif


#endif /* _MULTI_LINK_EVENT_H_ */
