param(
    [string]$BuildDir = "native/narutobb/out/build/win-amd64-source",
    [ValidateRange(5, 120)]
    [int]$BootSeconds = 30,
    [switch]$SkipGpuReplay
)

$ErrorActionPreference = "Stop"
& (Join-Path $PSScriptRoot "test_project_invariants.ps1")
& (Join-Path $PSScriptRoot "test_boot.ps1") -BuildDir $BuildDir -Seconds $BootSeconds
if (-not $SkipGpuReplay) {
    & (Join-Path $PSScriptRoot "test_gpu_replay.ps1") -BuildDir $BuildDir
}
Write-Host "REGRESSION_OK build=$BuildDir"
