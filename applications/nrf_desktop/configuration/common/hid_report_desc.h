/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 * @brief Header file HID report identifiers.
 */

#ifndef _HID_REPORT_DESC_H_
#define _HID_REPORT_DESC_H_

#include <stddef.h>
#include <zephyr/types.h>
#include <zephyr/toolchain.h>
#include <zephyr/sys/util.h>

#include "hid_report_mouse.h"
#include "hid_report_keyboard.h"
#include "hid_report_system_ctrl.h"
#include "hid_report_consumer_ctrl.h"
#include "hid_report_user_config.h"
#ifdef CONFIG_DPM_ENABLE
#include "hid_report_dpm.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HID Reports
 * @defgroup nrf_desktop_hid_reports HID Reports
 * @{
 */

/** @brief Identification numbers of HID reports. */
enum report_id {
	/** Reserved. */
	REPORT_ID_RESERVED, //0

	/** Mouse input report. */
	REPORT_ID_MOUSE, //1
	/** Keyboard input report. */
	REPORT_ID_KEYBOARD_KEYS, //2
	/** System control input report. */
	REPORT_ID_SYSTEM_CTRL, //3
	/** Consumer control input report. */
	REPORT_ID_CONSUMER_CTRL, //4

	/** Keyboard output report. */
	REPORT_ID_KEYBOARD_LEDS, //5

	/** Config channel feature report. */
	REPORT_ID_USER_CONFIG, //6
	/** Config channel output report. */
	REPORT_ID_USER_CONFIG_OUT, //7
 
	/** Reserved. */
	REPORT_ID_VENDOR_IN, //8
	/** Reserved. */
	REPORT_ID_VENDOR_OUT, //9

	/** Boot report - mouse. */
	REPORT_ID_BOOT_MOUSE, //10
	/** Boot report - keyboard. */
	REPORT_ID_BOOT_KEYBOARD, //11
#ifdef CONFIG_DPM_ENABLE
    REPORT_ID_DPM_INPUT = 17,
    REPORT_ID_DPM_FEATURE = 18,
#endif
#ifdef CONFIG_DPM_MULTI_LINK_SUPPORT //dummy report id
	REPORT_ID_BATTERY_LEVEL,
	REPORT_ID_FIRST_HOST_NAME, 
	REPORT_ID_SECOND_HOST_NAME, 
	REPORT_ID_FW_VERSION_CAPABILITY,
	REPORT_ID_SENSOR_DPI,
	REPORT_ID_REPORT_RATE,  
#endif
	/** Number of reports. */
	REPORT_ID_COUNT
};

/** @brief Input reports map. */
static const uint8_t input_reports[] = {
	REPORT_ID_MOUSE,
	REPORT_ID_KEYBOARD_KEYS,
	REPORT_ID_SYSTEM_CTRL,
	REPORT_ID_CONSUMER_CTRL,
	REPORT_ID_BOOT_MOUSE,
	REPORT_ID_BOOT_KEYBOARD,
#ifdef CONFIG_DPM_ENABLE
    REPORT_ID_DPM_INPUT,
#endif
};

/** @brief Output reports map. */
static const uint8_t output_reports[] = {
	REPORT_ID_KEYBOARD_LEDS,
};

/* Internal definitions used to calculate size of the biggest supported HID input report. */
#define _REPORT_BUFFER_SIZE_MOUSE \
	(IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_MOUSE_SUPPORT) ? (REPORT_SIZE_MOUSE) : (0))
#define _REPORT_BUFFER_SIZE_KEYBOARD_KEYS \
	(IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_KEYBOARD_SUPPORT) ? (REPORT_SIZE_KEYBOARD_KEYS) : (0))
#define _REPORT_BUFFER_SIZE_SYSTEM_CTRL								\
	(IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_SYSTEM_CTRL_SUPPORT) ?				\
	 (REPORT_SIZE_SYSTEM_CTRL) : (0))
#define _REPORT_BUFFER_SIZE_CONSUMER_CTRL							\
	(IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_CONSUMER_CTRL_SUPPORT) ?				\
	 (REPORT_SIZE_CONSUMER_CTRL) : (0))
#define _REPORT_BUFFER_SIZE_BOOT_MOUSE \
	(IS_ENABLED(CONFIG_DESKTOP_HID_BOOT_INTERFACE_MOUSE) ? (REPORT_SIZE_MOUSE_BOOT) : (0))
#define _REPORT_BUFFER_SIZE_BOOT_KEYBOARD \
	(IS_ENABLED(CONFIG_DESKTOP_HID_BOOT_INTERFACE_KEYBOARD) ? (REPORT_SIZE_KEYBOARD_BOOT) : (0))
#define _MAX6(a, b, c, d, e, f)	MAX(MAX(a, b), MAX(MAX(c, d), MAX(e, f)))

/* Size of the biggest supported HID input report that is part of input reports map. */
#define REPORT_BUFFER_SIZE_INPUT_REPORT								\
	_MAX6(_REPORT_BUFFER_SIZE_MOUSE, _REPORT_BUFFER_SIZE_KEYBOARD_KEYS,			\
	      _REPORT_BUFFER_SIZE_SYSTEM_CTRL, _REPORT_BUFFER_SIZE_CONSUMER_CTRL,		\
	      _REPORT_BUFFER_SIZE_BOOT_MOUSE, _REPORT_BUFFER_SIZE_BOOT_KEYBOARD)

/* Internal definitions used to calculate size of the biggest supported HID output report. */
#define _REPORT_BUFFER_SIZE_KEYBOARD_LEDS \
	(IS_ENABLED(CONFIG_DESKTOP_HID_REPORT_KEYBOARD_SUPPORT) ? (REPORT_SIZE_KEYBOARD_LEDS) : (0))

/* Size of the biggest supported HID output report that is part of output reports map. */
#define REPORT_BUFFER_SIZE_OUTPUT_REPORT	_REPORT_BUFFER_SIZE_KEYBOARD_LEDS

extern const uint8_t hid_report_desc[];
extern const size_t hid_report_desc_size;

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* _HID_REPORT_DESC_H_ */
