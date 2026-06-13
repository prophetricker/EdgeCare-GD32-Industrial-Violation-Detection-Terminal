param(
    [string]$JLinkGdbServerExe = "D:\SEGGER\JLink_V948\JLinkGDBServerCL.exe",
    [string]$Device = "GD32H759IMT6",
    [string]$Interface = "SWD",
    [int]$SpeedKHz = 4000,
    [int]$Port = 2331,
    [int]$SwoPort = 2332,
    [int]$TelnetPort = 2333
)

$ErrorActionPreference = "Stop"

if(-not (Test-Path $JLinkGdbServerExe)) {
    throw "JLinkGDBServerCL.exe not found: $JLinkGdbServerExe"
}

Write-Host "Starting J-Link GDB Server for EdgeCare:"
Write-Host "  Device: $Device"
Write-Host "  Interface: $Interface"
Write-Host "  SpeedKHz: $SpeedKHz"
Write-Host "  GDB port: $Port"
Write-Host "  SWO port: $SwoPort"
Write-Host "  Telnet port: $TelnetPort"
Write-Host ""
Write-Host "Stop it with Ctrl+C when the debug session is done."

& $JLinkGdbServerExe `
    -nogui `
    -if $Interface `
    -speed $SpeedKHz `
    -device $Device `
    -port $Port `
    -swoport $SwoPort `
    -telnetport $TelnetPort
