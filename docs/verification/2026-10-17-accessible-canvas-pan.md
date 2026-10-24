# Accessible actual-size canvas pan

Date: 2026-10-17. Base: 72f2a3a3f1823f18fdab64c09fdf8c636439ffbc.

## Scope

The Core Content canvas used InteractiveViewer pointer/scale gestures without
directional semantics actions. A consumer reported that actual-size content
could not be panned while VoiceOver was active. This is a concrete canvas
capability gap, not evidence that every native screen in the consumer is broken.

The preview now owns one TransformationController shared by pointer and semantic
operations. The non-focusable viewport semantics container exposes available
scrollUp/Down/Left/Right actions in actual-size mode. Each action translates by
80% of the viewport, constrained by the existing 80-logical-pixel boundary
margin. Repeated/stale callbacks cannot cross the bound; fit mode and axes
without overflow advertise no directional movement. Changes to the shared
controller update the actions, including after ordinary pointer panning.

Flutter action directions describe finger movement. Up/left translate content
up/left; down/right translate it down/right. The pinned iOS engine maps UIKit
vertical accessibility directions to opposite Flutter action names, while
horizontal names map directly. These are not scrollbar-direction aliases.

The container has the host identifier facetwire.canvas.viewport, no label,
hint, tap or focus request. It retains explicit content children rather than
merging or excluding the document. Selection, opacity and mode rebuilds retain
the controller/reading position. Document geometry, resources, saved bytes,
image descriptions, diagnostic-badge policy, dependencies and native ABI are
unchanged. There is no forced announcement or system-setting change.

## Evidence and limitations

- Initial new tests failed because the new viewport interface was absent.
  The strengthened baseline first verified the synthetic canvas was loaded,
  then traversed the whole semantics tree: zero four-direction containers
  versus one required, on iOS and macOS variants. Both failed as expected.
- First analysis rejected deprecated pipelineOwner usage in the test. The test
  now uses the actual RenderView owner and node owner; no warning suppression.
- The initial 12 variants passed but used scrollbar-style direction assumptions.
  A differential inspector ListView test failed twice. Its semantic dump and
  the pinned SDK showed that forward vertical scrolling is scrollUp, not
  scrollDown. The implementation and movement assertions were corrected to
  the actual contract, not relaxed to accept either direction.
- Final 14 variants cover the actual control ListView versus the canvas,
  four directions, physical end visibility, finite bounds and stale callbacks,
  fit mode, small/horizontal-only/vertical-only canvases, ordinary pointer pan,
  and reading-position preservation. Complete independent Flutter suite:
  103/103, analysis no issues, 23.933 seconds. Native CMake/CTest: 22/22,
  1.282 seconds, no C source changes.
- Consumer full Flutter suite: 31/31 with published/draft host pan assertions;
  existing image regressions 37/37, analysis no issues, 25.864 seconds.
- An initial consumer UIKit bridge run exercised actual inspector native scroll
  and canvas vertical content-frame movement successfully. That complete run
  still failed a new, invalid native-Form diagnostic assumption and is not a
  green consumer gate. A plain UIKit control also returned false from direct
  NSObject accessibilityScroll calls while VoiceOver was off. Native Form
  structural checks and actual VoiceOver gestures must be reported separately.
- Actual iPhone three-finger acceptance remains open. The consumer user also
  reported no movement in the native consumer settings and Apple's Settings
  app with the same attempted gesture. This does not establish a consumer-wide
  code defect or a unique OS/gesture cause. No installation or system-setting
  workaround is claimed by these widget/bridge tests.

Final consumer native test build: 40.290 seconds. The same ordinary build passed
the complete Chinese 107/107 (42.230 seconds; XCTest 17.169 seconds) and English
107/107 (30.383 seconds; XCTest 10.217 seconds), with no skipped tests. The real
SwiftUI-embedded Flutter viewport accepted all four UIKit directions, moved
recursive-content frames in the expected directions and left inspector offset
unchanged. The empty viewport was not a native accessible focus stop; image
label/attributed label/role/language/activation and source PNG assertions stayed.

The original complete 106-case run failed eight assertions in the new Form
diagnostic (80.668 seconds), while the embedded canvas passed. The plain UIKit
callback control returned down=false/up=false/no movement (27.730 seconds),
consistent with the documented default NSObject method. The Form test was
explicitly re-scoped and renamed to structural/native-offset checks, with the
diagnostic results retained; it is not renamed into gesture acceptance. The
final test count adds the plain UIKit control, not a skipped failing product
contract. No consumer native Form scrolling implementation was replaced.

Signing and actual device observations are recorded separately. Do not infer
VoiceOver speech or system gesture acceptance from a programmatically
dispatched action. The user's phone has not been updated to this candidate.
