#!/bin/bash
# Compila con arduino-cli y el core ESP32 2.0.17 (espressif:esp32).
# Requisitos: arduino-cli, core "esp32:esp32@2.0.17" (o instalado a mano como espressif:esp32).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
FQBN="${FQBN:-esp32:esp32:esp32:PartitionScheme=min_spiffs,PSRAM=disabled,CPUFreq=240,FlashMode=dio,FlashFreq=40}"
FLAGS="-I$HERE/GeekNestCX8ANM/src -DBOARD_V5A -DCORE_DEBUG_LEVEL=0"
arduino-cli compile -b "$FQBN" \
  --build-property "compiler.c.extra_flags=$FLAGS" \
  --build-property "compiler.cpp.extra_flags=$FLAGS" \
  --libraries "$HERE/libraries" --output-dir "$HERE/build" "$HERE/GeekNestCX8ANM"
# Imagen unica para grabar en 0x0:
# esptool.py --chip esp32 merge_bin -o CX8ANM-completo.bin --flash_mode dio --flash_freq 40m --flash_size 4MB \
#   0x1000 build/GeekNestCX8ANM.ino.bootloader.bin 0x8000 build/GeekNestCX8ANM.ino.partitions.bin \
#   0xe000 <core>/tools/partitions/boot_app0.bin 0x10000 build/GeekNestCX8ANM.ino.bin
