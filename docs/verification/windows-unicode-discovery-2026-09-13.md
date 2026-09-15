# Windows Unicode dynamic-discovery regression

Base: `b71e3f2d89b09c70f3b8023b21186b9936ee6907`. This change is limited to the conformance-test entry point and test registration. It does not change the runtime, ABI, plugin implementations, or UTF-8 path contract.

## Cause and correction

On the tested Windows host the ANSI code page is 936. The discovery test passed narrow CRT `argv[1]` to `fw_runtime_load_dynamic`, which interprets its path as UTF-8. The same DLL loaded successfully from an ASCII path but failed from the Chinese checkout path. The runtime already converts UTF-8 to UTF-16 and uses `LoadLibraryExW`; changing it to accept ANSI would break its contract.

The Windows test now uses `wmain` and explicitly converts the UTF-16 argument to UTF-8 with `WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ...)`. The terminating NUL is excluded from the string view length and the temporary buffer is freed after the load/unload/reload checks. Non-Windows `main` is unchanged; MinGW receives `-municode` for the wide CRT entry point.

A Windows-only CTest copies the built synthetic plugin into a path containing Chinese characters, Greek omega, and a supplementary-plane rocket character, then runs the same full discovery executable. This catches accidental ANSI conversion even under an ASCII checkout. No test is removed, and `/W4 /WX /permissive-` stays enabled. The regression is only registered when native loading is enabled; the original disabled-backend test remains intact.

## Automated results

Windows x64, Visual Studio 2022 generator, original Chinese checkout/build path:

- New Unicode regression before the code fix: failed at the original dynamic-load assertion; the 15-second child timeout bounded the assertion-dialog wait. This is the expected red test, not a successful run.
- Debug after fix: **15/15 CTest passed**, 2.30 seconds, including the original discovery test and the added Unicode test.
- Release after fix: **15/15 CTest passed**, 26.51 seconds; the conformance test still explicitly enables assertions.
- Separate Debug build with `FACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING=OFF`: **14/14 CTest passed**, 26.54 seconds, including the original unsupported-loading branch.

This is a test-harness encoding correction, not newly implemented runtime logic. No claim of whole-runtime branch coverage is made. No UI, network, real user data, provider calls or plugin installation was involved. macOS/Linux and MinGW were not rerun on this Windows host; their unchanged behavior or linker option still needs the corresponding platform CI/maintainer validation.

Commands (replace the build directory as needed):

```powershell
cmake -S . -B build-unicode -G "Visual Studio 17 2022" -A x64
cmake --build build-unicode --config Debug --parallel 4
ctest --test-dir build-unicode -C Debug --output-on-failure
cmake --build build-unicode --config Release --parallel 4
ctest --test-dir build-unicode -C Release --output-on-failure
cmake -S . -B build-disabled -G "Visual Studio 17 2022" -A x64 -DFACETWIRE_ENABLE_NATIVE_DYNAMIC_LOADING=OFF
cmake --build build-disabled --config Debug --parallel 4
ctest --test-dir build-disabled -C Debug --output-on-failure
```
