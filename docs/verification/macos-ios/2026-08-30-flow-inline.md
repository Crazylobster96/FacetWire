# Apple platform integration and Flow inline verification — 2026-08-30

## Scope and source state

This is an incremental verification on the existing Mac workspace. It preserves the prior macOS, iOS, and visionOS evidence and reconciles the already validated CocoaPods project state with the Flow Layout inline-object change.

- Upstream and test base: `be71d3b7fb34dd68c8711ebb1c3c83e32ef77e28` (`feat(flow): add inline object layout`)
- Backup branch: `codex/apple-platform-state-backup-20260830`
- Backup commit: `095d4cd09e44cd26a890bfbbdaea26a25b13558a`
- Integration branch: `codex/apple-flow-inline-integration`
- Flutter framework commit: `6655482ec06e547f90abf8ae7590466f4415978d` (exact `toolchains.lock.json` match)
- No branch was pushed during this work.

## Environment

| Component | Version |
| --- | --- |
| macOS | 26.5.1 (25F80) |
| CPU | arm64 |
| Xcode | 26.6 (17F113) |
| Flutter | 3.47.1 |
| Dart | 3.13.1 |
| CocoaPods | 1.17.0 |
| XcodeGen | 2.46.0 |
| visionOS Simulator SDK/runtime | 26.5 / 26.5 |
| iOS Simulator | iPhone 17 Pro Max, iOS 26.5 |

Flutter commands used the repository-pinned SDK through `FACETWIRE_FLUTTER_ROOT`; no system-PATH Flutter, Flutter upgrade, channel switch, dependency update, or lock-file upgrade was used.

## Apple project reconciliation

The original local changes were classified as follows:

| Original file group | Class | Resolution |
| --- | --- | --- |
| `examples/placeholder_demo/{ios,macos}/Flutter/*Debug*.xcconfig` and `*Release*.xcconfig` | A/B | Preserve Flutter generated-config inclusion and add the corresponding Pods target-support include. |
| `examples/placeholder_demo/{ios,macos}/Runner.xcodeproj/project.pbxproj` | A/B/D | Regenerate CocoaPods framework references and `[CP]` phases with `pod install`; preserve upstream Flutter/Native Assets project structure. |
| `examples/placeholder_demo/{ios,macos}/Runner.xcworkspace/contents.xcworkspacedata` | A/D | Regenerate the single Pods project reference; no duplicate workspace group. |
| `examples/placeholder_demo/{ios,macos}/Podfile` | A | Keep as the reproducible CocoaPods source configuration. |
| `examples/placeholder_demo/{ios,macos}/Podfile.lock` | A/D | Regenerate with `pod install`, never hand-merge or `pod update`; keep for reproducible application builds. |
| `spikes/playground_ui/ios/Runner.xcodeproj/project.pbxproj` (pre-existing local delta) | C/D | Preserve on the backup branch, but exclude local Team ID, `KnownAssetTags`, and Xcode object-version metadata from the public integration commit. Regenerate and retain only the required CocoaPods integration. |

No class-E (unexplained) change remained. The Spike also declares media plugins, so its iOS and macOS Podfiles, locks, xcconfig includes, workspace references, and generated `[CP]` project phases are required and are retained using the same policy as the formal Playground.

The generated `Pods/`, `.symlinks/`, Flutter ephemeral directories, `.dart_tool/`, `build/`, `DerivedData`, and `dist/` evidence are excluded. No certificate, provisioning profile, Apple account, device identifier, Team ID, private key, credential, or machine-local absolute path is included in the integration content.

### CocoaPods generation

| Command | Result |
| --- | --- |
| Formal Playground `flutter pub get` | PASS |
| Formal iOS `pod install` | PASS — 3 pods |
| Formal macOS `pod install` | PASS — 3 pods |
| Spike `flutter pub get` | PASS |
| Spike iOS `pod install` | PASS — 3 pods |
| Spike macOS `pod install` | PASS — 3 pods |

One initial `pod install` invocation was accidentally issued from the formal project root and exited with `No Podfile found`. This was an operator working-directory error, not a source or dependency failure; the command was immediately rerun from each `ios`/`macos` directory and passed. CocoaPods selected iOS 15.0 because the generated Podfile leaves the platform line commented. It also warned about custom Flutter base configurations and that the media plugins do not yet support Flutter Swift Package Manager. These warnings did not bypass or weaken any checks, and all four native builds passed.

## Automatic verification

| Command / suite | Result | Count or key evidence |
| --- | --- | --- |
| `./scripts/validate-mobile-macos.sh` | PASS | Root C/C++ build; CTest 14/14; formal analyze/test/builds |
| Root CTest | PASS | 14/14, including the real native Flow C test |
| Formal Playground `flutter analyze` | PASS | No issues |
| Formal Playground `flutter test` | PASS | 24/24; Native Assets bridge test executed without Dart fallback |
| Formal Playground `flutter build macos --debug` | PASS | Debug app produced |
| Formal Playground `flutter build ios --simulator --debug` | PASS | Simulator app produced |
| Spike `flutter analyze` | PASS | No issues |
| Spike `flutter test` | PASS | 12/12 |
| Spike `flutter build macos --debug` | PASS | Debug app produced |
| Spike `flutter build ios --simulator --debug` | PASS | Simulator app produced |
| `./scripts/validate-visionos-spike-macos.sh` | PASS | Generated project, built, and ran XCTest on Apple Vision Pro Simulator |
| visionOS XCTest | PASS | 8/8, including `testInlineObjectIsAtomicAndPreservesTextRanges` and `testInlineFallbackWorksAcrossColumns` |

