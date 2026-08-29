param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$demoRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $demoRoot "..\..")).Path
$nativeBuild = Join-Path $repoRoot "build\placeholder-demo-native-windows-ninja"
$runnerBuild = Join-Path $demoRoot "build\windows-ninja"

$flutterRoot = $env:FACETWIRE_FLUTTER_ROOT
if (-not $flutterRoot) {
    $flutterRoot = "D:\AICode\_toolchains\flutter"
}
$flutter = Join-Path $flutterRoot "bin\flutter.bat"
if (-not (Test-Path -LiteralPath $flutter)) {
    throw "Pinned Flutter SDK not found: $flutter"
}

$vsRoot = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$devShell = Join-Path $vsRoot "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
$cmake = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
$ninja = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
foreach ($tool in @($devShell, $cmake, $ctest, $ninja)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "Required Visual Studio tool not found: $tool"
    }
}

Import-Module $devShell
Enter-VsDevShell -VsInstallPath $vsRoot -SkipAutomaticLocation `
    -DevCmdArguments "-arch=x64 -host_arch=x64"

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)] [scriptblock]$Command,
        [Parameter(Mandatory = $true)] [string]$Failure
    )
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw $Failure
    }
}

function Ensure-NativeArchive {
    param(
        [Parameter(Mandatory = $true)] [string]$Url,
        [Parameter(Mandatory = $true)] [string]$Destination,
        [Parameter(Mandatory = $true)] [string]$Md5
    )

    if (Test-Path -LiteralPath $Destination) {
        $actual = (Get-FileHash -LiteralPath $Destination -Algorithm MD5).Hash
        if ($actual -ieq $Md5) {
            Write-Host "Using verified native media archive: $Destination"
            return
        }
    }

    $parent = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    $partial = "$Destination.download"
    if (Test-Path -LiteralPath $partial) {
        Remove-Item -LiteralPath $partial -Force
    }

    Write-Host "Downloading native media archive: $Url"
    try {
        Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $partial -TimeoutSec 600
        $actual = (Get-FileHash -LiteralPath $partial -Algorithm MD5).Hash
        if ($actual -ine $Md5) {
            throw "Native media archive checksum mismatch: $Destination"
        }
        Move-Item -LiteralPath $partial -Destination $Destination -Force
    } finally {
        if (Test-Path -LiteralPath $partial) {
            Remove-Item -LiteralPath $partial -Force
        }
    }
}

Invoke-Checked {
    & $cmake -S $repoRoot -B $nativeBuild -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$ninja" `
        "-DCMAKE_BUILD_TYPE=$Configuration" `
        -DFACETWIRE_BUILD_TESTS=ON `
        -DFACETWIRE_BUILD_EXAMPLES=ON `
        -DFACETWIRE_BUILD_PLACEHOLDER_RENDERER=ON `
        -DFACETWIRE_BUILD_PLACEHOLDER_DEMO=ON
} "Native CMake configure failed."

Invoke-Checked {
    & $cmake --build $nativeBuild --target `
        facetwire_placeholder_demo_bridge `
        facetwire_placeholder_demo_bridge_test `
        facetwire_playground_bridge_test `
        facetwire_placeholder_renderer_test `
        facetwire_placeholder_rendering_contract_test
} "Native bridge build failed."

Invoke-Checked {
    & $ctest --test-dir $nativeBuild --output-on-failure `
        -R "facetwire.placeholder_(renderer|demo)"
} "Native contract tests failed."

$env:NO_PROXY = "${env:NO_PROXY},localhost,127.0.0.1,::1"
$env:no_proxy = "${env:no_proxy},localhost,127.0.0.1,::1"
Push-Location $demoRoot
try {
    Invoke-Checked { & $flutter pub get } "flutter pub get failed."
    Invoke-Checked { & $flutter analyze } "flutter analyze failed."
    Invoke-Checked { & $flutter test } "flutter test failed."
} finally {
    Pop-Location
}

# media_kit performs synchronous downloads from its CMake configure step without
# progress or a timeout. Fetch and verify the pinned archives first so a clean
# Windows build cannot appear to hang indefinitely during configure.
$mpvArchive = Join-Path $runnerBuild "mpv-dev-x86_64-20230924-git-652a1dd.7z"
$angleArchive = Join-Path $runnerBuild "ANGLE.7z"
Ensure-NativeArchive `
    -Url "https://github.com/media-kit/libmpv-win32-video-build/releases/download/2023-09-24/mpv-dev-x86_64-20230924-git-652a1dd.7z" `
    -Destination $mpvArchive `
    -Md5 "a832ef24b3a6ff97cd2560b5b9d04cd8"
Ensure-NativeArchive `
    -Url "https://github.com/alexmercerind/flutter-windows-ANGLE-OpenGL-ES/releases/download/v1.0.1/ANGLE.7z" `
    -Destination $angleArchive `
    -Md5 "e866f13e8d552348058afaafe869b1ed"

