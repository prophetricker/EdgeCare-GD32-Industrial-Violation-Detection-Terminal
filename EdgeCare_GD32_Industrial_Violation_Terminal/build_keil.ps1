$ErrorActionPreference = "Stop"

$uv4 = "D:\Keil_v5\UV4\UV4.exe"
$project = Join-Path $PSScriptRoot "GD32H759I_START_Demo_Suites\Projects\01_EdgeCare_Industrial_Violation_Terminal\MDK-ARM\EdgeCare_GD32_Terminal.uvprojx"
$target = "EdgeCare_GD32_Terminal"
$log = Join-Path $PSScriptRoot "build-keil.log"

if (-not (Test-Path $uv4)) {
    throw "UV4.exe not found: $uv4"
}

if (-not (Test-Path $project)) {
    throw "Keil project not found: $project"
}

if (Test-Path $log) {
    Remove-Item -LiteralPath $log -Force
}

$arguments = "-b `"$project`" -t `"$target`" -j0 -o `"$log`""
$process = Start-Process -FilePath $uv4 -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
Write-Host "UV4 exit code: $($process.ExitCode)"

if (Test-Path $log) {
    Get-Content $log
} else {
    Write-Warning "Keil did not create log file: $log"
}
