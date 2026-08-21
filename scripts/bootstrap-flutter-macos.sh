#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
lock_file="$repo_root/toolchains.lock.json"
default_root="$HOME/Library/Application Support/FacetWire/toolchains"
toolchains_root="${FACETWIRE_TOOLCHAINS_ROOT:-$default_root}"
flutter_root="$toolchains_root/flutter"

flutter_version="$(
  sed -nE 's/^[[:space:]]*"version":[[:space:]]*"([^"]+)".*/\1/p' \
    "$lock_file" | head -n 1
)"
flutter_commit="$(
  sed -nE 's/^[[:space:]]*"frameworkCommit":[[:space:]]*"([^"]+)".*/\1/p' \
    "$lock_file" | head -n 1
)"

if [[ -z "$flutter_version" || -z "$flutter_commit" ]]; then
  echo "Unable to read the Flutter lock from $lock_file" >&2
  exit 1
fi
if ! command -v git >/dev/null 2>&1; then
  echo "Git is required. Run xcode-select --install, then retry." >&2
  exit 1
fi

if [[ ! -d "$flutter_root/.git" ]]; then
  mkdir -p "$toolchains_root"
  git clone --depth 1 --branch "$flutter_version" \
    https://github.com/flutter/flutter.git "$flutter_root"
fi

actual_commit="$(git -C "$flutter_root" rev-parse HEAD)"
if [[ "$actual_commit" != "$flutter_commit" ]]; then
  cat >&2 <<EOF
Flutter at '$flutter_root' is not the locked revision.
Expected: $flutter_commit
Actual:   $actual_commit
Move that directory aside or set FACETWIRE_TOOLCHAINS_ROOT to another path.
Never run 'flutter upgrade' for this repository.
EOF
  exit 1
fi

export FACETWIRE_FLUTTER_ROOT="$flutter_root"
export FLUTTER_SUPPRESS_ANALYTICS=true
if [[ "${FACETWIRE_USE_CHINA_MIRROR:-0}" == "1" ]]; then
  export FLUTTER_STORAGE_BASE_URL=https://storage.flutter-io.cn
  export PUB_HOSTED_URL=https://pub.flutter-io.cn
fi

env_dir="$HOME/.config/facetwire"
env_file="$env_dir/flutter.env"
mkdir -p "$env_dir"
{
  printf 'export FACETWIRE_FLUTTER_ROOT=%q\n' "$flutter_root"
  printf 'export FLUTTER_SUPPRESS_ANALYTICS=true\n'
  if [[ "${FACETWIRE_USE_CHINA_MIRROR:-0}" == "1" ]]; then
    printf 'export FLUTTER_STORAGE_BASE_URL=https://storage.flutter-io.cn\n'
  fi
} >"$env_file"

"$flutter_root/bin/flutter" config --no-analytics
"$flutter_root/bin/flutter" precache --ios --macos --android
"$flutter_root/bin/flutter" doctor -v

echo
echo "FacetWire Flutter SDK ready:"
echo "  root:    $flutter_root"
echo "  commit:  $actual_commit"
echo "  env:     $env_file"
echo "Future Codex sessions should run: source \"$env_file\""
