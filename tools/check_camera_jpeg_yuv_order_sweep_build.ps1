$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$FirmwareDir = Join-Path $ProjectRoot "EdgeCare_GD32_Industrial_Violation_Terminal"
$Artifact = Join-Path $FirmwareDir "GD32H759I_START_Demo_Suites\Projects\01_EdgeCare_Industrial_Violation_Terminal\MDK-ARM\Objects\EdgeCare_GD32_Terminal.axf"
$ManifestPath = Join-Path $ProjectRoot ".embeddedskills\build\camera_jpeg_yuv_order_sweep_manifest.json"

Write-Host "EdgeCare JPEG-to-YUV 0x4745 order-sweep artifact check:"
Write-Host "  Artifact: $Artifact"
Write-Host "  Manifest: $ManifestPath"

if(-not (Test-Path $Artifact)) {
    Write-Host "artifact_exists=0"
    Write-Host "manifest_exists=$([int](Test-Path $ManifestPath))"
    Write-Host "current_artifact_matches_manifest=0"
    Write-Host "ready_to_flash_sweep=0"
    exit 1
}

if(-not (Test-Path $ManifestPath)) {
    Write-Host "artifact_exists=1"
    Write-Host "manifest_exists=0"
    Write-Host "current_artifact_matches_manifest=0"
    Write-Host "ready_to_flash_sweep=0"
    exit 1
}

$ArtifactInfo = Get-Item -LiteralPath $Artifact
$Manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
$SweepEnabled = [int]$Manifest.sweep_enabled
$LengthMatches = ([int64]$Manifest.artifact_length -eq [int64]$ArtifactInfo.Length)
$TimeMatches = ([string]$Manifest.artifact_last_write_utc -eq $ArtifactInfo.LastWriteTimeUtc.ToString("o"))
$Matches = ($SweepEnabled -eq 1) -and $LengthMatches -and $TimeMatches

Write-Host "artifact_exists=1"
Write-Host "manifest_exists=1"
Write-Host "sweep_enabled=$SweepEnabled"
Write-Host "manifest_artifact_length=$($Manifest.artifact_length)"
Write-Host "current_artifact_length=$($ArtifactInfo.Length)"
Write-Host "manifest_artifact_last_write_utc=$($Manifest.artifact_last_write_utc)"
Write-Host "current_artifact_last_write_utc=$($ArtifactInfo.LastWriteTimeUtc.ToString("o"))"
Write-Host "current_artifact_matches_manifest=$([int]$Matches)"

if($Matches) {
    Write-Host "ready_to_flash_sweep=1"
    exit 0
}

Write-Host "ready_to_flash_sweep=0"
exit 1
