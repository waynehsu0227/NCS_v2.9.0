/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _HID_REPORT_DPM_H_
#define _HID_REPORT_DPM_H_

#ifdef __cplusplus
extern "C" {
#endif

#define REPORT_SIZE_DPM_INPUT                15 /* bytes */
#define REPORT_SIZE_DPM_FEATURE              15 /* bytes */
#define REPORT_MASK_DPM_INPUT		            {} /* Store the whole report */
#define DPM_INPUT_REPORT_COUNT_MAX	1

#define REPORT_MAP_DPM_INPUT(report_id)				\
    0x06, 0x00, 0xFF,       /* Usage Page (Vendor Defined 0xFF00) */\
    0x09, 0x00,             /* Usage (0x00) */\
    0xA1, 0x01,             /* Collection (Application) */\
    0x85, report_id,             /*   Report ID (17) */\
    0x09, 0x00,             /*   Usage (0x00) */\
    0x15, 0x00,             /*   Logical Minimum (0) */\
    0x26, 0xFF, 0x00,       /*   Logical Maximum (255) */\
    0x75, 0x08,             /*   Report Size (8) */\
    0x95, REPORT_SIZE_DPM_INPUT,  /*   Report Count (15) */\
    0x82, 0x02, 0x01,       /*   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Buffered Bytes) */\
    0xC0                    /* End Collection */\

#define REPORT_MAP_DPM_FEATURE(report_id)				\
    0x06, 0x01, 0xFF,       /* Usage Page (Vendor Defined 0xFF01) */\
    0x09, 0x01,             /* Usage (0x01) */\
    0xA1, 0x01,             /* Collection (Application) */\
    0x85, report_id,        /*   Report ID (18) */\
    0x95, REPORT_SIZE_DPM_FEATURE,  /*   Report Count (15) */\
    0x75, 0x08,             /*   Report Size (8) */\
    0x15, 0x00,             /*   Logical Minimum (0) */\
    0x26, 0xFF, 0x00,       /*   Logical Maximum (255) */\
    0x09, 0x20,             /*   Usage (0x20) */\
    0xB1, 0x02,             /*   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile) */\
    0xC0                    /* End Collection */\
\
/* 47 bytes */\

#ifdef __cplusplus
}
#endif

#endif /* _HID_REPORT_DPM_H_ */
