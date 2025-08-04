/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <app_event_manager.h>

#define MODULE main
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE);

#define GPIO_TEST false

#if GPIO_TEST
#include <zephyr/drivers/gpio.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
static const struct device * const gpio_devs[] = {
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio0)),
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio1)),
	DEVICE_DT_GET_OR_NULL(DT_NODELABEL(gpio2)),
};
#endif

int main(void)
{
#if GPIO_TEST
	//NRF_P1->PIN_CNF[8] = 0x2;
	//gpio_pin_configure(gpio_devs[2], 9, GPIO_OUTPUT_ACTIVE);
	//gpio_pin_configure(gpio_devs[2], 10, GPIO_OUTPUT_ACTIVE);
	//gpio_pin_set(gpio_devs[2], 9 ,0);
	//gpio_pin_set(gpio_devs[2], 10 ,0);
#endif
	LOG_INF("CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION %s",
		STRINGIFY(CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION));
	if (app_event_manager_init()) {
		LOG_ERR("Application Event Manager not initialized");
	} else {
		module_set_state(MODULE_STATE_READY);
	}
	return 0;
}
