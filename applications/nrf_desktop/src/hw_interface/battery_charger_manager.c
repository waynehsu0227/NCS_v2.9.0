/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/types.h>

#include <soc.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/spinlock.h>
#include <zephyr/drivers/charger.h>

#include "charger.h"

#include <app_event_manager.h>
#include <caf/events/power_event.h>
#include "battery_event.h"

#define MODULE battery_charger_manager
#include <caf/events/module_state_event.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CHARGER_LOG_LEVEL);
//LOG_MODULE_REGISTER(MODULE, LOG_LEVEL_DBG);


#define ERROR_CHECK_TIMEOUT_MS	4000

static const struct device *charger_dev =
	DEVICE_DT_GET_ONE(CHARGER_COMPATIBLE);

static struct k_work_delayable error_check;
static struct k_spinlock lock;

static enum battery_state battery_state = -1;
static atomic_t active;

static void update_charger_supply(void)
{
	static enum charger_online online = -1;
	union charger_propval val = {0};
	charger_get_prop(charger_dev, CHARGER_PROP_ONLINE, &val);
//	LOG_INF("old:%d, new:%d", online, val.online);
	if (online != val.online) {
		online = val.online;
		LOG_INF("online change to %d", online);
	}
}

static void update_charger_state(void)
{
	union charger_propval val = {0};
	enum battery_state state;
	charger_get_prop(charger_dev, CHARGER_PROP_STATUS, &val);
	switch(val.status) {
		case CHARGER_STATUS_CHARGING:
			state = BATTERY_STATE_CHARGING;
			break;
		case CHARGER_STATUS_DISCHARGING:
		case CHARGER_STATUS_NOT_CHARGING:
			state = BATTERY_STATE_IDLE;
			break;
		case CHARGER_STATUS_FULL:
			state = BATTERY_STATE_FULL;
			break;
		case CHARGER_STATUS_UNKNOWN:
		default:
			state = BATTERY_STATE_ERROR;
	}
//	LOG_INF("state=%d:%d", val.status, state);
	if (state != battery_state) {
		battery_state = state;

		struct battery_state_event *event = new_battery_state_event();
		event->state = battery_state;

		APP_EVENT_SUBMIT(event);
	}
}

static void update_charger_type(void)
{
	static enum charger_charge_type type = -1;
	union charger_propval val = {0};
	charger_get_prop(charger_dev, CHARGER_PROP_CHARGE_TYPE, &val);
	if (type != val.charge_type) {
		type = val.charge_type;
		LOG_INF("charge_type change to %d", type);
	}
}

static void update_charger_health(void)
{
	static enum charger_health health = -1;
	union charger_propval val = {0};
	charger_get_prop(charger_dev, CHARGER_PROP_HEALTH, &val);
	if (health != val.health) {
		health = val.health;
		LOG_INF("health change to %d", health);
	}
}

static void error_check_handler(struct k_work *work)
{
	update_charger_supply();
	update_charger_state();
	update_charger_type();
	update_charger_health();
	k_work_reschedule(&error_check, K_MSEC(ERROR_CHECK_TIMEOUT_MS));
}

static bool set_option(charger_prop_t prop, union charger_propval *val)
{
/*
	if (motion_sensor_option_attr[option] != -ENOTSUP) {
		k_spinlock_key_t key = k_spin_lock(&state.lock);
		state.option[option] = value;
		WRITE_BIT(state.option_mask, option, true);
		k_spin_unlock(&state.lock, key);
		k_sem_give(&sem);

		return true;
	}
*/
//	LOG_INF("Charger option %d is not supported", option);
//		return false;
	int err = charger_set_prop(charger_dev, prop, val);
	if (err)
		LOG_INF("%s prop:%d err:%d", __func__, prop, err);
	return err;
}

