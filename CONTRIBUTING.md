# Contributing to FacetWire

Thank you for helping build FacetWire.

## Before opening a change

1. Discuss ABI or specification changes in an issue before implementation.
2. Keep platform-specific objects behind adapters; do not expose them in the
   common C ABI.
3. Add or update conformance tests for observable contract changes.
4. Record new dependencies and their licenses in `THIRD_PARTY_NOTICES.md`.
5. Run the complete local build and test commands from the README.

## Compatibility rules

- Never reorder or change the meaning of an existing ABI field.
- Append fields and gate access with `struct_size`.
- Reject incompatible ABI major versions.
- A minor version may only add optional, size-gated behavior.
- Do not transfer ownership of raw allocations across the ABI.
- Public strings are UTF-8 byte spans with an explicit length.

## Commit certification

FacetWire uses the Developer Certificate of Origin 1.1. Sign off commits with:

```sh
git commit -s
```

The sign-off certifies that you have the right to submit the contribution under
the project's license. Read the full DCO at <https://developercertificate.org/>.

## Pull requests

Keep pull requests focused. Describe affected platforms, ABI impact, tests,
dependencies, and any compatibility risk. Security reports must follow
`SECURITY.md` rather than public issue disclosure.
