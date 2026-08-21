# FacetWire Playground UI Spike

This disposable spike validates the boundary selected by ADR-0001:

~~~text
FacetWire C ABI -> owned byte buffer -> Dart FFI decoder -> CustomPainter
                                              -> Flutter Semantics
~~~

It is deliberately not a production package. The native bridge and its CTest
suite can run without Flutter. Android, iOS, macOS, Windows, and Linux runner
sources are committed so every platform starts from the same reviewed project.
Generated SDK caches, local signing data, and build outputs remain untracked.

## Native verification

Run these commands from spikes/playground_ui:

~~~powershell
cmake -S native -B ../../build/playground-ui-native -G Ninja
cmake --build ../../build/playground-ui-native
ctest --test-dir ../../build/playground-ui-native --output-on-failure
~~~

## Flutter verification

Use the SDK pinned in ../../toolchains.lock.json, then run:

~~~powershell
flutter pub get
flutter analyze
flutter test
~~~

Build native first and copy or link the produced library beside the desktop
runner executable. Mobile integration must statically register the same C ABI.
Production work must move FFI calls to a worker isolate; this spike keeps the
decoder and client ports separate so that isolation can be measured without
changing the presentation API.

## Binary display-list batch v1

All integers and IEEE-754 floats are little-endian. The 12-byte header is
FWDL, uint16 version, uint16 headerSize, uint32 commandCount. Every
40-byte command contains uint8 opcode, three reserved bytes, then nine
float32 values: x, y, width, height, radius, r, g, b, a.

Unknown versions and opcodes fail closed. Buffers are owned by the native
library and must be released exactly through fwui_buffer_release.

## Mobile builds

Windows:

~~~powershell
powershell -ExecutionPolicy Bypass -File ..\..\scripts\build-android.ps1 -UseChinaMirror
~~~

For macOS and iOS, follow the Flutter local toolchain guide under docs/guides
from the repository root.
