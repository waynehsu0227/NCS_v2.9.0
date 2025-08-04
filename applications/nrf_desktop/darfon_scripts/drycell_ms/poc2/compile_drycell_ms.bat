@echo off
title Batch Script Menu
:menu
cls
echo Select a script to execute:
echo 1. Run drycell_poc2_l15.bat
echo 2. Run drycell_poc2_release_l15.bat
echo 3. Run drycell_ble_poc2_release_l15.bat
echo 4. Run drycell_multilink_poc2_release_l15.bat
echo 5. Exit
choice /c 12345 /n /m "Choose an option (1-5): "

if errorlevel 5 goto exit
if errorlevel 4 call make_cy25ms_drycell_multilink_poc2_release_l15.bat
if errorlevel 3 call make_cy25ms_drycell_ble_poc2_release_l15.bat
if errorlevel 2 call make_cy25ms_drycell_poc2_release_l15.bat
if errorlevel 1 call make_cy25ms_drycell_poc2_l15.bat


:exit
echo Goodbye!
