/*
 * Copyright (c) 2020 Seagate Technology LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ti_lp50xx

/**
 * @file
 * @brief LP50xx LED controller
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/led.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "lp50xx.h"

#define LOG_LEVEL CONFIG_LED_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lp50xx);

#define LP50XX_DEVICE_CONFIG0		0
#define   CONFIG0_CHIP_EN		BIT(6)

#define LP50XX_DEVICE_CONFIG1		0x1
#define   CONFIG1_LED_GLOBAL_OFF	BIT(0)
#define   CONFIG1_MAX_CURRENT_OPT	BIT(1)
#define   CONFIG1_PWM_DITHERING_EN	BIT(2)
#define   CONFIG1_AUTO_INCR_EN		BIT(3)
#define   CONFIG1_POWER_SAVE_EN		BIT(4)
#define   CONFIG1_LOG_SCALE_EN		BIT(5)

#define LP50XX_LED_CONFIG0		0x2
#define   CONFIG0_LED0_BANK_EN		BIT(0)
#define   CONFIG0_LED1_BANK_EN		BIT(1)
#define   CONFIG0_LED2_BANK_EN		BIT(2)
#define   CONFIG0_LED3_BANK_EN		BIT(3)

#define LP50XX_BANK_BRIGHTNESS		0x3
#define LP50XX_BANK_A_COLOR		0x4
#define LP50XX_BANK_B_COLOR		0x5
#define LP50XX_BANK_C_COLOR		0x6

#define LP50XX_LED_BRIGHTNESS_BASE	0x7
#define LP50XX_OUT_COLOR_BASE		0xB
#define LP50XX_RESET		        0x17

/* Expose channels starting from the bank registers. */
#define LP50XX_CHANNEL_BASE		LP50XX_BANK_BRIGHTNESS

//#define LED_TEST 1

struct lp50xx_config {
	struct i2c_dt_spec bus;
	struct gpio_dt_spec power_gpio;
	struct gpio_dt_spec en_gpio;
	uint8_t num_leds;
	bool log_scale_en;
	bool max_curr_opt;
	const struct led_info *leds_info;
};

struct lp50xx_data {
	uint8_t *chan_buf;
};

static int en_switch(const struct lp50xx_config *config, bool enable)
{
	return gpio_pin_set_dt(&config->en_gpio, (int)enable);
}

static const struct led_info *
lp50xx_led_to_info(const struct lp50xx_config *config, uint32_t led)
{
	int i;
    LOG_DBG("%s",__func__);
	for (i = 0; i < config->num_leds; i++) {
		if (config->leds_info[i].index == led) {
			return &config->leds_info[i];
		}
	}
	return NULL;
}

static int lp50xx_get_info(const struct device *dev, uint32_t led,
			   const struct led_info **info)
{
	const struct lp50xx_config *config = dev->config;
	const struct led_info *led_info = lp50xx_led_to_info(config, led);
    LOG_DBG("%s",__func__);
	if (!led_info) {
		return -EINVAL;
	}

	*info = led_info;

	return 0;
}

static int lp50xx_set_brightness(const struct device *dev,
				 uint32_t led, uint8_t value)
{
#ifdef LED_TEST
    return 0;
#endif
	const struct lp50xx_config *config = dev->config;
	const struct led_info *led_info = lp50xx_led_to_info(config, led);
	uint8_t buf[2];
	if (!led_info || value > 100) {
		return -EINVAL;
	}

	//buf[0] = LP50XX_LED_BRIGHTNESS_BASE + led_info->index;
    buf[0] = LP50XX_OUT_COLOR_BASE + led_info->index;
    buf[1] = (value * 0xff) / 100;
    LOG_DBG("%s:0x%02X,0x%02X",__func__, buf[0], buf[1]);
	return i2c_write_dt(&config->bus, buf, sizeof(buf));
}

#ifdef LED_TEST
static int set_brightness_test(const struct device *dev,
				 uint32_t led, uint8_t value)
{
	const struct lp50xx_config *config = dev->config;
	const struct led_info *led_info = lp50xx_led_to_info(config, led);
	uint8_t buf[2];
	if (!led_info || value > 100) {
		return -EINVAL;
	}

	//buf[0] = LP50XX_LED_BRIGHTNESS_BASE + led_info->index;
    buf[0] = LP50XX_OUT_COLOR_BASE + led_info->index;
    buf[1] = (value * 0xff) / 100;
    LOG_DBG("%s:0x%02X,0x%02X",__func__, buf[0], buf[1]);
	return i2c_write_dt(&config->bus, buf, sizeof(buf));
}
#endif

