/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _BATTERY_EVENT_H_
#define _BATTERY_EVENT_H_

/**
 * @brief Battery Event
 * @defgroup battery_event Battery Event
 * @{
 */

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

#define REPORT_SIZE_BATTERY_LEVEL        1 /* bytes */
#define BATTERY_LEVEL_REPORT_COUNT_MAX	 1

#define MOUSE_REPORT_BATTERY_LEVEL_MIN		0
#define MOUSE_REPORT_BATTERY_LEVEL_MAX		100

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Battery state list. */
#if (CONFIG_CY25KB_DRYCELL || CONFIG_CY25MS_DRYCELL)
#define BATTERY_STATE_LIST	\
	X(IDLE)			\
	X(CHARGING)		\
	X(ERROR)		\
	X(LOW_BAT)
#elif CONFIG_CY25KB_RECHARGEABLE
#define BATTERY_STATE_LIST	\
	X(IDLE)			\
	X(CHARGING)		\
	X(ERROR)		\
	X(FULL)			\
	X(LOW_BAT)
#else
#define BATTERY_STATE_LIST	\
	X(IDLE)			\
	X(CHARGING)		\
	X(ERROR)
#endif

/** @brief Battery states. */
enum battery_state {
#define X(name) _CONCAT(BATTERY_STATE_, name),
	BATTERY_STATE_LIST
#undef X

	BATTERY_STATE_COUNT
};


/** @brief Battery state event. */
struct battery_state_event {
	struct app_event_header header;

	enum battery_state state;
};

APP_EVENT_TYPE_DECLARE(battery_state_event);


/** @brief Battery voltage level event. */
struct battery_level_event {
	struct app_event_header header;

	uint8_t level;
	bool is_charging;
};

APP_EVENT_TYPE_DECLARE(battery_level_event);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* _BATTERY_EVENT_H_ */
