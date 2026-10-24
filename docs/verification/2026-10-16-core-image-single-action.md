# Described images: remove the redundant gesture semantics child

Date: 2026-10-16. Base: f4bb44bbf34f57346852b423856bf5c312c0c984.

## Finding and narrow correction

A consumer's synthetic saved package exposed a described image node with its
label, image role and tap action, plus a second blank tappable child. The prior
tests checked the description and action on one node, but not redundant child
actions. An iPhone user reported only the image role, then a sound-only swipe;
that feedback is not proof of the complete speech failure's cause.

The enclosing Semantics already owns the described built-in image's action.
Only its inner GestureDetector now excludes its own semantics when that image
has a nonempty string alternative. Pointer handling remains active. The image
description, role and semantic activation remain on the existing parent.
Decode-error text is retained and can merge into that parent's description;
it is not excluded. Custom renderers and missing/empty/non-string alternatives
keep their previous behavior. No content subtree is flattened or discarded.

No ASP/schema, saved document, native ABI, dependency or lockfile changes.

## Automated evidence

- Strengthened existing image matrix before product correction: 25 passed,
  12 failed (eight normal described images and four decode-error cases), each
  finding one duplicate action where zero was required.
- Complete Flutter analysis: no issues. Complete suite: 83/83.
  Existing pointer reselection, semantic activation, alt fallback, custom
  controls and decode-error readability remain checked.
- First full run after product correction: 79 passed/four failed because error
  text merged with the description, while a finder expected an exact standalone
  label. Updated that finder to check the retained description, error, role and
  action; did not remove the error or duplicate-action assertions. Full rerun
  passed, 21.717 seconds.
- Consumer's exact saved synthetic package, two viewport sizes: duplicate-action
  regression failed twice before correction (expected zero, actual one) and
  passed twice afterwards (zero). It does not run actual VoiceOver.
- Existing native CMake build/CTest: 22/22, 1.651 seconds; no C source changes.

Flutter 3.47.3/Dart 3.13.3 and the previously disclosed offline SDK-compatible
resolution were used. Tracked locks unchanged. PNG-based animated-image tests
do not certify GIF playback.

## Remaining validation

Consumer UIKit tests, complete iOS rebuilding and installation follow
separately. Actual iPhone speech and sequential navigation have not passed
this correction yet. Removing the reproducible blank action is not proof that
all image-description speech issues, platforms or accessibility requirements
are fixed. No automatic announcement or forced focus is introduced.
