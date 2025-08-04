BOARD=nrf54l15pdk_nrf54l15_cpuapp

#NAME=cy25kb_drycell
NAME=cy25ms_drycell

#OVERLAY=cy25kb_drycell_poc2
OVERLAY=cy25ms_drycell_poc2

#west build -p -b $BOARD
west build -p -b $BOARD -- -DCONF_FILE="prj_${NAME}.conf" -DDTC_OVERLAY_FILE="${OVERLAY}.overlay"