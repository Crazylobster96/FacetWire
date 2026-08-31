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

`org.facetwire.reference.flow-layout` now exposes the public C ABI, plugin manifest, deterministic Layout Plan, continuous, virtual-page, and multi-column block/inline flow, cross-region text and inline-object continuation, whole-object region advancement, four baseline modes, RTL placement, logical float-start/end, rectangular text exclusions, minimum-width clearing, bounded active floats, adjacent vertical margin collapse, Text Fragment/Child Measure service boundaries, and the Page/Fragment sink. The Playground exposes all three page modes and block/inline/float-start/float-end content through the native bridge. Overlays and break/keep/widow/orphan controls remain pending; valid requests outside the implemented slice return `FW_STATUS_UNSUPPORTED` explicitly. See the dedicated [macOS/iOS/visionOS Float validation prompt](docs/prompts/macos-ios-visionos-flow-float-incremental-validation.md) for incremental Apple-platform verification.


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
