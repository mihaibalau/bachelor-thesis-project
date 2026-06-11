param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('c', 'rust')]
    [string]$Backend,

    [Parameter(Mandatory = $true)]
    [ValidateSet('w1', 'w2', 'w3', 'w4', 'all')]
    [string]$Workload,

    [string]$BaseUrl = 'http://localhost:6767',

    [int]$Runs = 3
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Results = Join-Path $Root 'results'
$ScriptMap = @{
    w1 = 'k6/w1_login.js'
    w2 = 'k6/w2_statement.js'
    w3 = 'k6/w3_analytics.js'
    w4 = 'k6/w4_transfer.js'
}

function Get-K6Command {
    $cmd = Get-Command k6 -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $default = 'C:\Program Files\k6\k6.exe'
    if (Test-Path $default) { return $default }
    throw 'k6 not found. Install: winget install GrafanaLabs.k6'
}

function Test-BackendHealth {
    param([string]$Url)
    try {
        $r = Invoke-WebRequest -Uri "$Url/health" -UseBasicParsing -TimeoutSec 3
        return $r.StatusCode -eq 200
    } catch {
        return $false
    }
}

$k6 = Get-K6Command
New-Item -ItemType Directory -Force -Path $Results | Out-Null
$env:BASE_URL = $BaseUrl

if (-not (Test-BackendHealth $BaseUrl)) {
    throw @"
Backend not reachable at $BaseUrl/health

  • Start ONE backend (C or Rust) on port 6767.
  • Quick check: curl http://localhost:6767/health
"@
}

Write-Host "Backend OK at $BaseUrl/health" -ForegroundColor Green
Write-Host "Using k6: $k6" -ForegroundColor DarkGray

$workloads = if ($Workload -eq 'all') { @('w1', 'w2', 'w3', 'w4') } else { @($Workload) }

foreach ($w in $workloads) {
    if ($w -eq 'w4') {
        Write-Host 'Reminder: re-run db/seeds/benchmark_seed.sql before W4 if a prior transfer test drained balances.' -ForegroundColor Yellow
    }

    $script = Join-Path $Root $ScriptMap[$w]
    if (-not (Test-Path $script)) {
        throw "Script not found: $script"
    }

    Write-Host "`n=== $w ($Backend) -> $BaseUrl ===" -ForegroundColor Cyan

    for ($i = 1; $i -le $Runs; $i++) {
        $out = Join-Path $Results "$w-$Backend-run$i.json"
        Write-Host "Run $i/$Runs -> $out"
        & $k6 run --summary-export $out $script
        if ($LASTEXITCODE -ne 0) {
            throw "k6 failed on $w run $i (exit $LASTEXITCODE)"
        }
    }
}

Write-Host "`nDone. Summarize: .\summarize-results.ps1" -ForegroundColor Green
