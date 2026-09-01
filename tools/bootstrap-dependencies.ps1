[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $projectRoot '.deps'
$vcpkgRoot = Join-Path $dependencyRoot 'vcpkg'
$vst3Root = Join-Path $dependencyRoot 'vst3sdk'
$pythonRoot = Join-Path $dependencyRoot 'python'
$qtRoot = Join-Path $dependencyRoot 'Qt'
$qtVersion = '6.7.2'
$qtArchitecture = 'win64_msvc2019_64'
$vcpkgCommit = '127402f1c75bb3d5ff6bce04b285faa4930a5aca'
$vst3Commit = '3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96'

function Assert-LastExitCode {
    param([Parameter(Mandatory)][string]$Operation)

    if ($LASTEXITCODE -ne 0) {
        throw "$Operation failed with exit code $LASTEXITCODE."
    }
}

if (-not (Get-Command git.exe -ErrorAction SilentlyContinue)) {
    throw 'Git is required to bootstrap vcpkg.'
}

$pythonLauncher = Get-Command py.exe -ErrorAction SilentlyContinue
$pythonArguments = @('-3')
if (-not $pythonLauncher) {
    $pythonLauncher = Get-Command python.exe -ErrorAction SilentlyContinue
    $pythonArguments = @()
}
if (-not $pythonLauncher) {
    throw 'Python 3 is required to bootstrap the Qt downloader.'
}

New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null

if (-not (Test-Path -LiteralPath $vcpkgRoot)) {
    & git.exe clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
    Assert-LastExitCode 'Cloning vcpkg'
}
elseif (-not (Test-Path -LiteralPath (Join-Path $vcpkgRoot '.git'))) {
    throw "$vcpkgRoot exists but is not a vcpkg Git checkout."
}

& git.exe -C $vcpkgRoot checkout --detach $vcpkgCommit
Assert-LastExitCode 'Selecting the pinned vcpkg revision'

& (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
Assert-LastExitCode 'Bootstrapping vcpkg'

if (-not (Test-Path -LiteralPath $vst3Root)) {
    & git.exe clone --recursive https://github.com/steinbergmedia/vst3sdk.git $vst3Root
    Assert-LastExitCode 'Cloning the VST3 SDK'
}
elseif (-not (Test-Path -LiteralPath (Join-Path $vst3Root '.git'))) {
    throw "$vst3Root exists but is not a VST3 SDK Git checkout."
}

& git.exe -C $vst3Root checkout --detach $vst3Commit
Assert-LastExitCode 'Selecting the pinned VST3 SDK revision'
& git.exe -C $vst3Root submodule update --init --recursive
Assert-LastExitCode 'Updating VST3 SDK submodules'

$venvPython = Join-Path $pythonRoot 'Scripts\python.exe'
if (-not (Test-Path -LiteralPath $venvPython)) {
    & $pythonLauncher.Source @pythonArguments -m venv $pythonRoot
    Assert-LastExitCode 'Creating the local Python environment'
}

& $venvPython -m pip install --disable-pip-version-check 'aqtinstall==3.3.0'
Assert-LastExitCode 'Installing aqtinstall'

$qtCMakeConfig = Join-Path $qtRoot "$qtVersion\msvc2019_64\lib\cmake\Qt6\Qt6Config.cmake"
if (-not (Test-Path -LiteralPath $qtCMakeConfig)) {
    & $venvPython -m aqt install-qt windows desktop $qtVersion $qtArchitecture `
        --outputdir $qtRoot -m qtsvg
    Assert-LastExitCode 'Installing Qt'
}

Write-Host 'Dependencies are ready. Configure with: cmake --preset default'