static int lp50xx_on(const struct device *dev, uint32_t led)
{
    LOG_DBG("%s",__func__);
	return lp50xx_set_brightness(dev, led, 100);
}

static int lp50xx_off(const struct device *dev, uint32_t led)
{
    LOG_DBG("%s",__func__);
	return lp50xx_set_brightness(dev, led, 0);
}

static int lp50xx_set_color(const struct device *dev, uint32_t led,
			    uint8_t num_colors, const uint8_t *color)
{
	const struct lp50xx_config *config = dev->config;
	const struct led_info *led_info = lp50xx_led_to_info(config, led);
	uint8_t buf[4];

	if (!led_info || num_colors != led_info->num_colors) {
		return -EINVAL;
	}

	buf[0] = LP50XX_OUT_COLOR_BASE + 3 * led_info->index;
	buf[1] = color[0];
	buf[2] = color[1];
	buf[3] = color[2];
    LOG_DBG("%s:0x%02X,0x%02X,0x%02X,0x%02X",__func__,
            buf[0], buf[1], buf[2], buf[3]);
	return i2c_write_dt(&config->bus, buf, sizeof(buf));
}

static int lp50xx_write_channels(const struct device *dev,
				 uint32_t start_channel,
				 uint32_t num_channels, const uint8_t *buf)
{
	const struct lp50xx_config *config = dev->config;
	struct lp50xx_data *data = dev->data;
    LOG_DBG("%s",__func__);
	if (start_channel >= LP50XX_NUM_CHANNELS ||
	    start_channel + num_channels > LP50XX_NUM_CHANNELS) {
		return -EINVAL;
	}

	/*
	 * Unfortunately this controller don't support commands split into
	 * two I2C messages.
	 */
	data->chan_buf[0] = LP50XX_CHANNEL_BASE + start_channel;
	memcpy(data->chan_buf + 1, buf, num_channels);

	return i2c_write_dt(&config->bus, data->chan_buf, num_channels + 1);
}

static int lp50xx_init(const struct device *dev)
{
	const struct lp50xx_config *config = dev->config;
	uint8_t buf[5];
	int err;
    int i;
    LOG_DBG("%s LED=%d, log_scale_en=%d, max_curr_opt=%d",
            __func__, config->num_leds, config->log_scale_en, config->max_curr_opt);
	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}
	if (gpio_pin_configure_dt(&config->power_gpio, GPIO_OUTPUT_HIGH)) {
        LOG_ERR("GPIO not ready");
		return -ENODEV;
	}
    if (gpio_pin_configure_dt(&config->en_gpio, GPIO_OUTPUT)) {
        LOG_ERR("GPIO not ready");
		return -ENODEV;
	}
    en_switch(config, true);
	if (config->num_leds > LP50XX_MAX_LEDS) {
		LOG_ERR("%s: invalid number of LEDs %d (max %d)",
			dev->name, config->num_leds, LP50XX_MAX_LEDS);
		return -EINVAL;
	}

	/*
	 * Since the status of the LP50xx controller is unknown when entering
	 * this function, and since there is no way to reset it, then the whole
	 * configuration must be applied.
	 */
	/* Reset LED driver. */
    buf[0] = LP50XX_RESET;
	buf[1] = 0;
    LOG_DBG("Reset LED driver:0x%02X,0x%02X", buf[0], buf[1]);
    for (i = 0; i < 10; i ++) {
        LOG_DBG("Try reset LED driver %d", i);
        err = i2c_write_dt(&config->bus, buf, 2);
        if (err == 0) {
            break;
        }
    }
    if (err < 0) {
        LOG_ERR("Reset LED driver error");
        return err;
    }

	/* Disable bank control for all LEDs. */
	buf[0] = LP50XX_LED_CONFIG0;
	buf[1] = 0;
	buf[2] = 0;
    LOG_DBG("Disable bank control for all LEDs:0x%02X,0x%02X,0x%02X",
                                            buf[0], buf[1], buf[2]);
	err = i2c_write_dt(&config->bus, buf, 3);
	if (err < 0) {
        LOG_ERR("Disable bank control for all LEDs error");
		return err;
	}

	/* Enable LED controller. */
	buf[0] = LP50XX_DEVICE_CONFIG0;
	buf[1] = CONFIG0_CHIP_EN;
    LOG_DBG("Enable LED controller:0x%02X,0x%02X", buf[0], buf[1]);
	err = i2c_write_dt(&config->bus, buf, 2);
	if (err < 0) {
        LOG_ERR("Enable LED controller error");
		return err;
	}

	/* Apply configuration. */
	buf[0] = LP50XX_DEVICE_CONFIG1;
	buf[1] = CONFIG1_PWM_DITHERING_EN | CONFIG1_AUTO_INCR_EN
		| CONFIG1_POWER_SAVE_EN;
	if (config->max_curr_opt) {
		buf[1] |= CONFIG1_MAX_CURRENT_OPT;
	}
	if (config->log_scale_en) {
		buf[1] |= CONFIG1_LOG_SCALE_EN;
	}
    LOG_DBG("Apply configuration:0x%02X,0x%02X", buf[0], buf[1]);
    err = i2c_write_dt(&config->bus, buf, 2);
	if (err < 0) {
        LOG_ERR("Apply configuration error");
		return err;
	}
