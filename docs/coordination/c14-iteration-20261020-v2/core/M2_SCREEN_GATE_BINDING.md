# M2 screen源码/codegen/安全依据的有限绑定

本文件与`M2_SCREEN_GATE_BINDING.json`仅收束既有独立证据，供root以后真正的typed gates审阅。不是完整功能接受、动态机制通过、screen freeze、候选保留或Q/F准入。runtime、lifecycle、M2动态机制及最终全功能/工程接受均保持pending；不借R1门禁，不预判M2。新compile/native/queue/body读取均0，quiet优先，完成后idle。

## 精确独立源码与既有裸codegen

M2为C14独立派生`m2-exact-owns-v1`，154成员/14生产文件，仅main一处branch hunk改变，不含R1/M1。main SHA `3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f`；patch SHA `c5c3d7bf6e1993fae1bb129b58e918fa3f9bfa9777f17e1b668a966cf56b7497`。JSON绑定build身份及core身份、源review/root source接受、原请求/admission/worker/result和完整154/14 map。既有codegen/native worker的source154 before/after相同，且等于该候选map。

原candidate-only裸`-O3 -DNDEBUG -S`实际编译1次、native0，产物SHA `5551c11a4e7b4ae379e61bb499047795021f2bf9f3f7e12dc7e6b0a33b69da0f`。原`M2_CODEGEN_RESULT.json`只确认compile通过，其static_mechanism_pass=false是当时“等待独立审查”的范围；最终静态4→1依据是另份已passed的`M2_CODEGEN_MECHANISM_REVIEW.json`，不改写旧结果。复用的C14 control asm SHA `110bc1bc494a1d731c563660d5760a630d5bb0da39531eb6e43e83bd04479640`虽位于历史m1-v2目录，但不使用M1候选功能结果。

原独立实际汇编审查已经逐路径确认：有效非空Attr vector、非零descriptor、capacity containment miss时，control父data/canonical调用50761/50785/50795/50804，candidate父调用50760；即父节点4→1，子key/value递归解析不在此削减内。capacity hit/null/size-miss次序、32B Attr遍历和early true、双向assignment调用及其短路均保留。canonical helper与Attr子visitor实际文本相同，helper之外汇编相同。原审查记录两侧均6 pushes、local frame56B；candidate counted-loop test+LEA、result-address LEA与消除target stack reload的成本互换仍待测，不从局部调用数或文本尺寸推速度。本次只核原小回执及asm身份，不重编、不追符号。

旧61B缺口保留：独立审查明确M2旧admission没有显式捕获outer61B launcher的before/after。当前61B/c8ea06…与历史记录相符，不补造旧观察，不用后续full/business/机制的显式pin追溯修补。原实际compile、asm和源审查按其已记录范围复用；未来构建/筛选必须显式绑定outer及实际转发链。

## 安全证明的范围

原`REVIEW_M2.md`含实际patch与fixture附录，无静态阻断；root已有`M2_SOURCE_ACCEPTANCE.json`。唯一变化是非string owning vector先保存本节点`value.data()`，以原capacity字节范围作containment，hit即返回；仅miss且nontrivial时读size，`last=elements ? elements+count : nullptr`，按原顺序递归。没有nullptr+0、用capacity遍历live元素、predicate结果缓存、跨public调用指针缓存或同arena/epoch阴性捷径。

库内置owning-head的const data/size、builtin containment和closed C14 canonical恢复，在新捕获点到旧begin/end捕获点之间不新增用户callback、分配或退役。原canonical冷路径不安装TLS或驱逐terminal record，不因这一local捕获新增长寿命owner/record lease。原range-for已经在进入child递归前保存begin/end并跨递归使用；新代码没有延长相对原实现的父迭代器有效期。需要的同步owner/live descriptor/array前提不变，不扩大为无同步owner销毁或任意未知操作后的pointer缓存。

递归仍为原unqualified `move_assignment_owns_address(child,address)`，可能经ADL进入用户同名重载；不能因noexcept宣称所有generic实例都无callback。该表达式、const lvalue类型、子访问/early positive顺序、array/reflection/base/member/string/unknown分支，以及`!owns(destination,&source)`再`!owns(source,&destination)`短路次序均未改。外部擅自特化内部owning trait或破坏正在遍历父对象的有效性，不获得新保证。原C14“无内部cold eviction callback”的事实不能泛化成L1式跨PMR/validator callback缓存安全。

无新增field/layout/wire/schema/arena/host cache/owner维护/分配/free/销毁操作，也未改变完整fresh owning输出。表示与生命周期的静态证明可供root三gate中的safety依据；真实SAN/failpoint/回调/完整闭包仍按各自支持范围补充，不能称ISO全证明。

## 已有实际结果与仍pending的闭包

既有M2定向4normal+4SAN实际8次编译/8次native通过，source154 before/after相同；按原请求覆盖same-arena/nested ownership、locator/容量与退役、canonical/owner及focused fixture的各自实际范围，不能把单fixture描述成全部callback矩阵。已有M2 full normal73/SAN73实际结果、业务80+80及V1 main84+extras8/AB4/C4结果另列精确refs；业务fresh编码真值与固定旧wire机制/筛选并非同一运行，绝不借前者伪称后者已通过。

runtime/lifecycle/最终功能source/receipt闭包以及root独立接受仍为pending。在当前原始结果继续落盘期间，单个result出现不自动将本绑定的pending改成passed。需要验证会话最终实际closure和root接受记录，不能借R1 runtime/lifecycle或M1 full填空。工程typed gate、实际M2裸screen profile、完整process/资源/cost投影及独立freeze均未由此建立。

## 共同C14控制与动态机制仍待观察

原C草案在运行前预声明唯一C14为M2共同控制，实际C14 binary/compile/observer/AUDIT及最终共同hook/driver/channel的身份已绑定。C14控制mutate中owns320465、vectors154575、capacity_hits0、nontrivial_misses28652、nonempty_misses16131、data_reads228010，eligible集中在第7修改阶段。这只是控制侧真实机会；不是M2命中或速度。

`DYNAMIC_ACCEPTANCE.md`与既有offline checker定义主阶段及九修改阶段守恒，stage1=M1-P2；同scope owns/vector/branch计数相同，D=control data_reads-candidate data_reads、K=D-2*N，要求E≤K≤N且canonical减少=3*K，主mutate E/D/canonical减少均正，并先通过同口径event/exit/deadline/truth/资源/cleanup审计。M2自己的一个候选观察仍pending；禁止重跑控制、拼接不同hook/driver/profile或以静态4→1代替它。

原条件草案、core精确桥接与validation当前机制freeze（只是被引用的冻结准备）都保留其真实状态；以validation最终实际独立准入为准，不执行core旧提案、不改变已收束预算。全部动态/全功能/工程缺项和最终screen worker/host/resource闭合后，root才可审阅真正三typedgate及唯一72配对freeze。M2主mutate、原四主/36辅助族/df71、任何metric undefined阻止晋升、辅助lower>1否决、无retry/补样/前缀收益都保留。M2与R1不自动组合，正式性能尤其owning emit≤0.20仍未证明。
