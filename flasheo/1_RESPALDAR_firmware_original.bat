@echo off
chcp 65001 >nul
title CX8ANM - Respaldo del firmware original GeekNest V5A
echo.
echo  === PASO 1: RESPALDO DEL FIRMWARE ORIGINAL ===
echo  Conecta la radio por USB (cable de DATOS) y encendela.
echo.
call :esptool_check || goto :fin
set /p PUERTO=Escribi el puerto COM de la radio (ej: COM3): 
if "%PUERTO%"=="" goto :fin
echo.
echo  Leyendo los 4 MB de memoria (tarda unos 6 minutos)...
python -m esptool --chip esp32 --port %PUERTO% --baud 115200 read_flash 0 0x400000 respaldo_geeknest_original.bin
if errorlevel 1 (
  echo.
  echo  ERROR: no se pudo leer. Revisa el puerto, el cable y que la radio este encendida.
  goto :fin
)
echo.
echo  LISTO. Guarda "respaldo_geeknest_original.bin" en DOS lugares (pendrive, Drive).
echo  Es la unica forma de volver al firmware de fabrica.
goto :fin

:esptool_check
python -m esptool version >nul 2>&1 && exit /b 0
echo  Instalando esptool (necesita Python instalado desde python.org)...
python -m pip install esptool || (echo  ERROR: instala Python 3 desde https://www.python.org y volve a probar. & exit /b 1)
exit /b 0

:fin
echo.
pause
