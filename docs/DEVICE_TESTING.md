# 真机测试（3.2.0-rc.2）

本页给 3.2.0-rc.2 预览版的真机测试：在一台干净的电脑上拉取这个版本，把手机接上 USB，
一条命令跑完，再把结果文件传回来。

第 1–6 节是 **Android 手机 + Linux 服务端**（3.2.0-rc.1 起），只看功能是否正确；第 7 节是
**iPhone**（3.2.0-rc.2 新增：库测试、wire 语料、SLG 计时和内存上限）。

## 1. 测什么

`tools/device/android_device.sh` 在手机上做两件事，和 Android 模拟器上已经通过的检查相同：

1. **库**：C++20 字段表构建（Android NDK r29，arm64-v8a）的全量 CTest，每个测试程序用 adb 推到
   手机的 `/data/local/tmp/xo` 运行；再做 wire 语料检查：手机读入 Linux 服务端构建（GCC 16、
   C++26 反射）写出的参照 wire，必须全部接受，手机自己写出的字节必须与参照逐字节相同。
   15 个编译失败测试（编译期应当报错的用例）要用 NDK，在电脑上跑。
2. **游戏 demo**（`demo/mapstate`）：Linux 服务端 A/B（GCC 16、C++26 反射）在电脑的 docker
   容器里运行；demo 客户端（开异常、关异常两个版本）在手机上运行，经 `adb reverse` 连到电脑
   （手机上的 127.0.0.1:47800–48049 就是电脑）。每个客户端跑自检、回放 A 写的快照语料、
   1/100/10000 个单位各 500 步（dense、compact、zstd 三种快照轮换）、恶意输入。

## 2. 准备电脑

macOS（Apple 芯片或 Intel）或 Linux（x86-64、arm64）都可以。磁盘留 8 GB，能访问 github.com、
dl.google.com 和 Docker Hub（x86-64 用 `mirror.gcr.io` 镜像）。

**macOS**：

```sh
xcode-select --install                       # git、python3（已装 Xcode 可跳过）
brew install cmake ninja colima docker       # 没有 Homebrew 先装：https://brew.sh
colima start --cpu 4 --memory 6 --disk 40    # 或者用 Docker Desktop
```

**Linux**：git、python3、unzip、ninja、docker（当前用户能运行 `docker info`），CMake 3.30 以上
（发行版的太旧时：`pip3 install --user cmake`）。

adb 和 NDK 不用自己装：没有时脚本从 dl.google.com 下载 platform-tools 和 NDK r29
（约 1 GB，校验 SHA-1）到 `~/xo-device/sdk`。已经装了的话，`adb` 在 PATH 上或设 `ADB=`，
NDK 设 `ANDROID_NDK=`。

## 3. 准备手机

要求：64 位（arm64-v8a）、Android 9 以上。

1. 设置 → 关于手机 → 连续点「版本号」7 次，打开开发者选项。
2. 开发者选项里打开「USB 调试」；建议同时打开「不锁定屏幕」（充电时保持唤醒），长时间运行不被
   休眠打断。小米、OPPO、vivo 等若有「USB 调试（安全设置）」不用开：脚本只在
   `/data/local/tmp` 下运行程序，不安装 app。
3. 用 USB 数据线接到电脑，解锁手机，弹出「允许 USB 调试吗？」时选「始终允许」并确定。

## 4. 拉取这个版本

```sh
git clone --branch v3.2.0-rc.2 --depth 1 https://github.com/ximicpp/XOffsetDatastructure.git xoffset-3.2.0-rc.2
cd xoffset-3.2.0-rc.2
cat XOFFSET_VERSION          # 3.2.0-rc.2
git rev-parse HEAD           # 与发版记录里的提交一致
```

子模块（TypeLayout 和用到的 boost 头文件）由脚本按需拉取。

## 5. 运行

```sh
bash tools/device/android_device.sh
```

依次做 setup（检查工具，缺 adb/NDK 时下载）、build（Android 程序、docker 里的服务端和参照语料）、
run（手机上的三步）。第一次大约 30–60 分钟，大部分在编译。可以分开跑：

```sh
bash tools/device/android_device.sh setup
bash tools/device/android_device.sh build    # 编一次
bash tools/device/android_device.sh run      # 每台手机跑一次
```

接了多台手机时用 `adb devices -l` 看序列号，再 `ANDROID_SERIAL=<序列号> bash tools/device/android_device.sh run`，
一台一台跑。跑完 `bash tools/device/android_device.sh clean` 删掉容器和手机上的临时目录。
运行中途手机断开或锁屏导致失败时，重新 `run` 即可，不用重新 build。

