# Audio/Video Renderer 0.1 内存与资源生命周期审计

状态：**已完成（Windows 基线）**
范围：FacetWire Core、参考 Renderer、Native Bridge、Dart FFI 释放路径，以及新增 Core Media Renderer 0.1。

## 1. 检查对象

- C/C++ 中的 `malloc/calloc/realloc/free`；
- DLL/SO 句柄的 `LoadLibrary/FreeLibrary`、`dlopen/dlclose`；
- Text/Image/Media Service 的 acquire/release；
- Media Resource/Frame token 的 open/close、acquire/release；
- Dart FFI 的 `calloc.free`、Native Buffer release 和 Context close；
- 插件 load/unload、Runtime create/destroy 及失败路径。

## 2. 发现并修复的问题

### 2.1 插件失败加载返回部分 handle

不规范插件可能在 `load` 返回失败时仍写入非空 handle。原 Runtime 直接返回错误，部分资源会变得不可达。

修复：Runtime 在失败时发现非空 handle 且插件提供 `unload`，会执行防御性卸载。插件协议同时明确：插件必须在可失败初始化前把 `*out_handle` 设为 `NULL`，失败时保持为 `NULL`。

回归测试：故障插件分配一个 handle 后返回失败；测试断言 Runtime 调用一次 unload，插件计数不增加。

### 2.2 Native Bridge 覆盖未释放的输出 Buffer

Placeholder Demo Bridge 与 Playground Native Bridge 原先会在输出入口直接把 Buffer 指针清零。如果调用者忘记先 release 就复用同一 Buffer，旧地址会丢失。

修复：输出 Buffer 必须零初始化；非空 Buffer 会返回 `INVALID_ARGUMENT`，且原指针和长度保持不变，调用者仍可正常释放。公共头文件已记录此所有权规则。

回归测试：成功取得输出后不释放并再次调用，断言操作被拒绝、原 Buffer 未改变，然后正常 release。

## 3. Core Media Renderer 生命周期保证

- 插件自身只在 load 时分配一个 Context，并在 unload 时释放；
- validate、measure、render、build_semantics 和参数 Schema 查询不保留调用方指针；
- decoded-frame 严格执行 `open → acquire_frame → draw_frame → release_frame → close`；
- open、acquire 或 sink 任一阶段失败时，已取得的 token 都会配对释放；
- external-surface 不打开解码资源；poster-only 不打开主媒体；
- audio 不进入 decoded-frame 视觉路径；
- opacity 为 0 时不提交视觉命令，但保留 Session 和 Semantics。

## 4. 验证结果

| 检查 | 结果 |
| --- | --- |
| MSVC `/W4 /WX` 全量编译 | 通过 |
| 主原生测试（含 Media 与 CRT 生命周期） | 10/10 通过 |
| Placeholder Demo Bridge | 通过 |
| Playground Native Bridge | 通过 |
| MSVC AddressSanitizer 主项目与 Demo Bridge | 11/11 通过 |
| MSVC AddressSanitizer Playground Bridge | 1/1 通过 |
| MSVC Debug CRT 加载四个 Renderer 后堆快照 | 无残留分配 |
| Media open/acquire/draw 故障注入 | token 计数全部平衡 |
| 四个 Renderer Manifest 合同（ID/Capability/注册/权限） | 通过 |

Windows 的 AddressSanitizer 不支持 LeakSanitizer，因此泄漏检查由 Debug CRT 堆快照、静态生命周期审计和显式 acquire/release 计数共同完成；ASan 用于检测越界、释放后访问和重复释放。

## 5. 结论与后续门禁

当前受测代码中没有已知、可到达的未释放分配或媒体 token。第三方插件和平台播放器适配器仍必须独立通过同样的生命周期测试。Linux/macOS 后续应再使用 ASan+LeakSanitizer 或系统 Instruments/Valgrind 复验平台适配层；这不阻塞 Core Media Renderer 0.1 的公共 C 核心实现。
