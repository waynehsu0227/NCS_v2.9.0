/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _COLLABS_ID_H_
#define _COLLABS_KEY_ID_H_

#include <zephyr/sys/util.h>
#include <caf/key_id.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _COLLABS_POS (_COL_POS + _COL_SIZE)
#define _COLLABS_BIT BIT(_COLLABS_POS + 1)

#define COLLABS_KEY_ID(_col, _row)	(KEY_ID(_col, _row) | _COLLABS_BIT)
#define IS_COLLABS_KEY(_keyid)	((_COLLABS_BIT & _keyid) != 0)

#ifdef __cplusplus
}
#endif

#endif /* _COLLABS_KEY_ID_H_ */
