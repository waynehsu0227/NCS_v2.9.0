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
const struct {} fn_key_def_include_once;

static const uint16_t fn_keys[] = {
	KEY_ID(0x01, 0x00), /* f7 -> screen cut */
	KEY_ID(0x01, 0x01), /* f9 -> lock PC */
	KEY_ID(0x01, 0x02), /* f1 -> mute */
	KEY_ID(0x01, 0x03), /* f11 -> BT2 mode*/
	KEY_ID(0x01, 0x06), /* f5 -> Play/ Pause */
	KEY_ID(0x01, 0x09), /* f3 -> Volume Up */
	KEY_ID(0x02, 0x00), /* f8 -> desktop */
	KEY_ID(0x02, 0x01), /* f10 -> BT1 mode*/
	KEY_ID(0x02, 0x02), /* f4 -> Previous Track */
	KEY_ID(0x02, 0x03), /* f12 -> RF mode */
	KEY_ID(0x02, 0x06), /* f6 -> Next Track */
	KEY_ID(0x02, 0x0B), /* f2 -> Volume Down */
	KEY_ID(0x0A, 0x07), /* application -> copilot */
#if (CONFIG_DESKTOP_MAC_OS_KEYS_ENABLE)
	KEY_ID(0x0A, 0x0B), /* win-l -> Win-mode */
	KEY_ID(0x0B, 0x06), /* alt-l -> Mac-mode */
#endif
};

static const uint16_t fn_no_lock_keys[] = {
	KEY_ID(0x0A, 0x07), /* application -> copilot */
	//	KEY_ID(0x0A, 0x04), /* scr lock -> print screen */
#if (CONFIG_DESKTOP_MAC_OS_KEYS_ENABLE)
	KEY_ID(0x0A, 0x0B), /* win-l -> Win-mode */
	KEY_ID(0x0B, 0x06), /* alt-l -> Mac-mode */
#endif
};
