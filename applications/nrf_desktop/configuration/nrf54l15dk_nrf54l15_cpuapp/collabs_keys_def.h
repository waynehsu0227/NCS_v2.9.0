/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <caf/key_id.h>

/* This configuration file is included only once from fn_keys module and holds
 * information about key with alternate functions.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} collabs_key_def_include_once;

static const uint16_t collabs_keys[] = {
	KEY_ID(0x02, 0x07), /* Sharing Screen */
	KEY_ID(0x02, 0x0a), /* Chat */
	KEY_ID(0x08, 0x08), /* Video */
	KEY_ID(0x0B, 0x08), /* Mic */
};
