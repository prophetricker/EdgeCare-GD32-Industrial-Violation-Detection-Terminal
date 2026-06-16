param(
    [switch]$Flash,
    [string]$Port = "COM8",
    [int]$Baudrate = 115200,
    [int]$SerialDurationSec = 35,
    [string]$JLinkSerial = "",
    [string]$Device = "GD32H759IMT6",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 100,
    [uint32]$FrameAddress = 0x24000040,
    [uint32]$FrameBytes = 153600,
    [int]$TimeoutSec = 120
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$FrameDir = Join-Path $ProjectRoot "data\photo_evidence_real_raw8"
$FramePath = Join-Path $FrameDir ("edgecare_real_raw8_frame_{0}.bin" -f $Stamp)
$DecodeDir = Join-Path $FrameDir ("decoded_{0}" -f $Stamp)

Write-Host "EdgeCare camera photo-evidence run:"
Write-Host "  Flash: $($Flash.IsPresent)"
Write-Host "  Port: $Port @ $Baudrate"
Write-Host "  Device: $Device"
Write-Host "  JLinkSerial: $(if($JLinkSerial.Trim().Length -gt 0) { $JLinkSerial } else { '<auto>' })"
Write-Host "  SWD speed: $SpeedKHz kHz"
Write-Host "  Frame: $FramePath"
Write-Host "  Decode: $DecodeDir"

if(-not $Flash) {
    Write-Host ""
    Write-Host "Dry run: no flash, reset, serial capture, or RAM read performed."
    Write-Host "Re-run with -Flash only after the user explicitly authorizes programming the board."
    exit 0
}

New-Item -ItemType Directory -Force -Path $FrameDir | Out-Null

Write-Host ""
Write-Host "== Flash firmware =="
powershell.exe -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "jlink_flash_edgecare.ps1") `
    -Flash `
    -Device $Device `
    -Interface $Interface `
    -SpeedKHz $SpeedKHz `
    -JLinkSerial $JLinkSerial `
    -TimeoutSec $TimeoutSec
if($LASTEXITCODE -ne 0) {
    throw "Firmware flash failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "== Reset and capture serial =="
powershell.exe -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "jlink_reset_capture_serial.ps1") `
    -Port $Port `
    -Baudrate $Baudrate `
    -DurationSec $SerialDurationSec `
    -Device $Device `
    -Interface $Interface `
    -SpeedKHz $SpeedKHz `
    -JLinkSerial $JLinkSerial
if($LASTEXITCODE -ne 0) {
    throw "Serial capture failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "== Save camera RAM frame =="
powershell.exe -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "jlink_save_camera_frame.ps1") `
    -Output $FramePath `
    -Address $FrameAddress `
    -Bytes $FrameBytes `
    -Device $Device `
    -Interface $Interface `
    -SpeedKHz $SpeedKHz `
    -JLinkSerial $JLinkSerial `
    -TimeoutSec $TimeoutSec
if($LASTEXITCODE -ne 0) {
    throw "Frame save failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "== Decode frame candidates =="
python (Join-Path $PSScriptRoot "decode_camera_frame.py") $FramePath --out $DecodeDir
if($LASTEXITCODE -ne 0) {
    throw "Frame decode failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "Photo-evidence run complete."
Write-Host "Inspect representative candidates in: $DecodeDir"
