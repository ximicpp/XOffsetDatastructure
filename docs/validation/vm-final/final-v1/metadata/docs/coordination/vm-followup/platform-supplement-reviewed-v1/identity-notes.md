# G4 既有证据身份映射（只读，未执行矩阵）

本文件与 identity-mapping.json 比较当前相关基础F候选及既有profile；最终G4包和新角色身份仍未冻结。分类用于父会话裁决，不把缺少旧二进制哈希直接变成全量重跑要求。所有归档和证据保持原字节；没有构建、编译器调用、测试、Docker或远端工作。

StageIII原生完整48项的129个源文件记录中，127项与当前相同，仅 allocator 离线审计器及其Python测试两项不同。它们不在CMake构建/CTest闭包，48项运行程序/子进程/签名及3项配置检查相关输入不变，可继承既有48证据。签名3项已经包含在48内。

StageII原生full/storage消费包的安装/导出/consumer/依赖/许可证/专用package validator相关闭包不变。tests/CMake只新增不启用的allocator测试，其他变化是未启用工具或未调用文档/入口。每套installed3+subproject3可按原profile继承；storage实际用/usr/bin/clang++ Apple21 C++17，不能标成P2996。Linux包相关源码同样不变；独立审查已核对container-input.json及StageII索引，full为amd64/P2996镜像，storage为ARM64/GNU13.3本地镜像，外层绑定已补足；没有完整原docker argv/每容器daemonID或二进制seal，保留相应来源限制。

历史wire的直接harness/bundle、F双头、依赖及执行函数不变；保留504操作（84旧frame+420新操作）及原命令链。CMake和未执行sanitizer代码差异不属于wire直接编译闭包。

StageI standalone storage2与TSan15的运行源码和F不变，但旧CMake/接口/runner有实际改造；TSan runtime按62项记录核对，固定P2996 profile与SDK26.5仍分别保留。映射把这些active build入口差异列成需要只读证据/父会话审查的字段，不凭完整CMake文本变化命令全量重跑，也不冒称当前入口已用同一字节执行。可组合StageIII当前storage运行子集与StageII独立storage package的相关路径证据，由父会话决定是否已覆盖交付主张。

Linux旧普通full46的相关C++输入未变，当前注册新增allocator_model/failures两项。只有父会话明确需要“当前普通Linux48”主张时，这2项才是实际未执行的配置增量；现有StageIII sanitizer2不能改名为普通2。旧sanitizer26与StageIII新增2的精确case并集为当前28；runtime清单相同，但CMake3.28.3/4.2.3及旧接口差异分别记录，不能写成一次新28项通过。先接受逐case身份映射，不默认重跑基础26/28。

原始toolchain.json的platform默认值有陷阱：native-full/TSan/storage可能也写linux/amd64。本文以argv mode、host和compiler Target判断平台。包manifest的sha字符串与普通validator的{bytes,sha}已归一化；StageIII tar的source/包装和StageI flat归档也已处理，逐源文件核对原始归档。

旧证据没有每次执行时的可执行文件SHA和完整SDK字节seal。已保存source manifest、归档validator SHA、实际configure/build/test argv与退出码、CTest/LastTest/JUnit来源链；读取到的旧build文件或当前编译器hash只是事后观察，不能伪装成当时seal。字段缺口由已有归档补证/父会话接受明确profile范围处理，不自动重复昂贵矩阵。

新增W1网络/W2诊断/W3测量源码与基础F矩阵分别验收。W1已确认P2996 ASan v8与Apple clang_2100 runtime ABI不匹配；保留不可用前提，不重复同组合。若父会话需要新增网络源码的sanitizer证据，唯一候选是明确批准后的W1具体新源码/34组182子例sanitizer范围，兼容Linux工具链/构建契约和daemon适配器均另有前提；不能据此重跑基础F全量或引入未登记TSan变体。

当前无条件新基础C++任务为0。先冻结final surface，继承相关闭包不变的证据，补字段/审查旧入口差异；随后只派确有生产/消费者新代码或明确缺失配置的最小增量。Docker维持设计储备，不实现、不启动。

若父会话最终要求普通Linux48而确无新增2项普通配置证据，最小增量参数为 `--suite full --sanitizer none --targets test_allocator_model,test_allocator_failures`；其他46项继承，不以sanitizer2冒充普通2。当前不执行，不建立Docker。

