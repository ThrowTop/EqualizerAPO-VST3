[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuildDir
)

$ErrorActionPreference = 'SilentlyContinue'
$buildRoot = [IO.Path]::GetFullPath($BuildDir).TrimEnd('\')

function Set-HiddenDirectory([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Container)) {
        return
    }

    $item = Get-Item -LiteralPath $Path -Force
    $item.Attributes = $item.Attributes -bor [IO.FileAttributes]::Hidden
}

# Only the top-level Visual Studio target directories clutter the build root.
Get-ChildItem -LiteralPath $buildRoot -Directory -Force |
    Where-Object Name -Like '*.dir' |
    ForEach-Object { Set-HiddenDirectory $_.FullName }

$generatedDirectoryNames = @(
    '.intermediate',
    '.manual-install',
    '.qt',
    '.staging',
    '.vst3sdk',
    '.vs',
    'apps',
    'CMakeFiles',
    'Debug',
    'Release',
    'src',
    'tests',
    'Testing',
    'third_party',
    'vcpkg_installed',
    'VST3',
    'WIN_PDB64',
    'x64'
)
foreach ($name in $generatedDirectoryNames) {
    Set-HiddenDirectory (Join-Path $buildRoot $name)
}
