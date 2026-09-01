# macOS / iOS / visionOS 图表可视化 0.3 增量验证提示词

将以下内容作为已有 macOS Codex 项目的新任务。该项目已经完成过 macOS、iOS、visionOS
基础验证；本次只验证 Chart Visualization 0.3 增量。

---

你正在已有的 FacetWire macOS Codex 项目中工作。请验证最新 Chart Visualization 0.3：
主题、自动布局、标签策略、对比柱图、分区折线、范围面积、密度热力图、词云、
南丁格尔玫瑰图，以及 Hierarchical Chart Profile 的 Treemap、Sunburst、Packed Bubble。
同时增量验证 Chart Legend Composition Profile 0.1：图例容器、图例项、色标、
标签和可选数值的稳定身份、父子关系、级联和独立调整。

安全规则：

1. 先执行 `pwd`、`git status --short --branch`、`git branch --show-current` 和
   `git log -1 --oneline`。工作树不干净时停止在 pull 前，完整列出修改和未跟踪文件；
   不要 reset、checkout、stash、clean、覆盖或删除任何用户改动。
2. 工作树干净后执行 `git fetch`，确认 main 只需 fast-forward 再 `git pull --ff-only`。
   若分叉则停止并报告，不要自行 rebase 或 merge。
3. 使用该项目已配置的 Flutter SDK；先用 `which flutter` 和 `flutter --version` 记录路径，
   不要改用系统中的另一个 Flutter。
4. 不修改图表数据、布局算法、主题颜色、标签规则或演示行为来“让测试通过”。发现平台问题
   时先定位到 C 插件、Native Asset、Dart 投影或平台工程中的具体层。

执行验证：

```bash
cmake -S . -B build/chart-v03-macos -DFACETWIRE_BUILD_TESTS=ON
cmake --build build/chart-v03-macos --config Release
ctest --test-dir build/chart-v03-macos -C Release --output-on-failure

cd examples/placeholder_demo
flutter pub get
flutter analyze
flutter test
flutter build macos --release
flutter build ios --release --no-codesign
```

visionOS 继续使用项目现有的 visionOS Spike/Runner 流程，不重新生成工程，不改 bundle ID
和签名设置。若已有真机或模拟器脚本，按仓库文档执行；否则在 Xcode 中以当前 Runner scheme
构建并启动，记录设备、系统版本与构建结果。

人工检查 Playground 的 Core Chart 页面：

- 原有 17 种图表仍能显示；新增 9 种 Gallery 类型均可显示。
- Theme 的 Auto、Light、Dark、Business、Academic、High Contrast 均有明显且可读的变化。
- Legend 的 Bottom、Right、Hidden 生效；Labels 的 Auto、All、Important、None 生效。
- 打开/关闭 Auto layout 后布局行为可辨认；缩放 macOS 窗口时无崩溃、无无限重排。
- 对比柱图以零线向两侧展开；分区折线为独立面板；范围面积有上下界带；密度热力图有连续密度；
  词云无明显重叠；玫瑰图为等角变半径。
- Treemap、Sunburst、Packed Bubble 形状正确，并能跟随主题变化。
- 调整任意普通图表元素的不透明度、位置、缩放或颜色后，当前主题、图例和标签策略不回退。
- Element Inspector 中可分别选择“图例容器、图例项、图例色标、图例标签”；
  角色与 canonical ID 在 macOS/iOS/visionOS 一致。
- 对一个 legend-item 平移、缩放、旋转或调透明度时，其 marker 和 label 保持组合关系；
  只改 legend-marker 颜色时，label 不变；改 legend-container 时所有图例项级联生效。
- 图例色标被实际绘制，不得把内部 marker tag 暴露到公开 categoryId。
- 所有图表未覆盖区域透明，`opacity=1` 完全不透明、`opacity=0` 完全透明。

输出一张中文结果表，至少含：平台、构建命令、自动测试、启动、主题、标签、9 种新增图表、
图例层级与 canonical ID、图例项级联、图例子部件独立调整、元素调整保持主题、
透明留白、结果/阻塞。失败时附第一条真实错误和最小复现命令。

只有为 Apple 平台兼容所必需且不改变既有行为的修改才允许提交。提交前重跑受影响测试，
展示 `git diff --check` 和 `git status --short`；不要推送，除非用户明确要求。平台生成或签名
文件必须单独说明，不得与功能代码混在一个提交中。

---

## 提示词检查

- 保护已有 macOS/iOS/visionOS 项目改动，不会以清理工作树为名丢失内容。
- 自动测试、三平台构建和人工视觉验收均覆盖本次新增能力。
- 验证失败不会通过改变跨平台语义来掩盖。
