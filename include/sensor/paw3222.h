/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#ifndef ZEPHYR_INCLUDE_PAW3222_H_
#define ZEPHYR_INCLUDE_PAW3222_H_

/**
 * @file paw3222.h
 *
 * @brief Header file for the paw3222 driver.
 */

#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup paw3222 PAW3222 motion sensor driver
 * @{
 * @brief PAW3222 motion sensor driver.
 */

/** @brief Sensor specific attributes of PAW3222. */
enum paw3222_attribute {
	/** Sensor CPI for both X and Y axes. */
	PAW3222_ATTR_CPI = SENSOR_ATTR_PRIV_START,

	/** Enable or disable sleep modes. */
	PAW3222_ATTR_SLEEP_ENABLE,

	/** Entering time from Run mode to Sleep1 mode [ms]. */
	PAW3222_ATTR_SLEEP1_TIMEOUT,

	/** Entering time from Run mode to Sleep2 mode [ms]. */
	PAW3222_ATTR_SLEEP2_TIMEOUT,

	/** Entering time from Run mode to Sleep3 mode [ms]. */
	PAW3222_ATTR_SLEEP3_TIMEOUT,

	/** Sampling frequency time during Sleep1 mode [ms]. */
	PAW3222_ATTR_SLEEP1_SAMPLE_TIME,

	/** Sampling frequency time during Sleep2 mode [ms]. */
	PAW3222_ATTR_SLEEP2_SAMPLE_TIME,

	/** Sampling frequency time during Sleep3 mode [ms]. */
	PAW3222_ATTR_SLEEP3_SAMPLE_TIME,
};

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_PAW3222_H_ */