Distinct test total: **58 passed, 0 failed** (14 CTest + 24 formal Flutter + 12 Spike Flutter + 8 visionOS XCTest).

The native contracts were exercised, not inferred: content cases 0–2 map to block L1/L2/L3, 3–5 to inline L1/L2/L3; page modes 0/1/2 map to continuous/virtual-pages/columns; the inline capability flag, `inlineObjects`, `textStart`/`textEnd`, supported-slice string, and byte ranges `0..7` / `7..21` are covered by native C tests or visionOS XCTest.

## Build-setting comparison

No intended public setting changed silently:

| Target | Bundle identifier | Deployment target | Signing / entitlements | Architectures |
| --- | --- | --- | --- | --- |
| Formal iOS | `org.facetwire.facetwirePlaygroundUiSpike` | iOS 15.0 | Automatic; no Team ID committed | simulator `arm64 x86_64`; `i386` excluded |
| Formal macOS | `org.facetwire.facetwirePlaceholderDemo` | macOS 12.0 | Automatic; existing debug entitlements retained | arm64 |
| Spike iOS | `org.facetwire.facetwirePlaygroundUiSpike` | iOS 15.0 | Automatic; local Team ID excluded from integration | simulator `arm64 x86_64`; `i386` excluded |
| Spike macOS | `org.facetwire.facetwirePlaygroundUiSpike` | macOS 12.0 | Automatic; existing debug entitlements retained | arm64 |

`INFOPLIST_FILE`, `CODE_SIGN_STYLE`, `CODE_SIGN_ENTITLEMENTS`, deployment targets, bundle identifiers, architectures, and excluded architectures match the intended upstream settings. The backup branch remains the recovery point for the prior machine-local signing selection.

## Manual verification

### macOS formal Playground

- App launched with the `FacetWire Playground` title, no crash, and the real C ABI indicator.
- Block/inline and page mode are independent selectors; changing either retained the selected Level.
- Block baseline retained text-object-text; Level 3 retained the block Placeholder.
- Inline continuous, virtual-pages, and columns each rendered an atomic text-object-text sequence exactly once. Level 3 used the compact Placeholder without an overflow marker.
- The contract panel reported Native PASS, Complete PASS, Balanced PASS, Status 0, 3 fragments; columns reported 2 columns.
- Switching to fixed 1:1 retained Plan Key `57bd556d823bfe7ee9fadc26db49d163`, confirming viewer-only scaling and no reflow.
- Opacity, recursive positioning, window resizing, and the existing Placeholder/media navigation remained responsive; CocoaPods and Native Assets integration produced no launch regression.

### iOS Simulator formal Playground

- App installed and launched on iPhone 17 Pro Max / iOS 26.5 without crash or responsive-layout overflow.
- Level 3 remained selected while block/inline and continuous/virtual-pages/columns were changed independently.
- Inline Level 3 showed the compact Placeholder in all three page modes. Columns showed 2 columns and the contract panel reported Native/Complete/Balanced PASS, Status 0, and 3 fragments.
- Touch-style scrolling and selector interaction remained usable; existing Placeholder and media plugin registration were present after CocoaPods regeneration.

### visionOS Simulator

- The two-dimensional host window launched and showed Placeholder PASS/native ABI, Level, three page-mode choices, and Block/Inline pickers.
- Gaze-hover plus indirect-click input actually selected Level 3, Two columns, and Inline; the resulting selected state was captured.
- `Open volume` opened the volume window. The window survived interaction and relaunch.
- The atomic inline behavior and green diagnostic are backed by the actual 8/8 XCTest run, including the two named inline/columns tests above.
- This is Simulator evidence only. Vision Pro hardware interaction, comfort, real eye/hand tracking, device lifecycle, and performance remain NOT RUN and must not be inferred as passing.

## Local evidence

Screenshots are deliberately kept under ignored `dist/evidence/2026-08-30-flow-inline/` and are not committed. Principal files:

- `macos-inline-contract.png`
- `macos-inline-columns-level3-contract.png`
- `macos-inline-columns-level3-fixed.png`
- `macos-inline-columns-level3-single-confirmed.png`
- `ios-inline-continuous-level3.png`
- `ios-inline-virtual-pages-level3.png`
- `ios-inline-columns-level3-contract.png`
- `visionos-app.png`
- `visionos-open-volume-ax.png`
- `visionos-inline-columns-level3-confirmed.png`

## Conclusion and limitations

The CocoaPods integration is reproducible and should remain under version control as Podfile + Podfile.lock + the Flutter/Xcode integration references. All automated checks and the targeted macOS/iOS/visionOS Simulator manual checks passed. No shared FacetWire C bridge, Flow implementation, decoder, or product logic was modified during reconciliation; only Apple application project integration and this evidence record are changed. No additional macOS/iOS regression beyond the complete mobile validator and explicit formal/Spike builds was required.

Vision Pro hardware verification remains outstanding. No push was performed.
