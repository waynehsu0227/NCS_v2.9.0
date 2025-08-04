/*
 * Copyright (c) 2020 Seagate Technology LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT matrix_scan_leds

/**
 * @file
 * @brief lLED_MATRIX_SCAN LED controller
 */

#include <zephyr/drivers/led.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/kernel.h>

#include "led_matrix_scan.h"

#define LOG_LEVEL CONFIG_LED_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(matrix_scan_leds);

//#define TEST_LED

static void reset_gpios(const struct gpio_dt_spec *gpios, uint8_t pin_count)
{
    int err;

    for (int i = 0; i < pin_count; i ++) {
        err = gpio_pin_set_dt(&gpios[i], false);
        if (err) {
            LOG_ERR("clear gpios %d err:%d", gpios[i].pin, err);
        } 
    }
}

static void set_gpios(const struct gpio_dt_spec *gpios, uint8_t pin_count)
{
    int err;
    for (int i = 0; i < pin_count; i ++) {
        err = gpio_pin_set_dt(gpios, true);
        if (err) {
            LOG_ERR("set gpios%d err:%d", gpios[i].pin, err);
        }
    }
}

static void update_led_rows(const struct led_matrix_scan_config *config,
                            struct led_matrix_scan_data *data)
{
    int err;
    uint16_t index = data->col_scan_index;
    uint16_t max_row = config->num_rows;
    uint16_t *led_status = data->led_status;
    const struct gpio_dt_spec *row_gpios = config->row_gpios;
    for (int i = 0; i < max_row; i ++) {
        err = gpio_pin_set_dt(&row_gpios[i], led_status[max_row * index + i]);
        if (err) {
            LOG_ERR("update led row index%d err:%d", i, err);
        } 
    }
}

static void led_scaning_procsee(const struct device *dev)
{
    const struct led_matrix_scan_config *config = dev->config;
    struct led_matrix_scan_data *data = dev->data;
    uint16_t led_scaning = 0;
    uint16_t led_array_size = config->num_cols * config->num_rows;
    for (int i = 0; i < led_array_size; i ++) {
        led_scaning |= data->led_status[i];
    }
    if (led_scaning) {
#ifndef USE_TIMER
        if (!k_work_delayable_busy_get(&data->scan_work)) {
            set_gpios(&config->en_gpio, 1);
            k_work_reschedule(&data->scan_work,  K_MSEC(0));
        }
#else
        if (!k_timer_remaining_get(&data->scan_timer)) {
            set_gpios(&config->en_gpio, 1);
            k_timer_start(&data->scan_timer, K_NO_WAIT, K_MSEC(config->scan_interval));
        }
#endif
    } else {
#ifndef USE_TIMER
        if (k_work_delayable_busy_get(&data->scan_work)) {
            struct k_work_sync sync;
            k_work_cancel_delayable_sync(&data->scan_work, &sync);
            reset_gpios(config->row_gpios, config->num_rows);
            reset_gpios(config->col_gpios, config->num_cols);
            reset_gpios(&config->en_gpio, 1);
        }
#else
        if (k_timer_remaining_get(&data->scan_timer)) {
            k_timer_stop(&data->scan_timer);
            reset_gpios(config->row_gpios, config->num_rows);
            reset_gpios(config->col_gpios, config->num_cols);
            reset_gpios(&config->en_gpio, 1);
        }
#endif
    }
}
#ifndef USE_TIMER
static void led_matrix_scan_work_fn(struct k_work *w)
{
    struct led_matrix_scan_data *data = CONTAINER_OF(w, struct led_matrix_scan_data, scan_work);
	const struct led_matrix_scan_config *config = data->dev->config;
    
    reset_gpios(&config->col_gpios[data->col_scan_index], 1);
    reset_gpios(config->row_gpios, config->num_rows);
    if (++data->col_scan_index >= config->num_cols)
        data->col_scan_index = 0;
    k_busy_wait(350);
    set_gpios(&config->col_gpios[data->col_scan_index], 1);
    update_led_rows(config, data);

    k_work_reschedule(&data->scan_work, K_MSEC(config->scan_interval));
}
#else
static void led_matrix_scan_handler(struct k_timer *timer_id)
{
    struct led_matrix_scan_data *data = CONTAINER_OF(timer_id, struct led_matrix_scan_data, scan_timer);
	const struct led_matrix_scan_config *config = data->dev->config;
    
    reset_gpios(&config->col_gpios[data->col_scan_index], 1);
    reset_gpios(config->row_gpios, config->num_rows);
    if (++data->col_scan_index >= config->num_cols)
        data->col_scan_index = 0;
    k_busy_wait(350);
    //k_usleep(350);
    set_gpios(&config->col_gpios[data->col_scan_index], 1);
    update_led_rows(config, data);
}
#endif
static int led_matrix_scan_set_brightness(const struct device *dev,
				 uint32_t led, uint8_t value)
{
#ifndef TEST_LED
    LOG_DBG("%s led:%d value:%d", __func__, led, value);
    struct led_matrix_scan_data *data = dev->data;
    data->led_status[led] = value;
#endif
    led_scaning_procsee(dev);
    
    return 0;
}

