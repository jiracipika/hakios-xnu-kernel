#!/usr/bin/env bash
# backup-pixel7.sh — unrooted, READ-ONLY backup of a connected Pixel 7 to ./pixel7-backup-<date>/
# What this can and cannot do without root:
#   CAN: getprop dump, package list, /sdcard pull, bugreport.
#   CANNOT: image partitions (block devices are root-only). The system partitions are
#   recovered from Google's factory image instead — note the build fingerprint printed
#   at the end and download the matching one: https://developers.google.com/android/images#panther
# Run BEFORE any bootloader-unlock talk: `fastboot flashing unlock` wipes the device.
set -euo pipefail
OUT="${1:-pixel7-backup-$(date +%Y%m%d-%H%M%S)}"
DEV="$(adb shell getprop ro.product.device | tr -d '\r')"
[ "$DEV" = "panther" ] || { echo "refusing: device is '$DEV', expected 'panther'"; exit 1;
}
mkdir -p "$OUT"
adb shell getprop            > "$OUT/getprop.txt"
adb shell pm list packages   > "$OUT/packages.txt"
adb pull /sdcard "$OUT/sdcard" || echo "WARN: /sdcard pull incomplete (app-specific storage not included)"
adb bugreport "$OUT" >/dev/null 2>&1 || echo "note: bugreport failed (non-fatal)"
FINGERPRINT="$(grep -oP '^\[ro.build.fingerprint\]: \[\K[^\]]+' "$OUT/getprop.txt" || echo unknown)"
echo "build fingerprint: $FINGERPRINT"
echo "factory image for this build: https://developers.google.com/android/images (panther)"
echo "done: $OUT — user data only; partitions recover from the factory image"
