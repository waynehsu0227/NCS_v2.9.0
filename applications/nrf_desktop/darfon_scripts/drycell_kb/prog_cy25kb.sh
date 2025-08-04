#NAME=cy25kb_drycell_ble
#NAME=cy25kb_drycell_coex
#NAME=cy25kb_drycell_coex_release
#NAME=cy25kb_drycell_ble_poc2
#NAME=cy25kb_drycell_coex_poc2
NAME=cy25kb_drycell_coex_poc3
#NAME=cy25kb_drycell_coex_poc3_release
#NAME=cy25kb_drycell_coex_poc2_delta
#NAME=cy25kb_drycell_coex_poc2_release
#NAME=cy25kb_drycell_multilink
#NAME=cy25kb_drycell_multilink_poc2_release

#NAME=cy25kb_rechargeable_ble
#NAME=cy25kb_rechargeable_coex
#NAME=cy25kb_rechargeable_ble_poc2
#NAME=cy25kb_rechargeable_coex_poc2

startTime=`date +%Y%m%d-%H:%M:%S`
startTime_s=`date +%s`

west flash -d build_$NAME
#west flash -d build_$NAME --recover


#cp build_$NAME/zephyr/merged.hex delta_dfu/
##cp -f build_$NAME/zephyr/app_signed.hex delta_dfu/app_signed1.hex

endTime=`date +%Y%m%d-%H:%M:%S`
endTime_s=`date +%s`
sumTime=$[ $endTime_s - $startTime_s ]
echo ""
echo ""
echo "Total:$sumTime seconds"