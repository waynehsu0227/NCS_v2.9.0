/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <caf/click_detector.h>

/* This configuration file is included only once from click_detector module
 * and holds information about click detector configuration.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} click_detector_def_include_once;

static const struct click_detector_config click_detector_config[] = {
	{
#if defined(CONFIG_MULTILINK_ENABLE)
	#if defined(CONFIG_DESKTOP_BT)
			.key_id = CONFIG_MULTILINK_BLE_SELECTOR_BUTTON,
	#else
			.key_id = CONFIG_MULTI_LINK_PEER_CONTROL_BUTTON,
	#endif
#else
        .key_id = CONFIG_DESKTOP_BLE_PEER_CONTROL_BUTTON,
#endif
		.consume_button_event = false,
	},
};
