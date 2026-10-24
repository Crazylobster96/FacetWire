# Described image accessibility and Zone activation

Date: 2026-10-16. Base: `e4410e00a6c28021df34249d2cce975322ac94b3`.

## Finding and limited correction

The previous alt bridge populated a labelled Image child while the enclosing
Zone retained a separate type/ID label and selection action. The supplied
description existed in Flutter semantics, but its node had no tap action.
An iPhone user reported hearing only the type/ID and moving to other controls
with the next swipe. This does not by itself prove where native traversal lost
the child, or establish a defect in the ASP document structure.

For built-in image and animated-image Zones with a nonempty string alt, the
existing Zone node now owns the description, image role and selection action.
The inner Image's redundant semantics and the diagnostic opacity badge are
excluded. Image decode-error text remains readable; the entire content subtree
is not discarded. Pointer selection is unchanged. Text, nested documents,
media controls and custom renderer semantics are not flattened. Missing, empty
and non-string alt retain their prior fallback; this change does not certify
all decorative-image behavior or broaden schema validity.

No document bytes, coordinates, renderer registration, native C ABI, dependencies
or platform objects in the shared contract change.

## Automated evidence

- New action assertion before correction: 24 fallback cases passed and all
  eight described-image cases failed because the description had no tap action.
- 32 platform/type/asset-file/alt cases after correction verify the supplied
  description, image role, one description node, no diagnostic label pollution,
  real semantics-action dispatch to Zone selection and pointer reselection.
- Four invalid-image cases keep decode failure and description accessible.
- One custom-image renderer case preserves its own description and button action.
- Complete Flutter suite: 83/83; analysis: no issues.
- Existing native CMake build/CTest: 22/22; no C source changes.

Flutter 3.47.3/Dart 3.13.3 and the previously disclosed offline SDK-compatible
resolution were used. Tracked dependency locks remain unchanged. Animated-image
cases use a synthetic PNG and do not certify GIF playback. Initial test-only
errors (missing action import, lint notices and inspection of a now-excluded
Image render node instead of the owning description node) were retained and
corrected without removing the description/action/error expectations.

## Native and manual boundaries

A consumer-side UIKit bridge test is being prepared separately. Its initial
attempt produced an empty native tree, so it is a readiness/harness failure,
not proof that only alt was dropped. Native bridge, touch exploration, sequential
VoiceOver traversal and actual speech on rebuilt consumers must be checked
separately. Prior Mac user feedback is not an iPhone result; Flutter platform
variants do not run the actual iOS accessibility bridge. This record is not
complete cross-platform accessibility acceptance.
