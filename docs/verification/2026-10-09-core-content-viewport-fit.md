# Core Content fit-to-viewport correction — 2026-10-09

Scope: canonical `examples/placeholder_demo` Core Content viewport only. No ABI,
ASP schema, Zone bounds, recursive placement, dependency, or saved-package change.
The historical `spikes/playground_ui` is not the consumer and is not changed.

## Reproduction and correction

A phone-size preview of a 640 × 1200 synthetic document shrank the logical root
layout to approximately 148.3 × 278, then applied a paint transform again.
Absolute-positioned bottom Zones were clipped against the shrunken layout.
Fixed 1:1 retained 640 × 1200. The old root-only test checked the outside preview
box but did not assert the logical canvas or bottom-Zone geometry.

Use `FittedBox` inside that same outside box. Its child lays out at the declared
logical dimensions and its paint/hit-test transform applies the presentation
scale once. `contain`, top-left alignment, original outside dimensions, actual-size
pan/zoom policy, recursive `fit=none`, and session opacity behavior remain intact.

## Automated evidence

- New regression: two tests, synthetic in-memory package, two instances of the
  same child document. Old implementation failed both original-layout assertions.
- After correction: both pass across 375 × 640, 800 × 600, 1400 × 900 and
  1600 × 1600 surfaces, including root/child layout sizes, fitted paint rectangle,
  bottom-Zone position and bounds, transformed click selection, and fit/1:1
  transitions. The click assertion checks the selected border directly, not a
  lazily constructed controls-list label; the first label-based assertion failed
  and was corrected without weakening the geometry checks.
- Full canonical Flutter suite: 38/38 passed in 16.736 seconds on macOS with
  Flutter 3.47.3 / Dart 3.13.3. Native CMake rebuild and CTest: 22/22 passed
  (1.797 seconds including rebuild, CTest 0.36 seconds).
- First analyze found an unnecessary test import; it was removed and analyze
  rerun: no issues, exit 0 in 7.273 seconds (analyzer 6.4 seconds).

The standalone demo's first offline pub preparation resolved seven transitive
versions against the already prepared SDK/cache (clock, glob, jni_flutter,
pub_semver, stack_trace, vector_math, yaml). The resulting lockfile edits were
reverted exactly; no dependency changes are part of this patch. Tests used that
SDK-compatible local resolution, not a claim that the original standalone lock
was enforced. The consuming Pillow viewer retains its separate existing lock.

## Manual evidence and limits

The downstream iPhone screenshot established the original tiny-canvas failure.
Corrected macOS/iOS builds and actual phone visual acceptance are downstream
checks, not claimed by the Widget tests. Windows, Android, accessibility, full
media, and whole-module branch coverage are not newly certified by this fix.
