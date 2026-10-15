# Core Content image alternative text bridge

Date: 2026-10-15. Base: `e77f7a4a5160a81fb3d90310eea380b77f982d4c`.

## Scope

The Flutter Core Content host did not forward an image Zone's existing `alt`
string to `Image.asset` or `Image.file`. Its accessibility tree exposed an image
role and a type/ID label but not the supplied description. This affects the
shared static/animated-image UI path, not the native image C ABI.

The minimal correction forwards a string as `semanticLabel`. Missing or
non-string values retain the existing unlabelled-image fallback; an empty
string does not invent a description. The Zone type/ID label remains. This does
not broaden the content schema, modify resources, change selection, or alter
the native renderer contract. A non-string diagnostic input is not a claim that
it conforms to the package schema.

## Automated evidence

`core_content_image_semantics_test.dart` covers iOS/macOS platform variants,
static/animated-image types, asset/file sources, and nonempty UTF-8, missing,
empty, and non-string `alt`: 32 cases. It checks the Image node's role and
description separately from the containing Zone's type/ID label, verifies
image-to-selectable-text reselection, and checks local PNG bytes remain equal.
The animated-image cases use an original synthetic PNG to exercise this shared
path; they do not certify GIF playback or animation semantics.

- Before correction: 24 fallback cases passed; eight nonempty descriptions
  failed with a null Image semantic label.
- After correction: all 32 cases passed.
- Complete Flutter suite: 78/78; `flutter analyze --no-pub`: no issues.
- Existing native CMake build and CTest: 22/22.

Validation used Flutter 3.47.3/Dart 3.13.3 and the existing SDK-compatible offline
resolution. Tracked dependency locks were not changed; this is not a claim that
the older standalone lock/toolchain was forcibly re-resolved and certified.

Initial test-only failures were retained: a missing `dart:ui` import, a semantics
handle disposed after the binding check, an assumption that the labelled Image
and outer Zone remained merged, and two analyzer notices. The fixture was
corrected to dispose before the binding check and inspect both actual nodes;
the description/role/selection expectations and input cases were not removed.

## Manual validation still required

Rebuild consumers; already running windows do not update automatically. With a
screen reader, verify the image's supplied description and image role, readable
root text, focus traversal, and activation. Verify both local-file and bundled
consumers as applicable. Widget semantics are not a VoiceOver/device result.
Light/dark appearance, maximum accessibility text size, transparency reduction,
and complete multi-platform accessibility acceptance remain separate checks.
