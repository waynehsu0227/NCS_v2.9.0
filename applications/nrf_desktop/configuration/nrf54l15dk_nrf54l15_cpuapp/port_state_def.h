/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <soc.h>

#include "port_state.h"

/* This configuration file is included only once from board module and holds
 * information about default pin states set while board is on and off.
 */

/* This structure enforces the header file is included only once in the build.
 * Violating this requirement triggers a multiple definition error at link time.
 */
const struct {} port_state_def_include_once;


static const struct pin_state port0_on[] = {

};

static const struct pin_state port1_on[] = {

};

static const struct pin_state port2_on[] = {
#ifdef CONFIG_CY25MS_DRYCELL
    {10, 1},    /* 3v3 enable */
#endif
};

static const struct pin_state port0_off[] = {
};

static const struct pin_state port1_off[] = {
};

static const struct pin_state port2_off[] = {
#ifdef CONFIG_CY25MS_DRYCELL
    {10, 0},    /* 3v3 enable */
#endif
};


static const struct port_state port_state_on[] = {
	{
		.port     = DEVICE_DT_GET(DT_NODELABEL(gpio2)),
		.ps       = port2_on,
		.ps_count = ARRAY_SIZE(port2_on),
	}
};

static const struct port_state port_state_off[] = {
	{
		.port     = DEVICE_DT_GET(DT_NODELABEL(gpio2)),
		.ps       = port2_off,
		.ps_count = ARRAY_SIZE(port2_off),
	}
};
