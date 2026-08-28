# Mac Codex：FacetWire 拉取、跨平台验证与 iOS 真机测试 Prompt

将下面整段复制给 Mac 上的新 Codex 项目。开始前把目标仓库目录、Apple Team ID
等占位值按需补充；如果暂时没有 Team ID，可让 Codex 先完成无签名和模拟器测试。

~~~text
你正在 macOS 上验证 FacetWire。仓库地址是：
https://github.com/Crazylobster96/FacetWire.git

目标：
1. 拉取 origin/main 的最新提交并记录精确 commit。
2. 使用仓库锁定的 Flutter SDK，不使用系统 PATH 中未验证的 Flutter。
3. 完成 C/C++ 运行时、Flutter、macOS、iOS Simulator 测试。
4. 在我连接的 iPhone/iPad 上完成真机启动和人工验收。
5. 在签名条件允许时生成 development IPA，并给出 SHA-256。
6. 记录全部证据；不能把签名材料、账号信息或大二进制提交到仓库。

工作规则：
- 先读取 README.md、CONTRIBUTING.md、toolchains.lock.json、
  docs/adr/0001-cross-platform-ui-framework.md、
  docs/verification/ui-spike/summary.md 和仓库中的 AGENTS.md（如果存在）。
- 保留用户已有修改；不要 reset --hard，不要删除未知文件。
- 不执行 flutter upgrade，不切换 Flutter channel，不擅自更新锁文件或依赖。
- 所有 Flutter 命令使用
  "$FACETWIRE_FLUTTER_ROOT/bin/flutter" 的绝对路径。
- 不为通过测试而降低签名、安全、编译器或测试约束。
- 遇到需要接受 Apple 协议、登录 Apple ID、选择 Team、信任设备或开启
  Developer Mode 时，明确告诉我并等待我操作；不要索取密码或私钥。
- 只修复明显的脚本/平台生成问题。若需要改产品逻辑，先报告根因和建议，不要
  未经确认扩大修改范围。

步骤 A：准备仓库
- 如果仓库不存在，执行：
  git clone https://github.com/Crazylobster96/FacetWire.git
- 进入仓库，运行 git status --short、git remote -v、git fetch origin。
- 工作树干净时运行 git switch main 和 git pull --ff-only origin main。
- 若工作树不干净，先报告文件，不覆盖它们。
- 记录 git rev-parse HEAD。

步骤 B：准备固定 Flutter SDK
- 检查完整 Xcode：xcodebuild -version 和 xcode-select -p。
- 检查 cmake --version。若缺失，先向我说明并在我同意后使用 Homebrew 安装
  CMake；不要在缺少原生测试工具时跳过 CTest。
- 运行：
  chmod +x scripts/bootstrap-flutter-macos.sh scripts/validate-mobile-macos.sh
  FACETWIRE_USE_CHINA_MIRROR=1 ./scripts/bootstrap-flutter-macos.sh
  source "$HOME/.config/facetwire/flutter.env"
- 如果当前网络访问官方存储稳定，可以不设置中国镜像。
- 对比 Flutter SDK HEAD 与 toolchains.lock.json 的 flutter.frameworkCommit，
  必须完全相同。
- 保存 flutter doctor -v、xcodebuild -version、sw_vers 和 uname -m 的输出。
  报告中可以保留版本，但删除用户名、Apple ID 或不必要的个人路径。

步骤 C：自动化测试
- 运行 ./scripts/validate-mobile-macos.sh。
- 它应依次完成：
  1) 根 FacetWire CMake（启用统一 Playground Bridge）配置、编译和 CTest；
  2) flutter pub get；
  3) flutter analyze；
  4) flutter test；
  5) flutter build macos --debug；
  6) flutter build ios --simulator --debug。
- 任一步失败时，记录完整命令、退出码、首个根因错误和相关版本。区分代码失败、
  网络失败、工具缺失和 Apple 签名失败。