退出码：0 全部通过，3 有步骤失败（看 RESULT.md），2 缺工具或没连上手机（什么都没跑）。

## 6. 回传结果

结束时脚本打印结果包的位置，形如：

```
result: ~/xo-device/3.2.0-rc.2-<提交前 7 位>/out-<UTC 时间>/RESULT.md; send back ~/xo-device/3.2.0-rc.2-<提交前 7 位>/out-<UTC 时间>.tar.gz
```

把 **`out-<UTC 时间>.tar.gz`** 拖进项目对话里，并说一下手机型号。包里有：

| 文件 | 内容 |
| --- | --- |
| `RESULT.md` | 每一步的结论（先看它） |
| `device.txt` | 型号、SoC、Android 版本、ABI、页大小、内存 |
| `ctest.log`、`host_compile_tests.txt` | 手机上的 CTest、电脑上的编译失败测试 |
| `corpus-*.log`、`corpus.sha256` | wire 语料检查 |
| `demo/E2E_RESULT.md` 和各目录 | demo 每个客户端的自检、回放、三档、恶意输入 |
| `SHA256SUMS` | 以上文件的校验和 |

通过的样子：RESULT.md 里 `library CTest (phone)`、`wire corpus`、`demo` 三行都是 passed，
编译失败测试一行显示全部通过。

## 7. iPhone

iPhone 上不能直接运行命令行程序：`tools/device/ios_device.sh` 把每个测试编成测试宿主 app
（`tools/test_host`）里的一个模块，用 Xcode 里登录的 Apple ID 签名，装到手机上一次跑一个，
结果从 app 的数据容器取回。**这一步已在 iOS 模拟器上跑通，还没有在 iPhone 上跑过。**

准备（一台 Mac）：

1. 装 Xcode；`export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer`；PATH 上有
   CMake 3.30 以上和 ninja（`brew install cmake ninja`）。
2. Xcode → Settings → Accounts 里登录 Apple ID（免费账号即可，签名 7 天有效）。
3. iPhone 用数据线接到 Mac，点「信任此电脑」；在 Xcode 的 Devices 窗口里出现一次之后，打开
   设置 → 隐私与安全性 → 开发者模式，按提示重启。
4. 自动锁定设为「永不」，插上充电器。

运行（在拉取的目录里）：

```sh
bash tools/device/ios_device.sh device out-iphone --steps build,info,ctest,slg,mem
```

- 接了多台 iPhone 时，用 `xcrun devicectl list devices` 查 UDID，加 `--udid <UDID>`。
- wire 语料检查：步骤加 `,corpus` 和 `--ref <参照目录>`。参照目录就是第 5 节 Android 脚本生成的
  `~/xo-device/3.2.0-rc.2-<提交前 7 位>/ref-corpus`（Linux GCC 16 C++26 写出）。
- `slg` 要和 FlatBuffers 对照时，加 `--fb-include <flatbuffers 25.12.19 的 include 目录>` 和
  `--fb-generated <slg_generated.h 所在目录>`；不加只测 XOffset。
- 只有 `net`、`clients` 两步用 `--server-host`（同一 Wi-Fi 下 Mac 的地址：`ipconfig getifaddr en0`）；
  第一次会弹「本地网络」权限，3 分钟内点允许。iPhone 上的 demo 这一版不在步骤里。

回传：`tar -czf out-iphone.tar.gz out-iphone`，把包拖进项目对话。先看 `RESULT.md`，其余是
`info.txt`、ctest 日志、`slg/ANALYSIS.md` 和 `run_*.jsonl`、`mem/SUMMARY.md`。`mem` 的 commit
探测被系统杀掉（rc 125）是预期的，那正是要测的内存上限。

## 8. 常见问题

- **`adb devices` 显示 unauthorized**：手机没点「允许 USB 调试」。解锁手机重新插线；仍不弹时在
  开发者选项里「撤销 USB 调试授权」后再插。
- **Linux 上显示 no permissions**：缺 udev 规则。装发行版的 `android-sdk-platform-tools-common`
  （Debian/Ubuntu）或 `android-udev-rules`（Arch），重新插线。
- **容器写不了工作目录**：工作目录默认在 `~/xo-device`，Docker Desktop 和 colima 默认共享家目录；
  改了 `XO_WORK` 时要放在共享的目录下。
- **端口被占用**：电脑上 127.0.0.1 的 47800–47849、47900–47949、48000–48049 要空着。
- **只想重跑某一步**：`XO_SKIP="ctest corpus" bash tools/device/android_device.sh run` 只跑 demo。
- **手机上的性能和内存**：这一版不测。「在手机上实测性能和体积」会在同一脚本上加 `slg`、`mem` 子命令。
