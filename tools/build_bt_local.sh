#!/bin/bash
# Build the CX8ANM Bluetooth image (speaker + transmitter), same sources with CX8_BT_FIRMWARE.
set -e
T=/home/claude/tools
export ARDUINO_DIRECTORIES_DATA=$T/ardata ARDUINO_DIRECTORIES_USER=$T/sketchbook
S=$(cd "$(dirname "$0")/.." && pwd)/GeekNestCX8ANM
FLAGS="-I$S/src -DBOARD_V5A -DCORE_DEBUG_LEVEL=0 -DCX8_BT_FIRMWARE"
$T/arduino-cli compile -b espressif:esp32:esp32:PartitionScheme=min_spiffs,PSRAM=disabled,CPUFreq=240,FlashMode=dio,FlashFreq=40 \
  --build-property "compiler.c.extra_flags=$FLAGS" \
  --build-property "compiler.cpp.extra_flags=$FLAGS" \
  --build-path /home/claude/fw/build_bt \
  --libraries /home/claude/fw/libraries --output-dir /home/claude/fw/out_bt "$@" $S 2>&1 | grep -v "^Error initializing\|Downloading index"
