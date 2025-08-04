/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _CHARGER_H_
#define _CHARGER_H_

#if CONFIG_BQ2561X

#include <charger/bq2561x.h>
#define CHARGER_COMPATIBLE ti_bq2561x

#elif CONFIG_BQ24295

#include <charger/bq24295.h>
#define CHARGER_COMPATIBLE ti_bq24295

#elif CONFIG_BT2306

#include <charger/bt2306.h>
#define CHARGER_COMPATIBLE m3tek_bt2306

#else
#error "Charger not supported"
#endif

#endif /* _CHARGER_H_ */
