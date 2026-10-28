#!/usr/bin/env bash
# Device mesh demo: the cloud side of the Mac batch (run_mesh_mac.sh). Builds the
# Android arm64 device programs of COMMIT with the NDK (C++20 field lists, API 28,
# PIE, static libc++), adds the source (with the header-only submodules) and the
# cloud's mailbox for the Mac devices (run_mailbox.sh pre: MAILBOX_DIR/mac_inbox),
# and writes into OUT_DIR:
#
#   mesh_mac_pack_<sha7>.tar.gz.part-NN   the pack in 10 MB parts (the Mac fetches
#                                         files of at most 16 MB)
#   run_mesh_mac.sh                       with the pack name and sha256 filled in
#   cloud_mailbox_<sha7>.tar.gz           the cloud's mailbox state after `pre`
#                                         (plan, box, device list) for `post`
#
#   demo/mesh/tools/pack_mesh_mac.sh REPO COMMIT MAILBOX_DIR OUT_DIR
#
# COMMIT must be the commit the mailbox was planned with (MAILBOX_DIR/mac_inbox/
# INBOX_INFO.txt). Prints the sha256 of every file it writes.
set -euo pipefail
REPO=$1; COMMIT=$2; MB=$3; OUT=$4
NDK=${ANDROID_NDK:-/opt/android-ndk-r29}
TAG=$(git -C "$REPO" rev-parse --short=7 "$COMMIT")
FULL=$(git -C "$REPO" rev-parse "$COMMIT")
grep -q "^commit=$FULL\$" "$MB/mac_inbox/INBOX_INFO.txt" || { echo "the mailbox was not planned at $FULL" >&2; exit 1; }
ROOT=/Volumes/XOffsetCS/mesh
P=$ROOT/mesh-$TAG
rm -rf "$P"; mkdir -p "$P/src" "$P/android/bin"
git -C "$REPO" archive "$FULL" | tar -x -C "$P/src"
for sm in external/typelayout; do git -C "$REPO/$sm" archive HEAD | tar -x -C "$P/src/$sm"; done
# only the boost libs the library uses (a full --recursive checkout has ~160)
BOOST_LIBS=${BOOST_LIBS:-"assert config container container_hash core intrusive move predef static_assert throw_exception type_traits winapi"}
for l in $BOOST_LIBS; do
    d="$REPO/external/boost/libs/$l"; [ -e "$d/include" ] || { echo "missing boost lib $l" >&2; exit 1; }
    mkdir -p "$P/src/external/boost/libs/$l"; git -C "$d" archive HEAD | tar -x -C "$P/src/external/boost/libs/$l"
done

PROGS="xo_peer xo_peer_noexc xmesh_selftest xmesh_selftest_noexc"
B=$(mktemp -d)
cmake -S "$P/src" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD=20 -DXOFFSET_BUILD_MESH_DEMO=ON \
    -DXMESH_PEER_ONLY=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF \
    -DCMAKE_TOOLCHAIN_FILE="$P/src/cmake/toolchains/android-arm64.cmake" -DANDROID_NDK="$NDK" > "$B.configure.log" 2>&1 \
    || { tail -20 "$B.configure.log" >&2; exit 1; }
# shellcheck disable=SC2086
cmake --build "$B" --parallel "$(nproc)" --target $PROGS > "$B.build.log" 2>&1 || { tail -30 "$B.build.log" >&2; exit 1; }
STRIP=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip
for p in $PROGS; do "$STRIP" --strip-debug -o "$P/android/bin/$p" "$B/bin/Release/$p"; done
file "$P/android/bin/xo_peer"
rm -rf "$B" "$B.configure.log" "$B.build.log"
cp -R "$MB/mac_inbox" "$P/mac_inbox"
{
    echo "commit=$FULL"
    echo "ndk=$(grep Pkg.Revision "$NDK/source.properties")"
    echo "android=arm64-v8a API 28 PIE c++_static, C++20 field lists: $PROGS"
    echo "built_utc=$(date -u +%FT%TZ)"
    sed 's/^/inbox_/' "$MB/mac_inbox/INBOX_INFO.txt"
} > "$P/PACK_INFO.txt"

mkdir -p "$OUT"
NAME=mesh_mac_pack_$TAG.tar.gz
tar -czf "$OUT/$NAME.tmp" -C "$ROOT" "mesh-$TAG"
mv "$OUT/$NAME.tmp" "$OUT/$NAME"
PACK_SHA=$(sha256sum "$OUT/$NAME" | cut -c1-64)
rm -f "$OUT/$NAME".part-*
split -b 10m -d -a 2 "$OUT/$NAME" "$OUT/$NAME.part-"
rm -f "$OUT/$NAME"
sed -e "s/@SHA7@/$TAG/g" -e "s/@PACK_SHA@/$PACK_SHA/g" "$REPO/demo/mesh/tools/run_mesh_mac.sh" > "$OUT/run_mesh_mac.sh"
chmod +x "$OUT/run_mesh_mac.sh"
# the cloud's state for `run_mailbox.sh post` in a later session
tar -czf "$OUT/cloud_mailbox_$TAG.tar.gz" -C "$MB" plan cloud_devices.txt devices.in label.txt mac_inbox.tar.gz
rm -rf "$P"
echo "joined $NAME $PACK_SHA"
( cd "$OUT" && sha256sum "$NAME".part-* run_mesh_mac.sh "cloud_mailbox_$TAG.tar.gz" )
