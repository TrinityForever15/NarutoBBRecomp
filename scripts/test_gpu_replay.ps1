param(
    [string]$BuildDir = "native/narutobb/out/build/win-amd64-source",
    [string]$TraceDir = "recomp/fase4/traces",
    [string]$OutputDir = "artifacts/test-results/gpu-replay",
    [string]$BaselinePath = "tests/baselines/gpu_replay_baseline.json",
    [ValidateRange(0, 10)]
    [double]$MeanRgbTolerance = 0.25,
    [ValidateRange(0, 100000)]
    [long]$NonBlackTolerance = 1000
)

$ErrorActionPreference = "Stop"
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
function Resolve-ProjectPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $ProjectRoot $Path))
}

$ResolvedBuildDir = Resolve-ProjectPath $BuildDir
$ResolvedTraceDir = Resolve-ProjectPath $TraceDir
$ResolvedOutputDir = Resolve-ProjectPath $OutputDir
$ResolvedBaselinePath = Resolve-ProjectPath $BaselinePath
$Tool = Join-Path $ResolvedBuildDir "narutobb_trace_dump.exe"
if (-not (Test-Path -LiteralPath $Tool)) { throw "Replay tool is missing: $Tool" }
if (-not (Test-Path -LiteralPath $ResolvedBaselinePath)) { throw "Baseline is missing: $ResolvedBaselinePath" }
$traces = @(Get-ChildItem -LiteralPath $ResolvedTraceDir -Filter "55530825_*.xtr" -File | Sort-Object Name)
if ($traces.Count -eq 0) { throw "No local traces found in $ResolvedTraceDir" }
New-Item -ItemType Directory -Force -Path $ResolvedOutputDir | Out-Null

$baselineByTrace = @{}
$baselineEntries = Get-Content -LiteralPath $ResolvedBaselinePath -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($entry in $baselineEntries) {
    $baselineByTrace[$entry.trace] = $entry
}

$results = @()
$failures = 0
foreach ($trace in $traces) {
    $base = Join-Path $ResolvedOutputDir $trace.BaseName
    $log = "$base.log"
    Write-Host "[replay] $($trace.Name)"
    & $Tool $trace.FullName $base `
        --render_target_path_d3d12=rtv `
        --depth_transfer_not_equal_test=true `
        --async_shader_compilation=false *>&1 |
        Tee-Object -FilePath $log | Out-Null
    $exitCode = $LASTEXITCODE
    $summaryPattern = "TRACE_DUMP_SUMMARY mean_rgb=([\d\.]+) nonblack_pixels=(\d+) total_pixels=(\d+) size=(\d+)x(\d+)"
    $summaryLine = Select-String -LiteralPath $log -Pattern $summaryPattern |
        Select-Object -Last 1
    $bmp = "$base.bmp"
    $parsed = $null
    if ($summaryLine) { $parsed = $summaryLine.Matches[0] }
    $baseline = $baselineByTrace[$trace.Name]
    $meanRgb = if ($parsed) { [double]::Parse($parsed.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture) } else { -1 }
    $nonBlack = if ($parsed) { [long]$parsed.Groups[2].Value } else { -1 }
    $meanDelta = if ($baseline) { [Math]::Abs($meanRgb - [double]$baseline.mean_rgb) } else { [double]::PositiveInfinity }
    $nonBlackDelta = if ($baseline) { [Math]::Abs($nonBlack - [long]$baseline.nonblack_pixels) } else { [long]::MaxValue }
    $baselinePassed = $baseline -and $meanDelta -le $MeanRgbTolerance -and $nonBlackDelta -le $NonBlackTolerance
    $passed = $exitCode -eq 0 -and $parsed -and (Test-Path -LiteralPath $bmp) -and $baselinePassed
    if (-not $passed) { $failures++ }
    Write-Host ("[result] passed={0} mean={1:F4} delta={2:F4} nonblack={3} delta={4}" -f `
        $passed, $meanRgb, $meanDelta, $nonBlack, $nonBlackDelta)
    $results += [pscustomobject]@{
        Trace = $trace.Name
        ExitCode = $exitCode
        MeanRgb = $meanRgb
        MeanRgbDelta = $meanDelta
        NonBlack = $nonBlack
        NonBlackDelta = $nonBlackDelta
        TotalPixels = if ($parsed) { [long]$parsed.Groups[3].Value } else { -1 }
        Width = if ($parsed) { [int]$parsed.Groups[4].Value } else { -1 }
        Height = if ($parsed) { [int]$parsed.Groups[5].Value } else { -1 }
        Bmp = $bmp
        Log = $log
        BaselinePassed = [bool]$baselinePassed
        Passed = [bool]$passed
    }
}

$csv = Join-Path $ResolvedOutputDir "gpu_replay_results.csv"
$json = Join-Path $ResolvedOutputDir "gpu_replay_results.json"
$results | Export-Csv -LiteralPath $csv -NoTypeInformation -Encoding UTF8
$results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $json -Encoding UTF8
if ($failures) { throw "GPU_REPLAY_FAIL failures=$failures results=$json" }
Write-Host "GPU_REPLAY_OK traces=$($results.Count) results=$json"
