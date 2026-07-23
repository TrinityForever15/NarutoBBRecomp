# Deterministically bisect a draw that makes a locally captured frame black.
# Uses narutobb_trace_dump, trace_draw_* runtime options, and TRACE_DUMP_SUMMARY.
#
# Run from the repository root. Trace files and rendered output remain local and
# must never be committed or distributed.

param(
    [Parameter(Mandatory = $true)][string]$Trace,
    [switch]$Inventory,
    [string]$SkipIndices,
    [switch]$SweepCopies,
    [long]$SkipBegin = -1,
    [long]$SkipEnd = -1,
    [long]$DumpState = -1,
    [string]$OutDir = "recomp\fase4\traces\bisect",
    [double]$BlackMeanThreshold = 2.0
)

$ErrorActionPreference = "Stop"
$Tool = "native\narutobb\out\build\win-amd64-source\narutobb_trace_dump.exe"
if (-not (Test-Path $Tool)) { throw "Replay tool not found: $Tool (run from the repository root)" }
if (-not (Test-Path $Trace)) { throw "Trace not found: $Trace" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$BaseFlags = @(
    "--render_target_path_d3d12=rtv",
    "--depth_transfer_not_equal_test=true",
    "--async_shader_compilation=false"
)
$TraceName = [IO.Path]::GetFileNameWithoutExtension($Trace)

function Invoke-Replay {
    param([string]$Tag, [string[]]$ExtraFlags)
    $OutBase = Join-Path $OutDir "$TraceName`_$Tag"
    $LogPath = "$OutBase.log"
    # Positional arguments: trace path, output base (the tool appends .bmp).
    & $Tool $Trace $OutBase @BaseFlags @ExtraFlags *>&1 | Out-File -Encoding utf8 $LogPath
    $Summary = Select-String -Path $LogPath -Pattern "TRACE_DUMP_SUMMARY mean_rgb=([\d\.]+) nonblack_pixels=(\d+)" | Select-Object -Last 1
    $Total = Select-String -Path $LogPath -Pattern "total draw candidates=(\d+)" | Select-Object -Last 1
    return [pscustomobject]@{
        Tag      = $Tag
        MeanRgb  = if ($Summary) { [double]$Summary.Matches[0].Groups[1].Value } else { -1 }
        NonBlack = if ($Summary) { [long]$Summary.Matches[0].Groups[2].Value } else { -1 }
        Draws    = if ($Total) { [long]$Total.Matches[0].Groups[1].Value } else { -1 }
        Bmp      = "$OutBase.bmp"
        Log      = $LogPath
    }
}

Write-Host "[baseline] replaying $TraceName without draw skips..."
$Baseline = Invoke-Replay -Tag "baseline" -ExtraFlags @("--trace_draw_log_all=true")
Write-Host ("[baseline] mean_rgb={0} nonblack={1} draws={2}" -f $Baseline.MeanRgb, $Baseline.NonBlack, $Baseline.Draws)
if ($Baseline.MeanRgb -lt 0) { throw "TRACE_DUMP_SUMMARY not found in $($Baseline.Log)" }

$InventoryCsv = Join-Path $OutDir "$TraceName`_draws.csv"
$DrawLines = Select-String -Path $Baseline.Log -Pattern "TRACE_DRAW idx=(\d+) (\w+) prim=(\d+) indices=(\d+) indexed=(\d+) edram_mode=(\d+) VS=([0-9A-F]+) PS=([0-9A-F]+)"
$Draws = foreach ($Match in $DrawLines) {
    [pscustomobject]@{
        Idx = [long]$Match.Matches[0].Groups[1].Value
        Prim = [int]$Match.Matches[0].Groups[3].Value
        Indices = [long]$Match.Matches[0].Groups[4].Value
        Indexed = [int]$Match.Matches[0].Groups[5].Value
        EdramMode = [int]$Match.Matches[0].Groups[6].Value
        VS = $Match.Matches[0].Groups[7].Value
        PS = $Match.Matches[0].Groups[8].Value
    }
}
$Draws | Export-Csv -NoTypeInformation -Encoding utf8 $InventoryCsv
Write-Host "[inventory] $($Draws.Count) draws written to $InventoryCsv"
if ($Inventory -and -not $SkipIndices -and -not $SweepCopies -and $SkipBegin -lt 0 -and $DumpState -lt 0) { return }

if ($DumpState -ge 0) {
    $Result = Invoke-Replay -Tag "state$DumpState" -ExtraFlags @("--trace_draw_log_state=$DumpState")
    Select-String -Path $Result.Log -Pattern "TRACE_DRAW_STATE|TRACE_DRAW_COPY|TRACE_DRAW idx=$DumpState " | ForEach-Object { $_.Line }
    return
}

if ($SkipBegin -ge 0 -and $SkipEnd -gt $SkipBegin) {
    $Result = Invoke-Replay -Tag "skip$($SkipBegin)-$($SkipEnd)" -ExtraFlags @("--trace_draw_skip_begin=$SkipBegin", "--trace_draw_skip_end=$SkipEnd")
    Write-Host ("[skip {0}-{1}] mean_rgb={2} (baseline {3}) nonblack={4} bmp={5}" -f $SkipBegin, $SkipEnd, $Result.MeanRgb, $Baseline.MeanRgb, $Result.NonBlack, $Result.Bmp)
    return
}

$Targets = @()
if ($SkipIndices) {
    $Targets = $SkipIndices -split "[,; ]+" | Where-Object { $_ -ne "" } | ForEach-Object { [long]$_ }
} elseif ($SweepCopies) {
    # EDRAM kCopy = 6 (kNoOperation=0, kColorDepth=4, kDepthOnly=5).
    $Targets = ($Draws | Where-Object { $_.EdramMode -eq 6 }).Idx
    Write-Host "[sweep] $($Targets.Count) resolve/copy draws"
}
if (-not $Targets) {
    Write-Host "Nothing to sweep. Use -SkipIndices, -SweepCopies, -SkipBegin/-SkipEnd, or -DumpState."
    return
}

$Results = @()
foreach ($Index in $Targets) {
    $Result = Invoke-Replay -Tag "skip$Index" -ExtraFlags @("--trace_draw_skip_begin=$Index", "--trace_draw_skip_end=$($Index + 1)")
    $Delta = $Result.MeanRgb - $Baseline.MeanRgb
    $Flag = if ($Baseline.MeanRgb -le $BlackMeanThreshold -and $Result.MeanRgb -gt $BlackMeanThreshold) { " <== FRAME IS NO LONGER BLACK" } else { "" }
    Write-Host ("[skip {0}] mean_rgb={1:N3} delta={2:N3} nonblack={3}{4}" -f $Index, $Result.MeanRgb, $Delta, $Result.NonBlack, $Flag)
    $Results += [pscustomobject]@{ SkipIdx = $Index; MeanRgb = $Result.MeanRgb; Delta = $Delta; NonBlack = $Result.NonBlack; Bmp = $Result.Bmp }
}

$ResultsCsv = Join-Path $OutDir "$TraceName`_sweep.csv"
$Results | Export-Csv -NoTypeInformation -Encoding utf8 $ResultsCsv
Write-Host "[done] results written to $ResultsCsv"
$Candidates = $Results | Where-Object { $_.MeanRgb -gt $BlackMeanThreshold } | Sort-Object -Descending MeanRgb
if ($Baseline.MeanRgb -le $BlackMeanThreshold -and $Candidates) {
    Write-Host "CANDIDATE CULPRIT DRAWS (skipping restored the image):"
    $Candidates | ForEach-Object { Write-Host ("  draw {0}: mean_rgb={1:N3} bmp={2}" -f $_.SkipIdx, $_.MeanRgb, $_.Bmp) }
}