static int led_matrix_scan_on(const struct device *dev, uint32_t led)
{
    LOG_DBG("%s", __func__);
	return led_matrix_scan_set_brightness(dev, led, 100);
}

static int led_matrix_scan_off(const struct device *dev, uint32_t led)
{
    LOG_DBG("%s", __func__);
	return led_matrix_scan_set_brightness(dev, led, 0);
}

static int led_matrix_scan_set_color(const struct device *dev, uint32_t led,
			    uint8_t num_colors, const uint8_t *color)
{
    LOG_DBG("%s", __func__);
/*    
	const struct led_matrix_scan_config *config = dev->config;
	const struct led_info *led_info = led_matrix_scan_led_to_info(config, led);
	uint8_t buf[4];

	if (!led_info || num_colors != led_info->num_colors) {
		return -EINVAL;
	}

	buf[0] = LP503X_OUT_COLOR_BASE + 3 * led_info->index;
	buf[1] = color[0];
	buf[2] = color[1];
	buf[3] = color[2];

	return i2c_write_dt(&config->bus, buf, sizeof(buf));
*/
    return 0;
}

static int gpio_pin_init(const struct gpio_dt_spec *gpio_pin) {

    if (device_is_ready(gpio_pin->port)) {
        int err = gpio_pin_configure_dt(gpio_pin, GPIO_OUTPUT_INACTIVE);
        if (err) {
            LOG_ERR("Cannot configure GPIO (err %d)", err);
        }
        return err;
    } else {
        LOG_ERR("%s: GPIO not ready", gpio_pin->port->name);
        return -ENODEV;
    }
    LOG_DBG("pin %d ok", gpio_pin->pin);
    return 0;
}

static int led_matrix_scan_init(const struct device *dev)
{
	int err = 0;
    struct led_matrix_scan_data *data = dev->data;
    const struct led_matrix_scan_config *config = dev->config;
    data->dev = dev;

    err |= gpio_pin_init(&config->en_gpio);

   	for (size_t i = 0; (i < config->num_cols) && !err; i++) {
        err |= gpio_pin_init(&config->col_gpios[i]);
	}

    for (size_t i = 0; (i < config->num_rows) && !err; i++) {
        err |= gpio_pin_init(&config->row_gpios[i]);
	}
#ifndef USE_TIMER
    k_work_init_delayable(&data->scan_work, led_matrix_scan_work_fn);
#else
    k_timer_init(&data->scan_timer, led_matrix_scan_handler, NULL);
#endif
    LOG_DBG("%s ok", __func__);
#ifdef TEST_LED
    for (size_t i = 0; (i < (config->num_cols * config->num_rows)); i ++) {
        data->led_status[i] = 0xff;
    }
#endif
    return err;
}

static const struct led_driver_api led_matrix_scan_led_api = {
	.on		= led_matrix_scan_on,
	.off		= led_matrix_scan_off,
	.set_brightness	= led_matrix_scan_set_brightness,
	.set_color	= led_matrix_scan_set_color,
};

#define LED_MATRIX_SCAN_DEVICE(id)					            \
								                                \
static struct led_matrix_scan_data data_##id;				    \
								                                \
static const struct gpio_dt_spec col_gpio_##id[] = {            \
	DT_FOREACH_PROP_ELEM_SEP(DT_NODELABEL(led_matrix_scan),     \
				 col_gpios, GPIO_DT_SPEC_GET_BY_IDX, (,))       \
};                                                              \
                                                                \
static const struct gpio_dt_spec row_gpio_##id[] = {            \
	DT_FOREACH_PROP_ELEM_SEP(DT_NODELABEL(led_matrix_scan),     \
				 row_gpios, GPIO_DT_SPEC_GET_BY_IDX, (,))       \
};                                                              \
                                                                \
static uint16_t led_status_##id[ARRAY_SIZE(col_gpio_##id) *     \
                                ARRAY_SIZE(row_gpio_##id)];	    \
                                                                \
static const struct led_matrix_scan_config config_##id = {	    \
    .en_gpio = GPIO_DT_SPEC_INST_GET(id, en_gpios),             \
    .col_gpios = col_gpio_##id,                                 \
    .row_gpios = row_gpio_##id,                                 \
    .num_cols = ARRAY_SIZE(col_gpio_##id),                      \
    .num_rows = ARRAY_SIZE(row_gpio_##id),                      \
    .scan_interval	= DT_INST_PROP(id, scan_interval),	        \
};								                                \
                                                                \
static struct led_matrix_scan_data data_##id = {			    \
	.led_status	= led_status_##id,			                    \
};								                                \
                                \
    DEVICE_DT_INST_DEFINE(id,					\
		    &led_matrix_scan_init,				\
		    NULL,					\
		    &data_##id,                   \
		    &config_##id,			\
		    POST_KERNEL, CONFIG_LED_INIT_PRIORITY,	\
		    &led_matrix_scan_led_api);

DT_INST_FOREACH_STATUS_OKAY(LED_MATRIX_SCAN_DEVICE)