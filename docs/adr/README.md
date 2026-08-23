# FacetWire Architecture Decision Records

ADR 记录对实现有长期影响、但不应隐式写入 FacetWire C ABI 或文件标准的技术决定。

| ADR | 标题 | 状态 | 下一门禁 |
| --- | --- | --- | --- |
| [0001](0001-cross-platform-ui-framework.md) | Playground 跨平台 UI 技术选型 | 有条件接受 Flutter | 生产性能与完整无障碍验证 |
| [0002](0002-visionos-host-strategy.md) | visionOS 原生宿主与空间展示降级 | 接受 | 生产宿主与签名发布流程 |
| [0003](0003-presentation-session-boundary.md) | Document、Presentation Session 与 Renderer Runtime 边界 | 接受 | Placeholder Renderer 合同与跨平台测试 |

状态含义：

- 提议：候选和约束尚未完成评审；
- 有条件接受：已有首选方案，但必须通过文中 Gate；
- 接受：Gate 通过，可以作为生产实现基线；
- 已取代：由后续 ADR 替换；
- 已拒绝：验证失败，不得作为生产基线。

## 本章检查

- 每个长期技术决定有独立状态和验证门禁；
- 索引不会把“有条件接受”显示成最终接受；
- ADR 约束实现边界；公共 ABI 的实际变化仍必须同步到接口与版本文档。
