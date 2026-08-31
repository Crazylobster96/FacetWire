# Flow logical-float incremental verification — 2026-08-31

## Scope and source state

This is an incremental verification in the existing Mac workspace. Prior macOS, iOS, visionOS, and Flow inline evidence was preserved. The scope is the latest `main` Flow Layout `float-start` / `float-end` implementation, the shared native bridge contract, Flutter UI, and the visionOS Host integration.

- Upstream and test base: `e2c7c8ea6a98441fea0816eff5fb50d21d5bd5b8` (`feat(flow): add logical float layout`)
- Verification branch: `codex/flow-float-apple-validation-20260831`
- Flutter framework commit: `6655482ec06e547f90abf8ae7590466f4415978d` (exact `toolchains.lock.json` match)
- The worktree was clean before `git fetch origin` and `git pull --ff-only origin main`.
- No Flutter upgrade, channel change, lock-file update, Runner regeneration, signing change, or dependency update was performed.
- No branch was pushed during this work.

## Environment

| Component | Version |
| --- | --- |
| macOS | 26.5.1 (25F80) |
| CPU | arm64 |
| Xcode | 26.6 (17F113) |
| CMake | 4.4.2 |
| Flutter | 3.47.1 |
| Dart | 3.13.1 |
| XcodeGen | 2.46.0 |
| visionOS Simulator SDK/runtime | 26.5 / 26.5 |
| iOS Simulator | iPhone 17 Pro Max, iOS 26.5 |

All Flutter commands used the repository-pinned SDK through `FACETWIRE_FLUTTER_ROOT`; the unverified system-PATH Flutter was not used.

## Contract verification

The native and Flutter tests, report inspection, and platform UI checks confirmed:

- `contentCase` 0–2 = block L1/L2/L3, 3–5 = inline L1/L2/L3, 6–8 = float-start L1/L2/L3, and 9–11 = float-end L1/L2/L3.
- `pageMode` 0/1/2 = continuous/virtual-pages/columns.
- Float reports contain the corresponding `placementMode`, `inlineObjects=false`, `composeStatus=0`, `complete=true`, and `fragmentCount=3`.
- The supported slice contains `float-start+float-end`.
- Level 3 float-start/end uses the same Placeholder source item and bounds as the block fallback.
- Existing block and inline behavior remains covered by the full regression suites.

## visionOS Host integration fix

The shared C Bridge already exposed the float contract, but the visionOS Swift host had not decoded `placementMode`, exposed float choices in its Picker, or asserted the float contract in XCTest. This made the latest native capability inaccessible and unverifiable in that host.

The minimal fix:

- decodes `placementMode` and adds `floatStart` / `floatEnd` paragraph modes;
- maps each paragraph mode to its three `contentCase` values without changing the C ABI;
- exposes Float start / Float end in the existing visionOS Picker and validates the native float report;
- adds XCTest coverage across all page modes and checks Level 3 Placeholder bounds.

Only `spikes/visionos_host/apple` Swift source and tests changed. No shared C/C++ Bridge, decoder, Flutter source, Xcode signing, bundle identifier, CocoaPods state, or product ABI changed.

## Automatic verification

| Command / suite | Exit | Result | Count or key evidence |
| --- | ---: | --- | --- |
| `./scripts/validate-mobile-macos.sh` | 0 | PASS | Root C/C++ configure/build, CTest, formal Flutter checks, macOS and iOS Simulator Debug builds |
| Root CTest | 0 | PASS | 14/14; native Flow contracts executed |
| Formal Playground `flutter analyze` | 0 | PASS | No issues |
| Formal Playground `flutter test` | 0 | PASS | 25/25; Native Assets path exercised |
| Formal Playground `flutter build macos --debug` | 0 | PASS | Debug app produced |
| Formal Playground `flutter build ios --simulator --debug` | 0 | PASS | Simulator app produced |
| Spike `flutter analyze` | 0 | PASS | No issues |
| Spike `flutter test` | 0 | PASS | 13/13 |
| Spike `flutter build macos --debug` | 0 | PASS | Debug app produced |
| Spike `flutter build ios --simulator --debug` | 0 | PASS | Simulator app produced |
| `./scripts/validate-visionos-spike-macos.sh` | 0 | PASS | XcodeGen, xrsimulator build, and XCTest |
| visionOS XCTest | 0 | PASS | 10/10, including both float placement tests |

Distinct test total: **62 passed, 0 failed** (14 CTest + 25 formal Flutter + 13 Spike Flutter + 10 visionOS XCTest).

