param(
    [switch]$Flash,
    [string]$UV4 = "D:\Keil_v5\UV4\UV4.exe",
    [string]$Project = "",
    [string]$Target = "EdgeCare_GD32_Terminal",
    [string]$JLinkDevice = "GD32H759IMT6",
    [int]$TimeoutSec = 120,
    [string]$Log = ""
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$FirmwareDir = Join-Path $ProjectRoot "EdgeCare_GD32_Industrial_Violation_Terminal"
$DefaultProject = Join-Path $FirmwareDir "GD32H759I_START_Demo_Suites\Projects\01_EdgeCare_Industrial_Violation_Terminal\MDK-ARM\EdgeCare_GD32_Terminal.uvprojx"
$StaleArm7DeviceLine = 'Device="ARM7"'
$StaleCortexDeviceLine = 'Device="Cortex-M7"'
$ExpectedDeviceLine = 'Device="GD32H759IMT6"'

if([string]::IsNullOrWhiteSpace($Project)) {
    $Project = $DefaultProject
}
if([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $FirmwareDir "flash-keil.log"
}

if(-not (Test-Path $UV4)) {
    throw "UV4.exe not found: $UV4"
}
if(-not (Test-Path $Project)) {
    throw "Keil project not found: $Project"
}

$ResolvedProject = (Resolve-Path $Project).Path
$ProjectDir = Split-Path -Parent $ResolvedProject
$JLinkSettings = Join-Path $ProjectDir "JLinkSettings.ini"

Write-Host "Keil flash target:"
Write-Host "  Project: $ResolvedProject"
Write-Host "  Target: $Target"
Write-Host "  UV4: $UV4"
Write-Host "  JLinkSettings.ini: $JLinkSettings"
Write-Host "  J-Link Device: $JLinkDevice"
Write-Host "  TimeoutSec: $TimeoutSec"
Write-Host "  Log: $Log"

if(-not $Flash) {
    Write-Host ""
    Write-Host "Dry run: no flash performed. Re-run with -Flash to download via Keil."
    Write-Host "This script fixes stale local JLinkSettings.ini entries such as $StaleArm7DeviceLine before calling UV4 -f."
    exit 0
}

if(Test-Path $JLinkSettings) {
    $settings = [System.IO.File]::ReadAllText($JLinkSettings)
    $fixed = $settings
    if($fixed -match [regex]::Escape($StaleArm7DeviceLine)) {
        $fixed = $fixed -replace 'Device="ARM7"', ('Device="{0}"' -f $JLinkDevice)
    }
    if($fixed -match [regex]::Escape($StaleCortexDeviceLine)) {
        $fixed = $fixed -replace 'Device="Cortex-M7"', ('Device="{0}"' -f $JLinkDevice)
    }
    if($fixed -ne $settings) {
        [System.IO.File]::WriteAllText($JLinkSettings, $fixed, [System.Text.Encoding]::ASCII)
        Write-Host "Updated JLinkSettings.ini Device=`"$JLinkDevice`""
    }
} else {
    Write-Host "JLinkSettings.ini not found; Keil will create or use project defaults."
}

if(Test-Path $Log) {
    Remove-Item -LiteralPath $Log -Force
}

$arguments = "-f `"$ResolvedProject`" -t `"$Target`" -j0 -o `"$Log`""
Write-Host ""
Write-Host "== Keil flash =="
Write-Host "UV4 arguments: $arguments"

$process = Start-Process -FilePath $UV4 -ArgumentList $arguments -PassThru -WindowStyle Hidden
if(-not $process.WaitForExit($TimeoutSec * 1000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    throw "Keil flash timed out after $TimeoutSec seconds. See log: $Log"
}
$process.Refresh()

if(-not (Test-Path $Log)) {
    throw "Keil did not create flash log: $Log"
}

$output = Get-Content -LiteralPath $Log
$output | ForEach-Object { Write-Host $_ }
$text = $output -join "`n"
$ok = ($text -match "Erase Done") -and ($text -match "Programming Done") -and ($text -match "Verify OK")

if($ok) {
    Write-Host "keil_flash_ok=1"
} else {
    Write-Host "keil_flash_ok=0"
}

if((($null -ne $process.ExitCode) -and ($process.ExitCode -ne 0)) -and (-not $ok)) {
    throw "Keil flash failed with exit code $($process.ExitCode). See log: $Log"
}
if(-not $ok) {
    throw "Keil flash log did not contain Erase Done + Programming Done + Verify OK. See log: $Log"
}
