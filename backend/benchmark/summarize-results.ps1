# Print median p95 (ms) and rps from results/*.json for the paper table.
$ErrorActionPreference = 'Stop'
$Results = Join-Path $PSScriptRoot 'results'

if (-not (Test-Path $Results)) {
    Write-Host 'No results/ folder yet.'
    exit 0
}

function Get-Median([double[]]$Values) {
    if ($Values.Count -eq 0) { return $null }
    $sorted = $Values | Sort-Object
    $mid = [math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 0) {
        return ($sorted[$mid - 1] + $sorted[$mid]) / 2
    }
    return $sorted[$mid]
}

$rows = @()
foreach ($backend in @('c', 'rust')) {
    foreach ($w in @('w1', 'w2', 'w3', 'w4')) {
        $files = Get-ChildItem -Path $Results -Filter "$w-$backend-run*.json" -ErrorAction SilentlyContinue
        if (-not $files) { continue }

        $p95List = @()
        $rpsList = @()
        foreach ($f in $files) {
            $json = Get-Content $f.FullName -Raw | ConvertFrom-Json
            $p95List += [double]$json.metrics.'http_req_duration'.'p(95)'
            $rpsList += [double]$json.metrics.http_reqs.rate
        }

        $rows += [PSCustomObject]@{
            Workload = $w.ToUpper()
            Backend  = $backend
            Runs     = $files.Count
            'p95_ms' = [math]::Round((Get-Median $p95List), 1)
            rps      = [math]::Round((Get-Median $rpsList), 2)
        }
    }
}

if ($rows.Count -eq 0) {
    Write-Host 'No result JSON files found in results/.'
    exit 0
}

Write-Host ''
Write-Host 'Median metrics (use for paper table):' -ForegroundColor Cyan
$rows | Sort-Object Workload, Backend | Format-Table -AutoSize
Write-Host 'Source: metrics.http_req_duration.p(95) and metrics.http_reqs.rate'
