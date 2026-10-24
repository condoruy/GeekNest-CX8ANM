#!/bin/bash
source "$(dirname "$0")/_comun.sh"
echo; echo "=== INSTALAR FIRMWARE CX8ANM (unos 1-2 minutos) ==="
if [ ! -f respaldo_geeknest_original.bin ]; then
  echo "ATENCION: no hay respaldo. Primero ejecuta 1_RESPALDAR_firmware_original.command"
  read -p "Escribi SI para instalar igual: " R; case "$R" in [sS][iIíÍ]) ;; *) exit 0;; esac
fi
if esp write_flash -z 0x0 CX8ANM-GeekNestV5A-completo.bin; then
  echo; echo "LISTO. Desconecta la radio, apagala y volve a encenderla."
else echo; echo "ERROR: copia el texto de arriba y mandalo."; fi
read -p "Enter para cerrar"
