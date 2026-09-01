[CmdletBinding()]
param(
    [string]$ConfigFile
)

$ErrorActionPreference = 'Stop'

$installPath = $PSScriptRoot
$apoDll = Join-Path $installPath 'EqualizerAPO.dll'
$deviceControl = Join-Path $installPath 'DeviceControl.exe'

$preMixClsid = '{EACD2258-FCAC-4FF4-B36D-419E924A6D79}'
$postMixClsid = '{EC1CC9CE-FAED-4822-828A-82A81A6F018F}'
$appRegistryPath = 'HKLM:\SOFTWARE\EqualizerAPO'
$productRegistryPath = 'HKLM:\SOFTWARE\EqualizerAPO-VST3'
$upstreamUninstallPath = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\EqualizerAPO'
$audioRegistryPath = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio'

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Test-ApoRegistration {
    param(
        [Parameter(Mandatory)]
        [string]$Clsid
    )

    $classPath = "HKLM:\SOFTWARE\Classes\CLSID\$Clsid\InprocServer32"
    $apoPath = "HKLM:\SOFTWARE\Classes\AudioEngine\AudioProcessingObjects\$Clsid"

    if (-not (Test-Path -LiteralPath $classPath) -or -not (Test-Path -LiteralPath $apoPath)) {
        return $false
    }

    $registeredDll = (Get-Item -LiteralPath $classPath).GetValue('')
    if ([string]::IsNullOrWhiteSpace($registeredDll)) {
        return $false
    }

    try {
        return [IO.Path]::GetFullPath($registeredDll) -eq [IO.Path]::GetFullPath($apoDll)
    }
    catch {
        return $false
    }
}

function Resolve-ConfigFile {
    param([string]$Path)

    $Path = $Path.Trim().Trim('"')
    if ($Path -notmatch '^(?:[A-Za-z]:[\\/]|\\\\)') {
        throw "The configuration file path must be absolute: $Path"
    }
    if ([IO.Path]::GetExtension($Path) -ine '.txt') {
        throw "The configuration file must have a .txt extension: $Path"
    }
    return [IO.Path]::GetFullPath($Path)
}

if ([string]::IsNullOrWhiteSpace($ConfigFile)) {
    $existingConfig = Get-ItemPropertyValue -Path $appRegistryPath -Name 'ConfigFile' -ErrorAction SilentlyContinue
    if ($existingConfig -and (Test-Path -LiteralPath $existingConfig -PathType Leaf)) {
        $ConfigFile = $existingConfig
        Write-Host "Keeping active configuration:`n$ConfigFile"
    }
    else {
        $documents = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
        $ConfigFile = Join-Path $documents 'EqualizerAPO-VST3\config.txt'
        Write-Host "Using a new default configuration:`n$ConfigFile"
    }
}
$ConfigFile = Resolve-ConfigFile -Path $ConfigFile

if (-not [Environment]::Is64BitOperatingSystem) {
    throw 'EqualizerAPO-VST3 requires 64-bit Windows.'
}

if (-not [Environment]::Is64BitProcess) {
    throw 'Run this installer with 64-bit PowerShell.'
}

if (-not (Test-Administrator)) {
    Write-Host 'Restarting the installer with administrator rights...'
    $powerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $elevationArguments = @(
        '-NoLogo'
        '-NoProfile'
        '-ExecutionPolicy'
        'Bypass'
        '-File'
        ('"{0}"' -f $PSCommandPath)
        '-ConfigFile'
        ('"{0}"' -f $ConfigFile.Replace('"', '\"'))
    )
    $elevated = Start-Process `
        -FilePath $powerShell `
        -ArgumentList $elevationArguments `
        -WorkingDirectory $installPath `
        -Verb RunAs `
        -Wait `
        -PassThru
    exit $elevated.ExitCode
}

