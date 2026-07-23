param(
    [string]$BuildDir = "native/narutobb/out/build/win-amd64-source",
    [ValidateRange(5, 120)]
    [int]$Seconds = 30
)

$ErrorActionPreference = "Stop"
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$ResolvedBuildDir = if ([IO.Path]::IsPathRooted($BuildDir)) {
    [IO.Path]::GetFullPath($BuildDir)
} else {
    [IO.Path]::GetFullPath((Join-Path $ProjectRoot $BuildDir))
}
$Game = Join-Path $ResolvedBuildDir "narutobb.exe"
$Assets = Join-Path $ProjectRoot "recomp/fase4/assets"
$TraceDir = Join-Path $ProjectRoot "recomp/fase4/traces"
$LogsDir = Join-Path $ResolvedBuildDir "logs"
$ResultsDir = Join-Path $ProjectRoot "artifacts/test-results/boot"

foreach ($required in @($Game, (Join-Path $Assets "default.xex"))) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing: $required" }
}
New-Item -ItemType Directory -Force -Path $TraceDir, $ResultsDir | Out-Null
$startedAt = Get-Date

$psi = [Diagnostics.ProcessStartInfo]::new()
$psi.FileName = $Game
$psi.WorkingDirectory = $ResolvedBuildDir
$psi.UseShellExecute = $false
$psi.Arguments = '"' + $Assets + '" --game_data_root "' + $Assets +
    '" --log-level info --mnk_mode --keybind_start Return --audio_maxqframes=64' +
    ' --render_target_path_d3d12=rtv --trace_gpu_prefix "' + $TraceDir + '"'

$process = $null
$survived = $false
try {
    $process = [Diagnostics.Process]::Start($psi)
    $survived = -not $process.WaitForExit($Seconds * 1000)
    if (-not $survived) {
        throw "Game exited before $Seconds seconds (exit=$($process.ExitCode))"
    }
} finally {
    if ($process -and -not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
}

$log = Get-ChildItem -LiteralPath $LogsDir -Filter "narutobb_*.log" -File |
    Where-Object LastWriteTime -GE $startedAt.AddSeconds(-1) |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $log) { throw "The run did not create a new log in $LogsDir" }
$content = Get-Content -LiteralPath $log.FullName -Raw -Encoding UTF8

$requiredPatterns = @(
    "Runtime initialized successfully",
    "Loading XEX image",
    "Initializing shader storage",
    "SDL audio output",
    "NARUTO_PACER"
)
$missing = @($requiredPatterns | Where-Object { $content -notmatch [regex]::Escape($_) })
$fatalLines = @(Get-Content -LiteralPath $log.FullName -Encoding UTF8 |
    Where-Object { $_ -match '\[(fatal)\]|Call to invalid or unregistered function' })

$result = [ordered]@{
    tested_at = (Get-Date).ToString("o")
    build_dir = $ResolvedBuildDir
    duration_seconds = $Seconds
    survived_until_test_stop = $survived
    log = $log.FullName
    missing_required_patterns = $missing
    fatal_lines = $fatalLines
    passed = ($survived -and $missing.Count -eq 0 -and $fatalLines.Count -eq 0)
}
$resultPath = Join-Path $ResultsDir ("boot_" + (Get-Date -Format "yyyyMMdd_HHmmss") + ".json")
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $resultPath -Encoding UTF8

if (-not $result.passed) {
    throw "BOOT_TEST_FAIL result=$resultPath log=$($log.FullName)"
}
Write-Host "BOOT_TEST_OK duration=${Seconds}s log=$($log.Name) result=$resultPath"
