[CmdletBinding()]
param(
    [string]$ToolchainsRoot = (Join-Path $env:LOCALAPPDATA "FacetWire\toolchains"),
    [string]$AndroidSdk = "",
    [string]$Jdk = "",
    [switch]$PersistEnvironment,
    [switch]$UseChinaMirror
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$lockPath = Join-Path $repoRoot "toolchains.lock.json"
$lock = Get-Content -Raw -LiteralPath $lockPath | ConvertFrom-Json
$flutterRoot = Join-Path $ToolchainsRoot "flutter"
$flutterExe = Join-Path $flutterRoot "bin\flutter.bat"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git is required. Install Git, reopen the terminal, and retry."
}

if (-not (Test-Path -LiteralPath (Join-Path $flutterRoot ".git"))) {
    New-Item -ItemType Directory -Force -Path $ToolchainsRoot | Out-Null
    $cloneArgs = @(
        "clone", "--depth", "1", "--branch", $lock.flutter.version,
        "https://github.com/flutter/flutter.git", $flutterRoot
    )
    & git @cloneArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Flutter clone failed with exit code $LASTEXITCODE."
    }
}

$actualCommit = (& git -C $flutterRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $lock.flutter.frameworkCommit) {
    throw @"
Flutter at '$flutterRoot' is not the locked revision.
Expected: $($lock.flutter.frameworkCommit)
Actual:   $actualCommit
Move that directory aside or pass a different -ToolchainsRoot. Never run
'flutter upgrade' for this repository.
"@
}

$env:FACETWIRE_FLUTTER_ROOT = $flutterRoot
$env:FLUTTER_SUPPRESS_ANALYTICS = "true"
if ($UseChinaMirror) {
    $env:FLUTTER_STORAGE_BASE_URL = "https://storage.flutter-io.cn"
    $env:PUB_HOSTED_URL = "https://pub.flutter-io.cn"
}

if (-not $AndroidSdk) {
    $candidate = Join-Path $env:LOCALAPPDATA "Android\Sdk"
    if (Test-Path -LiteralPath $candidate) {
        $AndroidSdk = $candidate
    }
}
if ($AndroidSdk) {
    $env:ANDROID_SDK_ROOT = $AndroidSdk
    & $flutterExe config --android-sdk $AndroidSdk
    if ($LASTEXITCODE -ne 0) {
        throw "Flutter rejected Android SDK '$AndroidSdk'."
    }
}

if (-not $Jdk) {
    $candidate = "C:\Program Files\Android\Android Studio\jbr"
    if (Test-Path -LiteralPath $candidate) {
        $Jdk = $candidate
    }
}
if ($Jdk) {
    $env:JAVA_HOME = $Jdk
    & $flutterExe config --jdk-dir $Jdk
    if ($LASTEXITCODE -ne 0) {
        throw "Flutter rejected JDK '$Jdk'."
    }
}

& $flutterExe config --no-analytics
if ($LASTEXITCODE -ne 0) {
    throw "Unable to disable Flutter analytics."
}
& $flutterExe precache --android --windows
if ($LASTEXITCODE -ne 0) {
    throw "Flutter precache failed."
}

if ($PersistEnvironment) {
    [Environment]::SetEnvironmentVariable(
        "FACETWIRE_FLUTTER_ROOT",
        $flutterRoot,
        [EnvironmentVariableTarget]::User
    )
    if ($AndroidSdk) {
        [Environment]::SetEnvironmentVariable(
            "ANDROID_SDK_ROOT",
            $AndroidSdk,
            [EnvironmentVariableTarget]::User
        )
    }
}

Write-Host ""
Write-Host "FacetWire Flutter SDK ready:"
Write-Host "  root:    $flutterRoot"
Write-Host "  commit:  $actualCommit"
Write-Host "  command: & '$flutterExe' doctor -v"
if ($PersistEnvironment) {
    Write-Host "FACETWIRE_FLUTTER_ROOT was persisted for future Codex sessions."
}
& $flutterExe doctor -v
