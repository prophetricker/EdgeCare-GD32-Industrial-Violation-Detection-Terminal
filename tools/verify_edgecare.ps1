param(
    [switch]$SkipSafetyScan
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$FirmwareDir = Join-Path $ProjectRoot "EdgeCare_GD32_Industrial_Violation_Terminal"
$BuildScript = Join-Path $FirmwareDir "build_keil.ps1"
$BuildLog = Join-Path $FirmwareDir "build-keil.log"
$SafetyScan = Join-Path $env:USERPROFILE ".codex\skills\project-kanban-github\scripts\git_safety_scan.ps1"

function Invoke-Step {
    param(
        [string]$Name,
        [scriptblock]$Body
    )

    Write-Host ""
    Write-Host "== $Name =="
    & $Body
}

function Assert-LastExitCode {
    param([string]$StepName)

    if($LASTEXITCODE -ne 0) {
        throw "$StepName failed with exit code $LASTEXITCODE"
    }
}

Push-Location $ProjectRoot
try {
    Invoke-Step "git diff --check" {
        git diff --check
        Assert-LastExitCode "git diff --check"
    }

    Invoke-Step "git diff --cached --check" {
        git diff --cached --check
        Assert-LastExitCode "git diff --cached --check"
    }

    if(-not $SkipSafetyScan) {
        Invoke-Step "git safety scan" {
            if(Test-Path $SafetyScan) {
                powershell.exe -ExecutionPolicy Bypass -File $SafetyScan
                Assert-LastExitCode "git safety scan"
            } else {
                Write-Host "Skipped: safety scan script not found at $SafetyScan"
            }
        }
    }

    Invoke-Step "Keil build" {
        powershell.exe -ExecutionPolicy Bypass -File $BuildScript
        Assert-LastExitCode "Keil build"
    }

    Invoke-Step "build log summary" {
        if(-not (Test-Path $BuildLog)) {
            throw "Build log not found: $BuildLog"
        }

        $Summary = Select-String -Path $BuildLog -Pattern "Program Size|Error\(s\)|Warning\(s\)"
        $Summary | ForEach-Object { Write-Host $_.Line }

        $Success = Select-String -Path $BuildLog -Pattern "0 Error\(s\), 0 Warning\(s\)" -Quiet
        if(-not $Success) {
            throw "Keil build did not finish with 0 Error(s), 0 Warning(s)"
        }
    }

    Write-Host ""
    Write-Host "EdgeCare verify passed."
} finally {
    Pop-Location
}
