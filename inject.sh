#!/bin/sh
# Deploys libespmod.so to a device over adb and arms it for one package.
# Mechanism: wrap.<package> system property -> LD_PRELOAD at next app start.
# Works for debuggable apps, or on rooted / userdebug devices.
# Usage: inject.sh <package>            arm and launch
#        inject.sh --remove <package>   disarm and delete the library
set -eu

LIB_SHA256=""
DEST_DIR="${ESPMOD_DEST:-/data/local/tmp}"
HERE="$(cd "$(dirname "$0")" && pwd)"
LIB="$HERE/build/libespmod.so"
DEST="$DEST_DIR/libespmod.so"

die() { echo "inject.sh: $*" >&2; exit 1; }

REMOVE=0
if [ "${1:-}" = "--remove" ]; then REMOVE=1; shift; fi
PKG="${1:-}"
[ -n "$PKG" ] || die "usage: inject.sh [--remove] <package>"
case "$PKG" in *[!A-Za-z0-9._]*) die "invalid package name: $PKG" ;; esac

command -v adb >/dev/null 2>&1 || die "adb not found in PATH"
[ "$(adb get-state 2>/dev/null)" = "device" ] || die "no authorised adb device"

if [ "$REMOVE" -eq 1 ]; then
  adb shell setprop "wrap.$PKG" '""'
  adb shell rm -f "$DEST"
  adb shell am force-stop "$PKG"
  echo "disarmed $PKG"
  exit 0
fi

[ -f "$LIB" ] || die "missing $LIB"
[ -n "$LIB_SHA256" ] || die "unstamped script: use the inject.sh from the CI artifact"
if command -v sha256sum >/dev/null 2>&1; then
  ACTUAL="$(sha256sum "$LIB" | cut -d' ' -f1)"
else
  ACTUAL="$(shasum -a 256 "$LIB" | cut -d' ' -f1)"
fi
[ "$ACTUAL" = "$LIB_SHA256" ] || die "sha256 mismatch: expected $LIB_SHA256, got $ACTUAL"

adb push "$LIB" "$DEST" >/dev/null
adb shell chmod 755 "$DEST"
adb shell setprop "wrap.$PKG" "LD_PRELOAD=$DEST"
adb shell am force-stop "$PKG"
adb shell monkey -p "$PKG" -c android.intent.category.LAUNCHER 1 >/dev/null
echo "armed $PKG with $DEST; watch: adb logcat -s espmod"
