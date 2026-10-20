# 唯一r1-m2-combined-v1：初始与完整功能执行交接

实际组合source已由root独立接受：`reviews/COMBINED_SOURCE_ACCEPTANCE.json`，8470B/SHA256 `6b92309f8e147df886fa2651a00a3cfb3b9d372aceedba073b08531e2e7bd31d`。候选main为279066B/`155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501`；combined.patch2520B/`02273bdee63a43031183a370ad19f18b3af5e1910e483711c0e27573e3dc8ccb`；IDENTITY45694B/`f88dba6a6f18a6b70bbe47253d1edae360463c8e812051694202f15175486d67`。只有两份被保留patch的精确union，154/14、两种应用顺序相同，默认根154与旧候选不改。

## 当前不可变请求

| 请求 | bytes | SHA256 | 执行范围 |
|---|---:|---|---|
| REQUESTS_COMBINED_INITIAL.json | 53428 | `3006c3057c6f05300d391e71cb5a70ccdfd0417abc1819bdc9df1c836bde0e2c` | 三组：normal73、SAN73、external4 |
| REQUESTS_COMBINED_EXTERNAL.json | 202565 | `88e8d3a1ab95c0ac5e6d2b65632a38666447f17d781a1ae028229610d0cf07ff` | 两份原external fixture各normal/SAN，4compile+4native |
| REQUESTS_COMBINED_FULL.json | 84671 | `d53584c5feef956307aaa717e8b638248608dc576b5ac55c9665ca13fca352f7` | 初始实际通过后七组剩余完整门禁，不重复初始运行 |

INITIAL的完整闭包及固定工具没有更改，normal已由验证者启动。FULL当前是以上最终字节，待root/独立phase审阅准入；未声称已经执行或通过。初始工具独立固定，后续business准备不延迟/改变它们。

未执行FULL早期draft83337B/6209f54c…的实际旧字节、旧checker和生成器已保存在`build/c14-iteration-20261020-v2/review-drafts/combined-full-before-CTest-path-fix/`。最终修订只有：checker适配真实CTest `add_test(name "build/bin/Release/name")`（不是预想的bracket语法与build/tests路径），以及FULL明确登记INITIAL的唯一external执行argv和非操作性残留字段。没有变source、fixture、样本、模式、原功能预算或INITIAL；不回写已执行产物。

**执行INITIAL.jobs中的精确argv。** external request继承的informational `worker_argv`仍指旧M2请求，原run_native_targets不读取该字段，INITIAL第三job精确argv已正确指向REQUESTS_COMBINED_EXTERNAL.json。此历史字段不授权任何旧M2运行，不可单独使用。其实际`commands`全部是当前组合source，仅原test_r1_thread_handoff_v2/test_m2_owns各normal/SAN，4程序，source154与输入前后检查保留。

## 剩余七组执行与闭包

按FULL.jobs顺序，在当前host/backing/依赖/真实原global lease准入下执行：

1. `prepare_combined_business.py`：先只读验证INITIAL真实normal/SAN两个73-case full与external4，并按实际source/compileDB/assertions/SAN/CTest/JUnit绑定六个repo定向case；产出COMBINED_INITIAL_GATES。诚实写source/input bridge区分FULL request identity与INITIAL request identity，然后调用原business dispatcher的prepare（300s，dispatcher自取原lease）。
2. 原business bench派生（2400s）：新组合candidate普通/SAN原driver profiles；同身份控制/竞品不重编。
3. 原business functional派生：normal80+SAN80与各完整审计、原SML/query/9阶段canonical/owning emit/reload；main V1 84+extras8、AB4/C4双向。保留normal1800/audit600/SAN1800/audit600/compat1500的原顺序有限jobs；dispatcher本身在held lease之外。
4. 原package消费者（2400s）：storage/full安装、subproject、relocated prefix、archive与原依赖/API合同。
5. 原两共享runtime消费者（2400s）：ABI/state/跨TU等现有检查，新组合source真实构建。
6. 原lifecycle矩阵（3600s）：25normal+25SAN+2small-pool，52实际children；原OOM/new_handler/replaceable-delete/pool/资源模型及scope限制保留。
7. `finalize_combined_full.py`（120s）：重新核当前FULL全部88项工具/输入（含outer61B）、INITIAL、actual full child requests、源154/生产14、真实JUnit/inventory/compileDB、六repo实际targeted与external4、PREPARED/source bridge、business/V1/消费端/runtime/lifecycle refs；只有实际全通过才写COMBINED_FUNCTIONAL_GATES。

所有新实际产物在`/root/xoffset-c14-iteration/20261020/v2/validation/combined-*`及`validation/combined_full/`。各dispatcher仍使用原历史`run_job.py`和`Documents/.../build/vm-coordination/queue.lock`；外层不要再持lease后调用会自行取lease的dispatcher，不新增锁、重试或框架。external的精确outer argv自取一次原lease，里面的run_native_targets只接受held lease。outer61B launcher `c8ea06…`显式pin，当前compiler/libc++/runtime/dependency与SAN资源保留原实际闭包检查。

## 六repo定向scope的复用而非替代

实际组合full中必须有test_canonical_zero_target、test_canonical_cold_no_callback、test_canonical_registry_v2、test_owner_identity、test_same_arena_move、test_locator_soundness，各normal/SAN真实case。checker要求相同154内fixture/测试CMake与原failpoint源码、O3、最终assertions enabled、P2996/libc++/pthread、normal无SAN/SAN address+undefined与固定resource-dir、原SAN env、无disabled/expected-failure override、无额外命令参数、原Release binary路径、真实JUnit无failure/skip，并输出完整compile argv/CTest properties/source refs。

原manual targeted使用gnu++26；项目full的argv包含gnu++26后再加c++26，effective为项目原C++26。这一实际差异在FULL与checker输出明确披露，按原项目支持的C++26功能/断言scope提交独立审阅，不谎称argv逐字节相同。如独立审阅认定任何scope不等价，或实际checker发现缺项，停止后仅注册精确缺失case；不能伪造覆盖或默认再跑16程序。已存在M2 full metadata仅用于有限parser已知答案检查，不冒充实际组合通过。

两external fixture在原154/full inventory之外，必须自己的normal/SAN4实际程序；其初始请求已精确注册。原R1/M2独立B/native/full门禁从未充当组合通过。

## 薄派生与准备检查

`combined_full/DERIVATION.json`记录65文件M2→COMBINED身份派生与完整TOOLS.patch；18份C++/header fixture字节相同。变化仅candidate/kind/source/doc/output/request/receipt literals与初始/FULL request桥接。model/oracle/workload/flags/assertions/timeout/runner/消费者语义不改。`COMBINED_PREPARATION_CHECK.json`核对10个INITIAL与87个FULL本地pin（另有61B外部pin）、两个business/matrix manifests、65个派生身份、18个fixture、Python AST及6-case parser在既有metadata上的已知答案。此检查是源码就绪，不是平台/组合功能结果。

主源码、INITIAL、FULL及绑定工具到此保持当前最终字节；若独立审查后需要未执行FULL修订，先保留当前draft并明确新identity，不改INITIAL或历史before/after。当前核心新C++compile/native/queue为0，组合实际正确性由验证会话独占执行。无新72、额外机制/control、性能合并或Q/F准入；最终实际裸六角色与资源闭包仍另行验收。
