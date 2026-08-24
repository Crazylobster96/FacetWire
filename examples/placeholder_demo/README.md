# FacetWire Placeholder Demo / 占位渲染演示

This is a shared Flutter desktop demo for Windows and macOS. It calls the real
FacetWire reference placeholder renderer through a narrow C bridge and loads a
conforming, uncompressed three-level `.agscene` directory fixture.

这是 Windows 与 macOS 共用的 Flutter 桌面 Demo。界面通过窄 C Bridge 调用真实
FacetWire `placeholder-renderer`，并加载符合 ASP Directory Profile 0.1 的未压缩、
未加密、三层递归 `.agscene` 示例目录。

## Demonstrated capabilities / 演示能力

- plugin lifecycle, descriptor, capability and interface discovery；
- `validate`, seven measure-source outcomes, normalized DisplayList recording；
- hidden/minimal/standard/diagnostic modes and six action intents；
- 12 fallback reasons, six Presentation Session phases, fraction progress and
  stale state；
- opacity multiplication, target font scale, dark/high-contrast/reduce-motion
  profile fields；
- Semantics and real hit testing；
- deterministic parent Zone → child Canvas composition across three recursively
  nested documents, preserving 1:1 intrinsic size by default and applying
  `contain`/`cover`/`fill` only when explicitly requested；
- Canvas/Page/Layer/Zone relationship-path inspection；
- renderer parameter schema inspection。

The current renderer contract accepts phase/progress/reduce-motion fields, but
not every accepted field has a dedicated visual animation yet. The report strip
shows the actual native contract result so accepted state is not confused with
an already implemented visual treatment.

当前 Renderer 合同已经接收 phase/progress/reduce-motion，但并非每个字段都已有专属
视觉动画。界面底部展示真实 Native 合同结果，以区分“合同已支持”和“视觉已实现”。

## Package fixture / 示例包

The source fixture is:

```text
examples/documents/recursive-placeholder-demo.agscene/
  recursive-placeholder-demo.agscene.dis.json       # depth 1
  resources/level-1.txt
  documents/level-2.agscene/
    level-2.agscene.dis.json                         # depth 2
    resources/level-2.txt
    documents/level-3.agscene/
      level-3.agscene.dis.json                       # depth 3
      resources/level-3.txt
```

The Demo bundles an identical copy under `assets/documents`. Flutter unit tests
load and procedurally validate all three descriptors, resources, path rules,
depth limits, IDs, and geometry.

## Windows build and run / Windows 构建运行

From the repository root:

```powershell
powershell -ExecutionPolicy Bypass `
  -File examples\placeholder_demo\scripts\build-windows.ps1
```

The script builds and tests the C bridge, runs `flutter analyze` and
`flutter test`, builds the Release app, and bundles the DLL beside the EXE. It
prints the final executable path.

脚本会依次构建/测试 C Bridge、执行 Flutter 静态分析与单元测试、构建 Release 应用，
最后把 DLL 放到 EXE 同目录并输出可执行文件路径。

## macOS build and run / macOS 构建运行

From the repository root on a Mac with the pinned Flutter SDK bootstrapped:

```bash
bash examples/placeholder_demo/scripts/build-macos.sh
```

The script builds/tests the same C sources, runs Flutter checks, builds the
Release `.app`, puts the dylib in `Contents/Frameworks`, and ad-hoc re-signs the
local test bundle. It does not configure Developer ID distribution signing.

脚本使用同一份 C/Dart 源码完成测试，把 dylib 放入 `.app/Contents/Frameworks`，并对
本地测试包进行 ad-hoc 重签名；它不会代替正式 Developer ID 分发签名配置。

## Manual acceptance / 手工验收

1. Scene tree shows `document Zone → child Canvas` at depths 1, 2, and 3; the
   two embedding Zones are not hidden.
2. The center preview composes all three Canvases at once. With no placement in
   the fixture, every child Canvas keeps 1:1 intrinsic logical size. The viewer
   starts at 100% and uses horizontal/vertical scrolling; it does not implicitly
   fit the root Canvas to the window.
3. Selecting image/chart/video Zones changes the complete relationship path,
   reason, label, and semantics role.
4. “不透明度” from 100% to 0% reveals the actual parent Canvas below the
   selected Zone; final background alpha equals background alpha × opacity.
5. Mode and size/font scale change visual density from none through actions.
6. Clicking the action rectangle reports the actual native Action Intent.
7. Measure scenario changes the returned source/size without changing Zone
   placement.
8. Parameter Schema opens from the app-bar `{}` button.
9. Dark/high-contrast/stale/phase changes are reflected by drawing or the native
   report strip as defined by the current implementation.

