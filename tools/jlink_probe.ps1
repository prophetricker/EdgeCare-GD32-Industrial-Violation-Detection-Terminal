param(
    [string]$JLinkExe = "D:\SEGGER\JLink_V948\JLink.exe",
    [string]$JLinkSerial = "",
    [string]$Device = "Cortex-M7",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 4000,
    [int]$TimeoutSec = 20
)

$ErrorActionPreference = "Stop"

function New-JLinkCommandFile {
    param([string[]]$Lines)

    $path = Join-Path $env:TEMP ("edgecare_jlink_{0}.jlink" -f ([Guid]::NewGuid().ToString("N")))
    $Lines | Set-Content -LiteralPath $path -Encoding ASCII
    return $path
}

function Get-JLinkUsbArgs {
    if($JLinkSerial.Trim().Length -gt 0) {
        return @("-USB", $JLinkSerial)
    }
    return @()
}

function Invoke-JLinkBounded {
    param(
        [string]$Name,
        [string[]]$Lines,
        [string[]]$ExtraArgs = @()
    )

    $script = New-JLinkCommandFile $Lines
    $stdout = Join-Path $env:TEMP ("edgecare_jlink_probe_{0}_{1}.stdout.log" -f $Name, ([Guid]::NewGuid().ToString("N")))
    $stderr = Join-Path $env:TEMP ("edgecare_jlink_probe_{0}_{1}.stderr.log" -f $Name, ([Guid]::NewGuid().ToString("N")))
    $process = $null
    try {
        $arguments = @("-NoGui", "1") + $ExtraArgs + @("-CommandFile", $script) + (Get-JLinkUsbArgs)
        $process = Start-Process -FilePath $JLinkExe `
                                 -ArgumentList $arguments `
                                 -NoNewWindow `
                                 -PassThru `
                                 -RedirectStandardOutput $stdout `
                                 -RedirectStandardError $stderr
        if(-not $process.WaitForExit($TimeoutSec * 1000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw "J-Link $Name timed out after $TimeoutSec seconds"
        }

        $output = @()
        if(Test-Path $stdout) {
            $output += Get-Content -LiteralPath $stdout
        }
        if(Test-Path $stderr) {
            $err = Get-Content -LiteralPath $stderr
            if($err) {
                $output += "--- stderr ---"
                $output += $err
            }
        }
        if(($null -ne $process.ExitCode) -and ($process.ExitCode -ne 0)) {
            $output | ForEach-Object { Write-Host $_ }
            throw "J-Link $Name failed with exit code $($process.ExitCode)"
        }
        return $output
    } finally {
        Remove-Item -LiteralPath $script,$stdout,$stderr -Force -ErrorAction SilentlyContinue
        if(($null -ne $process) -and (-not $process.HasExited)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

if(-not (Test-Path $JLinkExe)) {
    throw "JLink.exe not found: $JLinkExe"
}

Write-Host "== J-Link USB probe =="
$emuOutput = Invoke-JLinkBounded -Name "usb" -Lines @(
    "ShowEmuList",
    "Exit"
)
$emuOutput | ForEach-Object { Write-Host $_ }
if(-not ($emuOutput -match "J-Link\[0\]")) {
    throw "No USB J-Link probe found in ShowEmuList output"
}

Write-Host ""
Write-Host "== J-Link target probe =="
$targetOutput = Invoke-JLinkBounded -Name "target" `
    -Lines @(
        "h",
        "mem32 0xE000ED00,1",
        "mem32 0xE0042000,1",
        "g",
        "Exit"
    ) `
    -ExtraArgs @("-Device", $Device, "-If", $Interface, "-Speed", $SpeedKHz, "-AutoConnect", "1")
$targetOutput | ForEach-Object { Write-Host $_ }
if(-not ($targetOutput -match "Cortex-M7 identified")) {
    throw "Target probe did not identify Cortex-M7"
}
if(-not ($targetOutput -match "E000ED00 = 411FC272")) {
    throw "CPUID readback did not match expected Cortex-M7 CPUID"
}

Write-Host ""
Write-Host "J-Link probe passed: USB probe and Cortex-M7 target are reachable."
