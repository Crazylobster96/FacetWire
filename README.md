# FacetWire

**English** | [简体中文](README.zh-CN.md)

**Write one rendering plugin, connect it everywhere.**

FacetWire is a portable plugin contract and runtime for agent-native,
cross-platform rich-media rendering. It defines a stable C ABI between a host
and independently developed capabilities such as text layout, images, video,
subtitles, charts, controls, document pagination, and format parsers.

> Project status: **0.1 bootstrap / experimental**. The ABI and the Agent Scene
> Package Directory Profile are intentionally small and are not stable until
> their 1.0 conformance suites are published.

## Design goals

- One source implementation across Windows, Linux, macOS, iOS, and Android.
- The same logical plugin contract for dynamic, static, process-isolated,
  remote, and future WebAssembly transports.
- A host-owned memory and rendering model that does not leak platform objects
  across the ABI.
- Open core with room for independently licensed proprietary format and codec
  plugins.
- Deterministic capability discovery, version negotiation, and diagnostics.

## Repository map

```text
include/facetwire/                 Public C ABI and runtime API
src/                               Portable runtime implementation
spec/                              Normative and experimental contracts
spec/schema/                       Machine-readable manifest/scene schemas
docs/                              Architecture and project policy
examples/hello_plugin/             Minimal statically registered plugin
examples/placeholder_demo/         Windows/macOS real-renderer demo
examples/documents/                Conforming uncompressed .agscene fixtures
plugins/text_renderer/             Reference Text Renderer 0.1
plugins/core_image_renderer/        Reference Image and Animated Image renderer
plugins/core_media_renderer/        Reference Audio and Video renderer
plugins/core_chart_renderer/        Reference Core Chart Renderer 0.3
plugins/hierarchical_chart_renderer/ Reference Hierarchical Chart Renderer 0.1
plugins/flow_layout/                Experimental Flow Layout 0.1 reference
spikes/playground_ui/               Shared Windows/macOS/iOS/Android demo host
plugins/placeholder_renderer/      Reference fallback renderer
tests/                             ABI and conformance smoke tests
```

## Build

FacetWire requires a C11 compiler and CMake 3.21 or newer.

