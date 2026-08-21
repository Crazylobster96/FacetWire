#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
project_root="$repo_root/spikes/visionos_host/apple"
destination="${FW_VISIONOS_DESTINATION:-platform=visionOS Simulator,name=Apple Vision Pro}"

command -v xcodebuild >/dev/null || {
  echo "xcodebuild is required. Install the current Xcode release." >&2
  exit 1
}
command -v xcodegen >/dev/null || {
  echo "XcodeGen is required: brew install xcodegen" >&2
  exit 1
}

cd "$project_root"
xcodegen generate --spec project.yml
xcodebuild \
  -project FacetWireVisionOSSpike.xcodeproj \
  -scheme FacetWireVisionOSSpike \
  -sdk xrsimulator \
  -destination "$destination" \
  -derivedDataPath "$repo_root/build/visionos-spike-derived-data" \
  CODE_SIGNING_ALLOWED=NO \
  test

echo "visionOS Simulator build and unit tests passed."
