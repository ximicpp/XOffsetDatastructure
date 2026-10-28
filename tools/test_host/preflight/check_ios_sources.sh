#!/bin/bash
# Syntax check of the iOS test host's Objective-C++ (host_ios.mm) on a computer without
# Xcode: Clang with the GNUstep Objective-C runtime ABI and stand-in UIKit declarations
# (preflight/UIKit/UIKit.h, only what host_ios.mm uses). Catches C++ and Objective-C
# mistakes before a Mac build; it is not a substitute for building with the iOS SDK.
#   tools/test_host/preflight/check_ios_sources.sh [clang++]
set -eu
here=$(cd "$(dirname "$0")" && pwd)
cxx=${1:-$(command -v clang++ || command -v clang++-18)}
"$cxx" -x objective-c++ -std=c++17 -fobjc-runtime=gnustep-2.0 -fobjc-arc -fblocks -fsyntax-only \
    -I"$here" -I"$here/.." "$here/../host_ios.mm"
echo "host_ios.mm: syntax ok"
