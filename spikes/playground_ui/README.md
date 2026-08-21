# FacetWire Playground UI Spike

This disposable spike validates the boundary selected by ADR-0001:

```text
FacetWire C ABI -> owned byte buffer -> Dart FFI decoder -> CustomPainter
                                              -> Flutter Semantics
```

It is deliberately not a production package. The native bridge and its CTest
suite can run without Flutter. The Flutter project is source-complete but its
platform runner folders are generated locally so SDK artefacts are not copied
into the repository.

## Native verification

```powershell
cmake -S native -B ../../build/playground-ui-native -G Ninja
cmake --build ../../build/playground-ui-native
ctest --test-dir ../../build/playground-ui-native --output-on-failure
```

## Flutter verification

Use the SDK pinned in `../../toolchains.lock.json`, then run:

```powershell
flutter create . --project-name facetwire_playground_ui_spike --platforms windows,linux,macos,android,ios
flutter pub get
flutter analyze
flutter test
```

Build `native` first and copy/link the produced library beside the desktop
runner executable. Mobile integration must statically register the same C ABI.
Production work must move FFI calls to a worker isolate; this spike keeps the
decoder and client ports separate so that isolation can be measured without
changing the presentation API.

## Binary display-list batch v1

All integers and IEEE-754 floats are little-endian. The 12-byte header is
`FWDL`, `uint16 version`, `uint16 headerSize`, `uint32 commandCount`. Every
40-byte command contains `uint8 opcode`, three reserved bytes, then nine
`float32` values: `x, y, width, height, radius, r, g, b, a`.

Unknown versions and opcodes fail closed. Buffers are owned by the native
library and must be released exactly through `fwui_buffer_release`.
