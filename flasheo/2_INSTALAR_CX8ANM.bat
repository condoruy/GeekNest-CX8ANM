@echo off
title CX8ANM - Instalar firmware
cd /d "%~dp0"
set "FIRMWARE=CX8ANM-GeekNestV5A-completo.bin"
echo.
echo  === PASO 2: INSTALAR FIRMWARE CX8ANM ===
if exist respaldo_geeknest_original.bin goto :grabar
echo  ATENCION: no hay respaldo en esta carpeta.
echo  Si ya lo hiciste, copia respaldo_geeknest_original.bin aca.
set /p SEGUIR=  Escribi SI para instalar igual: 
if /i not "%SEGUIR%"=="SI" goto :fin
:grabar
call _comun.bat || goto :fin
echo  Grabando %FIRMWARE% ^(1 a 2 minutos^)...
%PY% -m esptool --chip esp32 --port %PUERTO% --baud 460800 write_flash -z 0x0 %FIRMWARE%
if errorlevel 1 (
  echo  Reintentando mas lento...
  %PY% -m esptool --chip esp32 --port %PUERTO% --baud 115200 write_flash -z 0x0 %FIRMWARE%
)
if errorlevel 1 (
  echo.
  echo  ERROR: copia el texto de arriba y mandalo.
  goto :fin
)
echo.
echo  LISTO. Solta la rueda, desconecta la radio, apagala y volve a encenderla.
echo  Tiene que aparecer el logo CX8ANM con "fw 1.3.0".
:fin
echo.
pause
