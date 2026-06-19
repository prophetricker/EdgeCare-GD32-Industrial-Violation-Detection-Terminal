param(
    [switch]$Flash,
    [string]$UV4 = "D:\Keil_v5\UV4\UV4.exe",
    [string]$Project = "",
    [string]$Target = "EdgeCare_GD32_Terminal",
    [int]$TimeoutSec = 180,
    [string]$Log = ""
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$FirmwareDir = Join-Path $ProjectRoot "EdgeCare_GD32_Industrial_Violation_Terminal"
$DefaultProject = Join-Path $FirmwareDir "GD32H759I_START_Demo_Suites\Projects\01_EdgeCare_Industrial_Violation_Terminal\MDK-ARM\EdgeCare_GD32_Terminal.uvprojx"

if([string]::IsNullOrWhiteSpace($Project)) {
    $Project = $DefaultProject
}
if([string]::IsNullOrWhiteSpace($Log)) {
    $Log = Join-Path $FirmwareDir "flash-gdlink-keil.log"
}

if(-not (Test-Path $UV4)) {
    throw "UV4.exe not found: $UV4"
}
if(-not (Test-Path $Project)) {
    throw "Keil project not found: $Project"
}

$ResolvedProject = (Resolve-Path $Project).Path

Write-Host "Keil GD-Link/CMSIS-DAP flash target:"
Write-Host "  Project: $ResolvedProject"
Write-Host "  Target: $Target"
Write-Host "  UV4: $UV4"
Write-Host "  TimeoutSec: $TimeoutSec"
Write-Host "  Log: $Log"
Write-Host ""
Write-Host "This script uses the debugger/download driver already saved in the Keil project."
Write-Host "Current expected setup: CMSIS-DAP/GD-Link + GD32H7xx_3840KB flash algorithm."

if(-not $Flash) {
    Write-Host ""
    Write-Host "Dry run: no flash performed. Re-run with -Flash to download via Keil/GD-Link."
    exit 0
}

if(Test-Path $Log) {
    Remove-Item -LiteralPath $Log -Force
}

$arguments = "-f `"$ResolvedProject`" -t `"$Target`" -j0 -o `"$Log`""
Write-Host ""
Write-Host "== Keil GD-Link flash =="
Write-Host "UV4 arguments: $arguments"

$process = Start-Process -FilePath $UV4 -ArgumentList $arguments -PassThru -WindowStyle Hidden
if(-not $process.WaitForExit($TimeoutSec * 1000)) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    throw "Keil GD-Link flash timed out after $TimeoutSec seconds. See log: $Log"
}
$process.Refresh()
Write-Host "UV4 exit code: $($process.ExitCode)"

if(-not (Test-Path $Log)) {
    throw "Keil did not create flash log: $Log"
}

$output = Get-Content -LiteralPath $Log
$output | ForEach-Object { Write-Host $_ }
$text = $output -join "`n"
$ok = ($text -match "Erase Done") -and ($text -match "Programming Done") -and ($text -match "Verify OK")

if($ok) {
    Write-Host "keil_gdlink_flash_ok=1"
} else {
    Write-Host "keil_gdlink_flash_ok=0"
}

if((($null -ne $process.ExitCode) -and ($process.ExitCode -ne 0)) -and (-not $ok)) {
    throw "Keil GD-Link flash failed with exit code $($process.ExitCode). See log: $Log"
}
if(-not $ok) {
    throw "Keil GD-Link flash log did not contain Erase Done + Programming Done + Verify OK. See log: $Log"
}
