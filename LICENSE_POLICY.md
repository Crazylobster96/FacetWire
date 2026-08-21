# FacetWire licensing policy

## Project code

Unless a file states otherwise, original FacetWire source code is licensed
under the Mozilla Public License 2.0 (`MPL-2.0`). New source files should carry:

```text
SPDX-License-Identifier: MPL-2.0
```

MPL-2.0 applies at the source-file level. Independent plugins and larger works
may use other open-source or proprietary licenses, provided they do not copy
covered code into proprietary files and comply with all applicable notices.

## Plugin licensing

Every distributed plugin must declare an SPDX license expression or a stable
`LicenseRef-*` identifier in its manifest. A plugin must also identify:

- bundled and externally supplied dependencies;
- whether redistribution is permitted;
- required proprietary SDKs or system frameworks;
- known patent, codec, font, or media-asset obligations.

The plugin ABI is a technical boundary, not a promise that every combination
is legally redistributable. Each distributor remains responsible for the
licenses of the exact build it ships.

## Third-party material

Third-party code and assets must remain clearly separated and must be listed in
`THIRD_PARTY_NOTICES.md` or an equivalent notice generated with the release.
Do not commit proprietary SDKs, restricted sample documents, commercial fonts,
or patented codec binaries unless their terms explicitly permit redistribution.

## Contributions

Contributions are accepted under MPL-2.0. By submitting a contribution, the
contributor confirms that they have the right to license it on those terms.
FacetWire currently uses a Developer Certificate of Origin sign-off rather
than a copyright assignment.
