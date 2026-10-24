#!/bin/bash
source "$(dirname "$0")/_comun.sh"
echo; echo "=== RESTAURAR FIRMWARE DE FABRICA ==="
[ -f respaldo_geeknest_original.bin ] || { echo "No encuentro respaldo_geeknest_original.bin en esta carpeta."; read -p "Enter"; exit 1; }
esp write_flash 0x0 respaldo_geeknest_original.bin
read -p "Enter para cerrar"
