# This script is used to build the origin application and program it to the board. The signed image is generated and copied to the delta_dfu/binaries/signed_images folder for delta image source generation

# Build the application
west build -p -b nrf54l15pdk_nrf54l15_cpuapp -d build_54l -- -DCONF_FILE=prj_delta.conf -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"1.0.0+0\"
# Backup original hex file
echo "-----Copy merged.hex to delta_100.hex-----"
cp build_54l/zephyr/merged.hex delta_100.hex
# Program the application to the board
echo "Programmng........"
west flash --erase -d build_54l

echo "Generate signed binary image from hex"
# Generate signed binary image from hex for later delta patch generation: arm-zephyr-eabi-objcopy
~/zephyr-sdk-0.16.1/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy --input-target=ihex --output-target=binary --gap-fill=0xff build_54l/zephyr/app_signed.hex ../../../delta_dfu/binaries/signed_images/source_100.bin

echo "First image rework done........................"