#!/bin/bash
# adb_run.sh PROGRAM ARGS...: CMAKE_CROSSCOMPILING_EMULATOR for an Android phone or emulator
# (tools/device/android_device.sh; ANDROID_SERIAL picks the device when several are attached).
# Pushes PROGRAM (once per content) to the device and runs it there with adb shell.
# An argument that is an absolute host path is mapped to $XO_DEV/fs<path>: an existing
# file or directory is pushed first; a path that does not exist yet is created on the
# device by the program and pulled back afterwards (e.g. --write DIR).
# Exit status = the program's exit status on the device. macOS bash 3.2 compatible.
set -u
ADB=${ADB:-adb}
DEV=${XO_DEV:-/data/local/tmp/xo}
STAMPS=${XO_ADB_STAMPS:-${TMPDIR:-/tmp}/xo-adb-stamps}
exe=$1; shift
q() { printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"; }
mkdir -p "$STAMPS"
name=$(basename "$exe")
sum=$(shasum -a 256 "$exe" 2>/dev/null || sha256sum "$exe"); sum=${sum%% *}
if [ "$(cat "$STAMPS/$name" 2>/dev/null)" != "$sum" ]; then
    $ADB shell "mkdir -p $DEV/bin" >/dev/null || exit 125
    $ADB push "$exe" "$DEV/bin/$name" >/dev/null || exit 125
    $ADB shell "chmod 755 $DEV/bin/$name" >/dev/null || exit 125
    echo "$sum" > "$STAMPS/$name"
fi
cmd="cd $DEV && "
for v in $(env | sed -n 's/^\(XOFFSET_[A-Za-z0-9_]*\)=.*/\1/p; s/^\(MAPDEMO_[A-Za-z0-9_]*\)=.*/\1/p'); do
    eval "val=\${$v}"; cmd="$cmd$v=$(q "$val") "
done
cmd="$cmd$DEV/bin/$name"
pulls=""
for a in "$@"; do
    case "$a" in
        /*)
            r=$DEV/fs$a
            if [ -e "$a" ]; then
                $ADB shell "rm -rf $(q "$r"); mkdir -p $(q "$(dirname "$r")")" >/dev/null
                $ADB push "$a" "$(dirname "$r")/" >/dev/null || exit 125
            else
                $ADB shell "rm -rf $(q "$r"); mkdir -p $(q "$(dirname "$r")")" >/dev/null
                pulls="$pulls$a
"
            fi
            cmd="$cmd $(q "$r")" ;;
        *) cmd="$cmd $(q "$a")" ;;
    esac
done
$ADB shell "$cmd"
rc=$?
printf '%s' "$pulls" | while IFS= read -r p; do
    [ -n "$p" ] || continue
    r=$DEV/fs$p
    if $ADB shell "test -e $(q "$r")" >/dev/null 2>&1; then
        mkdir -p "$(dirname "$p")"; $ADB pull "$r" "$(dirname "$p")/" >/dev/null
    fi
done
exit $rc
