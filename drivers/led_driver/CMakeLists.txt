#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory_ifdef(CONFIG_LP5009 lp50xx)
add_subdirectory_ifdef(CONFIG_LP5012 lp50xx)
add_subdirectory_ifdef(CONFIG_MATRIX_SCAN_LED led_matrix_scan)