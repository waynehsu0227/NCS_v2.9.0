#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Script is located at: $SCRIPT_DIR"


 
NAME=cy25kb_drycell_coex_poc3_delta_dfu
BUILD_FOLDER=cy25kb_drycell_coex_poc3_delta_dfu_l10
OVERLAY=cy25kb_drycell_poc3_pwm_nrf54l10

BOARD=nrf54l15dk/nrf54l10/cpuapp
west build -p -b ${BOARD} -d build_${BUILD_FOLDER} -- -DCONF_FILE=prj_${NAME}.conf -DDTC_OVERLAY_FILE=${OVERLAY}.overlay -DFILE_SUFFIX=${NAME} -DNCS_TOOLCHAIN_VERSION=NONE -DCONFIG_DEBUG_OPTIMIZATIONS=y -DCONFIG_DEBUG_THREAD_INFO=y
cp build_${BUILD_FOLDER}/merged.hex hex/full
cp build_${BUILD_FOLDER}/nrf_desktop/zephyr/zephyr.signed.hex hex

