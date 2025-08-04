/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <caf/key_id.h>
//#include "fn_key_id.h"
/* This configuration file is included only once from fn_keys module and holds
 * information about key with alternate functions.
 */
/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} mac_os_key_def_include_once;


static const uint16_t mac_os_keys[] = {
	KEY_ID(0x0A, 0x07),	   /* copilot -> alt-r */
	KEY_ID(0x0A, 0x0B),    /* win-l   -> alt-l */
	KEY_ID(0x0B, 0x06),    /* alt-l   -> cmd-l */
	KEY_ID(0x0B, 0x0A),    /* alt-r   -> cmd-r */
};

