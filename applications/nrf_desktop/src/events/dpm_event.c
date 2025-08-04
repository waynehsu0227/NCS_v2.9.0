/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dpm_event.h"

APP_EVENT_TYPE_DEFINE(dpm_save_host_name_event,
		      NULL,
		      NULL,
		      APP_EVENT_FLAGS_CREATE());

#ifdef CONFIG_DPM_COLLABS_KEY_SUPPORT
APP_EVENT_TYPE_DEFINE(dpm_set_collabs_led_event,
		      NULL,
		      NULL,
		      APP_EVENT_FLAGS_CREATE());
#endif

#ifdef CONFIG_DESKTOP_PERIPHERAL_TYPE_MOUSE
APP_EVENT_TYPE_DEFINE(dpm_set_report_rate_event,
		      NULL,
		      NULL,
		      APP_EVENT_FLAGS_CREATE());
#endif

#ifdef CONFIG_DPM_MULTI_LINK_SUPPORT
APP_EVENT_TYPE_DEFINE(dpm_multi_link_event,
		      NULL,
		      NULL,
		      APP_EVENT_FLAGS_CREATE());
#endif
