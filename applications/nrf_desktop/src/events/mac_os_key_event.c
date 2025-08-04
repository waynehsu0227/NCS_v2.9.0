/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "mac_os_key_event.h"

APP_EVENT_TYPE_DEFINE(mac_os_mode_on_event,
		      NULL,
		      NULL,
		      APP_EVENT_FLAGS_CREATE());
