param(
    [string]$Port = "COM8",
    [int]$Baudrate = 115200,
    [int]$DurationSec = 25,
    [string]$JLinkExe = "D:\SEGGER\JLink_V948\JLink.exe",
    [string]$Device = "GD32H759IMT6",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 4000,
    [string]$JLinkSerial = "",
    [int]$JLinkTimeoutSec = 20,
    [switch]$ResetLineOnly,
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"

function New-JLinkCommandFile {
    param([string[]]$Lines)

    $path = Join-Path $env:TEMP ("edgecare_jlink_reset_{0}.jlink" -f ([Guid]::NewGuid().ToString("N")))
    $Lines | Set-Content -LiteralPath $path -Encoding ASCII
    return $path
}

function Get-JLinkUsbArgs {
    if($JLinkSerial.Trim().Length -gt 0) {
        return @("-USB", $JLinkSerial)
    }
    return @()
}

function Invoke-JLinkCommandFile {
    param([string[]]$Lines)

    $script = New-JLinkCommandFile $Lines
    $stdout = Join-Path $env:TEMP ("edgecare_jlink_reset_{0}.stdout.log" -f ([Guid]::NewGuid().ToString("N")))
    $stderr = Join-Path $env:TEMP ("edgecare_jlink_reset_{0}.stderr.log" -f ([Guid]::NewGuid().ToString("N")))
    $process = $null
    try {
        if($ResetLineOnly) {
            $arguments = @("-NoGui", "1", "-CommandFile", $script)
        } else {
            $arguments = @("-NoGui", "1", "-Device", $Device, "-If", $Interface, "-Speed", $SpeedKHz, "-AutoConnect", "1", "-CommandFile", $script)
        }
        $arguments = $arguments + (Get-JLinkUsbArgs)

        $process = Start-Process -FilePath $JLinkExe `
                                 -ArgumentList $arguments `
                                 -NoNewWindow `
                                 -PassThru `
                                 -RedirectStandardOutput $stdout `
                                 -RedirectStandardError $stderr
        if(-not $process.WaitForExit($JLinkTimeoutSec * 1000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            throw "J-Link command timed out after $JLinkTimeoutSec seconds"
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
            throw "J-Link command failed with exit code $($process.ExitCode)"
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

if([string]::IsNullOrWhiteSpace($Output)) {
    $logDir = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "..")) ".embeddedskills\logs\serial"
    New-Item -ItemType Directory -Force -Path $logDir | Out-Null
    $Output = Join-Path $logDir ("edgecare_reset_capture_{0}.log" -f (Get-Date -Format "yyyyMMdd_HHmmss"))
}

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baudrate, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$serial.Encoding = [System.Text.Encoding]::UTF8
$serial.ReadTimeout = 200
$serial.NewLine = "`n"
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.ReadBufferSize = 262144
$lines = New-Object System.Collections.Generic.List[string]

try {
    if(-not $ResetLineOnly) {
        Write-Host "Reset-halt target via J-Link before opening serial..."
        $resetOutput = Invoke-JLinkCommandFile @(
            "r",
            "h",
            "Exit"
        )
        $resetOutput |
            Select-String -Pattern "VTref|Device|Found Cortex|O\.K\.|Reset|Memory map|Error|Cannot|Failed" |
            ForEach-Object { Write-Host ("JLink: " + $_.Line) }
    }

    $serial.Open()
    $serial.DiscardInBuffer()
    Write-Host "Serial opened: $Port @ $Baudrate"

    if($ResetLineOnly) {
        Write-Host "Pulsing target RESET via J-Link after serial is ready..."
        $runOutput = Invoke-JLinkCommandFile @(
            "ShowHWStatus",
            "ClrRESET",
            "Sleep 300",
            "SetRESET",
            "Sleep 500",
            "ShowHWStatus",
            "Exit"
        )
        $runOutput |
            Select-String -Pattern "VTref|O\.K\.|TRES|Error|Cannot|Failed" |
            ForEach-Object { Write-Host ("JLink: " + $_.Line) }
    } else {
        Write-Host "Running target via J-Link after serial is ready..."
        $runOutput = Invoke-JLinkCommandFile @(
            "g",
            "Exit"
        )
        $runOutput |
            Select-String -Pattern "VTref|Device|Found Cortex|O\.K\.|Reset|Memory map|Error|Cannot|Failed" |
            ForEach-Object { Write-Host ("JLink: " + $_.Line) }
    }

    Write-Host "Capturing serial for $DurationSec seconds..."
    $deadline = (Get-Date).AddSeconds($DurationSec)
    $pending = ""

    while((Get-Date) -lt $deadline) {
        try {
            $chunk = $serial.ReadExisting()
            if($chunk.Length -gt 0) {
                $pending += $chunk
                while($pending -match "`n") {
                    $idx = $pending.IndexOf("`n")
                    $line = $pending.Substring(0, $idx).TrimEnd("`r")
                    $pending = $pending.Substring($idx + 1)
                    if($line.Length -gt 0) {
                        $lines.Add($line)
                        Write-Host $line
                    }
                }
            } else {
                Start-Sleep -Milliseconds 50
            }
        } catch [TimeoutException] {
            Start-Sleep -Milliseconds 50
        }
    }

    if($pending.Trim().Length -gt 0) {
        $line = $pending.TrimEnd("`r", "`n")
        $lines.Add($line)
        Write-Host $line
    }

    $lines | Set-Content -LiteralPath $Output -Encoding UTF8

    Write-Host ""
    Write-Host "Captured $($lines.Count) non-empty serial lines."
    Write-Host "Saved log: $Output"
    Write-Host ""
    Write-Host "== Interesting camera lines =="
    $interesting = $lines | Where-Object {
        $_ -match "edgecare-01 boot|camera_id|camera_init|camera_capture_probe|jpeg_yuv_order_capture_sweep|camera_byte_scale|camera_dvp_mode_sweep_best|camera_dvp_regs\[after_dvp_mode_sweep\]|camera_dvp_ext_regs\[after_dvp_mode_sweep\]|camera_data_pad_sweep|camera_data_pad_summary|camera_raw_pclk_sample|camera_isp_path_regs|camera_isp_path_sweep|camera_forced_sync_dci|camera_dci_sync_matrix|camera_dci_dma_summary|camera_dci_status_probe|camera_capture_mode_sweep|camera_capture\["
    }
    if($interesting) {
        $interesting | ForEach-Object { Write-Host $_ }
    } else {
        Write-Host "No matching camera lines captured."
    }
    if($interesting -match "camera_byte_scale") {
        Write-Host ""
        Write-Host "Summarize JPEG-to-YUV order sweep with:"
        Write-Host "  py .\tools\analyze_camera_byte_scale_log.py `"$Output`""
    }
} finally {
    if($serial.IsOpen) {
        $serial.Close()
    }
}