```sh
cmake -S . -B build -DFACETWIRE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

If Ninja is installed, the supplied presets can be used:

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

To build and run the Windows/macOS demonstration, see
[`examples/placeholder_demo/README.md`](examples/placeholder_demo/README.md).
The Text/Image/GIF three-level recursive demo and its four-platform validation
matrix are documented in
[`docs/guides/core-content-renderers-demo-validation.zh-CN.md`](docs/guides/core-content-renderers-demo-validation.zh-CN.md).
### Current Flow Layout implementation

`org.facetwire.reference.flow-layout` now exposes the public C ABI, plugin manifest, deterministic Layout Plan, continuous, virtual-page, and multi-column block/inline flow, cross-region text and inline-object continuation, whole-object region advancement, four baseline modes, RTL placement, logical float-start/end, rectangular text exclusions, minimum-width clearing, bounded active floats, adjacent vertical margin collapse, anchored overlays, explicit breaks, bounded keep chains, keep-together, and plain-paragraph orphan/widow balancing. Constraint relaxations are reported through diagnostic flags instead of being hidden. The Playground exposes all three page modes and block/inline/float-start/float-end/overlay/pagination-constraint content through the native bridge. Inline paragraphs currently preserve atomic inline objects but report widow/orphan relaxation when exact line rollback cannot be delegated safely. See the [cross-platform Flow validation guide](docs/guides/flow-layout-cross-platform-validation.zh-CN.md) for device verification.


### Current Core Chart implementation

org.facetwire.reference.core-chart-renderer exposes
facetwire.renderer.chart.v1, facetwire.renderer.chart.elements.v1, and
facetwire.renderer.chart.presentation.v1 for
normalized category/series/value models,
bar/stacked/area/scatter/polar/statistical/financial/combo chart families,
diverging bars, faceted lines, range areas, density heatmaps, word clouds,
Nightingale roses, six themes, automatic layout, governed labels, shared
VisualTransform, opacity and transparent
uncovered regions, aggregate semantics, data-node hit testing, bounded
resources, and deterministic cache keys. CSV and Excel remain independent
future Data Source Adapters. The canonical Playground includes a real Native
Asset Chart verification page with 30 gallery choices and selectable title, axes, legend, series,
datum, and label elements plus opacity, color, translation, scale, rotation,
z-offset, and promotion controls. Legends use the standardized container → item →
marker/label/optional-value composition, so a whole legend item or an individual part
can be adjusted through the same element framework.

`org.facetwire.reference.hierarchical-chart-renderer` separately exposes
`facetwire.renderer.hierarchical-chart.v1` for Treemap, Sunburst, and Packed
Bubble using stable parent-first nodes. See the [0.3 requirements](docs/requirements/chart-visualization-expansion-requirements-v0.3.md),
[detailed design](docs/design/chart-visualization-expansion-detailed-design-v0.3.md),
the [Chinese user guide](docs/guides/chart-visualization-expansion-user-guide.zh-CN.md),
and [Chart Visual Design 0.4](docs/design/chart-visual-design-system-v0.4.md).

## ABI model

The host obtains a `fw_plugin_api_v1` table, validates its size and ABI
version, and then registers it with the runtime. A dynamically loaded plugin
exports the symbol `facetwire_plugin_query`. Restricted platforms may pass the
same query function directly to `fw_runtime_register_static`.
Desktop and controlled Android hosts may instead pass an already authorized
absolute library path to `fw_runtime_load_dynamic`. Core never scans plugin
directories or makes trust decisions. Capability providers can be enumerated
and selected deterministically before querying a versioned interface.

No allocation may be freed on the opposite side of the ABI. Strings are UTF-8
byte spans and are not assumed to be NUL-terminated. Every extensible structure
starts with `struct_size` and an ABI version.

See [the 0.1 plugin contract](spec/plugin-contract-v0.1.md), the experimental
[plugin manifest specification](spec/plugin-manifest-v0.1.zh-CN.md), the
[ASP Directory Profile](spec/agent-scene-package-directory-v0.1.md), the
[Core Content Profile](spec/core-content-profile-v0.1.zh-CN.md), and the
[VisualTransform specification](spec/visual-transform-v0.1.zh-CN.md), the
[Flow Content Profile](spec/flow-content-profile-v0.1.zh-CN.md), and the
[architecture overview](docs/architecture.md). The first content plugin is specified by the
[Text Renderer requirements](docs/requirements/text-renderer-requirements-v0.1.md) and
[function-level detailed design](docs/design/text-renderer-detailed-design-v0.1.md).
Flow layout is specified by its [requirements](docs/requirements/flow-layout-renderer-requirements-v0.1.md)
and [function-level design](docs/design/flow-layout-renderer-detailed-design-v0.1.md).
Core Chart is specified by its [requirements](docs/requirements/core-chart-renderer-requirements-v0.1.md)
and [function-level design](docs/design/core-chart-renderer-detailed-design-v0.1.md).
Chart element layering is specified by its
[0.2 requirements](docs/requirements/chart-element-layering-requirements-v0.2.md),
[detailed design](docs/design/chart-element-layering-detailed-design-v0.2.md), and
[Chinese user guide](docs/guides/chart-element-layering-user-guide.zh-CN.md). The reusable
legend sub-template is defined by the [Legend Composition Profile 0.1](spec/chart-legend-composition-profile-v0.1.md)
and its [Chinese edition](spec/chart-legend-composition-profile-v0.1.zh-CN.md).
Chart visualization 0.3 and the Hierarchical Chart Profile are specified by the
[expansion requirements](docs/requirements/chart-visualization-expansion-requirements-v0.3.md),
[detailed design](docs/design/chart-visualization-expansion-detailed-design-v0.3.md), and
[Chinese user guide](docs/guides/chart-visualization-expansion-user-guide.zh-CN.md).
The Playground, recursive fixture, and five-platform acceptance procedure are documented in
[the Chinese cross-platform validation guide](docs/guides/flow-layout-cross-platform-validation.zh-CN.md).
Audio and video rendering are specified by the
[Media Renderer requirements](docs/requirements/media-renderers-requirements-v0.1.md)
and [function-level design](docs/design/media-renderers-detailed-design-v0.1.md).
The shared Image/GIF/Video/future-Chart geometry contract is documented in the
[VisualTransform function-level design](docs/design/visual-transform-detailed-design-v0.1.md).
Long-lived technical decisions are indexed in the
[architecture decision records](docs/adr/README.md).

## Licensing

FacetWire is licensed under the [Mozilla Public License 2.0](LICENSE). MPL-2.0
keeps changes to covered source files public while permitting independent open
or proprietary plugins. Third-party formats, codecs, SDKs, fonts, and test
assets retain their own licenses and may require additional patent or vendor
permission. See [LICENSE_POLICY.md](LICENSE_POLICY.md).

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md), the
[Code of Conduct](CODE_OF_CONDUCT.md), and [SECURITY.md](SECURITY.md) before
opening a contribution or reporting a vulnerability.
