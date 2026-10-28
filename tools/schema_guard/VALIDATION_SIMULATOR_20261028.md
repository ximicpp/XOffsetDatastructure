# Apple simulator follow-up, 2026-10-28

The optional x64 iOS simulator fixture was compiled and linked on the Apple
Silicon Mac using the iOS Simulator SDK. The installed iOS 26.4.1 simulator
runtime reported only arm64 support. Starting the x64 fixture with simctl
failed with signal 6: dyld_sim was not a compatible architecture. The runtime
matrix records ios-simulator-x64 as FAIL_RUNTIME, rather than a wire pass.

A separate matrix run using the same newly built fixtures recorded:
- macos-arm64 PASS_REFERENCE
- macos-x64 PASS_WIRE
- ios-simulator-arm64 PASS_WIRE
- ios-simulator-x64 FAIL_RUNTIME

Other targets were not configured in that invocation and therefore remain
BLOCKED_RUNTIME in its report. These statuses must not be combined into a
fabricated single full-gate pass. The earlier full-gate Windows x64 result
is retained in VALIDATION_WINDOWS_20261028.md. A simulator is not a physical
iPhone; neither the iOS device runtime nor x64 simulator has a passing result.

Mac evidence:
- results/schema-wire-apple-x64sim-20261028-v1/
- results/schema-wire-apple-x64sim-report-20261028-v1/
- results/schema-ios-sim-x64-20261028-v1/

A real Windows ARM64 VM is possible on Apple Silicon. Microsoft provides a
90-day Windows 11 IoT Enterprise LTSC ARM64 evaluation without a product key,
subject to registration and the evaluation terms. No guest was installed and
no registration or license terms were accepted by this test.

Sources:
- https://learn.microsoft.com/en-us/windows/arm/iso
- https://www.microsoft.com/en-us/evalcenter/download-windows-11-iot-enterprise-ltsc-eval

A Windows-guest shared-directory worker has also been added. A local Windows
x64 exercise ran the native fixture identity, read the Mac reference corpus,
and wrote a ten-file byte-identical corpus through that worker. This tests the
worker on a local filesystem; neither a VM share nor Windows arm64 was tested.
The copied evidence archive is:
results/schema-wire-apple-x64sim-report-20261028-v1/shared-real-native-v3.tar.gz
