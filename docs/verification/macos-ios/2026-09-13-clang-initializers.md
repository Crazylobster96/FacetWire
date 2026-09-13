# Apple Clang hierarchical-chart test initialization compatibility

Date: 2026-09-13. Scope: test-only compatibility repair on baseline
`2b545b17cc370a074930577d2242981ff124d6b6`.

## Cause and bounded upstream reuse

Eight result structures in
`plugins/hierarchical_chart_renderer/tests/hierarchical_chart_renderer_test.c`
used `{sizeof(value)}`. Apple Clang reports their omitted fields under
`-Wmissing-field-initializers`; the existing `-Werror` makes this a build failure.

Each structure is now initialized with `{0}`, followed by an explicit assignment
to `struct_size`. Member values and test semantics remain unchanged. No production
code, public ABI, test assertions, targets, warning settings or dependencies change.
The same file was checked for other instances of this partial initialization.

Remote inspection found this exact repair in upstream
`bc015482c2b5dacb90775bd3fadb555cd76201b3`. Only its test-file change is used here;
the resulting file has no diff against that upstream version. Its Apple-platform
report is not copied as evidence for this run. Later commits `e600532` and
`65f816b` contain unrelated flow/demo changes and are not incorporated. Work is on
`codex/macos-clang-initializers-20260913`, based on the fixed baseline. The user
subsequently approved committing and pushing this bounded independent repair
before updating the consuming Pillow gitlink. No release or unrelated upstream
upgrade is authorized.

## Automated verification actually run

Environment: macOS 26.5.1 arm64, Xcode 26.6, Apple Clang 21.0.0
(`clang-2100.1.1.101`), CMake 4.4.2, Debug. Existing C standard requirements and
`-Wall -Wextra -Wpedantic -Werror` are unchanged.

From the consuming Pillow checkout, using a newly created build directory:

```sh
cmake -S third_party/FacetWire -B build/facetwire-mac-fixes-48ZL2B -DFACETWIRE_BUILD_TESTS=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build/facetwire-mac-fixes-48ZL2B --parallel 4
ctest --test-dir build/facetwire-mac-fixes-48ZL2B --output-on-failure
```

Configure, complete build and CTest all returned **0**. CTest executed **14/14**,
all passed, no Not Run/disabled/skipped tests (7.48 seconds reported by CTest):

- placeholder renderer contract and rendering contract;
- text, core image, core media, core chart and hierarchical chart contracts;
- flow layout, hello example, plugin manifests and visual transform;
- runtime contract, runtime discovery and memory lifecycle.

Raw logs and subprocess exit records are preserved in the consuming checkout's
ignored `work/mac-fixes-20260913.48ZL2B/facetwire-{configure,build,ctest}.{log,json}`.
No pre-existing build, evidence or running application was replaced.

## Limits and handoff

This is an automated build/contract result, **not measured 100% branch coverage**,
nor a new feature or a complete three-platform certification. No new FacetWire
standalone UI, iOS/device, Linux or Windows run was performed for this test-only
repair. Windows should run the complete CMake build and all 14 baseline CTests
with its supported native toolchain and unchanged warning policy.

After explicit approval, publish this bounded independent repair first; only then
may the consumer advance its submodule pointer. Do not publish an unreachable
consumer gitlink or silently incorporate the other upstream features.
