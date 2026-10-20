# R1 实际 screen freeze 接入窄审

日期：2026-10-20（Asia/Shanghai）；审查者 `v2_stats_review`。本次只核已审实现进入实际freeze的差分、精确引用和原门禁接入；未重做统计测试、完整runtime链审计、编译、native、采样或宿主操作。

**结论：统计/调度和门禁接入通过。实际host/quiet、完整身份和资源最终准入仍由root并行核定；本文件本身不授权启动window，也不批准Q/F。**

## 冻结身份与已审实现

- 实际文件 `validation/screen/R1/frozen-v1/freeze.json`：1827170 bytes，SHA256 `c168eed40879c5339e91a390506eb6c32cce73fc47ea407ab7ae51cb6a437cf3`。
- 全部POLICY字段直接等于已审R1 worker：主read_sum，72对/144child，AB/BA各36、df71、原4主/36辅助，临界值 `2.289930854179058` / `3.327935133019535`，0暖身/重试/替换，20秒child/600秒window/3600秒audit、30秒seal等原预算不变。
- 实际完整144项schedule逐项等于design children按角色展开，完整randomization也与已审design一致；design原始字节SHA为 `3c835b01d1b84dbd06a28d0660a932fc97b4148f68bc71703a674f146309410d`。候选/控制方向没有改变。
- runtime.tools绑定的实际R1 worker仍为59903 bytes、SHA `0ae713213e7f5cfeba5cba8cd7dded155fe44a3114fbcecabf30f23f6f651a21`；统计预审、schedule审查文件的实际字节与freeze记录一致。此前对合法零、全部undefined禁止晋升、辅助下界>1否决、derived先求和及失败无summary的实现结论可以在此精确身份继续适用。

## 十一门禁和实际配置接入

`gate_admission`实际文件 `validation/screen/R1/GATES_v2.json` 为5327 bytes、SHA `a04a9e686e8da43ccf55b236ec0f676450618c9a17c346ccd05386ae0ca7034d`。其十一项引用与freeze的path/SHA/bytes数值全部相同；旧归档的部分bytes用整数、freeze用规范十进制字符串，规范化后相同，不涉及文件变化。

对 codegen、mechanism、targeted、full_normal、full_san、business_SML、v1_bidirectional、consumers、lifecycle、safety、engineering，逐项核实际receipt bytes/SHA、status passed、同一candidate_identity以及与freeze runtime一致的source154/production14；各独立review引用也核对实际bytes/SHA。codegen/safety/engineering明确引用root的 `reviews/R1_SCREEN_GATES_v2/` 三份最终receipt，独立review为 `R1_SCREEN_GATE_ACCEPTANCE.json`；没有残留pending draft或借用M1门禁。本次不重复此前各原始门禁内容审查。

config_identity为605133 bytes、SHA `55b98b943b66d865c17f85dd5cdcfc73c64cb4913386bbd42df2d19385b08c33`。两角色effective argv保持 `-O3 -DNDEBUG`，不含 `-UNDEBUG`、sanitize、forced include、common_diag或C14_DIAG；profile/binary路径均为已有bare normal业务路径。读取实际profile JSON并核其bytes/SHA与freeze一致，profile所列xoffset binary路径也与config对应：

| 角色 | profile SHA256 |
|---|---|
| control | `64a0a6756f2e60f6cdf54654b5c5b2b7bb5227ea038d58322ab5936e4b21d00d` |
| candidate | `243fdc2c96977462e5896e7457598bb87a19e1488d65f953816e64360fe0c866` |

此处不重复实际binary、依赖和所有runtime库的身份审计，root负责最终完整闭包准入。

## 资源capture隔离

freeze引用独立路径 `/root/xoffset-c14-iteration/20261020/v2/validation/r1-screen-resource-capture-v1`；实际manifest SHA与freeze projection.capture_manifest一致。其status明确 `timing_inference_samples=0`、`warmup=false`、`retries=0`，与独立的 `R1-screen-20261020-v2-v1` screen输出路径分开。

该capture仅支持资源投影；本审查不从capture阶段时间推断收益，不把其两个进程混入144项schedule，也不复活旧M1窗口。实际freeze仍标 `independent_review_required=true`、`performance_accepted=false`。性能晋升必须等待唯一完整窗口、恢复、独立离线审计/重算及root工程判定。

文中相对路径均以 `docs/coordination/c14-iteration-20261020-v2/` 为根。审查后停止扫描，供root决定是否进入quiet与正式计时。
