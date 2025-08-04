/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "test_mode_event.h"

APP_EVENT_TYPE_DEFINE(test_mode_event,
		      NULL,
		      NULL,
		      APP_EVENT_FLAGS_CREATE());
