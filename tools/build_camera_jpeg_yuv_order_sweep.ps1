param(
    [switch]$Build
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$FirmwareDir = Join-Path $ProjectRoot "EdgeCare_GD32_Industrial_Violation_Terminal"
$BuildScript = Join-Path $FirmwareDir "build_keil.ps1"
$BuildLog = Join-Path $FirmwareDir "build-keil.log"
$BoardConfig = Join-Path $FirmwareDir "GD32H759I_START_Demo_Suites\Projects\01_EdgeCare_Industrial_Violation_Terminal\board\board_config.h"
$Artifact = Join-Path $FirmwareDir "GD32H759I_START_Demo_Suites\Projects\01_EdgeCare_Industrial_Violation_Terminal\MDK-ARM\Objects\EdgeCare_GD32_Terminal.axf"
$ManifestDir = Join-Path $ProjectRoot ".embeddedskills\build"
$ManifestPath = Join-Path $ManifestDir "camera_jpeg_yuv_order_sweep_manifest.json"
$MacroName = "EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP"
$Pattern = "(?m)^(\s*#define\s+$MacroName\s+)0U(\s*)$"
$Replacement = "`${1}1U`${2}"

Write-Host "EdgeCare JPEG-to-YUV 0x4745 order-sweep diagnostic build:"
Write-Host "  Build: $($Build.IsPresent)"
Write-Host "  Macro: $MacroName"
Write-Host "  Board config: $BoardConfig"
Write-Host "  This script never flashes the board."

if(-not $Build) {
    Write-Host ""
    Write-Host "Dry run: no file changes and no build performed."
    Write-Host "Re-run with -Build to temporarily enable the sweep, build once, and restore board_config.h."
    exit 0
}

if(-not (Test-Path $BoardConfig)) {
    throw "board_config.h not found: $BoardConfig"
}

$OriginalText = [System.IO.File]::ReadAllText($BoardConfig)
if($OriginalText -notmatch $Pattern) {
    throw "Expected default macro line not found: #define $MacroName 0U"
}

$DiagnosticText = [regex]::Replace($OriginalText, $Pattern, $Replacement, 1)
try {
    [System.IO.File]::WriteAllText($BoardConfig, $DiagnosticText, [System.Text.UTF8Encoding]::new($false))

    Write-Host ""
    Write-Host "== Build diagnostic firmware =="
    Write-Host "Temporarily set $MacroName=1U for this build."
    powershell.exe -ExecutionPolicy Bypass -File $BuildScript
    if($LASTEXITCODE -ne 0) {
        throw "Diagnostic build failed with exit code $LASTEXITCODE"
    }
    if(-not (Test-Path $Artifact)) {
        throw "Build artifact not found: $Artifact"
    }

    $ArtifactInfo = Get-Item -LiteralPath $Artifact
    $BuildSummary = @()
    if(Test-Path $BuildLog) {
        $BuildSummary = Select-String -Path $BuildLog -Pattern "Program Size|Error\(s\)|Warning\(s\)" | ForEach-Object { $_.Line }
    }
    New-Item -ItemType Directory -Force -Path $ManifestDir | Out-Null
    $Manifest = [ordered]@{
        kind = "camera_jpeg_yuv_order_sweep"
        sweep_enabled = 1
        macro = $MacroName
        artifact = $Artifact
        artifact_length = $ArtifactInfo.Length
        artifact_last_write_utc = $ArtifactInfo.LastWriteTimeUtc.ToString("o")
        built_at_utc = (Get-Date).ToUniversalTime().ToString("o")
        build_summary = $BuildSummary
    }
    $ManifestJson = $Manifest | ConvertTo-Json -Depth 4
    [System.IO.File]::WriteAllText($ManifestPath, $ManifestJson, [System.Text.UTF8Encoding]::new($false))

    Write-Host ""
    Write-Host "Diagnostic build complete. Flash manually only if explicitly intended."
    Write-Host "Diagnostic manifest: $ManifestPath"
    Write-Host "Expected boot log should include: jpeg_yuv_order_capture_sweep=1"
    Write-Host "After capture, summarize serial log with:"
    Write-Host "  py .\tools\analyze_camera_byte_scale_log.py <reset-log.txt>"
} finally {
    [System.IO.File]::WriteAllText($BoardConfig, $OriginalText, [System.Text.UTF8Encoding]::new($false))
    Write-Host ""
    Write-Host "Restored board_config.h to its original default state."
}
