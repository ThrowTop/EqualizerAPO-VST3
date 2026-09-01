[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$SourceDir,

    [Parameter(Mandatory)]
    [string]$InstallDir,

    [string]$ConfigFile,

    [string]$LogFile
)

$ErrorActionPreference = 'Stop'
$appRegistryPath = 'HKLM:\SOFTWARE\EqualizerAPO'

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Quote-ProcessArgument([string]$Value) {
    return '"' + $Value.Replace('"', '\"') + '"'
}

$SourceDir = [IO.Path]::GetFullPath($SourceDir)
$InstallDir = [IO.Path]::GetFullPath($InstallDir)

if ([string]::IsNullOrWhiteSpace($ConfigFile)) {
    $ConfigFile = Get-ItemPropertyValue -Path $appRegistryPath -Name 'ConfigFile' -ErrorAction SilentlyContinue
}
if ([string]::IsNullOrWhiteSpace($ConfigFile)) {
    throw 'ConfigFile is not set. Pass -ConfigFile C:\path\to\your-config.txt on the first deployment.'
}
$ConfigFile = $ConfigFile.Trim().Trim('"')

if ($ConfigFile -notmatch '^(?:[A-Za-z]:[\\/]|\\\\)' -or
    [IO.Path]::GetExtension($ConfigFile) -ine '.txt' -or
    -not (Test-Path -LiteralPath $ConfigFile -PathType Leaf)) {
    throw "ConfigFile must be an existing absolute .txt file: $ConfigFile"
}
$ConfigFile = [IO.Path]::GetFullPath($ConfigFile)

if (-not (Test-Administrator)) {
    Write-Host 'Requesting administrator rights for development deployment...'
    $powerShell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    if ([string]::IsNullOrWhiteSpace($LogFile)) {
        $LogFile = Join-Path ([IO.Path]::GetDirectoryName($InstallDir)) 'deploy-dev.log'
    }
    Remove-Item -LiteralPath $LogFile -Force -ErrorAction SilentlyContinue
    $arguments = @(
        '-NoLogo', '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', (Quote-ProcessArgument $PSCommandPath),
        '-SourceDir', (Quote-ProcessArgument $SourceDir),
        '-InstallDir', (Quote-ProcessArgument $InstallDir),
        '-ConfigFile', (Quote-ProcessArgument $ConfigFile),
        '-LogFile', (Quote-ProcessArgument $LogFile)
    )
    $process = Start-Process -FilePath $powerShell -ArgumentList $arguments -Verb RunAs -Wait -PassThru
    if ($process.ExitCode -ne 0 -and (Test-Path -LiteralPath $LogFile -PathType Leaf)) {
        Get-Content -LiteralPath $LogFile | Write-Host
    }
    elseif ($process.ExitCode -eq 0) {
        Remove-Item -LiteralPath $LogFile -Force -ErrorAction SilentlyContinue
        Write-Host 'Development deployment complete. No reboot is required.'
        Write-Host "Active configuration: $ConfigFile"
    }
    exit $process.ExitCode
}

if ([string]::IsNullOrWhiteSpace($LogFile)) {
    $LogFile = Join-Path ([IO.Path]::GetDirectoryName($InstallDir)) 'deploy-dev.log'
}
Start-Transcript -LiteralPath $LogFile -Force | Out-Null
trap {
    Write-Error $_
    Stop-Transcript -ErrorAction SilentlyContinue | Out-Null
    exit 1
}

$requiredFiles = @(
    'EqualizerAPO.dll',
    'Editor.exe',
    'DeviceControl.exe',
    'ConfigControl.exe'
)
foreach ($requiredFile in $requiredFiles) {
    $path = Join-Path $SourceDir $requiredFile
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "The staged runtime is incomplete: $path"
    }
}

