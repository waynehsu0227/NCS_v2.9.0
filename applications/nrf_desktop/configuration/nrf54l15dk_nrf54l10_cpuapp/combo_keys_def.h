/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <caf/key_id.h>
#include "fn_key_id.h"
/* This configuration file is included only once from fn_keys module and holds
 * information about key with alternate functions.
 */
/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} combo_key_def_include_once;

static const uint16_t screen_cut_combo[] = {
	3,						/*size*/
	KEY_ID(0x0C, 0x0C),		/*win-l*/
	KEY_ID(0x09, 0x02),		/*shift-l*/
	KEY_ID(0x06, 0x0B),		/*s*/
};

static const uint16_t lock_PC_combo[] = {
	2,						/*size*/
	KEY_ID(0x0C, 0x0C),		/*win-l*/
	KEY_ID(0x03, 0x04),		/*l*/
};

static const uint16_t task_view_combo[] = {
	2,						/*size*/
	KEY_ID(0x0C, 0x0C),		/*win-l*/
	KEY_ID(0x04, 0x02),		/*tab*/
};

static const uint16_t desktop_combo[] = {
	2,						/*size*/
	KEY_ID(0x0C, 0x0C),		/*win-l*/
	KEY_ID(0x04, 0x06),		/*d*/
};

static const uint16_t copilot_combo[] = {
	3,						/*size*/
	KEY_ID(0x0C, 0x0C),		/*win-l*/
	KEY_ID(0x09, 0x02),		/*shift-l*/
	KEY_ID(0x0C, 0x0A),		/*F23*/
};

static const uint16_t combo_keys[] = {
	KEY_ID(0x0A, 0x07),    /* application -> copilot */
	FN_KEY_ID(0x01, 0x00), /* f7 -> screen cut */
	FN_KEY_ID(0x01, 0x01), /* f9 -> lock PC */
	FN_KEY_ID(0x02, 0x00), /* f8 -> desktop */
};

static const uint16_t *combo_keys_table[] = {
	copilot_combo,
	screen_cut_combo,
	lock_PC_combo,
	desktop_combo,
};
