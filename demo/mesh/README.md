# Device mesh demo (跨设备数据互传 demo)

Any device (Linux, Windows, Android, iOS, macOS; all 64-bit little endian)
joins a mesh at a central hub. An object of one of six everyday data types
(Document, Order, SensorSeries, KvConfig, SceneTree, ChatRoom) travels device
to device as its XOffset wire: the sender does not serialize it and the
receiver does not parse it. Each receiver verifies the bytes, checks them
against a plain-std reference model, changes the object in place and sends it
on. The hub replays every job and checks that every device sent the same
bytes. The contract (schema, protocol, hops, programs) is
[INTERFACES.md](INTERFACES.md); results are in [RESULT.md](RESULT.md).

First version: pure relay. Each device keeps one TCP connection to the hub;
the hub registers it, tells everyone who is in the mesh and forwards each hop
to the next device without reading it. Devices never connect to each other,
so an emulator or a phone behind NAT needs no inbound port.

## Build

```bash
# hub, device programs, self-test (Linux)
cmake -S . -B build/mesh-host -G Ninja -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD=20 \
      -DXOFFSET_BUILD_MESH_DEMO=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF
cmake --build build/mesh-host
# a device target: add -DXMESH_PEER_ONLY=ON and a toolchain file from cmake/toolchains/
```

Programs land in `<build>/bin/Release/`: `xo_hub`, `xo_peer`, `xo_peer_noexc`
(built without exceptions, as engine clients are), `xmesh_selftest`,
`xmesh_schema_freeze`.

## Run

```bash
xo_hub --run demo --out out/demo --expect 3          # waits for 3 devices, then runs every job
xo_peer --hub HUB_HOST:47801 --name my-laptop        # on each device
xo_peer --join 'http://HUB_HOST:47802/join?hub=HUB_HOST:47801&run=demo'
```

The hub serves a dashboard at `http://HUB_HOST:47802/` (devices, jobs in
flight, the sender x receiver matrix, the latest objects) and a join page at
`/join` with a QR code of the join link; `xo_peer --join` takes that link as
it is. `--advertise HOST` puts a fixed host in the link (otherwise the host
the browser used, then the hub's LAN address). On the Android emulator the
host is `10.0.2.2`.

Hub options beyond INTERFACES.md section 6: `--max-per-device N` (jobs in
flight one device takes part in, default 2), `--advertise HOST`, and
`--http-port 0` to turn the dashboard off. Device options beyond it:
`--connect-s S` (how long to retry the first connection, default 30).

Output (`--out DIR`): `devices.csv`, `jobs.csv`, `hops.csv` (`hop` is the
route position: 0 is route[0]'s start, k its k-th receiver), `matrix.csv`,
`summary.txt` (`RESULT PASS` or `RESULT FAIL`) and `archive/<job>-<hop>.xmhop`
(each forwarded hop payload, the mailbox format).

## Cloud mesh

`tools/run_mesh.sh` builds the hub and every device column this host has a
toolchain for (Linux x86-64 GCC and Clang, Linux arm64 under qemu, Windows
MinGW / MSVC / clang-cl under wine, Android arm64 under qemu and x86-64),
runs each column's self-test, then one hub with two devices per column (with
and without exceptions) and writes `MESH_RESULT.md` with
`tools/mesh_report.py`.

```bash
demo/mesh/tools/run_mesh.sh --out build/mesh-run            # all columns available here
demo/mesh/tools/run_mesh.sh linux-x64-gcc windows-mingw      # some columns
```

Toolchains: `apt-get install wine g++-mingw-w64-x86-64-posix qemu-user-static
g++-aarch64-linux-gnu msitools clang-19 lld-19`; MSVC with
`tools/msvc-wine/fetch_msvc.py /opt/msvc` and `case_links.py`; the Android NDK
at `/opt/android-ndk-r29` (or `$ANDROID_NDK`).

## Files

| Path | What |
| --- | --- |
| `include/xmesh/` | frozen contract (schema, types, protocol, hop, result) and `net.hpp` |
| `src/types/`, `src/hop.cpp` | the six types, mutations, model, replay, evil cases, per-hop checks |
| `src/net.cpp` | TCP framing, POSIX and Winsock, no exceptions |
| `src/peer_main.cpp`, `src/peer/control.hpp` | the device program; control wires and the build line |
| `src/hub_main.cpp`, `src/hub/` | the hub: relay loop and scoring (`hub.cpp`), HTTP pages (`http.cpp`), QR encoder (`qr.cpp`) |
| `tools/run_mesh.sh`, `tools/mesh_report.py` | the cloud mesh run and its report |
