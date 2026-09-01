# FacetWire Chart Visual Design 0.4

**English** | [简体中文](chart-visual-design-system-v0.4.zh-CN.md)

Status: Draft implementation baseline
Scope: Core Chart Renderer, Hierarchical Chart Renderer and Playground Draw Sink
Compatibility: additive visual policy; no Chart ABI or persisted scene schema change

## 1. Purpose

Chart Visual Design 0.4 defines a consistent presentation system for every
FacetWire chart kind. It separates persistent chart meaning from host interaction:

- renderer themes own semantic colors, foreground, grid, spacing and mark density;
- the cross-platform Draw Sink owns device-pixel details such as rounded corners,
  smooth strokes and system sans-serif text rasterization;
- the interactive host owns hover/touch state and transient Tooltip surfaces.

This keeps the same chart file deterministic and portable while allowing a desktop
pointer, touch screen or spatial device to expose appropriate interaction.

Chapter check: the ownership boundary is explicit; no transient pointer state is
stored in an Agent Scene Package and no renderer ABI extension is required.

## 2. Design tokens

### 2.1 Color roles

| Token | Purpose | Business baseline |
|---|---|---|
| `chart.foreground` | Titles, axes and labels | deep gray `#344052` |
| `chart.grid` | Major reference lines only | cool gray at 30% alpha |
| `chart.series.primary` | Primary comparison series | technology blue |
| `chart.series.secondary` | Secondary comparison series | muted blue-green |
| `chart.accent` | Promoted or important datum | controlled bright orange |
| `chart.surface` | Playground preview only | cool off-white |

Light, Business and Academic themes use low-saturation technology-blue,
premium-gray and Morandi families. Dark uses softened accessible variants.
High Contrast remains intentionally stronger because accessibility takes priority
over the low-saturation aesthetic.

### 2.2 Geometry roles

- Cartesian plot: 12% left, 12% right, 16% top and 22% bottom reserve.
- Bar gap ratio: 0.32 by default.
- Bar corner radius: `min(6 device pixels, 16% of shortest side)`.
- Line width: 0.004 normalized units with round caps and joins.
- Area fill opacity: 0.22 by default.
- Tooltip radius: 12 device pixels; blur sigma 12; shadow blur 18.

Chapter check: every value has a normalized renderer meaning or a device-pixel
Draw Sink meaning, so scaling behavior is unambiguous.

## 3. Mark rules

### 3.1 Bars

Bar, horizontal bar, stacked bar, percent bar, histogram, waterfall and diverging
bar marks use a small radius. Rectangular heatmap cells and treemap nodes remain
square because rounding them would change adjacency and area perception.

### 3.2 Lines

Line, time-series, facet-line and combination line marks use a monotonic cubic
visual interpolation between emitted points. The source values and hit-test
anchors remain unchanged. Axes, grid lines and waterfall connectors remain linear.

### 3.3 Typography

Hosts use their modern system sans-serif family. Titles are semibold; labels are
regular; text uses theme deep gray rather than pure black except in High Contrast.

Chapter check: the rules are constrained by chart semantics, preventing decorative
rounding or smoothing from corrupting heatmap cells, axes or source data.

## 4. Layout and grid

The plot reserves more top and bottom whitespace than 0.3. Grid lines remain major
references only and are rendered at low alpha. Themes must not add opaque chart
backgrounds; uncovered pixels remain transparent under the FacetWire renderer
contract. The Playground surface is only a host preview surface.

Chapter check: renderer transparency and preview-surface color are explicitly
different responsibilities, avoiding a regression of the transparent-layer rule.

## 5. Tooltip state

Tooltip is transient host UI and is not a chart element persisted in a scene.
Pointer hover and touch tap use the rendered command geometry for hit testing.
Guide lines are excluded. The Tooltip shows the command label plus available series,
category and matching value-label information.

Visual state:

- dark-gray translucent surface;
- 12-pixel backdrop blur and subtle shadow;
- 12-pixel radius and a faint white border;
- white title and muted gray detail text;
- clamped to the chart viewport so it cannot pollute controls or sidebars.

Chapter check: desktop and touch activation are both supported; the Tooltip is
clipped to its viewport and does not alter deterministic render output.

## 6. Accessibility and fallbacks

- High Contrast preserves stronger colors and pure-black foreground where needed.
- Tooltip information must not be the only future access path; semantics and
  keyboard focus remain the authoritative non-pointer path.
- If backdrop blur is unavailable, the translucent dark surface remains readable.
- Small labels continue to respect the presentation collision and label policies.

Chapter check: visual polish does not replace semantics, contrast or non-pointer
access, and graceful degradation is defined.

## 7. Compatibility and extension

0.4 changes theme token values and host drawing policy only. Existing chart kind,
series, category, element ID, cache key inputs, element overrides and saved files
remain valid. A future style ABI may expose corner radius, interpolation and Tooltip
content policies, but it must default to this baseline and remain additive.

Final consistency check: palettes, geometry, mark rules, interaction ownership,
transparency and accessibility form one coherent stack. Hierarchical and Cartesian
charts share color roles while retaining different geometry. No rule conflicts with
VisualTransform, element-layer overrides, Flow Layout or future FacetWire-Forge
editing because this document governs presentation rather than source mutation.
