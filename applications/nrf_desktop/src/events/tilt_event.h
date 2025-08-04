/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _TILT_EVENT_H_
#define _TILT_EVENT_H_

/**
 * @brief tilt Event
 * @defgroup tilt_event tilt Event
 * @{
 */

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tilt_event {
	struct app_event_header header;

	int8_t tilt;
};

APP_EVENT_TYPE_DECLARE(tilt_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _TILT_EVENT_H_ */