# Flutter's Visual Studio generator can select HostX86 for CompilerId on some
# installations. The committed CMake runner is generator-neutral, so the demo
# uses the already verified Hostx64 + Ninja path deterministically.
Invoke-Checked {
    & $cmake -S (Join-Path $demoRoot "windows") -B $runnerBuild -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$ninja" `
        "-DCMAKE_BUILD_TYPE=$Configuration" `
        -DFLUTTER_TARGET_PLATFORM=windows-x64
} "Flutter Windows CMake configure failed."

# media_kit downloads archives during configure, but its extraction targets do
# not establish file-level dependencies for Ninja. Pre-extract them, then
# regenerate so the imported libraries exist before media_kit_video links.
function Expand-NativeArchive {
    param(
        [Parameter(Mandatory = $true)] [string]$Archive,
        [Parameter(Mandatory = $true)] [string]$Destination,
        [Parameter(Mandatory = $true)] [string]$Expected
    )
    if (Test-Path -LiteralPath $Expected) { return }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Push-Location $Destination
    try {
        Invoke-Checked {
            & $cmake -E tar xvf $Archive
        } "Failed to extract native media archive: $Archive"
    } finally {
        Pop-Location
    }
    if (-not (Test-Path -LiteralPath $Expected)) {
        throw "Native media archive did not produce: $Expected"
    }
}

$mpvRoot = Join-Path $runnerBuild "libmpv"
Expand-NativeArchive -Archive $mpvArchive -Destination $mpvRoot `
    -Expected (Join-Path $mpvRoot "libmpv.dll.a")
$mpvNestedHeaders = Join-Path $mpvRoot "include\mpv"
$mpvHeaders = Join-Path $mpvRoot "include"
if ((Test-Path -LiteralPath $mpvNestedHeaders) -and
    -not (Test-Path -LiteralPath (Join-Path $mpvHeaders "client.h"))) {
    Copy-Item -LiteralPath (Join-Path $mpvNestedHeaders "client.h") `
        -Destination $mpvHeaders -Force
    Copy-Item -LiteralPath (Join-Path $mpvNestedHeaders "render.h") `
        -Destination $mpvHeaders -Force
    Copy-Item -LiteralPath (Join-Path $mpvNestedHeaders "render_gl.h") `
        -Destination $mpvHeaders -Force
    Copy-Item -LiteralPath (Join-Path $mpvNestedHeaders "stream_cb.h") `
        -Destination $mpvHeaders -Force
}

$angleRoot = Join-Path $runnerBuild "ANGLE"
Expand-NativeArchive -Archive $angleArchive -Destination $angleRoot `
    -Expected (Join-Path $angleRoot "lib\libEGL.dll.lib")

Invoke-Checked {
    & $cmake -S (Join-Path $demoRoot "windows") -B $runnerBuild -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$ninja" `
        "-DCMAKE_BUILD_TYPE=$Configuration" `
        -DFLUTTER_TARGET_PLATFORM=windows-x64
} "Flutter Windows CMake regenerate after media extraction failed."
Invoke-Checked { & $cmake --build $runnerBuild } "Windows Runner build failed."
Invoke-Checked {
    & $cmake --install $runnerBuild --config $Configuration
} "Windows Runner bundle install failed."

$dll = Join-Path $nativeBuild "bin\facetwire_placeholder_demo_bridge.dll"
$runnerDir = Join-Path $runnerBuild "runner"
if (-not (Test-Path -LiteralPath $dll)) {
    throw "Native bridge DLL was not produced: $dll"
}
Copy-Item -LiteralPath $dll -Destination $runnerDir -Force
Push-Location $demoRoot
try {
    Invoke-Checked {
        & (Join-Path $flutterRoot "bin\dart.bat") run tool\native_smoke.dart $dll
    } "Dart FFI smoke test failed."
} finally {
    Pop-Location
}
$exe = Join-Path $runnerDir "facetwire_placeholder_demo.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Demo EXE was not produced: $exe"
}

Write-Host "FacetWire Playground Windows build passed."
Write-Host "Run: $exe"