$requiredPaths = @(
    $apoDll,
    $deviceControl,
    (Join-Path $installPath 'Editor.exe'),
    (Join-Path $installPath 'fftw3f.dll'),
    (Join-Path $installPath 'sndfile.dll'),
    (Join-Path $installPath 'Qt6Core.dll'),
    (Join-Path $installPath 'Qt6Gui.dll'),
    (Join-Path $installPath 'Qt6Widgets.dll'),
    (Join-Path $installPath 'qt\platforms\qwindows.dll'),
    (Join-Path $installPath 'ConfigControl.exe')
)

$missingPaths = $requiredPaths | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }
if ($missingPaths) {
    throw "The staged payload is incomplete. Missing:`n$($missingPaths -join "`n")"
}

if (Test-Path -LiteralPath $upstreamUninstallPath) {
    throw 'Equalizer APO is currently installed. Uninstall it before installing EqualizerAPO-VST3; the products use the same Windows audio registrations.'
}

if (-not (Test-Path -LiteralPath $ConfigFile -PathType Leaf)) {
    $configDirectory = Split-Path -Parent $ConfigFile
    New-Item -ItemType Directory -Path $configDirectory -Force | Out-Null
    $initialConfig = "# EqualizerAPO-VST3 configuration`r`nPreamp: 0 dB`r`n"
    [IO.File]::WriteAllText($ConfigFile, $initialConfig, [Text.UTF8Encoding]::new($false))
    Write-Host "Created initial configuration:`n$ConfigFile"
}

Write-Host "Preparing EqualizerAPO-VST3 at:`n$installPath"

if (-not (Test-Path -LiteralPath $productRegistryPath)) {
    New-Item -Path $productRegistryPath | Out-Null
}
New-ItemProperty -Path $productRegistryPath -Name 'InstallPath' -Value $installPath -PropertyType String -Force | Out-Null

# These values must exist before the APO or any utility initializes FilterEngine.
if (-not (Test-Path -LiteralPath $appRegistryPath)) {
    New-Item -Path $appRegistryPath | Out-Null
}
New-ItemProperty -Path $appRegistryPath -Name 'InstallPath' -Value $installPath -PropertyType String -Force | Out-Null
New-ItemProperty -Path $appRegistryPath -Name 'ConfigFile' -Value $ConfigFile -PropertyType String -Force | Out-Null
Remove-ItemProperty -Path $appRegistryPath -Name 'ConfigPath' -ErrorAction SilentlyContinue
New-ItemProperty -Path $appRegistryPath -Name 'EnableTrace' -Value 'false' -PropertyType String -Force | Out-Null

# Equalizer APO expects the non-protected Windows audio engine so its third-party APO can load.
if (-not (Test-Path -LiteralPath $audioRegistryPath)) {
    New-Item -Path $audioRegistryPath | Out-Null
}
New-ItemProperty -Path $audioRegistryPath -Name 'DisableProtectedAudioDG' -Value 1 -PropertyType DWord -Force | Out-Null

$preMixRegistered = Test-ApoRegistration -Clsid $preMixClsid
$postMixRegistered = Test-ApoRegistration -Clsid $postMixClsid
if (-not ($preMixRegistered -and $postMixRegistered)) {
    Write-Host 'Registering EqualizerAPO.dll...'
    $regsvr32 = Join-Path $env:SystemRoot 'System32\regsvr32.exe'
    $registrationProcess = Start-Process `
        -FilePath $regsvr32 `
        -ArgumentList @('/s', ('"{0}"' -f $apoDll)) `
        -Wait `
        -PassThru
    if ($registrationProcess.ExitCode -ne 0) {
        throw "regsvr32 failed with exit code $($registrationProcess.ExitCode)."
    }
}
else {
    Write-Host 'EqualizerAPO.dll is already registered at the correct path.'
}

if (-not (Test-ApoRegistration -Clsid $preMixClsid) -or -not (Test-ApoRegistration -Clsid $postMixClsid)) {
    throw 'APO registration verification failed.'
}

Write-Host 'Application paths and APO registration are ready.'
Write-Host 'EqualizerAPO-VST3 installation completed successfully. Manage audio endpoints from Configuration Editor.'