#if 0
    /* Set LED CONFIG0. */
	buf[0] = LP50XX_LED_CONFIG0;
	buf[1] = 0x0F;
    LOG_DBG("Set LED CONFIG0:0x%02X,0x%02X", buf[0], buf[1]);
	err = i2c_write_dt(&config->bus, buf, 2);
	if (err < 0) {
        LOG_ERR("Set LED CONFIG0 error");
		return err;
	}

    /* Set BANK BRIGHTNESS. */
	buf[0] = LP50XX_BANK_BRIGHTNESS;
	buf[1] = 255;
    buf[2] = 255;
    buf[3] = 255;
    buf[4] = 255;
    LOG_DBG("Set BANK_BRIGHTNESS:0x%02X,0x%02X,0x%02X,0x%02X,0x%02X",
            buf[0], buf[1], buf[2], buf[3], buf[4]);
	err = i2c_write_dt(&config->bus, buf, 5);
	if (err < 0) {
        LOG_ERR("Set BANK_BRIGHTNESS error");
		return err;
	}
#endif
#ifdef LED_TEST
    set_brightness_test(dev, 0, 100);
    set_brightness_test(dev, 1, 100);
    set_brightness_test(dev, 3, 100);
    set_brightness_test(dev, 5, 100);
    set_brightness_test(dev, 6, 100);
    set_brightness_test(dev, 7, 100);
    set_brightness_test(dev, 8, 100);
    set_brightness_test(dev, 9, 100);
    set_brightness_test(dev, 10, 100);
    set_brightness_test(dev, 11, 100);
#endif
    LOG_DBG("%s OK",__func__);
    return err;
}

static const struct led_driver_api lp50xx_led_api = {
	.on		= lp50xx_on,
	.off		= lp50xx_off,
	.get_info	= lp50xx_get_info,
	.set_brightness	= lp50xx_set_brightness,
	.set_color	= lp50xx_set_color,
	.write_channels	= lp50xx_write_channels,
};

#define COLOR_MAPPING(led_node_id)				\
const uint8_t color_mapping_##led_node_id[] =			\
		DT_PROP(led_node_id, color_mapping);

#define LED_INFO(led_node_id)					\
{								\
	.label		= DT_PROP(led_node_id, label),		\
	.index		= DT_PROP(led_node_id, index),		\
	.num_colors	=					\
		DT_PROP_LEN(led_node_id, color_mapping),	\
	.color_mapping	= color_mapping_##led_node_id,		\
},

#define LP50xx_DEVICE(id)					\
								\
DT_INST_FOREACH_CHILD(id, COLOR_MAPPING)			\
								\
const struct led_info lp50xx_leds_##id[] = {			\
	DT_INST_FOREACH_CHILD(id, LED_INFO)			\
};								\
								\
static uint8_t lp50xx_chan_buf_##id[LP50XX_NUM_CHANNELS + 1];	\
								\
static const struct lp50xx_config lp50xx_config_##id = {	\
	.bus		= I2C_DT_SPEC_INST_GET(id),		\
	.power_gpio    = GPIO_DT_SPEC_INST_GET(id, power_gpios),	\
    .en_gpio    = GPIO_DT_SPEC_INST_GET(id, en_gpios),	\
	.num_leds	= ARRAY_SIZE(lp50xx_leds_##id),		\
	.max_curr_opt	= DT_INST_PROP(id, max_curr_opt),	\
	.log_scale_en	= DT_INST_PROP(id, log_scale_en),	\
	.leds_info	= lp50xx_leds_##id,			\
};								\
								\
static struct lp50xx_data lp50xx_data_##id = {			\
	.chan_buf	= lp50xx_chan_buf_##id,			\
};								\
								\
DEVICE_DT_INST_DEFINE(id,					\
		    &lp50xx_init,				\
		    NULL,					\
		    &lp50xx_data_##id,				\
		    &lp50xx_config_##id,			\
		    POST_KERNEL, CONFIG_LED_INIT_PRIORITY,	\
		    &lp50xx_led_api);

DT_INST_FOREACH_STATUS_OKAY(LP50xx_DEVICE)
