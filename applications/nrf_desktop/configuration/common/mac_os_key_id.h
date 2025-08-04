/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _MAC_OS_KEYS_ID_H_
#define _MAC_OS_KEYS_ID_H_

#include <zephyr/sys/util.h>
#include <caf/key_id.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _MAC_OS_POS (_COL_POS + _COL_SIZE)
#define _MAC_OS_BIT BIT(_MAC_OS_POS + 2)

#define MAC_OS_KEY_ID(_col, _row)	(KEY_ID(_col, _row) | _MAC_OS_BIT)
#define IS_MAC_OS_KEY(_keyid)	((_MAC_OS_BIT & _keyid) != 0)

#ifdef __cplusplus
}
#endif

#endif /* _MAC_OS_KEYS_ID_H_ */
