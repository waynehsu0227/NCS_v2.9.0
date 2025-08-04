#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#ifndef LED_DRIVERS_LED_MATRIX_SCAN_H_
#define LED_DRIVERS_LED_MATRIX_SCAN_H_
#define USE_TIMER
struct led_matrix_scan_config {
    const struct gpio_dt_spec en_gpio;
    const struct gpio_dt_spec *col_gpios;
    const struct gpio_dt_spec *row_gpios;
    const uint16_t num_cols;
    const uint16_t num_rows;
    const uint8_t scan_interval;
};

struct led_matrix_scan_data {
    const struct device         *dev;
#ifndef USE_TIMER
    struct k_work_delayable     scan_work;
#else
    struct k_timer              scan_timer;
#endif
    uint16_t                    *led_status;
    uint16_t                    col_scan_index;
};

#endif