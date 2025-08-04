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

static const struct gpio_pin col[] = {
    { .port = 2, .pin = 5  },       //Y0
	{ .port = 2, .pin = 0  },       //Y1
	{ .port = 2, .pin = 1  },       //Y2
	{ .port = 2, .pin = 4  },       //Y3
	{ .port = 2, .pin = 2  },       //Y4
	{ .port = 2, .pin = 3  },       //Y5
	{ .port = 2, .pin = 7  },       //Y6
	{ .port = 2, .pin = 6  },       //Y7
    { .port = 0, .pin = 0  },       //Y8
    { .port = 2, .pin = 9  },       //Y9
    { .port = 2, .pin = 8  },       //YA
    { .port = 2, .pin = 10 },       //YB
};

static const struct gpio_pin row[] = {
    { .port = 1, .pin = 5  },       //X0
	{ .port = 1, .pin = 6  },       //X1
	{ .port = 1, .pin = 1  },       //X2
	{ .port = 1, .pin = 2  },       //X3
	{ .port = 1, .pin = 7  },       //X4
	{ .port = 0, .pin = 2  },       //X5
	{ .port = 1, .pin = 3  },       //X6
	{ .port = 1, .pin = 8  },       //X7
    { .port = 0, .pin = 3  },       //X8
    { .port = 1, .pin = 4  },       //X9
    { .port = 0, .pin = 4  },       //XA
    { .port = 1, .pin = 0  },       //XB
};
