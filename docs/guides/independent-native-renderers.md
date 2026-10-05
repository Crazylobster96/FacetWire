# Independently packaged native renderer libraries

FacetWire 0.1's C runtime already loads one explicitly authorized absolute
DLL/`.so`/`.dylib` and queries its capabilities. A host still owns manifest,
trust, permissions and renderer-specific UI integration.

Build with `FACETWIRE_BUILD_RENDERERS_SHARED=ON` to leave Core static while
building each reference renderer and Flow Layout as its own native library.
The default remains the existing static build. `FACETWIRE_BUILD_SHARED=ON`
continues to make Core and renderers shared as before.

For example, after configuring and building for `windows-x86_64`:

```text
python scripts/package-renderer.py \
  --manifest /absolute/FacetWire/plugins/text_renderer/facetwire.plugin.json \
  --library /absolute/build/bin/Release/facetwire_text_renderer.dll \
  --license /absolute/FacetWire/LICENSE \
  --target windows-x86_64 \
  --output /absolute/dist/facetwire_text_renderer.zip
```

Repeat with the matching reference manifest/library for Placeholder, Image,
Media, Chart, Hierarchical Chart or Flow Layout. Each archive contains one
native library, an unchanged plugin identity/capability declaration with a
`native-dynamic` artifact and its SHA-256, and the MPL-2.0 license. The tool
does not overwrite an output. The target is an operator assertion; perform
platform/architecture verification as part of release signing. Neither a hash
nor a successful load makes untrusted native code safe to execute.

The Windows conformance matrix stages **one DLL at a time in a Unicode path**,
then verifies ABI, descriptor identity, capability, v1 interface, unload and
recovery. This proves independent native loading, not a complete UI renderer.
The current Flutter rich-content example still dispatches its built-in
Text/Image/Media/Chart widgets statically. A future host must explicitly map
the installed capability interface to its drawing, semantics and interaction
services. iOS/visionOS applications cannot load newly downloaded executable
native plugins after distribution; they register approved modules statically
in a new app build. Do not call either case silent hot installation.
