# Selectable text Zone re-selection — 2026-10-09

Scope: canonical Core Content Flutter host, not the C ABI or ASP schema. No
dependency, package byte, bounds, persistent edit, or native renderer change.

## Failure and correction

Downstream macOS manual acceptance selected a text Zone, then an image Zone.
Clicking the actual selectable text afterwards left the selected type as image.
`SelectableText` owns a tap recognizer, so the enclosing Zone's GestureDetector
does not receive that same successful gesture. Its internal `onTap` had not been
connected to the Zone selection callback.

Pass that same callback to `SelectableText.onTap`. Keep the selectable text,
read-only text selection, copy behavior and outer empty-area selection intact.
Repeated selection of one Zone is idempotent, not a deselection toggle. No
pointer-down interception or opaque input overlay is introduced.

## Automated evidence

- New in-memory synthetic fixture: root text, two instances of one shared child
  text document, and an original synthetic 16x16 PNG. No external resource or
  real conversation is used.
- Eight cases: macOS/mouse and iOS/touch platform variants, fit and fixed 1:1,
  selectable and plain text. Each clicks real text glyphs after selecting the
  image, checks both the inspector and exact border, repeats root selection and
  selects each shared child independently.
- Clean original-code baseline: four plain-text cases passed, all four
  selectable cases failed re-selection (7.069 seconds, exit 1). Two earlier
  test-fixture iterations miscounted inspector SelectableText widgets and used
  platform cleanup too late for the test binding invariant; their failures were
  retained, then fixed without weakening the selection assertions.
- One callback correction: eight cases passed (6.581 seconds). The completed
  cases also verify read-only/interactive selection remains enabled, select-all
  selects the exact text, and copy sends that exact text to an in-test clipboard
  mock; the operating-system clipboard is not changed. These API checks do not
  claim actual native copy-menu or accessibility acceptance.
- Format + completed eight-case suite + analyze: exit 0, 14.383 seconds, no
  analysis issues (analyzer 5.7 seconds).
- Full canonical Flutter suite: 46/46, 15.096 seconds, exit 0.
- Native CMake rebuild and CTest: 22/22, 1.461 seconds including rebuild,
  CTest 0.32 seconds, exit 0.

Tests reuse the already prepared Flutter 3.47.3 / Dart 3.13.3 local resolution.
As recorded in the preceding viewport correction, the standalone demo's tracked
lock is unchanged, while its local SDK-compatible resolution differs in seven
transitives. No dependency update is delivered here.

## Limits

The macOS failure is manual evidence; the iOS platform-variant Widget failure is
not a report of actual iPhone behavior. Corrected downstream Mac/iPhone builds
and manual re-selection are separate acceptance steps. Windows, Android, full
accessibility and whole-module branch coverage are not newly certified.
