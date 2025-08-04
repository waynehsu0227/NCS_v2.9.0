/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT ti_bq2561x

#include "zephyr/device.h"
#include "zephyr/drivers/charger.h"
#include "zephyr/drivers/i2c.h"
#include "zephyr/kernel.h"
#include "zephyr/sys/util.h"
#include "zephyr/logging/log.h"
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(BQ2561X, LOG_LEVEL_DBG);

#include "bq2561x.h"

struct bq2561x_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec ce_gpio;
	struct gpio_dt_spec int_gpio;
};

struct bq2561x_data {
	const struct device          *dev;
	struct gpio_callback         irq_gpio_cb;
	struct k_work                trigger_handler_work;
	struct k_spinlock            lock;
//	uint8_t ss_reg;
//	unsigned int ichg_ua;
//	unsigned int vreg_uv;
	enum charger_status state;
	enum charger_online online;
	struct k_work_delayable update_status_work;
	uint8_t batfet_disable;
	uint8_t vbus_stat, chrg_stat, pg_stat;
	uint8_t chrg_fault, bat_fault, ntc_fault;
};
#define UPDATE_STATUS_TIMEOUT_MS 1000
static int update_reg_ctrl_3(const struct device *dev)
{
	const struct bq2561x_config *const config = dev->config;
	struct bq2561x_data *data = dev->data;
	uint8_t tmp;

	int ret = i2c_reg_read_byte_dt(&config->i2c, BQ2561X_CHARGER_CONTROL_3, &tmp);
	if (ret) {
		LOG_ERR("get BQ2561X_CHARGER_CONTROL_3 err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ2561X_CHARGER_CONTROL_3=0x%02X", tmp);
	data->batfet_disable = FIELD_GET(BQ2561X_BATFET_DIS, tmp);
	return 0;
}

static int update_reg_status_0(const struct device *dev)
{
	const struct bq2561x_config *const config = dev->config;
	struct bq2561x_data *data = dev->data;
	uint8_t tmp;

	int ret = i2c_reg_read_byte_dt(&config->i2c, BQ2561X_CHARGER_STATUS_0, &tmp);
	if (ret) {
		LOG_ERR("get BQ2561X_CHARGER_STATUS_0 err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ2561X_CHARGER_STATUS_0=0x%02X", tmp);
	data->vbus_stat = FIELD_GET(BQ2561X_VBUS_STAT_MASK, tmp);
	data->chrg_stat = FIELD_GET(BQ2561X_CHRG_STAT_MASK, tmp);
	data->pg_stat = FIELD_GET(BQ2561X_PG_STAT, tmp);
	return 0;
}

static int update_reg_status_1(const struct device *dev)
{
	const struct bq2561x_config *const config = dev->config;
	struct bq2561x_data *data = dev->data;
	uint8_t tmp;

	int ret = i2c_reg_read_byte_dt(&config->i2c, BQ2561X_CHARGER_STATUS_1, &tmp);
	if (ret) {
		LOG_ERR("get BQ2561X_CHARGER_STATUS_1 err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ2561X_CHARGER_STATUS_1=0x%02X", tmp);
		ret = i2c_reg_read_byte_dt(&config->i2c, BQ2561X_CHARGER_STATUS_1, &tmp);
	if (ret) {
		LOG_ERR("get BQ2561X_CHARGER_STATUS_1 err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ2561X_CHARGER_STATUS_1=0x%02X", tmp);
	data->chrg_fault = FIELD_GET(BQ2561X_CHRG_FAULT_MASK, tmp);
	data->bat_fault = FIELD_GET(BQ2561X_BAT_FAULT, tmp);
	data->ntc_fault = FIELD_GET(BQ2561X_NTC_FAULT_MASK, tmp);
	return 0;
}

static int update_reg_status_2(const struct device *dev)
{
	const struct bq2561x_config *const config = dev->config;
//	struct bq2561x_data *data = dev->data;
	uint8_t tmp;

	int ret = i2c_reg_read_byte_dt(&config->i2c, BQ2561X_CHARGER_STATUS_2, &tmp);
	if (ret) {
		LOG_ERR("get BQ2561X_CHARGER_STATUS_2 err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ2561X_CHARGER_STATUS_2=0x%02X", tmp);
		ret = i2c_reg_read_byte_dt(&config->i2c, BQ2561X_CHARGER_STATUS_2, &tmp);
	if (ret) {
		LOG_ERR("get BQ2561X_CHARGER_STATUS_2 err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ2561X_CHARGER_STATUS_2=0x%02X", tmp);
	return 0;
}

static int set_charger_setting(const struct device *dev)
{
	const struct bq2561x_config *const config = dev->config;

	uint8_t v = FIELD_PREP(BQ2561X_SYS_MIN_MASK, BQ2561X_SYS_MIN_3V);
	int ret = i2c_reg_update_byte_dt(&config->i2c,
								BQ2561X_CHARGER_CONTROL_0,
								BQ2561X_SYS_MIN_MASK,
								v);
	if (ret) {
		LOG_ERR("set BQ2561X_CHARGER_CONTROL_0 SYS_MIN err:%d", ret);
		return ret;
	}

	ret = i2c_reg_update_byte_dt(&config->i2c,
								BQ2561X_CHARGER_CONTROL_1,
								BQ2561X_WATCHDOG_MASK,
								BQ2561X_WATCHDOG_DIS);
	if (ret) {
		LOG_ERR("set BQ2561X_CHARGER_CONTROL_1 WATCHDOG err:%d", ret);
		return ret;
	}

	ret = i2c_reg_update_byte_dt(&config->i2c, 
								BQ2561X_CHARGER_CONTROL_1,
								BQ2561X_JEITA_VSET,
								BQ2561X_JEITA_VSET);
	if (ret) {
		LOG_ERR("set BQ2561X_CHARGER_CONTROL_1 JEITA_VSET err:%d", ret);
		return ret;
	}

	ret = i2c_reg_update_byte_dt(&config->i2c, 
								BQ2561X_CHARGER_CONTROL_4,
								BQ2561X_JEITA_WARM_ISET_MASK,
								BQ2561X_JEITA_WARM_ISET_20);
	if (ret) {
		LOG_ERR("set BQ2561X_CHARGER_CONTROL_4 JEITA_WARM_ISET err:%d", ret);
		return ret;
	}
	return 0;
}

static int update_charge_current_status(const struct device *dev)
{
	int ret;

	ret = update_reg_ctrl_3(dev);
	if (ret) {
		return ret;
	}

	ret = update_reg_status_0(dev);
	if (ret) {
		return ret;
	}

	ret = update_reg_status_1(dev);
	if (ret) {
		return ret;
	}

	ret = update_reg_status_2(dev);
	if (ret) {
		return ret;
	}
	return 0;
}

static void update_status_handler(struct k_work *work)
{
	LOG_DBG("%s", __func__);
	struct k_work_delayable *d_work = k_work_delayable_from_work(work);
	struct bq2561x_data *data = CONTAINER_OF(d_work, struct bq2561x_data,
						     update_status_work);
	const struct device *dev = data->dev;

	update_charge_current_status(dev);
	
	if (data->pg_stat && !data->batfet_disable) //online
		k_work_reschedule(&data->update_status_work, K_MSEC(UPDATE_STATUS_TIMEOUT_MS));
}


static int bq2561x_charger_get_online(const struct device *dev, enum charger_online *online)
{
//	LOG_DBG("%s", __func__);
	struct bq2561x_data *data = dev->data;
	if (data->pg_stat && !data->batfet_disable) {
		*online = CHARGER_ONLINE_FIXED;
	} else {
		*online = CHARGER_ONLINE_OFFLINE;
	}
	return 0;
}

static int bq2561x_charger_get_status(const struct device *dev, enum charger_status *status)
{
//	LOG_DBG("%s", __func__);
	struct bq2561x_data *data = dev->data;
	if (data->vbus_stat ==  BQ2561X_CHRG_STAT_NO_INPUT ||
		data->vbus_stat == BQ2561X_CHRG_STAT_BOOST_MODE)
		*status = CHARGER_STATUS_NOT_CHARGING;
	else {
		switch (data->chrg_stat) {
			case BQ2561X_CHRG_STAT_NOT_CHRGING:
				*status = CHARGER_STATUS_NOT_CHARGING;
				break;
			case BQ2561X_CHRG_STAT_PRECHRGING:
			case BQ2561X_CHRG_STAT_FAST_CHRGING:
				*status = CHARGER_STATUS_CHARGING;
				break;
			case BQ2561X_CHRG_STAT_CHRG_TERM:
				*status = CHARGER_STATUS_FULL;
				break;
			default:
				*status = CHARGER_STATUS_UNKNOWN;
				break;
		}
	}
	return 0;
}

static int bq2561x_charger_get_charge_type(const struct device *dev,
					   enum charger_charge_type *charge_type)
{
//	LOG_DBG("%s", __func__);
	struct bq2561x_data *data = dev->data;
	switch(data->chrg_stat)
	{
		case BQ2561X_CHRG_STAT_NOT_CHRGING:
			*charge_type = CHARGER_CHARGE_TYPE_NONE;
			break;
		case BQ2561X_CHRG_STAT_PRECHRGING:
			*charge_type = CHARGER_CHARGE_TYPE_TRICKLE;
			break;
		case BQ2561X_CHRG_STAT_FAST_CHRGING:
		case BQ2561X_CHRG_STAT_CHRG_TERM:
			*charge_type = CHARGER_CHARGE_TYPE_FAST;
			break;
		default:
			*charge_type = CHARGER_CHARGE_TYPE_UNKNOWN;
	}
	return 0;
}

static int bq2561x_charger_get_health(const struct device *dev, enum charger_health *health)
{
//	LOG_DBG("%s", __func__);
	struct bq2561x_data *data = dev->data;
	if (data->chrg_fault) {
		switch (data->chrg_fault) {
			case BQ2561X_CHRG_FAULT_INPUT:
				*health = CHARGER_HEALTH_OVERVOLTAGE;
				break;
			case BQ2561X_CHRG_FAULT_THERMAL:
				*health = CHARGER_HEALTH_OVERHEAT;
				break;
			case BQ2561X_CHRG_FAULT_TIMER:
				*health = CHARGER_HEALTH_SAFETY_TIMER_EXPIRE;
				break;
			default:
				*health = CHARGER_HEALTH_UNKNOWN;
		}
	} else if (data->bat_fault) {
		*health = CHARGER_HEALTH_OVERVOLTAGE;
	} else if (data->ntc_fault) {
		switch (data->ntc_fault) {
			case BQ2561X_NTC_FAULT_WARM:
				*health = CHARGER_HEALTH_WARM;
				break;
			case BQ2561X_NTC_FAULT_COOL:
				*health = CHARGER_HEALTH_COOL;
				break;
			case BQ2561X_NTC_FAULT_COLD:
				*health = CHARGER_HEALTH_COLD;
				break;
			case BQ2561X_NTC_FAULT_HOT:
				*health = CHARGER_HEALTH_HOT;
				break;
			default:
				*health = CHARGER_HEALTH_UNKNOWN;
		}
	} else {
		*health = CHARGER_HEALTH_GOOD;
	}

	return 0;
}


static int bq2561x_set_constant_charge_current(const struct device *dev, uint32_t current_ua)
{
	LOG_DBG("%s=%duA", __func__, current_ua);
	const struct bq2561x_config *const config = dev->config;
	uint8_t v;
	static const uint32_t SPEC_table_uA[] = {BQ2561X_ICHG_SPEC_VAL_uA};
	int i;
	for (i = 0; i < ARRAY_SIZE(SPEC_table_uA); i ++) {
		if (current_ua > SPEC_table_uA[i] || current_ua == SPEC_table_uA[i]) {
			current_ua = SPEC_table_uA[i];
			v = BQ2561X_ICHG_VAL_MAX - i;
			break;
		}
	}
	if (i == ARRAY_SIZE(SPEC_table_uA)) {
		current_ua = CLAMP(current_ua, BQ2561X_ICHG_MIN_uA, BQ2561X_ICHG_MAX_uA);
		v = (current_ua - BQ2561X_ICHG_MIN_uA) / BQ2561X_ICHG_STEP_uA;
	}
	LOG_DBG("set ICHG=0x%02X-%duA", v, current_ua);
	v = FIELD_PREP(BQ2561X_ICHG_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ2561X_CHARGE_CURRENT_LIMIT, BQ2561X_ICHG_MASK, v);
}

static int bq2561x_set_precharge_current(const struct device *dev, uint32_t current_ua)
{
	LOG_DBG("%s=%duA", __func__, current_ua);
	const struct bq2561x_config *const config = dev->config;
	current_ua = CLAMP(current_ua, BQ2561X_IPRECHG_MIN_uA, BQ2561X_IPRECHG_MAX_uA);
	uint8_t v;
	v = (current_ua - BQ2561X_IPRECHG_MIN_uA) / BQ2561X_IPRECHG_STEP_uA;
	LOG_DBG("set IPRECHG=0x%02X-%duA", v, current_ua);
	v = FIELD_PREP(BQ2561X_IPRECHG_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ2561X_PRECHG_AND_TERM_CURR_LIM, BQ2561X_IPRECHG_MASK, v);
}

static int bq2561x_set_charge_term_current(const struct device *dev, uint32_t current_ua)
{
	LOG_DBG("%s=%duA", __func__, current_ua);
	const struct bq2561x_config *const config = dev->config;
	current_ua = CLAMP(current_ua, BQ2561X_ITERM_MIN_uA, BQ2561X_ITERM_MAX_uA);
	uint8_t v;
	v = (current_ua - BQ2561X_ITERM_MIN_uA) / BQ2561X_ITERM_STEP_uA;
	LOG_DBG("set ITERM=0x%02X-%duA", v, current_ua);
	v = FIELD_PREP(BQ2561X_ITERM_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ2561X_PRECHG_AND_TERM_CURR_LIM, BQ2561X_ITERM_MASK, v);
}

static int bq2561x_set_constant_charge_voltage(const struct device *dev, uint32_t voltage_uv)
{
	LOG_DBG("%s=%duV", __func__, voltage_uv);
	const struct bq2561x_config *const config = dev->config;
	static const uint32_t SPEC_table_mv[] = {BQ2561X_VBATREG_SPEC_VAL_mV};
	uint8_t v;
	uint32_t voltage_mv = voltage_uv/1000;
	int i;
	for (i = 0; i < ARRAY_SIZE(SPEC_table_mv); i ++) {
		if (voltage_mv < SPEC_table_mv[i] || voltage_mv == SPEC_table_mv[i]) {
			voltage_mv = SPEC_table_mv[i];
			v = i;
			break;
		}
	}
	if (i == ARRAY_SIZE(SPEC_table_mv)) {
		voltage_mv = CLAMP(voltage_mv, BQ2561X_VBATREG_MIN_mV, BQ2561X_VBATREG_MAX_mV);
		v = (voltage_mv - BQ2561X_VBATREG_MIN_mV) / BQ2561X_VBATREG_STEP_mV + i;
	}
	LOG_DBG("set VBATREG=0x%02X-%dmV", v, voltage_mv);
	v = FIELD_PREP(BQ2561X_VBATREG_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ2561X_BATTERY_VOLTAGE_LIMIT, BQ2561X_VBATREG_MASK, v);
}

static int bq2561x_set_input_regulation_current(const struct device *dev, uint32_t current_ua)
{
	LOG_DBG("%s=%duA", __func__, current_ua);
	const struct bq2561x_config *const config = dev->config;
	current_ua = CLAMP(current_ua, BQ2561X_IINDPM_MIN_uA, BQ2561X_IINDPM_MAX_uA);
	uint8_t v;
	v = (current_ua - BQ2561X_IINDPM_MIN_uA) / BQ2561X_IINDPM_STEP_uA;
	LOG_DBG("set IINDPM=0x%02X-%duA", v, current_ua);
	v = FIELD_PREP(BQ2561X_IINDPM_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ2561X_INPUT_CURRENT_LIMIT, BQ2561X_IINDPM_MASK, v);
}

static int bq2561x_set_input_regulation_voltage(const struct device *dev, uint32_t voltage_uv)
{
	LOG_DBG("%s=%duV", __func__, voltage_uv);
	const struct bq2561x_config *const config = dev->config;
	uint8_t v;
	uint32_t voltage_mv = voltage_uv/1000;

	voltage_mv = CLAMP(voltage_mv, BQ2561X_VINDPM_MIN_mV, BQ2561X_VINDPM_MAX_mV);
	v = (voltage_mv - BQ2561X_VINDPM_MIN_mV) / BQ2561X_VINDPM_STEP_mV;
	LOG_DBG("set VINDPM=0x%02X-%dmV", v, voltage_mv);
	v = FIELD_PREP(BQ2561X_VINDPM_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ2561X_CHARGER_CONTROL_2, BQ2561X_VINDPM_MASK, v);
}


static int bq2561x_get_prop(const struct device *dev, charger_prop_t prop,
			    union charger_propval *val)
{
//	LOG_DBG("%s", __func__);

	switch (prop) {
	case CHARGER_PROP_ONLINE:
		return bq2561x_charger_get_online(dev, &val->online);
	case CHARGER_PROP_STATUS:
		return bq2561x_charger_get_status(dev, &val->status);
	case CHARGER_PROP_CHARGE_TYPE:
		return bq2561x_charger_get_charge_type(dev, &val->charge_type);
	case CHARGER_PROP_HEALTH:
		return bq2561x_charger_get_health(dev, &val->health);

	default:
		return -ENOTSUP;
	}

}

static int bq2561x_set_prop(const struct device *dev, charger_prop_t prop,
			    const union charger_propval *val)
{
//	LOG_DBG("%s", __func__);

	switch (prop) {

	case CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA:
		return bq2561x_set_constant_charge_current(dev, val->const_charge_current_ua);
	case CHARGER_PROP_PRECHARGE_CURRENT_UA:
		return bq2561x_set_precharge_current(dev, val->precharge_current_ua);
	case CHARGER_PROP_CHARGE_TERM_CURRENT_UA:
		return bq2561x_set_charge_term_current(dev, val->charge_term_current_ua);
	case CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV:
		return bq2561x_set_constant_charge_voltage(dev, val->const_charge_voltage_uv);
	case CHARGER_PROP_INPUT_REGULATION_CURRENT_UA:
		return bq2561x_set_input_regulation_current(dev, val->input_current_regulation_current_ua);
	case CHARGER_PROP_INPUT_REGULATION_VOLTAGE_UV:
		return bq2561x_set_input_regulation_voltage(dev, val->input_voltage_regulation_voltage_uv);
	default:
		return -ENOTSUP;
	}
}

static int bq2561x_register_reset(const struct device *dev)
{
	const struct bq2561x_config *const config = dev->config;
	int ret, limit = BQ2561X_RESET_MAX_TRIES;
	uint8_t val;
	do {
		ret = i2c_reg_write_byte_dt(&config->i2c,
									BQ2561X_PART_INFORMATION,
									BQ2561X_RESET_MASK);
		if (!ret)
			break;
	} while(--limit);
	if (!limit && ret) {
		LOG_ERR("Reset reg write timeout");
		return ret;
	} else {
		LOG_DBG("Reset reg write %d times", BQ2561X_RESET_MAX_TRIES -limit + 1);
	}
	
	limit = BQ2561X_RESET_MAX_TRIES;
	do {
		ret = i2c_reg_read_byte_dt(&config->i2c, BQ2561X_PART_INFORMATION, &val);
		if (ret) {
			return ret;
		}
		val = FIELD_GET(BQ2561X_RESET_MASK, val);
		if (!val)
			break;
	} while(--limit);

	if (!limit && val) {
		LOG_ERR("Reset reg read timeout val:0x%02X", val);
		return -ETIME;
	} else {
		LOG_DBG("Reset reg read %d times", BQ2561X_RESET_MAX_TRIES -limit + 1);
	}

	return 0;
}

static int bq2561x_charge_enable(const struct device *dev, const bool enable)
{
	LOG_DBG("%s:%s", __func__, enable ? "true":"false");

	const struct bq2561x_config *cfg = dev->config;
	uint8_t value = enable ? BQ2561X_CHG_CONFIG_MASK : 0;
	int ret;

	ret = i2c_reg_update_byte_dt(&cfg->i2c, BQ2561X_CHARGER_CONTROL_0,
				     BQ2561X_CHG_CONFIG_MASK, value);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static void trigger_handler(struct k_work *work)
{
	LOG_DBG("%s", __func__);
//	sensor_trigger_handler_t handler;
	int err = 0;
	struct bq2561x_data *data = CONTAINER_OF(work, struct bq2561x_data,
						     trigger_handler_work);
	const struct device *dev = data->dev;
	const struct bq2561x_config *cfg = dev->config;
	k_spinlock_key_t key;
#if 0
	k_spinlock_key_t key = k_spin_lock(&data->lock);
	update_charge_current_status(dev);
	k_spin_unlock(&data->lock, key);
#endif
	k_work_reschedule(&data->update_status_work, K_NO_WAIT);

/*
	k_spinlock_key_t key = k_spin_lock(&data->lock);

	handler = data->data_ready_handler;
	k_spin_unlock(&data->lock, key);

	if (!handler) {
		return;
	}

	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};

	handler(dev, &trig);
*/

	key = k_spin_lock(&data->lock);
//	if (data->data_ready_handler) {
		err = gpio_pin_interrupt_configure_dt(&cfg->int_gpio,
						      GPIO_INT_LEVEL_ACTIVE);
//	}
	k_spin_unlock(&data->lock, key);

	if (unlikely(err)) {
		LOG_ERR("Cannot re-enable IRQ");
		k_panic();
	}

}

static void irq_handler(const struct device *gpiob, struct gpio_callback *cb,
			uint32_t pins)
{
	LOG_DBG("%s", __func__);

	struct bq2561x_data *data = CONTAINER_OF(cb, struct bq2561x_data,
						 irq_gpio_cb);
	const struct device *dev = data->dev;
	const struct bq2561x_config *config = dev->config;

	k_spinlock_key_t key = k_spin_lock(&data->lock);
	int err = gpio_pin_interrupt_configure_dt(&config->int_gpio,
					      GPIO_INT_DISABLE);
	if (unlikely(err)) {
		LOG_ERR("Cannot disable IRQ");
		k_panic();
	}
	k_spin_unlock(&data->lock, key);

	k_work_submit(&data->trigger_handler_work);
}

static int bq2561x_init_irq(const struct device *dev)
{
	int err;
	struct bq2561x_data *data = dev->data;
	const struct bq2561x_config *config = dev->config;

	if (!device_is_ready(config->int_gpio.port)) {
		LOG_ERR("IRQ GPIO device not ready");
		return -ENODEV;
	}

	err = gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT);
	if (err) {
		LOG_ERR("Cannot configure IRQ GPIO");
		return err;
	}

	gpio_init_callback(&data->irq_gpio_cb, irq_handler,
			   BIT(config->int_gpio.pin));

	err = gpio_add_callback(config->int_gpio.port,
				&data->irq_gpio_cb);

	if (err) {
		LOG_ERR("Cannot add IRQ GPIO callback");
	}

	k_spinlock_key_t key = k_spin_lock(&data->lock);

	err = gpio_pin_interrupt_configure_dt(&config->int_gpio,
						      GPIO_INT_LEVEL_ACTIVE);
							  //GPIO_INT_EDGE_TO_ACTIVE);

	k_spin_unlock(&data->lock, key);

	if (err < 0) {
		LOG_ERR("Could not enable pin callback");
		return err;
	}

	return err;
}

static int bq2561x_init(const struct device *dev)
{
	struct bq2561x_data *data = dev->data;
	data->dev = dev;
	const struct bq2561x_config *const config = dev->config;
	int err;

	if (!gpio_is_ready_dt(&config->int_gpio)) {
		LOG_ERR("%s: INT gpio is not ready", dev->name);
		return -ENODEV;
	}

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("%s: I2C device not ready", dev->name);
		return -ENODEV;
	}
	k_work_init(&data->trigger_handler_work, trigger_handler);
	err = bq2561x_init_irq(dev);
	if (err) {
		return err;
	}

	err = bq2561x_register_reset(dev);
	if (err) {
		return err;
	}

	set_charger_setting(dev);

	err = update_charge_current_status(dev);
	if (err) {
		return err;
	}
	k_work_init_delayable(&data->update_status_work, update_status_handler);
	return 0;
};

static const struct charger_driver_api bq2561x_driver_api = {
	.get_property = bq2561x_get_prop,
	.set_property = bq2561x_set_prop,
	.charge_enable = bq2561x_charge_enable,
};

#define BQ2561X_INIT(inst)                                              \
                                                                        \
	static struct bq2561x_data bq2561x_data_##inst;				        \
	static const struct bq2561x_config bq2561x_config_##inst = {        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, int_gpios),	      		\
		.i2c = I2C_DT_SPEC_INST_GET(inst),                              \
	};                                                                  \
                                                                        \
	DEVICE_DT_INST_DEFINE(inst,bq2561x_init, NULL,						\
						&bq2561x_data_##inst,                      		\
			      		&bq2561x_config_##inst,							\
						POST_KERNEL, CONFIG_CHARGER_INIT_PRIORITY,		\
			      		&bq2561x_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BQ2561X_INIT)
