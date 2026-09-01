# FacetWire architecture

**English** | [简体中文](architecture.zh-CN.md)

## 1. Scope

FacetWire Core is the plugin boundary beneath an Agent Scene Package renderer. The
Core ABI does not depend on a complete scene document, layout engine, or graphics API.
The repository separately defines an experimental ASP Directory Profile so hosts and
renderers can share deterministic fixtures without coupling that format to the ABI.
It defines how a host discovers capabilities, negotiates a compatible contract,
provides constrained services, and manages plugin lifetime across platforms.

### Section check

- The project boundary separates the plugin mechanism from the scene format.
- Rendering, parsing, interaction, and transport can evolve independently.
- No platform-specific object is required by the common contract.

## 2. Layer model

```text
Application / Agent
        |
ASP scene and layout model
        |
FacetWire capability routing
        |
+-------+-----------+-------------+----------------+
| renderer plugin  | parser      | interaction    |
| text/image/video | plugin      | plugin         |
+-------+-----------+-------------+----------------+
        |
Host services and normalized DisplayList (planned)
        |
Skia / Metal / Direct2D / Vulkan / WebGPU / platform UI
```

A capability is smaller than a plugin. One plugin may expose multiple
capabilities, and a host selects them by stable identifiers and policy.

### Section check

- Plugins are deployment units; capabilities are selection units.
- Graphics backends remain below the normalized host-service boundary.
- The architecture supports both display and non-display capabilities.

## 3. Deployment profiles

All profiles use the same logical descriptor and lifecycle:

| Profile | Connection | Typical platforms |
| --- | --- | --- |
| Static | query function registered by the app | iOS, embedded, all platforms |
| Native dynamic | exported query symbol in DLL/SO/dylib | desktop, controlled Android |
| Process isolated | serialized adapter around the contract | untrusted parsers |
| WebAssembly | generated adapter and constrained imports | portable sandbox |
| Remote | versioned RPC adapter | services and heavyweight renderers |

The 0.1 implementation contains static registration, explicit native dynamic
loading on supported platforms, deterministic capability selection, and
versioned interface lookup. Process, Wasm, and remote transports remain future
adapters and must preserve the same capability semantics.

### Section check

- Restricted platforms do not depend on downloading executable code.
- Dynamic loading remains available where platform policy permits it.
- Isolation changes transport and trust, not the plugin's conceptual API.

## 4. ABI invariants

1. The boundary is C ABI, even if either side is implemented in another
   language.
2. Every extensible function table and descriptor begins with `struct_size`.
3. ABI major versions must match; a host may accept an older minor version.
4. Strings are UTF-8 `(pointer, length)` views.
5. Ownership stays with the allocator. Returned tables and descriptors are
   immutable plugin-owned data valid until unload.
6. Calls are synchronous unless an interface explicitly defines async behavior.
7. Plugins receive only declared host services; they do not infer ambient
   filesystem, network, graphics, or UI authority.

### Section check

- The rules avoid C++ ABI, allocator, exception, and standard-library coupling.
- Size-gated tables allow additive evolution.
- Capability-based host services support sandboxed and remote implementations.

## 5. Planned capability families

- `facetwire.renderer.*`: text, image, animation, video, chart, controls.
- `facetwire.parser.*`: ASP and optional third-party document formats.
- `facetwire.layout.*`: flow, pagination, anchoring, responsive variants.
- `facetwire.interaction.*`: input, scrolling, dragging, commands.
- `facetwire.transport.*`: dynamic, IPC, Wasm, and remote adapters.
- `facetwire.export.*`: flattening, rasterization, document export.

Each family will define a versioned function table in a separate header. A
plugin discovers optional host services and exposes optional interfaces through
`query_interface`.

### Section check

- Families cover the previously identified media and static-document use cases.
- New families can be added without modifying the base lifecycle ABI.
- Unsupported content can retain layout through future placeholder metadata.

## 6. Security boundary