if ($SourceDir.TrimEnd('\') -ieq $InstallDir.TrimEnd('\')) {
    throw 'The development install directory must differ from the disposable staging directory.'
}

$projectRoot = [IO.Path]::GetFullPath((Join-Path $InstallDir '..\..')).TrimEnd('\')
$runningApps = Get-Process -Name Editor,Benchmark,ConfigControl,DeviceControl -ErrorAction SilentlyContinue |
    Where-Object {
        try {
            -not [string]::IsNullOrWhiteSpace($_.Path) -and
                [IO.Path]::GetFullPath($_.Path).StartsWith($projectRoot, [StringComparison]::OrdinalIgnoreCase)
        }
        catch {
            $false
        }
    }
if ($runningApps) {
    Write-Host "Stopping repository utilities that block deployment (PID: $(($runningApps.Id | Sort-Object) -join ', '))..."
    $runningApps | Stop-Process -Force
    $runningApps | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
}

$activeDependents = @(
    Get-Service -Name AudioSrv | ForEach-Object {
        $_.DependentServices | Where-Object Status -eq Running | Select-Object -ExpandProperty Name
    }
)
$audioWasRunning = (Get-Service -Name AudioSrv).Status -eq 'Running'

Write-Host 'Stopping Windows Audio so EqualizerAPO.dll can be replaced...'
if ($audioWasRunning) {
    Stop-Service -Name AudioSrv -Force
}

try {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $InstallDir 'VSTPlugins') -Force | Out-Null

    Write-Host "Deploying:`n$SourceDir`n-> $InstallDir"
    & robocopy.exe $SourceDir $InstallDir /MIR /R:2 /W:1 /NFL /NDL /NJH /NJS /NP /XD VSTPlugins VST3 | Out-Host
    if ($LASTEXITCODE -gt 7) {
        throw "robocopy failed with exit code $LASTEXITCODE."
    }

    $apoDll = Join-Path $InstallDir 'EqualizerAPO.dll'
    $regsvr32 = Join-Path $env:SystemRoot 'System32\regsvr32.exe'
    $registration = Start-Process -FilePath $regsvr32 -ArgumentList @('/s', (Quote-ProcessArgument $apoDll)) -Wait -PassThru
    if ($registration.ExitCode -ne 0) {
        throw "regsvr32 failed with exit code $($registration.ExitCode)."
    }

    if (-not (Test-Path -LiteralPath $appRegistryPath)) {
        New-Item -Path $appRegistryPath | Out-Null
    }
    New-ItemProperty -Path $appRegistryPath -Name InstallPath -Value $InstallDir -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $appRegistryPath -Name ConfigFile -Value $ConfigFile -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $appRegistryPath -Name EnableTrace -Value 'false' -PropertyType String -Force | Out-Null
    Remove-ItemProperty -Path $appRegistryPath -Name ConfigPath -ErrorAction SilentlyContinue

    $audioRegistryPath = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Audio'
    if (-not (Test-Path -LiteralPath $audioRegistryPath)) {
        New-Item -Path $audioRegistryPath | Out-Null
    }
    New-ItemProperty -Path $audioRegistryPath -Name DisableProtectedAudioDG -Value 1 -PropertyType DWord -Force | Out-Null
}
finally {
    if ($audioWasRunning) {
        Write-Host 'Starting Windows Audio...'
        Start-Service -Name AudioSrv
        foreach ($serviceName in $activeDependents) {
            Start-Service -Name $serviceName -ErrorAction Continue
        }
    }
}

$sourceHash = (Get-FileHash -LiteralPath (Join-Path $SourceDir 'EqualizerAPO.dll') -Algorithm SHA256).Hash
$installedHash = (Get-FileHash -LiteralPath (Join-Path $InstallDir 'EqualizerAPO.dll') -Algorithm SHA256).Hash
if ($sourceHash -ne $installedHash) {
    throw 'Deployment verification failed: installed EqualizerAPO.dll does not match the build.'
}

Write-Host 'Development deployment complete. No reboot is required.'
Write-Host "Active configuration: $ConfigFile"
Stop-Transcript | Out-Null
