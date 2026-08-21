# FacetWire Architecture Decision Records

ADR 记录对实现有长期影响、但不应写入 FacetWire C ABI 或文件标准的技术决定。

| ADR | 标题 | 状态 | 下一门禁 |
| --- | --- | --- | --- |
| [0001](0001-cross-platform-ui-framework.md) | Playground 跨平台 UI 技术选型 | 有条件接受 Flutter | 五平台 UI Spike/P0 PoC |

状态含义：

- 提议：候选和约束尚未完成评审；
- 有条件接受：已有首选方案，但必须通过文中 Gate；
- 接受：Gate 通过，可以作为生产实现基线；
- 已取代：由后续 ADR 替换；
- 已拒绝：验证失败，不得作为生产基线。

## 本章检查

- 每个长期技术决定有独立状态和验证门禁；
- 索引不会把“有条件接受”显示成最终接受；
- ADR 只约束实现，不暗中修改公共标准。
