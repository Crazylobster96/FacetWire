# FacetWire SpatialSurface 0.1 规范草案

## 1. 目的

`SpatialSurface` 用于把已有的二维 FacetWire Canvas 放置到空间展示环境中。它是可选宿主能力，不改变被引用 Canvas 的内容和二维坐标系。

## 2. 标准结构

```json
{
  "schema": "org.facetwire.spatial-surface",
  "version": "0.1",
  "id": "main-surface",
  "contentRef": "#page-1",
  "presentation": "volume",
  "physicalSizeMeters": [1.0, 0.625],
  "depthMeters": 0.08,
  "transform": {
    "positionMeters": [0.0, 1.25, -1.2],
    "rotationDegrees": [0.0, 0.0, 0.0],
    "scale": [1.0, 1.0, 1.0]
  },
  "interaction": {
    "focus": true,
    "indirectPinch": true,
    "directTouch": false
  },
  "fallback": {
    "presentation": "window",
    "fit": "contain"
  }
}
```

## 3. 字段约束

| 字段 | 类型 | 约束 |
| --- | --- | --- |
| `schema` | string | 必须为 `org.facetwire.spatial-surface` |
| `version` | string | 本草案必须为 `0.1` |
| `id` | string | 非空，并且在文档内唯一 |
| `contentRef` | string | 必须解析到二维 Canvas、Page 或 Zone |
| `presentation` | enum | `window`、`volume` 或 `immersive` |
| `physicalSizeMeters` | 两个数字 | 均为有限数且大于 0，依次表示宽和高 |
| `depthMeters` | number | 有限且不小于 0；二维窗口可忽略 |
| `transform.positionMeters` | 三个数字 | 世界空间中的 x/y/z，均为有限数 |
| `transform.rotationDegrees` | 三个数字 | pitch/yaw/roll，均为有限数 |
| `transform.scale` | 三个数字 | 均为有限数且大于 0 |
| `fallback.presentation` | enum | 0.1 版本必须为 `window` |
| `fallback.fit` | enum | `contain`、`cover` 或 `fill`；默认 `contain` |

编辑器必须保留未知字段。遇到未知枚举值时，宿主不得猜测含义；必须使用 fallback，或者输出可诊断的 placeholder。

## 4. 坐标与尺寸

被引用内容继续使用自身的二维逻辑单位。宿主按照 `fallback.fit` 把 Canvas 边界映射到 `physicalSizeMeters`。文本应先完成二维排版，再进行空间放置。宿主可以因无障碍设置增加物理尺寸，但除非文档明确允许 reflow，否则必须保持宽高比。

坐标约定：x 正方向向右，y 正方向向上，初始观察者原点前方为 z 负方向。旋转语义依次为 yaw、pitch、roll。最终 Transform 由宿主决定，因为 visionOS 可能出于安全和舒适性调整内容位置。

## 5. 能力协商

初始能力标识如下：

- `facetwire.spatial.surface.v1`
- `facetwire.spatial.volume.v1`
- `facetwire.spatial.immersive.v1`
- `facetwire.input.gaze.v1`
- `facetwire.input.indirect-pinch.v1`

宿主缺少请求的空间展示能力时，必须按照 `fallback` 展示二维内容，不能只因为不支持空间放置就丢弃内容。

## 6. 输入语义

`focus` 表示内容可参与宿主焦点系统；`indirectPinch` 表示可接收标准化选择/激活动作；`directTouch` 只声明内容是否接受近距离直接触碰。插件接收标准化 Action Intent，不接收平台 Gesture 对象。

## 7. 安全与隐私

本描述不得请求原始眼动数据、摄像头画面、房间 Mesh 或手部骨骼。这些数据必须由独立、需要授权的能力和宿主策略管理。焦点和间接捏合事件只暴露内容执行动作所需的标准化信息。

## 8. 降级与错误

- `contentRef` 无法解析：显示带诊断信息的 placeholder。
- `volume`/`immersive` 不受支持：使用 `fallback.presentation=window`。
- 物理尺寸或 Transform 含 NaN/Infinity：拒绝空间描述，但仍尝试显示二维内容。
- 未知字段：保留并忽略，不影响已知字段解析。
- 未知必需语义：不得猜测，使用 fallback。

## 9. 完备性检查

草案已经定义标识、内容引用、展示形态、物理尺寸、Transform、输入意图、降级、未知字段处理、能力协商和隐私边界。空间音频、沉浸环境组合和立体视频明确延后，不隐式塞入二维 Zone 属性。
