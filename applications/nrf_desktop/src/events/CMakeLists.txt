#
# Copyright (c) 2018 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE
	       ${CMAKE_CURRENT_SOURCE_DIR}/battery_event.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/ble_event.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/hid_event.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/motion_event.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/passkey_event.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/selector_event.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/wheel_event.c
	       ${CMAKE_CURRENT_SOURCE_DIR}/tilt_event.c
)

target_sources_ifdef(CONFIG_DESKTOP_BLE_DONGLE_PEER_ID_INFO app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ble_dongle_peer_event.c)

target_sources_ifdef(CONFIG_DESKTOP_CONFIG_CHANNEL_ENABLE app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/config_event.c)

target_sources_ifdef(CONFIG_DESKTOP_CPU_MEAS_ENABLE app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpu_load_event.c)

target_sources_ifdef(CONFIG_DESKTOP_USB_ENABLE app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/usb_event.c)

target_sources_ifdef(CONFIG_DESKTOP_HIDS_ENABLE app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/hids_event.c)

target_sources_ifdef(CONFIG_MULTILINK_ENABLE app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/test_mode_event.c)

target_sources_ifdef(CONFIG_MULTI_LINK_PEER_EVENTS app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/multi_link_peer_event.c)

target_sources_ifdef(CONFIG_MOTION_SENSOR_CONFIG_EVENTS app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/motion_sensor_config_event.c)

target_sources_ifdef(CONFIG_DPM_ENABLE app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/dpm_event.c)

target_sources_ifdef(CONFIG_DESKTOP_MAC_OS_KEYS_ENABLE app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mac_os_key_event.c)

target_sources_ifdef(CONFIG_MULTILINK_ENABLE app
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/battery_read_request_event.c)			
 
 
 