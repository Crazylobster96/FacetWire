#!/usr/bin/env bash
set -euo pipefail

demo_root="$(cd "$(dirname "$0")/.." && pwd)"
repo_root="$(cd "$demo_root/../.." && pwd)"
native_build="$repo_root/build/placeholder-demo-native-macos"

if [[ -z "${FACETWIRE_FLUTTER_ROOT:-}" ]]; then
  env_file="$HOME/.facetwire/toolchains.env"
  if [[ -f "$env_file" ]]; then
    # shellcheck disable=SC1090
    source "$env_file"
  fi
fi
if [[ -z "${FACETWIRE_FLUTTER_ROOT:-}" ]]; then
  echo "FACETWIRE_FLUTTER_ROOT is not configured." >&2
  exit 1
fi
flutter="$FACETWIRE_FLUTTER_ROOT/bin/flutter"

cmake -S "$repo_root" -B "$native_build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DFACETWIRE_BUILD_TESTS=ON \
  -DFACETWIRE_BUILD_EXAMPLES=ON \
  -DFACETWIRE_BUILD_PLACEHOLDER_RENDERER=ON \
  -DFACETWIRE_BUILD_PLACEHOLDER_DEMO=ON
cmake --build "$native_build" --config Release \
  --target facetwire_placeholder_demo_bridge \
  facetwire_placeholder_demo_bridge_test \
  facetwire_playground_bridge_test \
  facetwire_placeholder_renderer_test \
  facetwire_placeholder_rendering_contract_test
ctest --test-dir "$native_build" -C Release --output-on-failure \
  -R 'facetwire.placeholder_(renderer|demo)'

cd "$demo_root"
"$flutter" pub get
"$flutter" analyze
NO_PROXY="${NO_PROXY:+$NO_PROXY,}localhost,127.0.0.1,::1" \
no_proxy="${no_proxy:+$no_proxy,}localhost,127.0.0.1,::1" \
  "$flutter" test
"$flutter" build macos --release

app="$demo_root/build/macos/Build/Products/Release/facetwire_placeholder_demo.app"
dylib="$(find "$native_build" -name 'libfacetwire_placeholder_demo_bridge.dylib' -print -quit)"
if [[ ! -d "$app" || -z "$dylib" ]]; then
  echo "Expected app or native dylib was not produced." >&2
  exit 1
fi
mkdir -p "$app/Contents/Frameworks"
"$FACETWIRE_FLUTTER_ROOT/bin/dart" run tool/native_smoke.dart "$dylib"
cp "$dylib" "$app/Contents/Frameworks/"
codesign --force --deep --sign - "$app"

echo "FacetWire Playground macOS build passed."
echo "Run: open '$app'"

