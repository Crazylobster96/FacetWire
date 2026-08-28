#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
app_root="$repo_root/examples/placeholder_demo"
env_file="$HOME/.config/facetwire/flutter.env"

if [[ -z "${FACETWIRE_FLUTTER_ROOT:-}" && -f "$env_file" ]]; then
  source "$env_file"
fi
if [[ -z "${FACETWIRE_FLUTTER_ROOT:-}" ]]; then
  echo "Flutter is not configured. Run scripts/bootstrap-flutter-macos.sh." >&2
  exit 1
fi
flutter="$FACETWIRE_FLUTTER_ROOT/bin/flutter"
if [[ ! -x "$flutter" ]]; then
  echo "Flutter executable not found at $flutter" >&2
  exit 1
fi

actual_commit="$(git -C "$FACETWIRE_FLUTTER_ROOT" rev-parse HEAD)"
expected_commit="$(
  sed -nE 's/^[[:space:]]*"frameworkCommit":[[:space:]]*"([^"]+)".*/\1/p' \
    "$repo_root/toolchains.lock.json" | head -n 1
)"
[[ "$actual_commit" == "$expected_commit" ]] || {
  echo "Flutter revision mismatch: $actual_commit != $expected_commit" >&2
  exit 1
}

cmake -S "$repo_root" -B "$repo_root/build/macos-root" \
  -DFACETWIRE_BUILD_TESTS=ON \
  -DFACETWIRE_BUILD_PLACEHOLDER_DEMO=ON
cmake --build "$repo_root/build/macos-root"
ctest --test-dir "$repo_root/build/macos-root" --output-on-failure


export NO_PROXY="${NO_PROXY:+$NO_PROXY,}localhost,127.0.0.1,::1"
export no_proxy="${no_proxy:+$no_proxy,}localhost,127.0.0.1,::1"

cd "$app_root"
"$flutter" pub get
"$flutter" analyze
"$flutter" test
"$flutter" build macos --debug
"$flutter" build ios --simulator --debug

echo "macOS app: $app_root/build/macos/Build/Products/Debug/"
echo "iOS simulator app: $app_root/build/ios/iphonesimulator/Runner.app"
