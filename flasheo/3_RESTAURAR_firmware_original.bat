@echo off
title CX8ANM - Restaurar firmware de fabrica
cd /d "%~dp0"
echo.
echo  === RESTAURAR EL FIRMWARE ORIGINAL DE FABRICA ===
if not exist respaldo_geeknest_original.bin (
  echo  No encuentro respaldo_geeknest_original.bin en esta carpeta.
  goto :fin
)
call _comun.bat || goto :fin
%PY% -m esptool --chip esp32 --port %PUERTO% --baud 460800 write_flash 0x0 respaldo_geeknest_original.bin
if errorlevel 1 %PY% -m esptool --chip esp32 --port %PUERTO% --baud 115200 write_flash 0x0 respaldo_geeknest_original.bin
:fin
echo.
pause
