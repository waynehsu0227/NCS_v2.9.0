@echo off
title Batch Script Menu
:menu
cls
echo Select a script to execute:
echo 1. Run coex_poc3_delta_dfu_l10.bat
echo 2. Run coex_poc3_delta_dfu_release_l10.bat
echo 3. Run multilink_poc3_delta_l10.bat
echo 4. Run multilink_poc3_delta_release_l10.bat
echo 5. Exit
choice /c 12345 /n /m "Choose an option (1-5): "

if errorlevel 5 goto exit
if errorlevel 4 call make_cy25kb_drycell_multilink_poc3_delta_release_l10.bat
if errorlevel 3 call make_cy25kb_drycell_multilink_poc3_delta_l10.bat
if errorlevel 2 call make_cy25kb_drycell_coex_poc3_delta_dfu_release_l10.bat
if errorlevel 1 call make_cy25kb_drycell_coex_poc3_delta_dfu_l10.bat


:exit
echo Goodbye!
