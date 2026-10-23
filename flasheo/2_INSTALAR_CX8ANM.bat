@echo off
chcp 65001 >nul
title CX8ANM - Instalar firmware en GeekNest V5A
set FIRMWARE=CX8ANM-GeekNestV5A-v1.0.0-completo.bin
echo.
echo  === PASO 2: INSTALAR FIRMWARE CX8ANM ===
if exist respaldo_geeknest_original.bin goto :grabar
echo  ATENCION: no encuentro respaldo_geeknest_original.bin en esta carpeta.
echo  Primero ejecuta 1_RESPALDAR_firmware_original.bat
set /p SEGUIR=Escribi SI para instalar igual sin respaldo: 
if /i not "%SEGUIR%"=="SI" goto :fin
:grabar
python -m esptool version >nul 2>&1 || python -m pip install esptool
set /p PUERTO=Escribi el puerto COM de la radio (ej: COM3): 
if "%PUERTO%"=="" goto :fin
echo.
echo  Grabando %FIRMWARE% (tarda unos 3 minutos)...
python -m esptool --chip esp32 --port %PUERTO% --baud 115200 write_flash -z 0x0 %FIRMWARE%
if errorlevel 1 (
  echo.
  echo  ERROR al grabar. Revisa el puerto y el cable y volve a intentar.
  goto :fin
)
echo.
echo  LISTO. Desconecta y volve a encender la radio. Debe aparecer el logo CX8ANM.
:fin
echo.
pause
