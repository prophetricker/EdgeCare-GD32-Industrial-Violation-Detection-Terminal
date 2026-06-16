param(
    [string]$JLinkExe = "D:\SEGGER\JLink_V948\JLink.exe",
    [string]$JLinkSerial = "",
    [string]$Device = "GD32H759IMT6",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 100,
    [string]$Output = "",
    [uint32]$Address = 0x24000040,
    [uint32]$Bytes = 153600,
    [int]$TimeoutSec = 30
)

$ErrorActionPreference = "Stop"

function New-JLinkCommandFile {
    param([string[]]$Lines)

    $path = Join-Path $env:TEMP ("edgecare_jlink_save_frame_{0}.jlink" -f ([Guid]::NewGuid().ToString("N")))
    $Lines | Set-Content -LiteralPath $path -Encoding ASCII
    return $path
}

function Get-JLinkUsbArgs {
    if($JLinkSerial.Trim().Length -gt 0) {
        return @("-USB", $JLinkSerial)
    }
    return @()
}

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
if([string]::IsNullOrWhiteSpace($Output)) {
    $outDir = Join-Path $ProjectRoot "data\photo_evidence_real_raw8"
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $Output = Join-Path $outDir ("edgecare_real_raw8_frame_{0}.bin" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
}
$LogDir = Join-Path $ProjectRoot ".embeddedskills\logs\jlink"

if(-not (Test-Path $JLinkExe)) {
    throw "JLink.exe not found: $JLinkExe"
}

$ResolvedOutputDir = (New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Output)).FullName
$ResolvedOutput = Join-Path $ResolvedOutputDir (Split-Path -Leaf $Output)
$ResolvedLogDir = (New-Item -ItemType Directory -Force -Path $LogDir).FullName
$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$Stdout = Join-Path $ResolvedLogDir ("edgecare_jlink_save_frame_{0}.stdout.log" -f $Stamp)
$Stderr = Join-Path $ResolvedLogDir ("edgecare_jlink_save_frame_{0}.stderr.log" -f $Stamp)
$script = New-JLinkCommandFile @(
    "h",
    ("savebin ""{0}"",0x{1:X8},0x{2:X}" -f $ResolvedOutput, $Address, $Bytes),
    "g",
    "Exit"
)

try {
    Write-Host "J-Link camera frame save:"
    Write-Host "  Output: $ResolvedOutput"
    Write-Host ("  Address: 0x{0:X8}" -f $Address)
    Write-Host "  Bytes: $Bytes"
    Write-Host "  Device: $Device"
    Write-Host "  Interface: $Interface"
    Write-Host "  SpeedKHz: $SpeedKHz"
    Write-Host "  JLinkSerial: $(if($JLinkSerial.Trim().Length -gt 0) { $JLinkSerial } else { '<auto>' })"
    Write-Host "  stdout: $Stdout"
    Write-Host "  stderr: $Stderr"

    $arguments = @(
        "-NoGui", "1",
        "-Device", $Device,
        "-If", $Interface,
        "-Speed", $SpeedKHz,
        "-AutoConnect", "1",
        "-CommandFile", $script
    ) + (Get-JLinkUsbArgs)
    $process = Start-Process -FilePath $JLinkExe `
                             -ArgumentList $arguments `
                             -NoNewWindow `
                             -PassThru `
                             -RedirectStandardOutput $Stdout `
                             -RedirectStandardError $Stderr

    if(-not $process.WaitForExit($TimeoutSec * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "J-Link savebin timed out after $TimeoutSec seconds."
    }
    $process.Refresh()

    $output = @()
    if(Test-Path $Stdout) {
        $output += Get-Content -LiteralPath $Stdout
    }
    if(Test-Path $Stderr) {
        $stderrOutput = Get-Content -LiteralPath $Stderr
        if($stderrOutput) {
            $output += "--- stderr ---"
            $output += $stderrOutput
        }
    }
    $output |
        Select-String -Pattern "VTref|Device|Found Cortex|savebin|O\.K\.|FAILED|Cannot|Error|Script processing completed|Writing" |
        ForEach-Object { Write-Host $_.Line }

    if(($null -ne $process.ExitCode) -and ($process.ExitCode -ne 0)) {
        throw "J-Link savebin failed with exit code $($process.ExitCode). See log: $Stdout"
    }
    if(-not (Test-Path $ResolvedOutput)) {
        throw "J-Link savebin did not create output: $ResolvedOutput. See log: $Stdout"
    }
    $size = (Get-Item -LiteralPath $ResolvedOutput).Length
    if($size -ne $Bytes) {
        throw "Unexpected frame size: got $size bytes, expected $Bytes bytes. See log: $Stdout"
    }

    Write-Host "Saved camera frame: $ResolvedOutput"
} finally {
    Remove-Item -LiteralPath $script -Force -ErrorAction SilentlyContinue
}
