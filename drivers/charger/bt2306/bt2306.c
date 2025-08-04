/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT m3tek_bt2306

#include "zephyr/device.h"
#include "zephyr/drivers/charger.h"
#include "zephyr/kernel.h"
#include "zephyr/sys/util.h"
#include "zephyr/logging/log.h"
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(BT2306, LOG_LEVEL_DBG);

#include "bt2306.h"

#define awmsg 0
#ifdef awmsg
    #define awprintk(...) printk(__VA_ARGS__)
#else
    #define awprintk(...)  
#endif

 
typedef void (*user_layer_trigger_handler_t)(const struct device *dev, int int_input_state);	 

struct bt2306_Config {
	struct gpio_dt_spec int_gpio;
};

struct bt2306_data {
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
	user_layer_trigger_handler_t    user_layer_trigger_handler;

};
#define UPDATE_STATUS_TIMEOUT_MS 1000
 
 
 

static void update_status_handler(struct k_work *work)
{
	LOG_DBG("%s", __func__);
	struct k_work_delayable *d_work = k_work_delayable_from_work(work);
	struct bt2306_data *data = CONTAINER_OF(d_work, struct bt2306_data,
						     update_status_work);
	 	
	
	if (data->pg_stat && !data->batfet_disable) //online
		k_work_reschedule(&data->update_status_work, K_MSEC(UPDATE_STATUS_TIMEOUT_MS));
}


static int bt2306_Charger_get_online(const struct device *dev, enum charger_online *online)
{
 
 
	const struct bt2306_Config *config = dev->config;
	
	 
	int val = gpio_pin_get_raw(config->int_gpio.port, config->int_gpio.pin);
	if (val) {
		*online = CHARGER_ONLINE_FIXED;
		 
	} else {
		*online = CHARGER_ONLINE_OFFLINE;
		 
	}
	
	return 0;
}

static int bt2306_Charger_get_status(const struct device *dev, enum charger_status *status)
{
//	LOG_DBG("%s", __func__);
	
	struct bt2306_data *data = dev->data;

	if (data->vbus_stat ==  BT2306_VBUS_STAT_NO_INPUT )
		*status = CHARGER_STATUS_NOT_CHARGING;
	else
		*status = CHARGER_STATUS_CHARGING;
	 
	return 0;
}

static int bt2306_Charger_get_charge_type(const struct device *dev,
					   enum charger_charge_type *charge_type)
{
//	LOG_DBG("%s", __func__);
	
	*charge_type = CHARGER_CHARGE_TYPE_NONE;
		
	return 0;
}

static int bt2306_Charger_get_health(const struct device *dev, enum charger_health *health)
{

	*health = CHARGER_HEALTH_UNKNOWN;
	

	return 0;
}


 
static int bt2306_trigger_set(const struct device *dev,	user_layer_trigger_handler_t handler)
{
	struct bt2306_data *data = dev->data;

	data->user_layer_trigger_handler = handler;
	 

	return 0;
}




static int bt2306_get_prop(const struct device *dev, charger_prop_t prop,
			    union charger_propval *val)
{
//	LOG_DBG("%s", __func__);

	switch (prop) {
	case CHARGER_PROP_ONLINE:
		return bt2306_Charger_get_online(dev, &val->online);
	case CHARGER_PROP_STATUS:
		return bt2306_Charger_get_status(dev, &val->status);
	case CHARGER_PROP_CHARGE_TYPE:
		return bt2306_Charger_get_charge_type(dev, &val->charge_type);
	case CHARGER_PROP_HEALTH:
		return bt2306_Charger_get_health(dev, &val->health);

	default:
		return -ENOTSUP;
	}

}

static int bt2306_set_prop(const struct device *dev, charger_prop_t prop,
			    const union charger_propval *val)
{
//	LOG_DBG("%s", __func__);
	switch (prop) {

	case CHARGER_PROP_SYSTEM_VOLTAGE_NOTIFICATION_UV:
		return bt2306_trigger_set(dev, (user_layer_trigger_handler_t)val->system_voltage_notification);	
	default:
		return -ENOTSUP;
	}

	return 0;
}

 
static int bt2306_Charge_enable(const struct device *dev, const bool enable)
{
	LOG_DBG("%s:%s", __func__, enable ? "true":"false");

	awprintk("bt2306_Charge_enable has no function!\n");

	
	return 0;
}





