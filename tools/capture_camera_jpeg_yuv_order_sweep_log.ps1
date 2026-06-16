param(
    [string]$Port = "COM8",
    [int]$Baudrate = 115200,
    [int]$DurationSec = 60,
    [string]$JLinkExe = "D:\SEGGER\JLink_V948\JLink.exe",
    [string]$JLinkSerial = "",
    [string]$Device = "GD32H759IMT6",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 4000,
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"

$CheckScript = Join-Path $PSScriptRoot "check_camera_jpeg_yuv_order_sweep_build.ps1"
$CaptureScript = Join-Path $PSScriptRoot "jlink_reset_capture_serial.ps1"
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

Write-Host "EdgeCare JPEG-to-YUV 0x4745 order-sweep capture helper:"
Write-Host "  This script does not build firmware and does not flash the board."
Write-Host "  It only verifies the current AXF is the sweep diagnostic artifact, then captures reset serial."

Write-Host ""
Write-Host "== Check current sweep artifact =="
$checkOutput = powershell.exe -ExecutionPolicy Bypass -File $CheckScript 2>&1
$checkOutput | ForEach-Object { Write-Host $_ }
if($LASTEXITCODE -ne 0 -or -not ($checkOutput -match "ready_to_flash_sweep=1")) {
    throw "Current AXF is not the JPEG-to-YUV order-sweep diagnostic artifact. Re-run the dedicated sweep build helper before manual flash/capture."
}

Write-Host ""
Write-Host "== Capture reset serial =="
$captureArgs = @(
    "-ExecutionPolicy", "Bypass",
    "-File", $CaptureScript,
    "-Port", $Port,
    "-Baudrate", $Baudrate,
    "-DurationSec", $DurationSec,
    "-JLinkExe", $JLinkExe,
    "-JLinkSerial", $JLinkSerial,
    "-Device", $Device,
    "-Interface", $Interface,
    "-SpeedKHz", $SpeedKHz
)
if(-not [string]::IsNullOrWhiteSpace($Output)) {
    $captureArgs += @("-Output", $Output)
}

$captureOutput = powershell.exe @captureArgs 2>&1
$captureOutput | ForEach-Object { Write-Host $_ }
if($LASTEXITCODE -ne 0) {
    throw "Reset serial capture failed with exit code $LASTEXITCODE"
}

$savedLine = $captureOutput | Where-Object { $_ -match "^Saved log: " } | Select-Object -Last 1
if(-not $savedLine) {
    throw "Could not determine saved reset log path from capture output."
}
$savedLog = [string]$savedLine
$savedLog = $savedLog.Substring("Saved log: ".Length).Trim()

if(-not ($captureOutput -match "jpeg_yuv_order_capture_sweep=1")) {
    throw "Captured reset log did not confirm jpeg_yuv_order_capture_sweep=1. Verify the diagnostic firmware was manually flashed."
}

Write-Host ""
Write-Host "== Analyze byte-scale sweep log =="
Push-Location $ProjectRoot
try {
    py .\tools\analyze_camera_byte_scale_log.py $savedLog
    if($LASTEXITCODE -ne 0) {
        throw "analyze_camera_byte_scale_log.py failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}
