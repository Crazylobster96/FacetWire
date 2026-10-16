# Host-controlled diagnostic badges

Date: 2026-10-16. Base: 9a114af904d02515f604e150abe14afde2a8338a.

## Presentation boundary

An image's alternative text can be correct while an OS text-recognition feature
also reads visible debug text. Excluding the badge from the semantic tree does
not remove its painted pixels. A consumer requested a clean published reading
view while preserving the independent Demo and unsaved preview diagnostics.

CoreContentDemoScreen now accepts showDiagnosticBadges, default true. The host
can suppress only the Zone type/opacity and document depth/ID corner badges.
The setting follows recursive document rendering. It does not change borders,
selection, opacity, geometry, controls, content, resource loading or custom
renderer behavior. False omits those badge widgets rather than painting
transparent text. The default independent Demo remains unchanged.

This is an optional Flutter host presentation argument, not an ASP field or
native ABI change. No saved document, dependencies or lockfiles are changed.

## Evidence

- Consumer published/draft regression with the old host policy: two published
  variants failed because eleven badges were still present; two draft variants
  passed. Initial fixture compile/cleanup failures were retained separately.
- Six independent iOS/macOS widget variants cover default/explicit true/false,
  all three recursive document badges, absence of painted diagnostic Text,
  image description and pointer selection. Complete independent Flutter suite:
  89/89, analysis no issues, final 23.049 seconds.
- First analysis run found an unused test import. The next full run had five
  new fixture readiness failures (no loaded directory summary), with 84 cases
  passing. The same repository synthetic package through its file entry passed
  all six new cases; expectations and time budget were not removed or relaxed.
- Existing native CMake/CTest: 22/22, 1.038 seconds; no C source changed.
- Consumer full Flutter suite: 31/31; analysis no issues; existing 37 image
  semantics regressions passed. The published host supplies false and the draft
  host true. The production read-authority gate is unchanged.

The widget variants are not native-device VoiceOver or OS OCR acceptance.
Consumer native builds, signatures and actual enabled-text-recognition behavior
must be checked separately. No forced announcement, focus or system setting
change is introduced.