static void trigger_handler(struct k_work *work)
{
	LOG_DBG("%s", __func__);
//	sensor_trigger_handler_t handler;
	int err = 0;
	struct bt2306_data *data = CONTAINER_OF(work, struct bt2306_data,
						     trigger_handler_work);
	const struct device *dev = data->dev;
	const struct bt2306_Config *cfg = dev->config;
	k_spinlock_key_t key;
 
	k_work_reschedule(&data->update_status_work, K_NO_WAIT);
	int val = gpio_pin_get_raw(cfg->int_gpio.port, cfg->int_gpio.pin);

	if (val == 1)
	{
		data->vbus_stat = BT2306_VBUS_STAT_ADAPTER;
	} else {		 
		data->vbus_stat = BT2306_VBUS_STAT_NO_INPUT;
	}

	if (data->user_layer_trigger_handler)
	{		
		data->user_layer_trigger_handler(dev,val);
	}
	
 
	key = k_spin_lock(&data->lock);
//	if (data->data_ready_handler) {
		err = gpio_pin_interrupt_configure_dt(&cfg->int_gpio,
								GPIO_INT_EDGE_BOTH);
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

	struct bt2306_data *data = CONTAINER_OF(cb, struct bt2306_data,
						 irq_gpio_cb);
	const struct device *dev = data->dev;
	const struct bt2306_Config *config = dev->config;

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

static int bt2306_init_irq(const struct device *dev)
{
	int err;
	struct bt2306_data *data = dev->data;
	const struct bt2306_Config *config = dev->config;

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

	err = gpio_pin_interrupt_configure_dt(&config->int_gpio,GPIO_INT_EDGE_BOTH);
							//GPIO_INT_EDGE_BOTH);
						      //GPIO_INT_LEVEL_ACTIVE);
							  //GPIO_INT_EDGE_TO_ACTIVE);

	k_spin_unlock(&data->lock, key);

	if (err < 0) {
		LOG_ERR("Could not enable pin callback");
		return err;
	}

	int val = gpio_pin_get_raw(config->int_gpio.port, config->int_gpio.pin);

	if (val == 1)
	{
		data->vbus_stat = BT2306_VBUS_STAT_ADAPTER;
	} else {		 
		data->vbus_stat = BT2306_VBUS_STAT_NO_INPUT;
	}

	return err;
}

static int bt2306_init(const struct device *dev)
{
	struct bt2306_data *data = dev->data;
	data->dev = dev;
	const struct bt2306_Config *const config = dev->config;
	int err;
	awprintk("###aw bt2306_init 1\n ");
	if (!gpio_is_ready_dt(&config->int_gpio)) {
		LOG_ERR("%s: INT gpio is not ready", dev->name);
		return -ENODEV;
	}
	
	k_work_init(&data->trigger_handler_work, trigger_handler);
	err = bt2306_init_irq(dev);
	if (err) {
		return err;
	}

	/*int val = gpio_pin_get_raw(config->int_gpio.port, config->int_gpio.pin);
	awprintk("test gpio val = %x\n",val);*/
 
	k_work_init_delayable(&data->update_status_work, update_status_handler);
	return 0;
};

static const struct charger_driver_api bt2306_driver_api = {
	.get_property = bt2306_get_prop,
	.set_property = bt2306_set_prop,
	.charge_enable = bt2306_Charge_enable,
};

#define bt2306_INIT(inst)                                              \
                                                                        \
	static struct bt2306_data bt2306_data_##inst;				        \
	static const struct bt2306_Config bt2306_Config_##inst = {        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, int_gpios),	      		\
	};                                                                  \
                                                                        \
	DEVICE_DT_INST_DEFINE(inst,bt2306_init, NULL,						\
						&bt2306_data_##inst,                      		\
			      		&bt2306_Config_##inst,							\
						POST_KERNEL, CONFIG_CHARGER_INIT_PRIORITY,		\
			      		&bt2306_driver_api);

DT_INST_FOREACH_STATUS_OKAY(bt2306_INIT)
