@echo off
chcp 65001 >nul
title CX8ANM - Restaurar firmware original
echo.
echo  === RESTAURAR EL FIRMWARE ORIGINAL DE FABRICA ===
if not exist respaldo_geeknest_original.bin (
  echo  No encuentro respaldo_geeknest_original.bin en esta carpeta.
  goto :fin
)
python -m esptool version >nul 2>&1 || python -m pip install esptool
set /p PUERTO=Escribi el puerto COM de la radio (ej: COM3): 
if "%PUERTO%"=="" goto :fin
python -m esptool --chip esp32 --port %PUERTO% --baud 115200 write_flash 0x0 respaldo_geeknest_original.bin
:fin
echo.
pause