步骤 D：macOS 与 iOS Simulator 人工验收
- 启动 macOS Debug App，确认窗口打开、无崩溃。
- 启动一个可用 iOS Simulator，并运行：
  cd examples/placeholder_demo
  "$FACETWIRE_FLUTTER_ROOT/bin/flutter" devices
  "$FACETWIRE_FLUTTER_ROOT/bin/flutter" run -d <simulator-id>
- 对 macOS 和 iOS Simulator 分别检查：
  1) FacetWire Playground 标题和应用名正确；
  2) 画布、占位区域、图形边界和文字可见；
  3) 不透明度滑块可拖动，0% 和 100% 语义与显示一致；
  4) 窗口或方向变化后布局不溢出、不崩溃；
  5) 系统字体放大后仍可操作；
  6) VoiceOver 和语义树能识别关键控件；
  7) 前后台切换和重新启动后无异常。
- 截图只保存在本地 dist/evidence，除非我明确要求提交。

步骤 E：iOS 真机
- 让我连接并解锁设备、信任 Mac、开启 Developer Mode。
- 运行 "$FACETWIRE_FLUTTER_ROOT/bin/flutter" devices，列出设备名称和 ID。
- 打开 examples/placeholder_demo/ios/Runner.xcworkspace。
- 如果 Signing & Capabilities 尚未配置，让我选择 Apple Development Team。
- 默认 Bundle ID 是 org.facetwire.facetwirePlaygroundUiSpike；若冲突，让我提供
  一个唯一 Bundle ID。
- Team、Bundle ID 或 provisioning 的本地改动不得连同个人信息提交。测试结束前
  用 git diff 明确列出它们。
- 执行：
  "$FACETWIRE_FLUTTER_ROOT/bin/flutter" run -d <physical-device-id>
- 在真机重复步骤 D 的人工验收，并额外检查触摸拖动、横竖屏、系统文字大小、
  退到后台再恢复、冷启动。

步骤 F：development IPA
- 只有 Xcode 已配置有效 Apple Development 证书和 development provisioning
  profile 时才执行：
  "$FACETWIRE_FLUTTER_ROOT/bin/flutter" build ipa --release --export-method development
- 把生成的 IPA 复制到 dist/ios，运行 shasum -a 256。
- 使用 codesign 检查 app 签名，并记录 TeamIdentifier、Bundle Identifier、
  版本和支持架构；不要把完整 profile 或账号内容写进报告。
- 如果免费 Personal Team 不允许导出 IPA，但 flutter run 已能安装真机，标记：
  真机运行通过、IPA 导出因账号能力未满足而未执行或失败。不要绕过签名。

步骤 G：报告
- 新建 docs/verification/macos-ios/YYYY-MM-DD.md，至少包含：
  仓库 commit、macOS/CPU、Xcode、Flutter/Dart、设备/iOS 版本、每条命令结果、
  自动测试计数、人工用例通过/失败/阻塞、产物路径/大小/SHA-256、已知限制。
- 同步更新 docs/verification/ui-spike/FacetWire-UI-Spike-Test-Matrix.xlsx 中能可靠
  更新的状态；若当前工具不能安全修改 xlsx，就只更新 Markdown 并说明。
- 运行 git status --short 和 git diff --check。
- 只提交报告和必要的可移植脚本修复。不要提交 dist、build、.dart_tool、证书、
  provisioning profile、keychain 内容、Apple ID、设备 UDID 或本地绝对路径。
- 提交前向我汇报测试结论、阻塞项和待提交文件；得到确认后再提交并推送。

最终回复必须包含：
- 实际测试的 Git commit；
- 自动测试通过或失败数量；
- macOS、iOS Simulator、iOS 真机各自状态；
- IPA 是否生成；若没有，说明准确原因；
- 每个交付产物的绝对路径、字节数和 SHA-256；
- 报告文件路径；
- 未解决问题和建议下一步。
~~~

## Prompt 完备性检查

- 区分了模拟器构建、真机运行和可导出 IPA 三种不同成功标准。
- 工具链版本、签名授权、敏感信息和 Git 变更边界均有明确约束。
- 自动化测试与视觉、可访问性人工测试均被覆盖。
- Personal Team 无法导出 IPA 时有真实、可审计的阻塞状态，不会假报成功。