An in-process native plugin has the host process's effective privileges. The ABI
does not sandbox it. Untrusted and proprietary parsers should support a
process-isolated profile with bounded input, memory, CPU, recursion, and output.
Manifests declare permissions and dependency/license metadata, while the host
remains the policy authority. Core accepts only an explicit absolute dynamic
library path and never performs discovery, signature verification, or trust
decisions.

### Section check

- Capability declarations are not confused with actual operating-system
  isolation.
- Parser threat models include malformed documents and resource exhaustion.
- License metadata is part of distribution policy, not runtime authorization.

## 7. Evolution sequence

1. **Implemented:** lifecycle, discovery, diagnostics, and static registration.
2. **Implemented:** manifest schema, exact-path native dynamic loader, and
   deterministic capability routing.
3. **Implemented:** DisplayList, shared Semantics, host-service contracts, and
   the Placeholder reference capability.
4. **Implemented:** Core Content for text, image, animated image, video, and
   audio, with Placeholder, Text, Core Image, and Core Media reference plugins.
5. **Partially implemented:** Flow Layout 0.1 now has its public ABI, manifest,
   deterministic Layout Plan, and continuous, virtual-pages, and columns
   block/inline/float slices, including cross-region text/object continuation,
   atomic replacement objects, four baseline modes, RTL and logical float
   placement, rectangular exclusions, minimum-width clearing, and whole-object
   region advancement. The next gate is overlay and pagination controls.
6. Define and implement Subtitle/Cue rendering and Media Controls/Interaction.
7. Define structured data sources, starting with CSV and adding Excel through
   a separate adapter.
8. **Partially implemented:** Core Chart Renderer 0.3 now accepts a
   host-normalized category/series/compound-value model and supports
   cartesian, polar, statistical, financial, mixed-series and six extended
   chart families, plus themes, auto layout, label governance, VisualTransform,
   transparency, semantics, and hit testing. Chart
   Element Layering adds stable element identities, presentation overrides,
   and on-demand promotion intent. The separate Hierarchical Chart 0.1 profile
   implements Treemap, Sunburst, and Packed Bubble. CSV/Excel adapters, dual
   axes, maps, flame graphs, and Forge persistence remain pending.
9. Define a common Graph Model with import adapters for mind maps, Visio, Edraw,
   and similar formats.
10. Implement the professional Image Composition Profile after data, chart,
    and graph capabilities are stable.
11. Graduate the validated spike into the production FacetWire Playground and
    add conformance certification.
12. Then add ASP/third-party parsers, process isolation, Wasm, and bindings.

This order deliberately moves structured data, charts, and graph formats ahead
of professional image composition. CSV/Excel remain data sources, charts remain
view models, and Visio/mind maps remain graph inputs rather than one renderer.

### Section check

- Each phase produces a usable and testable increment.
- Higher-risk loading and rendering features build on a stable base.
- Cross-platform consistency is verified by contracts and fixtures, not names
  alone.

## 8. Reference host and UI technology boundary

FacetWire Core does not select an application UI framework. The current Playground reference host
has a conditional implementation decision in
[ADR-0001](adr/0001-cross-platform-ui-framework.md): Flutter/Dart is the preferred candidate,
subject to a five-platform UI Spike. Qt Quick is the first fallback.

The following boundary is invariant regardless of the Playground framework:

```text
UI framework presentation
        |
typed application port
        |
Playground C Bridge
        |
FacetWire Runtime / C ABI plugins
        |
versioned DisplayList + Semantics + HitRegion
```

A plugin cannot return UI controls, framework objects, platform views, or callbacks into the UI
thread. Replacing the Playground UI may replace the presentation, binding, and player adapters; it
must not require a plugin ABI or scene-format change.

### Section check

- The reference application decision is linked but not promoted into the core standard.
- Flutter, Qt, Avalonia, and non-visual hosts can consume the same C ABI contracts.
- Failure of the UI Spike has a bounded replacement surface and preserves core conformance tests.
