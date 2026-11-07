<#
.SYNOPSIS
  Build Expressive Composer core tests, CLI, or the JUCE VST3/Standalone plugin.

.DESCRIPTION
  Windows requires an ASCII build path for JUCE's juceaide resource generator.
  If the repository path contains non-ASCII characters, this script creates a
  directory junction at <SystemDrive>:\ecwork and builds through it. Source
  files remain in the original workspace; only build outputs use the junction.

.EXAMPLE
  pwsh -File scripts/build.ps1 -Target all -Configuration Release
#>
param(
    [ValidateSet('core', 'plugin', 'all')]
    [string]$Target = 'all',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'

function Find-Tool([string]$Name, [string[]]$Candidates) {
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($candidate in $Candidates) {
        if (Test-Path $candidate) { return $candidate }
    }
    throw "Cannot find $Name. Install it and add it to PATH."
}

$cmake = Find-Tool 'cmake' @(
    "$env:APPDATA\Python\Python312\Scripts\cmake.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python312\Scripts\cmake.exe"
)
$ninja = Find-Tool 'ninja' @(
    "$env:APPDATA\Python\Python312\Scripts\ninja.exe",
    "$env:LOCALAPPDATA\Programs\Python\Python312\Scripts\ninja.exe"
)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found. Install Visual Studio 2022 Build Tools with C++.' }
$env:PATH = (Split-Path $cmake) + ';' + (Split-Path $ninja) + ';' + $env:PATH
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual Studio C++ build tools not found.' }
$devCmd = Join-Path $vsPath 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path $devCmd)) { throw "VsDevCmd.bat not found at $devCmd" }

$repo = Split-Path -Parent $PSScriptRoot
$asciiPattern = '^[\x00-\x7F]+$'
$buildRoot = $repo
if ($repo -notmatch $asciiPattern) {
    $candidates = @('D:\ecwork', (Join-Path $env:SystemDrive 'ecwork')) | Select-Object -Unique
    $junction = $null
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $item = Get-Item $candidate
            if ($item.LinkType -eq 'Junction' -and $item.Target -contains $repo) {
                $junction = $candidate
                break
            }
        }
    }
    if (-not $junction) {
        $junction = Join-Path $env:SystemDrive 'ecwork'
        cmd /c "mklink /J `"$junction`" `"$repo`"" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to create junction $junction -> $repo" }
    }
    $buildRoot = $junction
}

function Invoke-Native([string]$Command) {
    Write-Host "> $Command" -ForegroundColor DarkGray
    cmd /c $Command
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE" }
}

$source = $buildRoot
$coreBuild = Join-Path $buildRoot 'build-core'
$pluginBuild = Join-Path $buildRoot 'build-plugin-ascii'

if ($Target -eq 'core' -or $Target -eq 'all') {
    $tests = if ($SkipTests) { 'OFF' } else { 'ON' }
    $command = "call `"$devCmd`" -arch=x64 -host_arch=x64 >nul && " +
               "`"$cmake`" -S `"$source`" -B `"$coreBuild`" -G Ninja " +
               "-DCMAKE_BUILD_TYPE=$Configuration -DEC_BUILD_PLUGIN=OFF " +
               "-DEC_BUILD_TESTS=$tests -DEC_BUILD_BENCH=ON && " +
               "`"$cmake`" --build `"$coreBuild`" --parallel"
    Invoke-Native $command
    if (-not $SkipTests) {
        Invoke-Native "`"$coreBuild\ec_tests.exe`""
    }
}

if ($Target -eq 'plugin' -or $Target -eq 'all') {
    if (-not (Test-Path (Join-Path $source 'third_party\JUCE\CMakeLists.txt'))) {
        throw 'JUCE not found. Run scripts/fetch_dependencies.ps1 first.'
    }
    $command = "call `"$devCmd`" -arch=x64 -host_arch=x64 >nul && " +
               "`"$cmake`" -S `"$source`" -B `"$pluginBuild`" -G Ninja " +
               "-DCMAKE_BUILD_TYPE=$Configuration -DEC_BUILD_PLUGIN=ON -DEC_BUILD_CLAP=OFF " +
               "-DEC_BUILD_TESTS=OFF -DEC_BUILD_BENCH=OFF && " +
               "`"$cmake`" --build `"$pluginBuild`" --parallel 4"
    Invoke-Native $command
    Write-Host ''
    Write-Host 'Artifacts:' -ForegroundColor Green
    Get-ChildItem $pluginBuild -Recurse -File -Include '*.vst3','Expressive Composer.exe' -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName
}
