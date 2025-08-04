/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT ti_bq24295

#include "zephyr/device.h"
#include "zephyr/drivers/charger.h"
#include "zephyr/drivers/i2c.h"
#include "zephyr/kernel.h"
#include "zephyr/sys/util.h"
#include "zephyr/logging/log.h"
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(BQ24295, LOG_LEVEL_DBG);

#include "bq24295.h"

//#define awmsg 0
#ifdef awmsg
    #define awprintk(...) printk(__VA_ARGS__)
#else
    #define awprintk(...)  
#endif



struct bq24295_config {
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec ce_gpio;
	struct gpio_dt_spec int_gpio;
};

struct bq24295_data {
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
static int update_reg_operation_ctrl(const struct device *dev)
{
	const struct bq24295_config *const config = dev->config;
	struct bq24295_data *data = dev->data;
	uint8_t tmp;

	int ret = i2c_reg_read_byte_dt(&config->i2c, BQ24295_MISC_OPERATION_CONTROL, &tmp);
	if (ret) {
		LOG_ERR("get BQ24295_MISC_OPERATION_CONTROL err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ24295_MISC_OPERATION_CONTROL=0x%02X", tmp);
	data->batfet_disable = FIELD_GET(BQ24295_BATFET_DISABLE_MASK, tmp);
	return 0;
}

static int update_reg_system_status(const struct device *dev)
{
	const struct bq24295_config *const config = dev->config;
	struct bq24295_data *data = dev->data;
	uint8_t tmp;

	int ret = i2c_reg_read_byte_dt(&config->i2c, BQ24295_SYSTEM_STATUS, &tmp);
	if (ret) {
		LOG_ERR("get BQ24295_SYSTEM_STATUS err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ24295_SYSTEM_STATUS=0x%02X", tmp);
	data->vbus_stat = FIELD_GET(BQ24295_VBUS_STAT_MASK, tmp);
	data->chrg_stat = FIELD_GET(BQ24295_CHRG_STAT_MASK, tmp);
	data->pg_stat = FIELD_GET(BQ24295_PG_STAT, tmp);
	return 0;
}

static int update_reg_new_fault(const struct device *dev)
{
	const struct bq24295_config *const config = dev->config;
	struct bq24295_data *data = dev->data;
	uint8_t tmp;

	int ret = i2c_reg_read_byte_dt(&config->i2c, BQ24295_NEW_FAULT, &tmp);
	if (ret) {
		LOG_ERR("get BQ24295_NEW_FAULT err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ24295_NEW_FAULT=0x%02X", tmp);
		ret = i2c_reg_read_byte_dt(&config->i2c, BQ24295_NEW_FAULT, &tmp);
	if (ret) {
		LOG_ERR("get BQ24295_NEW_FAULT err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ24295_NEW_FAULT=0x%02X", tmp);
	data->chrg_fault = FIELD_GET(BQ24295_CHRG_FAULT_MASK, tmp);
	data->bat_fault = FIELD_GET(BQ24295_BAT_FAULT_MASK, tmp);
	data->ntc_fault = FIELD_GET(BQ24295_NTC_FAULT_MASK, tmp);
	return 0;
}

#if 0
static int update_reg_status_2(const struct device *dev)
{
	const struct bq24295_config *const config = dev->config;
//	struct bq24295_data *data = dev->data;
	uint8_t tmp;

	int ret = i2c_reg_read_byte_dt(&config->i2c, BQ24295_CHARGER_STATUS_2, &tmp);
	if (ret) {
		LOG_ERR("get BQ24295_CHARGER_STATUS_2 err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ24295_CHARGER_STATUS_2=0x%02X", tmp);
		ret = i2c_reg_read_byte_dt(&config->i2c, BQ24295_CHARGER_STATUS_2, &tmp);
	if (ret) {
		LOG_ERR("get BQ24295_CHARGER_STATUS_2 err:%d", ret);
		return ret;
	}
	LOG_DBG("get BQ24295_CHARGER_STATUS_2=0x%02X", tmp);
	return 0;
}
#endif
static int set_charger_setting(const struct device *dev)
{
	const struct bq24295_config *const config = dev->config;

	uint8_t v = FIELD_PREP(BQ24295_SYS_MIN_MASK, BQ24295_SYS_MIN_DEF_3V);
	int ret = i2c_reg_update_byte_dt(&config->i2c,
								BQ24295_POWER_ON_CONFIG,
								BQ24295_SYS_MIN_MASK,
								v);
	if (ret) {
		LOG_ERR("set BQ24295_POWER_ON_CONFIG SYS_MIN err:%d", ret);
		return ret;
	}

	v = FIELD_PREP(BQ24295_WATCHDOG_MASK, BQ24295_WATCHDOG_DISABLE);
	ret = i2c_reg_update_byte_dt(&config->i2c,
								BQ24295_CHARGER_TERM_TIMER_CONTROL,
								BQ24295_WATCHDOG_MASK,
								v);
	if (ret) {
		LOG_ERR("set BQ24295_CHARGER_TERM_TIMER_CONTROL WATCHDOG err:%d", ret);
		return ret;
	}
#if 0
	v = FIELD_PREP(BQ24295_ICHG_MASK, BQ24295_USER_ICHG_FCCL);
	ret = i2c_reg_update_byte_dt(&config->i2c, 
								BQ24295_CHARGE_CURRENT_CONTROL,
								BQ24295_ICHG_MASK,
								v);
	if (ret) {
		LOG_ERR("set BQ24295_CHARGE_CURRENT_CONTROL Fast Charge Current Limit err:%d", ret);
		return ret;
	}

	v = FIELD_PREP(BQ24295_VREG_MASK, BQ24295_USER_CVL);	
	ret = i2c_reg_update_byte_dt(&config->i2c, 
								BQ24295_CHARGE_VOLTAGE_CONTROL,
								BQ24295_VREG_MASK,
								v);
	if (ret) {
		LOG_ERR("set BQ24295_CHARGE_VOLTAGE_CONTROL Charge Voltage Limit err:%d", ret);
		return ret;
	}
#endif
	return 0;
}

static int update_charge_current_status(const struct device *dev)
{
	int ret;

	ret = update_reg_operation_ctrl(dev);
	if (ret) {
		return ret;
	}

	ret = update_reg_system_status(dev);
	if (ret) {
		return ret;
	}

	ret = update_reg_new_fault(dev);
	if (ret) {
		return ret;
	}
#if 0
	ret = update_reg_status_2(dev);
	if (ret) {
		return ret;
	}
#endif
	return 0;
}

static void update_status_handler(struct k_work *work)
{
	LOG_DBG("%s", __func__);
	struct k_work_delayable *d_work = k_work_delayable_from_work(work);
	struct bq24295_data *data = CONTAINER_OF(d_work, struct bq24295_data,
						     update_status_work);
	const struct device *dev = data->dev;

	update_charge_current_status(dev);
	
	if (data->pg_stat && !data->batfet_disable) //online
		k_work_reschedule(&data->update_status_work, K_MSEC(UPDATE_STATUS_TIMEOUT_MS));
}


static int bq24295_charger_get_online(const struct device *dev, enum charger_online *online)
{
//	LOG_DBG("%s", __func__);
	struct bq24295_data *data = dev->data;
	if (data->pg_stat && !data->batfet_disable) {
		*online = CHARGER_ONLINE_FIXED;
	} else {
		*online = CHARGER_ONLINE_OFFLINE;
	}
	return 0;
}

static int bq24295_charger_get_status(const struct device *dev, enum charger_status *status)
{
//	LOG_DBG("%s", __func__);
	struct bq24295_data *data = dev->data;
	if (data->vbus_stat ==  BQ24295_VBUS_STAT_NO_INPUT ||
		data->vbus_stat == BQ24295_VBUS_STAT_OTG_MODE)
		*status = CHARGER_STATUS_NOT_CHARGING;
	else {
		switch (data->chrg_stat) {
			case BQ24295_CHRG_STAT_NOT_CHRGING:
				*status = CHARGER_STATUS_NOT_CHARGING;
				break;
			case BQ24295_CHRG_STAT_PRECHRGING:
			case BQ24295_CHRG_STAT_FAST_CHRGING:
				*status = CHARGER_STATUS_CHARGING;
				break;
			case BQ24295_CHRG_STAT_CHRG_TERM:
				*status = CHARGER_STATUS_FULL;
				break;
			default:
				*status = CHARGER_STATUS_UNKNOWN;
				break;
		}
	}
	return 0;
}

static int bq24295_charger_get_charge_type(const struct device *dev,
					   enum charger_charge_type *charge_type)
{
//	LOG_DBG("%s", __func__);
	struct bq24295_data *data = dev->data;
	switch(data->chrg_stat)
	{
		case BQ24295_CHRG_STAT_NOT_CHRGING:
			*charge_type = CHARGER_CHARGE_TYPE_NONE;
			break;
		case BQ24295_CHRG_STAT_PRECHRGING:
			*charge_type = CHARGER_CHARGE_TYPE_TRICKLE;
			break;
		case BQ24295_CHRG_STAT_FAST_CHRGING:
		case BQ24295_CHRG_STAT_CHRG_TERM:
			*charge_type = CHARGER_CHARGE_TYPE_FAST;
			break;
		default:
			*charge_type = CHARGER_CHARGE_TYPE_UNKNOWN;
	}
	return 0;
}

static int bq24295_charger_get_health(const struct device *dev, enum charger_health *health)
{
//	LOG_DBG("%s", __func__);
	struct bq24295_data *data = dev->data;
	if (data->chrg_fault) {
		switch (data->chrg_fault) {
			case BQ24295_CHRG_FAULT_INPUT:
				*health = CHARGER_HEALTH_OVERVOLTAGE;
				break;
			case BQ24295_CHRG_FAULT_THERMAL:
				*health = CHARGER_HEALTH_OVERHEAT;
				break;
			case BQ24295_CHRG_FAULT_TIMER:
				*health = CHARGER_HEALTH_SAFETY_TIMER_EXPIRE;
				break;
			default:
				*health = CHARGER_HEALTH_UNKNOWN;
		}
	} else if (data->bat_fault) {
		*health = CHARGER_HEALTH_OVERVOLTAGE;
	} else if (data->ntc_fault) {
		switch (data->ntc_fault) {
		
			case BQ24295_NTC_FAULT_COLD:
				*health = CHARGER_HEALTH_COLD;
				break;
			case BQ24295_NTC_FAULT_HOT:
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


static uint8_t bq24295_Set_Limit_and_check(uint32_t inout_val,uint16_t setmax, uint8_t outmax,uint16_t setmin, uint8_t outmin, uint16_t* bit_vals, uint8_t bit_vals_len)
{
	uint8_t reg_val = 0;
	awprintk("##aw inout_val=%d, setmax=%d, outmax=%x, setmin=%d, outmin=%x, bit_vals_len=%d\n",inout_val, setmax, outmax, setmin, outmin, bit_vals_len);
	if (inout_val<setmin)
	{
		awprintk("##aw outmin=%x\n",outmin);
		return outmin;
	}
	else if (inout_val>setmax)
	{
		awprintk("##aw outmax=%x\n",outmax);
		return outmax;
	}
	else
	{
		int startidx = bit_vals_len-1; 
		inout_val -= setmin;
		for (int i = startidx; i >= 0; i--) {
			if (inout_val >= bit_vals[i]) {
				reg_val |= (1 << i);
				inout_val -= bit_vals[i];
			}
		}
	}
	awprintk("##aw out reg_val=%x\n",reg_val);
    return reg_val;
}



static uint8_t bq24295_Set_Fast_Charge_Current_Limit_and_check(uint32_t target_ma)
{
	uint16_t bit_vals[] = {64, 128, 256, 512, 1024, 2048};
	 
    return bq24295_Set_Limit_and_check(target_ma,3008,0x27,512,0,bit_vals,6);
}


static int bq24295_set_constant_charge_current(const struct device *dev, uint32_t current_ua)
{
	LOG_DBG("%s=%duA", __func__, current_ua);
	const struct bq24295_config *const config = dev->config;
	uint8_t v;
	v = bq24295_Set_Fast_Charge_Current_Limit_and_check(current_ua/1000);
	LOG_DBG("input current=%dma, set ICHG=0x%02X",current_ua/1000, v);
	v = FIELD_PREP(BQ24295_ICHG_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ24295_CHARGE_CURRENT_CONTROL, BQ24295_ICHG_MASK, v);
}



static uint8_t bq24295_set_Pre_Charge_Current_Limit_and_check(uint32_t target_ma)
{	
    uint16_t bit_vals[] = {128, 256, 512, 1024};
	 
    return bq24295_Set_Limit_and_check(target_ma,2048,0xF,128,0,bit_vals,4);
}


static int bq24295_set_precharge_current(const struct device *dev, uint32_t current_ua)
{
	LOG_DBG("%s=%duA", __func__, current_ua);
	const struct bq24295_config *const config = dev->config;
	uint8_t v;
	v = bq24295_set_Pre_Charge_Current_Limit_and_check(current_ua/1000);
	LOG_DBG("set IPRECHG=0x%02X-%duA", v, current_ua/1000);
	v = FIELD_PREP(BQ24295_IPRECHG_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ24295_PRECHG_TERM_CURR_LIM, BQ24295_IPRECHG_MASK, v);
}

static uint8_t bq24295_set_Termination_Current_Limit_and_check(uint32_t target_ma)
{
	uint16_t bit_vals[] = {128, 256, 512, 1024};
	 
    return bq24295_Set_Limit_and_check(target_ma,2048,0xF,128,0,bit_vals,4);
}

static int bq24295_set_charge_term_current(const struct device *dev, uint32_t current_ua)
{
	LOG_DBG("%s=%duA", __func__, current_ua);
	const struct bq24295_config *const config = dev->config; 
	uint8_t v;
	v = bq24295_set_Termination_Current_Limit_and_check(current_ua/1000);
	LOG_DBG("set ITERM=0x%02X-%duA", v, current_ua/1000);
	v = FIELD_PREP(BQ24295_ITERM_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ24295_PRECHG_TERM_CURR_LIM, BQ24295_ITERM_MASK, v);
}



static uint8_t bq24295_Set_Charge_Voltage_Control(uint32_t target_mv)
{
	uint16_t bit_vals[] = {16, 32, 64, 128, 256, 512};
	 
    return bq24295_Set_Limit_and_check(target_mv,4400,0x38,3504,0,bit_vals,6);
}

static int bq24295_set_constant_charge_voltage(const struct device *dev, uint32_t voltage_uv)
{
	LOG_DBG("%s=%duV", __func__, voltage_uv);
	const struct bq24295_config *const config = dev->config;	
	uint8_t v;
	uint32_t voltage_mv = voltage_uv/1000;
	v = bq24295_Set_Charge_Voltage_Control(voltage_mv);
	LOG_DBG("set VBATREG=0x%02X-%dmV", v, voltage_mv);
	v = FIELD_PREP(BQ24295_VREG_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ24295_CHARGE_VOLTAGE_CONTROL, BQ24295_VREG_MASK, v);
}



static uint8_t bq24295_Set_Input_Current_Limit(uint32_t target_mv)
{
	uint16_t bit_vals[] = { 100, 150, 500, 900, 1000, 1500, 2000, 3000};
	uint8_t reg_val=0;
	 
	for (int i =7; i >= 0; i--) {
		if (target_mv >= bit_vals[i]) {
			reg_val = i;
			break;
		}
	}

    return reg_val;
}

static int bq24295_set_input_regulation_current(const struct device *dev, uint32_t current_ua)
{
	LOG_DBG("%s=%duA", __func__, current_ua);
	const struct bq24295_config *const config = dev->config;
	uint32_t current_ma = (current_ua/1000);
	uint8_t v;
	v = bq24295_Set_Input_Current_Limit(current_ma);
	LOG_DBG("set IINDPM=0x%02X-%dmA", v, current_ma);
	awprintk("##aw set IINDPM=0x%02X-%dmA\n", v, current_ma);
	v = FIELD_PREP(BQ24295_IINLIM_MASK, v);

	return i2c_reg_update_byte_dt(&config->i2c, BQ24295_INPUT_SOURCE_CONTROL, BQ24295_IINLIM_MASK, v);
}

static uint8_t bq24295_Set_Input_Voltage_Limit(uint32_t target_mv)
{
	uint16_t bit_vals[] = { 80, 160, 320, 640};	 
    return bq24295_Set_Limit_and_check(target_mv,5080,0xF,3880,0,bit_vals,4);
}

static int bq24295_set_input_regulation_voltage(const struct device *dev, uint32_t voltage_uv)
{
	LOG_DBG("%s=%duV", __func__, voltage_uv);
	const struct bq24295_config *const config = dev->config;
	uint8_t v;
	uint32_t voltage_mv = voltage_uv/1000;	
	v = bq24295_Set_Input_Voltage_Limit(voltage_mv);
	LOG_DBG("set VINDPM=0x%02X-%dmV", v, voltage_mv);
	v = FIELD_PREP(BQ24295_VINDPM_MASK, v);
	return i2c_reg_update_byte_dt(&config->i2c, BQ24295_INPUT_SOURCE_CONTROL, BQ24295_VINDPM_MASK, v);
}


static int bq24295_get_prop(const struct device *dev, charger_prop_t prop,
			    union charger_propval *val)
{
//	LOG_DBG("%s", __func__);

	switch (prop) {
	case CHARGER_PROP_ONLINE:
		return bq24295_charger_get_online(dev, &val->online);
	case CHARGER_PROP_STATUS:
		return bq24295_charger_get_status(dev, &val->status);
	case CHARGER_PROP_CHARGE_TYPE:
		return bq24295_charger_get_charge_type(dev, &val->charge_type);
	case CHARGER_PROP_HEALTH:
		return bq24295_charger_get_health(dev, &val->health);

	default:
		return -ENOTSUP;
	}

}

static int bq24295_set_prop(const struct device *dev, charger_prop_t prop,
			    const union charger_propval *val)
{
//	LOG_DBG("%s", __func__);

	switch (prop) {

	case CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA:
		return bq24295_set_constant_charge_current(dev, val->const_charge_current_ua);
	case CHARGER_PROP_PRECHARGE_CURRENT_UA:
		return bq24295_set_precharge_current(dev, val->precharge_current_ua);
	case CHARGER_PROP_CHARGE_TERM_CURRENT_UA:
		return bq24295_set_charge_term_current(dev, val->charge_term_current_ua);
	case CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV:
		return bq24295_set_constant_charge_voltage(dev, val->const_charge_voltage_uv);
	case CHARGER_PROP_INPUT_REGULATION_CURRENT_UA:
		return bq24295_set_input_regulation_current(dev, val->input_current_regulation_current_ua);
	case CHARGER_PROP_INPUT_REGULATION_VOLTAGE_UV:
		return bq24295_set_input_regulation_voltage(dev, val->input_voltage_regulation_voltage_uv);
	default:
		return -ENOTSUP;
	}
}

static int bq24295_register_reset(const struct device *dev)
{
	const struct bq24295_config *const config = dev->config;
	int ret, limit = BQ24295_RESET_MAX_TRIES;
	uint8_t val;
	do {
		ret = i2c_reg_write_byte_dt(&config->i2c,	BQ24295_POWER_ON_CONFIG,
			BQ24295_REG_RESET_MASK);
		if (!ret)
			break;
	} while(--limit);
	if (!limit && ret) {
		LOG_ERR("Reset reg write timeout");
		return ret;
	} else {
		LOG_DBG("Reset reg write %d times", BQ24295_RESET_MAX_TRIES -limit + 1);
	}
	
	limit = BQ24295_RESET_MAX_TRIES;
	do {
		ret = i2c_reg_read_byte_dt(&config->i2c, BQ24295_POWER_ON_CONFIG, &val);
		if (ret) {
			return ret;
		}
		val = FIELD_GET(BQ24295_REG_RESET_MASK, val);
		if (!val)
			break;
	} while(--limit);

	if (!limit && val) {
		LOG_ERR("Reset reg read timeout val:0x%02X", val);
		return -ETIME;
	} else {
		LOG_DBG("Reset reg read %d times", BQ24295_RESET_MAX_TRIES -limit + 1);
	}

	ret = i2c_reg_read_byte_dt(&config->i2c, BQ24295_VENDER_PART_REVISION_STATUS, &val);
	if (ret) {
		return ret;
	}
	val = FIELD_GET(BQ24295_PN_MASK, val);
	if (val == 0x06)
	{
		LOG_DBG("BQ24295 is Found!!!\n");
		awprintk("BQ24295 is Found!!!\n");
	}




	return 0;
}

static int bq24295_charge_enable(const struct device *dev, const bool enable)
{
	LOG_DBG("%s:%s", __func__, enable ? "true":"false");

	const struct bq24295_config *cfg = dev->config;
	uint8_t value = enable ? BQ24295_CHG_CONFIG_MASK : 0;
	int ret;

	ret = i2c_reg_update_byte_dt(&cfg->i2c, BQ24295_POWER_ON_CONFIG,
		BQ24295_CHG_CONFIG_MASK, value);
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
	struct bq24295_data *data = CONTAINER_OF(work, struct bq24295_data,
						     trigger_handler_work);
	const struct device *dev = data->dev;
	const struct bq24295_config *cfg = dev->config;
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

	struct bq24295_data *data = CONTAINER_OF(cb, struct bq24295_data,
						 irq_gpio_cb);
	const struct device *dev = data->dev;
	const struct bq24295_config *config = dev->config;

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

static int bq24295_init_irq(const struct device *dev)
{
	int err;
	struct bq24295_data *data = dev->data;
	const struct bq24295_config *config = dev->config;

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

static int bq24295_init(const struct device *dev)
{
	struct bq24295_data *data = dev->data;
	data->dev = dev;
	const struct bq24295_config *const config = dev->config;
	int err;
	awprintk("###aw bq24295_init 1\n ");
	if (!gpio_is_ready_dt(&config->int_gpio)) {
		LOG_ERR("%s: INT gpio is not ready", dev->name);
		return -ENODEV;
	}
	awprintk("###aw bq24295_init 2\n ");
	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("%s: I2C device not ready", dev->name);
		return -ENODEV;
	}
	awprintk("###aw bq24295_init 3\n ");
	k_work_init(&data->trigger_handler_work, trigger_handler);
	err = bq24295_init_irq(dev);
	if (err) {
		return err;
	}
	awprintk("###aw bq24295_init 4\n ");
	err = bq24295_register_reset(dev);
	if (err) {
		return err;
	}
	awprintk("###aw bq24295_init 5\n ");
	set_charger_setting(dev);
	awprintk("###aw bq24295_init 6\n ");
	err = update_charge_current_status(dev);
	if (err) {
		return err;
	}
	awprintk("###aw bq24295_init 7\n ");
	k_work_init_delayable(&data->update_status_work, update_status_handler);
	return 0;
};

static const struct charger_driver_api bq24295_driver_api = {
	.get_property = bq24295_get_prop,
	.set_property = bq24295_set_prop,
	.charge_enable = bq24295_charge_enable,
};

#define BQ24295_INIT(inst)                                              \
                                                                        \
	static struct bq24295_data bq24295_data_##inst;				        \
	static const struct bq24295_config bq24295_config_##inst = {        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, int_gpios),	      		\
		.i2c = I2C_DT_SPEC_INST_GET(inst),                              \
	};                                                                  \
                                                                        \
	DEVICE_DT_INST_DEFINE(inst,bq24295_init, NULL,						\
						&bq24295_data_##inst,                      		\
			      		&bq24295_config_##inst,							\
						POST_KERNEL, CONFIG_CHARGER_INIT_PRIORITY,		\
			      		&bq24295_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BQ24295_INIT)
