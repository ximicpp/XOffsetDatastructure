# M1 唯一内部筛选入口

工具为 `tools/c14_iteration_20261019/validation/screen/m1_screen.py`。仅 M1，不能用于 Q/F、旧窗口或其他候选。准备工具本身没有启动编译、native 或计时。所有执行由唯一 heavy 执行者发起。

固定协议：72配对、144实际children、AB/BA各36、0暖身/0条件器/0重试/0补样；命令600秒、child20秒、独立audit3600秒；主mutate的df71四族单侧t=2.289930854179058；九辅助的36族双侧t=3.327935133019535。取消候选不回收alpha。八阶段原值保留，read_sum逐child相加，total严格为八阶段之和（原native.total_ns另外保留，其含计时记账，不能替代上述total）。合法零阶段保留，相关log推断无定义，禁止晋升。

下列 `$R`、`$T`、`$D`、`$B`、`$O` 是文档路径缩写；实际request command必须使用展开后的绝对路径。`$R`为C14 WSL挂载工作树，`$T=$R/tools/c14_iteration_20261019/validation/screen`，`$D=$R/docs/coordination/c14-iteration-20261019-v1/validation/screen`，`$B=/root/xoffset-c14-iteration/20261019/v1/validation/m1-business-v2`，`$O=/root/xoffset-c14-iteration/20261019/v1/validation/M1-screen-20261019-v1`。

## 具体调用顺序

1. 业务bare binary/profile就绪后生成具体配置，无编译/native：

```text
/usr/bin/python3 -B $T/m1_screen.py config --root $R --out $D/CONFIG.json --output $O --control-profile /root/xoffset-receive-completion/20261019/v1/profile-normal-v2.json --candidate-profile $B/profile-normal-v2.json
```

配置从两个实际profile、adapter literals、sc_xoffset compile_commands交叉核对label/content/argv。实际driver为-O3 -DNDEBUG，拒绝-UNDEBUG/SAN/forced include。控制154用C14材料化根，实际旧build source14另核；旧SOURCE_MANIFEST差异仅保留为元数据事实。实际依赖/工具链/runtime/profile/adapter/helper/request逐文件绑定，避免扫描未使用vendor整树。候选完整154/14均核对，绝不以主头代替身份。

两角色prefix为**本轮2026-10-19重新核实并由统筹明确登记**的`/usr/bin/taskset -c 2`；含taskset/宿主loader/libc身份，native之前重新检查CPU2属于当前sched_getaffinity。不是自动继承旧CPU设置。

2. 原共享run_job的functional租约执行且只执行两次capture，out预先仅含其queue：

```text
/usr/bin/python3 -B $T/m1_screen.py capture --config $D/CONFIG.json --out /root/xoffset-c14-iteration/20261019/v1/validation/m1-screen-capture-v1 --queue /root/xoffset-c14-iteration/20261019/v1/validation/m1-screen-capture-v1/queue
```

外层request须owner W3、kind functional、timeout120、wait0、原lock_dir。这两次是固定输入功能capture，**不属于144，也不是暖身或推断样本**。每次保留完整native/wire/canonical/queries/stdout/stderr/receipt。无需describe/encode/validate额外native。

3. 全部门禁结束后由执行者写`GATES.json`，结构为`status:"passed", candidate_identity_sha256:<core/M1_IDENTITY.json hash>, gates:{...}`。键固定为codegen/mechanism/targeted/full_normal/full_san/business_SML/v1_bidirectional/consumers/safety，每值是`{path,identity:{bytes:<十进制字符串>,sha256}}`。可以多个门禁引用同一实际综合回执；独立审查必须确认其实际scope，不以字段名称冒充通过。builder只核精确文件，实际资格仍由独立review审核。

```text
/usr/bin/python3 -B $T/m1_screen.py freeze --config $D/CONFIG.json --capture /root/xoffset-c14-iteration/20261019/v1/validation/m1-screen-capture-v1 --gates $D/GATES.json --out $D/frozen-v1
```

freeze输出`freeze.json`和其SHA256。资源投影保守假定144份wire全不同，只对完整语义后byte-exact相同的canonical/query硬链接去重；保留每child原路径和去重证据。投影含7slot单child临时峰值及64MiB metadata，实际unique inode的st_blocks*512不超过4GiB；WSL guest及C backing分别覆盖峰值并留2GiB。时间投影为144×capture最大完整child+在线审计耗时+60秒非child余量，另留30秒seal；不能fit就拒绝启动。RLIMIT_FSIZE与RLIMIT_AS=16GiB均由原execute的子进程preexec设置，native实际RSS另核。

4. 独立review读取**该精确freeze和工具hash**后提供REVIEW.json：`kind:"independent-screen-admission",status:"passed",freeze_sha256`。宿主控制由执行者独立实施，HOST.json：`kind:"host-quiet-admission",status:"passed",freeze_sha256,quiet:true,original_global_lease:true,restoration_ready:true,ready_monotonic_ns:<WSL单调ns字符串>`，并附实际进程/创建时间/句柄、watchdog、电源等所需证据。此工具不暂停程序、不改电源。

5. 在ready→resume期间通过原共享run_job的timing租约运行。request owner W3/kind timing/timeout600/wait0，command argv顺序必须与下列一致，外部hash以真实freeze值替换：

```text
/usr/bin/python3 -B $T/m1_screen.py run --frozen $D/frozen-v1/freeze.json --freeze-sha256 HASH --review $D/REVIEW.json --host $D/HOST.json --out $O --queue $O/queue
```

独占一次attempt，目录已使用即拒绝。deadline保留当前child20秒及seal30秒，不能fit停止并封存前缀；不延时、不补样。全体原始body由借用的完整G.native_result校验后，canonical/query与固定oracle逐字节比对，才原子替换为硬链接。失败或partial不去重，不删除其证据。所有定向安全/功能通过仍不能保证筛选结果改善。

6. **先恢复用户进程**；恢复回执RESTORED.json要求`status:"passed",freeze_sha256,user_processes_restored:true,resumed_monotonic_ns:<WSL单调ns字符串>,power_before,power_after`，两个power均为原Balanced GUID `381b4222-f694-41f0-9685-ff5bb260df2e`。工具审计first child≥ready且last child≤resume。

7. 只读`pin --out $O`取得外部postrun pin（主manifest与queue manifest的组合hash），由执行者在独立记录保存。通过**另一个**原共享run_job validation租约/timeout3600/wait0运行离线审计，audit queue不得放入原筛选目录：

```text
/usr/bin/python3 -B $T/m1_screen.py audit --out $O --restoration $D/RESTORED.json --result $D/AUDIT.json --freeze-sha256 HASH --postrun-pin POSTRUN --audit-queue /root/xoffset-c14-iteration/20261019/v1/validation/m1-screen-audit-v1/queue
```

原queue的六文件list manifest逐个核body、runid/owner/kind/cleanup/command cap/实际argv/时间；capture也同核。若外层kill导致无seal，使用同样3600原lease的`failure`，参数与audit相同但无`--postrun-pin`，结果写独立FAILURE.json；只归档失败prefix身份及queue，不补造完整72对结果。run目录保持原状。

独立统计复算、完整工程/资源/宿主验收后根统筹才可决定保留M1。AUDIT.json始终不自动写performance_accepted=true，也不授权Q/F。
