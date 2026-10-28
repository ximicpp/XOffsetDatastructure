#!/bin/bash
# Signs the test host app (built with cmake/toolchains/ios-device.cmake and
# -DXOFFSET_TEST_HOST=ON) for one iPhone, with a development certificate of the Apple
# ID signed in to Xcode (a free account is enough; its profiles last 7 days).
#
#   tools/test_host/sign_ios_app.sh APP OUT_APP DEVICE_UDID [TEAM_ID]
#
# The provisioning profile comes from Xcode: a stub app with the same bundle identifier
# is built once with automatic signing (xcodebuild -allowProvisioningUpdates registers
# the phone and creates the profile); its profile and entitlements are then put on a
# copy of APP, whose libraries and executable are signed with the same certificate.
# TEAM_ID defaults to the team of the first "Apple Development" certificate.
# macOS bash 3.2. Exit 0 signed, 1 failed (the log says where), 2 usage.
set -uo pipefail
APP=${1:?app}; OUT=${2:?out app}; UDID=${3:?device udid}; TEAM=${4:-}
REPO=$(cd "$(dirname "$0")/../.." && pwd)
BUNDLE_ID=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP/Info.plist") || exit 2
MIN_OS=$(/usr/libexec/PlistBuddy -c 'Print :MinimumOSVersion' "$APP/Info.plist") || exit 2
W=$(mktemp -d -t xo-sign)
say() { echo "sign_ios_app: $*"; }

if [ -z "$TEAM" ]; then
    name=$(security find-identity -v -p codesigning | sed -n 's/.*"\(Apple Development: [^"]*\)".*/\1/p' | head -1)
    [ -n "$name" ] || { say "no Apple Development certificate: sign in to Xcode (Settings > Accounts) and let it create one"; exit 1; }
    TEAM=$(security find-certificate -c "$name" -p | openssl x509 -noout -subject -nameopt multiline | sed -n 's/^ *organizationalUnitName *= *//p' | head -1)
    [ -n "$TEAM" ] || { say "cannot read the team of '$name'"; exit 1; }
fi
say "bundle $BUNDLE_ID, team $TEAM, device $UDID"

# 1. stub app with automatic signing: Xcode makes (or reuses) the profile for this phone
mkdir -p "$W/stub"
cp "$APP/Info.plist" "$W/stub/Info.plist"
cat > "$W/stub/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.20)
project(xo_stub LANGUAGES CXX OBJCXX)
add_executable(xo_stub MACOSX_BUNDLE "$REPO/tools/test_host/host_ios.mm" "$REPO/tools/test_host/host_core.cpp")
set_target_properties(xo_stub PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "$W/stub/Info.plist" OUTPUT_NAME xoffset_test_host
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "$BUNDLE_ID"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "$TEAM"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
target_compile_features(xo_stub PRIVATE cxx_std_17)
target_link_libraries(xo_stub PRIVATE "-framework UIKit" "-framework CoreGraphics" "-framework Foundation")
EOF
cmake -S "$W/stub" -B "$W/stub-build" -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_OS" \
    -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_XCODE_GENERATE_SCHEME=ON > "$W/stub-configure.log" 2>&1 \
    || { say "stub configure failed:"; tail -20 "$W/stub-configure.log"; exit 1; }
xcodebuild -project "$W/stub-build/xo_stub.xcodeproj" -scheme xo_stub -configuration Release \
    -destination "id=$UDID" -allowProvisioningUpdates -allowProvisioningDeviceRegistration \
    -derivedDataPath "$W/dd" build > "$W/stub-build.log" 2>&1 \
    || { say "stub build failed (Apple ID signed in to Xcode? phone connected, unlocked and trusted?):"; grep -E "error:|warning: .*[Pp]rovision" "$W/stub-build.log" | head -20; exit 1; }
STUB=$(find "$W/dd" -type d -name 'xoffset_test_host.app' -path '*iphoneos*' | head -1)
[ -f "$STUB/embedded.mobileprovision" ] || { say "the stub has no embedded.mobileprovision"; exit 1; }
IDENTITY=$(codesign -dvv "$STUB" 2>&1 | sed -n 's/^Authority=//p' | head -1)
codesign -d --entitlements - --xml "$STUB" > "$W/entitlements.plist" 2>/dev/null \
    || codesign -d --entitlements :- "$STUB" > "$W/entitlements.plist"
say "identity '$IDENTITY'; entitlements: $(plutil -convert json -o - "$W/entitlements.plist" 2>/dev/null)"

# 2. the real app: profile, libraries, then the app itself
rm -rf "$OUT"
cp -R "$APP" "$OUT"
cp "$STUB/embedded.mobileprovision" "$OUT/embedded.mobileprovision"
rm -rf "$OUT/_CodeSignature"
n=0
for lib in "$OUT"/Frameworks/*.dylib; do
    [ -e "$lib" ] || continue
    codesign --force --sign "$IDENTITY" --timestamp=none "$lib" > /dev/null 2>&1 || { say "signing $lib failed"; exit 1; }
    n=$((n + 1))
done
codesign --force --sign "$IDENTITY" --timestamp=none --entitlements "$W/entitlements.plist" "$OUT" \
    || { say "signing the app failed"; exit 1; }
codesign --verify --deep --strict "$OUT" || { say "the signed app does not verify"; exit 1; }
say "signed $OUT ($n libraries)"
rm -rf "$W"