static void set_default_configuration(void)
{
	union charger_propval val = {0};
	if (CONFIG_CHARGER_CONSTANT_CHARGE_CURRENT_UA) {
		val.const_charge_current_ua = CONFIG_CHARGER_CONSTANT_CHARGE_CURRENT_UA;
		set_option(CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA, &val);
	}

	if (CONFIG_CHARGER_PRECHARGE_CURRENT_UA) {
		val.precharge_current_ua = CONFIG_CHARGER_PRECHARGE_CURRENT_UA;
		set_option(CHARGER_PROP_PRECHARGE_CURRENT_UA, &val);
	}

	if (CONFIG_CHARGER_CHARGE_TERM_CURRENT_UA) {
		val.charge_term_current_ua = CONFIG_CHARGER_CHARGE_TERM_CURRENT_UA;
		set_option(CHARGER_PROP_CHARGE_TERM_CURRENT_UA, &val);
	}

	if (CONFIG_CHARGER_CONSTANT_CHARGE_VOLTAGE_UV) {
		val.const_charge_voltage_uv = CONFIG_CHARGER_CONSTANT_CHARGE_VOLTAGE_UV;
		set_option(CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV, &val);
	}

	if (CONFIG_CHARGER_INPUT_REGULATION_CURRENT_UA) {
		val.input_current_regulation_current_ua = CONFIG_CHARGER_INPUT_REGULATION_CURRENT_UA;
		set_option(CHARGER_PROP_INPUT_REGULATION_CURRENT_UA, &val);
	}

	if (CONFIG_CHARGER_INPUT_REGULATION_VOLTAGE_UV) {
		val.input_voltage_regulation_voltage_uv = CONFIG_CHARGER_INPUT_REGULATION_VOLTAGE_UV;
		set_option(CHARGER_PROP_INPUT_REGULATION_VOLTAGE_UV, &val);
	}
}

static int charging_switch(bool enable)
{
	return charger_charge_enable(charger_dev, enable);
}

static int start_battery_state_check(void)
{
#if 0
	int err = gpio_pin_interrupt_configure_dt(&cso_gpio, GPIO_INT_DISABLE);

	if (!err) {
		/* Lock is not needed as interrupt is disabled */
		cso_counter = 1;
		k_work_reschedule(&error_check,
				      K_MSEC(ERROR_CHECK_TIMEOUT_MS));

		err = gpio_pin_interrupt_configure_dt(&cso_gpio,
						      GPIO_INT_EDGE_BOTH);
	}

	if (err) {
		LOG_ERR("Cannot start battery state check");
	}

	return err;
#endif
	return 0;
}

static int init_fn(void)
{
	int err;

	if (!device_is_ready(charger_dev)) {
		LOG_ERR("charger device not ready");
		return -ENODEV;
	}

//	charging_switch(false);
	set_default_configuration();
	k_work_init_delayable(&error_check, error_check_handler);
	k_work_reschedule(&error_check, K_MSEC(ERROR_CHECK_TIMEOUT_MS));
	err = start_battery_state_check();
	if (err) {
		goto error;
	}
error:
	return err;
}

static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_module_state_event(aeh)) {
		struct module_state_event *event = cast_module_state_event(aeh);

		if (check_state(event, MODULE_ID(main), MODULE_STATE_READY)) {
			static bool initialized;

			__ASSERT_NO_MSG(!initialized);
			initialized = true;

			int err = init_fn();
            
			err = 0;

			if (err) {
				module_set_state(MODULE_STATE_ERROR);
			} else {
				module_set_state(MODULE_STATE_READY);
				atomic_set(&active, true);
			}
		}

		return false;
	}

	if (is_wake_up_event(aeh)) {
		if (!atomic_get(&active)) {
			atomic_set(&active, true);

			int err = start_battery_state_check();

			if (!err) {
				module_set_state(MODULE_STATE_READY);
			} else {
				module_set_state(MODULE_STATE_ERROR);
			}
		}

		return false;
	}

	if (is_power_down_event(aeh)) {
		if (atomic_get(&active)) {
			atomic_set(&active, false);

			/* Cancel cannot fail if executed from another work's context. */
			(void)k_work_cancel_delayable(&error_check);

		}

		return false;
	}

	/* If event is unhandled, unsubscribe. */
	__ASSERT_NO_MSG(false);

	return false;
}
APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, module_state_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, power_down_event);
APP_EVENT_SUBSCRIBE(MODULE, wake_up_event);
