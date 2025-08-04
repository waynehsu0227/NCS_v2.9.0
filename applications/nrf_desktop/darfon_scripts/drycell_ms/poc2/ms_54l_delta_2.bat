@REM This script is used to build the update application and create delta patch binary. The delta patch is generated and copied to the delta_dfu/binaries/patches folder for update.

@REM Build the application
west build -p -b nrf54l15pdk_nrf54l15_cpuapp -d build_ms_poc2_delta_dfu -- -DDTC_OVERLAY_FILE="cy25ms_drycell_poc2.overlay" -DCONF_FILE=prj_cy25ms_drycell_coex_poc2_delta.conf -DCONFIG_BT_LOG_SNIFFER_INFO=y -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"2.0.0+0\"
@REM Backup original hex file
copy build_ms_poc2_delta_dfu\zephyr\merged.hex delta_200.hex
@REM Generate signed binary image from hex for later delta patch generation
C:\ncs\arm-gnu-toolchain-13.3.rel1-mingw-w64-i686-arm-none-eabi\bin\arm-none-eabi-objcopy --input-target=ihex --output-target=binary --gap-fill=0xff build_ms_poc2_delta_dfu\zephyr\app_signed.hex C:\ncs\v2.6.0\delta_dfu\binaries\signed_images\target_200.bin

@REM Check if patches directory exists, if not create it
setlocal
:: Define the directory want to create
set "dir=C:\ncs\v2.6.0\delta_dfu\binaries\patches"
:: Check if the directory exists
if not exist "%dir%" (
    :: Create the directory and its parent directories
    mkdir "%dir%"
)
endlocal
@REM Generate delta patch binary with patch_with_arg.py script
C:\Users\test\AppData\Local\Microsoft\WindowsApps\python.exe C:\ncs\v2.6.0\delta_dfu\scripts\patch_with_arg.py -d build_ms_poc2_delta_dfu


::C:\ncs\v2.6.0\nrf\scripts\hid_configurator>C:\Users\test\AppData\Local\Microsoft\WindowsApps\python.exe configurator_cli.py nrf54l15pdk dfu C:\ncs\v2.6.0\delta_dfu\binaries\patches\dfu_application.zip
