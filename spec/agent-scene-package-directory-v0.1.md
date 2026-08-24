# Agent Scene Package Directory Profile 0.1

Status: experimental. Normative words **MUST**, **MUST NOT**, **SHOULD**, and
**MAY** are to be interpreted as requirements of this profile.

## 1. Purpose and naming

This profile defines the uncompressed, unencrypted directory form of an Agent
Scene Package (ASP). A package directory MUST end in `.agscene`. Its root
descriptor MUST be UTF-8 JSON and MUST have the package directory name followed
by `.dis.json`.

Example:

```text
news-card.agscene/
  news-card.agscene.dis.json
  resources/
  documents/
```

The media type for the descriptor is
`application/vnd.facetwire.agscene+json`. Version `0.1` is experimental and is
not ABI-stable.

### Section check

- A package can be copied without an archive implementation.
- Descriptor discovery is deterministic and case-sensitive.
- Compression and encryption are explicitly outside this profile.

## 2. Portable path rules

Every descriptor reference MUST be a relative POSIX path using `/`. Absolute
paths, drive letters, URI schemes, empty segments, `.`, `..`, NUL, and `\` are
forbidden. A reference MUST resolve below the package directory after Unicode
normalization. Portable packages MUST NOT rely on symbolic links.

Resource files MAY use any extension. A host MUST treat them as bytes until a
selected parser or renderer accepts the declared media type.

### Section check

- The same bytes resolve on Windows and macOS.
- A descriptor cannot escape its package root.
- Unknown resources can remain opaque and still retain layout.

## 3. Descriptor object

The root JSON object MUST contain:

- `format`: exactly `facetwire.agent-scene-package`;
- `version`: exactly `0.1` for this profile;
- `id`: a stable document identifier;
- `title`: a human-readable title;
- `canvas`: one Canvas object;
- `resources`: an array, which MAY be empty.

Unknown members MUST be preserved by editors and ignored by renderers unless a
negotiated extension defines them. IDs MUST be unique within their owning
document and SHOULD remain stable across AI patches.

### Section check

- Version negotiation is explicit.
- Stable IDs support deterministic AI edits.
- Additive extensions do not invalidate older readers.

## 4. Canvas, Page, Layer, and Zone

`canvas` contains an `id`, logical `size`, and ordered `pages`. Logical units
are device-independent pixels for screen media. Each Page contains an `id`,
`size`, and ordered `layers`. Each Layer contains an `id`, integer `z`, and
ordered `zones`. Each Zone contains an `id`, `bounds`, optional `semantics`, and
one `content` object.

`bounds` is `{x, y, width, height}`. Values MUST be finite, and width/height
MUST be non-negative. A Zone retains these bounds even when its content cannot
be rendered. Layer order is ascending `z`, then descriptor order.

This profile defines two content types:

- `placeholder`: declares `kind`, `reason`, `mode`, optional `label`, optional
  `requiredCapability`, and optional `permittedActions`;
- `document`: declares `source`, pointing to another `.agscene` directory's
  same-name descriptor, plus optional `placement`.

Unknown content types MUST reserve the Zone bounds and route to the placeholder
capability when available.

`document.placement` contains optional `fit`, `alignment`, and `clip`. `fit` is
one of `none`, `contain`, `cover`, or `fill` and defaults to `none`.
`alignment` is `{x, y}` in the inclusive range 0 through 1 and defaults to
`{0, 0}` (top-left). `clip` defaults to `false`. Omitting `placement` therefore
preserves the child Canvas intrinsic logical size without implicit clipping.


### Section check

- Placeholder output replaces Zone content, never Canvas/Page/Layer layout.
- Page and layer ordering remain deterministic.
- Unsupported future content retains its intended space.

## 5. Recursive documents

A `document.source` reference MAY point to a nested package below the current
package directory. Each nested package is independently valid and owns its own
Canvas coordinate system. The referring Zone is the child Canvas viewport and
defines the transform into the parent Canvas:

- `none` preserves the child Canvas logical size at 1:1 and applies no scale;
  its origin follows `alignment` and content MAY extend beyond the Zone;
- `contain` applies the smaller uniform scale so the complete child Canvas is
  visible; unused viewport space follows `alignment`;
- `cover` applies the larger uniform scale so the viewport is covered; overflow
  follows `alignment` and is clipped when `clip` is true;
- `fill` scales the two axes independently to exactly match the Zone;
- `alignment.x=0/1` means left/right and `alignment.y=0/1` means top/bottom.

The transform is deterministic from child Canvas size, parent Zone bounds, and
`placement`; it MUST NOT depend on the desktop window size. Font and responsive
profile processing occurs inside the child Canvas before this embedding
transform. A host that cannot compose the child MUST retain the parent Zone and
route it to the placeholder renderer.

A viewer's default viewport scale MUST be `1.0`: one Canvas logical unit maps to
one platform logical display unit. A viewer MAY zoom the composed root Canvas
only in response to an explicit user choice or selected responsive presentation
profile, and MUST expose the active zoom factor. Viewport zoom is not a document
placement transform and MUST NOT modify stored Zone bounds, child Canvas size,
layout measurements, nested coordinates, or AI edits.

Hosts MUST detect descriptor-path cycles and MUST apply a configurable maximum
depth. The conformance default is 32. When either limit is reached, the host
MUST retain the referring Zone and render a placeholder with reason
`resource_limited` or `parse_failed`; it MUST NOT abandon sibling Zones.

Depth counts the root package as 1. A “three-level package” therefore contains
root, child, and grandchild descriptors.

### Section check

- Recursive composition is bounded and cycle-safe.
- Nested documents remain independently inspectable.
- Failure isolation is at the referring Zone.
- Parent and child coordinate systems are connected by an explicit,
  reproducible placement transform.
- The default viewer preserves one-to-one logical size and uses scrolling, not
  implicit fit-to-window scaling.
## 6. Resources

Each entry in `resources` contains `id`, `source`, and `mediaType`. `integrity`
MAY contain a future digest object and is omitted by the 0.1 demo fixture.
Resource paths follow Section 2. The descriptor MUST NOT contain credentials,
private absolute paths, presentation-session progress, or remote task IDs.

### Section check

- Persistent content is separated from Presentation Session state.
- Resource identity is stable while storage remains relative.
- Integrity can be added without changing the path model.

## 7. Conformance

A Directory Profile 0.1 validator MUST check naming, UTF-8 JSON, required
members, path safety, ID uniqueness, finite geometry, recursive depth, cycles,
and existence of referenced descriptors/resources. A renderer MAY support only
a subset of content types, but it MUST preserve valid geometry and use a safe
fallback for unsupported content.

The normative JSON Schema is
`spec/schema/agent-scene-package-v0.1.schema.json`. Cross-file path, naming,
cycle, and existence checks are procedural and are not expressible by that
schema alone.

### Section check

- Structural and procedural validation responsibilities are separated.
- Partial renderers remain conforming through placeholders.
- The profile has a deterministic test surface.

## 8. Overall consistency check

- The document owns persistent Canvas/Page/Layer/Zone data; runtime progress
  remains in Presentation Session Projection 0.1.
- Recursive documents compose at a Zone and therefore do not merge coordinate
  systems implicitly.
- All unsupported content paths preserve bounds and route to the existing
  placeholder renderer contract.
- Compression can later wrap the exact directory tree without changing any
  descriptor-relative references.

