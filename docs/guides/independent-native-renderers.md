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
then verifies ABI, descriptor identity, every declared capability's kind and
flags, each v1 interface, unload and recovery. A manifest/descriptor mismatch
fails the package gate. This proves independent native loading, not a complete
UI renderer.
The current Flutter rich-content example still dispatches its built-in
Text/Image/Media/Chart widgets statically. A future host must explicitly map
the installed capability interface to its drawing, semantics and interaction
services. iOS/visionOS applications cannot load newly downloaded executable
native plugins after distribution; they register approved modules statically
in a new app build. Do not call either case silent hot installation.

## Flutter modules in a signed host build

The Playground's `CoreContentPackageLoader` and `CoreContentDemoScreen` also
accept an explicit list of `FacetWireZoneRenderer` modules. A module has its
own `type`, content/resource validator and Flutter widget builder. The loader
rejects unregistered types and runs the registered validator before exposing
the document; the same registry is passed into nested canvases. A synchronous
builder failure shows `Renderer unavailable` only in that zone and does not
change the saved source; this is not a sandbox for hostile or asynchronous
plugin code. Modules can
be delivered as separate Flutter packages and included/removed in a new host
build without editing the Playground's built-in type switch. An application
using a custom loader must pass the same modules to the screen. The module's
validator must check every content field and resource ID it consumes, because
the host intentionally does not guess a third-party content schema.

This is **build-time** composition, including on iOS/visionOS. It does not
interpret installed native DLLs as Flutter widgets or load downloaded Dart
code. Desktop runtime installation still needs a capability-to-widget host
bridge and explicit trust policy; until then the native ZIPs and Flutter
modules are separate delivery surfaces, not one end-to-end hot-install path.

## Bounded desktop data-only renderer packs

The Flutter example also exposes `DeclarativeRendererPack`, a separate,
runtime-loaded **data-only** profile for new zone types. A trusted desktop
installer pins the SHA-256 of an absolute regular JSON file before passing
it to `load(path, expectedSha256: digest)`. The loader accepts at most 64 KiB,
validates a small fixed view grammar (text, colored box, column), limits
depth/nodes and referenced content fields, and returns the same
`FacetWireZoneRenderer` module used by the recursive document loader. A
builder failure remains isolated to the affected zone. It cannot execute
native/Dart code, call tools or models, access the network, add interactions,
or replace built-in text/image/video/chart/document types. Mobile applications
do not use this runtime path; they must bundle modules in the signed app.

Example profile (the installer/host supplies the actual digest):

```json
{
  "schema": "facetwire.declarative-zone.v1",
  "type": "status-tile",
  "view": {
    "kind": "box", "colorField": "background",
    "child": {"kind": "text", "field": "label", "colorField": "foreground"}
  }
}
```

The zone content provides `label`, `background` and `foreground`; colors use
`#RRGGBBAA`. This is a constrained runtime extension point, **not** a bridge
that maps arbitrary installed native DLL Renderer APIs to Flutter drawing.
That broader native capability/semantics/interaction bridge remains open.
