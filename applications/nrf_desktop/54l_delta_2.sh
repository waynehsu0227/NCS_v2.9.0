# This script is used to build the update application and create delta patch binary. The delta patch is generated and copied to the delta_dfu/binaries/patches folder for update.

#Build the application
west build -p -b nrf54l15pdk_nrf54l15_cpuapp -d build_54l -- -DCONF_FILE=prj_delta_gazell_ble.conf -DCONFIG_BT_LOG_SNIFFER_INFO=y -DCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"2.0.0+0\"
# Backup original hex file
echo "-----Copy merged.hex to delta_200.hex-----"
cp build_54l/zephyr/merged.hex delta_200.hex

# Generate signed binary image from hex for later delta patch generation
echo "Generate signed binary image from hex"
~/zephyr-sdk-0.16.1/arm-zephyr-eabi/bin/arm-zephyr-eabi-objcopy --input-target=ihex --output-target=binary --gap-fill=0xff build_54l/zephyr/app_signed.hex ../../../delta_dfu/binaries/signed_images/target_200.bin

echo "Second image rework done........................"
# Generate delta patch binary with patch_with_arg.py script
python3  ../../../delta_dfu/scripts/patch_with_arg.py -d build_54l

cp ../../../delta_dfu/binaries/patches/dfu_application.zip delta_dfu_application.zip
