@echo off
title CX8ANM - Respaldo del firmware original
cd /d "%~dp0"
echo.
echo  === PASO 1: RESPALDO DEL FIRMWARE ORIGINAL ===
if exist respaldo_geeknest_original.bin (
  echo  Ya existe respaldo_geeknest_original.bin en esta carpeta. No lo piso.
  goto :fin
)
call _comun.bat || goto :fin
echo  Leyendo los 4 MB de la radio ^(unos 2 minutos^)...
%PY% -m esptool --chip esp32 --port %PUERTO% --baud 460800 read_flash 0 0x400000 respaldo_geeknest_original.bin
if errorlevel 1 (
  echo  Reintentando mas lento ^(unos 6 minutos^)...
  %PY% -m esptool --chip esp32 --port %PUERTO% --baud 115200 read_flash 0 0x400000 respaldo_geeknest_original.bin
)
if errorlevel 1 (
  echo.
  echo  ERROR: copia el texto de arriba y mandalo.
  goto :fin
)
echo.
echo  LISTO. Guarda respaldo_geeknest_original.bin en un pendrive o en Drive.
echo  Es la unica forma de volver al firmware de fabrica.
:fin
echo.
pause
