@echo off
title CX8ANM - Registro de la radio
cd /d "%~dp0"
echo.
echo  === GUARDAR REGISTRO (para diagnosticar fallas) ===
echo  Aca NO hace falta apretar la rueda: la radio tiene que estar funcionando.
set "PY="
where py >nul 2>&1 && set "PY=py -3"
if not defined PY set "PY=python"
%PY% -c "import serial" >nul 2>&1 || %PY% -m pip install --user pyserial >nul 2>&1
set "PUERTO="
for /f "usebackq delims=" %%P in (`powershell -NoProfile -Command "$p = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '(CH340|CH910|CP210|USB-SERIAL|USB Serial|UART).*\(COM\d+\)' } | Select-Object -First 1; if ($p) { [regex]::Match($p.Name, 'COM\d+').Value }"`) do set "PUERTO=%%P"
if not defined PUERTO set /p PUERTO=  Escribi el puerto ^(ej: COM3^): 
echo  Registrando desde %PUERTO% en registro.txt
echo  Usa la radio hasta que falle y despues cerra esta ventana.
echo.
%PY% registro.py %PUERTO%
pause
