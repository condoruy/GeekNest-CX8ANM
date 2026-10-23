#!/bin/bash
# Builds a release into flasheo/: full image (cable), app image (WiFi update) and ota.json.
# usage: tools/make_release.sh <version> "<notes>"
# Requires arduino-cli with core esp32 2.0.17 and esptool.py. Set ESP32_CORE to the core folder
# (the one that contains tools/partitions/boot_app0.bin).
set -e
VERSION="$1"; NOTES="$2"
[ -n "$VERSION" ] || { echo "usage: $0 <version> \"<notes>\""; exit 1; }
HERE="$(cd "$(dirname "$0")/.." && pwd)"
grep -q "CX8_FIRMWARE_VERSION \"$VERSION\"" "$HERE/GeekNestCX8ANM/src/ui/splash.h" || {
  echo "Update CX8_FIRMWARE_VERSION in GeekNestCX8ANM/src/ui/splash.h to $VERSION first"; exit 1; }
"$HERE/build.sh"
B="$HERE/build"; F="$HERE/flasheo"
ESPTOOL="${ESPTOOL:-esptool.py}"
"$ESPTOOL" --chip esp32 merge_bin -o "$F/CX8ANM-GeekNestV5A-completo.bin" --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 "$B/GeekNestCX8ANM.ino.bootloader.bin" 0x8000 "$B/GeekNestCX8ANM.ino.partitions.bin" \
  0xe000 "${ESP32_CORE:?set ESP32_CORE}/tools/partitions/boot_app0.bin" 0x10000 "$B/GeekNestCX8ANM.ino.bin"
cp "$B/GeekNestCX8ANM.ino.bin" "$F/CX8ANM-GeekNestV5A-app.bin"
SIZE=$(stat -c %s "$F/CX8ANM-GeekNestV5A-app.bin")
SHA=$(sha256sum "$F/CX8ANM-GeekNestV5A-app.bin" | cut -d' ' -f1)
python3 - "$F/ota.json" "$VERSION" "$SIZE" "$SHA" "$NOTES" <<'PY'
import json, sys
path, version, size, sha, notes = sys.argv[1:]
url = "https://raw.githubusercontent.com/condoruy/GeekNest-CX8ANM/main/flasheo/CX8ANM-GeekNestV5A-app.bin"
json.dump({"version": version, "url": url, "size": int(size), "sha256": sha, "notes": notes},
          open(path, "w"), ensure_ascii=False, indent=2)
PY
echo "Release $VERSION ready in flasheo/ ($SIZE bytes, sha256 $SHA)"
