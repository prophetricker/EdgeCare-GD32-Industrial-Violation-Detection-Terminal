param(
    [string]$Port = "COM8",
    [int]$Baudrate = 115200,
    [int]$SerialDurationSec = 4,
    [string]$JLinkExe = "D:\SEGGER\JLink_V948\JLink.exe",
    [string]$JLinkSerial = "",
    [string]$Device = "GD32H759IMT6",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 100,
    [int]$JLinkTimeoutSec = 20
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$CheckSweepScript = Join-Path $PSScriptRoot "check_camera_jpeg_yuv_order_sweep_build.ps1"

function New-JLinkCommandFile {
    param([string[]]$Lines)

    $path = Join-Path $env:TEMP ("edgecare_jlink_health_{0}.jlink" -f ([Guid]::NewGuid().ToString("N")))
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
        [string[]]$Lines
    )

    $script = New-JLinkCommandFile $Lines
    $stdout = Join-Path $env:TEMP ("edgecare_jlink_health_{0}_{1}.stdout.log" -f $Name, ([Guid]::NewGuid().ToString("N")))
    $stderr = Join-Path $env:TEMP ("edgecare_jlink_health_{0}_{1}.stderr.log" -f $Name, ([Guid]::NewGuid().ToString("N")))
    try {
        $arguments = @("-NoGui", "1", "-CommandFile", $script) + (Get-JLinkUsbArgs)
        $process = Start-Process -FilePath $JLinkExe `
                                 -ArgumentList $arguments `
                                 -NoNewWindow `
                                 -PassThru `
                                 -RedirectStandardOutput $stdout `
                                 -RedirectStandardError $stderr
        if(-not $process.WaitForExit($JLinkTimeoutSec * 1000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            return @{
                TimedOut = $true
                ExitCode = $null
                Lines = @("J-Link command '$Name' timed out after $JLinkTimeoutSec seconds")
            }
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
        return @{
            TimedOut = $false
            ExitCode = $process.ExitCode
            Lines = $output
        }
    } finally {
        Remove-Item -LiteralPath $script,$stdout,$stderr -Force -ErrorAction SilentlyContinue
        Get-Process JLink -ErrorAction SilentlyContinue | Stop-Process -Force
    }
}

function Read-SerialBounded {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $Baudrate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
    $serial.Encoding = [System.Text.Encoding]::UTF8
    $serial.ReadTimeout = 200
    $serial.DtrEnable = $false
    $serial.RtsEnable = $false
    $lines = New-Object System.Collections.Generic.List[string]
    try {
        $serial.Open()
        $deadline = (Get-Date).AddSeconds($SerialDurationSec)
        $pending = ""
        while((Get-Date) -lt $deadline) {
            $chunk = $serial.ReadExisting()
            if($chunk.Length -gt 0) {
                $pending += $chunk
                while($pending -match "`n") {
                    $idx = $pending.IndexOf("`n")
                    $line = $pending.Substring(0, $idx).TrimEnd("`r")
                    $pending = $pending.Substring($idx + 1)
                    if($line.Length -gt 0) {
                        $lines.Add($line)
                    }
                }
            } else {
                Start-Sleep -Milliseconds 50
            }
        }
        if($pending.Trim().Length -gt 0) {
            $lines.Add($pending.TrimEnd("`r", "`n"))
        }
        return @{
            Opened = $true
            Lines = $lines.ToArray()
            Error = ""
        }
    } catch {
        return @{
            Opened = $false
            Lines = @()
            Error = $_.Exception.Message
        }
    } finally {
        if($serial.IsOpen) {
            $serial.Close()
        }
    }
}

if(-not (Test-Path $JLinkExe)) {
    throw "JLink.exe not found: $JLinkExe"
}

Write-Host "EdgeCare J-Link/SWD health check:"
Write-Host "  This is read-only for flash: no flash performed."
Write-Host "  Device: $Device"
Write-Host "  Interface: $Interface"
Write-Host "  SpeedKHz: $SpeedKHz"
Write-Host "  JLinkSerial: $(if($JLinkSerial.Trim().Length -gt 0) { $JLinkSerial } else { '<auto>' })"
Write-Host "  Serial: $Port @ $Baudrate"

