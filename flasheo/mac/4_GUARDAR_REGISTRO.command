#!/bin/bash
# Muestra y guarda lo que la radio informa por USB (registro.txt) para diagnosticar fallas.
cd "$(dirname "$0")"
PUERTO="/dev/cu.usbserial-1410"
[ -e "$PUERTO" ] || PUERTO=$(ls /dev/cu.* 2>/dev/null | grep -i -E "usbserial|wchusb|slab|usbmodem" | head -1)
[ -n "$PUERTO" ] || { echo "No encuentro la radio por USB."; read -p "Enter para salir"; exit 1; }
python3 -c "import serial" 2>/dev/null || python3 -m pip install --user pyserial >/dev/null 2>&1
echo "Registrando desde $PUERTO en registro.txt"
echo "Usa la radio hasta que falle y despues cerra esta ventana (o Ctrl+C)."
echo
python3 - "$PUERTO" <<'PY'
import sys, serial, time
s = serial.Serial()
s.port = sys.argv[1]; s.baudrate = 115200; s.timeout = 0.5
s.dtr = False; s.rts = False          # no reiniciar la radio al abrir el puerto
s.open()
with open("registro.txt", "a", encoding="utf-8", errors="replace") as f:
    f.write("\n===== %s =====\n" % time.strftime("%Y-%m-%d %H:%M:%S"))
    try:
        while True:
            line = s.readline()
            if line:
                text = time.strftime("%H:%M:%S ") + line.decode("utf-8", "replace").rstrip()
                print(text); f.write(text + "\n"); f.flush()
    except KeyboardInterrupt:
        pass
PY
