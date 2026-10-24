#!/bin/bash
source "$(dirname "$0")/_comun.sh"
echo; echo "=== RESPALDO DEL FIRMWARE ORIGINAL (unos 2 minutos) ==="
if [ -f respaldo_geeknest_original.bin ]; then echo "Ya existe respaldo_geeknest_original.bin, no lo piso."; read -p "Enter para salir"; exit 0; fi
if esp read_flash 0 0x400000 respaldo_geeknest_original.bin; then
  echo; echo "LISTO. Guarda respaldo_geeknest_original.bin en un pendrive o en Drive."
else echo; echo "ERROR: copia el texto de arriba y mandalo."; fi
read -p "Enter para cerrar"
