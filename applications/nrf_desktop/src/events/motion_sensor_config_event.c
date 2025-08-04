/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "motion_sensor_config_event.h"

APP_EVENT_TYPE_DEFINE(motion_sensor_config_event,
		      NULL,
		      NULL,
		      APP_EVENT_FLAGS_CREATE());
