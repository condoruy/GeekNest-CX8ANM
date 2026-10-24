# Funciones comunes de los scripts CX8ANM para Mac
cd "$(dirname "$0")"
PUERTO="/dev/cu.usbserial-1410"
if [ ! -e "$PUERTO" ]; then
  OTRO=$(ls /dev/cu.* 2>/dev/null | grep -i -E "usbserial|wchusb|slab|usbmodem" | head -1)
  if [ -n "$OTRO" ]; then PUERTO="$OTRO"; else
    echo "No encuentro la radio. Conectala con un cable de DATOS y encendela."; read -p "Enter para salir"; exit 1; fi
fi
echo "Puerto de la radio: $PUERTO"
if ! python3 -m esptool version >/dev/null 2>&1; then
  echo "Instalando esptool..."
  python3 -m pip install --user --no-deps "esptool==4.8.1" pyserial intelhex >/dev/null 2>&1 || \
  python3 -m pip install --user --break-system-packages --no-deps "esptool==4.8.1" pyserial intelhex
fi

# Prueba primero rapido (460800) y si falla repite a 115200.
esp() { python3 -m esptool --chip esp32 --port "$PUERTO" --baud 460800 "$@" || { echo; echo "Reintentando mas lento..."; sleep 2; python3 -m esptool --chip esp32 --port "$PUERTO" --baud 115200 "$@"; }; }
echo
echo "  IMPORTANTE (placa V1.0.12):"
echo "  1. Encende la radio (llave de encendido en ON)."
echo "  2. Mantene APRETADA la rueda (encoder) y NO la sueltes hasta que termine."
echo "     Consejo: sujetala con una gomita o un broche."
read -p "  Cuando estes apretando la rueda, presiona Enter para empezar... "
