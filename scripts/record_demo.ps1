param(
    [Parameter(Mandatory = $true)]
    [ValidateSet(1, 2, 3)]
    [int]$Part,

    [ValidateRange(0.1, 20.0)]
    [double]$ReplaySpeed = 2.0
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot 'build-recording'
$singleDemo = Join-Path $buildRoot 'elemental_demo.exe'
$teamDemo = Join-Path $buildRoot 'elemental_team_demo.exe'

if (-not (Test-Path -LiteralPath $singleDemo) -or
    -not (Test-Path -LiteralPath $teamDemo)) {
    throw 'Recording executables are missing. Follow docs/RECORDING_GUIDE.zh-CN.md and build first.'
}

Set-Location -LiteralPath $projectRoot
Clear-Host

$titles = @{
    1 = 'PART 1 / Single-target reaction engine (no four-character party)'
    2 = 'PART 2 / Classic reaction team (two enemies)'
    3 = 'PART 3 / Hyperbloom team (Dendro reactions)'
}

Write-Host '============================================================'
Write-Host $titles[$Part]
Write-Host '============================================================'
Write-Host "Replay speed: ${ReplaySpeed}x"
Write-Host ''

foreach ($second in 3..1) {
    Write-Host "Starting in $second..."
    Start-Sleep -Seconds 1
}
Write-Host ''

switch ($Part) {
    1 {
        & $singleDemo --replay-speed $ReplaySpeed
    }
    2 {
        & $teamDemo --preset classic --duration 12 --enemies 2 --replay-speed $ReplaySpeed --compact
    }
    3 {
        & $teamDemo --preset hyperbloom --duration 10 --enemies 1 --replay-speed $ReplaySpeed --compact
    }
}

Write-Host ''
Write-Host 'Recording segment complete. Stop Xbox Game Bar now.'
Read-Host 'After stopping the recording, press Enter to close this segment'
