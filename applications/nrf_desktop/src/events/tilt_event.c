/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include "tilt_event.h"

static void log_tilt_event(const struct app_event_header *aeh)
{
	const struct tilt_event *event = cast_tilt_event(aeh);

	APP_EVENT_MANAGER_LOG(aeh, "tilt=%d", event->tilt);
}

static void profile_tilt_event(struct log_event_buf *buf,
				    const struct app_event_header *aeh)
{
	const struct tilt_event *event = cast_tilt_event(aeh);

	nrf_profiler_log_encode_int8(buf, event->tilt);
}

APP_EVENT_INFO_DEFINE(tilt_event,
		  ENCODE(NRF_PROFILER_ARG_S8),
		  ENCODE("tilt"),
		  profile_tilt_event);

APP_EVENT_TYPE_DEFINE(tilt_event,
		  log_tilt_event,
		  &tilt_event_info,
		  APP_EVENT_FLAGS_CREATE(
			IF_ENABLED(CONFIG_DESKTOP_INIT_LOG_tilt_EVENT,
				(APP_EVENT_TYPE_FLAGS_INIT_LOG_ENABLE))));
