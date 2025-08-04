#
# Copyright (c) 2019 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#


add_subdirectory_ifdef(CONFIG_BQ2561X bq2561x)
add_subdirectory_ifdef(CONFIG_BQ24295 bq24295)
add_subdirectory_ifdef(CONFIG_BT2306 bt2306)