Write-Host ""
Write-Host "== Sweep artifact =="
$sweepReady = $false
if(Test-Path $CheckSweepScript) {
    $checkOutput = powershell.exe -ExecutionPolicy Bypass -File $CheckSweepScript 2>&1
    $checkOutput | ForEach-Object { Write-Host $_ }
    $sweepReady = ($LASTEXITCODE -eq 0 -and ($checkOutput -match "ready_to_flash_sweep=1"))
} else {
    Write-Host "check script missing: $CheckSweepScript"
}

Write-Host ""
Write-Host "== J-Link SWD DP =="
$dp = Invoke-JLinkBounded -Name "swd_dp" -Lines @(
    "ShowHWStatus",
    "Device $Device",
    "SelectInterface $Interface",
    "Speed $SpeedKHz",
    "Connect",
    "ReadDP 0",
    "ReadDP 3",
    "mem32 0xE000ED00,1",
    "Exit"
)
$dp.Lines | ForEach-Object { Write-Host $_ }
$dpText = ($dp.Lines -join "`n")
$swdDpOk = (-not $dp.TimedOut) -and
           ($dpText -match "VTref=([1-9]\d*\.\d+|[1-9]\d*)V") -and
           ($dpText -match "Found SW-DP|Cortex-M7 identified|E000ED00 = 411FC272") -and
           ($dpText -notmatch "Reading failed") -and
           ($dpText -notmatch "timed out")

Write-Host ""
Write-Host "== J-Link RESET line =="
$serialBefore = Read-SerialBounded
$lastBefore = ($serialBefore.Lines | Select-Object -Last 1)
if($lastBefore) {
    Write-Host "serial_before_last=$lastBefore"
} elseif(-not $serialBefore.Opened) {
    Write-Host "serial_before_error=$($serialBefore.Error)"
} else {
    Write-Host "serial_before_last=<none>"
}
$reset = Invoke-JLinkBounded -Name "reset_line" -Lines @(
    "ShowHWStatus",
    "Device $Device",
    "SelectInterface $Interface",
    "Speed $SpeedKHz",
    "ClrRESET",
    "Sleep 300",
    "SetRESET",
    "Sleep 500",
    "ShowHWStatus",
    "Exit"
)
$reset.Lines | ForEach-Object { Write-Host $_ }
$serialAfter = Read-SerialBounded
$resetObserved = $false
foreach($line in $serialAfter.Lines) {
    Write-Host $line
    if($line -match "edgecare-01 boot|camera_id|camera_capture_probe|\[0\] state=|\[[0-9]{1,4}\] state=") {
        $resetObserved = $true
    }
}

Write-Host ""
Write-Host "== COM serial =="
if($serialAfter.Opened) {
    Write-Host "serial_lines=$($serialAfter.Lines.Count)"
} else {
    Write-Host "serial_error=$($serialAfter.Error)"
}

Write-Host ""
Write-Host "== Health summary =="
Write-Host ("ready_to_flash_sweep={0}" -f ($(if($sweepReady) { 1 } else { 0 })))
Write-Host ("swd_dp_ok={0}" -f ($(if($swdDpOk) { 1 } else { 0 })))
Write-Host ("reset_line_observed={0}" -f ($(if($resetObserved) { 1 } else { 0 })))
Write-Host ("serial_lines={0}" -f $serialAfter.Lines.Count)

if($swdDpOk -and $resetObserved) {
    Write-Host "next_action=flash_sweep_then_capture"
} elseif((-not $swdDpOk) -and $resetObserved) {
    Write-Host "next_action=fix_swdio_swclk_pa13_pa14_connection"
} elseif(-not $swdDpOk) {
    Write-Host "next_action=fix_swdio_swclk_gnd_vtref_connection"
} elseif(-not $resetObserved) {
    Write-Host "next_action=fix_reset_line_or_use_manual_reset_for_serial_capture"
}