The first post-fix validator attempt was blocked when the sandbox denied a write to the pinned Flutter cache (`Operation not permitted`) after CTest had already passed 14/14. It was rerun with the same pinned SDK under the permitted execution context and passed completely. This was an execution-environment failure, not a source, test, or toolchain failure.

## Manual verification matrix

### macOS formal Playground — PASS

All 18 Level × float placement × page-mode combinations were exercised in `Flow Layout 0.1 验证`.

| Check | Result |
| --- | --- |
| Level 1/2/3 × float-start/end × continuous | PASS — object appeared at logical start/end; following text wrapped on the remaining side without overlap |
| Level 1/2/3 × virtual-pages/columns | PASS — object advanced as an atomic item; no stale exclusion, blank loop, duplicate object, or crash observed |
| Level 3 Placeholder | PASS — same compact Placeholder type/bounds retained |
| Contract panel | PASS — Native/Complete/Balanced PASS, Status 0, 3 fragments; columns reported 2 columns |
| Repeated block/inline/float and page-mode switching | PASS — Level, source object type, and Plan Key did not drift |
| Recursive/single, fit/fixed 1:1, opacity endpoints | PASS — viewer changes did not reflow the plan; geometry remained stable |

### iOS Simulator formal Playground — PASS

The app was installed and launched on iPhone 17 Pro Max / iOS 26.5. Float-start/end and all three page modes were exercised across Levels 1–3 using simulator touch input.

| Check | Result |
| --- | --- |
| Responsive controls and scrolling | PASS — selectors remained reachable; no RenderFlex overflow, clipping, or crash |
| Logical float and page modes | PASS — start/end placement, wrapping, virtual pages, and columns matched the contract |
| Level 3 contract | PASS — Placeholder shown; Native/Complete/Balanced PASS, Status 0, 3 fragments, 2 columns, float slice present |
| Repeated mode, recursion, viewer, and opacity interaction | PASS — app remained responsive and selection state stayed independent |

### visionOS Simulator — PASS for build/XCTest; PARTIAL for manual viewport

- The Apple Vision Pro Simulator app installed and launched successfully.
- Indirect-click input selected Float start / Float end, Levels 1/2/3, and continuous / virtual pages / two columns; the host remained stable while switching states.
- The native float behavior is exercised by the actual 10/10 XCTest run, including `testFloatPlacementContractsAcrossPageModes` and `testFloatFallbackPreservesPlaceholderBounds`.
- The Flow canvas and green diagnostic are below the visible host viewport. Simulator scrolling gestures were consumed by the spatial camera rather than the SwiftUI `ScrollView`, so a reliable manual screenshot of `PASS · native <mode> · logical float/exclusion` could not be captured. This visual/input item is recorded as PARTIAL rather than inferred from XCTest.

### Vision Pro hardware — NOT RUN

No physical Vision Pro was available. Real gaze/hand tracking, indirect pinch behavior, device window/volume lifecycle, comfort, performance, and the physical-device float matrix remain NOT RUN. Simulator success is not treated as hardware success.

## Local evidence

Screenshots and logs are kept in the ignored directory `dist/evidence/2026-08-31-flow-float/` and are not committed. Principal files:

- `validate-mobile-after-fix.log`
- `spike-analyze-after-fix.log`
- `spike-test-after-fix.log`
- `spike-build-macos-after-fix.log`
- `spike-build-ios-simulator-after-fix.log`
- `validate-visionos-after-fix.log`
- `macos-level1-float-start-continuous.png`
- `macos-level1-float-end-continuous.png`
- `macos-level3-float-end-columns.png`
- `macos-level3-float-end-columns-single-fixed.png`
- `macos-opacity-0.png`
- `macos-recursive-opacity-restored.png`
- `ios-level1-float-start-continuous.png`
- `ios-level1-float-end-columns.png`
- `ios-level2-float-start-virtual.png`
- `ios-level2-float-end-columns.png`
- `ios-level3-controls.png`
- `visionos-app-ready.png`
- `visionos-float-start.png`
- `visionos-level1-float-end-columns.png`
- `visionos-level3-matrix.png`

## Conclusion and remaining work

The shared native float contract, formal and Spike Flutter suites, macOS and iOS Simulator builds, visionOS build, and all 62 automated tests passed. macOS and iOS Simulator manual matrices passed. The visionOS Simulator host launch and selector interaction passed, while the below-viewport Flow canvas/diagnostic screenshot remains a manual limitation caused by simulator input routing. Physical Vision Pro verification remains outstanding.

Because shared code was not modified, no additional targeted regression was required; nevertheless, the complete mobile validator, both Spike Flutter build targets, and the complete visionOS validator were run after the host fix. No binary evidence or sensitive signing/device data is included.
