<#
================================================================================
 build.ps1 - one-command build script

 What it does: locate CMake (prefers the copy bundled with Visual Studio),
 configure, build, then run the program.

 Usage:
     .\build.ps1                       # Release build, then run
     .\build.ps1 -Config Debug         # Debug build
     .\build.ps1 -NoRun                # build only
     .\build.ps1 -Clean                # wipe build/ first, then reconfigure
     .\build.ps1 -Generator "Ninja"    # use a different generator

 NOTE: this file is intentionally ASCII-only (English messages, no non-ASCII
 characters). Windows PowerShell 5.1 decodes a .ps1 file using the system ANSI
 code page unless the file starts with a UTF-8 BOM. Chinese text in this script
 would therefore be garbled on a zh-CN system and break parsing, so the script
 stays ASCII. The C++ sources contain Chinese comments and are fine because the
 compiler is told to read them as UTF-8 via the /utf-8 flag in CMakeLists.txt.

 Why this script looks for CMake itself: Visual Studio installs CMake inside its
 own directory and does not add it to PATH, so a bare `cmake` command fails.
================================================================================
#>

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Config = 'Release',

    [string]$Generator = 'Visual Studio 18 2026',

    [switch]$Clean,

    [switch]$NoRun
)

$ErrorActionPreference = 'Stop'

$Root     = $PSScriptRoot
$BuildDir = Join-Path $Root 'build'

function Write-Step($text) {
    Write-Host ''
    Write-Host "==> $text" -ForegroundColor Cyan
}

# ---------------------------------------------------------------------------
# 1. Locate CMake: prefer the Visual Studio bundled copy, then PATH.
# ---------------------------------------------------------------------------
function Find-CMake {
    $candidates = @(
        'D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'D:\Program Files\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\CMake\bin\cmake.exe'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) { return $candidate }
    }

    # Fall back to a recursive search under the Visual Studio install roots.
    $vsRoots = @(
        'D:\Program Files\Microsoft Visual Studio',
        'C:\Program Files\Microsoft Visual Studio'
    )
    foreach ($vsRoot in $vsRoots) {
        if (Test-Path $vsRoot) {
            $found = Get-ChildItem $vsRoot -Recurse -Filter 'cmake.exe' -ErrorAction SilentlyContinue |
                     Select-Object -First 1
            if ($found) { return $found.FullName }
        }
    }

    # Last resort: whatever is on PATH.
    $onPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    throw 'cmake.exe not found. Install CMake, or make sure the Visual Studio C++ workload is installed.'
}

$CMakeExe = Find-CMake
Write-Host "CMake: $CMakeExe" -ForegroundColor DarkGray

# ---------------------------------------------------------------------------
# 2. Optionally wipe build/
# ---------------------------------------------------------------------------
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Step "Cleaning $BuildDir"
    Remove-Item $BuildDir -Recurse -Force
}

# ---------------------------------------------------------------------------
# 3. Configure
# ---------------------------------------------------------------------------
Write-Step "Configure (generator: $Generator, config: $Config)"
# CMAKE_BUILD_TYPE only means something to single-config generators (Ninja,
# Makefiles). Passing it to a multi-config generator such as Visual Studio makes
# CMake print an "unused variable" warning, so only pass it when it applies.
$configureArgs = @('-S', $Root, '-B', $BuildDir, '-G', $Generator)
if ($Generator -notmatch 'Visual Studio') {
    $configureArgs += "-DCMAKE_BUILD_TYPE=$Config"
}
& $CMakeExe @configureArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed (exit code $LASTEXITCODE)" }

# ---------------------------------------------------------------------------
# 4. Build
# ---------------------------------------------------------------------------
Write-Step 'Build'
& $CMakeExe --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit code $LASTEXITCODE)" }

$Exe = Join-Path $BuildDir 'bin\bitmap_demo.exe'
if (-not (Test-Path $Exe)) { throw "Build reported success but $Exe is missing." }
Write-Host "Built: $Exe" -ForegroundColor Green

# ---------------------------------------------------------------------------
# 5. Run
#     Run from the exe directory so the program writes sphere.bmp into
#     build/bin/ instead of polluting the source tree.
# ---------------------------------------------------------------------------
if (-not $NoRun) {
    Write-Step 'Run'
    # The program prints UTF-8 Chinese text; switch the console output encoding
    # so it is not garbled on a zh-CN console.
    try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }
    Push-Location (Split-Path $Exe -Parent)
    try {
        & $Exe
        Write-Host "Exit code: $LASTEXITCODE" -ForegroundColor DarkGray
    } finally {
        Pop-Location
    }
}

Write-Step 'Done'
