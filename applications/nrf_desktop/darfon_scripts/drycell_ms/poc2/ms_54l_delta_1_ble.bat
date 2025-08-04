echo ZEPHYR_BASE=%ZEPHYR_BASE%
@REM This script is used to build the origin application and program it to the board. The signed image is generated and copied to the delta_dfu/binaries/signed_images folder for delta image source generation

@REM Build the application
west build -p -b nrf54l15pdk_nrf54l15_cpuapp -d build_ms_poc2_delta_dfu -- -DDTC_OVERLAY_FILE="cy25ms_drycell_poc2.overlay" -DCONF_FILE=prj_cy25ms_drycell_ble_poc2_delta.conf -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"1.0.0+0\"
@REM Backup original hex file
copy build_ms_poc2_delta_dfu\zephyr\merged.hex delta_100.hex
@REM Program the application to the board
west flash --erase -d build_ms_poc2_delta_dfu

@REM Check if signed_images directory exists, if not create it
setlocal
:: Define the directory want to create
set "dir=C:\ncs\v2.6.0\delta_dfu\binaries\signed_images"

:: Check if the directory exists
if not exist "%dir%" (
    :: Create the directory and its parent directories
    mkdir "%dir%"
)
endlocal
@REM Generate signed binary image from hex for later delta patch generation
C:\ncs\arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi\bin\arm-none-eabi-objcopy --input-target=ihex --output-target=binary --gap-fill=0xff build_ms_poc2_delta_dfu\zephyr\app_signed.hex C:\ncs\v2.6.0\delta_dfu\binaries\signed_images\source_100.bin