统筹已开始W3独占计时限制；此后不再进行归档/外部文件批量哈希或身份扫描。余下effective flags/outer receipt等字段仅在窗口结束且父会话认为有必要时补既有只读证据，不能解释为待重跑任务。

## 已完成的有界参数补证

Linux旧SAN26实际为19个目标。逐目标flags.make/link.txt与StageIII已生成的当前SAN配置对照：17个full目标仅多前置`-std=gnu++26`，最后仍为`-std=c++26`；2个OS目标只多`-I/src`，源码明确引用`../xoffset_virtual_memory.hpp`，其本地闭包不变。所有19个link只多CMake的`-Xlinker --dependency-file=...`元数据输出选项。宏、最终语言、sanitizer/resource flags、库和RPATH未发现差异；62/369 runtime清单沿用前轮已核对记录，没有重扫。26+2可作为明确的继承逐case并集28，绝非一次新28项运行；当前旧19目标的StageIII metadata生成不等于它们在那里编译或运行。

TSan8个目标的其余基础参数相同，但有真实deployment差异：旧构建明确`-mmacosx-version-min=10.15`，当前native基线cache为空。旧CMake用CACHE FORCE设置，当前改为非FORCE时既有空cache未被改写。不能把该flag归入无关元数据，不能声称当前TSan已经生成或运行。父会话可保留原10.15 profile及其15项证据；若另选profile则单独裁决，不自动创造TSan新变体或15项重跑。

旧standalone storage为Apple21 C++17；当前完整矩阵storage子目标为P2996，分别保留。自有inventory的storage/package-storage模板已绑定/usr/bin/clang++，package-storage移除非历史的/usr/local库覆盖，避免无必要的profile变化。当前接口新增的源根-I对显式../header不改变本地解析；没有当前Apple standalone Debug新跑或其生成metadata的主张。

Linux package outer绑定由独立审查提供，未重复核验：full container-input SHA6509d40194dfdb77484d9b0d6e366a1f034f447bfc77aca2e6435fe28d2d2d53，linux/amd64/P2996、CMake4.2.3；storage SHA353e370a3e73a43f52b5e5b0769a05e2288e00a9dc525481aaa0dbf1de9c84bd，linux/arm64/GNU13.3、CMake3.28.3、固定local image。两者索引匹配，12项JUnit无失败/跳过；storage预期配置rc1保留。ARM64 storage不是amd64仿真，也不是独立设备性能证据。

额外I/O暂停已解除，本轮便宜元数据补证已完成，没有尚未执行的廉价扫描项。剩余是原始seal不可补造、TSan最终profile选择，以及G4/新角色最终冻结；它们不自动产生新运行任务。无条件新基础C++任务仍为0，Docker仅设计储备。

## 最终父会话裁决与尚缺字段

父会话已接受基础身份映射作为G4继承依据；完整48、wire504、原生各package6不重跑，无条件基础C++新任务仍为0。standalone storage保留实际Apple C++17测试profile；TSan15保留实际macOS26.6.2 arm64执行profile。TSan的“10.15”仅指记录的`-mmacosx-version-min=10.15`编译选项，绝不表示在macOS10.15运行通过，也未核对驱动调整后的Mach-O有效最低系统版本。

历史TSan profile与当前minimum未设置的default profile不能称为相同flags。仅当最终主张坚持后者时，才由父会话选择这一配置身份/必要运行范围；这不是新mandatory矩阵、不是自动15项重跑，也不改CMake或尝试重建ABI。原始二进制/编译器/完整SDKseal仍不可凭空补造。

W3的default-aa-1-acceptance已按小JSON读取并记录hash：一次实际F/F共78进程（6warm、72measured），接受执行与证据链但噪声不合格；4指标及anchors未达门槛，没有重试、D/F或消融。N3仍开放，网络线独立继续。G4继续等待W2网络诊断实现与W3网络入口/最终冻结身份，不把基础F继承证据用于新增实验源码。

Linux package外层补证最终为30个非archive文件索引一致、0失配；12项JUnit通过，storage预期full-component拒绝rc1单列。引用独立审查的linux-package-provenance.json，未重复扫描其归档。
