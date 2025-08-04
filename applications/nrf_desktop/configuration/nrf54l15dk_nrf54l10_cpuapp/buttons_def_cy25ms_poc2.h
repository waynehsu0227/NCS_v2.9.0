/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <caf/gpio_pins.h>

/* This configuration file is included only once from button module and holds
 * information about pins forming keyboard matrix.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} buttons_def_include_once;

static const struct gpio_pin col[] = {};

static const struct gpio_pin row[] = {
	{ .port = 0, .pin = 4 },//L      			0
	{ .port = 0, .pin = 0 },//R      			1
	{ .port = 1, .pin = 14 },//M     			2
	{ .port = 0, .pin = 2 },//SW_Forward      	3
	{ .port = 0, .pin = 3 },//SW_Backward       4
	{ .port = 1, .pin = 12 },//Tilt_L     		5
	{ .port = 1, .pin = 13 },//Tilt_R      		6
	{ .port = 1, .pin = 10 },//Mode  			7
};
