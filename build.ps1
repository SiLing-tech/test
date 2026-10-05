<#
================================================================================
 build.ps1 - one-command build script for the whole book repository

 The repository is organised by chapter (common/ holds code shared by all
 chapters). This script configures the root CMake project, builds every target,
 then runs one chapter's demo.

 What it does: locate CMake (prefers the copy bundled with Visual Studio),
 configure, build, then run the selected program.

 Usage:
     .\build.ps1                       # Release build, run section_2_demo
     .\build.ps1 -List                 # list the executables that were built
     .\build.ps1 -Target <name>        # run a specific executable (no .exe)
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

    # Which executable to run. Run `.\build.ps1 -List` to see what is available.
    # Chapter demos are named <chapter>_demo (e.g. section_2_demo).
    [string]$Target = 'section_2_demo',

    [switch]$Clean,

    [switch]$NoRun,

    [switch]$List
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

$BinDir = Join-Path $BuildDir 'bin'
$BuiltExes = @()
if (Test-Path $BinDir) {
    $BuiltExes = @(Get-ChildItem $BinDir -Filter '*.exe' -File -ErrorAction SilentlyContinue |
                   Sort-Object Name)
}

# ---------------------------------------------------------------------------
# 5. List available executables, if asked
# ---------------------------------------------------------------------------
if ($List) {
    Write-Step 'Available executables'
    if ($BuiltExes.Count -eq 0) {
        Write-Host '  (none built yet)'
    } else {
        foreach ($item in $BuiltExes) {
            Write-Host ("  {0}" -f $item.Name)
        }
        Write-Host ''
        Write-Host 'Run one with:  .\build.ps1 -Target <name without .exe>' -ForegroundColor DarkGray
    }
    Write-Step 'Done'
    return
}

# ---------------------------------------------------------------------------
# 6. Pick the executable to run
#    Each chapter produces its own <chapter>_demo, so the script discovers what
#    actually exists instead of hard-coding one path. Adding a chapter does not
#    require touching this script.
# ---------------------------------------------------------------------------
$Exe = Join-Path $BinDir "$Target.exe"
if (-not (Test-Path $Exe)) {
    $available = if ($BuiltExes.Count -gt 0) {
        ($BuiltExes | ForEach-Object { $_.Name }) -join ', '
    } else { '(none)' }
    throw "Executable '$Target.exe' not found in build/bin. Available: $available"
}
Write-Host "Built: $Exe" -ForegroundColor Green

# ---------------------------------------------------------------------------
# 7. Run
#     Run from the exe directory so the program writes its output bitmap into
#     build/bin/ instead of polluting the source tree.
# ---------------------------------------------------------------------------
if (-not $NoRun) {
    Write-Step "Run $Target"
    # The program prints UTF-8 Chinese text; switch the console output encoding
    # so it is not garbled on a zh-CN console.
    try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }
    Push-Location $BinDir
    try {
        & $Exe
        Write-Host "Exit code: $LASTEXITCODE" -ForegroundColor DarkGray
    } finally {
        Pop-Location
    }
}

Write-Step 'Done'
