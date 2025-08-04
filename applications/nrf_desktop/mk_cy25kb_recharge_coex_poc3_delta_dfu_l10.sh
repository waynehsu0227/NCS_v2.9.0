#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Script is located at: $SCRIPT_DIR"

export ZEPHYR_GENERATOR=Ninja
export ZEPHYR_CCACHE_ENABLED=1
export MAKEFLAGS="-j$(nproc)"
 
NAME=cy25kb_recharge_coex_poc3_delta_dfu
BUILD_FOLDER=cy25kb_recharge_coex_delta_dfu_l10
OVERLAY=cy25kb_recharge_poc3_pwm_nrf54l10

# 記錄開始時間
start_time=$(date +%s%3N)

BOARD=nrf54l15dk/nrf54l10/cpuapp
west build -p auto -b ${BOARD} -d build_${BUILD_FOLDER} -- -DCONF_FILE=prj_${NAME}.conf -DDTC_OVERLAY_FILE=${OVERLAY}.overlay -DFILE_SUFFIX=${NAME} -DNCS_TOOLCHAIN_VERSION=NONE -DCONFIG_DEBUG_OPTIMIZATIONS=y -DCONFIG_DEBUG_THREAD_INFO=y
#cp build_${BUILD_FOLDER}/merged.hex hex/full
#cp build_${BUILD_FOLDER}/nrf_desktop/zephyr/zephyr.signed.hex hex

# 複製檔案
mkdir -p hex/full
cp build_$BUILD_FOLDER/merged.hex hex/full/
cp build_$BUILD_FOLDER/nrf_desktop/zephyr/zephyr.signed.hex hex/

# 記錄結束時間
end_time=$(date +%s%3N)

# 計算時間差
elapsed=$((end_time - start_time))
hours=$((elapsed / 3600000))
mins=$(( (elapsed % 3600000) / 60000 ))
secs=$(( (elapsed % 60000) / 1000 ))
msecs=$((elapsed % 1000))
printf "Compilation & CMake Time : %02d:%02d:%02d.%03d\n" $hours $mins $secs $msecs