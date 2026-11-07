param(
    [switch]$WithClap
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$third = Join-Path $repo 'third_party'
$downloads = Join-Path $third 'downloads'
New-Item -ItemType Directory -Force $third, $downloads | Out-Null

$juceDir = Join-Path $third 'JUCE'
if (-not (Test-Path (Join-Path $juceDir 'CMakeLists.txt'))) {
    $archive = Join-Path $downloads 'juce-8.0.6.tar.gz'
    if (-not (Test-Path $archive)) {
        Write-Host 'Downloading JUCE 8.0.6...'
        Invoke-WebRequest -Uri 'https://codeload.github.com/juce-framework/JUCE/tar.gz/refs/tags/8.0.6' -OutFile $archive -TimeoutSec 600
    }
    Write-Host 'Extracting JUCE...'
    tar -xzf $archive -C $third
    $extracted = Get-ChildItem $third -Directory -Filter 'JUCE-*' | Select-Object -First 1
    if (-not $extracted) { throw 'JUCE extraction failed' }
    Move-Item $extracted.FullName $juceDir
    Write-Host "JUCE ready: $juceDir"
} else {
    Write-Host "JUCE already present: $juceDir"
}

if ($WithClap) {
    $clap = Join-Path $third 'clap-juce-extensions'
    if (-not (Test-Path (Join-Path $clap 'CMakeLists.txt'))) {
        git clone --depth 1 https://github.com/free-audio/clap-juce-extensions.git $clap
    }
    Push-Location $clap
    try {
        git submodule update --init --recursive
    } finally {
        Pop-Location
    }
    Write-Host 'CLAP extension submodules initialized. Configure with -DEC_BUILD_CLAP=ON.'
}

Get-FileHash (Join-Path $downloads 'juce-8.0.6.tar.gz') -Algorithm SHA256 -ErrorAction SilentlyContinue |
    Format-List
