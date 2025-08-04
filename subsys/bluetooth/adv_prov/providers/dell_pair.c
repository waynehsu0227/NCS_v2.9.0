/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <bluetooth/adv_prov.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(bt_le_adv_prov, LOG_LEVEL_DBG);

#define GRACE_PERIOD_S	CONFIG_BT_ADV_PROV_DELL_PAIR_COOL_DOWN_DURATION

static bool enabled = true;


void bt_le_adv_prov_dell_pair_enable(bool enable)
{
	enabled = enable;
}

static int get_data(struct bt_data *ad, const struct bt_le_adv_prov_adv_state *state,
		    struct bt_le_adv_prov_feedback *fb)
{
	static const uint8_t data[] = {
		0x1E, 0x04,         /* Dell SIG ID */
        0x38,               /* Dell Pair ID */
        0x02,               /* API Version */
        (CONFIG_DESKTOP_DEVICE_PID & 0x00ff), (CONFIG_DESKTOP_DEVICE_PID >> 8),        /* PID */	
	};

	if (!enabled) {
        LOG_INF("dell pair %s fail! cause not enable", __func__);
		return -ENOENT;
	}

	if (state->in_grace_period || (!state->pairing_mode)) {
        LOG_INF("dell pair %s fail! in_grace_period=%s pairing_mode=%s",
        __func__,
        state->in_grace_period ? "true" : "false",
        state->pairing_mode ? "true" : "false");
		return -ENOENT;
	}
    LOG_INF("dell pair %s ok!", __func__);

	ad->type = BT_DATA_MANUFACTURER_DATA;
	ad->data_len = sizeof(data);
	ad->data = data;

	fb->grace_period_s = GRACE_PERIOD_S;

	return 0;
}

BT_LE_ADV_PROV_AD_PROVIDER_REGISTER(quick_pair, get_data);
