# macOS / iOS / visionOS Flow Overlay 与分页约束增量验证 Prompt

请在现有 FacetWire Mac 工作区中验证最新 main 上的 Flow Layout 0.1 增量实现。不要重建
仓库、不要覆盖签名设置、不要删除 Podfile/lockfile 或 Xcode 工程变更。

开始前：

1. 运行 `git status --short --branch`。若有未提交改动，停止在 pull 前并列出文件。
2. 工作树干净时运行 `git pull --ff-only`；若分叉或冲突，停止，不要 reset/stash/checkout。
3. 记录待测 commit。

自动门禁：

1. 运行 `./scripts/validate-mobile-macos.sh`。
2. 运行 `./scripts/validate-visionos-spike-macos.sh`。
3. 确认根 CTest 中、新 Flow、Playground Bridge、Flutter analyze/test、macOS Debug、
   iOS Simulator Debug 与 visionOS XCTest 的逐 PASS 数量和失败详情。

手工验证：

1. 在 macOS 与 iOS App 打开“Flow Layout 0.1 验证”，确认 Native PASS、Complete PASS、
   Balanced PASS、Status 0。
2. 对 Level 1/2/3 选择“覆盖层”，分别切换连续、虚拟页、双栏。对象必须覆盖首段、
   z=10、与 anchor 位于同一 page/column，且不推动后一段；不得越界污染侧边栏。
3. 对 Level 1/2/3 选择“分页约束”。虚拟页中末段进入下一页，双栏中末段进入下一栏；
   continuous 不制造空白页但保留 break 标记。
4. 合同区应显示 Constraints exact；若显示 Relaxed，记录 diagnostic 值、Level、页面模式
   与截图，不能把放宽当作精确通过。
5. 回归 block、inline、float-start/end，确认几何、Placeholder 身份、透明度和递归三层
   行为未变化。
6. visionOS 原生区域至少验证 contentCase 12、15，并确认同一 C 实现输出相同 flags、
   z、pageIndex/columnIndex 和 plan 完整性。

完成后输出：commit、平台/设备/系统、自动测试摘要、逐项手工结果、截图路径、是否产生
本地平台文件改动。只有自动门禁与上述手工项全部通过时才标记 Apple 增量验证 PASS；
不要自行提交或推送，除非用户另行明确授权。
