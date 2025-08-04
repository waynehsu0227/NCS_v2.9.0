@REM This script is used to build the update application and create delta patch binary. The delta patch is generated and copied to the delta_dfu/binaries/patches folder for update.

SET NAME=cy25kb_drycell_coex_poc2_delta_dfu_release
SET BUILD_FOLDER=cy25kb_delta_only
SET OVERLAY=cy25kb_drycell_poc2
SET BOARD=nrf54l15dk/nrf54l15/cpuapp

@REM Build the application
west build -p -b %BOARD% -d build_%BUILD_FOLDER% -- -DCONF_FILE=prj_%NAME%.conf -DDTC_OVERLAY_FILE=%OVERLAY%.overlay -DFILE_SUFFIX=%NAME% -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"2.0.0+0\"
@REM Backup original hex file
copy build_%BUILD_FOLDER%\merged.hex delta_200.hex
@REM Generate signed binary image from hex for later delta patch generation
arm-zephyr-eabi-objcopy --input-target=ihex --output-target=binary --gap-fill=0xff build_%BUILD_FOLDER%\nrf_desktop\zephyr\zephyr.signed.hex %ZEPHYR_BASE%\..\delta_dfu\binaries\signed_images\target_200.bin

@REM Check if patches directory exists, if not create it
setlocal
:: Define the directory want to create
set "dir=%ZEPHYR_BASE%\..\delta_dfu\binaries\patches"
:: Check if the directory exists
if not exist "%dir%" (
    :: Create the directory and its parent directories
    mkdir "%dir%"
)
endlocal
@REM Generate delta patch binary with patch_with_arg.py script
python  %ZEPHYR_BASE%\..\delta_dfu\scripts\patch_with_arg.py -d build_%BUILD_FOLDER%
