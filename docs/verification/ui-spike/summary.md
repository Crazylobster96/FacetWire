# Playground UI Spike verification summary

> Date: 2026-08-21
> Host: Windows x64
> Decision: ADR-0001 remains **conditionally accepted**

## Results

| Scope | Result | Evidence |
|---|---:|---|
| Existing FacetWire native baseline | PASS | CTest: 2/2 passed before Spike implementation |
| Playground UI Spike native bridge | PASS | MSVC 19.41.34120, CMake 3.29.5, Release, CTest 1/1 passed |
| Native invalid input and output clearing | PASS | `facetwire_ui_spike_test` assertions |
| Deterministic display-list batch v1 | PASS | byte-for-byte repeat assertion |
| Opacity boundary 0.0 / 1.0 | PASS | encoded-alpha assertions |
| Native ownership / repeated release | PASS | release-twice and 1,000 render/release cycles |
| Semantics payload | PASS | role and label assertions |
| Dart unit and Flutter widget tests | BLOCKED | Flutter/Dart SDK unavailable on this host |
| Windows Flutter integration/package | NOT RUN | Requires pinned Flutter 3.47.1 installation |
| macOS, iOS, Linux, Android gates | NOT RUN | Requires corresponding hosts/devices and assistive technologies |

## What this proves

The C boundary can return a versioned, bounded, deterministic display-list
batch and a separate semantics payload without leaking allocator ownership.
The repository also contains a strict Dart decoder, an injectable runtime port,
a `CustomPainter` consumer, and widget/unit tests ready for the pinned SDK.

It does **not** yet prove Flutter performance, worker-isolate behavior, platform
packaging, golden consistency, or accessibility on the five target platforms.
Those remain explicit acceptance gates in the test workbook.

## Reproduce the completed native gate

```powershell
cmake -S spikes/playground_ui/native -B build/playground-ui-native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/playground-ui-native
ctest --test-dir build/playground-ui-native --output-on-failure
```

See `FacetWire-UI-Spike-Test-Matrix.xlsx` for the full manual/automated test
inventory, status definitions, evidence fields, and platform environment log.
