# D023 工具独立审查（2026-10-16，Asia/Shanghai）

审查者：`/root/owner_review`。本文件由主代理据其只读结论整理；审查者未编辑文件、启动C++、基准或全量哈希。纯统计器与测试分别由独立子代理实施。

静态工具审查通过，无已知首次formal child前必失败、审批联结或receipt搬迁阻断。审查期间已修正：本地`artifact:`依赖不得作为外部路径读取；W4不支持archive kind，因此归档使用exact W3 validation；归档queue校验先于bulk读；子进程前后及窗口末复核binary；补fractions Python身份；固定native环境并拒绝loader override；approval/proposal SHA先于导入bundle工具；wait超时也记timed_out；唯一marker、样本文件集、receipt原路径与搬迁副本分别核验。

D027到达后，小范围只读复核实际proposal-v1与权威链：

- `producer_authority`将D027精确SHA、四项已审结果、D022 toolkit checker/contract/layout correction、原native source/binaries及26-case功能结果相互绑定。
- 实际checker来自tooling-errata-v2，SHA `2a380368741b1ecf0261c66d8d19ec463024befee9476e851339897fd1f1c83d`。build-v4里的旧checker仅为历史身份，不用于新的语义审计。
- 1,020项外部身份派生自接受的timing include closure、runtime、SDK、compiler；本地source/双头依赖分别绑定；冻结工具与当前源码一致。
- proposal SHA `8dde7bf6e5536704dc8921de4507564499e8a4e208a4fe0606fe599596268b9a`。
- producer binding SHA `6b3da0f9c127dfc7fcf58ad08a8262a7e1bee9d06d93dba4230c9ff082335b14`。

状态仍为unapproved_proposal、native_execution_authorized=false。正式approval、canonical window marker、window-v1和timing receipt均不存在；78-process正式窗口未批准、未执行。W2的准备/搬迁检查使用独立validation queue，不是formal window或额外native样本。

本审查仅限工具、绑定及现有准备产物。统计资格、正式窗口、完整window搬迁审计均须等父精确批准后由W3执行，不能从当前准备通过推断性能或采用结论。
