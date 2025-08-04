/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "passkey_buttons.h"

/* This configuration file is included only once from passkey_buttons module
 * and holds information about buttons used to input passkey.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} passkey_buttons_def_include_once;

const static uint16_t confirm_keys[] = {
    KEY_ID(0x08, 0x03),   /* enter */
	KEY_ID(0x02, 0x08),   /* keypad enter */
};

const static uint16_t delete_keys[] = {
    KEY_ID(0x00, 0x04),   /* backspace */
};

const static struct passkey_input_config input_configs[] = {
    {
		.key_ids = {
			KEY_ID(0x04, 0x01),   /* 0 */
			KEY_ID(0x04, 0x0B),   /* 1 */
			KEY_ID(0x03, 0x02),   /* 2 */
			KEY_ID(0x05, 0x02),   /* 3 */
			KEY_ID(0x05, 0x0B),   /* 4 */
			KEY_ID(0x05, 0x06),   /* 5 */
			KEY_ID(0x03, 0x06),   /* 6 */
			KEY_ID(0x05, 0x00),   /* 7 */
			KEY_ID(0x04, 0x00),   /* 8 */
			KEY_ID(0x05, 0x01),   /* 9 */
		},
	},
	{
		.key_ids = {
			KEY_ID(0x06, 0x04),   /* keypad 0 */
			KEY_ID(0x00, 0x05),   /* keypad 1 */
			KEY_ID(0x06, 0x07),   /* keypad 2 */
			KEY_ID(0x06, 0x08),   /* keypad 3 */
			KEY_ID(0x03, 0x05),   /* keypad 4 */
			KEY_ID(0x03, 0x07),   /* keypad 5 */
			KEY_ID(0x03, 0x0A),   /* keypad 6 */
			KEY_ID(0x05, 0x07),   /* keypad 7 */
			KEY_ID(0x03, 0x08),   /* keypad 8 */
			KEY_ID(0x05, 0x08),   /* keypad 9 */
		},
	},
};
