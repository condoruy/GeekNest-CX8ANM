@echo off
rem Funciones comunes del kit CX8ANM para Windows: Python, esptool y puerto de la radio.
rem --- Python (lanzador "py" o "python") ---
set "PY="
where py >nul 2>&1 && set "PY=py -3"
if not defined PY (python --version >nul 2>&1 && set "PY=python")
if not defined PY (
  echo.
  echo  ERROR: no encuentro Python.
  echo  Instala Python 3 desde https://www.python.org/downloads/
  echo  y en la primera pantalla marca "Add python.exe to PATH".
  exit /b 1
)
rem --- esptool (sin dependencias opcionales: evita el error de "cryptography") ---
%PY% -m esptool version >nul 2>&1
if errorlevel 1 (
  echo  Instalando esptool...
  %PY% -m pip install --user --no-deps "esptool==4.8.1" pyserial intelhex >nul 2>&1
  %PY% -m esptool version >nul 2>&1 || (
    echo  ERROR: no se pudo instalar esptool. Revisa la conexion a Internet.
    exit /b 1
  )
)
rem --- Puerto COM de la radio (CH340 / CP210x / USB Serial) ---
set "PUERTO="
for /f "usebackq delims=" %%P in (`powershell -NoProfile -Command "$p = Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '(CH340|CH910|CP210|USB-SERIAL|USB Serial|UART).*\(COM\d+\)' } | Select-Object -First 1; if ($p) { [regex]::Match($p.Name, 'COM\d+').Value }"`) do set "PUERTO=%%P"
if defined PUERTO (
  echo  Radio encontrada en %PUERTO%
) else (
  echo  No encontre la radio automaticamente.
  echo  Mira el numero en Administrador de dispositivos - Puertos ^(COM y LPT^).
  set /p PUERTO=  Escribi el puerto ^(ej: COM3^): 
)
if not defined PUERTO exit /b 1
echo.
echo  IMPORTANTE ^(placa V1.0.12^):
echo   1. Radio encendida y conectada por USB con cable de DATOS.
echo   2. Mantene APRETADA la rueda y NO la sueltes hasta que termine.
echo      Consejo: sujetala con una gomita o un broche.
echo.
pause
exit /b 0
