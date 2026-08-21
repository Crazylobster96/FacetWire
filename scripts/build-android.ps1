[CmdletBinding()]
param(
    [ValidateSet("debug", "release")]
    [string]$Mode = "debug",
    [ValidateSet("android-arm64", "android-arm", "android-x64")]
    [string]$TargetPlatform = "android-arm64",
    [switch]$UseChinaMirror
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$appRoot = Join-Path $repoRoot "spikes\playground_ui"

$candidates = @()
if ($env:FACETWIRE_FLUTTER_ROOT) {
    $candidates += $env:FACETWIRE_FLUTTER_ROOT
}
$candidates += "D:\AICode\_toolchains\flutter"
$candidates += (Join-Path $env:LOCALAPPDATA "FacetWire\toolchains\flutter")
$flutterRoot = $candidates |
    Where-Object { Test-Path -LiteralPath (Join-Path $_ "bin\flutter.bat") } |
    Select-Object -First 1
if (-not $flutterRoot) {
    throw "Flutter not found. Run scripts\bootstrap-flutter.ps1 first."
}
$flutterExe = Join-Path $flutterRoot "bin\flutter.bat"

if (-not $env:JAVA_HOME) {
    $candidate = "C:\Program Files\Android\Android Studio\jbr"
    if (Test-Path -LiteralPath $candidate) {
        $env:JAVA_HOME = $candidate
    }
}
if (-not $env:ANDROID_SDK_ROOT) {
    $candidate = Join-Path $env:LOCALAPPDATA "Android\Sdk"
    if (Test-Path -LiteralPath $candidate) {
        $env:ANDROID_SDK_ROOT = $candidate
    }
}
if (-not $env:ANDROID_SDK_ROOT) {
    throw "Android SDK not found. Run scripts\bootstrap-flutter.ps1 first."
}
if ($UseChinaMirror) {
    $env:FLUTTER_STORAGE_BASE_URL = "https://storage.flutter-io.cn"
}
$env:FLUTTER_SUPPRESS_ANALYTICS = "true"

Push-Location $appRoot
try {
    & $flutterExe pub get
    if ($LASTEXITCODE -ne 0) { throw "flutter pub get failed." }
    & $flutterExe analyze
    if ($LASTEXITCODE -ne 0) { throw "flutter analyze failed." }
    & $flutterExe test
    if ($LASTEXITCODE -ne 0) { throw "flutter test failed." }
    & $flutterExe build apk "--$Mode" --target-platform $TargetPlatform
    if ($LASTEXITCODE -ne 0) { throw "flutter build apk failed." }
} finally {
    Pop-Location
}

$source = Join-Path $appRoot "build\app\outputs\flutter-apk\app-$Mode.apk"
$dist = Join-Path $repoRoot "dist\android"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$abi = $TargetPlatform.Replace("android-", "")
$destination = Join-Path $dist "FacetWire-Playground-UI-Spike-0.1.0-$abi-$Mode.apk"
Copy-Item -LiteralPath $source -Destination $destination -Force

$buildToolsRoot = Join-Path $env:ANDROID_SDK_ROOT "build-tools"
$apksigner = Get-ChildItem -LiteralPath $buildToolsRoot -Filter apksigner.bat -Recurse |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if ($apksigner) {
    & $apksigner verify --verbose --print-certs $destination
    if ($LASTEXITCODE -ne 0) { throw "APK signature verification failed." }
}

$artifact = Get-Item -LiteralPath $destination
$hash = Get-FileHash -Algorithm SHA256 -LiteralPath $destination
Write-Host ""
Write-Host "APK ready: $($artifact.FullName)"
Write-Host "Bytes:     $($artifact.Length)"
Write-Host "SHA-256:   $($hash.Hash)"
