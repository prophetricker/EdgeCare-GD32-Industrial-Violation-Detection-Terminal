param(
    [string]$JLinkExe = "D:\SEGGER\JLink_V948\JLink.exe",
    [string]$Device = "Cortex-M7",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 4000
)

$ErrorActionPreference = "Stop"

function New-JLinkCommandFile {
    param([string[]]$Lines)

    $path = Join-Path $env:TEMP ("edgecare_jlink_{0}.jlink" -f ([Guid]::NewGuid().ToString("N")))
    $Lines | Set-Content -LiteralPath $path -Encoding ASCII
    return $path
}

if(-not (Test-Path $JLinkExe)) {
    throw "JLink.exe not found: $JLinkExe"
}

$emuScript = New-JLinkCommandFile @(
    "ShowEmuList",
    "Exit"
)

$targetScript = New-JLinkCommandFile @(
    "h",
    "mem32 0xE000ED00,1",
    "mem32 0xE0042000,1",
    "g",
    "Exit"
)

try {
    Write-Host "== J-Link USB probe =="
    $emuOutput = & $JLinkExe -NoGui 1 -CommandFile $emuScript 2>&1
    $emuOutput | ForEach-Object { Write-Host $_ }
    if($LASTEXITCODE -ne 0) {
        throw "J-Link USB probe failed with exit code $LASTEXITCODE"
    }
    if(-not ($emuOutput -match "J-Link\[0\]")) {
        throw "No USB J-Link probe found in ShowEmuList output"
    }

    Write-Host ""
    Write-Host "== J-Link target probe =="
    $targetOutput = & $JLinkExe -NoGui 1 -Device $Device -If $Interface -Speed $SpeedKHz -AutoConnect 1 -CommandFile $targetScript 2>&1
    $targetOutput | ForEach-Object { Write-Host $_ }
    if($LASTEXITCODE -ne 0) {
        throw "J-Link target probe failed with exit code $LASTEXITCODE"
    }
    if(-not ($targetOutput -match "Cortex-M7 identified")) {
        throw "Target probe did not identify Cortex-M7"
    }
    if(-not ($targetOutput -match "E000ED00 = 411FC272")) {
        throw "CPUID readback did not match expected Cortex-M7 CPUID"
    }

    Write-Host ""
    Write-Host "J-Link probe passed: USB probe and Cortex-M7 target are reachable."
} finally {
    Remove-Item -LiteralPath $emuScript,$targetScript -Force -ErrorAction SilentlyContinue
}
