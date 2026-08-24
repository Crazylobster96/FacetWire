# FacetWire

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
docs/                              Architecture and project policy
examples/hello_plugin/             Minimal statically registered plugin
examples/placeholder_demo/         Windows/macOS real-renderer demo
examples/documents/                Conforming uncompressed .agscene fixtures
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

## ABI model

The host obtains a `fw_plugin_api_v1` table, validates its size and ABI
version, and then registers it with the runtime. A dynamically loaded plugin
exports the symbol `facetwire_plugin_query`. Restricted platforms may pass the
same query function directly to `fw_runtime_register_static`.

No allocation may be freed on the opposite side of the ABI. Strings are UTF-8
byte spans and are not assumed to be NUL-terminated. Every extensible structure
starts with `struct_size` and an ABI version.

See [the 0.1 plugin contract](spec/plugin-contract-v0.1.md), the experimental
[ASP Directory Profile](spec/agent-scene-package-directory-v0.1.md), and the
[architecture overview](docs/architecture.md).

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
