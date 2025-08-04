@REM @REM This script is used to build the origin application and program it to the board. The signed image is generated and copied to the delta_dfu/binaries/signed_images folder for delta image source generation


SET NAME=cy25kb_drycell_coex_poc2_delta_dfu_release
SET BUILD_FOLDER=cy25kb_delta_only
SET OVERLAY=cy25kb_drycell_poc2
SET BOARD=nrf54l15dk/nrf54l15/cpuapp

@REM @REM Build the application
west build -p -b %BOARD% -d build_%BUILD_FOLDER% -- -DCONF_FILE=prj_%NAME%.conf -DDTC_OVERLAY_FILE=%OVERLAY%.overlay -DFILE_SUFFIX=%NAME% -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"2.0.0+0\"
@REM @REM Backup original hex file
copy build_%BUILD_FOLDER%\merged.hex delta_100.hex
@REM Program the application to the board
west flash -d build_%BUILD_FOLDER%


:: Check if the %ZEPHYR% environment variable is defined

if not defined ZEPHYR_BASE (
    echo %ZEPHYR_BASE% is not set, setting it to a relative path...
    set "ZEPHYR_BASE_REL=%CD%\..\..\..\zephyr"
)
:: Use for to get the absolute path
for %%I in ("%ZEPHYR_BASE_REL%") do (
    set "ZEPHYR_BASE=%%~fI"
)

echo "%ZEPHYR_BASE%" current value: %ZEPHYR_BASE%

@REM Check if signed_images directory exists, if not create it
setlocal

:: Define the directory want to create
set "dir=%ZEPHYR_BASE%\..\delta_dfu\binaries\signed_images"

:: Check if the directory exists
if not exist "%dir%" (
    :: Create the directory and its parent directories
    mkdir "%dir%"
)
endlocal
@REM Generate signed binary image from hex for later delta patch generation
@REM arm-none-eabi-objcopy --input-target=ihex --output-target=binary --gap-fill=0xff build_%BUILD_FOLDER%\nrf_desktop\zephyr\zephyr.signed.hex %ZEPHYR_BASE%\..\delta_dfu\binaries\signed_images\source_100.bin
arm-zephyr-eabi-objcopy --input-target=ihex --output-target=binary --gap-fill=0xff build_%BUILD_FOLDER%\nrf_desktop\zephyr\zephyr.signed.hex %ZEPHYR_BASE%\..\delta_dfu\binaries\signed_images\source_100.bin
