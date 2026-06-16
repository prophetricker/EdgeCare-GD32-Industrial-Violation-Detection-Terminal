param(
    [switch]$Flash,
    [string]$JLinkExe = "D:\SEGGER\JLink_V948\JLink.exe",
    [string]$JLinkSerial = "",
    [string]$Device = "GD32H759IMT6",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 1000,
    [string]$AxfPath = "",
    [int]$TimeoutSec = 120,
    [string]$LogDir = ""
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$DefaultAxf = Join-Path $ProjectRoot "EdgeCare_GD32_Industrial_Violation_Terminal\GD32H759I_START_Demo_Suites\Projects\01_EdgeCare_Industrial_Violation_Terminal\MDK-ARM\Objects\EdgeCare_GD32_Terminal.axf"

function New-JLinkCommandFile {
    param([string[]]$Lines)

    $path = Join-Path $env:TEMP ("edgecare_jlink_flash_{0}.jlink" -f ([Guid]::NewGuid().ToString("N")))
    $Lines | Set-Content -LiteralPath $path -Encoding ASCII
    return $path
}

function Get-JLinkUsbArgs {
    if($JLinkSerial.Trim().Length -gt 0) {
        return @("-USB", $JLinkSerial)
    }
    return @()
}

if([string]::IsNullOrWhiteSpace($AxfPath)) {
    $AxfPath = $DefaultAxf
}
if([string]::IsNullOrWhiteSpace($LogDir)) {
    $LogDir = Join-Path $ProjectRoot ".embeddedskills\logs\jlink"
}

if(-not (Test-Path $JLinkExe)) {
    throw "JLink.exe not found: $JLinkExe"
}
if(-not (Test-Path $AxfPath)) {
    throw "AXF not found: $AxfPath"
}
if($Flash -and $Device -eq "Cortex-M7") {
    throw "Refusing to flash with generic Device=Cortex-M7. Use a SEGGER flash-supported GD32H7 device name, for example GD32H759IMT6."
}

$ResolvedAxf = (Resolve-Path $AxfPath).Path
$ResolvedLogDir = (New-Item -ItemType Directory -Force -Path $LogDir).FullName

Write-Host "J-Link flash target:"
Write-Host "  AXF: $ResolvedAxf"
Write-Host "  Device: $Device"
Write-Host "  Interface: $Interface"
Write-Host "  SpeedKHz: $SpeedKHz"
Write-Host "  JLinkSerial: $(if($JLinkSerial.Trim().Length -gt 0) { $JLinkSerial } else { '<auto>' })"
Write-Host "  TimeoutSec: $TimeoutSec"

if(-not $Flash) {
    Write-Host ""
    Write-Host "Dry run: no flash performed. Re-run with -Flash to program the target."
    Write-Host "Note: SEGGER J-Link uses full device names such as GD32H759IMT6; Keil's shorter GD32H759IM name is not a J-Link device string."
    exit 0
}

$script = New-JLinkCommandFile @(
    "r",
    ("loadfile ""{0}""" -f $ResolvedAxf),
    "Exit"
)

try {
    Write-Host ""
    Write-Host "== J-Link flash =="
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $stdout = Join-Path $ResolvedLogDir ("edgecare_jlink_flash_{0}.stdout.log" -f $stamp)
    $stderr = Join-Path $ResolvedLogDir ("edgecare_jlink_flash_{0}.stderr.log" -f $stamp)
    Write-Host "  stdout: $stdout"
    Write-Host "  stderr: $stderr"

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
                             -RedirectStandardOutput $stdout `
                             -RedirectStandardError $stderr

    if(-not $process.WaitForExit($TimeoutSec * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "J-Link flash timed out after $TimeoutSec seconds. See log: $stdout"
    }
    $process.Refresh()
    $output = @()
    if(Test-Path $stdout) {
        $output += Get-Content -LiteralPath $stdout
    }
    if(Test-Path $stderr) {
        $stderrOutput = Get-Content -LiteralPath $stderr
        if($stderrOutput) {
            $output += "--- stderr ---"
            $output += $stderrOutput
        }
    }

    $output | ForEach-Object { Write-Host $_ }
    $outputText = $output -join "`n"
    $flashProgrammed = (($outputText -match "Programming flash") -and ($outputText -match "Done")) -or
                        ($outputText -match "Contents already match")
    $flashVerified = (($outputText -match "Verifying flash") -and ($outputText -match "Done")) -or
                      ($outputText -match "Contents already match")
    $flashProgramVerifyOk = $flashProgrammed -and $flashVerified
    if($flashProgramVerifyOk) {
        Write-Host "flash_program_verify_ok=1"
    } else {
        Write-Host "flash_program_verify_ok=0"
    }

    if((($null -ne $process.ExitCode) -and ($process.ExitCode -ne 0)) -and (-not $flashProgramVerifyOk)) {
        throw "J-Link flash failed with exit code $($process.ExitCode). See log: $stdout"
    }
    if(-not $flashProgramVerifyOk) {
        throw "J-Link completed, but the log did not contain a clear Programming flash + Verifying flash success marker. See log: $stdout"
    }
} finally {
    Remove-Item -LiteralPath $script -Force -ErrorAction SilentlyContinue
}
